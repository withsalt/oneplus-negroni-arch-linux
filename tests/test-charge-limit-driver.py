#!/usr/bin/env python3
"""Compile the real driver policy/command functions with a fake ADSP transport."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "linux/drivers/power/supply/oplus_battmgr.c").read_text()
core = (root / "linux/drivers/power/supply/qcom_battmgr.c").read_text()
header = (root / "linux/drivers/power/supply/oplus_battmgr.h").read_text()
parts = []
for name in ("oplus_opcodes", "battery_property_id", "usb_property_id", "wireless_property_id"):
    start = header.index("enum " + name + " {")
    end = header.index("\n};", start) + 3
    parts.append(header[start:end])
shared = (root / "linux/drivers/power/supply/qcom_battmgr.h").read_text()
stubs = (root / "tests/charge-limit-driver-stubs.h").read_text()
structures = []
for name in ("message", "info", "status", "usb", "wireless"):
    start = shared.index("struct qcom_battmgr_" + name + " {")
    end = shared.index("\n};", start) + 3
    structures.append(shared[start:end])
parts.append(stubs.replace("/* SHARED_STRUCTURES */", "\n".join(structures)))
functions = [(core, "int qcom_battmgr_request_property"),
             (source, "static int oplus_battmgr_usb_wire_property"),
             (source, "static int oplus_battmgr_usb_logical_property"),
             (source, "static int oplus_battmgr_request_property_locked"),
             (source, "static int oplus_battmgr_apply_negroni_charge_limit"),
             (source, "static int oplus_battmgr_refresh_negroni_charge_limit"),
             (source, "static void battmgr_callback")]
for content, declaration in functions:
    start = content.index(declaration + "(")
    end = content.index("\n}\n", start) + 3
    parts.append(content[start:end])
parts.append((root / "tests/charge-limit-driver-main.c").read_text())
with tempfile.TemporaryDirectory(prefix="negroni-driver-test-") as tmp:
    path = Path(tmp) / "driver.c"
    path.write_text("\n".join(parts))
    binary = Path(tmp) / "driver-test"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root / "tests"), str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
