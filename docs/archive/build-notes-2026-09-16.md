# OnePlus 10 Pro（oneplus-negroni）构建记录与刷机说明

> 历史记录：保留当时的构建与设备状态；文中的本地文件不随 Git 分发。当前操作入口见[文档索引](../README.md)。

项目仓库：<https://github.com/withsalt/oneplus-negroni-arch-linux>。本工作区已迁入 `/home/ubuntu/oneplus-negroni-arch-linux`；内核以子模块保存，包含尚未推送的两个本地提交。迁移范围、缓存和恢复方式见 [MIGRATION.md](workspace-migration-2026-09-16.md)。首次克隆请执行：

```sh
git clone https://github.com/withsalt/oneplus-negroni-arch-linux
cd oneplus-negroni-arch-linux
bash scripts/init-submodules.sh
```

项目链接：[OnePlus 10 Pro（oneplus-negroni）— postmarketOS Wiki](<https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_(oneplus-negroni)>)。

本项目使用 **Arch Linux ARM + Plasma Mobile**，按照上述 Wiki 的 Arch Linux ARM / XEC 主线内核方案构建。**当前使用 `7.3-rc2` 分支，含虚拟键盘修复和默认 SSH 的版本已完成编译并刷入手机，SSH 默认生效已由用户确认。新增常用工具的本地镜像已打包，尚未刷入。2026-09-16 已将 GPU runtime PM 规避方案写入本地镜像构建流程，并直接启用到手机。** 本次未使用 pmbootstrap 制作 postmarketOS 镜像。

### 当前构建与设备状态（2026-09-16）

