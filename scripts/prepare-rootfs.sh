#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
image_dir=${IMAGE_DIR:-images}
log_dir=${LOG_DIR:-logs}
[[ $EUID == 0 ]] || { echo 'Run this script with sudo.' >&2; exit 1; }
if [[ ${1:-} != --private-mounts ]]; then
    exec unshare --mount --propagation private bash "$0" --private-mounts
fi
exec > >(tee -a "$project_dir/$log_dir/rootfs-build.log") 2>&1
export LC_ALL=C
mkdir -p alarm "$log_dir" "$image_dir"
if [[ ! -f "$log_dir/bootstrap.extracted" ]]; then
    tar --numeric-owner -xpf downloads/ArchLinuxARM-aarch64-latest.tar.gz -C alarm
    touch "$log_dir/bootstrap.extracted"
fi
if ! chroot alarm /bin/true; then
    /usr/lib/systemd/systemd-binfmt /usr/lib/binfmt.d/qemu-aarch64.conf
    chroot alarm /bin/true
fi
mkdir -p alarm/{proc,sys,dev,run,tmp,mnt}
mount --bind alarm alarm
mount -t proc proc alarm/proc
mount --rbind /sys alarm/sys
mount --make-rslave alarm/sys
mount --rbind /dev alarm/dev
mount --make-rslave alarm/dev
mount -t tmpfs tmpfs alarm/run
trap 'chroot "$project_dir/alarm" gpgconf --kill all 2>/dev/null || true; umount -R "$project_dir/alarm/mnt" 2>/dev/null || true; umount -R "$project_dir/alarm/dev" 2>/dev/null || true; umount -R "$project_dir/alarm/sys" 2>/dev/null || true; umount "$project_dir/alarm/proc" "$project_dir/alarm/run" 2>/dev/null || true; umount "$project_dir/alarm" 2>/dev/null || true' EXIT
if [[ -L alarm/etc/resolv.conf ]]; then unlink alarm/etc/resolv.conf; fi
cp -L /etc/resolv.conf alarm/etc/resolv.conf
ln -sfn /proc/self/mounts alarm/etc/mtab
printf 'KEYMAP=us\n' > alarm/etc/vconsole.conf
install -m 0644 config/pacman/mirrorlist alarm/etc/pacman.d/mirrorlist
sed -i -E 's/^#?(ParallelDownloads *= *).*/\18/; s/^#?(DisableSandboxFilesystem)/\1/; s/^#?(DisableSandboxSyscalls)/\1/' alarm/etc/pacman.conf
if ! grep -q '^\[xec\]' alarm/etc/pacman.conf; then
    cat >> alarm/etc/pacman.conf <<'EOF'

[xec]
SigLevel = Optional TrustAll
Server = https://xlie-electronic-customs.github.io/xec-arch-repo/
EOF
fi
chroot alarm /bin/bash -euo pipefail <<'CHROOT'
pacman-key --init
pacman-key --populate archlinuxarm
if pacman -Q linux-aarch64 >/dev/null 2>&1; then
    pacman -Rdd --noconfirm linux-aarch64
fi
mapfile -t firmware < <(pacman -Qq | grep '^linux-firmware' || true)
if (( ${#firmware[@]} )); then pacman -Rdd --noconfirm "${firmware[@]}"; fi
pacman -Syu --needed --noconfirm archlinuxarm-keyring archinstall arch-install-scripts mkinitcpio
CHROOT
if [[ ! -f "$image_dir/root.img" ]]; then
    mkdir -p "$image_dir"
    truncate -s 5G "$image_dir/root.img"
    mkfs.ext4 -F -L root "$image_dir/root.img"
fi
mount -o loop "$image_dir/root.img" alarm/mnt
chroot alarm /bin/bash -euo pipefail <<'CHROOT'
pacstrap -c /mnt --needed --noconfirm base pacman archlinuxarm-keyring systemd-sysvcompat networkmanager openssh python msm-firmware-loader msm-modem-uim-selection linux-firmware-oplus-negroni rmtfs-oplus tqftpserv
pacstrap -c /mnt --needed --noconfirm sddm plasma-mobile plasma-desktop bluedevil spacebar qmlkonsole plasma-dialer plasma-settings kscreen plasma-nm modemmanager mkinitcpio sudo qt6-virtualkeyboard
if pacman -r /mnt -Q iio-sensor-proxy >/dev/null 2>&1; then
    pacman -r /mnt -Rdd --noconfirm iio-sensor-proxy
fi
systemctl --root=/mnt enable sddm NetworkManager ModemManager msm-modem-uim-selection bluetooth systemd-timesyncd msm-firmware-loader msm-firmware-loader-unpack rmtfs tqftpserv
CHROOT
cp alarm/etc/pacman.conf alarm/mnt/etc/pacman.conf
cp alarm/etc/pacman.d/mirrorlist alarm/mnt/etc/pacman.d/mirrorlist
sync
touch "$log_dir/rootfs.packages-complete"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$image_dir/root.img" "$log_dir/rootfs-build.log" "$log_dir/bootstrap.extracted" "$log_dir/rootfs.packages-complete"
