**结论：有依据实现 80% 充电上限。推荐在当前 `qcom_battmgr` 模块内补齐一加的充电禁止控制及电量阈值状态机，先验证普通 USB PD，再扩展 SuperVOOC。**

后续进展：已按用户要求完成源码实现与固件封装，服务默认关闭，采用 75% 恢复、80% 停止的包含边界。实际接口、使用方法和验证状态见 [实现说明](../../CHARGE-LIMIT.md)。下文保留前期调研时的方案比较，不作为最终接口定义。

调研日期：2026-09-16。对象：OnePlus 10 Pro / negroni，Arch Linux ARM，`7.3.0-rc2-sm8450+`。本次完成源码分析和 SSH 只读检查，未发送停充命令、修改模块、修改休眠策略或安装服务。下文接口和配置均为拟议实现。

**依据和已知边界**

实机记录见 [device-readonly.txt](device-readonly.txt)。当前电池为 100%，USB PD 输入约 9 V；`qcom-battmgr-bat` 没有 `charge_control_start_threshold`、`charge_control_end_threshold`、`charge_behaviour`，USB 的 `input_current_limit` 也是只读。现在安装 TLP、定时写 sysfs 或使用 Android ACC，均不能凭空获得缺失的驱动控制接口。

手机磁盘上的 `qcom_battmgr.ko` 与本机 `linux/output/drivers/power/supply/qcom_battmgr.ko` 的 SHA-256 一致：

```text
88936815a1d73307096e91f1a34157e55570e733bceb2ca7807ca2568301cd84
```

本地内核源码提交为 `593ad628e5b838457d6a316cc08ba5dcc924a01f`，配置是 `CONFIG_BATTERY_QCOM_BATTMGR=m`。这提供了匹配的模块构建基础。注意 `linux/output/out-modules/` 中同名文件的校验值不同，后续应以已核对的实际构建产物为基准，不能混用暂存模块。

这次发现的最直接证据是，一加官方 OnePlus 10 Pro 源码已经包含上限策略：

- `oplus_set_chg_up_limit()` 保存充电上限与恢复阈值。
- `monitor_ui_soc_to_enable_chg_up_limit()` 比较电量，到达上限后停止，低于恢复阈值后重新允许充电，中间区域保持状态。
- `oplus_enforce_chg_up_limit_result()` 调用 `mmi_charging_enable()`。
- `mmi_charging_enable(..., 0)` 关闭快充路径，再停止普通充电；恢复时重新开启相应路径。

