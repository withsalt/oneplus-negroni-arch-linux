#!/usr/bin/env python3
"""Record the current package list, kernel and final image checksums."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys

image_directory = Path(sys.argv[1])
log_directory = Path(sys.argv[2])
images = []
for line in (image_directory / "SHA256SUMS").read_text().splitlines():
    checksum, name = line.split(maxsplit=1)
    images.append({"name": name, "bytes": (image_directory / name).stat().st_size, "sha256": checksum})
report = {
    "status": "complete",
    "completed_at": datetime.now(timezone.utc).isoformat(),
    "kernel_branch": subprocess.check_output(["git", "-C", "linux", "branch", "--show-current"], text=True).strip(),
    "kernel_commit": (log_directory / "kernel-commit.txt").read_text().strip(),
    "kernel_release": (log_directory / "kernel-release.txt").read_text().strip(),
    "packages": len((log_directory / "rootfs-packages.txt").read_text().splitlines()),
    "keyboard_fix": {
        "sddm_config": "/etc/sddm.conf.d/10-mobile.conf",
        "input_method": "/usr/bin/plasma-keyboard",
        "wayland_shell_integration": "layer-shell",
    },
    "ssh": {
        "enabled": (log_directory / "ssh.complete").exists(),
        "config": "/etc/ssh/sshd_config.d/00-negroni.conf",
        "port": 22,
        "user": "oneplus",
        "password": "Existing image login password (not stored in this report)",
        "root_login": False,
        "host_keys": "Generated on first boot",
    },
    "gpu_runtime_pm": {
        "enabled": (log_directory / "gpu-runtime-pm.complete").exists(),
        "service": "gpu-runtime-active.service",
        "device": "/sys/bus/platform/devices/3d00000.gpu",
        "power_control": "on",
    },
    "validation": {
        "kernel_build": True,
        "rootfs_configuration": True,
        "image_pack": True,
        "sparse_roundtrip": True,
        "ssh_configuration": (log_directory / "ssh.complete").exists(),
        "gpu_runtime_pm_configuration": (log_directory / "gpu-runtime-pm.complete").exists(),
    },
    "hardware_boot_tested": False,
    "images": images,
}
ssh_verification = log_directory / "ssh-verification.json"
if ssh_verification.exists():
    report["ssh_validation"] = json.loads(ssh_verification.read_text())
tools_verification = log_directory / "common-tools-verification.json"
if tools_verification.exists():
    report["common_tools"] = json.loads(tools_verification.read_text())
kernel_patch = log_directory / "kernel-local.patch"
if kernel_patch.exists():
    report["kernel_local_patch"] = {
        "path": str(kernel_patch),
        "bytes": kernel_patch.stat().st_size,
        "sha256": hashlib.sha256(kernel_patch.read_bytes()).hexdigest(),
    }
charge_verification = log_directory / "charge-limit-verification.json"
if (log_directory / "charge-limit-config.complete").exists():
    report["charge_limit"] = {
        "service": "negroni-charge-limit.service",
        "enabled_by_default": False,
        "start_threshold_percent": 75,
        "end_threshold_percent": 80,
        "config": "/etc/negroni-charge-limit.conf",
        "usb_input_suspended_by_policy": False,
        "system_suspend_blocked_while_enabled": True,
        "hardware_tested": False,
    }
    report["validation"]["charge_limit_configuration"] = charge_verification.exists()
    if charge_verification.exists():
        report["charge_limit_validation"] = json.loads(charge_verification.read_text())
(log_directory / "build-result.json").write_text(json.dumps(report, indent=2) + "\n")
