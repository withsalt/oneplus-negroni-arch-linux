#!/usr/bin/env python3
"""Regression tests using the pinned kernel's real Kconfig, in isolated outputs."""
from pathlib import Path
import subprocess
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]
CONFIGURE = PROJECT / "scripts/configure-docker-kernel.sh"
VERIFY = PROJECT / "scripts/verify-docker-kernel.py"


def run(*args):
    result = subprocess.run(list(map(str, args)), cwd=PROJECT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise AssertionError(result.stdout)
    return result.stdout


class DockerConfigTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="negroni-docker-config-")
        self.addCleanup(self.temporary.cleanup)
        self.output = Path(self.temporary.name)

    def test_fresh_configuration_and_repeat_build(self):
        run("bash", CONFIGURE, self.output)
        config = (self.output / ".config").read_bytes()
        self.assertIn(b"CONFIG_BATTERY_OPLUS_BATTMGR=m\n", config)
        run("bash", CONFIGURE, self.output)
        self.assertEqual(config, (self.output / ".config").read_bytes())

    def test_existing_incompatible_configuration_is_repaired_without_reset(self):
        make = ("make", "-C", PROJECT / "linux", f"O={self.output}", "ARCH=arm64",
                "CROSS_COMPILE=aarch64-linux-gnu-")
        run(*make, "defconfig")
        run(*make, "sm8450.config")
        before = subprocess.run(["python3", str(VERIFY), "--config-only", str(self.output / ".config")],
                                capture_output=True, text=True)
        self.assertNotEqual(before.returncode, 0)
        self.assertIn("CONFIG_NFT_COMPAT", before.stderr)
        self.assertIn("CONFIG_NETFILTER_XT_TARGET_MASQUERADE", before.stderr)
        run("bash", PROJECT / "linux/scripts/config", "--file", self.output / ".config",
            "--set-str", "LOCALVERSION", "-sm8450-preserve-custom-setting")
        run("bash", PROJECT / "linux/scripts/config", "--file", self.output / ".config",
            "--disable", "BATTERY_OPLUS_BATTMGR")
        run("bash", CONFIGURE, self.output)
        self.assertIn("CONFIG_BATTERY_OPLUS_BATTMGR=m\n", (self.output / ".config").read_text())
        self.assertIn('CONFIG_LOCALVERSION="-sm8450-preserve-custom-setting"',
                      (self.output / ".config").read_text())

    def test_missing_container_isolation_is_rejected(self):
        run("bash", CONFIGURE, self.output)
        run("bash", PROJECT / "linux/scripts/config", "--file", self.output / ".config",
            "--disable", "NET_NS")
        result = subprocess.run(["python3", str(VERIFY), "--config-only", str(self.output / ".config")],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("CONFIG_NET_NS is required", result.stderr)


if __name__ == "__main__":
    unittest.main()
