# 设备状态与使用说明

源码与实机更新日期：2026-11-04；完整 rootfs 构建日期为 2026-11-01，其他历史实机记录日期为 2026-09-16。已记录设备为 OnePlus 10 Pro NE2210，SoC 为 SM8450，GPU 为 Adreno 730。其他 NE221x 变体不视为已验证。

## 源码、镜像和实机

当前内核为 `7.3`，固定到 `23a6b6d834e93dd54d9ec6f188761a208e1e1ed3`，包含电池、CPU、触觉驱动、USB/PD 和角色切换修复，历史未合并 `7.3-rc2`。ARM64 内核/模块/DTB 与主机回归通过。手机已通过 Wi-Fi SSH 更新启动内核、设备树、电池模块和 USB 网卡固件并重启验证，根文件系统沿用既有安装；这不代表 2026-11-01 的完整 rootfs 重新刷入。

带 PD 的 Hub 已验证 USB2 接收器、RTL8153 网卡和充电同时工作，重启后可自动枚举。物理热插拔仍出现过 SMMU `IOVA=0x0c` 与 xHCI 挂死，后续成功拔插不能证明故障消除。电脑端 gadget、无外接供电 OTG、其他 Hub、DP 显示与完整系统深度睡眠仍未覆盖。

2026-11-04 追加验证：已复现电源域关闭后恢复时的非法 DMA 和 USB 全部丢失，并在驱动中补齐 DWC3 到 xHCI 的状态丢失通知。相同诊断配置下，关闭通知的对比内核失败，启用通知后连续 3 次 `pm_test=platform` 均恢复接收器与网卡。这是设备/电源域挂起测试，不能代替真正的深度睡眠验证，也不能等同此前 `IOVA=0xc` 热插拔问题已解决。

手机现已切回包含修复的普通版内核 `7.3.0-sm8450+`，重启后接收器与网卡自动枚举，有线通信和启动成功标记通过；自动故障抓取及本次诊断组件已移除。

A 槽曾因缺少成功标记而耗尽重试次数、进入 recovery，现已恢复并验证重启。[启动成功服务](boot-success.md)已接入构建，需为每台设备确认固件历史和 Linux 槽位后配置。

- **源码与本地镜像**：充电限制、常用软件、GPU runtime PM 配置均已集成，完整构建与离线检查通过。
- **最近完整刷入的系统**：包含键盘修复和默认 SSH 的早期 7.3-rc2 镜像；SSH 已由用户确认默认生效。
- **随后单独部署的变更**：电池电流符号修复已通过充放电及拔插验证，GPU runtime PM 规避服务已部署并记录验证。
- **CPU 性能与调度**：2026-09-16 已在手机启用 Boost / performance，并更新 `vendor_boot_a` 的大小核能力及功耗模型。重启后 capacity 为 276 / 849 / 1024，性能服务自启动和负载迁移验证通过；硬件频率限制仍存在。两项现已集成到完整固件默认构建并默认启用，见 [复现指南](cpu-performance.md)和 [CPU 实测记录](../artifacts/cpu-performance-20260916/README.md)。
- **Docker 内核支持**：已补齐 nftables 兼容、NAT 和 IPv4/IPv6 FIB 配置，完成内核及固件重建、模块一致性与自动加载别名检查。手机此前的 IPv4 模块补装已验证 Docker 正常运行；本次新固件尚未刷入，冷启动和 IPv6 容器网络待验证，见 [Docker 说明](docker.md)。
- **尚未完成**：全新完整 rootfs 的安装验证、最新常用软件逐项验证、USB 偶发热插拔故障的根因定位及上述未覆盖的连接场景。

## SSH

镜像默认启用 OpenSSH，TCP 端口为 22。手机连接 Wi-Fi 后，在同一局域网电脑执行：

```sh
ssh oneplus@手机IP
```

密码为构建时本地 `image-credentials.json` 中的 `password`；也支持为 `oneplus` 配置 SSH 公钥。root 远程登录和空密码登录被禁用，主机密钥在首次启动时生成。镜像未预置 Wi-Fi 凭据，USB 数据线也不会自动提供 SSH 网络。

## 已知问题与配置取舍

| 项目 | 状态与依据 |
| --- | --- |
| 登录虚拟键盘 | 使用 KWin 启动 Plasma Keyboard；7.2 已确认可输入，7.3-rc2 登录操作尚待单独记录，见[修复记录](archive/keyboard-fix-2026-09-15.md) |
| GPU 慢亮屏 | 默认启用 `gpu-runtime-active.service`，将 GPU 的 `power/control` 设为 `on`；这是阻止 GPU runtime suspend 的规避方式，可能增加待机耗电，见[诊断资料](../artifacts/wake-latency-20260916/) |
| 电池状态图标 | 已修正电流符号及 Oplus 充电状态计算；最终模块已实测 5 V Hub 下充电、限充停止与解除恢复，内核与 UPower 状态正确、外设在线，见[充电说明](charge-limit.md) |
| 充电限制 | 镜像默认关闭；本机已启用 75/80%，PD Hub 下 holding/解除与保留外设已验证，启用后阻止挂起/休眠，见[使用说明](charge-limit.md) |
| USB2 / OTG | 已补齐 PHY/时钟、PD、角色切换及失电后的 xHCI 重建；带 PD 的 Hub 可用，偶发热插拔挂死尚未确认解决 |
| A/B 槽启动 | 已修复本机成功位并验证重启；自动标记需设备本地确认固件历史及 Linux 槽位，见[启动成功服务](boot-success.md) |
| 根分区定位 | 改用文件系统 UUID，实机连续 3 次重启通过；原失败启动的早期日志缺失，UFS 偶发初始化问题仍未排除。单独更新 boot 须使用手机当前根 UUID，见[刷机说明](flashing.md) |
| 音频、相机、GPS | 尚无本机恢复可用的证据；不能由公共驱动更新推断功能已恢复 |
| 显示、触摸、Wi-Fi、蓝牙、基带 | 仍需对当前完整镜像逐项记录，已有桌面或 SSH 使用记录不能替代完整兼容性测试 |

SDDM 默认使用 Plasma Mobile、Wayland 与 Breeze；镜像语言为 `en_US.UTF-8`，时区为 `Asia/Shanghai`。配置来源见 [config/sddm/10-mobile.conf](../config/sddm/10-mobile.conf)、[SSH 配置](../config/ssh/00-negroni.conf)和[软件清单](../config/packages/common-tools.txt)。

报告问题时请附设备型号、固件升级历史、父仓库与内核提交、复现步骤和相关日志，说明运行的是哪次镜像；详见[贡献指南](../CONTRIBUTING.md)。
