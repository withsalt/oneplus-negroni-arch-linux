#!/usr/bin/env bash
# Apply the absent-SIM policy to an existing rootfs without rebuilding the kernel.
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
exec > >(tee "$log_dir/modem-uim-install.log") 2>&1
target_dir=$(mktemp -d /tmp/negroni-modem-uim-image.XXXXXX)
cleanup() {
    umount "$target_dir" 2>/dev/null || true
    rmdir "$target_dir" 2>/dev/null || true
}
trap cleanup EXIT
mount -o loop "$image_dir/root.img" "$target_dir"
bash "$project_dir/scripts/configure-modem-uim.sh" "$target_dir" "$log_dir"
sync -f "$target_dir/usr/bin/msm-modem-uim-selection.sh"
umount "$target_dir"
python3 scripts/verify-modem-uim-image.py "$image_dir" "$log_dir"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/modem-uim-install.log" \
    "$log_dir/modem-uim-verification.json"