见官方 [oplus_charger.c](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/877203a7cf9b79973d44a9db716490f95d820110/vendor/oplus/kernel/charger/oplus_charger.c#L10978)，本地快照 [sources/oplus_charger.c](sources/oplus_charger.c)。原厂使用 Android 的 `ui_soc`，当前 Linux 应使用本驱动成功读取的新鲜 SOC，不能假定两者显示值和滤波行为完全一致。

这证明官方设计采用了主机侧电量监测与充电开关，并不证明手机当前 ADSP 固件已经接受一个可直接写入的“80%”参数，也不等于已验证当前移植驱动的停充效果。

**可选方案比较**

| 方案 | 可行性与代价 | 取舍 |
|---|---|---|
| 直接使用现有 sysfs / TLP / ACC | 实机没有可写控制接口 | 当前不可直接用 |
| 将其他高通机型的原生阈值接口加到属性列表 | Oplus 属性编号的含义不同，未确认固件原生阈值协议 | 不采用直接套用 |
| 驱动提供充电开关，systemd 服务监测 75–80% | 实现较小、便于验证；仍须修改驱动的自动重启充电路径；服务停止、启动时序和休眠需要额外处理 | 适合首轮验证 |
| 驱动内维护 75–80% 阈值状态，systemd 负责配置恢复 | 能统一处理热插拔、快充、ADSP 恢复；需明确休眠行为 | 推荐长期实现 |
| 智能插座按电量切断适配器 | 不需要驱动补丁，但可能连同 Hub 供电一起切断，并让电池周期性供整机用电 | 软件停充无效时的替代方案 |

上游 `qcom_battmgr` 目前给 SM8550 / X1E80100 等分支提供阈值属性，SM8450 在这份移植代码中回退到 SM8350 属性集。读写协议不能只看同一个驱动文件名：

| 属性编号 | 上游通用定义 | 一加 SM8450 定义 |
|---|---|---|
| 24 | `BATT_CHG_CTRL_EN` | `BATT_CHG_EN`，充电开关 |
| 25 | `BATT_CHG_CTRL_START_THR` | `BATT_SET_PDO`，设置 PD 电压 |
| 26 | `BATT_CHG_CTRL_END_THR` | `BATT_SET_QC`，QC 控制 |

因此不能把一加伪装成 SM8550，也不能让阈值读操作沿用通用属性 25/26。上游设置阈值另外使用报文 opcode `0x48`，当前未找到一加这版固件支持该请求布局的证据。一加还有值为 `0x48` 的 VOOC **通知编号**；通知 payload 与请求 opcode 是不同字段，数值相同本身不能证明直接冲突。

**底层控制：停止电池充电，保留 USB 输入**

原厂有两个不同的操作，代码见 [sources/oplus_battery_sm8450.c](sources/oplus_battery_sm8450.c)：

| 目的 | PMIC GLINK 请求 | 代码依据 |
|---|---|---|
| 允许／禁止普通电池充电 | owner `32778`，type `1`，opcode `0x31`，battery `0`，property `24`，value `1` / `0` | `smbchg_charging_enable()` / `smbchg_charging_disable()`，约 8265 / 9116 行 |
| 暂停 USB 输入 | opcode `0x33`，USB property `5` (`USB_INPUT_CURR_LIMIT`)，value `0` | `smbchg_usb_suspend_enable()`，约 9164 行 |
| 允许／禁止 ADSP VoocPHY | opcode `0x33`，USB property `13`，value `1` / `0` | `oplus_adsp_voocphy_enable()`，约 8349 行 |
| 向 ADSP 回答当前是否允许充电 | opcode `0x31`，battery property `37` | `oplus_chg_status_send_adsp_work()`，约 8412 行 |

首轮 PD 验证使用 property `24` 的停充开关，保持 USB 输入电流限制与 PD 协商状态。将输入电流置零属于暂停输入，不能当作“适配器供整机、电池不充电”的等价实现。`charge_control_limit` 也不是百分比，其标准 ABI 单位是微安。

SuperVOOC 不能仅凭普通充电开关成功就宣布支持。原厂停充还调用 `oplus_adsp_voocphy_turn_off()`，最终写 `USB_VOOCPHY_ENABLE=0`；相关函数见 [sources/oplus_voocphy.c](sources/oplus_voocphy.c)，约 8640 行。当前移植驱动注释还指出 VoocPHY 与电量计初始化有关，所以关闭后必须检查 SOC、温度、电流读数是否仍持续更新。采用原厂调用次序和有限重试作为起点，不能直接移植整套 Android 依赖。

原厂的“关充电”和“暂停输入”是分开的，这为保留外接供电提供了依据；**是否在这台手机上形成稳定的电源直供仍须测量，不能由函数名推定。** 即使稳定使用外接电源，也不代表电池已从电气上完全断开。

**推荐的驱动实现约定**

对 `of_machine_is_compatible("oplus,negroni")` 使用独立能力标记和电池属性集，新增标准接口：

```text
/sys/class/power_supply/qcom-battmgr-bat/charge_control_start_threshold
/sys/class/power_supply/qcom-battmgr-bat/charge_control_end_threshold
/sys/class/power_supply/qcom-battmgr-bat/charge_behaviour
```

`charge_behaviour` 首先支持 `auto` 与 `inhibit-charge`；不提供未经验证的 `force-discharge`。试验阶段先实现该开关和恢复路径的约束，完成实测后再加入阈值控制。ABI 参考 [sources/sysfs-class-power.txt](sources/sysfs-class-power.txt)，约 352 / 502 行。

推荐上限 80%，恢复阈值 75%，约每 5 秒评估一次，插拔、SOC 通知、阈值变化、ADSP 恢复及系统唤醒时立即补评估。Linux ABI 将 start 描述为“低于该值恢复”，建议严格使用 `SOC < 75`；整数电量显示通常会在 74% 触发，用户可理解为降到 75% 附近恢复。若产品要求恰在显示 75% 就恢复，应明确调整 start 的取值或边界约定，避免接口说明和行为不一致。

| 条件 | 行为 |
|---|---|
| `inhibit-charge` | 禁止电池充电；保持有线供电路径 |
| `auto` 且 SOC ≥ 80 | 停充，记录“达到上限”状态 |
| `auto` 且 SOC < 75 | 解除上限造成的停充；继续服从 ADSP/温控等其他禁止原因 |
| 75 ≤ SOC < 80 | 保持上次状态，避免反复开关 |
| 重启／首次插入时处在区间内 | 明确定义初始状态；建议保守地等待低于 start 再充 |
| 无外接电源或 OTG 输出 | 保留阈值配置，不按“正在插电充电”处理 |
| 读取失败、固件重连或动作失败 | 标记测量／执行状态未知，记录并有限重试，不声称已经停充 |

阈值读取返回驱动管理的配置，不向一加固件查询通用编号 25/26。用户的期望状态、最近一次成功下发状态和观测电流应分开保存。返回 `inhibit-charge` 代表策略请求，不应被解释为电流计的硬件验证结果。

新状态机应统一经过一个“计算是否允许充电并应用”的入口。修改点均在 [sources/local-qcom_battmgr.c](sources/local-qcom_battmgr.c)：

- 初始化（`qcom_battmgr_oplus_gauge_init()`）：现有代码直接发送 `CHG_EN=1`；先取得有效电量和策略，再决定充电权限。
- 每 5 秒的适配器检查（`qcom_battmgr_voocphy_recheck()`）：热插拔／类型变化时也会直接发送 `CHG_EN=1`，DCP 情况还反复启用 VoocPHY；全部受用户停充与阈值状态约束。
- ADSP 的充电许可询问（`qcom_battmgr_chg_status_reply_work()`）：现有逻辑只看 `adsp_suspended_chg`，应同时考虑用户禁止与上限，避免已停充却回复“可以充电”。原厂还检查 USB suspend 状态，快充支持阶段应同步核对。
- 快充通信错误处理（`qcom_battmgr_voocphy_status_worker()`）：错误恢复会触发重新配置，不应绕过停充策略。
- ADSP 重连（`qcom_battmgr_pdr_notify()`）：现有 DOWN 仅清 `service_up`，未清 `gauge_init_done` 等状态。应使已下发状态和电量缓存失效，在 UP 后重新初始化、测量并应用策略，且不得在回调内做等待 ACK 的同步请求。
- 状态显示（`qcom_battmgr_bat_get_property()`）：现有代码把“有外接电源且固件报 Discharging”强制改成 Charging。已确认执行停充时需要正确显示 `Not charging`；实际电池仍在供电时应结合可靠测量表示放电，不能用充电图标验证结果。

所有会发送相关命令的路径使用同一串行化规则，明确状态锁与请求锁顺序。现有注释指出并发固件请求可能导致无响应；另起一个 GLINK 客户端向同一个 owner 发命令不能解决共享状态和 ACK 归属问题。`pmic_glink.c` 会把同 owner 的响应分发给所有匹配客户端，因此不推荐旁路模块或用户态直接创建 rpmsg endpoint。

写操作只有 `ret == 0` 才视为成功；当前固件可能返回正数错误码，例如 514，需要映射为 Linux 的负错误码并保留原码日志。响应必须检查长度及匹配的 opcode/property；超时后的迟到 ACK 不能被当作下一次动作成功。不要复用现有阈值 setter 中无论请求成功与否都返回 0 的做法。

电量应带有效性和新鲜度：每次判定要求成功读取、范围 0–100，不能因为缓存初值 0 或旧 OEM 数据就允许充电。短暂读取失败保持最近的保守状态并重试；持续失败应明确报错并选择已验证的停充策略，不能宣称仍保证 80%。关机及 ADSP 不受主机控制的阶段不在软件阈值保证范围内。

恢复“正常充满”建议使用 `start=0,end=100` 作为关闭主机阈值限制的配置，加上 `charge_behaviour=auto`。仍由原充电固件负责满充终止与保护，不用主机每次读到 100% 都强行反复开关。阈值应验证 `0 ≤ start < end ≤ 100`。

**休眠和持久化**

实机支持 `freeze mem`，当前列出的睡眠 inhibitors 只有 delay 类型，未发现阻止系统休眠的 block inhibitor。屏幕熄灭不等于系统 suspend；普通 systemd timer 和内核 delayed_work 都不能在 CPU 真正休眠期间持续评估 SOC。把轮询移进内核也不会自动解决这个问题。

针对长期插电的使用方式，第一版可明确采用“限充启用且插电时保持系统运行，允许熄屏”的配置，通过 logind sleep inhibitor 配合驱动电源管理处理。inhibitor 能约束正常桌面休眠，但不能视为对所有强制休眠入口的保证；需要验证 PM 路径。若以后要支持插电深度休眠，应实现并验证以下策略之一：休眠前确认停充、恢复时立即评估；或可唤醒的定时机制。休眠前停充失败时必须显式处理，不能无声进入会继续越过上限的状态。

systemd 在驱动节点出现后应用并回读 75/80 与 `auto`，配置可放 `/etc/negroni-charge-limit.conf`。可用 udev 触发对应 oneshot；仅使用 `ConditionPathExists` 而没有重触发，会在启动时错过尚未出现的设备。驱动内部保存运行期配置并负责 ADSP 重连，systemd 负责重启／重新加载模块后的恢复。

若只靠系统启动后的服务配置，早期启动存在策略尚未应用的窗口。专用于本机的部署可加启动时可设置的默认阈值，使驱动首次允许充电前就知道 75/80；正式补丁的通用默认行为应保持正常充电，避免将个人偏好硬编码给其他设备。关机充电和启动前固件阶段仍需单独验证，不能称为固件永久限充。

**验证与部署顺序**

1. 在本地备份匹配的 `.ko`、源码和构建配置，制作只针对 negroni 的 `charge_behaviour` 验证补丁；包含上述恢复路径约束、错误返回和模块生命周期处理。先验证编译和状态转换，不立即覆盖手机。
2. 在电量明显低于满充终止区间、确实有持续充电电流时，使用现有 PD 适配器验证 `auto → inhibit-charge → auto`。当前手机已 100%，单纯看到小电流不能证明命令生效。若用户目标是降低现有 100%，需要先通过正常使用放电；限制功能不会自动抽走电量。
3. 观察停充阶段至少数分钟，恢复充电也至少数分钟。记录 SOC、温度、电池 `current_now`、USB online/电压/电流、动作 ACK 和内核日志。电池正电流应停止持续流入；恢复后应重新出现明确正充电电流。验收依据是电流与 SOC 的联合趋势，而非读回一个配置字符串。
4. 在空闲和有负载两种情况下验证外接供电：停充后 USB 持续供电，电池电流在零附近，负载增加主要表现为 USB 输入增加。若电池持续负电流，只能得出“停充但仍由电池补电／供电”，不能称为稳定直供。必要时用外接 USB 功率计交叉验证；设备的部分电量/功率属性存在移植与单位问题，不能只看一个 `power_now` 数字。
5. 加入 75/80 阈值控制和启动配置。先用当前电量附近的临时阈值缩短边界测试，再恢复 75/80；验证一次实际达到 80% 的停止，以及下降后的恢复。预期为报告电量约 80% 停止，存在 SOC 取整与轮询延迟，不承诺化学容量精确等于 80.00%。
6. 验证拔插、PD 类型变化、重启、熄屏与受控休眠／唤醒、错误返回及恢复正常充电。ADSP 重启的故障注入安排在有恢复条件的测试阶段，避免把普通读状态当成已测试固件崩溃恢复。
7. 单独验证 SuperVOOC 的退快充、停充、恢复和电量计更新。原厂快充测试通过前，交付范围明确为已测试的有线 PD；无线充电和 OTG 同样单列，不从 PD 结果推定支持。

模块方式意味着通常无需重刷整套 rootfs 或改 bootloader。首轮建议“备份并替换磁盘上的匹配模块，再受控重启”，而非立即在线 `rmmod`。当前代码仅为 `enable_work` 注册了自动取消，其余 delayed work / work 缺少完整清理，需要先补齐；否则在线卸载可能留下访问已释放状态的任务。

本地 `logs/7.3-rc2/initramfs-contents.txt` 未列出 `qcom_battmgr.ko`，这支持模块从真实根文件系统加载的判断，但正式部署还要检查手机实际启动镜像是否包含另一份模块。如果包含，需同时更新真正被 bootloader 使用的镜像；仅改 `/boot/initramfs-linux.img` 文件不等于已更新 Android boot 分区。部署和回退步骤据这项检查决定。

**尚待实验回答的问题**

| 问题 | 当前证据 | 如何关闭这个不确定性 |
|---|---|---|
| 当前 ADSP 是否接受 `BATT_CHG_EN=0` 并实际停止 PD 充电 | 原厂使用该命令；当前移植驱动已经用 `=1` 开启充电 | 低于满电时完成停充／恢复 A/B 实测 |
| 停充后能否由适配器持续供整机 | 原厂明确区分充电开关和输入暂停 | 测量空闲和负载下 USB 与电池电流 |
| SuperVOOC 关闭后电量计是否继续正常更新 | 原厂有 VoocPHY 关闭路径；移植驱动存在相关初始化耦合 | 独立测试快充退出与 SOC 新鲜度 |
| 上限是否跨休眠有效 | 主机轮询在 suspend 期间不可运行 | 实现明确 PM 策略并受控测试 |
| 是否适用于无线／关机充电 | 本次没有相关验证 | 作为后续独立范围 |

达到第一阶段成功的条件是：**在正在 PD 充电时可以可靠停止并恢复，停充不会被现有后台 worker 撤销，且供电行为已测明。** 达到这些条件后，75/80 阈值和自动恢复配置才有可靠基础。

**资料**

- [postmarketOS 设备 Wiki](https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_(oneplus-negroni)#Battery)：确认 ADSP、电量显示、PD 与 SuperVOOC 的设备背景；页面没有给出 80% 限制方法。
- [一加官方充电策略源码](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/877203a7cf9b79973d44a9db716490f95d820110/vendor/oplus/kernel/charger/oplus_charger.c#L11046)。
- [一加官方 SM8450 命令与开关](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/877203a7cf9b79973d44a9db716490f95d820110/vendor/oplus/kernel/charger/charger_ic/oplus_battery_sm8450.c#L9116)。
- [一加官方 VoocPHY 关闭流程](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/877203a7cf9b79973d44a9db716490f95d820110/vendor/oplus/kernel/charger/voocphy/oplus_voocphy.c#L8640)。
- [上游 qcom_battmgr](https://github.com/torvalds/linux/blob/37689bcbc41b3850781b6c84f90cb4bd632bfbf0/drivers/power/supply/qcom_battmgr.c)：调研时该文件最新提交 `37689bcbc41b3850781b6c84f90cb4bd632bfbf0`，仍区分不同平台能力。
- [sources.json](sources.json)：本地源码快照来源和 SHA-256；[sources/](sources/) 保存本次分析依据，便于后续实现时对照。
