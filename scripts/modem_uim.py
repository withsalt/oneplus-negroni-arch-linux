#!/usr/bin/env python3
"""Patch only the packaged UIM helper's confirmed-empty-SIM branch."""
from pathlib import Path
import sys

SCRIPT_PATH = "/usr/bin/msm-modem-uim-selection.sh"
OLD_BRANCH = b'''\tif [ "$count" -ge "$SIM_WAIT_TIME" ]
\tthen
\t\techo "No sim detected after $SIM_WAIT_TIME seconds."
\t\texit 4
\tfi
'''
NEW_BRANCH = '''\tif [ "$count" -ge "$SIM_WAIT_TIME" ]
\tthen
\t\techo '无卡，跳过配置'
\t\texit 0
\tfi
'''.encode("utf-8")


def patched_script(source):
    """Accept the known original or our patched branch; reject unknown versions."""
    if source.count(OLD_BRANCH) == 1 and NEW_BRANCH not in source:
        return source.replace(OLD_BRANCH, NEW_BRANCH, 1)
    if source.count(NEW_BRANCH) == 1 and OLD_BRANCH not in source:
        return source
    raise ValueError("Unrecognized UIM no-SIM branch; refusing to patch the script")


def patch(path):
    source = path.read_bytes()
    updated = patched_script(source)
    if updated != source:
        # Keep the package file's existing owner and permissions.
        path.write_bytes(updated)
    print(f"{path}: confirmed absent SIM is a successful skip")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("Usage: modem_uim.py PATH_TO_MSM_MODEM_UIM_SELECTION_SCRIPT")
    patch(Path(sys.argv[1]))
