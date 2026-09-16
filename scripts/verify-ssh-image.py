#!/usr/bin/env python3
"""Test the ARM image's sshd in private mount/network namespaces using QEMU.

Run as root via unshare --mount --net --propagation private. The rootfs is
mounted read-only; host keys, PID files and login state live on temporary RAM.
Requires the host's python3-pexpect package and registered ARM64 binfmt.
"""
import argparse
import ctypes
import ctypes.util
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import tempfile
import time

import pexpect


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("image", type=Path)
parser.add_argument("log_directory", type=Path)
args = parser.parse_args()
project = Path(__file__).resolve().parent.parent
image = args.image.resolve()
logs = args.log_directory.resolve()
assert os.geteuid() == 0
assert os.readlink("/proc/self/ns/net") != os.readlink("/proc/1/ns/net"), "Use a private network namespace"
credentials = json.loads((project / "image-credentials.json").read_text())
assert credentials["username"] == "oneplus"
logs.mkdir(parents=True, exist_ok=True)


def run(*command):
    return subprocess.run(command, check=True, capture_output=True, text=True)


run("ip", "link", "set", "lo", "up")
report = {"environment": "ARM64 sshd under QEMU, isolated loopback network", "hardware_tested": False}
with tempfile.TemporaryDirectory(prefix="negroni-ssh-test-") as temporary:
    directory = Path(temporary)
    target = directory / "rootfs"
    target.mkdir()
    server = None
    client = None
    mounted = False
    try:
        run("mount", "-o", "loop,ro", str(image), str(target))
        mounted = True
        run("mount", "-t", "tmpfs", "tmpfs", str(target / "run"))
        run("mount", "-t", "proc", "proc", str(target / "proc"))
        run("mount", "--rbind", "/dev", str(target / "dev"))
        run("mount", "--make-rslave", str(target / "dev"))
        assert not list((target / "etc/ssh").glob("ssh_host_*_key*"))
        assert run("chroot", str(target), "systemctl", "is-enabled", "sshd").stdout.strip() == "enabled"
        service = (target / "usr/lib/systemd/system/sshd.service").read_text()
        assert "sshdgenkeys.service" in service
        generator = (target / "usr/lib/systemd/system/sshdgenkeys.service").read_text()
        assert "ssh-keygen -A" in generator
        report.update(service_enabled=True, first_boot_key_generation=True, shared_host_keys_absent=True)
        # Check the existing password against the image's hash without printing
        # either value. This also catches an accidentally locked login account.
        shadow = next(line.split(":") for line in (target / "etc/shadow").read_text().splitlines()
                      if line.startswith("oneplus:"))
        stored_hash = shadow[1].encode()
        assert stored_hash.startswith(b"$")
        libcrypt = ctypes.CDLL(ctypes.util.find_library("crypt"))
        libcrypt.crypt.argtypes = (ctypes.c_char_p, ctypes.c_char_p)
        libcrypt.crypt.restype = ctypes.c_char_p
        assert libcrypt.crypt(credentials["password"].encode(), stored_hash) == stored_hash
        report["existing_password_hash_matches"] = True
        key_directory = target / "run/ssh-test"
        key_directory.mkdir(mode=0o700)
        key = key_directory / "host_key"
        run("ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-f", str(key))
        known_hosts = directory / "known_hosts"
        public_key = " ".join(key.with_suffix(".pub").read_text().split()[:2])
        known_hosts.write_text(f"[127.0.0.1]:2222 {public_key}\n")
        run("chroot", str(target), "/usr/bin/sshd", "-t", "-h", "/run/ssh-test/host_key")
        with (logs / "ssh-login-test-server.log").open("w") as server_log:
            server = subprocess.Popen([
                "chroot", str(target), "/usr/bin/sshd", "-D", "-e", "-p", "2222",
                "-h", "/run/ssh-test/host_key", "-o", "ListenAddress=127.0.0.1",
                "-o", "PidFile=/run/ssh-test/sshd.pid",
            ], stdout=server_log, stderr=subprocess.STDOUT, start_new_session=True)
            for _ in range(100):
                if server.poll() is not None:
                    raise RuntimeError("Image sshd exited before listening; see server log")
                try:
                    with socket.create_connection(("127.0.0.1", 2222), timeout=0.2):
                        break
                except OSError:
                    time.sleep(0.1)
            else:
                raise RuntimeError("Image sshd did not listen within 10 seconds")
            client = pexpect.spawn("ssh", [
                "-F", "/dev/null", "-p", "2222", "-o", "ConnectTimeout=10",
                "-o", "StrictHostKeyChecking=yes", "-o", f"UserKnownHostsFile={known_hosts}",
                "-o", "GlobalKnownHostsFile=/dev/null", "-o", "PreferredAuthentications=password",
                "-o", "PubkeyAuthentication=no", "-o", "NumberOfPasswordPrompts=1",
                "oneplus@127.0.0.1", "id -un && cat /etc/hostname",
            ], encoding="utf-8", timeout=30)
            client.expect(r"(?i)password:")
            client.sendline(credentials["password"])
            client.expect(pexpect.EOF)
            output = client.before.replace("\r", "")
            client.close()
            assert client.exitstatus == 0, "SSH command failed; see server log"
            assert "oneplus\noneplus-negroni\n" in output, "Unexpected remote identity"
            report.update(password_login=True, remote_command=True, result="passed")
    except Exception as error:
        # Do not log pexpect exception text: it may retain password input.
        report.update(result="failed", error_type=type(error).__name__)
        server_log_path = logs / "ssh-login-test-server.log"
        server_output = server_log_path.read_text() if server_log_path.exists() else ""
        if "prctl(PR_SET_SECCOMP): Invalid argument" in server_output:
            report.update(result="limited", password_login=False, remote_command=False,
                          limitation="QEMU user mode does not support sshd's PR_SET_SECCOMP sandbox")
        else:
            raise RuntimeError("SSH image login test failed; see the report and server log") from None
    finally:
        if client is not None and client.isalive():
            client.close(force=True)
        if server is not None and server.poll() is None:
            os.killpg(server.pid, signal.SIGTERM)
            try:
                server.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(server.pid, signal.SIGKILL)
                server.wait()
        if mounted:
            run("umount", "-R", str(target))
        (logs / "ssh-verification.json").write_text(json.dumps(report, indent=2) + "\n")
        for filename in ("ssh-verification.json", "ssh-login-test-server.log"):
            path = logs / filename
            if path.exists():
                os.chown(path, 1000, 1000)
if report["result"] == "passed":
    print("SSH image test passed: existing oneplus password and remote command verified.")
else:
    print("SSH configuration and password hash verified; login test blocked by QEMU seccomp support.")
