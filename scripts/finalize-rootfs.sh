#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
image_dir=${IMAGE_DIR:-images}
log_dir=${LOG_DIR:-logs}
[[ $EUID == 0 ]] || { echo 'Run this script with sudo.' >&2; exit 1; }
[[ -f "$log_dir/kernel.complete" && -f "$log_dir/rootfs.packages-complete" ]]
if [[ ${1:-} != --private-mounts ]]; then
    exec unshare --mount --propagation private bash "$0" --private-mounts
fi
exec > >(tee "$project_dir/$log_dir/rootfs-finalize.log") 2>&1
export LC_ALL=C
mkdir -p "$image_dir" "$log_dir"
target_dir=$project_dir/alarm/mnt
kernel_release=$(cat "$log_dir/kernel-release.txt")
mount -o loop "$image_dir/root.img" "$target_dir"
mount -t proc proc "$target_dir/proc"
mount --rbind /sys "$target_dir/sys"
mount --make-rslave "$target_dir/sys"
mount --rbind /dev "$target_dir/dev"
mount --make-rslave "$target_dir/dev"
mount -t tmpfs tmpfs "$target_dir/run"
trap 'umount -R "$target_dir/dev" 2>/dev/null || true; umount -R "$target_dir/sys" 2>/dev/null || true; umount "$target_dir/proc" "$target_dir/run" 2>/dev/null || true; umount "$target_dir" 2>/dev/null || true' EXIT
cp -a linux/output/out-modules/lib/modules/. "$target_dir/usr/lib/modules/"
cp linux/output/arch/arm64/boot/Image.gz "$target_dir/boot/Image.gz"
cp linux/output/.config "$target_dir/boot/config-$kernel_release"
cp linux/output/System.map "$target_dir/boot/System.map-$kernel_release"
chroot "$target_dir" depmod -a "$kernel_release"
printf 'oneplus-negroni\n' > "$target_dir/etc/hostname"
printf 'KEYMAP=us\n' > "$target_dir/etc/vconsole.conf"
printf 'LANG=en_US.UTF-8\n' > "$target_dir/etc/locale.conf"
sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' "$target_dir/etc/locale.gen"
chroot "$target_dir" locale-gen
ln -sfn /usr/share/zoneinfo/Asia/Shanghai "$target_dir/etc/localtime"
cat > "$target_dir/etc/fstab" <<'EOF'
LABEL=root / ext4 defaults,noatime 0 1
EOF
cat > "$target_dir/etc/hosts" <<'EOF'
127.0.0.1 localhost
::1 localhost
127.0.1.1 oneplus-negroni
EOF
if ! chroot "$target_dir" id oneplus >/dev/null 2>&1; then
    chroot "$target_dir" useradd -m -G wheel,audio,video,input -s /bin/bash oneplus
fi
python3 - "$target_dir" <<'PY'
import json,subprocess,sys
with open('image-credentials.json') as f:
    c=json.load(f)
subprocess.run(['chroot',sys.argv[1],'chpasswd'],input=f"{c['username']}:{c['password']}\nroot:{c['root_password']}\n",text=True,check=True)
PY
install -d -m 750 "$target_dir/etc/sudoers.d"
printf '%%wheel ALL=(ALL:ALL) ALL\n' > "$target_dir/etc/sudoers.d/10-wheel"
chmod 440 "$target_dir/etc/sudoers.d/10-wheel"
chroot "$target_dir" visudo -c
bash "$project_dir/scripts/configure-ssh.sh" "$target_dir" "$project_dir/$log_dir"
bash "$project_dir/scripts/configure-gpu-runtime-active.sh" "$target_dir" "$project_dir/$log_dir"
bash "$project_dir/scripts/configure-charge-limit.sh" "$target_dir" "$project_dir/$log_dir"
install -d "$target_dir/etc/sddm.conf.d"
cat > "$target_dir/etc/sddm.conf.d/10-mobile.conf" <<'EOF'
[General]
DisplayServer=wayland
InputMethod=
GreeterEnvironment=QT_WAYLAND_SHELL_INTEGRATION=layer-shell

[Theme]
Current=breeze

[Wayland]
CompositorCommand=kwin_wayland --drm --no-lockscreen --no-global-shortcuts --locale1 --inputmethod /usr/bin/plasma-keyboard
EOF
install -d -o 0 -g 0 "$target_dir/var/lib/sddm"
cat > "$target_dir/var/lib/sddm/state.conf" <<'EOF'
[Last]
Session=/usr/share/wayland-sessions/plasma-mobile.desktop
User=oneplus
EOF
chroot "$target_dir" chown -R sddm:sddm /var/lib/sddm
chroot "$target_dir" systemctl set-default graphical.target
ln -sfn /run/NetworkManager/resolv.conf "$target_dir/etc/resolv.conf"
ln -sfn /proc/self/mounts "$target_dir/etc/mtab"
sed -i -E 's/^MODULES=\(([^)]*)\)/MODULES=(phy-qcom-qmp-ufs)/; s/\<(microcode|autodetect|kms)\>//g; s/  +/ /g' "$target_dir/etc/mkinitcpio.conf"
chroot "$target_dir" mkinitcpio -k "$kernel_release" -g /boot/initramfs-linux.img
cp "$target_dir/boot/initramfs-linux.img" "$image_dir/initramfs-linux.img"
chroot "$target_dir" pacman -Q > "$log_dir/rootfs-packages.txt"
chroot "$target_dir" systemctl list-unit-files --state=enabled --no-pager > "$log_dir/rootfs-enabled-services.txt"
chroot "$target_dir" lsinitcpio /boot/initramfs-linux.img > "$log_dir/initramfs-contents.txt"
test -f "$target_dir/usr/share/wayland-sessions/plasma-mobile.desktop"
test -x "$target_dir/usr/bin/kwin_wayland"
test -d "$target_dir/usr/share/sddm/themes/breeze"
test -f "$target_dir/usr/lib/firmware/ath11k/WCN6855/hw2.1/board-2.bin" || test -f "$target_dir/usr/lib/firmware/ath11k/WCN6855/hw2.1/board-2.bin.zst" || test -f "$target_dir/usr/lib/firmware/ath11k/WCN6855/hw2.1/board.bin"
grep -q 'phy-qcom-qmp-ufs.ko' "$log_dir/initramfs-contents.txt"
truncate -s 0 "$target_dir/etc/machine-id"
sync
touch "$log_dir/rootfs.complete"
cp "$log_dir/kernel-artifacts.sha256" "$log_dir/rootfs-kernel.sha256"
chown "${SUDO_UID:-1000}:${SUDO_GID:-1000}" "$image_dir/root.img" "$image_dir/initramfs-linux.img" "$log_dir/rootfs-finalize.log" "$log_dir/rootfs-packages.txt" "$log_dir/rootfs-enabled-services.txt" "$log_dir/initramfs-contents.txt" "$log_dir/rootfs.complete" "$log_dir/ssh.complete" "$log_dir/sshd-effective.conf" "$log_dir/gpu-runtime-pm-config.txt" "$log_dir/gpu-runtime-pm.complete"
