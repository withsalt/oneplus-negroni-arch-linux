#!/usr/bin/env bash
# Arm first-boot ext4 expansion in a mounted rootfs, without running the helper.
set -euo pipefail
project_dir=$(cd "$(dirname "$0")/.." && pwd)
target_dir=$(realpath -e -- "${1:?Usage: configure-grow-rootfs.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
log_dir=$(realpath -m -- "${2:?Usage: configure-grow-rootfs.sh ROOTFS_DIRECTORY LOG_DIRECTORY}")
[[ $EUID == 0 && $target_dir != / ]] || exit 1
for dependency in python3 findmnt lsblk blockdev dumpe2fs resize2fs; do
    test -x "$target_dir/usr/bin/$dependency"
done
mkdir -p "$log_dir"
rm -f "$log_dir/grow-rootfs-sources.sha256" "$log_dir/grow-rootfs-verification.json"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-grow-rootfs"
fi
install -d -o 0 -g 0 -m 0755 "$target_dir/usr/local/libexec" "$target_dir/etc/systemd/system"
install -o 0 -g 0 -m 0755 "$project_dir/config/grow-rootfs/negroni-grow-rootfs" \
    "$target_dir/usr/local/libexec/negroni-grow-rootfs"
install -o 0 -g 0 -m 0644 "$project_dir/config/systemd/negroni-grow-rootfs.service" \
    "$target_dir/etc/systemd/system/negroni-grow-rootfs.service"
# A reusable image must not carry the completion state from another boot.
rm -f "$target_dir/var/lib/negroni-grow-rootfs/done.json" "$target_dir/var/lib/negroni-grow-rootfs/done.tmp"
systemctl --root="$target_dir" enable negroni-grow-rootfs.service
[[ $(systemctl --root="$target_dir" is-enabled negroni-grow-rootfs.service) == enabled ]]
cd "$project_dir"
sha256sum config/grow-rootfs/negroni-grow-rootfs config/systemd/negroni-grow-rootfs.service \
    scripts/configure-grow-rootfs.sh scripts/update-grow-rootfs-image.sh \
    scripts/verify-grow-rootfs-image.py > "$log_dir/grow-rootfs-sources.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/grow-rootfs-sources.sha256"
