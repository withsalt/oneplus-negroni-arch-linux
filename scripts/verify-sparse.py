#!/usr/bin/env python3
"""Round-trip the final Android sparse image with the platform simg2img tool."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import sys

project = Path(__file__).resolve().parent.parent
image_dir = Path(sys.argv[1]).resolve().parent if len(sys.argv) > 1 else project / "images"
log_dir = Path(sys.argv[3]).resolve() if len(sys.argv) > 3 else project / "logs"
sparse_path = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else image_dir / "sparse_root.img"
original_path = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else image_dir / "root.img"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


with tempfile.TemporaryDirectory(prefix="sparse-check-", dir=log_dir) as directory:
    restored = Path(directory) / "root.img"
    subprocess.run([
        "simg2img", str(sparse_path), str(restored)
    ], check=True)
    original = original_path
    original_hash = digest(original)
    restored_hash = digest(restored)
    if original.stat().st_size != restored.stat().st_size or original_hash != restored_hash:
        raise SystemExit("Android sparse image did not reconstruct the original root filesystem")
    report = {
        "check": "simg2img round-trip",
        "result": "passed",
        "size_bytes": restored.stat().st_size,
        "original_sha256": original_hash,
        "restored_sha256": restored_hash,
    }
    (log_dir / "sparse-roundtrip.json").write_text(json.dumps(report, indent=2) + "\n")
    print("Android sparse image round-trip passed.")
