# USB Hub 键鼠失效诊断（2026-09-16）

## 结论

已经确认的直接故障是 **USB2 Hub 及其下游罗技接收器断开，此后没有重新枚举**。USB3 Hub 和网卡曾继续工作。通用 HID 驱动已经成功创建过键盘和鼠标输入设备，因此当前应优先排查 USB2 链路的维持和恢复，而非桌面输入法或通用 HID 驱动缺失。

这与设备 Wiki 所述的 USB2、OTG、充电切换兼容性问题相符，但现有日志没有同步记录人工插拔时刻，尚不能确认具体触发代码，也不能断言所有 xHCI 注销都是驱动自行重启。用户已尝试 Wiki 推荐的接线顺序，仍然无效；该建议并非对所有 Hub 的保证，Wiki 点名测试的是 Ugreen CM888。

本轮只采集诊断信息；未修改内核或手机配置，未重启、重新编译或刷机。

## 已核实的证据

手机：`7.3.0-rc2-sm8450+`，通过 Wi-Fi 地址 `192.168.188.91` 访问。本地源码分支 `7.3-rc2`，提交 `593ad628e5b8`，工作树干净。

完整开机日志来自 `journalctl -k -b`。以下时间约为启动后秒数：

| 时间 | 事件 | 含义 |
| --- | --- | --- |
| 32.43 s | `usb 1-1`：`0bda:5423`，4-Port USB 2.0 Hub | USB2 上行链路成功建立 |
| 32.58 s | `usb 2-1`：`0bda:0423`，4-Port USB 3.0 Hub | 同一个物理 Hub 的 USB3 部分也成功建立 |
| 32.94 s | `usb 1-1.4`：`046d:c52b`，Logitech USB Receiver | 接收器完成 USB 枚举 |
| 33.32–33.38 s | `hid-generic`：Keyboard、Mouse、Consumer Control、System Control | HID 绑定并创建输入设备；尚无实际按键事件验证 |
| 37.16 s | `usb 1-1` 和 `usb 1-1.4`：USB disconnect | 整个 USB2 Hub 分支掉线；这一时刻没有 xHCI 注销记录 |
| 64 s 以后 | USB3 下游 Hub `045e:0955` 和网卡 `045e:0927` 出现 | USB3 通路可工作，不能据此推断 USB2 正常 |

完整日志中，USB2 子设备的成功枚举只有上表的两次，没有发现 USB2 descriptor error 记录。不要将 Wiki 的“descriptor error”直接当成本次实测错误。

USB3 Hub 具有独立的 USB2 和 SuperSpeed 通路。网卡走 SuperSpeed，接收器走 USB2 的 full-speed；USB2 通路丢失时，网卡仍可能可用。

本轮 03:09–03:14 的实时快照：

- USB 设备树中只剩两个根 Hub，外接 Hub、网卡、接收器均不在树中。当前物理接线尚待用户确认，不能将此快照直接等同于最初“网卡仍可识别”的场景。
- USB role 与 DWC3 debugfs 都为 `host`；Type-C 为 `host [device]`、`source [sink]`，方括号表示实际选中的 Device/Sink。
- 系统日志记录，此前约 02:03:09 曾执行 `echo host > /sys/class/usb_role/a600000.usb-role-switch/role`。这可以解释当前控制器与 Type-C 角色不一致，不能单凭不一致认定 UCSI 自行失步。
- `usbhid`、`hid_generic` 已加载；`CONFIG_USB_HID=m`、`CONFIG_HID_GENERIC=m`、`CONFIG_INPUT_EVDEV=y`。
- 未构建 `hid_logitech_dj` 和 `hid_logitech_hidpp`，但接收器曾由通用驱动创建键盘和鼠标设备。这一配置缺项不能解释上游 USB2 Hub 的消失；链路稳定后再验证配对和按键，必要时补齐罗技专用支持。
- USB2 PHY 与 DWC3 runtime 状态为 active；USB2 PHY 的三路 regulator 均启用。根 Hub 的 suspended 本身不是供电故障的证据。
- FSA4480 已绑定，读取 ID 为 `0x09`，switch enable/select 为 `0x98/0x18`，与该驱动的 USB 通路使能设置一致。此次快照不支持“FSA4480 当前把 USB2 通路关掉”的猜测，但不能还原掉线瞬间。
- I2C `990000.i2c` 上有传输错误，该控制器连接触摸屏；FSA4480 位于另一条 `a98000.i2c` 总线。不能把触摸屏 I2C 错误直接归因于 USB 切换芯片。

