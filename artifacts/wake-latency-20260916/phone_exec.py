#!/usr/bin/env python3
"""Run a diagnostic command over SSH without logging credentials."""
import argparse
import json
from pathlib import Path
import shlex
import sys

import pexpect

parser = argparse.ArgumentParser()
parser.add_argument('--root', action='store_true')
parser.add_argument('--timeout', type=int, default=60)
parser.add_argument('--output', type=Path)
parser.add_argument('command')
args = parser.parse_args()
credentials = json.loads((Path(__file__).resolve().parents[2] / 'image-credentials.json').read_text())
command = args.command
if args.root:
    command = "sudo -S -p '__DIAG_SUDO__' bash -c " + shlex.quote(command)
child = pexpect.spawn('ssh', ['-T', '-o', 'StrictHostKeyChecking=accept-new', '-o',
    'ConnectTimeout=12', credentials['username'] + '@192.168.188.91', command],
    encoding='utf-8', timeout=args.timeout)
chunks = []
while True:
    match = child.expect([r'(?i)password:', '__DIAG_SUDO__', pexpect.EOF, pexpect.TIMEOUT])
    chunks.append(child.before)
    if match in (0, 1):
        child.setecho(False)
        child.sendline(credentials['password'])
    elif match == 2:
        break
    else:
        child.close(force=True)
        chunks.append('\nDIAGNOSTIC COMMAND TIMED OUT\n')
        break
child.close()
result = ''.join(chunks)
for key in ('password', 'root_password'):
    result = result.replace(credentials[key], '<redacted>')
if args.output:
    args.output.write_text(result)
sys.stdout.write(result)
sys.exit(child.exitstatus if child.exitstatus is not None else 124)
