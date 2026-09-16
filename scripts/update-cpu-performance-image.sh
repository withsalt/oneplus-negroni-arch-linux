#!/usr/bin/env bash
# Refresh the default CPU policy in an existing image without executing ARM code.
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
image_dir=${IMAGE_DIR:-images}
log_dir=${LOG_DIR:-logs}
[[ $EUID == 0 ]] || { echo 'Run this script as root.' >&2; exit 1; }
[[ -f "$image_dir/root.img" && -f "$log_dir/rootfs.complete" ]]
if [[ ${1:-} != --private-mounts ]]; then
    exec unshare --mount --propagation private bash "$0" --private-mounts
fi
mkdir -p "$log_dir"
exec > >(tee "$log_dir/cpu-performance-install.log") 2>&1
export LC_ALL=C
target_dir=$(mktemp -d /tmp/negroni-cpu-performance-image.XXXXXX)
cleanup() {
    umount "$target_dir" 2>/dev/null || true
    rmdir "$target_dir" 2>/dev/null || true
}
trap cleanup EXIT
mount -o loop "$image_dir/root.img" "$target_dir"
bash "$project_dir/scripts/configure-cpu-performance.sh" "$target_dir" "$log_dir"
sync -f "$target_dir/etc/systemd/system/negroni-cpu-performance.service"
umount "$target_dir"
python3 scripts/verify-cpu-performance-image.py "$image_dir" "$log_dir"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/cpu-performance-install.log" \
    "$log_dir/cpu-performance-verification.json"
