#!/usr/bin/env python3
"""Verify the default CPU helper, unit and enablement in an unmounted ext4 image."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
UNIT = "negroni-cpu-performance.service"


def verify(image_dir, log_dir):
    image = image_dir / "root.img"

    def debugfs(command):
        result = subprocess.run(["debugfs", "-R", command, str(image)],
                                capture_output=True, check=True)
        errors = result.stderr.decode().splitlines()[1:]
        if errors:
            raise RuntimeError("\n".join(errors))
        return result.stdout

    checksums = {}
    files = {
        "/usr/local/sbin/negroni-cpu-performance": ("config/cpu-performance/negroni-cpu-performance", "0755"),
        f"/etc/systemd/system/{UNIT}": (f"config/systemd/{UNIT}", "0644"),
    }
    for target, (source, mode) in files.items():
        installed = debugfs(f"cat {target}")
        if installed != (PROJECT / source).read_bytes():
            raise RuntimeError(f"image/source mismatch: {target}")
        meta = debugfs(f"stat {target}").decode()
        if (not re.search(rf"Type:\s+regular\s+Mode:\s+{mode}", meta)
                or not re.search(r"User:\s+0\s+Group:\s+0", meta)):
            raise RuntimeError(f"{target} must be regular, root-owned, mode {mode}")
        checksums[target] = hashlib.sha256(installed).hexdigest()
    link = f"/etc/systemd/system/multi-user.target.wants/{UNIT}"
    meta = debugfs(f"stat {link}").decode()
    match = re.search(r'Fast link dest: "([^"]+)"', meta)
    if ("Type: symlink" not in meta or not match
            or match[1] not in (f"/etc/systemd/system/{UNIT}", f"../{UNIT}")):
        raise RuntimeError(f"CPU policy must be enabled with a link to the installed unit: {link}")
    verification = {
        "image": str(image), "service": UNIT, "service_enabled": True,
        "governor": "performance", "boost_enabled": True,
        "files_match_sources": True, "installed_file_sha256": checksums,
        "enablement_link": link, "enablement_target": match[1],
        "hardware_tested": False,
    }
    log_dir.mkdir(parents=True, exist_ok=True)
    (log_dir / "cpu-performance-verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    print("CPU image verified: matching helper/unit, root ownership/modes, enabled at boot")
    return verification


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: verify-cpu-performance-image.py IMAGE_DIRECTORY LOG_DIRECTORY")
    verify(*map(Path, sys.argv[1:]))
