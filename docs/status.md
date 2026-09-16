# 设备状态与使用说明

记录日期：2026-09-16。已记录设备为 OnePlus 10 Pro NE2210，SoC 为 SM8450，GPU 为 Adreno 730。其他 NE221x 变体不视为已验证。

## 源码、镜像和实机

当前内核固定提交为 `2c092365fd5c72efcedc29a578601b0a41ce45c4`，包含电池电流符号修正和默认关闭的充电限制。

- **源码与本地镜像**：充电限制、常用软件、GPU runtime PM 配置均已集成，完整构建与离线检查通过。
- **最近完整刷入的系统**：包含键盘修复和默认 SSH 的早期 7.3-rc2 镜像；SSH 已由用户确认默认生效。
- **随后单独部署的变更**：电池电流符号修复已通过充放电及拔插验证，GPU runtime PM 规避服务已部署并记录验证。
- **尚未完成**：当前完整镜像的真机启动、充电阈值控制及最新常用软件更新的实机验证。

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
| 电池状态图标 | Negroni 电流符号已修正，见[充电状态验证](../artifacts/charging-status-20260915/README.md) |
| 充电限制 | 默认关闭，启用后会阻止系统挂起/休眠；命令确认不等于已验证电源直供，见[使用说明](charge-limit.md) |
| USB2 / OTG | 曾出现 USB2 Hub 与接收器断开而 USB3 继续工作；尚无通用修复，见[诊断报告](../artifacts/usb2-diagnosis-20260916-030952/README.md) |
| 音频、相机、GPS | 尚无本机恢复可用的证据；不能由公共驱动更新推断功能已恢复 |
| 显示、触摸、Wi-Fi、蓝牙、基带 | 仍需对当前完整镜像逐项记录，已有桌面或 SSH 使用记录不能替代完整兼容性测试 |

SDDM 默认使用 Plasma Mobile、Wayland 与 Breeze；镜像语言为 `en_US.UTF-8`，时区为 `Asia/Shanghai`。配置来源见 [config/sddm/10-mobile.conf](../config/sddm/10-mobile.conf)、[SSH 配置](../config/ssh/00-negroni.conf)和[软件清单](../config/packages/common-tools.txt)。

报告问题时请附设备型号、固件升级历史、父仓库与内核提交、复现步骤和相关日志，说明运行的是哪次镜像；详见[贡献指南](../CONTRIBUTING.md)。
