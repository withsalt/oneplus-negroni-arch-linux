#!/usr/bin/env python3
"""Exercise the real offline installer and ext4 verifier as root (or in a user namespace)."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]
UNIT = "negroni-cpu-performance.service"
spec = importlib.util.spec_from_file_location("cpu_image", PROJECT / "scripts/verify-cpu-performance-image.py")
verifier = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verifier)


class CPUImageTests(unittest.TestCase):
    def setUp(self):
        if os.geteuid() != 0:
            self.fail("Run with sudo or unshare --user --map-root-user")
        self.temporary = tempfile.TemporaryDirectory(prefix="negroni-cpu-image-test-")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.root = self.base / "root"
        self.logs = self.base / "logs"
        for name in ("etc/systemd/system", "usr/bin", "usr/lib/systemd/system"):
            (self.root / name).mkdir(parents=True, exist_ok=True)
        python = self.root / "usr/bin/python3"
        python.write_text("#!/bin/sh\nexit 99\n")  # Offline install must never execute this.
        python.chmod(0o755)
        (self.root / "usr/lib/systemd/system/multi-user.target").write_text("[Unit]\nDescription=Multi-user\n")
        self.logs.mkdir()

    def install(self):
        env = dict(os.environ, SUDO_UID="0", SUDO_GID="0", LC_ALL="C")
        subprocess.run(["bash", str(PROJECT / "scripts/configure-cpu-performance.sh"),
                        str(self.root), str(self.logs)], check=True, env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def image(self):
        subprocess.run(["truncate", "-s", "32M", str(self.base / "root.img")], check=True)
        subprocess.run(["mkfs.ext4", "-q", "-F", "-d", str(self.root),
                        str(self.base / "root.img")], check=True, capture_output=True)

    def test_fresh_and_incremental_install(self):
        (self.logs / "images.complete").touch()
        self.install()
        self.assertFalse((self.logs / "images.complete").exists())
        self.assertTrue((self.logs / "images.complete.before-cpu-performance").exists())
        subprocess.run(["sha256sum", "--check", "--status",
                        str(self.logs / "cpu-performance-sources.sha256")], cwd=PROJECT, check=True)
        self.image()
        self.assertTrue(verifier.verify(self.base, self.logs)["service_enabled"])
        # Simulate an old image with a stale helper and no startup link.
        (self.root / "usr/local/sbin/negroni-cpu-performance").write_text("old helper\n")
        (self.root / f"etc/systemd/system/multi-user.target.wants/{UNIT}").unlink()
        (self.logs / "images.complete").touch()
        self.install()
        self.image()
        self.assertTrue(verifier.verify(self.base, self.logs)["service_enabled"])
        self.assertFalse((self.logs / "images.complete").exists())

    def test_disabled_service_is_rejected(self):
        self.install()
        (self.root / f"etc/systemd/system/multi-user.target.wants/{UNIT}").unlink()
        self.image()
        with self.assertRaises(RuntimeError):
            verifier.verify(self.base, self.logs)

    def test_wrong_helper_mode_is_rejected(self):
        self.install()
        (self.root / "usr/local/sbin/negroni-cpu-performance").chmod(0o644)
        self.image()
        with self.assertRaisesRegex(RuntimeError, "mode 0755"):
            verifier.verify(self.base, self.logs)

    def test_changed_unit_is_rejected(self):
        self.install()
        with (self.root / f"etc/systemd/system/{UNIT}").open("a") as stream:
            stream.write("\n# stale configuration\n")
        self.image()
        with self.assertRaisesRegex(RuntimeError, "image/source mismatch"):
            verifier.verify(self.base, self.logs)


if __name__ == "__main__":
    unittest.main()
