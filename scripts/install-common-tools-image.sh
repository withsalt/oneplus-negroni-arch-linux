#!/usr/bin/env bash
# Add the standard terminal tools to a prepared rootfs image.
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
image_dir=${IMAGE_DIR:-images}
log_dir=${LOG_DIR:-logs}
[[ $EUID == 0 ]] || { echo 'Run this script as root.' >&2; exit 1; }
[[ -f "$image_dir/root.img" && -f "$log_dir/rootfs.packages-complete" ]]
if [[ ${1:-} != --private-mounts ]]; then
    exec unshare --mount --propagation private bash "$0" --private-mounts
fi
mkdir -p "$log_dir"
exec > >(tee "$log_dir/common-tools-install.log") 2>&1
export LC_ALL=C
mapfile -t packages < "$project_dir/config/packages/common-tools.txt"
(( ${#packages[@]} > 0 ))
target_dir=$(mktemp -d "$project_dir/alarm/tools-image.XXXXXX")
cleanup() {
    chroot "$target_dir" gpgconf --kill all 2>/dev/null || true
    umount -R "$target_dir" 2>/dev/null || true
    rmdir "$target_dir" 2>/dev/null || true
}
trap cleanup EXIT
mount -o loop "$image_dir/root.img" "$target_dir"
mount -t proc proc "$target_dir/proc"
mount --rbind /dev "$target_dir/dev"
mount --make-rslave "$target_dir/dev"
mount -t tmpfs tmpfs "$target_dir/run"
mount --bind "$project_dir/alarm/var/cache/pacman/pkg" "$target_dir/var/cache/pacman/pkg"
# The image uses NetworkManager's resolver. Provide DNS only in temporary /run.
[[ $(readlink "$target_dir/etc/resolv.conf") == /run/NetworkManager/resolv.conf ]]
mkdir -p "$target_dir/run/NetworkManager"
cp -L /etc/resolv.conf "$target_dir/run/NetworkManager/resolv.conf"
for marker in images.complete common-tools.list; do
    if [[ -f "$log_dir/$marker" ]]; then
        mv "$log_dir/$marker" "$log_dir/$marker.before-tools"
    fi
done
install -o 0 -g 0 -m 0644 config/pacman/mirrorlist "$target_dir/etc/pacman.d/mirrorlist"
# Keep the existing repository snapshot and verify signed packages normally.
chroot "$target_dir" pacman -S --needed --noconfirm "${packages[@]}"
chroot "$target_dir" pacman -Q "${packages[@]}" > "$log_dir/common-tools-versions.txt"
# Exercise ARM binaries and a ZIP/JSON/file-copy round trip in temporary RAM.
chroot "$target_dir" /bin/bash -euo pipefail <<'CHROOT'
work_dir=$(mktemp -d /run/common-tools-check.XXXXXX)
cd "$work_dir"
printf 'negroni common tools\n' > original.txt
nano --version >/dev/null
zip -q archive.zip original.txt
unzip -p archive.zip original.txt > restored.txt
cmp original.txt restored.txt
rsync -a original.txt copy.txt
cmp original.txt copy.txt
printf '{"ready":true}\n' | jq -e '.ready == true' >/dev/null
rg -q 'negroni common tools' copy.txt
fd --max-depth 1 '^copy\.txt$' | grep -Fxq copy.txt
git init -q repository
git -C repository rev-parse --is-inside-work-tree | grep -Fxq true
curl --version >/dev/null
wget --version >/dev/null
htop --version >/dev/null
less --version >/dev/null
file --version >/dev/null
tree --version >/dev/null
tmux -V >/dev/null
lsof -v >/dev/null 2>&1
strace -V >/dev/null
which nano >/dev/null
test -r /usr/share/bash-completion/bash_completion
echo 'Common tools verified: ARM executables, ZIP round trip, rsync copy, JSON, search, and Git repository.'
CHROOT
chroot "$target_dir" pacman -Q > "$log_dir/rootfs-packages.txt"
df -h "$target_dir"
sync
cp config/packages/common-tools.txt "$log_dir/common-tools.list"
python3 - "$log_dir" <<'PY'
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
logs = Path(sys.argv[1])
versions = dict(line.split(' ', 1) for line in (logs / 'common-tools-versions.txt').read_text().splitlines())
(logs / 'common-tools-verification.json').write_text(json.dumps({
    'result': 'passed',
    'checked_at': datetime.now(timezone.utc).isoformat(),
    'environment': 'ARM64 chroot under QEMU',
    'packages': versions,
    'checks': ['executables', 'zip_roundtrip', 'rsync_copy', 'jq_json', 'ripgrep_search', 'fd_search', 'git_repository', 'bash_completion_present'],
    'hardware_tested': False,
}, indent=2) + '\n')
PY
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/common-tools-install.log" \
    "$log_dir/common-tools-versions.txt" "$log_dir/common-tools.list" \
    "$log_dir/common-tools-verification.json" "$log_dir/rootfs-packages.txt"
