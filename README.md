# Arch Linux ARM for OnePlus 10 Pro

为 OnePlus 10 Pro（`oneplus-negroni`，目前记录的实机为 **NE2210**）构建 **Arch Linux ARM + Plasma Mobile**，包含内核构建、rootfs 配置、Android 启动镜像封装和离线校验。

基于 [postmarketOS 设备 Wiki](https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_%28oneplus-negroni%29) 的 Arch Linux ARM / XEC 方案，使用 `7.3-rc2` 内核。本仓库维护构建工具与设备配置；Linux、AVB 和固件加载工具以固定提交的 Git 子模块管理。

> 项目处于实验阶段。当前含充电限制的完整镜像已通过本地构建与离线检查，尚未真机启动验证。刷入 rootfs 会覆盖 `userdata`，操作前请阅读[刷机说明](docs/flashing.md)，特别是固件升级历史与 qbootctl 的限制。

## 功能与状态

以下状态截至 **2026-09-16**；详细依据见[设备状态](docs/status.md)。

| 功能 | 当前状态 |
| --- | --- |
| 内核、DTB、模块、rootfs、启动镜像 | 完整构建与离线校验通过 |
| Plasma Mobile / SDDM 虚拟键盘 | 修复已集成；7.2 实机确认可输入，7.3-rc2 待单独记录 |
| 默认 SSH | 早期 7.3-rc2 镜像已在实机确认生效 |
| 电池充电状态 | 电流符号修复已通过实机拔插验证 |
| 熄屏后慢亮屏 | GPU runtime PM 规避服务已部署，并集成到镜像 |
| 常用命令行工具 | 已集成并在本地镜像内检查 |
| 75% / 80% 充电限制 | 默认关闭；构建和逻辑测试通过，待真机验证 |
| USB2 / OTG | 存在 Hub 断连问题，保留诊断记录 |
| 音频、相机、GPS | 尚无本机恢复可用的验证记录 |

## 从这里开始

在 Linux 构建主机上克隆项目并恢复固定版本的源码：

```sh
git clone https://github.com/withsalt/oneplus-negroni-arch-linux.git
cd oneplus-negroni-arch-linux
bash scripts/init-submodules.sh
```

内核固定在 `2c092365fd5c72efcedc29a578601b0a41ce45c4`。其中两个提交尚未发布到内核远端，初始化脚本会从随仓库提供的 [Git bundle](patches/linux/README.md) 恢复；首次克隆请使用上述方式。

接着按[构建指南](docs/building.md)安装依赖、下载 rootfs 并创建本地登录凭据，再执行：

```sh
./build.sh 7.3-rc2
```

产物写入 `images/7.3-rc2/`，构建报告和日志写入 `logs/7.3-rc2/`。镜像、下载缓存与凭据不随 Git 分发。

## 文档

| 目标 | 入口 |
| --- | --- |
| 从源码构建或续建 | [构建指南](docs/building.md) · [构建集成细节](docs/build-integration.md) |
| 核对分区、刷入及首次启动 | [刷机说明](docs/flashing.md) |
| 使用 SSH、了解已知问题 | [设备状态与使用说明](docs/status.md) |
| 开启、配置和关闭充电限制 | [充电限制](docs/charge-limit.md) |
| 查找调研与历史操作记录 | [文档索引](docs/README.md) |
| 提交问题或改动 | [贡献指南](CONTRIBUTING.md) |
| 了解代码来源与许可 | [来源与许可](docs/licensing.md) |

## 仓库结构

```text
.
├── build.sh            构建与续建入口
├── config/             镜像内的服务、桌面、SSH、软件源和包清单
├── scripts/            构建、配置与校验脚本
├── tests/              充电限制的策略、驱动和服务测试
├── docs/               使用文档及历史记录
├── patches/linux/      尚未发布的内核提交及恢复说明
├── analysis/           内核分支对比与差异资料
├── artifacts/          已归档的调研、实现和实机验证依据
├── linux/              内核子模块
└── tools/              AVB 与固件加载工具子模块
```

`alarm/`、`downloads/`、`images/`、`logs/` 为本地构建目录，已由 `.gitignore` 排除。

## 开发检查

```sh
bash scripts/check.sh                 # Shell / Python 语法及服务测试
bash scripts/check.sh --with-kernel   # 另运行固定内核的策略与驱动测试
```

检查不需要 root 或手机；第二条命令要求先初始化子模块。GitHub Actions 执行相同检查，完整镜像构建和实机验证需另外进行。

感谢 [Arch Linux ARM](https://archlinuxarm.org/)、[Xlie Electronic Customs](https://github.com/Xlie-Electronic-Customs)、[postmarketOS](https://postmarketos.org/)、Linux、KDE 及相关上游项目的工作。
