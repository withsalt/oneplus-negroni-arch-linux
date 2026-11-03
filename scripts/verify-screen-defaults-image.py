#!/usr/bin/env python3
"""Verify desktop and greeter power settings in an unmounted ext4 image."""
import configparser
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
FILES = {
    "/etc/xdg/powerdevilrc": ("config/powerdevil/powerdevilrc", "0644"),
    "/etc/sddm.conf.d/10-mobile.conf": ("config/sddm/10-mobile.conf", "0644"),
    "/etc/xdg/negroni-greeter/powerdevilrc": ("config/sddm/powerdevilrc", "0644"),
    "/etc/systemd/logind.conf.d/60-negroni-power-key.conf": ("config/logind/60-negroni-power-key.conf", "0644"),
    "/usr/local/libexec/negroni-sddm-compositor": ("config/sddm/negroni-sddm-compositor", "0755"),
    "/usr/local/libexec/negroni-sddm-power": ("config/sddm/negroni-sddm-power", "0755"),
}
DEPENDENCIES = ("/usr/bin/dbus-run-session", "/usr/bin/kwin_wayland",
                "/usr/bin/plasma-keyboard", "/usr/lib/org_kde_powerdevil")


def read_config(data):
    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read_string(data.decode())
    return config


def profiles(config, greeter=False):
    result = {}
    for profile in ("AC", "Battery", "LowBattery"):
        display = config[f"{profile}][Display"]
        suspend = config[f"{profile}][SuspendAndShutdown"]
        result[profile] = {
            "dim_when_idle": display.getboolean("DimDisplayWhenIdle"),
            "dim_idle_timeout_seconds": display.getint("DimDisplayIdleTimeoutSec"),
            "turn_off_when_idle": display.getboolean("TurnOffDisplayWhenIdle"),
            "turn_off_idle_timeout_seconds": display.getint("TurnOffDisplayIdleTimeoutSec"),
            "auto_suspend_idle_timeout_seconds": suspend.getint("AutoSuspendIdleTimeoutSec"),
        }
        if greeter:
            result[profile].update({
                "lock_before_turn_off": display.getboolean("LockBeforeTurnOffDisplay"),
                "power_button_action": suspend.getint("PowerButtonAction"),
                "power_down_action": suspend.getint("PowerDownAction"),
                "lid_action": suspend.getint("LidAction"),
            })
    return result


def verify(image_dir, log_dir):
    root_image = image_dir / "root.img"
    log_dir.mkdir(parents=True, exist_ok=True)
    report = log_dir / "screen-defaults-verification.json"
    report.unlink(missing_ok=True)

    def debugfs(command):
        result = subprocess.run(["debugfs", "-R", command, str(root_image)],
                                capture_output=True, check=True)
        # debugfs often returns success for missing files; inspect diagnostics too.
        errors = result.stderr.decode().splitlines()[1:]
        if errors:
            raise RuntimeError("\n".join(errors))
        return result.stdout

    contents = {}
    checksums = {}
    for target, (source, mode) in FILES.items():
        installed = debugfs(f"cat {target}")
        if installed != (PROJECT / source).read_bytes():
            raise RuntimeError(f"image/source mismatch: {target}")
        metadata = debugfs(f"stat {target}").decode()
        if (not re.search(rf"Type:\s+regular\s+Mode:\s+{mode}", metadata)
                or not re.search(r"User:\s+0\s+Group:\s+0", metadata)):
            raise RuntimeError(f"{target} must be regular, root-owned, mode {mode}")
        contents[target] = installed
        checksums[target] = hashlib.sha256(installed).hexdigest()

    for dependency in DEPENDENCIES:
        metadata = debugfs(f"stat {dependency}").decode()
        mode = re.search(r"Type:\s+regular\s+Mode:\s+([0-7]+)", metadata)
        if not mode or not int(mode[1], 8) & 0o111:
            raise RuntimeError(f"missing executable greeter dependency: {dependency}")

    desktop = profiles(read_config(contents["/etc/xdg/powerdevilrc"]))
    greeter = profiles(read_config(contents["/etc/xdg/negroni-greeter/powerdevilrc"]), greeter=True)
    for profile in greeter.values():
        if (not profile["turn_off_when_idle"] or profile["turn_off_idle_timeout_seconds"] != 120
                or profile["auto_suspend_idle_timeout_seconds"] != 0
                or profile["power_button_action"] != 128 or profile["lock_before_turn_off"]):
            raise RuntimeError("greeter must turn off after 120 idle seconds and toggle DPMS on power key")
    logind = read_config(contents["/etc/systemd/logind.conf.d/60-negroni-power-key.conf"])
    if logind["Login"]["HandlePowerKey"] != "ignore":
        raise RuntimeError("logind must not power off on a short power-key press")

    verification = {
        "image": str(root_image),
        "config": "/etc/xdg/powerdevilrc",
        "files_match_sources": True,
        "installed_file_sha256": checksums["/etc/xdg/powerdevilrc"],
        "regular_file_mode_0644_owned_by_root": True,
        "profiles": desktop,
        "installed_files_sha256": checksums,
        "greeter": {
            "config": "/etc/xdg/negroni-greeter/powerdevilrc",
            "profiles": greeter,
            "runtime_dependencies": list(DEPENDENCIES),
            "private_session_bus": True,
            "power_key_action": "ToggleScreenOnOff",
            "logind_handle_power_key": "ignore",
        },
        "scope": "Desktop system defaults (user settings take precedence); separate SDDM policy",
        "hardware_tested": False,
    }
    report.write_text(json.dumps(verification, indent=2) + "\n")
    print("Screen defaults verified: desktop defaults, SDDM 120s idle/DPMS power key, runtime dependencies")
    return verification


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: verify-screen-defaults-image.py IMAGE_DIRECTORY LOG_DIRECTORY")
    verify(*map(Path, sys.argv[1:]))
