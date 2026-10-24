#!/usr/bin/env python3
"""Exercise real driver commands, notifications and workers with a fake ADSP."""
from pathlib import Path
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / "linux/drivers/power/supply/oplus_battmgr.c").read_text()
core = (root / "linux/drivers/power/supply/qcom_battmgr.c").read_text()
header = (root / "linux/drivers/power/supply/oplus_battmgr.h").read_text()
parts = []
for name in ("oplus_opcodes", "oplus_notifications", "battery_property_id", "usb_property_id", "wireless_property_id"):
    start = header.index("enum " + name + " {")
    end = header.index("\n};", start) + 3
    parts.append(header[start:end])
power_supply = (root / "linux/include/linux/power_supply.h").read_text()
start = power_supply.index("enum power_supply_property {")
parts.append(power_supply[start:power_supply.index("\n};", start) + 3])
shared = (root / "linux/drivers/power/supply/qcom_battmgr.h").read_text()
stubs = (root / "tests/charge-limit-driver-stubs.h").read_text()
structures = []
for name in ("enable_request", "message", "info", "status", "usb", "wireless"):
    start = shared.index("struct qcom_battmgr_" + name + " {")
    end = shared.index("\n};", start) + 3
    structures.append(shared[start:end])
parts.append(stubs.replace("/* SHARED_STRUCTURES */", "\n".join(structures)))
functions = [(core, "int qcom_battmgr_request_property"),
             (core, "void qcom_battmgr_pdr_notify"),
             (source, "static int oplus_battmgr_usb_wire_property"),
             (source, "static int oplus_battmgr_usb_logical_property"),
             (source, "static int oplus_battmgr_request_property_locked"),
             (source, "static int oplus_battmgr_request_property"),
             (source, "static int oplus_battmgr_apply_negroni_charge_limit"),
             (source, "static int oplus_battmgr_refresh_negroni_charge_limit"),
             (source, "static void oplus_battmgr_charge_limit_worker"),
             (source, "static int oplus_battmgr_negroni_set_threshold"),
             (source, "static int bat_update"),
             (source, "static int battery_get_property"),
             (source, "static int oplus_battmgr_usb_update"),
             (source, "static int oplus_ap_init_adsp_gague"),
             (source, "static int oplus_adsp_voocphy_enable"),
             (source, "static int smbchg_charging_disable"),
             (source, "static int smbchg_charging_enable"),
             (source, "static int smbchg_usb_suspend_enable"),
             (source, "static int smbchg_usb_suspend_disable"),
             (source, "static int oplus_adsp_voocphy_reset_again"),
             (source, "static void oplus_chg_status_send_adsp_work"),
             (source, "static void oplus_chg_update_work"),
             (source, "static void oplus_adsp_voocphy_err_work"),
             (source, "static void oplus_battmgr_notification_worker"),
             (source, "static void oplus_battmgr_notification"),
             (source, "static void battmgr_callback"),
             (source, "static void oplus_battmgr_callback"),
             (source, "static void oplus_battmgr_enable_worker"),
             (source, "static void oplus_adsp_crash_recover_func"),
             (source, "static void oplus_battmgr_stop_workers")]
for declaration in ("static const u8 usb_prop_map[USB_PROP_MAX] = {",
                    "static const u8 bat_prop_map[] = {"):
    start = source.index(declaration)
    parts.append(source[start:source.index("\n};", start) + 3])
for content, declaration in functions:
    start = content.index(declaration + "(")
    end = content.index("\n}\n", start) + 3
    parts.append(content[start:end])
parts.append((root / "tests/charge-limit-driver-main.c").read_text())
parts.append((root / "tests/charge-limit-driver-workers.c").read_text())
with tempfile.TemporaryDirectory(prefix="negroni-driver-test-") as tmp:
    path = Path(tmp) / "driver.c"
    path.write_text("\n".join(parts))
    binary = Path(tmp) / "driver-test"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(root / "tests"), str(path), "-o", str(binary)], check=True)
    for case in sys.argv[1:] or ("commands", "telemetry", "charge-status", "usb-input", "vooc-replay", "gauge-retry", "shutdown"):
        subprocess.run([str(binary), case], check=True)
