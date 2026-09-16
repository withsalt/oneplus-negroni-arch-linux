#!/usr/bin/env python3
"""Require the tested SM8450 CPU capacity and power properties before packing."""
import json
from pathlib import Path
import subprocess
import sys


def verify(dtb, log_dir):
    cpus = []
    for cpu in range(8):
        node = "/cpus/cpu@" + ("0" if cpu == 0 else f"{cpu}00")
        expected = (1024, 100) if cpu < 4 else (2253, 251) if cpu < 7 else (2386, 396)
        values = {"cpu": cpu}
        for prop, value in zip(("capacity-dmips-mhz", "dynamic-power-coefficient"), expected):
            actual = int(subprocess.check_output(["fdtget", "-t", "u", str(dtb), node, prop], text=True))
            if actual != value:
                raise RuntimeError(f"{node}/{prop}: expected {value}, got {actual}")
            values[prop] = actual
        cpus.append(values)
    report = {"dtb": str(dtb), "cpu_capacity_properties_verified": True, "cpus": cpus,
              "hardware_tested": False}
    log_dir.mkdir(parents=True, exist_ok=True)
    (log_dir / "cpu-capacity-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print("CPU DTB verified: all 8 cores have the tested capacity and power values")
    return report


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("Usage: verify-cpu-performance-dtb.py DTB LOG_DIRECTORY")
    verify(*map(Path, sys.argv[1:]))
