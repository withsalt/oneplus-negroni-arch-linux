# 刷机说明

本流程适用于已核对固件、启动布局和分区条件的 OnePlus 10 Pro。已记录的实机为 **NE2210**，不代表其他变体已验证。操作基于 [postmarketOS Wiki 固定版本](https://wiki.postmarketos.org/index.php?title=OnePlus_10_Pro_%28oneplus-negroni%29&oldid=106143)。

**写入 rootfs 会覆盖 `userdata`，清除当前 Android 用户数据或 Linux 根文件系统。** 请先保存需要的数据并准备适合具体型号的恢复方案。本仓库不包含原厂固件、ABL 或设备分区备份。

当前完整镜像包含尚未真机验证的充电限制代码，服务默认关闭。早期 7.3-rc2 的刷写成功记录不能替代当前镜像的启动验证，详见[设备状态](status.md)。

## 必须先核对的条件

| 项目 | 要求 |
| --- | --- |
| 设备身份 | 核对具体 NE221x 型号与目标设备序列号；`product=taro` 本身不足以确认机型 |
| 固件升级历史 | 查明是否曾升级到 OOS/COS 15.0.0.1302 或更高版本，见下方 qbootctl 限制 |
| Bootloader | 已解锁，处于 bootloader fastboot；`is-userspace=yes` 表示 fastbootd |
| ABL / 固件组合 | 能支持本流程的 boot / vendor_boot / recovery 布局；不能因机型相同就假定 ABL 适用 |
| 槽位与分区 | 重新查询当前槽位、分槽情况与容量，不沿用历史设备的槽位 |
| 根设备映射 | 当前打包脚本使用 `root=/dev/sda14`，必须与目标设备的主线 Linux 映射相符 |
| 数据与恢复 | 保存需要的数据，准备匹配具体机型的恢复材料 |

### qbootctl 限制

Wiki 和[上游 issue #4](https://github.com/linux-msm/qbootctl/issues/4)警告：**曾升级到 OOS/COS 15.0.0.1302 或更高版本的设备不能使用 qbootctl，存在变砖风险；降级不能消除这一条件，该版本还会失去免费 EDL 恢复条件。**

镜像安装的 `msm-firmware-loader` 会调用 `qbootctl -a` 获取槽位。若有上述升级历史或无法确认历史，不应直接执行本流程；需先验证适合该固件的启动与固件加载方案并调整镜像。原记录中的 NE2210 由用户确认原厂历史为 ColorOS 14、未升级到 15，这一结论仅适用于当时设备。

### ABL 与根分区

本镜像按教程的 Android 12 ABL 布局封装：DTB 在 `vendor_boot`，内核和 initramfs 在 `boot`，`recovery` 仅含 ramdisk，依赖 boot 的内核与命令行。`fastboot boot boot.img` 不会同时配置其余镜像，不能作为完整刷入验证。

若设备无法进入合适的 fastboot 模式，先阅读 [Wiki 启动模式说明](https://wiki.postmarketos.org/index.php?title=OnePlus_10_Pro_%28oneplus-negroni%29&oldid=106143#How_to_enter_flash_mode)。本项目未验证自动回退或更换 ABL 的流程。

在可用的 root recovery 中，`readlink -f /dev/block/by-name/userdata` 和 `ls -l /dev/block/by-name/` 可辅助检查布局，但 Android recovery 的设备名仍需结合主线 UFS/LUN 枚举核对。若目标根设备不是 `/dev/sda14`，先修改 [pack-images.sh](../scripts/pack-images.sh) 中的 `root=` 并重新封装、校验；历史记录未单独保存主线系统中的根分区映射实测结果。

## 产物与分区

所有产物来自同一次构建，当前分支的输出目录为本地 `images/7.3/`。大小与哈希以该次 `SHA256SUMS` 和 `logs/7.3/build-result.json` 为准，不使用历史文档中的固定哈希。

| 文件 | 写入分区 / 用途 |
| --- | --- |
| `boot.img` | `boot`：内核、initramfs 与命令行 |
| `vendor_boot.img` | `vendor_boot`：设备树，无 vendor ramdisk |
| `recovery.img` | `recovery`：ramdisk |
| `vbmeta.img` | 同一文件分别写入 `vbmeta`、`vbmeta_vendor`、`vbmeta_system`，Flags=2 |
| `sparse_root.img` | `userdata`：Android sparse 根文件系统 |
| `root.img` | 原始 ext4 镜像，供本地检查与修改，不需另外刷一次 |
| `initramfs-linux.img` | 已封装进 boot/recovery，不单独刷入 |

展开后的 rootfs 为 **5 GiB（5,368,709,120 字节）**，userdata 容量需按展开大小判断。刷入更大分区不会自动扩容文件系统。

本流程按 Wiki 擦除 dtbo；没有可刷入的 `dtbo.img`。固件加载依赖设备保留的 modem/bluetooth/persist 等原厂内容，不要清空 `super`、`modem`、`persist`、`abl` 或其他未列为写入目标的分区。

## 1. 校验本地镜像

以下命令从项目根目录执行，主机需已安装 `adb`、`fastboot`：

```sh
(cd images/7.3 && sha256sum -c SHA256SUMS)
```

结果必须全部为 `OK`。重新生成或修改镜像后需使用新的校验清单。

## 2. 识别设备与分区

若手机仍运行 Android 且启用了 USB 调试，可以查询软件信息；已运行 Linux 或处于 fastboot 时跳过这组 ADB 命令：

```sh
adb devices -l
ADB_SERIAL='填写目标手机的 adb 序列号'
adb -s "$ADB_SERIAL" shell getprop ro.product.model
adb -s "$ADB_SERIAL" shell getprop ro.product.device
adb -s "$ADB_SERIAL" shell getprop ro.build.display.id
adb -s "$ADB_SERIAL" shell getprop ro.build.version.incremental
```

当前属性不能证明完整升级历史。需要备份原始启动分区时，应在已核实可读取这些分区的 root recovery 中，按真实分区名和大小保存；不要假定 bootloader 支持 `fastboot fetch`。个人数据应另行保存为可解密、可读取的文件。

只有设备仍运行 Android 时才通过 ADB 进入 bootloader；已经在 fastboot 中则直接查询：

```sh
adb -s "$ADB_SERIAL" reboot bootloader
```

```sh
fastboot devices -l
DEVICE_SERIAL='填写目标手机的 fastboot 序列号'
fastboot -s "$DEVICE_SERIAL" getvar product
fastboot -s "$DEVICE_SERIAL" getvar unlocked
fastboot -s "$DEVICE_SERIAL" getvar is-userspace
fastboot -s "$DEVICE_SERIAL" getvar current-slot
fastboot -s "$DEVICE_SERIAL" getvar slot-count
for partition in boot vendor_boot recovery dtbo vbmeta vbmeta_vendor vbmeta_system userdata; do
  fastboot -s "$DEVICE_SERIAL" getvar "has-slot:$partition"
done
fastboot -s "$DEVICE_SERIAL" getvar partition-size:userdata
```

fastboot 通常把查询结果写到 stderr。变量不受支持时，需用设备支持的方式补齐证据，不能直接视为通过。若仍处于 fastbootd，使用 `fastboot -s "$DEVICE_SERIAL" reboot bootloader` 切换后重新核对。

根据当前槽位与 `has-slot` 查询实际分区大小，例如仅在已确认槽位为 `a` 且 boot 分槽时查询 `partition-size:boot_a`；其他分区同理。每个启动分区都需能容纳对应镜像。记录设备、固件历史、解锁状态、模式、槽位、分区名称及容量后继续。

## 3. 刷入镜像

先读完整流程，再将两个占位符替换为已核实的值。以下是 **Bash** 命令；使用分区逻辑名，保持当前活动槽位，不切槽，也不写入所有槽位。必须先确认这些逻辑名解析到前面核实的目标分区。

```bash
set -euo pipefail
DEVICE_SERIAL='填写已核实的 fastboot 序列号'
EXPECTED_SLOT='填写已核实的当前槽位 a 或 b'
case "$EXPECTED_SLOT" in a|b) ;; *) echo '必须先填写已核实的槽位'; exit 1 ;; esac

(cd images/7.3 && sha256sum -c SHA256SUMS)
slot_report=$(fastboot -s "$DEVICE_SERIAL" getvar current-slot 2>&1)
printf '%s\n' "$slot_report"
actual_slot=$(printf '%s\n' "$slot_report" | sed -nE 's/.*current-slot:[[:space:]]*([ab])[[:space:]]*$/\1/p')
[[ "$actual_slot" == "$EXPECTED_SLOT" ]] || { echo '当前槽位与记录不一致，停止'; exit 1; }

fastboot -s "$DEVICE_SERIAL" erase dtbo
fastboot -s "$DEVICE_SERIAL" flash vbmeta images/7.3/vbmeta.img
fastboot -s "$DEVICE_SERIAL" flash vbmeta_vendor images/7.3/vbmeta.img
fastboot -s "$DEVICE_SERIAL" flash vbmeta_system images/7.3/vbmeta.img
fastboot -s "$DEVICE_SERIAL" flash vendor_boot images/7.3/vendor_boot.img
fastboot -s "$DEVICE_SERIAL" flash recovery images/7.3/recovery.img
fastboot -s "$DEVICE_SERIAL" flash boot images/7.3/boot.img

# 清除现有 userdata，再写入 Linux 根文件系统。
fastboot -s "$DEVICE_SERIAL" erase userdata
fastboot -s "$DEVICE_SERIAL" flash userdata images/7.3/sparse_root.img
```

每条命令均需成功；出现 USB 传输失败、分区不存在或容量不足时，停在 fastboot 保留输出并排查。全部成功后，再单独执行：

```sh
fastboot -s "$DEVICE_SERIAL" getvar current-slot
fastboot -s "$DEVICE_SERIAL" reboot
```

- 写入 rootfs 后不要执行 `fastboot -w`、`format userdata` 或再次 `erase userdata`。
- 保留 AVB Flags=2 与 bootloader 解锁状态，不要在安装自定义镜像后重新锁定。
- userdata 通常为共享分区；另一槽位不能保留被覆盖的 Android 用户数据，切槽也不是已验证的恢复方案。

## 4. 首次启动

预期进入 SDDM，默认会话为 Plasma Mobile，账号为 `oneplus`。密码从本地 `image-credentials.json` 读取；网络连接和 SSH 说明见[设备状态](status.md)。

在手机终端记录：

```sh
uname -r
findmnt /
lsblk -f
systemctl --failed
systemctl status sddm NetworkManager ModemManager msm-firmware-loader rmtfs tqftpserv gpu-runtime-active
cat /sys/bus/platform/devices/3d00000.gpu/power/control
journalctl -b -p warning
```

核对内核、根设备映射、虚拟键盘、联网及各项硬件功能。离线校验与刷写成功不等于全部硬件可用；充电限制默认关闭，需在确认新系统运行后按[专门说明](charge-limit.md)测试。

无法启动时先保存屏幕现象、槽位和刷入输出，检查 DTB、boot/recovery、根设备与 ABL 组合；根设备找不到与桌面服务失败应分别定位。历史命令及记录见[构建归档](archive/build-notes-2026-09-16.md)。