新增：可选的 75%/80% 充电限制已加入当前源码，固件位于 `images/7.3-rc2/`，服务默认关闭。本次未刷入手机。启停命令与配置见下方「[充电限制](#充电限制2026-09-16)」，实现细节见 [CHARGE-LIMIT.md](../charge-limit.md)。

| 项目 | 当前记录 |
| --- | --- |
| 当前源码分支 / 提交 | `7.3-rc2` / `2c092365fd5c72efcedc29a578601b0a41ce45c4` |
| 内核 / 桌面 | `7.3.0-rc2-sm8450+` / Plasma Mobile `6.7.5` |
| 完整构建 | 2026-09-15 21:16 完成；内核、727 个模块、DTB、initramfs 和全套 Android 镜像均已生成 |
| 首次刷入 | 2026-09-15 21:34–21:38，从原 7.2 系统更新至包含键盘修复的 7.3-rc2 |
| 最近刷入 | 2026-09-15 22:29–22:33，重新刷入包含键盘修复和默认 SSH 的 7.3-rc2（617 个包）；所有擦除、写入及重启命令均返回 `OKAY` |
| 设备 | OnePlus 10 Pro NE2210；序列号 `e2d3522f`；fastboot `product=taro` |
| 刷机时状态 | 已解锁；bootloader fastboot（`is-userspace=no`）；双槽，保持当前槽位 `a` |
| 分区容量 | `boot_a` 为 201,326,592 字节（192 MiB）；`userdata` 为 496,241,799,168 字节 |
| 数据处理 | 按用户要求不备份，擦除并写入 userdata；本次没有刷写 ABL |
| 登录虚拟键盘 | 修复已集成到构建步骤和已刷入的 7.3-rc2 rootfs |
| 默认 SSH | 已刷入并由用户确认默认生效；OpenSSH 开机自启，使用现有 `oneplus` 账号 |
| 常用软件 | 2026-09-16 00:50 完成本地镜像更新与打包，内置 nano、zip、unzip 等 19 项常用工具，rootfs 共 639 个包；此次新增软件尚未刷入手机 |
| GPU runtime PM | `gpu-runtime-active.service` 已在手机启用，并加入后续镜像；启动图形目标前将 A730 的 `power/control` 设为 `on`，以避免熄屏后 GPU/GMU resume 超时导致慢亮屏 |
| 充电限制 | 2026-09-16 14:15 完成固件封装；默认关闭，服务启动后 ≤75% 恢复、≥80% 停充；尚未刷入或真机验证 |
| 充电限制提交 | `2c092365fd5c`，仅提交内核驱动及状态机头文件；服务、配置和构建脚本由本父仓库管理，`linux/` 为子模块 |
| 新固件 / 日志 | `images/7.3-rc2/` / `logs/7.3-rc2/` |

以上时间均为 Asia/Shanghai。手机运行最近刷入的键盘修复与默认 SSH 版本，用户已确认“ssh默认生效”，见 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/ssh-hardware-verification.json`（本地文件）。本地 `images/7.3-rc2/` 已进一步加入常用软件，该更新尚未部署到手机。GPU runtime PM 规避配置已直接部署到手机，并会写入重新打包后的本地镜像；验证记录在 [artifacts/wake-latency-20260916/](../../artifacts/wake-latency-20260916/)。本次 7.3-rc2 的登录画面和键盘操作仍待单独记录；此前 7.2 环境中的键盘修复已由用户确认可输入，见 [修复说明](keyboard-fix-2026-09-15.md)。

刷机前用户确认原厂系统历史为 ColorOS 14，未升级到 ColorOS/OxygenOS 15；首次从基于 7.2 的 Linux 更新，本次在已有 7.3-rc2 系统上重新刷入。完整原厂版本号、ABL 具体版本和主线系统内的 `root=/dev/sda14` 映射未另行记录。最近一次实际操作见 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/flash.log`（本地文件） 和 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/result.json`（本地文件），首次刷入记录保留在文末索引，SSH 配置见 [构建集成说明](../build-integration.md)。

后续只需要刷机时，直接阅读「[产物与分区对应关系](#产物与分区对应关系)」「[刷机前必须核对的条件](#刷机前必须核对的条件)」和「[后续刷机操作](#后续刷机操作)」。当前镜像已经生成，不需要重新运行构建脚本。

此前已完成 [7.2、7.3-rc2、master 分支对比](../../analysis/kernel-branches.md)：新分支包含 A730 电源管理、触摸 DMA 和公共驱动调整，尚无本机声音、摄像头或 GPS 已恢复的证据。该分析记录之后，已按用户选择完成 `7.3-rc2` 全套构建和刷入；源码差异及分支分析仍保存在 `analysis/`。

## 充电限制（2026-09-16）

已修改当前内核驱动 `qcom_battmgr`，并将配置、systemd 服务和镜像更新步骤接入构建流程。新固件及 `images/7.3-rc2/SHA256SUMS`（本地文件） 位于 `images/7.3-rc2`（本地文件）。**本次只完成源码修改与本地构建，未刷写手机，也未在手机上启用充电限制。**

服务和驱动策略均默认关闭。启动服务后，电量 **≥80% 请求停充，≤75% 允许恢复充电**；两者之间保持上一状态，首次开启时若处于区间内则先停充。上下限可配置，仍服从固件温控等限制。停充保留 USB 输入通路，以优先利用外接电源；实际能否稳定由适配器供整机仍需真机测量，命令确认不代表已验证电源直供。如果当前电量为 100%，开启限制不会强制放电到 80%。

以下命令在运行新固件的手机上执行。仅本次启动服务：

```sh
sudo systemctl start negroni-charge-limit.service
```

启动并设置以后开机自动运行：

```sh
sudo systemctl enable --now negroni-charge-limit.service
```

编辑 `/etc/negroni-charge-limit.conf`，默认内容如下，要求 `0 <= START_THRESHOLD < END_THRESHOLD <= 100`：

```ini
START_THRESHOLD=75
END_THRESHOLD=80
```

修改配置后重启服务，并查看状态：

```sh
sudo systemctl restart negroni-charge-limit.service
systemctl status negroni-charge-limit.service --no-pager
/usr/local/libexec/negroni-charge-limit status
journalctl -u negroni-charge-limit.service -b --no-pager
```

`enabled=1 state=holding error=0` 表示停充命令已确认；`released` 表示本策略放行充电；`pending` 表示尚未成功应用，应检查错误和日志。关闭限制并取消开机自启：

```sh
sudo systemctl disable --now negroni-charge-limit.service
```

**服务启用期间会阻止系统挂起和休眠，熄屏与 CPU idle 仍可用。** 这是为了持续监测电量；拔掉电源后，只要服务仍运行，该限制也仍存在。停止服务会解除挂起限制并请求恢复正常充电，固件离线时等待重连后恢复。

固件和日志统一使用 `images/7.3-rc2/`、`logs/7.3-rc2/`。后续在当前工作区执行 `./build.sh 7.3-rc2`，会检查源码与配置变化，更新 rootfs 内的模块和 initramfs，再重新封装；生成的镜像仍默认关闭服务。

内核改动已提交到本地 `7.3-rc2` 分支：`2c092365fd5c72efcedc29a578601b0a41ce45c4`（`power: supply: qcom_battmgr: add opt-in Negroni charge limits`）。提交只含 `qcom_battmgr.c` 与 `qcom_battmgr_charge_limit.h`，未推送远端。服务、配置、构建脚本、测试和本文档已纳入本父仓库，`linux/` 作为子模块。两个未推送的内核提交已保存为 [Git bundle](../../patches/linux/README.md)，首次克隆后运行 `bash scripts/init-submodules.sh` 恢复。迁移工作区时需保留这些配套文件，并使用该内核提交；也可在其父提交上应用 [导出的内核提交补丁](../../artifacts/charge-limit-implementation/qcom-battmgr-charge-limit.patch)。

已通过完整内核交叉编译、阈值与驱动逻辑测试、8 项服务测试、systemd 单元检查，以及最终镜像文件比对、ext4 检查、sparse 往返校验和启动镜像解包比对。新固件启动、实际停充/恢复、稳定外接供电、SuperVOOC 电量计行为及无线充电尚未真机验证。结果见 `logs/7.3-rc2/build-result.json`（本地文件）、`logs/7.3-rc2/charge-limit-verification.json`（本地文件）和[实现记录](../../artifacts/charge-limit-implementation/README.md)。

## 参考资料与固定输入

| 项目 | 本次使用的内容 |
| --- | --- |
| 项目页面 | [OnePlus 10 Pro（oneplus-negroni）](<https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_(oneplus-negroni)>) |
| 教程固定版本 | [postmarketOS Wiki revision 106143](https://wiki.postmarketos.org/index.php?title=OnePlus_10_Pro_(oneplus-negroni)&oldid=106143) |
| 教程本地快照 | `downloads/tutorial.html`（本地文件） |
| 工作目录 | `/home/ubuntu/oneplus-negroni-arch-linux` |
| 内核仓库 | `https://github.com/withsalt/linux` |
| 内核分支 / 提交 | `7.3-rc2` / `2c092365fd5c72efcedc29a578601b0a41ce45c4` |
| 内核配置 | ARM64 `defconfig`，随后合并 `sm8450.config` |
| Arch Linux ARM 输入 | `https://ca.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz` |
| 输入归档 MD5 | `23eec86365b24f7913c403e8f4e8719b`，已与官方 `.md5` 文件核对 |
| 输入归档 SHA-256 | `42a4eeaa038994ffd31fa173256ef2f0ef511358eeb41b9ea1f8626391b9b319` |
| 设备软件仓库 | `https://xlie-electronic-customs.github.io/xec-arch-repo/` |
| avbtool 仓库 | `https://android.googlesource.com/platform/external/avb` |
| avbtool 提交 / 版本 | `c5066a96caa7bf4150c0a8cc8cc14ab81733fdc7` / `1.4.0` |
| 宿主机 | Ubuntu 26.04 x86_64，8 个逻辑 CPU，内存约 14 GiB |
| 交叉编译器 | `aarch64-linux-gnu-gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0` |
| Android 工具 | `mkbootimg`、`img2simg`、`simg2img`；本机 adb/fastboot 为 `34.0.5-debian` |

当前构建结果及每个文件的字节数、SHA-256 见 `logs/7.3-rc2/build-result.json`（本地文件）。原始归档、工具和 7.2 输入记录见 `logs/build-inputs.json`（本地文件）；本次 7.3-rc2 重新编译内核，复用原 rootfs 软件包基础并合入键盘、SSH 配置及常用软件。Arch 和 XEC 均为滚动仓库，重新下载、升级软件包不保证产生相同镜像，应保留源码提交、归档、包缓存和校验清单。

## 目录与文件

```text
oneplus-negroni/
├── README.md                         本文件
├── build.sh                          ./build.sh 7.3-rc2 构建/续建
├── BUILD-INTEGRATION.md              分支目录、键盘、SSH 和常用软件构建接入
├── CHARGE-LIMIT.md                   充电限制的使用、源码和验证说明
├── image-credentials.json            镜像登录凭据，权限 0600
├── images/
│   ├── 7.3-rc2/                      当前镜像和 SHA256SUMS（含默认关闭的充电限制）
│   └── 7.2/                          原 7.2 产物归档，保留原目录名
├── config/ssh/00-negroni.conf         默认 SSH 登录配置
├── config/systemd/gpu-runtime-active.service  A730 runtime PM 规避服务
├── config/systemd/negroni-charge-limit.service  默认关闭的充电限制服务
├── config/charge-limit/             充电限制脚本与 75/80 默认配置
├── config/packages/common-tools.txt 默认安装的常用软件清单
├── config/pacman/mirrorlist          Arch Linux ARM 软件源列表
├── keyboard-fix/10-mobile.conf       修复后的 SDDM 配置副本
├── linux/                            固定提交的内核源码
│   └── output/
│       ├── .config                   实际内核配置
│       ├── arch/arm64/boot/Image.gz   ARM64 压缩内核
│       ├── arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb
│       └── out-modules/lib/modules/7.3.0-rc2-sm8450+/
├── alarm/                            ARM64 构建用 chroot
│   ├── mnt/                          构建时挂载 images/7.3-rc2/root.img；结束后卸载
│   └── var/cache/pacman/pkg/         Arch/XEC 软件包缓存
├── downloads/
│   ├── ArchLinuxARM-aarch64-latest.tar.gz
│   ├── ArchLinuxARM-aarch64-latest.tar.gz.md5
│   ├── tutorial.html                 本次教程快照
│   └── xec.db                        下载时的 XEC 仓库索引
├── tools/
│   ├── avb/                          Android 官方 avbtool
│   └── msm-firmware-loader/          用于核对固件加载逻辑的上游源码
├── scripts/
│   ├── build-kernel.sh
│   ├── prepare-rootfs.sh
│   ├── finalize-rootfs.sh
│   ├── configure-ssh.sh
│   ├── enable-ssh-image.sh
│   ├── install-common-tools-image.sh
│   ├── pack-images.sh
│   ├── configure-gpu-runtime-active.sh
│   ├── enable-gpu-runtime-active-image.sh
│   ├── verify-sparse.py
│   ├── verify-ssh-image.py
│   └── write-build-report.py
└── logs/
    ├── 7.3-rc2/                      当前构建、配置、清单及校验记录
    │   ├── pre-ssh/                  首次刷入（未含 SSH）的结果和 SHA256SUMS
    │   └── pre-common-tools/         添加常用软件前的构建结果、包清单和 SHA256SUMS
    ├── flash-7.3-rc2-2026-09-15.*     首次刷机日志及结果
    └── flash-7.3-rc2-ssh-2026-09-15-2228/  最近含 SSH 镜像的刷机记录及校验清单
```

`downloads/host-packages.plan`、`host-packages.tsv` 和空的 `debs/` 是最初评估本地解包依赖时留下的准备文件；最终采用 sudo 正常安装依赖，不依赖这些文件。`alarm/` 是构建环境，最终系统在 `images/7.3-rc2/root.img` 内，不应将整个 `alarm/` 当成手机 rootfs。

## 完整构建过程

以下按当前 `7.3-rc2` 分支说明构建步骤，包含已接入脚本的虚拟键盘修复、默认 SSH、GPU runtime PM 规避和常用软件。除明确标注为 chroot 内部命令的部分外，命令均从项目目录执行。**不要在已有产物上重新执行格式化命令。** 若要从头重建，应克隆本仓库并运行 `bash scripts/init-submodules.sh`，再重新准备输入及凭据。

现有工作区的构建/续建入口为：

```sh
cd /home/ubuntu/oneplus-negroni-arch-linux
./build.sh 7.3-rc2
```

入口将产物写到 `images/7.3-rc2/`、日志写到 `logs/7.3-rc2/`。每次都会运行增量 Kbuild；源码或充电服务配置变化时，通过校验值更新 rootfs 中的模块和服务并重新封装，其余阶段按完成标记续建。下面的单阶段命令显式传入相同目录；切换其他内核分支重新构建时，应准备独立或全新的内核输出目录，避免复用上一分支的 `.config` 和模块。

### 1. 安装宿主依赖

本机已有 Git、curl、Python 3、make、宿主 GCC、bc、cpio、kmod、rsync、zstd、xz 和 e2fsprogs。额外安装了：

```sh
cd /home/ubuntu/oneplus-negroni-arch-linux
sudo apt-get install -y \
  gcc-aarch64-linux-gnu flex bison libssl-dev libelf-dev \
  device-tree-compiler pahole qemu-user qemu-user-binfmt binfmt-support \
  mkbootimg android-sdk-libsparse-utils
```

Ubuntu 是 x86_64，内核使用交叉编译器；ARM64 用户空间通过 QEMU/binfmt 在 chroot 中执行。挂载镜像和 chroot 操作使用 sudo。宿主机认证密码没有写入本项目或镜像。

### 2. 下载源码、工具和根文件系统归档

全新工作目录可用下面的命令准备输入；当前目录已经具有源码、归档和工具，无需再次 clone：

```sh
mkdir -p downloads logs/7.3-rc2 scripts images/7.3-rc2 tools
bash scripts/init-submodules.sh

curl -L --fail --retry 3 -C - \
  -o downloads/ArchLinuxARM-aarch64-latest.tar.gz \
  https://ca.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz
curl -L --fail \
  -o downloads/ArchLinuxARM-aarch64-latest.tar.gz.md5 \
  https://ca.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz.md5
(cd downloads && md5sum -c ArchLinuxARM-aarch64-latest.tar.gz.md5)
```

官方归档校验结果为 `OK`。内核及工具版本由父仓库的子模块提交固定。初始化脚本从 Git bundle 恢复原始本地内核提交 `2c092365fd5c72efcedc29a578601b0a41ce45c4`，不依赖它已存在于远端。可以核对：

```sh
git submodule status
git -C linux rev-parse HEAD
```

### 3. 编译内核、DTB 和模块

执行 [scripts/build-kernel.sh](../../scripts/build-kernel.sh)：

```sh
LOG_DIR=logs/7.3-rc2 bash scripts/build-kernel.sh
```

脚本在首次尚无 `.config` 时依次执行以下内核命令；下面的相对路径从 `linux/` 开始：

```sh
make O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 defconfig
make O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8 sm8450.config
make O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
make O=output ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
  INSTALL_MOD_PATH=out-modules modules_install
```

编译成功，生成 `Image.gz`、`sm8450-oneplus-negroni.dtb` 和 727 个模块。版本字符串 `7.3.0-rc2-sm8450+` 中的 `+` 是本次构建产生的实际后缀，模块目录和 initramfs 使用同一版本，不应手工删掉。DTB 的 `model` 为 `OnePlus 10 Pro 5G`，`compatible` 为 `oplus,negroni qcom,sm8450`。

内核源码基于 `593ad628e5b838457d6a316cc08ba5dcc924a01f`，包含 Negroni 电池电流符号修复：设备树兼容 `oplus,negroni` 时，将固件上报的电流符号转换为 Linux power_supply ABI 约定，使 UPower 正确显示充电状态。7.3-rc2 编译日志中有两条上游 `qcom_battmgr.c` 指针比较警告，没有编译错误。详情见 `logs/7.3-rc2/kernel-build.log`（本地文件）。

### 4. 建立 ARM chroot 并安装 rootfs

执行 [scripts/prepare-rootfs.sh](../../scripts/prepare-rootfs.sh)：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/prepare-rootfs.sh
```

全新安装时，脚本完成以下操作。本次 7.3-rc2 复用了原 7.2 的 rootfs 软件包基础，并在后续步骤加入新内核、键盘修复、OpenSSH 和常用软件：

1. 将归档保留数值 UID/GID 解包到 `alarm/`，用 `chroot alarm /bin/true` 验证 ARM 执行能力；必要时注册 `qemu-aarch64` binfmt。
2. 使用 `unshare --mount --propagation private` 隔离挂载；绑定 chroot 自身及 `/dev`、`/sys`，挂载 `/proc`、临时 `/run`；退出时清理挂载。
3. 准备 DNS、`/etc/mtab -> /proc/self/mounts` 和 `KEYMAP=us`。从 [config/pacman/mirrorlist](../../config/pacman/mirrorlist) 安装软件源列表，优先使用 `de3.mirror.archlinuxarm.org`，加拿大镜像站为备用；保留 TLS 及软件包签名校验。
4. 在 ARM chroot 中初始化并填充 `archlinuxarm` 包签名密钥；移除构建环境原来的 `linux-aarch64` 和通用 `linux-firmware*` 包，更新系统并安装 `archinstall`、`arch-install-scripts`、`mkinitcpio`。
5. 按教程增加 XEC 源；设置 `ParallelDownloads = 8`，并启用教程使用的 `DisableSandboxFilesystem`、`DisableSandboxSyscalls`。这些设置只作用于 ARM 构建环境和生成的系统。
6. 仅当 `images/7.3-rc2/root.img` 不存在时，创建 5 GiB 文件并格式化为标签 `root` 的 ext4；以 loop 方式挂载到 `alarm/mnt`。
7. 通过 `pacstrap -c` 安装下面的软件包，使用构建环境的包缓存；生成的系统无需保存同一份下载缓存。

XEC 源配置与教程一致：

```ini
[xec]
SigLevel = Optional TrustAll
Server = https://xlie-electronic-customs.github.io/xec-arch-repo/
```

创建镜像的内部命令为：

```sh
# 仅用于首次创建新镜像；不要对已完成的 root.img 重复格式化。
truncate -s 5G images/7.3-rc2/root.img
mkfs.ext4 -F -L root images/7.3-rc2/root.img
```

以下两组命令由脚本在 **ARM chroot 内**运行，其中 `/mnt` 是已挂载的最终镜像：

```sh
pacstrap -c /mnt --needed --noconfirm \
  base pacman archlinuxarm-keyring systemd-sysvcompat networkmanager openssh \
  msm-firmware-loader msm-modem-uim-selection linux-firmware-oplus-negroni \
  rmtfs-oplus tqftpserv

pacstrap -c /mnt --needed --noconfirm \
  sddm plasma-mobile plasma-desktop bluedevil spacebar qmlkonsole \
  plasma-dialer plasma-settings kscreen plasma-nm modemmanager \
  mkinitcpio sudo qt6-virtualkeyboard
```

按教程使用 `pacman -r /mnt -Rdd --noconfirm iio-sensor-proxy` 移除暂不适用的传感器代理。因此普通依赖一致性检查可能指出这个被有意移除的依赖，不能据此直接把它加回。

准备阶段启用 `sddm`、`NetworkManager`、`ModemManager`、`msm-modem-uim-selection`、`bluetooth`、`systemd-timesyncd`、`msm-firmware-loader`、`msm-firmware-loader-unpack`、`rmtfs` 和 `tqftpserv`。下一步配置账号后启用 `sshd`。最终服务状态见 `logs/7.3-rc2/rootfs-enabled-services.txt`（本地文件）。

首次安装曾报告 `could not determine cachedir mount point /var/cache/pacman/pkg`，继而误报空间不足。通过为 chroot 根目录增加自身 bind mount 修复了挂载点识别，随后安装成功；没有通过禁用 `CheckSpace` 绕过检查，也没有扩大 root.img。失败和后续成功记录均保留在 `logs/rootfs-build.log`（本地文件）。

### 5. 配置账号、桌面、内核模块和 initramfs

凭据在 `image-credentials.json` 中，本次单独随机生成，文件权限为 `0600`。**现有文件与当前镜像配套，不能只改 JSON 就期待镜像中的密码一起改变。** 若在全新工作目录重建，可以先用下面的命令创建凭据；`O_EXCL` 会拒绝覆盖已有文件：

```sh
python3 - <<'PY_CREDENTIALS'
import json, os, secrets, string
credentials = {
    'username': 'oneplus',
    'password': ''.join(secrets.choice(string.digits) for _ in range(10)),
    'root_password': secrets.token_urlsafe(15),
}
fd = os.open('image-credentials.json', os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, 'w') as stream:
    json.dump(credentials, stream, indent=2)
    stream.write('\n')
PY_CREDENTIALS
```

内核和软件包安装完成后，执行 [scripts/finalize-rootfs.sh](../../scripts/finalize-rootfs.sh)：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/finalize-rootfs.sh
```

此脚本再次在私有挂载命名空间中挂载镜像，并完成：

- 复制 `out-modules/lib/modules/` 到镜像的 `/usr/lib/modules/`，复制 `Image.gz`、内核配置和 `System.map` 到 `/boot/`，执行匹配版本的 `depmod`。
- 设置主机名 `oneplus-negroni`、语言 `en_US.UTF-8`、键盘 `us`、时区 `Asia/Shanghai`；rootfs 的 `/etc/fstab` 使用 `LABEL=root / ext4 defaults,noatime 0 1`。
- 创建普通用户 `oneplus`，加入 `wheel,audio,video,input`；通过 `chpasswd` 写入普通用户及 root 密码，并启用需要认证的 wheel 组 sudo。没有把明文传给要求密码哈希的 `useradd -p`。
- 配置 SDDM 使用 KWin Wayland、Breeze 主题和由 KWin 启动的 Plasma Keyboard，合入登录界面虚拟键盘修复；默认会话为 `/usr/share/wayland-sessions/plasma-mobile.desktop`，默认目标为 `graphical.target`，未启用自动登录。
- 调用 `scripts/configure-ssh.sh`，安装默认 SSH 登录配置并启用 `sshd.service`；用临时密钥执行 `sshd -t` / `sshd -T` 检查。
- 调用 `scripts/configure-gpu-runtime-active.sh`，安装并启用 `gpu-runtime-active.service`；该服务在图形目标启动、显示管理器启动前将 A730 GPU 的 runtime PM 控制设为 `on`。
- 配置 `/etc/resolv.conf -> /run/NetworkManager/resolv.conf`；清空 `machine-id`，使设备首次启动自行生成身份。
- 将 mkinitcpio 的 `MODULES` 设为 `(phy-qcom-qmp-ufs)`；按教程移除 `autodetect`、`microcode`、`kms`，保留 `base systemd modconf keyboard sd-vconsole block filesystems fsck`。

#### 登录界面虚拟键盘修复

`finalize-rootfs.sh` 将以下配置写入最终 rootfs 的 `/etc/sddm.conf.d/10-mobile.conf`，属主为 root:root，权限为 0644：

```ini
[General]
DisplayServer=wayland
InputMethod=
GreeterEnvironment=QT_WAYLAND_SHELL_INTEGRATION=layer-shell

[Theme]
Current=breeze

[Wayland]
CompositorCommand=kwin_wayland --drm --no-lockscreen --no-global-shortcuts --locale1 --inputmethod /usr/bin/plasma-keyboard
```

Breeze 的 Wayland 登录界面通过 KWin 管理虚拟键盘。因此需要清空原来的 `InputMethod=qtvirtualkeyboard`，由 `kwin_wayland --inputmethod /usr/bin/plasma-keyboard` 启动键盘，并设置 layer-shell 集成。当前镜像已安装 `plasma-keyboard 6.7.5-1` 和 `qt6-virtualkeyboard`；检查最终 rootfs 中 `/usr/bin/plasma-keyboard` 可执行，且 KWin、Breeze 主题和 Wayland 依赖存在。

此配置随 rootfs 构建写入，新镜像首次启动即可使用修复后的设置。配置副本见 [keyboard-fix/10-mobile.conf](../../config/sddm/10-mobile.conf)，原因和原 7.2 真机验证见 [KEYBOARD-FIX-2026-09-15.md](keyboard-fix-2026-09-15.md)。

#### 默认启用 SSH 远程登录

第 4 步的 `pacstrap` 安装 `openssh`。第 5 步创建 `oneplus` 账号后，`configure-ssh.sh` 将 [config/ssh/00-negroni.conf](../../config/ssh/00-negroni.conf) 以 root:root、0644 安装到 `/etc/ssh/sshd_config.d/00-negroni.conf`：

```sshconfig
Port 22
PermitRootLogin no
AllowUsers oneplus
PasswordAuthentication yes
PubkeyAuthentication yes
PermitEmptyPasswords no
KbdInteractiveAuthentication no
UsePAM yes
```

脚本在最终 rootfs 的 chroot 内执行 `systemctl enable sshd.service`。SSH 随系统开机启动，使用现有 `oneplus` 账号和图形登录密码，支持用户配置公钥，禁止 root 远程登录和空密码。镜像不预置 SSH 主机私钥；首次启动由 `sshdgenkeys.service` 执行 `ssh-keygen -A`，为设备生成自己的密钥后启动 sshd。构建检查使用 `/run` 的临时密钥，检查后删除，不将它打入镜像。

手机连接 Wi-Fi 后，在同一局域网的电脑上执行：

```sh
ssh oneplus@手机IP
```

手机 IP 可在网络设置、路由器 DHCP 列表或手机终端的 `ip -4 addr` 输出中查看。镜像没有预置 Wi-Fi 凭据，需要先完成联网；这里没有配置 USB 网络或固定 USB IP。SSH 服务不依赖图形桌面登录，但仍需要网络可达。

对于已有完整 rootfs，`build.sh` 检测到缺少 `ssh.complete` 时调用 [scripts/enable-ssh-image.sh](../../scripts/enable-ssh-image.sh) 补装、配置 SSH，并使旧的打包完成标记失效，随后重新生成 sparse 和校验清单。手动执行等价步骤为：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/enable-ssh-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

当前本地镜像已完成 SSH 配置、自启、密码哈希一致性和无共享主机密钥检查。早期 QEMU 登录测试因不支持 OpenSSH 使用的 `PR_SET_SECCOMP` 而在认证前中断，记录保留在 `logs/7.3-rc2/ssh-verification.json`（本地文件）；镜像沙箱设置未因此修改。最近刷入的 SSH 版本已由用户确认默认生效，见 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/ssh-hardware-verification.json`（本地文件） 和 `logs/7.3-rc2/sshd-effective.conf`（本地文件）。该真机确认对应添加常用软件之前的镜像。

#### 熄屏后慢亮屏的 GPU runtime PM 规避

实机检查确认慢亮屏不由整机 suspend 引起：`suspend_success=0`。默认 `power/control=auto` 时，熄屏后 A730 GPU 和 GMU 会 runtime suspend；亮屏恢复路径会阻塞在 `a6xx_gmu_resume -> a6xx_gmu_set_freq -> a6xx_hfi_set_freq -> a6xx_hfi_wait_for_msg_interrupt`，内核记录 `HFI_H2F_MSG_GX_BW_PERF_VOTE ... timed out`。将 GPU 的 runtime PM 策略改为 `on` 后，连续两次 DPMS 亮屏实测约 `0.37-0.41 s`，未出现该 HFI 超时。原始状态、日志和周期记录保存在 [wake-latency-20260916](../../artifacts/wake-latency-20260916/)。

[config/systemd/gpu-runtime-active.service](../../config/systemd/gpu-runtime-active.service) 是启用到 `graphical.target` 的 one-shot 服务，在 `display-manager.service` 前执行：

```sh
printf on > /sys/bus/platform/devices/3d00000.gpu/power/control
```

`on` 是 Linux runtime PM 的“禁止该设备自动 runtime suspend”策略，不是保持屏幕常亮，也不会阻止系统执行整机 suspend。它让 GPU/GMU 在熄屏期间维持可用状态，代价是待机功耗增加。当前设备按长期插电使用的要求接受此代价。

全新 rootfs 在 `finalize-rootfs.sh` 中自动配置。对于已有的 rootfs，`build.sh` 发现缺少 `gpu-runtime-pm.complete` 时会执行 [scripts/enable-gpu-runtime-active-image.sh](../../scripts/enable-gpu-runtime-active-image.sh)，使旧的打包标记失效并重新生成 sparse 镜像和校验清单。手动步骤为：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 \
  bash scripts/enable-gpu-runtime-active-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

手机已通过 SSH 直接安装同一 unit。检查当前状态：

```sh
systemctl is-enabled gpu-runtime-active.service
systemctl is-active gpu-runtime-active.service
cat /sys/bus/platform/devices/3d00000.gpu/power/control
```

预期依次为 `enabled`、`active`、`on`。若以后需要恢复省电策略，执行：

```sh
sudo systemctl disable --now gpu-runtime-active.service
echo auto | sudo tee /sys/bus/platform/devices/3d00000.gpu/power/control
```

#### 生成匹配内核的 initramfs

在最终 rootfs 的 chroot 内生成 initramfs：

```sh
mkinitcpio -k '7.3.0-rc2-sm8450+' -g /boot/initramfs-linux.img
```

生成成功后导出为 `images/7.3-rc2/initramfs-linux.img`，并检查 UFS 模块、Wi-Fi 固件、桌面会话和主题文件存在。密码认证配置通过 `visudo -c` 检查。日志和 initramfs 文件清单见 `logs/7.3-rc2/rootfs-finalize.log`、`logs/7.3-rc2/initramfs-contents.txt`。

#### 默认安装常用软件

软件清单统一维护在 [config/packages/common-tools.txt](../../config/packages/common-tools.txt)，由 [scripts/install-common-tools-image.sh](../../scripts/install-common-tools-image.sh) 安装到最终 rootfs：

| 用途 | 默认软件 |
| --- | --- |
| 编辑、阅读与终端 | `nano`、`less`、`tmux`、`bash-completion` |
| 压缩与解压 | `zip`、`unzip` |
| 下载与同步 | `curl`、`wget`、`rsync` |
| 文件搜索与数据处理 | `file`、`tree`、`ripgrep`（`rg`）、`fd`、`jq`、`which` |
| 系统排查 | `htop`、`lsof`、`strace` |
| 版本管理 | `git` |

运行 `./build.sh 7.3-rc2` 时，此步骤在 SSH 配置之后、打包之前自动执行。脚本比较清单与 `logs/7.3-rc2/common-tools.list`；新增或修改清单后会重新安装、验证并打包。安装使用现有仓库数据库和 `pacman -S --needed`，不执行整套系统升级；从清单移除条目不会卸载镜像中已有的软件。也可手动更新已有镜像：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/install-common-tools-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

本次清单共 19 项，其中 `curl`、`file`、`ripgrep` 已存在；实际新增 16 个工具包和 6 个依赖，rootfs 从 617 个包增至 639 个包，已有包版本保持不变。安装约增加 69 MiB，5 GiB 文件系统仍有约 1.1 GiB 可用空间。软件源下载曾遇加拿大镜像站超时及通用镜像站证书域名不匹配，现统一使用前述镜像列表，首轮失败日志保存在 `logs/7.3-rc2/common-tools-install-first-attempt.log`。

已在 ARM64/QEMU 环境验证程序执行、ZIP 压缩解压、rsync 复制、JSON 处理、文件搜索和 Git 初始化。结果见 `logs/7.3-rc2/common-tools-verification.json`（本地文件）、`logs/7.3-rc2/common-tools-versions.txt`（本地文件） 和 `logs/7.3-rc2/common-tools-install.log`（本地文件）。更新后的镜像尚未刷入手机。

键盘、SSH 配置和常用软件属于 rootfs 内容，单独刷写 boot 不会把这些更新带到手机；修改现有 rootfs 后需重新封装并部署对应根文件系统，或在已运行的手机系统中应用更新。

### 6. 打包 Android 镜像

rootfs 配置结束并卸载后，执行 [scripts/pack-images.sh](../../scripts/pack-images.sh)：

```sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

实际打包参数如下，路径均相对于项目根目录：

```sh
python3 tools/avb/avbtool.py make_vbmeta_image \
  --output images/7.3-rc2/vbmeta.img --flags 2

mkbootimg --header_version 4 --vendor_boot images/7.3-rc2/vendor_boot.img \
  --dtb linux/output/arch/arm64/boot/dts/qcom/sm8450-oneplus-negroni.dtb \
  --pagesize 4096

mkbootimg --kernel linux/output/arch/arm64/boot/Image.gz \
  --ramdisk images/7.3-rc2/initramfs-linux.img \
  --header_version 4 --pagesize 4096 \
  --os_version 12.0.0 --os_patch_level 2026-03-00 \
  --cmdline 'console=tty0 root=/dev/sda14 rootwait rw bootconfig' \
  -o images/7.3-rc2/boot.img

mkbootimg --ramdisk images/7.3-rc2/initramfs-linux.img \
  --header_version 4 --pagesize 4096 \
  --os_version 12.0.0 --os_patch_level 2026-03-00 \
  -o images/7.3-rc2/recovery.img

img2simg images/7.3-rc2/root.img images/7.3-rc2/sparse_root.img
```

这些是 header v4 镜像，页大小 4096 字节；Android 版本字段为 `12.0.0`，补丁月份为 `2026-03`。这两个字段是沿用教程的 Android 镜像头参数，不代表装入了 Android 12 系统。

### 7. 离线校验及续建行为

本次已完成：

| 校验 | 结果 / 证据 |
| --- | --- |
| 官方 Arch 归档 MD5 | 通过，见归档旁 `.md5` 文件 |
| 内核与模块编译 | 成功，`7.3.0-rc2-sm8450+`，727 个模块；见 `logs/7.3-rc2/kernel-build.log` |
| DTB 设备标识 | `OnePlus 10 Pro 5G`、`oplus,negroni qcom,sm8450` |
| initramfs | 生成成功，包含 `phy-qcom-qmp-ufs.ko`；见 `logs/7.3-rc2/initramfs-contents.txt` |
| ext4 | `e2fsck -fn images/7.3-rc2/root.img` 五阶段检查通过；见 `logs/7.3-rc2/image-pack.log` |
| boot 解包比对 | 内核与 ramdisk 分别和构建输入 `cmp` 一致 |
| vendor_boot 解包比对 | DTB 与 negroni DTB 输入完全一致 |
| recovery 解包比对 | ramdisk 与导出的 initramfs 完全一致，kernel_size=0 符合教程 |
| vbmeta | `avbtool info_image` 显示 Flags=2，Algorithm=NONE |
| sparse 转换 | `simg2img` 还原后字节数与 SHA-256 均和 root.img 一致；见 `logs/7.3-rc2/sparse-roundtrip.json` |
| 最终清单 | 7 个镜像的 SHA-256 已写入 `images/7.3-rc2/SHA256SUMS` |
| 登录键盘配置 | 与修复副本一致，root:root / 0644，Plasma Keyboard 已安装 |
| SSH | 软件包、自启、生效配置、密码哈希和主机密钥策略通过检查；最近刷入版本的默认 SSH 已由用户确认生效 |
| GPU runtime PM | `gpu-runtime-active.service` 已启用；离线检查 unit、systemd 启用状态，真机在 `on` 策略下两次 DPMS 亮屏约 `0.37-0.41 s` 且无 HFI 超时 |
| 常用软件 | 19 项全部安装，ARM 程序及文件操作检查通过；rootfs 共 639 个包 |
| 实际刷入和重启 | 7.3-rc2（含键盘修复、默认 SSH，617 个包）已重新刷入并重启，所有命令 `OKAY`；新增常用软件的镜像尚未部署 |
| 7.3-rc2 登录画面、屏幕/触摸/网络等 | 重启后的具体功能仍待真机记录 |

当前目录续建入口：

```sh
cd /home/ubuntu/oneplus-negroni-arch-linux
./build.sh 7.3-rc2
```

`build.sh 7.3-rc2` 在已有 `linux/` 仓库中获取该分支并切换本地分支，不负责首次 clone 或创建凭据，要求前述输入已就绪。产物和日志按分支目录保存，它按下列标记跳过完成阶段：

| 标记 | 含义 |
| --- | --- |
| `logs/7.3-rc2/bootstrap.extracted` | 全新安装时记录 ARM 引导环境已解包；本次复用原环境及软件包阶段 |
| `logs/7.3-rc2/kernel.complete` | 内核和模块安装成功 |
| `logs/7.3-rc2/rootfs.packages-complete` | 最终 rootfs 软件包和服务安装完成 |
| `logs/7.3-rc2/rootfs.complete` | 账号、桌面、内核及 initramfs 配置完成 |
| `logs/7.3-rc2/ssh.complete` | SSH 已安装、配置、启用并通过构建检查 |
| `logs/7.3-rc2/gpu-runtime-pm.complete` | GPU runtime PM 规避服务已安装、启用并通过构建检查 |
| `logs/7.3-rc2/common-tools.list` | 常用软件已安装并通过检查；与配置清单不一致时重新执行该阶段 |
| `logs/7.3-rc2/images.complete` | 打包脚本及镜像比对全部成功 |

标记仅表示某次完成，不会全面检测源码、配置、包版本或产物是否随后变化。若修改内核配置、rootfs 或打包参数，必须重新执行受影响步骤和校验；从头重建建议使用新目录。打包阶段调用 `write-build-report.py` 更新 `logs/7.3-rc2/build-result.json`，同时重建 SHA256SUMS；原始 `logs/build-inputs.json` 和本文内嵌的大小/哈希仍需在构建输入或产物变化后同步更新。

## 产物与分区对应关系

当前最终文件位于 `images/7.3-rc2`（本地文件）。下表对应 2026-09-16 00:50 完成打包的本地镜像，包含键盘修复、默认 SSH、GPU runtime PM 规避和常用软件。GPU 规避服务已直接启用到手机；其余本地镜像更新尚未整体刷入。手机最近刷入版本的校验清单保存在 `logs/flash-7.3-rc2-ssh-2026-09-15-2228`（本地文件），添加软件前的构建结果和包清单保存在 `logs/7.3-rc2/pre-common-tools`（本地文件）；首次未含 SSH 的镜像记录保存在 `logs/7.3-rc2/pre-ssh`（本地文件）。最近刷机直接写入 `boot_a`，其余分区名称见日志；再次刷机仍需核对设备和槽位。

| 产物 | 精确大小（字节） | 写入分区 / 用途 |
| --- | ---: | --- |
| `images/7.3-rc2/boot.img`（本地文件） | 29,532,160 | `boot`：内核、initramfs、根分区命令行 |
| `images/7.3-rc2/vendor_boot.img`（本地文件） | 180,224 | `vendor_boot`：OnePlus 10 Pro 主线 DTB，无 vendor ramdisk |
| `images/7.3-rc2/recovery.img`（本地文件） | 18,690,048 | `recovery`：仅包含 ramdisk；按教程依赖 boot 分区中的内核和命令行 |
| `images/7.3-rc2/vbmeta.img`（本地文件） | 256 | 同一个文件分别写入 `vbmeta`、`vbmeta_vendor`、`vbmeta_system`，Flags=2 |
| `images/7.3-rc2/sparse_root.img`（本地文件） | 3,761,316,300 | `userdata`：用于 fastboot 的 Android sparse 根文件系统 |
| `images/7.3-rc2/root.img`（本地文件） | 5,368,709,120 | 原始 ext4 镜像；用于检查、修改、重新生成 sparse，按本文刷机时无需额外写入它 |
| `images/7.3-rc2/initramfs-linux.img`（本地文件） | 18,682,974 | 中间产物，已封装进 boot/recovery；不单独刷入任何分区 |
| `images/7.3-rc2/SHA256SUMS`（本地文件） | — | 最终校验值，刷机前核对 |

**sparse 文件约 3.50 GiB，但展开后的 rootfs 是 5 GiB。** `userdata` 至少需要容纳 5,368,709,120 字节；不能按 sparse 文件大小判断是否装得下。当前 ext4 文件系统本身也只有 5 GiB，刷到更大分区不会自动扩容；先完成启动验证，再考虑备份后的文件系统扩容。

本项目没有 `dtbo.img`、`abl.img`、原厂固件或设备分区备份。教程对 `dtbo` 的操作是擦除，不能把 DTB 或 `vendor_boot.img` 当成 dtbo 镜像。固件加载服务会读取设备保留的 modem/bluetooth/persist 等分区，本次产物不替代这些原厂内容。不要根据本表去清空 `super`、`modem`、`persist`、`abl` 或其他未列为写入目标的分区。

当前本地镜像完整 SHA-256（包含常用软件和 GPU runtime PM 配置）：

```text
6b592077999d2da15d3b40a2b155d285f427a5dad8e81e98c9b5837a58b330b8  boot.img
5a1d42ca81fd839b769baeba89ecac7feef1559fec1f11ba332cb6599c9fdce0  vendor_boot.img
ee36c0400eb24d8c7862da44f2b5c2aac0cfb1873a9cf9ea126b4811047ad797  recovery.img
8b282508cd8bb372194cd577f7404e095a2a4f204136ac4adb31325db88ebdc7  vbmeta.img
c353c644a5ddc155825a117d31d9cb6e528e37e287ba1b9c1ec02f1077fe0d82  root.img
2d97bf1fa2adf0c47981eca77fb130d2ea01de6d089440af57343abeafdc677c  sparse_root.img
3792f99b715f4bf175eab5d4431d9c7e5dd457c3ff46f6ffbe1c02b78a02603e  initramfs-linux.img
```

## 刷机前必须核对的条件

以下是本次刷机已确认的条件和仍需保留的约束。7.3-rc2 已完成实际写入；后续换设备、换固件或再次刷机时，应重新读取设备状态，不能直接沿用旧槽位。

| 项目 | 当前状态 / 要求 |
| --- | --- |
| 设备身份 | NE2210；fastboot 序列号 `e2d3522f`，`product=taro` |
| OOS/COS 升级历史 | 用户确认原厂系统为 ColorOS 14，未升级过 ColorOS/OxygenOS 15；首次刷机前运行基于 7.2 的 Linux，本次刷机前为 7.3-rc2 |
| Bootloader 解锁 / 模式 | fastboot 返回 `unlocked=yes`、`is-userspace=no` |
| ABL / 固件组合 | 本次沿用原 7.3-rc2 系统所用设备固件，未刷写 ABL；ABL 具体版本未记录 |
| 槽位和分区 | 双槽，保持槽位 `a`；`has-slot:boot=yes`；其他 `has-slot` 查询不支持，实际逻辑分区写入全部成功 |
| 分区容量 | `boot_a` 为 192 MiB；userdata 为 496,241,799,168 字节，大于展开后的 5 GiB rootfs |
| 根分区映射 | 继续使用原镜像的 `root=/dev/sda14`；主线启动后的映射未另行实测 |
| 数据和恢复准备 | 本次用户明确要求不备份、直接刷入，已擦除并写入 userdata；未生成设备分区备份 |

### qbootctl 的已知硬性限制

Wiki 明确警告：**曾升级到 OOS/COS 15.0.0.1302 或更高版本的设备不能使用 qbootctl，存在变砖风险，降级也不能消除这个条件；该版本还会失去免费 EDL 恢复条件。** 参考 [上游 issue #4](https://github.com/linux-msm/qbootctl/issues/4) 和本地教程快照。

本镜像按教程安装了 `qbootctl 0.2.2-1`；已启用的 `msm-firmware-loader` 会调用 `qbootctl -a` 获取槽位。因此这不是可以忽略的工具安装提示：**若存在上述升级历史，或无法确认历史，不应直接执行下面的刷入/启动步骤。** 应先研究并验证适合该固件的启动与固件加载方案，调整镜像、重做校验；简单降级或只更改槽位不能证明问题已解决。

### Bootloader、ABL 与根分区

Wiki 的“无法进入 fastboot”分支包含回退 Android 12、解锁、再升级 OxygenOS/ColorOS 14，并通过 fastbootd 写入指定 ABL 的步骤。解锁会清除用户数据；回退包和 ABL 必须匹配机型，不能用本项目的镜像替代。本次没有验证你的手机是否需要这条路径，也没有下载教程链接的 ABL。请根据 [教程的启动模式说明](https://wiki.postmarketos.org/index.php?title=OnePlus_10_Pro_(oneplus-negroni)&oldid=106143#How_to_enter_flash_mode) 判断；不要为了“匹配教程”给已经合适的设备盲目回退或更换 bootloader。

教程说明 Android 12 ABL 从 `vendor_boot` 读取 DTB，从 `boot` 读取内核和 ramdisk，`recovery` 使用 `boot` 的内核和命令行；本次镜像即按该布局生成。`fastboot boot boot.img` 不会自动配置 vendor_boot、recovery 或 userdata，不能视为对整套镜像的完整验证；`recovery.img` 本身也没有内核。

在可用的 root recovery 中，可用 `readlink -f /dev/block/by-name/userdata`、`ls -l /dev/block/by-name/` 辅助核对存储布局。Android recovery 中的设备名与主线 Linux 的枚举仍需结合本机 UFS/LUN 布局判断。若确定实际根设备并非 `/dev/sda14`，先修改打包命令中的 `root=` 并重新生成 boot、校验值和记录，不能直接使用当前 boot.img。

## 后续刷机操作

2026-09-15 22:29–22:33 已按此分区顺序重新刷入包含键盘修复和默认 SSH 的 7.3-rc2 并重启，实际执行记录见 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/flash.log`（本地文件）。按用户要求不备份，已擦除并写入 userdata。下面的命令供后续重新刷机使用；`images/7.3-rc2/` 现已更新为包含常用软件和 GPU runtime PM 配置的本地镜像。GPU 服务已直接部署到手机，其余软件更新尚未刷入。`erase userdata` 会清除手机当前 Linux/Android 用户数据和系统根文件系统。

### 1. 核对本地文件

```sh
cd /home/ubuntu/oneplus-negroni-arch-linux
(cd images/7.3-rc2 && sha256sum -c SHA256SUMS)
```

必须全部为 `OK`。镜像若被修改、截断或重新生成，不能继续依赖本文记录的旧校验值。

### 2. 识别手机、记录状态和数据处理要求

本机已有 adb/fastboot。若换一台 Ubuntu 主机，可安装 `adb fastboot`。当前手机已刷入 Linux；若已进入 fastboot，直接进行下面的 fastboot 查询。只有设备仍运行 Android 且允许 USB 调试时，才使用以下 Android 身份查询：

```sh
adb devices -l
# 把值替换为上一步目标手机的 adb 序列号。
ADB_SERIAL='填写目标手机的 adb 序列号'
adb -s "$ADB_SERIAL" shell getprop ro.product.model
adb -s "$ADB_SERIAL" shell getprop ro.product.device
adb -s "$ADB_SERIAL" shell getprop ro.build.display.id
adb -s "$ADB_SERIAL" shell getprop ro.build.version.incremental
```

这些属性只能证明当前软件状态，不能证明手机从未升级到受 qbootctl 限制的版本。升级历史需要另外确认。

本次已按用户要求跳过备份。后续如需保留数据，可先备份照片、文件、账号等个人数据，并准备匹配具体 NE221x 型号的恢复材料。对于会覆盖的分区，可在能读取原始分区的 root recovery 中保存 `boot`、`vendor_boot`、`recovery`、`dtbo`、三类 `vbmeta`；若分槽应分别记录两槽。分区名和大小以设备实际结果为准，不应假定消费级 bootloader 支持 `fastboot fetch`。

下面仅演示在 **已核实 root recovery** 中读取一个确实存在的 `boot_a` 分区；其余分区按实际名称分别备份。普通 Android shell 无权限时，此命令不能代替进入合适的恢复环境：

```sh
adb -s "$ADB_SERIAL" shell id
adb -s "$ADB_SERIAL" shell ls -l /dev/block/by-name/boot_a
adb -s "$ADB_SERIAL" shell blockdev --getsize64 /dev/block/by-name/boot_a
mkdir -p backups
# 输出保存为 partial，成功并核对字节数后才改名为正式备份。
adb -s "$ADB_SERIAL" exec-out dd if=/dev/block/by-name/boot_a bs=1048576 \
  > backups/boot_a.img.partial
stat -c '%s' backups/boot_a.img.partial
```

确认 `dd` 成功、文件大小与设备查询一致后，再改名并生成备份哈希。备份目录应另存到可靠位置；本次构建没有生成任何手机备份。原始 userdata 的副本也不能替代解密后的个人文件备份。

按数据处理要求准备后进入 bootloader fastboot 模式；设备已经在此模式时无需执行 adb 重启，以下 adb 命令也不适用于未启用 ADB 的 Linux 系统：

```sh
adb -s "$ADB_SERIAL" reboot bootloader
fastboot devices -l
```

随后把目标设备的 fastboot 序列号填入变量，**之后所有命令均显式指定这个序列号**：

```sh
DEVICE_SERIAL='填写目标手机的 fastboot 序列号'
fastboot -s "$DEVICE_SERIAL" getvar product
fastboot -s "$DEVICE_SERIAL" getvar unlocked
fastboot -s "$DEVICE_SERIAL" getvar is-userspace
fastboot -s "$DEVICE_SERIAL" getvar current-slot
fastboot -s "$DEVICE_SERIAL" getvar slot-count

for partition in boot vendor_boot recovery dtbo vbmeta vbmeta_vendor vbmeta_system userdata; do
  fastboot -s "$DEVICE_SERIAL" getvar "has-slot:$partition"
done
```

fastboot 通常把查询结果写到 stderr，这本身不表示失败。某个变量若提示不支持，不能将其当成已通过：用 bootloader 屏幕或设备支持的查询方式补齐证据。`is-userspace=yes` 表示 fastbootd；本文主镜像步骤按 bootloader fastboot 准备，若设备仍在 fastbootd，应先用 `fastboot -s "$DEVICE_SERIAL" reboot bootloader` 切换并重新核对。

结合 `has-slot` 和当前槽位，查询实际分区大小。例如仅在已确认当前槽位为 `a` 且相应分区分槽时，查询 `partition-size:boot_a`、`partition-size:vendor_boot_a` 等；不分槽的使用原名。另查询：

```sh
fastboot -s "$DEVICE_SERIAL" getvar partition-size:userdata
```

返回值可能为十六进制。各启动分区必须能容纳对应文件，userdata 必须能容纳展开后的 5 GiB rootfs。把手机序列号、型号、固件历史、解锁状态、fastboot 模式、当前槽位、分区名和大小保存到刷机记录，再进入下一步。

### 3. 按教程顺序刷入

**先读完整段，再执行。以下为 Bash 命令，擦除与刷入会修改手机。** 本流程采用 Wiki 的分区逻辑名并保持当前活动槽位；fastboot 对分槽分区默认操作当前槽位，对未分槽分区使用原名。执行前必须确认这些名称在目标设备上存在且解析到上一节记录的目标，不应把“分区不存在”当成可以跳过的提示。

将两个占位符替换为前面已经核实的值。槽位检查通过后，流程不执行 `set_active`，也不使用 `--slot all`。每个 fastboot 命令必须返回成功；`set -e` 会在命令失败时退出当前 Bash 执行段，保留日志并排查后再继续。

```bash
set -euo pipefail
cd /home/ubuntu/oneplus-negroni-arch-linux
DEVICE_SERIAL='填写已核实的 fastboot 序列号'
EXPECTED_SLOT='填写已核实的当前槽位 a 或 b'
case "$EXPECTED_SLOT" in a|b) ;; *) echo '必须先填写已核实的槽位'; exit 1 ;; esac

(cd images/7.3-rc2 && sha256sum -c SHA256SUMS)
slot_report=$(fastboot -s "$DEVICE_SERIAL" getvar current-slot 2>&1)
printf '%s\n' "$slot_report"
actual_slot=$(printf '%s\n' "$slot_report" | sed -nE 's/.*current-slot:[[:space:]]*([ab])[[:space:]]*$/\1/p')
[[ "$actual_slot" == "$EXPECTED_SLOT" ]] || { echo '当前槽位与记录不一致，停止'; exit 1; }

# 主线 DTB 已在 vendor_boot 中；按 Wiki 擦除相应 dtbo。
fastboot -s "$DEVICE_SERIAL" erase dtbo

# 同一个 vbmeta.img 分别写入三个 AVB 元数据分区。
fastboot -s "$DEVICE_SERIAL" flash vbmeta images/7.3-rc2/vbmeta.img
fastboot -s "$DEVICE_SERIAL" flash vbmeta_vendor images/7.3-rc2/vbmeta.img
fastboot -s "$DEVICE_SERIAL" flash vbmeta_system images/7.3-rc2/vbmeta.img

# 三个相互配套的主线启动镜像。
fastboot -s "$DEVICE_SERIAL" flash vendor_boot images/7.3-rc2/vendor_boot.img
fastboot -s "$DEVICE_SERIAL" flash recovery images/7.3-rc2/recovery.img
fastboot -s "$DEVICE_SERIAL" flash boot images/7.3-rc2/boot.img

# 此处清空当前 userdata（包括已有 Linux rootfs），然后写入新根文件系统。
fastboot -s "$DEVICE_SERIAL" erase userdata
fastboot -s "$DEVICE_SERIAL" flash userdata images/7.3-rc2/sparse_root.img
```

所有步骤成功后，保留终端输出，确认目标仍是同一设备、同一槽位，再单独重启：

```sh
fastboot -s "$DEVICE_SERIAL" getvar current-slot
fastboot -s "$DEVICE_SERIAL" reboot
```

注意以下与本套镜像直接相关的约束：

- 刷入的 rootfs 文件是 `sparse_root.img`，不是 `root.img` 和 sparse 各刷一次。
- 写入 rootfs 后不要执行 `fastboot -w`、`format userdata` 或再次 `erase userdata`，否则会销毁刚写入的 Linux 系统。
- 需要保留 AVB Flags=2 和已解锁状态；不要在安装本套自定义镜像后重新锁定 bootloader。
- 本流程没有写入另一槽位的启动镜像；userdata 通常是共享分区，不能因为保留了另一槽位就认为 Android 用户数据还在，也不能把切槽当成已验证的恢复方式。
- 若 USB 传输失败、分区大小不足、分区名不存在或某一步写入失败，应停在 fastboot 排查；不要在启动分区不完整时直接重启或盲目切换槽位。

### 4. 首次启动和登录

预期进入 SDDM 登录界面，默认选择 Plasma Mobile 会话。账号是 `oneplus`；密码从本地 `image-credentials.json`（本地文件） 的 `password` 字段读取，root 密码位于 `root_password`。README 不包含密码，分享构建说明时不应一并公开该凭据文件。

进入桌面后，在 QMLKonsole 等本机终端检查并记录：

```sh
uname -r
findmnt /
lsblk -f
systemctl --failed
systemctl status sddm NetworkManager ModemManager msm-firmware-loader rmtfs tqftpserv gpu-runtime-active
cat /sys/bus/platform/devices/3d00000.gpu/power/control
journalctl -b -p warning
```

预期内核为 `7.3.0-rc2-sm8450+`，根文件系统为本次写入的 ext4。含 SSH 的 7.3-rc2 已刷入并执行重启，用户已确认默认 SSH 生效；显示、触摸、Wi-Fi、蓝牙、充电和基带等逐项结果仍待记录。后续启动验证可点击 SDDM 密码框检查 Plasma Keyboard 能否弹出，联网后检查 `systemctl status sshd` 并从电脑登录。常用软件已在本地镜像内验证，尚无该更新的真机测试记录。Wiki 本身也标明音频、相机、GPS 等仍有未实现功能，不能将刷写成功视为所有硬件可用。

若无法启动，优先记录屏幕现象、fastboot 槽位和每一步刷入输出，核对 `vendor_boot`/DTB、recovery、boot、根设备映射、ABL 及固件历史。根分区找不到与桌面服务失败是不同问题，不应都通过反复擦除 userdata 处理。恢复原厂应使用预先准备且匹配本机的完整恢复方案，本项目不含可直接恢复 Android 的镜像。

## 构建与刷机记录索引

| 文件 / 目录 | 内容 |
| --- | --- |
| `logs/build-inputs.json`（本地文件） | 原始 7.2 构建的输入记录；当前分支、提交以 7.3-rc2 结果为准 |
| `logs/7.3-rc2/build-result.json`（本地文件） | 当前 7.3-rc2（含 SSH、GPU runtime PM 和常用软件）构建时间、验证状态及产物大小/哈希 |
| `logs/7.3-rc2/kernel-build.log`（本地文件） | 内核、DTB 和模块编译日志 |
| `logs/7.3-rc2/kernel.config`（本地文件） | 实际使用的完整内核配置 |
| `logs/7.3-rc2/kernel-release.txt`（本地文件） | 当前内核版本 |
| `logs/7.3-rc2/kernel-commit.txt`（本地文件） | 当前构建使用的源码提交 |
| `logs/rootfs-build.log`（本地文件） | chroot 初始错误、修复后的系统和桌面安装记录 |
| `logs/7.3-rc2/rootfs-finalize.log`（本地文件） | 账号配置及 initramfs 生成结果 |
| `logs/7.3-rc2/rootfs-packages.txt`（本地文件） | 当前 639 个包的精确版本，包含 OpenSSH 和常用软件 |
| `logs/7.3-rc2/rootfs-enabled-services.txt`（本地文件） | 已启用的系统服务 |
| `logs/7.3-rc2/initramfs-contents.txt`（本地文件） | initramfs 文件及模块清单 |
| `logs/7.3-rc2/image-pack.log`（本地文件） | ext4 检查、sparse 还原校验和 AVB 信息 |
| `logs/7.3-rc2/boot-header.txt`（本地文件） | boot 镜像头与命令行 |
| `logs/7.3-rc2/vendor-boot-header.txt`（本地文件） | vendor_boot 镜像头和 DTB 大小 |
| `logs/7.3-rc2/recovery-header.txt`（本地文件） | recovery 镜像头 |
| `logs/7.3-rc2/sparse-roundtrip.json`（本地文件） | sparse 展开后与 root.img 的 SHA-256 一致性 |
| `images/7.3-rc2/SHA256SUMS`（本地文件） | 交付镜像校验清单 |
| [keyboard-fix/10-mobile.conf](../../config/sddm/10-mobile.conf) | 登录界面虚拟键盘修复配置副本 |
| [config/ssh/00-negroni.conf](../../config/ssh/00-negroni.conf) | 默认 SSH 配置源文件 |
| [config/systemd/gpu-runtime-active.service](../../config/systemd/gpu-runtime-active.service) | A730 GPU runtime PM 规避 service 源文件 |
| `logs/7.3-rc2/gpu-runtime-pm-config.txt`（本地文件） | GPU runtime PM 策略、sysfs 节点和启用服务记录 |
| `logs/7.3-rc2/gpu-runtime-pm-install.log`（本地文件） | 已有镜像应用该配置的过程 |
| [artifacts/wake-latency-20260916/](../../artifacts/wake-latency-20260916/) | 慢亮屏诊断、GPU 保持活跃后的 DPMS 周期和内核日志 |
| `logs/7.3-rc2/ssh-install.log`（本地文件） | SSH 安装与配置记录 |
| `logs/7.3-rc2/sshd-effective.conf`（本地文件） | SSH 实际生效配置 |
| `logs/7.3-rc2/ssh-verification.json`（本地文件） | SSH 离线验证及 QEMU 登录测试限制 |
| [config/packages/common-tools.txt](../../config/packages/common-tools.txt) | 19 项默认常用软件清单 |
| [config/pacman/mirrorlist](../../config/pacman/mirrorlist) | 构建环境及镜像使用的 Arch Linux ARM 软件源 |
| `logs/7.3-rc2/common-tools-install.log`（本地文件） | 常用软件安装及检查输出 |
| `logs/7.3-rc2/common-tools-versions.txt`（本地文件） | 19 项常用软件的精确版本 |
| `logs/7.3-rc2/common-tools-verification.json`（本地文件） | ARM64/QEMU 功能检查结果 |
| `logs/7.3-rc2/pre-common-tools`（本地文件） | 添加常用软件前的构建结果、包清单和镜像哈希 |
| `logs/7.3-rc2/pre-ssh/build-result.json`（本地文件） | 首次刷入时的 7.3-rc2 构建结果（含键盘修复，未加入 SSH） |
| `logs/7.3-rc2/pre-ssh/SHA256SUMS`（本地文件） | 首次未含 SSH 镜像的校验清单 |
| `logs/flash-7.3-rc2-2026-09-15.log`（本地文件） | 首次刷入未含 SSH 版本的实际输出 |
| `logs/flash-7.3-rc2-2026-09-15.json`（本地文件） | 首次刷机完成结果 |
| `logs/flash-7.3-rc2-ssh-2026-09-15-2228/flash.log`（本地文件） | 含 SSH 镜像的全部擦除、写入和重启输出 |
| `logs/flash-7.3-rc2-ssh-2026-09-15-2228/result.json`（本地文件） | 设备、槽位、每一步状态及待验证项 |
| `logs/flash-7.3-rc2-ssh-2026-09-15-2228/preflight.log`（本地文件） | 解锁状态、槽位、分区大小核对 |
| `logs/flash-7.3-rc2-ssh-2026-09-15-2228/SHA256SUMS`（本地文件） | 已刷入的键盘修复、默认 SSH 镜像哈希 |
| `logs/flash-7.3-rc2-ssh-2026-09-15-2228/ssh-hardware-verification.json`（本地文件） | 用户确认最近刷入版本的默认 SSH 生效 |

本次已记录设备身份、解锁和槽位、无需备份的用户要求、镜像哈希、刷写/重启结果以及用户对默认 SSH 生效的确认。GPU runtime PM 规避服务已直接部署并验证；包含该服务和常用软件的完整本地镜像已重新打包，但尚未整体刷写。后续继续补充 7.3-rc2 登录画面、键盘输入和硬件功能结果。每次刷机的日志和校验清单独立保留。