## 对之前推断的修正

1. **“USB2 从未枚举”不成立。** `dmesg` 环形缓冲区已覆盖早期记录，完整 journal 保留了 USB2 和 HID 成功初始化的证据。
2. **`0x53` 不是未知硬件故障。** OnePlus 原厂 SM8450 源码将其定义为 `BC_CID_DETECT`，处理函数读取 `USB_CID_STATUS` 并更新插接状态。当前移植驱动未识别它，因而打印 unknown notification；这说明移植不完整，但日志邻近不能证明它导致 USB2 断开。
3. **不能直接删除 `USB_OTG_AP_ENABLE`。** 原厂初始化同样会发送该属性以启用 AP 管理 OTG。应调查时序与角色状态，不能把必要的 OTG 初始化本身视为已证实根因。
4. **不能把 Data Host 与 Power Source 等同。** PD Hub 边充边用时，Host + Sink 是合法组合。后续修正充电策略需区分数据角色和供电角色，而非一律禁止 Host 状态下充电。
5. **USB 类型 17 的错误是另一项问题。** 当前驱动直接把厂商 `PD_SDP=17` 返回给通用 power_supply 核心，而核心没有该枚举的显示项。只增加 `.usb_types` 的 `BIT(17)` 仍不足；更合理的是保留内部厂商类型，在对外 `USB_TYPE` 属性中转换成核心支持的标准类型。没有证据证明这项报告错误导致 Hub 断开。

`qcom_battmgr_voocphy_recheck()` 及 gauge 初始化中的 PDO、VoocPHY、充电参数写入仍是排查候选，USB2 PHY 状态恢复、Hub 自身兼容性和 Type-C/PD 切换也是候选。目前没有足够证据选定其中一个并称其为已验证根因。

## 下一步对照实验

先确认实时接线，再进行一次有时间标记的测试：

1. 手机在不接充电器、也不接 Hub 的状态下重新启动，消除之前强制角色和本次启动中遗留的状态；重启需要用户安排。
2. 通过 Wi-Fi SSH 记录日志，只接 Hub 和接收器，始终不接充电器，观察至少 60 秒。以 `usb 1-*`、HID/input 设备和实际按键事件判断在线，接收器指示灯亮不足以证明枚举正常。
3. 若这一阶段稳定，再接入 PD 电源并记录精确时间，检查 USB2、USB3、Type-C 数据/供电角色及充电通知是否变化。
4. 若完全不接充电器也失败，优先用另一只已验证支持的 Hub 或直接 OTG 连接做对照；若只在 PD 接入时掉线，再围绕 PD/VoocPHY 状态切换进行单一变量的内核实验。

要在某个候选函数中改行为前，先增加有时间戳的诊断或设计可回退的单项对照；当前没有验证过的固件补丁。

## 证据文件与来源

- [完整启动 USB 时间线](boot-usb-timeline.txt)
- [实时设备树、驱动配置及 dmesg](state.txt)
- [完整启动 journal、控制器寄存器及电源状态](details.txt)
- [HID 模块检查及早期接收器记录](mux.txt)
- [FSA4480 寄存器读取](mux-registers.txt)
- [设备绑定及此前强制 Host 的系统记录](bindings.txt)
- [Wiki USB OTG 原文摘录](wiki-usb-otg.txt)
- [设备 Wiki](https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_%28oneplus-negroni%29#USB_OTG)
- [原厂 SM8450 通知定义](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/oneplus/sm8450_v_15.0.0_oneplus_10_pro/vendor/oplus/kernel/charger/charger_ic/oplus_battery_sm8450.h#L74)
- [原厂 CID 通知处理](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/oneplus/sm8450_v_15.0.0_oneplus_10_pro/vendor/oplus/kernel/charger/charger_ic/oplus_battery_sm8450.c#L6156)

原厂源码副本已保存在本目录 `oplus_battery_sm8450.c/.h`，仅作为对照资料。认证信息从工作区 `image-credentials.json` 读取，未写入本报告或诊断脚本。
