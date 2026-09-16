# Negroni 充电状态修复（2026-09-15）

## 原因和修改

手机固件提供的电池电流为“充电负、放电正”，与 Linux power_supply ABI 约定相反。修复前已实测：插线时 status=Charging、电流为负，UPower 却为 State=2（放电）；拔线时电流为 +138000 µA。UPower 1.91.4 会使用电流符号纠正充电状态，因此桌面没有显示充电图标。

在 `linux/drivers/power/supply/qcom_battmgr.c` 导出电池 CURRENT_NOW 时反转电流，仅对设备树兼容字符串 `oplus,negroni` 生效；标准属性和 OEM 电流读取路径均适用。改动仅涉及电流读数，不修改充电电压、电流限值或充电控制逻辑。

Linux ABI 依据：`linux/Documentation/ABI/testing/sysfs-class-power` 的 current_now 说明。
UPower 依据：https://gitlab.freedesktop.org/upower/upower/-/blob/v1.91.4/src/linux/up-device-supply-battery.c#L519

## 编译与安装

- 内核仓库：`https://github.com/withsalt/linux`。
- 内核源码：分支 `7.3-rc2`，提交 `593ad628e5b838457d6a316cc08ba5dcc924a01f`（包含本次修复）。
- 目标内核：`7.3.0-rc2-sm8450+`。
- 使用已有 output 配置及符号信息执行：

  ```sh
  make O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j4 drivers/power/supply/qcom_battmgr.ko
  ```

- 模块为 ELF64 AArch64，新旧 vermagic 一致，原模块文件与手机原安装文件哈希一致。
- 编译通过；保留了源码已有的两处 OTG 指针比较警告。`git diff --check` 通过。
- 手机原模块备份：`/var/backups/negroni-charging-20260915/qcom_battmgr.ko`。
- 安装位置：`/usr/lib/modules/7.3.0-rc2-sm8450+/kernel/drivers/power/supply/qcom_battmgr.ko`。
- 已原子替换文件、执行 depmod 并重启。该模块不在现有 initramfs 中，无需重建 initramfs 或刷写启动分区。
- 新旧模块 SHA256、vermagic、Build ID 详见 `module-metadata.json`。

## 重启验证

- boot ID 从 `3f698769-586b-4462-83b0-a200f8774528` 变为 `d73833ce-172c-4087-8931-5fc8f4c0e2d4`。
- 已加载模块的 build-note SHA256 为 `95aea53772dcf05c55c49a69450018cb9a32e3886f8a5b09765cc62a1d3f7eb9`，与新模块完全一致。
- 插线时：status=Charging，current_now=+251000 µA，USB online=1，电量 94%。
- UPower 电池设备和 DisplayDevice 均为 State=1（正在充电），IconName=`battery-full-charging-symbolic`。
- UPower 和 SSH 服务正常运行；未发现模块格式、符号版本或驱动崩溃错误。日志另有 unknown notification 0x53 和 UPower 无法识别 Wireless 类型的提示，未纳入本次电流符号修复。
- 实测日志：`install.log`、`post-reboot.log`、`health.log`。重启初期手机时钟尚在同步，启动实例以 boot ID 和单调时间核对。

修复后的拔插验证也已通过（用户完成拔线约 20 秒再插回）：

| 场景 | 电池 status | 电池电流 | USB online | UPower 电池 / DisplayDevice |
| --- | --- | --- | --- | --- |
| 插线 | Charging | +231000 µA | 1 | 1 / 1（正在充电） |
| 拔线 | Discharging | -271000 µA | 0 | 2 / 2（放电） |
| 重新插线 | Charging | +253000 µA | 1 | 1 / 1（正在充电） |

对应图标从 `battery-full-charging-symbolic` 切换为 `battery-full-symbolic`，再恢复充电图标。后续采样持续充电，电量由 94% 增至 95%。这些结论来自内核 sysfs 和桌面使用的 UPower D-Bus 数据，未直接目视确认屏幕图标。

详见 `plug-cycle.log` 和 `final-state.log`。采样 SSH 会话在 210 秒上限结束，保留了 39 个完整样本（包含充电 → 放电 → 充电全过程）及最后一个不完整样本；完整样本足以验证两个方向的切换。

## 回退

在手机 SSH 终端执行以下命令恢复原模块并重启（仅适用于上述内核版本）：

```sh
sudo sh -eu <<'SH'
test "$(uname -r)" = '7.3.0-rc2-sm8450+'
target=/usr/lib/modules/7.3.0-rc2-sm8450+/kernel/drivers/power/supply/qcom_battmgr.ko
backup=/var/backups/negroni-charging-20260915/qcom_battmgr.ko
printf '%s  %s\n' 87ab0c2939924c9152ef9dc5aa35658c74486fdf42092c5be636e4e5e1f23acf "$backup" | sha256sum -c -
install -o root -g root -m 0644 "$backup" "$target.rollback-new"
mv -f "$target.rollback-new" "$target"
depmod -a 7.3.0-rc2-sm8450+
sync
systemctl reboot
SH
```

不要在运行中直接卸载该驱动；原有延迟工作任务缺少完整卸载清理。
