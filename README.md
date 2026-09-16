# Arch Linux ARM for OnePlus 10 Pro

为 OnePlus 10 Pro（`oneplus-negroni`，目前记录的实机为 **NE2210**）构建 **Arch Linux ARM + Plasma Mobile**，包含内核构建、rootfs 配置、Android 启动镜像封装和离线校验。

基于 [postmarketOS 设备 Wiki](https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_%28oneplus-negroni%29) 的 Arch Linux ARM / XEC 方案，使用 `7.3-rc2` 内核。本仓库维护构建工具与设备配置；Linux、AVB 和固件加载工具以固定提交的 Git 子模块管理。

## 优化项目

### 充电限制

集成可选的电池充电限制，镜像默认关闭。启用后，电量达到 **80%** 时请求停充，降到 **75%** 时允许恢复充电；两者之间保持上一状态，首次启用时在此区间先停充。停充保留 USB 输入通路，不主动将电池放电到目标电量。

在手机上执行：

```sh
sudo systemctl start negroni-charge-limit.service          # 仅本次启动
sudo systemctl enable --now negroni-charge-limit.service   # 启动并设置开机自启
sudo systemctl disable --now negroni-charge-limit.service  # 停止限制并取消自启
```

按需选择上述命令。阈值可在 `/etc/negroni-charge-limit.conf` 中通过 `START_THRESHOLD` 和 `END_THRESHOLD` 修改，修改后重启服务生效。停止服务会解除本策略的停充要求，充电仍受固件温控等条件约束。

已修复服务启动一直等待的问题；实机两次启动均不到 1 秒，看门狗通知正常。USB PD、电池 88% 时，启用后电池报告电流为 0，停止后恢复充电，USB 输入持续在线。自然跨越 75% / 80% 阈值、长期直供、SuperVOOC 和无线充电仍待验证。配置、状态查询与验证详情见[充电限制说明](docs/charge-limit.md)。

### 电源策略

插电、电池和低电量三种模式统一预置以下空闲策略：

| 项目 | 默认策略 |
| --- | --- |
| 空闲调暗 | 关闭 |
| 空闲熄屏 | 5 分钟 |
| 空闲自动休眠 | 关闭 |
| GPU 电源管理 | 默认启用 `gpu-runtime-active.service`，保持 GPU 处于 active，规避熄屏后慢亮屏；可能增加待机耗电 |
| 充电限制运行期间 | 阻止系统挂起与休眠，保证持续监测电量；熄屏和 CPU idle 仍可用，停止服务后解除此限制 |

屏幕与休眠配置安装到 `/etc/xdg/powerdevilrc`，可在系统设置中修改，已有用户配置优先。锁屏后的独立熄屏时间沿用上游默认值（当前为 60 秒），手动电源键及低电量临界保护不受上述空闲默认值影响。详见[屏幕与休眠默认设置](docs/screen-defaults.md)和 [GPU 唤醒诊断](artifacts/wake-latency-20260916/)。

### 其他已集成优化

| 项目 | 改进 |
| --- | --- |
| 电池状态显示 | 修正 Negroni 电池电流符号，改善充放电状态判断；已通过实机拔插验证 |
| 登录界面虚拟键盘 | 配置 SDDM / KWin 使用 Plasma Keyboard 和 layer-shell，修复触屏登录输入问题 |
| 远程维护 | 默认启用 SSH，允许 `oneplus` 密码或公钥登录，禁止 root 和空密码登录，首次启动生成主机密钥 |
| 常用工具 | 预装 nano、curl、wget、htop、rsync、git、tmux、ripgrep 等 19 项[命令行工具](config/packages/common-tools.txt) |
| Docker 内核支持 | 每次构建补齐 nftables 兼容与 NAT 模块，支持全新构建和续建；Docker / Compose 按需安装，详见[使用说明](docs/docker.md) |
| 构建与校验 | 通过源码和产物指纹识别需要更新的镜像，核对内核模块及配置，检查 ext4、启动镜像、sparse 往返和 SHA-256 |

## 从这里开始

在 Linux 构建主机上克隆项目并恢复固定版本的源码：

```sh
git clone https://github.com/withsalt/oneplus-negroni-arch-linux.git
cd oneplus-negroni-arch-linux
bash scripts/init-submodules.sh
```

内核固定在 `05e876f60a304a400bc1b44da2de3b155c2bc829`。其中三个提交尚未发布到内核远端，初始化脚本会从随仓库提供的 [Git bundle](patches/linux/README.md) 恢复；首次克隆请使用上述方式。

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
| 调整屏幕和休眠的固件默认值 | [屏幕默认设置](docs/screen-defaults.md) |
| 复现 CPU 性能与大小核调度优化 | [CPU 性能与调度指南](docs/cpu-performance.md) |
| 安装 Docker、检查容器内核支持 | [Docker 使用与固件集成](docs/docker.md) |
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
bash scripts/check.sh --with-kernel   # 另运行固定内核的策略、驱动与 Docker 配置测试
```

检查不需要 root 或手机；第二条命令要求先初始化子模块并安装 ARM64 交叉编译器、flex 和 bison。GitHub Actions 执行相同检查，完整镜像构建和实机验证需另外进行。

感谢 [Arch Linux ARM](https://archlinuxarm.org/)、[Xlie Electronic Customs](https://github.com/Xlie-Electronic-Customs)、[postmarketOS](https://postmarketos.org/)、Linux、KDE 及相关上游项目的工作。
