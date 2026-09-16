#!/usr/bin/env python3
"""Compile the real driver policy/command functions with a fake ADSP transport."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "linux/drivers/power/supply/qcom_battmgr.c").read_text()
names = ["qcom_battmgr_request_property", "qcom_battmgr_apply_negroni_charge_limit", "qcom_battmgr_refresh_negroni_charge_limit"]
parts = [(root / "tests/charge-limit-driver-stubs.h").read_text()]
for name in names:
    start = source.index("static int " + name + "(")
    end = source.index("\n}\n", start) + 3
    parts.append(source[start:end])
parts.append((root / "tests/charge-limit-driver-main.c").read_text())
with tempfile.TemporaryDirectory(prefix="negroni-driver-test-") as tmp:
    path = Path(tmp) / "driver.c"
    path.write_text("\n".join(parts))
    binary = Path(tmp) / "driver-test"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root / "tests"), str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
