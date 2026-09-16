#!/usr/bin/env python3
"""Verify Plasma defaults in an unmounted ext4 image without root privileges."""
import configparser
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

project = Path(__file__).resolve().parents[1]
image_dir, log_dir = map(Path, sys.argv[1:])
root_image = image_dir / "root.img"
target = "/etc/xdg/powerdevilrc"


def debugfs(command):
    result = subprocess.run(["debugfs", "-R", command, str(root_image)],
                            capture_output=True, check=True)
    # debugfs often returns success for missing files; inspect diagnostics too.
    errors = result.stderr.decode().splitlines()[1:]
    if errors:
        raise RuntimeError("\n".join(errors))
    return result.stdout


installed = debugfs(f"cat {target}")
if installed != (project / "config/powerdevil/powerdevilrc").read_bytes():
    raise RuntimeError(f"image/source mismatch: {target}")
metadata = debugfs(f"stat {target}").decode()
if (not re.search(r"Type:\s+regular\s+Mode:\s+0644", metadata)
        or not re.search(r"User:\s+0\s+Group:\s+0", metadata)):
    raise RuntimeError("screen defaults must be a regular file, mode 0644, owned by root")

config = configparser.ConfigParser(interpolation=None)
config.optionxform = str
config.read_string(installed.decode())
profiles = {}
for profile in ("AC", "Battery", "LowBattery"):
    display = config[f"{profile}][Display"]
    suspend = config[f"{profile}][SuspendAndShutdown"]
    profiles[profile] = {
        "dim_when_idle": display.getboolean("DimDisplayWhenIdle"),
        "dim_idle_timeout_seconds": display.getint("DimDisplayIdleTimeoutSec"),
        "turn_off_when_idle": display.getboolean("TurnOffDisplayWhenIdle"),
        "turn_off_idle_timeout_seconds": display.getint("TurnOffDisplayIdleTimeoutSec"),
        "auto_suspend_idle_timeout_seconds": suspend.getint("AutoSuspendIdleTimeoutSec"),
    }

verification = {
    "image": str(root_image),
    "config": target,
    "files_match_sources": True,
    "installed_file_sha256": hashlib.sha256(installed).hexdigest(),
    "regular_file_mode_0644_owned_by_root": True,
    "profiles": profiles,
    "scope": "System defaults; existing user settings take precedence",
    "hardware_tested": False,
}
(log_dir / "screen-defaults-verification.json").write_text(json.dumps(verification, indent=2) + "\n")
print("Screen defaults verified: image matches source, AC/Battery/LowBattery settings recorded")
