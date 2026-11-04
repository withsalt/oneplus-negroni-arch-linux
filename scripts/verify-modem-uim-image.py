#!/usr/bin/env python3
"""Verify that an offline image skips SIM provisioning when no card is present."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from modem_uim import NEW_BRANCH, SCRIPT_PATH, patched_script


def verify(image_dir, log_dir):
    image = image_dir / "root.img"
    log_dir.mkdir(parents=True, exist_ok=True)
    report = log_dir / "modem-uim-verification.json"
    report.unlink(missing_ok=True)

    def debugfs(command):
        result = subprocess.run(["debugfs", "-R", command, str(image)],
                                capture_output=True, check=True)
        errors = result.stderr.decode().splitlines()[1:]
        if errors:
            raise RuntimeError("\n".join(errors))
        return result.stdout

    source = debugfs(f"cat {SCRIPT_PATH}")
    if NEW_BRANCH not in source or patched_script(source) != source:
        raise RuntimeError("UIM helper must skip successfully when SIM cards are absent")
    subprocess.run(["bash", "-n"], input=source, check=True)
    metadata = debugfs(f"stat {SCRIPT_PATH}").decode()
    if (not re.search(r"Type:\s+regular\s+Mode:\s+0755", metadata)
            or not re.search(r"User:\s+0\s+Group:\s+0", metadata)):
        raise RuntimeError(f"{SCRIPT_PATH} must be root-owned, regular, mode 0755")
    verification = {
        "image": str(image), "script": SCRIPT_PATH,
        "no_sim_exit_status": 0, "no_sim_message": "无卡，跳过配置",
        "script_sha256": hashlib.sha256(source).hexdigest(),
        "hardware_tested": False,
    }
    report.write_text(json.dumps(verification, indent=2, ensure_ascii=False) + "\n")
    print("Modem UIM image verified: absent SIM skips provisioning successfully")
    return verification


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: verify-modem-uim-image.py IMAGE_DIRECTORY LOG_DIRECTORY")
    verify(*map(Path, sys.argv[1:]))
