# CPU 性能模式与大小核调度修复

适用于已运行本项目 `7.3-rc2` 系统的 OnePlus 10 Pro **NE2210 / Negroni / SM8450**。2026-09-16 已在安装外部散热器的实机上完成部署、回滚测试和重启验证。原始结果见 [实测记录](../artifacts/cpu-performance-20260916/README.md)。

这项优化包含两个独立部分：

| 部分 | 改动 | 生效方式 |
| --- | --- | --- |
| CPU 性能服务 | 开启硬件已有 Boost，三组 CPU 使用 `performance`，频率上限取硬件支持值 | 固件默认安装并开机自启，停止服务可恢复 |
| 大小核设备树 | 补充 8 核的 `capacity-dmips-mhz` 和 `dynamic-power-coefficient` | 默认内核包含修复，刷入新的 vendor_boot 后生效 |

修复后的能力值为 **小核 276、大核 849、超大核 1024**；原配置错误地把全部核心标为 1024。超大核频率上限从 2.8416 GHz 提高到 2.9952 GHz，但硬件/固件仍可能限制实际频率。实测全核负载主要为 1.7856 / 2.4192 / 2.8416 GHz。热保护继续生效；性能模式可能增加待机耗电。

常规 `./build.sh 7.3` **默认集成并启用**这两项变更。新固件无需再通过 SSH 手动安装服务或单独打补丁。没有对 Android 原厂系统、其他槽位布局或其他 NE221x 型号进行验证。固件升级历史与 bootloader 条件沿用[刷机说明](flashing.md#必须先核对的条件)。

## 默认固件构建与续建

从当前仓库构建时执行：

```sh
bash scripts/init-submodules.sh
./build.sh 7.3
```

固定内核为 `7.3` 修复提交 `3ca85b51d20fc9287aec159998e6b871887bb839`，基于上游 `476a4a89843` 直接重做设备修复，8 核的 16 个能力与功耗参数与历史实测值一致。Git bundle 同时提供线性的 `7.3` 修复和独立的 `7.3-rc2` 历史引用，供下方对比脚本使用；rc2 提交不在新 `7.3` 的祖先历史中。初始化脚本会将可快进的 `7.3` 分支更新到固定版本；旧合并分支若已分叉则保留，固定源码检出为 detached HEAD。构建入口拒绝缺少固定修复的旧内核。

- 全新 rootfs：`finalize-rootfs.sh` 调用 `configure-cpu-performance.sh` 安装 helper / unit，并在镜像内启用 `multi-user.target.wants` 链接；构建期间不运行手机性能策略。
- 已有镜像：`build.sh` 检测 CPU 配置/脚本指纹缺失或变化时，调用 `update-cpu-performance-image.sh` 补装并启用服务，使旧封装标记失效。内核产物指纹包含 DTB，设备树变化也会触发镜像更新。
- 打包门槛：必须检查 rootfs 中 helper / unit 与源码一致、root 所有权和权限正确、自启链接存在，以及 DTB 中全部 16 个 CPU 参数正确。结果写入 `build-result.json` 的 `cpu_performance`、`cpu_capacity` 及 `validation`。

当前分支产物在 `images/7.3/`。首次或完整重刷按项目[刷机说明](flashing.md)使用配套 rootfs 和 vendor_boot。已有系统只需更新设备树及服务时，可以继续采用下方独立部署流程。重刷优化之前保存的旧镜像仍会撤销相应变更。

手动刷新已有 rootfs 的 CPU 服务（需先构建含修复的内核，最终打包会检查 DTB）：

```sh
sudo env IMAGE_DIR=images/7.3 LOG_DIR=logs/7.3 bash scripts/update-cpu-performance-image.sh
IMAGE_DIR=images/7.3 LOG_DIR=logs/7.3 bash scripts/pack-images.sh
```

独立服务可以随时 `disable --now`，但后续配置源文件变化所触发的镜像更新会重新应用默认启用策略。CPU 温控及硬件保护仍保留。

2026-09-16 已完成默认内核增量构建、干净目录的 Kbuild DTB 编译、Git bundle 恢复、4 项 rootfs 安装/拒绝测试、完整镜像封装与 sparse 往返校验，并成功执行常规 `./build.sh 7.3-rc2` 续建。本地 `images/7.3-rc2/` 已更新为默认启用版本，原产物保存在 `logs/7.3-rc2/pre-cpu-defaults/`。因宿主 sudo 需交互认证，本次用已验证的安装器在 user namespace 中生成配置，再以 debugfs 更新镜像副本；挂载式镜像更新脚本未在此主机实际运行。新完整镜像未再次刷入手机，手机已验证的现场服务和 DTB 与新镜像对应文件相同。[校验与产物摘要](../artifacts/cpu-performance-20260916/default-build.json)

以下独立步骤用于复现现场部署和恢复旧设备树，不是新固件启用性能所必需的步骤。

## 1. 构建主机：从固定源码生成镜像

从项目根目录执行。仅构建设备树，无需编译内核、下载 rootfs 或使用 root：

```sh
sudo apt-get install git python3 gcc tar patch device-tree-compiler mkbootimg
bash scripts/init-submodules.sh
python3 scripts/build-cpu-performance-dtb.py --output logs/cpu-performance-build
(cd logs/cpu-performance-build && sha256sum -c SHA256SUMS)
```

`--output` 必须指向空目录；再次构建时选一个新的目录。脚本通过 Git 对象导出固定提交 `2c092365fd5c72efcedc29a578601b0a41ce45c4`，在临时目录应用 [CPU 参数补丁](../patches/linux/sm8450-cpu-capacity.patch)，不会改动 `linux/` 工作树或其 gitlink。补丁参数来自[一加原厂固定版本设备树](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/877203a7cf9b79973d44a9db716490f95d820110/kernel_platform/qcom/proprietary/devicetree/qcom/waipio.dtsi)。

| 产物 | 用途 |
| --- | --- |
| `baseline.dtb` | 原始设备树，用于比较 |
| `sm8450-oneplus-negroni.dtb` | 补齐 CPU 能力和功耗模型的设备树 |
| `vendor_boot-original.img` | 与本次固定源码对应的原始启动设备树镜像 |
| `vendor_boot-cpu-capacity.img` | 待部署的优化镜像 |
| `SHA256SUMS`、`build.json`、`dtc.log` | 校验值、输入提交/工具版本和编译诊断 |

脚本会逐属性确认：原设备树内容保持一致，恰好增加 16 个预期属性；再检查封装镜像中的 DTB、无 vendor ramdisk 的 v4 header，以及两份 header 除 DTB 长度外完全一致。任一校验失败都会退出。

本次实测部署的镜像 SHA-256：

```text
vendor_boot-original.img      5a1d42ca81fd839b769baeba89ecac7feef1559fec1f11ba332cb6599c9fdce0
vendor_boot-cpu-capacity.img  203b7d18ae310cbb576b88962062e1d500b555aef04da5275fb33750109cb554
```

当前主机使用 DTC 1.7.2 可重新生成完全相同的镜像。工具版本变化可能改变二进制输出；应保留 `build.json`，通过属性检查和下文的手机原分区比较后再部署。

## 2. 安装可恢复的性能服务

构建主机上设置目标手机地址并传输两个配置文件。SSH 使用自己的密码或公钥，不要把本地凭据文件复制到手机或提交到 Git：

```sh
PHONE='oneplus@手机IP'
ssh "$PHONE" 'mkdir -p ~/cpu-performance'
scp config/cpu-performance/negroni-cpu-performance \
    config/systemd/negroni-cpu-performance.service \
    "$PHONE:cpu-performance/"
ssh "$PHONE"
```

接着在手机上执行；更新已有服务时先停止，让旧版本恢复原设置：

```bash
set -euo pipefail
cd ~/cpu-performance
if systemctl cat negroni-cpu-performance.service >/dev/null 2>&1; then
    sudo systemctl stop negroni-cpu-performance.service
fi
sudo install -d -m 0755 /usr/local/sbin
sudo install -m 0755 negroni-cpu-performance /usr/local/sbin/negroni-cpu-performance
sudo install -m 0644 negroni-cpu-performance.service /etc/systemd/system/
sudo systemd-analyze verify /etc/systemd/system/negroni-cpu-performance.service
sudo systemctl daemon-reload
sudo /usr/local/sbin/negroni-cpu-performance status > cpu-settings-before.json
sudo systemctl start negroni-cpu-performance.service
sudo /usr/local/sbin/negroni-cpu-performance status
```

应看到全局 Boost 和 policy7 Boost 为 `1`，三组 governor 为 `performance`，上限分别为 `1785600 / 2496000 / 2995200` kHz。policy0 和 policy4 没有 Boost 档位，局部 Boost 为 `0` 是正常结果。

验证恢复后再启用自启动：

```sh
sudo systemctl stop negroni-cpu-performance.service
sudo /usr/local/sbin/negroni-cpu-performance status > cpu-settings-restored.json
diff -u cpu-settings-before.json cpu-settings-restored.json
sudo systemctl enable --now negroni-cpu-performance.service
```

`diff` 应无输出；若有差异，应先排查而不是继续启用服务。服务启动时将 governor、频率上下限和 Boost 保存到 `/run/negroni-cpu-performance/state.json`；停止时恢复该次启动前的状态。sysfs 的频率更新是异步的，脚本会等待回读值生效。若服务报错，先读 `journalctl -u negroni-cpu-performance.service`；保留状态文件以便恢复。

## 3. 备份并核对手机原设备树

下文的分区名限定于**本次已验证的 Linux A 槽**。在手机上执行以下备份；已有同名备份时命令会退出，保留首次备份。备份前 1 MiB 覆盖本次 180224 字节镜像及其后续区域：

```bash
set -euo pipefail
cd ~/cpu-performance
umask 077
test ! -e vendor_boot_a-before-first-1MiB.img
sudo dd if=/dev/disk/by-partlabel/vendor_boot_a \
    of="$PWD/vendor_boot_a-before-first-1MiB.img" bs=1M count=1 status=none
sudo chown "$(id -u):$(id -g)" vendor_boot_a-before-first-1MiB.img
sha256sum vendor_boot_a-before-first-1MiB.img
```

回到构建主机，将备份另存一份，并校验手机当前镜像确实对应构建基线：

```sh
scp "$PHONE:cpu-performance/vendor_boot_a-before-first-1MiB.img" \
    logs/cpu-performance-build/
python3 - <<'PY'
from pathlib import Path
p = Path('logs/cpu-performance-build')
backup = (p / 'vendor_boot_a-before-first-1MiB.img').read_bytes()
original = (p / 'vendor_boot-original.img').read_bytes()
updated = (p / 'vendor_boot-cpu-capacity.img').read_bytes()
assert len(backup) == 1024 * 1024
assert len(original) <= len(backup) and len(updated) <= len(backup)
assert backup[:len(original)] == original, '手机镜像与固定源码不一致，停止部署'
print('原分区前缀一致，备份覆盖全部待写入区域')
PY
```

如果手机已应用这次修复，应使用此前保存的原始备份，跳过重复刷入；不要用优化后的镜像覆盖原始备份。若首次部署比较不一致，应先确认设备当前内核和设备树版本。

## 4. 更新设备树并重启

本次现场部署采用 SSH 下写分区并回读；可复现部署也可使用标准 bootloader fastboot，仅更新相同的 `vendor_boot_a` 镜像。准备 USB fastboot 连接，按[刷机说明的设备核对步骤](flashing.md#2-识别设备与分区)确认解锁状态、bootloader 模式、型号、A 槽和分区容量。

从手机进入 bootloader：

```sh
sudo systemctl reboot bootloader
```

在构建主机的同一个 Bash 会话执行。把序列号替换为 `fastboot devices -l` 中确认的目标设备：

```bash
set -euo pipefail
fastboot devices -l
DEVICE_SERIAL='填写已核实的 fastboot 序列号'
slot_report=$(fastboot -s "$DEVICE_SERIAL" getvar current-slot 2>&1)
printf '%s\n' "$slot_report"
actual_slot=$(printf '%s\n' "$slot_report" | sed -nE 's/.*current-slot:[[:space:]]*([ab])[[:space:]]*$/\1/p')
[[ "$actual_slot" == a ]] || { echo '不是已验证的 A 槽，停止'; exit 1; }
(cd logs/cpu-performance-build && sha256sum -c SHA256SUMS)
fastboot -s "$DEVICE_SERIAL" flash vendor_boot_a \
    logs/cpu-performance-build/vendor_boot-cpu-capacity.img
fastboot -s "$DEVICE_SERIAL" reboot
```

此次更新仅涉及设备树，不需要重刷 rootfs、boot、recovery 或清空 userdata。`fastboot` 路径使用同一份已验证镜像，但本次现场实测的写入与回读是通过 SSH 完成的。

## 5. 重启后的验证

手机恢复联网后，先在构建主机核对镜像回读值：

```sh
scp logs/cpu-performance-build/vendor_boot-cpu-capacity.img "$PHONE:cpu-performance/"
ssh "$PHONE"
```

在手机上执行，`cmp` 应无输出：

```bash
set -euo pipefail
cd ~/cpu-performance
image_bytes=$(stat -c %s vendor_boot-cpu-capacity.img)
sudo head -c "$image_bytes" /dev/disk/by-partlabel/vendor_boot_a > vendor_boot-readback.img
cmp vendor_boot-cpu-capacity.img vendor_boot-readback.img
```

继续检查系统状态：

```sh
cat /sys/devices/system/cpu/online
for p in /sys/devices/system/cpu/cpu[0-7]/cpu_capacity; do
    printf '%s=%s\n' "$p" "$(cat "$p")"
done
sudo /usr/local/sbin/negroni-cpu-performance status
systemctl is-enabled negroni-cpu-performance.service
systemctl is-active negroni-cpu-performance.service sddm.service
sudo ls /sys/kernel/debug/energy_model/
```

预期 8 核在线，小核 capacity 为 `276`、大核为 `849`、超大核为 `1024`；Energy Model 有 `cpu0`、`cpu4`、`cpu7` 三组。CPU 服务为 enabled / active。`performance` 下不以 EAS 开关值作为通过条件；能耗模型存在不等于当前 governor 启用了 EAS。

查看实际频率和温度：

```sh
for p in /sys/devices/system/cpu/cpufreq/policy*; do
    printf '%s request=%s hardware=%s\n' "$p" \
        "$(cat "$p/scaling_max_freq")" "$(cat "$p/cpuinfo_cur_freq")"
done
for z in /sys/class/thermal/thermal_zone*; do
    printf '%s=%s\n' "$(cat "$z/type")" "$(cat "$z/temp")"
done
```

频率单位为 kHz，thermal zone 温度为毫摄氏度。硬件读数低于请求上限时，不能仅凭 governor 为 performance 就宣称持续满频。

复现吞吐测试可执行以下命令。测试时另开终端观察 CPU、电池温度和 cooling state；本次自动采样工具采用 CPU 85°C / 电池 43°C 停止条件，手动执行时达到该值应结束负载：

```sh
taskset -c 7 openssl speed -elapsed -seconds 15 -evp sha256 -bytes 16384
openssl speed -elapsed -seconds 15 -evp sha256 -bytes 16384 -multi 8
# 需要验证持续负载时，另跑 90 秒
openssl speed -elapsed -seconds 90 -evp sha256 -bytes 16384 -multi 8
```

SHA-256 测试受指令集加速、后台任务、温度和固件调频影响。历史 +5.24% 是固定在 CPU7 上的 Boost 对比，不是通用整机增幅；调度修复后的测试不支持额外固定百分比的吞吐提升。要观察负载迁移，可先把自己的计算进程绑定 CPU0，运行 1 秒后用 `taskset -pc 0-7 进程PID` 放开亲和性，再用 `ps -o pid,psr,comm -p 进程PID` 采样；具体落在哪个大核取决于当时负载。

## 6. 恢复

只恢复 CPU governor 和 Boost，无需重启：

```sh
sudo systemctl disable --now negroni-cpu-performance.service
sudo /usr/local/sbin/negroni-cpu-performance status
```

恢复设备树需再重启。在已核对同一手机、同一 A 槽的 bootloader fastboot 中，将第 3 步比较通过的原始镜像刷回：

```bash
set -euo pipefail
slot_report=$(fastboot -s "$DEVICE_SERIAL" getvar current-slot 2>&1)
printf '%s\n' "$slot_report"
actual_slot=$(printf '%s\n' "$slot_report" | sed -nE 's/.*current-slot:[[:space:]]*([ab])[[:space:]]*$/\1/p')
[[ "$actual_slot" == a ]] || { echo '不是已验证的 A 槽，停止'; exit 1; }
(cd logs/cpu-performance-build && sha256sum -c SHA256SUMS)
fastboot -s "$DEVICE_SERIAL" flash vendor_boot_a \
    logs/cpu-performance-build/vendor_boot-original.img
fastboot -s "$DEVICE_SERIAL" reboot
```

本次现场备份的手机路径、SHA-256 和 SSH 下恢复命令另见[实测记录的恢复部分](../artifacts/cpu-performance-20260916/README.md#如需恢复原设备树)。关停性能服务与恢复设备树相互独立；完成这两项才会回到优化前的 CPU 配置。
