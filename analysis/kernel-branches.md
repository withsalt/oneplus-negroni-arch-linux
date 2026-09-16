# OnePlus 10 Pro（NE2210）：7.2、7.3-rc2、master 内核分支比较

分析日期：2026-09-15（Asia/Shanghai）。仓库：[Xlie-Electronic-Customs/linux](https://github.com/Xlie-Electronic-Customs/linux)。设备：OnePlus 10 Pro / `oneplus-negroni` / Snapdragon 8 Gen 1（SM8450）、Adreno 730。用户确认为 NE2210、ColorOS 14、Bootloader 已解锁、未升级至 ColorOS/OxygenOS 15。

**结论：7.3-rc2 和 master 相对 7.2 都有值得测试的改进，主要是 A730 GPU 电源管理、触摸屏 DMA 配置和公共驱动修复。但不能据此宣布声音、摄像头、GPS 已可用，也不能保证续航或帧率提升。两个新分支的本机设备树、配置片段和主要 Qualcomm 驱动相同；master 另外包含一批通用修复，没有发现额外的 negroni 专属功能。**

对于当前尚未真机验证的构建，建议先用已经完成构建和离线校验的 7.2 建立启动基线；需要验证新修复时，再单独构建固定提交的 7.3-rc2 或 master。这个顺序依据现有产物和 Wiki 的构建基线，并非证明 7.2 的运行稳定性一定更高。

## 1. 比较对象与方法

原请求中的 `7.3-rc1` 在检查时已无对应远端分支；按用户确认改为分析 `7.3-rc2`。

| 分支 | 本次固定提交 | Makefile 中的版本 | HEAD 提交时间（北京时间） |
| --- | --- | --- | --- |
| 7.2 | `99e0c83637f903af7f77cffb8b2f0db7abef18dd` | 7.2.0 | 2026-08-17 07:38:02 |
| 7.3-rc2 | `e8291a2765b80f94c047d28ca514465c7716c374` | 7.3.0-rc2 | 2026-09-11 04:12:38 |
| master | `94ab11292128c98bf80d6f3a0b22e4045e0f28aa` | 7.3.0-rc2 | 2026-09-14 04:06:29 |

`master` 的版本号仍是 7.3-rc2；名称不代表稳定版，也不能据此将它理解成 7.3 正式版。其 HEAD 补丁的作者时间早于提交时间，是判断补丁重放时需要注意的区别。

本次读取了远端提交快照、GitHub 提交/API 元数据、实际源码和 [postmarketOS Wiki](https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_(oneplus-negroni))。Wiki 当前构建命令仍指定 `--branch 7.2`；本地文本快照见 [branch-review-wiki.txt](../logs/branch-review-wiki.txt)。

比较以 **两个端点的完整文件树 `git diff A B`** 为准。仓库有合并与补丁重放，GitHub 将三组比较均判为 `diverged`；不能把 ahead 提交数解释为新增手机功能，也不能把重放后的旧补丁再次计作相对 7.2 的改进。本地是浅克隆，GitHub compare 的文件清单还有数量上限，因此下列统计来自本地完整文件树，不来自 API 的截断文件列表。

| 文件树比较 | 改动文件数 | 增加行数 | 删除行数 |
| --- | ---: | ---: | ---: |
| 7.2 → 7.3-rc2 | 13,692 | 854,153 | 230,479 |
| 7.2 → master | 13,946 | 860,772 | 233,887 |
| 7.3-rc2 → master | 492 | 6,846 | 3,635 |

这些是整个 Linux 内核的规模，包含其他架构、设备、文档和测试，不是 OnePlus 10 Pro 的专属改动量。

## 2. 对 NE2210 的实际意义

下表中“新分支”同时指本次固定的 7.3-rc2 和 master；表内共同改动的代码已核对一致。

| 项目 | 7.2 基线 | 两个新分支的变化 | 对这台手机的判断 |
| --- | --- | --- | --- |
| 触摸屏 | 已有 Synaptics S3908 支持；设备树删除 I²C4 的 DMA 属性 | 恢复继承 DMA 属性，调整 GPI 配置，更新 GENI I²C 驱动 | 有明确面向 negroni 的修复；触摸响应和休眠唤醒仍需实测 |
| A730 GPU | 已有 3D 加速，但 A730 条目未启用 IFPC | 启用 IFPC，修正恢复寄存器及 GBIF 配置，撤回 2 µs 轮询特例 | 最明确的本机改进候选；预期影响空闲功耗和 GPU 唤醒，不能量化续航或 FPS |
| 屏幕 | 已有亮度、HBM、60/90/120 Hz；面板默认 FHD | 面板文件未变，但共用 DSC 压缩寄存器配置改变 | 有实际显示路径变化，需检查颜色、亮屏及休眠恢复；未增加动态分辨率切换 |
| 电池与充电 | Wiki 已记载电量、PD 30 W、SuperVOOC；充电图标有问题 | 调整 qcom_battmgr 的状态通知及 OTG 通知 | 有行为修正，不能认定充电图标已修复或充电功率提高 |
| USB / 扩展坞 / DP | 已支持，Wiki 记录 USB 2.0 插拔及连接顺序限制 | USB combo PHY 增加 mode-switch，配置撤销若干禁用项 | 有兼容性修复候选，不能承诺所有 Hub 或连接顺序都正常 |
| Wi-Fi | WCN6855 + ath11k，依赖正确的设备固件 | ath11k 扫描参数复制、WMI 长度/索引校验及资源释放修复 | 可获得公共驱动健壮性修复；无本机信号、吞吐提升证据 |
| 音频 | Wiki 为 Broken，通话没有音频 | SoundWire 参数、AudioReach 通道映射、TDM 和时钟支持增强 | 基础代码确有进展；没有证据表明本机扬声器、麦克风或通话音频已可用 |
| 蜂窝网络 | 已有 IPA 5.1；Wiki 记载 SIM1 数据、短信和无音频通话 | 有 IPA/remoteproc 公共更新；新增 IPA 6.0 面向其他硬件 | IPA 6.0 不是本机新功能；没有已验证的 SIM2 或通话音频改进 |
| 摄像头、GPS、振动 | Wiki 为 Broken | 未发现完成本机适配的证据 | 不能因为内核版本升级而认为这些功能已恢复 |
| NFC、三段式开关 | NFC 未测试；开关部分可用 | negroni 对应设备树段落未变 | 不能算新分支新增可用功能 |

### 2.1 触摸屏：有针对性修复，也有需要保留的判断边界

关键补丁：[6397b9693a8a — solve issue with touchscreen](https://github.com/Xlie-Electronic-Customs/linux/commit/6397b9693a8ab3a423023035a5f4d0e1244860c5)。

`sm8450-oneplus-negroni.dts` 的 `&i2c4` 删除了以下两条覆盖，从而重新继承 SoC 中的 DMA 描述：

```dts
/delete-property/ dmas;
/delete-property/ dma-names;
```

同时，`sm8450.dtsi` 的 GPI DMA0 添加 `dma-coherent`，`#dma-cells` 从 3 变成 5，I²C/SPI DMA 参数增加两个 cell；I²C4 的尾部值为 `64 2`。公共 GENI I²C 驱动也有较大更新，包括取消/中止/复位的独立 completion、错误处理与资源管理重构；7.2 中 GPI 设置失败后尝试回退 SE-DMA 的定制代码不再保留。因此这是一组 DMA 路径调整，不宜将两行 DTS 单独摘出就视为完整修复。

**源码检查发现：`drivers/dma/qcom/gpi.c` 相对 7.2 未改，`gpi_of_dma_xlate()` 只读取前三个参数，忽略新增的第四、第五个参数；对应 YAML binding 仍规定 `#dma-cells = 3`。** 所以不能声称 `64 2` 已在这个主线驱动中实现特定缓冲区或通道优化。当前 DTB 能通过普通编译，但不等于符合完整 DT schema，也不证明触摸硬件测试通过。

来源：[设备树](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dts)、[GPI 驱动](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/drivers/dma/qcom/gpi.c)、[GPI binding](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/Documentation/devicetree/bindings/dma/qcom,gpi.yaml)。

### 2.2 A730：省电和电源状态恢复是最值得关注的变化

相关补丁：

- [76f6729b499e — solve issue with SET_OOB on a730 and ifpc](https://github.com/Xlie-Electronic-Customs/linux/commit/76f6729b499e3e6195c923323673901df543db42)
- [a4c453638873 — set correct pwrup_reglist](https://github.com/Xlie-Electronic-Customs/linux/commit/a4c45363887318227934c9108c32ebb44d56d276)
- [7ec624494d3f — add a740 ifpc and some changes](https://github.com/Xlie-Electronic-Customs/linux/commit/7ec624494d3f4aaf73507cccbe66a2dec8fbae3e)
- [733223f36fa2 — 撤回 A730/A740 的 2 µs 轮询特例](https://github.com/Xlie-Electronic-Customs/linux/commit/733223f36fa2)

比较最终代码，A730 条目新增 `ADRENO_QUIRK_IFPC` 和 `a730_ifpc_reglist`，使用独立的 `a730_gbif`，并调整时钟门控与电源恢复寄存器表。IFPC 允许 GPU 在渲染间隙进入更低功耗状态，恢复寄存器配置则与退出该状态后的正确运行有关。

GMU OOB 与 HFI 响应轮询从 A730/A740 的 2 µs 特例回到 100 µs，超时上限没有随之变长。不能把这个变化解释为 GPU 频率提高或帧率提升。

“add a740 ifpc” 补丁还调整共享数据，但 A740 本身属于其他硬件；本机收益必须看最终的 A730 条目。7.2 已有 3D 加速、抢占等代码，这些不是本次新增。适合验证的场景是 Plasma 动画、静止画面功耗、长时间空闲后的首次绘制，以及反复灭屏/亮屏。

来源：[A730 配置](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/drivers/gpu/drm/msm/adreno/a6xx_catalog.c)，本地差异见 [adreno.patch](branches/7.2-to-7.3-rc2.adreno.patch)。

### 2.3 显示：面板相同，公共压缩代码并不相同

`drivers/gpu/drm/panel/panel-samsung-s6e3hc3.c` 在三个分支完全一致。亮度、HBM、刷新率及 FHD 默认配置不能重复算作新功能；新分支也没有在该文件中实现运行时 FHD/WQHD 切换。

但 [DSC 补丁 2913b2abdbbf](https://github.com/Xlie-Electronic-Customs/linux/commit/2913b2abdbbf) 与 [a8f1b6d26e97](https://github.com/Xlie-Electronic-Customs/linux/commit/a8f1b6d26e97) 修改了 `dpu_hw_dsc_1_2.c`。**SM8450 的 DPU 为 8.1，资源管理器对 DPU 主版本 ≥7 使用此实现，所以这些改动会涉及本机。** 不能把它们全部归为 OnePlus 15 专属修改。

本机面板使用 DSC 1.2、10 bits/component：新代码将 `ENC_DF_CTRL` bit 12 的设置条件限制为 DPU 主版本 ≥10，因此 SM8450 的这个寄存器设置会变；命令模式的 `DSC_CFG` bit 17 写入也被移除，并增加写内存屏障。另一处 native 4:2:0/4:2:2 的 bpp 翻倍逻辑不适用于当前 negroni 面板的默认参数。

这些是实际显示行为变化，但源码和提交标题不足以证明本机画质改善或避免黑屏。建议将亮屏、颜色、60/90/120 Hz、灭屏恢复作为新分支的优先验证项。

来源：[DPU 分派](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/drivers/gpu/drm/msm/disp/dpu1/dpu_rm.c)、[SM8450 DPU 数据](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/drivers/gpu/drm/msm/disp/dpu1/catalog/dpu_8_1_sm8450.h)、[DSC 实现](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/drivers/gpu/drm/msm/disp/dpu1/dpu_hw_dsc_1_2.c)。

### 2.4 电池：当前实际使用的驱动仍是 qcom_battmgr

[状态通知补丁 3ed8ac2bc515](https://github.com/Xlie-Electronic-Customs/linux/commit/3ed8ac2bc515) 删除/注释了多处 `power_supply_changed()`，包括插拔、Type-C、部分 VOOC 事件及重新检查时的通知，同时注释了插拔后 500 ms 复查的安排；在 OTG enable/disable 中增加 USB 电源状态通知。还有公共的工作项自动取消和字符串处理修复。

作者目标是减少不正确的供电状态变动。这可能减少错误通知，也改变了桌面收到更新的时机。因此不能直接结论为“充电图标已修复”，应对照实际电压、电流、电量和插拔状态测试。

[e5878d0bd011](https://github.com/Xlie-Electronic-Customs/linux/commit/e5878d0bd011d80de7a69a9a9e53991271c2c714) 新增了独立 `oplus_battmgr` 驱动及 PMIC GLINK 匹配分支，但按 Wiki 的 `defconfig + sm8450.config` 实际生成：

```text
CONFIG_BATTERY_QCOM_BATTMGR=m
# CONFIG_BATTERY_OPLUS_BATTMGR is not set
```

negroni 的设备树仍用 `qcom,sm8450-pmic-glink`, `qcom,pmic-glink`，并未切换到新的 `oplus,pmic-glink` 匹配路径。**新文件存在不等于本机已使用新充电驱动。** 本次没有启用或验证这个独立驱动。

来源：[电池补丁](branches/7.2-to-7.3-rc2.battery.patch)、[实际配置](branches/rc2.config)、[PMIC GLINK](https://github.com/Xlie-Electronic-Customs/linux/blob/e8291a2765b80f94c047d28ca514465c7716c374/drivers/soc/qcom/pmic_glink.c)。

### 2.5 USB 和内核配置：有调整，需以生成结果判断

[475e3ec5a9d1 — solve issue with usb hub](https://github.com/Xlie-Electronic-Customs/linux/commit/475e3ec5a9d100da7a8790f59396fee815e12508) 撤销了 SM8450 配置中的部分禁用项。另一配置补丁 [3a6d7f8fe496](https://github.com/Xlie-Electronic-Customs/linux/commit/3a6d7f8fe496) 移除了 `CONFIG_NR_CPUS=8`。按教程重新生成配置后的结果如下：

| 配置 | 原 7.2 构建 | 新分支实际生成结果 |
| --- | --- | --- |
| `CONFIG_NR_CPUS` | 8 | 512 |
| `CONFIG_MDIO_BITBANG` | 未启用 | 模块 |
| `CONFIG_SOC_TI` | 未启用 | 内建 |
| `CONFIG_RESET_TI_SYSCON` | 未启用 | 内建 |
| `CONFIG_PHY_NXP_PTN3222` | 未启用 | 模块 |

NR_CPUS 是编译时支持的 CPU 数量上限，手机仍是 8 核；这个变化不是性能提升，并且实际配置新增了 `CONFIG_CPUMASK_OFFSTACK=y`。上游 defconfig 和 Kconfig 还使一些其他 SoC 的时钟/引脚等驱动重新进入配置，不能只看 `sm8450.config` 删除了五行就认定产物仅有五项变化。完整有效配置差异见 [config-diff.txt](branches/7.2-to-7.3-rc2.config-diff.txt)。

SM8450 USB combo PHY 新增 `mode-switch`，与 USB/DP 模式切换有关。对于 Hub 问题，可以确认作者提交了相关配置修正；但启用一组公共驱动不意味着 NE2210 一定使用其中每个驱动，也不能替代具体扩展坞测试。

### 2.6 音频、无线和调制解调器

音频不是“完全没有变化”：`sound/soc/qcom/common.c` 新增 TDM slot 解析/应用，`q6apm-lpass-dais.c` 新增 TDM 操作和时钟支持，`sc8280xp.c` 增加 SM8450 专用参数；AudioReach 修正物理/逻辑通道映射，并扩充音频接口配置。SM8450 SoundWire RX 从 5 个输出端口变成 6 个，修改采样间隔和端口参数。

但 negroni 的声卡连线段落相对 7.2 未变，未补充上述通用 TDM helper 可读取的 `dai-tdm-slot-*` 属性；该 helper 在没有 slot 配置时直接返回。端到端音频还依赖 DSP 固件、拓扑和用户空间设置。当前 Wiki 仍写 Audio Broken、通话没有声音。合理结论是**音频实现基础增强，尚不能确认手机声音恢复**。本地证据见 [audio.patch](branches/7.2-to-7.3-rc2.audio.patch)。

Wi-Fi 的 ath11k 变化包含扫描 hint BSSID 复制方向修正、WMI 消息长度和 PHY 索引检查、错误路径释放内存等；这些公共修复能影响 WCN6855 所用代码。但它们没有解决设备专用 `board-2.bin`/`board.bin` 固件的准备问题，也没有证明 5 GHz 信号或测速提高。见 [ath11k.patch](branches/7.2-to-7.3-rc2.ath11k.patch)。

新增 IPA 6.0 是其他平台支持，SM8450 使用的 `ipa_data-v5.1.c` 相对 7.2 没有变化。remoteproc 的公共加载、资源管理和 minidump 代码有更新，另有 SM8750/kaanapali ELF64/SOCCP 适配；不能据此推导 NE2210 的 GPS、第二张 SIM 或通话音频已经工作。

### 2.7 内存预留与其他设备新增支持

新分支从 `sm8450.dtsi` 移除了 MTE 和两块 trusted-apps 预留内存描述；相应删除 negroni 里已经无目标的 `/delete-node/ &mte_mem`，避免引用不存在的节点。**7.2 的 negroni 本来就删除了 MTE 节点，不能把 SoC 文件中的 512 MiB 删除再次算作手机新增可用内存。** 其他预留区变化也不等于已经测得的 `MemAvailable` 增长。

此外有 SCM minidump SRAM 引用及 PDC 寄存器范围修正，属于平台描述和诊断支持更新。

仓库新增/推进的其他硬件包括 Xiaomi Vermeer/SM8550 的设备树、面板和 UFS，OnePlus 15 面板，SM8750/kaanapali 远端处理器，A740 IFPC、A830/A722 等 GPU 支持，WCN7860 ath12k 支持和 SDX75 配置。它们解释了一部分新增代码，但不能当作 OnePlus 10 Pro 的硬件功能。7.2 已有的 negroni NFC、屏幕 reset/HBM、OTG、PD 扩展坞及 IPA 5.1 代码，也不能因为在 master 历史中换了提交 SHA 就再次计算。

## 3. master 相对 7.3-rc2 到底多了什么

本次验证下列范围没有任何文件差异：

```text
arch/arm64/configs
arch/arm64/boot/dts/qcom
drivers/gpu/drm/msm
drivers/gpu/drm/panel/panel-samsung-s6e3hc3.c
drivers/power/supply
drivers/input
drivers/net/wireless/ath
drivers/net/ipa
drivers/remoteproc
drivers/soc/qcom
drivers/dma/qcom
drivers/i2c/busses
drivers/phy/qualcomm
drivers/usb
drivers/media/platform/qcom
sound/soc/qcom
```

因此，上文触摸、A730、DSC、充电、USB、Qualcomm 音频等共同改动，master 与 rc2 都有。

master 另外改动 492 个文件，包括通用网络栈、文件系统、调频/OPP、部分 DRM 核心、终端及其他硬件驱动。例如：

| 通用改动 | 代码层面的修正 | 对本机的意义 |
| --- | --- | --- |
| `drivers/cpufreq/cpufreq.c` | CPU mask 改为清零分配；在对外注册 policy 对象前初始化 rwsem | 本机使用的调频基础设施修复；尤其不能把新配置下的路径当作与 7.2 完全相同 |
| `drivers/opp/core.c` | 获取 clock 失败时，在释放 OPP table 前保存错误，避免释放后再次读取对象 | OPP 错误处理更完整；实际启动是否经过该错误路径需看设备状态 |
| `drivers/gpu/drm/drm_exec.c` | 空对象数组也处理争用锁，避免特定重试模式无限循环 | DRM 公共修复；没有证据表明 NE2210 正在触发此路径 |
| `drivers/video/fbdev/core/fbcon.c` | 限制 logo 占用行数并处理无可用行的情况 | 控制台边界处理修复，不是桌面显示功能升级 |

还有 OPP 数值/日志处理、thermal sysfs 格式化以及大量其他平台修复。完整清单见 [master 相对 rc2 的文件统计](branches/7.3-rc2-to-master.files.tsv)；上述重点差异见 [generic-fixes.patch](branches/7.3-rc2-to-master.generic-fixes.patch)。

**master 有额外的公共修复价值，但当前没有额外的 negroni 功能优势。** 如果准备长期调试主线和追踪公共修复，固定本次 master 提交有理由；如果只想试触摸和 A730 补丁，rc2 已包含同样的本机代码。两个分支都需要完成完整内核/模块构建和真机验证，不能从名字或文件数量直接排序运行稳定性。

## 4. 本次实际验证与产物边界

在独立目录从两个新分支的固定提交导出源码，分别执行：

```sh
# 从项目根目录执行，使用本次已导出的两份源码及各自的输出目录。
for variant in rc2 master; do
  make -C "analysis/validation/${variant}-source" \
    O="../${variant}-output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
  make -C "analysis/validation/${variant}-source" \
    O="../${variant}-output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- sm8450.config
  make -C "analysis/validation/${variant}-source" \
    O="../${variant}-output" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 \
    qcom/sm8450-oneplus-negroni.dtb
done
```

实际目录为 `analysis/validation/rc2-source`、`rc2-output`、`master-source`、`master-output`。结果：

- 两者配置生成成功；生成的 `.config` 完全一致。
- 两者目标 DTB 编译成功，DTB 编译日志未发现 warning/error。
- DTB 均为 173,881 字节，SHA-256 均为 `4797091fe0d7d6719b5c85f69ae1aa95a62a6767eb46a8efff82333057e03ad3`。
- DTB 的 model 为 `OnePlus 10 Pro 5G`，compatible 为 `oplus,negroni qcom,sm8450`；I²C4 的 DMA 属性已存在。
- **只验证了配置和目标 DTB，没有执行新分支完整内核、模块编译、DT schema 校验或真机启动。** 前文列出的 DMA binding 不一致仍然存在。

机器可读记录：[review-manifest.json](branches/review-manifest.json)。日志：[rc2 配置](branches/rc2-config-build.log)、[rc2 DTB](branches/rc2-dtb-build.log)、[master 配置](branches/master-config-build.log)、[master DTB](branches/master-dtb-build.log)。

现有 `linux/` 工作树仍在原 7.2 提交，本次没有修改其源码；`images/` 中完整镜像仍对应已构建的 `7.2.0-sm8450+`。`analysis/validation/` 的 DTB 是本次离线检查产物，**不构成一套可以直接替换原镜像的新分支刷机包**。

如果后续决定升级，须让新内核、DTB、模块目录、initramfs 和使用它们的 boot/recovery/vendor_boot 镜像保持一致。不要只替换一个 boot 文件后继续使用 7.2 的模块，也不应将“更换内核”自动等同于需要重新擦除 userdata。ColorOS/ABL/槽位/根分区条件仍按 [README](../README.md) 核对，分支更新本身没有绕过这些启动条件。

## 5. 后续分支选择与验证重点

| 目的 | 建议 |
| --- | --- |
| 首次将当前成果启动到这台 NE2210 | 使用现有 7.2 产物建立基线，先确认启动、固件加载、屏幕和输入 |
| 检查触摸或 GPU 空闲/唤醒问题 | 单独构建固定的 7.3-rc2；它包含已识别的本机新补丁 |
| 同时需要较新的公共修复并准备持续调试 | 单独构建固定的 master；其本机驱动与 rc2 一致，额外收益在公共代码 |
| 期待升级即恢复声音、摄像头、GPS 或振动 | 目前证据不足，不能将这作为升级后必然结果 |

比较实测应使用相同 rootfs、Mesa/Plasma 版本和固件，保持刷新率、亮度及负载一致；记录触摸与灭屏恢复、GPU 报错/空闲功耗、屏幕颜色、充电状态、USB Hub 插拔，以及音频设备枚举。这样才能区分内核分支影响与滚动发行版软件升级的影响。

## 6. 完整差异索引

| 比较 | 所有变化文件和增删行数 | 手机核心源码差异 |
| --- | --- | --- |
| 7.2 → 7.3-rc2 | [files.tsv](branches/7.2-to-7.3-rc2.files.tsv) | [phone-core.patch](branches/7.2-to-7.3-rc2.phone-core.patch) |
| 7.2 → master | [files.tsv](branches/7.2-to-master.files.tsv) | [phone-core.patch](branches/7.2-to-master.phone-core.patch) |
| 7.3-rc2 → master | [files.tsv](branches/7.3-rc2-to-master.files.tsv) | [phone-core.patch（空，所选范围无差异）](branches/7.3-rc2-to-master.phone-core.patch) |

本地 `branches/` 同时保留 GitHub compare/recent-commit JSON、关键提交详情、统计和补丁。文中提交链接有些使用 master 重放后的 SHA；是否在 rc2 中具有同样效果，以已经核对的文件树内容为依据。所有结论只适用于开头固定的三个提交，之后分支移动需要重新比较。
