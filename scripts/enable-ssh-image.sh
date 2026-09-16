#!/usr/bin/env bash
# Add SSH to an existing image without recompiling its kernel or initramfs.
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
exec > >(tee "$log_dir/ssh-install.log") 2>&1
export LC_ALL=C
target_dir=$(mktemp -d "$project_dir/alarm/ssh-image.XXXXXX")
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
# Invalidate the packaged images before changing the rootfs.
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-ssh"
fi
# Reuse the existing repository database and signed package cache.
chroot "$target_dir" pacman -S --needed --noconfirm openssh
bash "$project_dir/scripts/configure-ssh.sh" "$target_dir" "$project_dir/$log_dir"
chroot "$target_dir" pacman -Q > "$log_dir/rootfs-packages.txt"
chroot "$target_dir" systemctl list-unit-files --state=enabled --no-pager \
    > "$log_dir/rootfs-enabled-services.txt"
sync
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir/ssh-install.log" \
    "$log_dir/sshd-effective.conf" "$log_dir/ssh.complete" \
    "$log_dir/rootfs-packages.txt" "$log_dir/rootfs-enabled-services.txt"
