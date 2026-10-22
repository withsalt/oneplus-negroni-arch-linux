#!/usr/bin/env python3
"""Read the final ext4 image without mounting it; verify the opt-in policy."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

project = Path(__file__).resolve().parents[1]
image_dir, log_dir = map(Path, sys.argv[1:])
root_image = image_dir / "root.img"
release = (log_dir / "kernel-release.txt").read_text().strip()
service = "negroni-charge-limit.service"


def debugfs(command):
    result = subprocess.run(["debugfs", "-R", command, str(root_image)],
                            capture_output=True, text=True, check=True)
    # debugfs reports many command failures only on stderr, with exit status 0.
    errors = result.stderr.splitlines()[1:]
    if errors:
        raise RuntimeError("\n".join(errors))
    return result.stdout


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


files = {
    "/etc/negroni-charge-limit.conf": "config/charge-limit/negroni-charge-limit.conf",
    "/usr/local/libexec/negroni-charge-limit": "config/charge-limit/negroni-charge-limit",
    f"/etc/systemd/system/{service}": f"config/systemd/{service}",
    f"/usr/lib/modules/{release}/kernel/drivers/power/supply/qcom_battmgr.ko":
        "linux/output/drivers/power/supply/qcom_battmgr.ko",
    f"/usr/lib/modules/{release}/kernel/drivers/power/supply/oplus_battmgr.ko":
        "linux/output/drivers/power/supply/oplus_battmgr.ko",
    "/boot/Image.gz": "linux/output/arch/arm64/boot/Image.gz",
}
checksums = {}
with tempfile.TemporaryDirectory(prefix="negroni-image-check-") as tmp:
    for index, (target, source) in enumerate(files.items()):
        extracted = Path(tmp) / f"file-{index}{Path(target).suffix}"
        debugfs(f'dump "{target}" "{extracted}"')
        if digest(extracted) != digest(project / source):
            raise RuntimeError(f"image/source mismatch: {target}")
        checksums[target] = digest(extracted)
        if target.endswith(".ko"):
            vermagic = subprocess.check_output(
                ["modinfo", "-F", "vermagic", str(extracted)], text=True).strip()
            if vermagic.split()[0] != release:
                raise RuntimeError(f"wrong module vermagic: {vermagic}")
        if target.endswith("negroni-charge-limit.conf"):
            if not all(line in extracted.read_text().splitlines()
                       for line in ("START_THRESHOLD=75", "END_THRESHOLD=80")):
                raise RuntimeError("default thresholds are not 75/80")

metadata = debugfs("stat /usr/local/libexec/negroni-charge-limit")
if not re.search(r"Mode:\s+0755", metadata) or not re.search(r"User:\s+0\s+Group:\s+0", metadata):
    raise RuntimeError("service helper must be executable and owned by root")

# Follow real directories, not symlinks, and check any enablement/alias link.
directories = ["/etc/systemd/system"]
while directories:
    directory = directories.pop()
    for line in debugfs(f'ls -p "{directory}"').splitlines():
        if not line.startswith("/"):
            continue
        _, inode, mode, uid, gid, name, *_ = line.split("/")
        if name in (".", "..") or inode == "0":
            continue
        path = f"{directory}/{name}"
        kind = int(mode, 8) & 0o170000
        if kind == 0o040000:
            directories.append(path)
        elif kind == 0o120000:
            if name == service or service in debugfs(f'stat "{path}"'):
                raise RuntimeError(f"charge-limit service is enabled or aliased: {path}")

verification = {
    "image": str(root_image),
    "files_match_sources": True,
    "installed_file_sha256": checksums,
    "module_vermagic": vermagic,
    "service_enabled": False,
    "default_start_percent": 75,
    "default_end_percent": 80,
    "helper_executable_owned_by_root": True,
    "hardware_tested": False,
}
(log_dir / "charge-limit-verification.json").write_text(json.dumps(verification, indent=2) + "\n")
print("Charge-limit image verified: matching driver/helper/config/unit, defaults 75/80, service disabled")
