#!/usr/bin/env bash
# Refresh an existing root image after an incremental kernel/config build.
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
image_dir=${IMAGE_DIR:-images}
log_dir=${LOG_DIR:-logs}
[[ $EUID == 0 ]] || { echo 'Run this script as root.' >&2; exit 1; }
[[ -f "$image_dir/root.img" && -f "$log_dir/kernel-artifacts.sha256" ]]
if [[ ${1:-} != --private-mounts ]]; then
    exec unshare --mount --propagation private bash "$0" --private-mounts
fi
mkdir -p "$log_dir"
exec > >(tee "$log_dir/kernel-image-update.log") 2>&1
export LC_ALL=C
target_dir=$(mktemp -d /tmp/negroni-kernel-image.XXXXXX)
cleanup() {
    umount -R "$target_dir" 2>/dev/null || true
    rmdir "$target_dir" 2>/dev/null || true
}
trap cleanup EXIT
mount -o loop "$image_dir/root.img" "$target_dir"
mount -t proc proc "$target_dir/proc"
mount --rbind /dev "$target_dir/dev"
mount --make-rslave "$target_dir/dev"
mount --rbind /sys "$target_dir/sys"
mount --make-rslave "$target_dir/sys"
mount -t tmpfs tmpfs "$target_dir/run"
if [[ -f "$log_dir/images.complete" ]]; then
    mv "$log_dir/images.complete" "$log_dir/images.complete.before-kernel-update"
fi
release=$(cat "$log_dir/kernel-release.txt")
cp -a "linux/output/out-modules/lib/modules/$release" "$target_dir/usr/lib/modules/"
install -m 0644 linux/output/arch/arm64/boot/Image.gz "$target_dir/boot/Image.gz"
install -m 0644 linux/output/.config "$target_dir/boot/config-$release"
install -m 0644 linux/output/System.map "$target_dir/boot/System.map-$release"
chroot "$target_dir" depmod -a "$release"
bash "$project_dir/scripts/configure-charge-limit.sh" "$target_dir" "$project_dir/$log_dir"
chroot "$target_dir" mkinitcpio -k "$release" -g /boot/initramfs-linux.img
cp "$target_dir/boot/initramfs-linux.img" "$image_dir/initramfs-linux.img"
chroot "$target_dir" lsinitcpio /boot/initramfs-linux.img > "$log_dir/initramfs-contents.txt"
for module in qcom_battmgr oplus_battmgr; do
    cmp "linux/output/drivers/power/supply/$module.ko" "$target_dir/usr/lib/modules/$release/kernel/drivers/power/supply/$module.ko"
done
chroot "$target_dir" /usr/bin/python3 -c 'compile(open("/usr/local/libexec/negroni-charge-limit").read(), "negroni-charge-limit", "exec")'
chroot "$target_dir" systemctl list-unit-files --state=enabled --no-pager > "$log_dir/rootfs-enabled-services.txt"
sync
cp "$log_dir/kernel-artifacts.sha256" "$log_dir/rootfs-kernel.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$log_dir" "$image_dir/initramfs-linux.img" "$log_dir/kernel-image-update.log" "$log_dir/rootfs-kernel.sha256" "$log_dir/charge-limit-sources.sha256" "$log_dir/charge-limit-config.txt" "$log_dir/charge-limit-config.complete" "$log_dir/rootfs-enabled-services.txt" "$log_dir/initramfs-contents.txt"
