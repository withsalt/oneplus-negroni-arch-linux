#!/usr/bin/env python3
"""Fingerprint exactly the kernel artifacts copied into the root image."""
import hashlib
from pathlib import Path

output = Path(__file__).resolve().parents[1] / "linux/output"
release = (output / "include/config/kernel.release").read_text().strip()
module_root = output / "out-modules/lib/modules" / release
if not module_root.is_dir():
    raise SystemExit(f"missing installed modules: {module_root}")
files = [output / "arch/arm64/boot/Image.gz", output / ".config", output / "System.map",
         output / "arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb"]
files += sorted(p for p in module_root.rglob("*") if p.is_file() and not p.is_symlink())
digest = hashlib.sha256(release.encode())
for path in files:
    digest.update(str(path.relative_to(output)).encode() + b"\0")
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
print(digest.hexdigest())
