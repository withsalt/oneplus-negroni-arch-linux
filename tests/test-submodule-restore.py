#!/usr/bin/env python3
"""Exercise bundle recovery with independent kernel histories and local remotes."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]


class RestoreTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="negroni-restore-test-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.env = dict(os.environ, GIT_ALLOW_PROTOCOL="file", GIT_CONFIG_NOSYSTEM="1",
                        GIT_CONFIG_GLOBAL="/dev/null", GIT_TERMINAL_PROMPT="0",
                        GIT_AUTHOR_NAME="Test", GIT_AUTHOR_EMAIL="test@example.invalid",
                        GIT_COMMITTER_NAME="Test", GIT_COMMITTER_EMAIL="test@example.invalid")
        self.source = self.root / "kernel-remote"
        self.source.mkdir()
        self.git(self.source, "init", "-b", "base")
        self.base = self.commit(self.source, "base", "base")
        self.git(self.source, "checkout", "-b", "7.3")
        self.upstream = self.commit(self.source, "upstream", "7.3")
        self.git(self.source, "checkout", "-b", "7.3-rc2", self.base)
        self.historical = self.commit(self.source, "historical", "rc2 fixes")
        self.git(self.source, "checkout", "-b", "legacy", self.upstream)
        self.git(self.source, "merge", "--no-ff", "7.3-rc2", "-m", "Legacy merge")
        self.legacy = self.git(self.source, "rev-parse", "HEAD").strip()
        self.git(self.source, "checkout", "7.3")
        self.pin = self.commit(self.source, "fixed", "reimplemented fixes")

        self.project = self.root / "project"
        self.project.mkdir()
        self.git(self.project, "init", "-b", "main")
        (self.project / "scripts").mkdir()
        shutil.copy2(PROJECT / "scripts/init-submodules.sh", self.project / "scripts")
        bundle = self.project / "patches/linux/local-7.3.bundle"
        bundle.parent.mkdir(parents=True)
        self.git(self.source, "bundle", "create", str(bundle), "7.3", "7.3-rc2",
                 "^" + self.upstream, "^" + self.base)
        modules = []
        for name, pin in (("linux", self.pin), ("tools/avb", self.base),
                          ("tools/msm-firmware-loader", self.base)):
            modules.append(f'[submodule "{name}"]\n\tpath = {name}\n'
                           f'\turl = {self.source}\n' +
                           ("\tbranch = 7.3\n" if name == "linux" else ""))
            self.git(self.project, "update-index", "--add", "--cacheinfo",
                     f"160000,{pin},{name}")
        (self.project / ".gitmodules").write_text("".join(modules))
        self.git(self.project, "add", ".gitmodules", "scripts", "patches")
        self.git(self.project, "commit", "-m", "Pin fixtures")

    def run_command(self, directory, *args, check=True):
        result = subprocess.run(args, cwd=directory, env=self.env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if check and result.returncode:
            self.fail(result.stdout)
        return result

    def git(self, directory, *args):
        return self.run_command(directory, "git", *args).stdout

    def commit(self, directory, name, content):
        (directory / name).write_text(content)
        self.git(directory, "add", name)
        self.git(directory, "commit", "-m", content)
        return self.git(directory, "rev-parse", "HEAD").strip()

    def restore(self, check=True):
        return self.run_command(self.project, "bash", "scripts/init-submodules.sh", check=check)

    def seed_checkout(self, commit):
        kernel = self.project / "linux"
        kernel.mkdir()
        self.git(kernel, "init")
        self.git(kernel, "remote", "add", "origin", str(self.source))
        self.git(kernel, "fetch", "--depth", "1", "origin", commit)
        self.git(kernel, "checkout", "-b", "7.3", commit)
        return kernel

    def assert_restored(self):
        kernel = self.project / "linux"
        self.assertEqual(self.git(kernel, "rev-parse", "HEAD").strip(), self.pin)
        self.assertEqual(self.git(kernel, "rev-parse", "migration/7.3").strip(), self.pin)
        self.assertEqual(self.git(kernel, "rev-parse", "migration/7.3-rc2").strip(), self.historical)
        self.assertEqual(self.git(self.project, "status", "--porcelain"), "")

    def test_fresh_clone_and_repeat(self):
        self.restore()
        self.assert_restored()
        self.restore()
        self.assert_restored()

    def test_pin_present_but_historical_objects_missing(self):
        kernel = self.seed_checkout(self.pin)
        missing = self.run_command(kernel, "git", "cat-file", "-e", self.historical, check=False)
        self.assertNotEqual(missing.returncode, 0)
        self.restore()
        self.assert_restored()

    def test_divergent_local_branch_and_old_migration_ref(self):
        kernel = self.seed_checkout(self.legacy)
        self.git(kernel, "update-ref", "refs/remotes/migration/7.3", self.legacy)
        self.restore()
        self.assert_restored()
        self.assertEqual(self.git(kernel, "rev-parse", "7.3").strip(), self.legacy)
        self.assertEqual(self.run_command(kernel, "git", "symbolic-ref", "-q", "HEAD",
                                         check=False).returncode, 1)

    def test_dirty_checkout_is_preserved(self):
        self.restore()
        path = self.project / "linux/fixed"
        path.write_text("local work")
        result = self.restore(check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("linux/ has local changes", result.stdout)
        self.assertEqual(path.read_text(), "local work")


if __name__ == "__main__":
    unittest.main()
