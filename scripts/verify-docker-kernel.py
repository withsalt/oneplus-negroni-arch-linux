#!/usr/bin/env python3
"""Check Docker configuration before build and installed modules after build."""
import json
from pathlib import Path
import sys

from docker_kernel import digest, verify_config, verify_modules


def verify(output, log_dir):
    config_path = output / ".config"
    config = verify_config(config_path.read_text())
    release = (output / "include/config/kernel.release").read_text().strip()
    symbols = {line.split()[1] for line in (output / "Module.symvers").read_text().splitlines()}
    if "fib6_lookup" not in symbols:
        raise RuntimeError("kernel must export fib6_lookup for nft_fib_ipv6")
    checksums = verify_modules(output / "out-modules", release, config)
    result = {
        "kernel_release": release,
        "config_sha256": digest(config_path),
        "required_configuration": True,
        "module_resolution_and_aliases": True,
        "fib6_lookup_exported": True,
        "module_sha256": checksums,
        "hardware_tested": False,
    }
    log_dir.mkdir(parents=True, exist_ok=True)
    (log_dir / "docker-kernel-verification.json").write_text(json.dumps(result, indent=2) + "\n")
    print(f"Docker kernel verified: {release}, {len(checksums)} modules and dependencies, automatic-load aliases")
    return result


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--config-only":
        verify_config(Path(sys.argv[2]).read_text())
        print("Docker kernel configuration verified")
    elif len(sys.argv) == 3:
        verify(*map(Path, sys.argv[1:]))
    else:
        raise SystemExit("Usage: verify-docker-kernel.py OUTPUT_DIRECTORY LOG_DIRECTORY\n"
                         "       verify-docker-kernel.py --config-only CONFIG")
