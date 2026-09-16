#!/usr/bin/env python3
"""Verify Docker modules and automatic loading in an unmounted rootfs image."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

from docker_kernel import PROJECT, digest, required_modules, verify_config, verify_modules


def verify(image_dir, log_dir):
    image = image_dir / "root.img"
    output = PROJECT / "linux/output"
    release = (log_dir / "kernel-release.txt").read_text().strip()
    if release != (output / "include/config/kernel.release").read_text().strip():
        raise RuntimeError("kernel release log does not match build output")

    def debugfs(command):
        result = subprocess.run(["debugfs", "-R", command, str(image)],
                                capture_output=True, check=True)
        errors = result.stderr.decode().splitlines()[1:]
        if errors:
            raise RuntimeError("\n".join(errors))
        return result.stdout

    with tempfile.TemporaryDirectory(prefix="negroni-docker-image-") as temporary:
        root = Path(temporary)
        files = {
            f"/boot/config-{release}": output / ".config",
            "/boot/Image.gz": output / "arch/arm64/boot/Image.gz",
            f"/boot/System.map-{release}": output / "System.map",
        }
        checksums = {}
        for index, (target, source) in enumerate(files.items()):
            extracted = root / f"boot-file-{index}"
            debugfs(f'dump "{target}" "{extracted}"')
            if digest(extracted) != digest(source):
                raise RuntimeError(f"image/build mismatch: {target}")
            checksums[target] = digest(extracted)
        config = verify_config((root / "boot-file-0").read_text())
        module_root = root / "lib/modules" / release
        module_root.mkdir(parents=True)
        image_modules = f"/usr/lib/modules/{release}"
        # Plain dump preserves bytes without rdump's privileged ownership changes.
        for line in debugfs(f'ls -p "{image_modules}"').decode().splitlines():
            if not line.startswith("/"):
                continue
            _, inode, mode, uid, gid, name, *_ = line.split("/")
            if name.startswith("modules.") and int(mode, 8) & 0o170000 == 0o100000:
                debugfs(f'dump "{image_modules}/{name}" "{module_root / name}"')
        dependencies = {}
        for line in (module_root / "modules.dep").read_text().splitlines():
            name, _, deps = line.partition(":")
            dependencies[Path(name)] = [Path(dep) for dep in deps.split()]
        pending = list(required_modules(config).values())
        extracted_modules = set()
        while pending:
            relative = pending.pop()
            if relative in extracted_modules:
                continue
            if relative.is_absolute() or ".." in relative.parts or relative not in dependencies:
                raise RuntimeError(f"invalid or missing module dependency record: {relative}")
            destination = module_root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            debugfs(f'dump "{image_modules}/{relative}" "{destination}"')
            extracted_modules.add(relative)
            pending.extend(dependencies[relative])
        module_checksums = verify_modules(root, release, config,
                                         output / "out-modules/lib/modules" / release)
    result = {
        "image": str(image), "kernel_release": release,
        "files_match_build": True,
        "boot_file_sha256": checksums,
        "module_sha256": module_checksums,
        "module_resolution_and_aliases": True,
        "scope": "Docker bridge kernel prerequisites; Docker packages are optional",
        "hardware_tested": False,
    }
    log_dir.mkdir(parents=True, exist_ok=True)
    (log_dir / "docker-image-verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Docker image verified: matching kernel and {len(module_checksums)} modules/dependencies, automatic-load aliases")
    return result


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: verify-docker-image.py IMAGE_DIRECTORY LOG_DIRECTORY")
    verify(*map(Path, sys.argv[1:]))
