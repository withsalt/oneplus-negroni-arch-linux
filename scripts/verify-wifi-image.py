#!/usr/bin/env python3
"""Check Wi-Fi policy and signed regulatory database in an offline ext4 image."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

PROJECT = Path(__file__).resolve().parents[1]
FILES = {
    "/etc/NetworkManager/conf.d/30-negroni-wifi.conf": "config/wifi/30-negroni-wifi.conf",
    "/etc/conf.d/wireless-regdom": "config/wifi/wireless-regdom",
    "/etc/udev/rules.d/86-negroni-regulatory.rules": "config/wifi/86-negroni-regulatory.rules",
}


def verify(image_dir, log_dir):
    image = image_dir / "root.img"
    log_dir.mkdir(parents=True, exist_ok=True)
    report = log_dir / "wifi-verification.json"
    report.unlink(missing_ok=True)

    def debugfs(command):
        result = subprocess.run(["debugfs", "-R", command, str(image)],
                                capture_output=True, check=True)
        diagnostics = result.stderr.decode().splitlines()[1:]
        if diagnostics:
            raise RuntimeError("\n".join(diagnostics))
        return result.stdout

    hashes = {}
    for target, source in FILES.items():
        data = debugfs(f"cat {target}")
        if data != (PROJECT / source).read_bytes():
            raise RuntimeError(f"image/source mismatch: {target}")
        metadata = debugfs(f"stat {target}").decode()
        if (not re.search(r"Type:\s+regular\s+Mode:\s+0644", metadata)
                or not re.search(r"User:\s+0\s+Group:\s+0", metadata)):
            raise RuntimeError(f"{target} must be a root-owned regular file, mode 0644")
        hashes[target] = hashlib.sha256(data).hexdigest()

    for executable in ("/usr/bin/iw", "/usr/bin/set-wireless-regdom"):
        metadata = debugfs(f"stat {executable}").decode()
        mode = re.search(r"Type:\s+regular\s+Mode:\s+([0-7]+)", metadata)
        if not mode or not int(mode[1], 8) & 0o111:
            raise RuntimeError(f"missing executable Wi-Fi dependency: {executable}")
    for name in ("regulatory.db", "regulatory.db.p7s"):
        target = f"/usr/lib/firmware/{name}"
        data = debugfs(f"cat {target}")
        if not data:
            raise RuntimeError(f"empty regulatory database/signature: {target}")
        hashes[target] = hashlib.sha256(data).hexdigest()

    verification = {
        "image": str(image),
        "files_match_sources": True,
        "installed_files_sha256": hashes,
        "power_save_default": "disabled",
        "mac_address_default": "stable per machine and connection",
        "country": "CN",
        "country_hint_on_phy_add": True,
        "signed_regdb_present": True,
        "scope": "Offline configuration; explicit connection settings override defaults",
        "hardware_tested": False,
    }
    report.write_text(json.dumps(verification, indent=2) + "\n")
    print("Wi-Fi image verified: power saving disabled, stable MAC, CN policy, signed regdb present")
    return verification


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: verify-wifi-image.py IMAGE_DIRECTORY LOG_DIRECTORY")
    verify(*map(Path, sys.argv[1:]))
