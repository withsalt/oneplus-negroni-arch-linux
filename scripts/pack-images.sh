#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
image_dir=${IMAGE_DIR:-images}
log_dir=${LOG_DIR:-logs}
[[ -f "$log_dir/kernel.complete" && -f "$log_dir/rootfs.complete" ]]
mkdir -p "$image_dir" "$log_dir"
exec > >(tee "$log_dir/image-pack.log") 2>&1
python3 scripts/verify-docker-image.py "$image_dir" "$log_dir"
python3 scripts/verify-screen-defaults-image.py "$image_dir" "$log_dir"
python3 scripts/verify-cpu-performance-image.py "$image_dir" "$log_dir"
python3 scripts/verify-cpu-performance-dtb.py linux/output/arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb "$log_dir"
if [[ -f "$log_dir/charge-limit-config.complete" ]]; then
    python3 scripts/verify-charge-limit-image.py "$image_dir" "$log_dir"
fi
python3 tools/avb/avbtool.py make_vbmeta_image --output "$image_dir/vbmeta.img" --flags 2
mkbootimg --header_version 4 --vendor_boot "$image_dir/vendor_boot.img" --dtb linux/output/arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb --pagesize 4096
mkbootimg --kernel linux/output/arch/arm64/boot/Image.gz --ramdisk "$image_dir/initramfs-linux.img" --header_version 4 --pagesize 4096 --os_version 12.0.0 --os_patch_level 2026-03-00 --cmdline 'console=tty0 root=/dev/sda14 rootwait rw bootconfig' -o "$image_dir/boot.img"
mkbootimg --ramdisk "$image_dir/initramfs-linux.img" --header_version 4 --pagesize 4096 --os_version 12.0.0 --os_patch_level 2026-03-00 -o "$image_dir/recovery.img"
e2fsck -fn "$image_dir/root.img"
img2simg "$image_dir/root.img" "$image_dir/sparse_root.img"
python3 scripts/verify-sparse.py "$image_dir/sparse_root.img" "$image_dir/root.img" "$log_dir"
python3 tools/avb/avbtool.py info_image --image "$image_dir/vbmeta.img"
mkdir -p "$log_dir/unpacked-boot" "$log_dir/unpacked-vendor-boot" "$log_dir/unpacked-recovery"
unpack_bootimg --boot_img "$image_dir/boot.img" --out "$log_dir/unpacked-boot" > "$log_dir/boot-header.txt"
unpack_bootimg --boot_img "$image_dir/vendor_boot.img" --out "$log_dir/unpacked-vendor-boot" > "$log_dir/vendor-boot-header.txt"
unpack_bootimg --boot_img "$image_dir/recovery.img" --out "$log_dir/unpacked-recovery" > "$log_dir/recovery-header.txt"
cmp linux/output/arch/arm64/boot/Image.gz "$log_dir/unpacked-boot/kernel"
cmp "$image_dir/initramfs-linux.img" "$log_dir/unpacked-boot/ramdisk"
cmp "$image_dir/initramfs-linux.img" "$log_dir/unpacked-recovery/ramdisk"
cmp linux/output/arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb "$log_dir/unpacked-vendor-boot/dtb"
(cd "$image_dir" && sha256sum boot.img vendor_boot.img recovery.img vbmeta.img root.img sparse_root.img initramfs-linux.img > SHA256SUMS)
python3 scripts/write-build-report.py "$image_dir" "$log_dir"
touch "$log_dir/images.complete"
