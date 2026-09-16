#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
log_dir=${LOG_DIR:-logs}
mkdir -p "$log_dir"
exec > >(tee "$project_dir/$log_dir/kernel-build.log") 2>&1
cd linux
make_args=(O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-)
if [[ ! -f output/.config ]]; then
    make "${make_args[@]}" -j"$(nproc)" defconfig
    make "${make_args[@]}" -j"$(nproc)" sm8450.config
fi
make "${make_args[@]}" -j"${JOBS:-8}"
make "${make_args[@]}" INSTALL_MOD_PATH=out-modules modules_install
make -s "${make_args[@]}" kernelrelease > "$project_dir/$log_dir/kernel-release.txt"
git rev-parse HEAD > "$project_dir/$log_dir/kernel-commit.txt"
cp output/.config "$project_dir/$log_dir/kernel.config"
git diff --binary HEAD > "$project_dir/$log_dir/kernel-local.patch"
if ! git ls-files --error-unmatch drivers/power/supply/qcom_battmgr_charge_limit.h >/dev/null 2>&1; then
    git diff --no-index -- /dev/null drivers/power/supply/qcom_battmgr_charge_limit.h >> "$project_dir/$log_dir/kernel-local.patch" || [[ $? == 1 ]]
fi
python3 "$project_dir/scripts/kernel-artifacts-id.py" > "$project_dir/$log_dir/kernel-artifacts.sha256"
touch "$project_dir/$log_dir/kernel.complete"
