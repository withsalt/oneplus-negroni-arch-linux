#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
project_dir=$PWD
log_dir=${LOG_DIR:-logs}
mkdir -p "$log_dir"
exec > >(tee "$project_dir/$log_dir/kernel-build.log") 2>&1
cd linux
make_args=(O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-)
bash "$project_dir/scripts/configure-docker-kernel.sh" "$project_dir/linux/output"
make "${make_args[@]}" -j"${JOBS:-8}"
python3 "$project_dir/scripts/verify-cpu-performance-dtb.py" \
    output/arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb "$project_dir/$log_dir"
make "${make_args[@]}" INSTALL_MOD_PATH=out-modules modules_install
python3 "$project_dir/scripts/verify-docker-kernel.py" \
    "$project_dir/linux/output" "$project_dir/$log_dir"
make -s "${make_args[@]}" kernelrelease > "$project_dir/$log_dir/kernel-release.txt"
git rev-parse HEAD > "$project_dir/$log_dir/kernel-commit.txt"
cp output/.config "$project_dir/$log_dir/kernel.config"
git diff --binary HEAD > "$project_dir/$log_dir/kernel-local.patch"
for header in drivers/power/supply/qcom_battmgr_charge_limit.h include/trace/events/qcom_haptics.h; do
    if [[ -f $header ]] && ! git ls-files --error-unmatch "$header" >/dev/null 2>&1; then
        git diff --no-index -- /dev/null "$header" >> "$project_dir/$log_dir/kernel-local.patch" || [[ $? == 1 ]]
    fi
done
python3 "$project_dir/scripts/kernel-artifacts-id.py" > "$project_dir/$log_dir/kernel-artifacts.sha256"
touch "$project_dir/$log_dir/kernel.complete"
