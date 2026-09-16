# 构建集成说明

首次构建请先读[构建指南](building.md)。下文解释脚本接入方式；带日期的实机与镜像结果为历史记录，本地 `logs/` 不随 Git 分发。

充电限制已同时接入全新 rootfs 与已有镜像续建：`prepare-rootfs.sh` 显式安装 Python，`finalize-rootfs.sh` 安装服务与默认配置；`build.sh` 每次运行增量 Kbuild，并用内核产物指纹、服务源文件校验值判断是否调用 `update-kernel-image.sh`。更新会复制模块、内核并重建 initramfs，再使旧的封装完成标记失效。每次构建安装后服务均为 `disabled`，默认开始 75%、停止 80%。

2026-09-16 的产物统一在 `images/7.3-rc2/`，日志在 `logs/7.3-rc2/`，未刷入手机。`pack-images.sh` 会检查镜像中的驱动、配置、服务内容与默认关闭状态；构建报告记录内核提交及充电配置。内核改动已提交到本地 `7.3-rc2` 分支的 `2c092365fd5c`；配套服务、配置、构建脚本和文档已纳入本父仓库，`linux/` 作为固定提交的子模块，未推送提交另有 [Git bundle](../patches/linux/README.md) 备份。使用方式、实现边界和迁移源码说明见 [充电限制说明](charge-limit.md)。

`build.sh` 支持把内核分支和镜像产物隔离到对应目录。构建 `7.3-rc2`：

```sh
./build.sh 7.3-rc2
```

产物写入 `images/7.3-rc2/`，日志和校验记录写入 `logs/7.3-rc2/`。脚本通过 `IMAGE_DIR`、`LOG_DIR` 传递目录给各阶段；未指定分支时保留原来的 `images/`、`logs/` 默认路径。

最终 rootfs 的 `/etc/sddm.conf.d/10-mobile.conf` 从 [config/sddm/10-mobile.conf](../config/sddm/10-mobile.conf) 安装，使用 KWin 启动 Plasma Keyboard：清空 `InputMethod`，设置 `GreeterEnvironment=QT_WAYLAND_SHELL_INTEGRATION=layer-shell`，并在 `CompositorCommand` 加入 `--inputmethod /usr/bin/plasma-keyboard`。已有 `rootfs.complete` 的镜像不会自动重装 SDDM 配置；修改此文件后需单独更新镜像或使用全新工作区重建。

新镜像默认安装 OpenSSH 并启用 `sshd.service`，监听 TCP 22。使用现有 `oneplus` 账号和图形登录密码连接，也支持该账号的 SSH 公钥；禁用 root 远程登录和空密码。配置源文件为 `config/ssh/00-negroni.conf`，构建时安装到 `/etc/ssh/sshd_config.d/00-negroni.conf`。

手机连接 Wi-Fi 后，在同一局域网的电脑上执行：

```sh
ssh oneplus@手机IP
```

手机 IP 可在网络设置或路由器的 DHCP 列表中查询，也可在手机终端执行 `ip -4 addr`。SSH 随系统启动，不需要先登录图形桌面，但镜像没有预置 Wi-Fi 凭据，首次仍需连接网络。此配置使用局域网，不会把 USB 数据线自动变成 SSH 网络连接。

镜像不包含 SSH 主机私钥；首次启动由 Arch 的 `sshdgenkeys.service` 生成本机密钥，再启动 sshd。`scripts/configure-ssh.sh` 检查最终生效配置、账号、自启状态和镜像中不存在共享主机密钥。`scripts/verify-ssh-image.py` 还可在隔离网络中测试 ARM sshd；早期 QEMU 测试因不支持该版本 sshd 的 seccomp 沙箱，仅通过配置、监听和现有密码哈希检查。该限制保留在模拟测试记录中，镜像沙箱设置未因此修改；最近刷入版本的默认 SSH 已由用户在真机上确认生效。

对已有完整镜像，`build.sh` 检测到缺少 `ssh.complete` 时会调用 `scripts/enable-ssh-image.sh` 补装 OpenSSH，再重新封装镜像、生成 sparse 和 SHA256SUMS，无需重新编译内核。提权时显式传递 `IMAGE_DIR`、`LOG_DIR`，确保处理对应分支目录。也可以手动运行：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/enable-ssh-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

2026-09-15 22:33，已将包含键盘修复和默认 SSH 的镜像重新刷入手机并执行重启，见 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/result.json`（本地文件）。用户随后确认“ssh默认生效”，见 `logs/flash-7.3-rc2-ssh-2026-09-15-2228/ssh-hardware-verification.json`（本地文件）。该确认对应添加常用软件前的 617 包镜像。对于其他尚未启用 SSH 的旧系统，也可在手机联网后直接安装并启动 SSH：

```sh
sudo pacman -S --needed openssh
sudo systemctl enable --now sshd
```

以上两条命令使用 Arch 的默认 SSH 配置，已支持普通账号密码登录；完整镜像另外限定为 `oneplus` 账号并禁用 root 登录。

常用软件统一维护在 [config/packages/common-tools.txt](../config/packages/common-tools.txt)：`nano`、`zip`、`unzip`、`curl`、`wget`、`htop`、`rsync`、`git`、`less`、`file`、`tree`、`jq`、`tmux`、`ripgrep`、`fd`、`bash-completion`、`lsof`、`strace`、`which`。`build.sh` 在 SSH 配置之后、打包之前调用 [scripts/install-common-tools-image.sh](../scripts/install-common-tools-image.sh)，为最终 rootfs 安装软件并执行 ARM64/QEMU 检查。

完成后将清单写入 `logs/7.3-rc2/common-tools.list`；它与配置清单一致时续建跳过该阶段，清单改变时重新安装、验证并打包。从清单删除条目不会自动卸载软件。脚本使用现有仓库数据库执行 `pacman -S --needed`，不执行整套系统升级。构建环境和最终镜像均使用 [config/pacman/mirrorlist](../config/pacman/mirrorlist)，优先选择 `de3.mirror.archlinuxarm.org`，加拿大镜像站为备用，保留 TLS 和包签名校验。

已有镜像可手动更新：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/install-common-tools-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

2026-09-15 22:46，包含常用软件的本地镜像已重新打包至 `images/7.3-rc2/`，rootfs 共 639 个包。19 项常用软件全部存在，ZIP 压缩解压、rsync 复制、JSON 处理、文件搜索和 Git 初始化通过，见 `logs/7.3-rc2/common-tools-verification.json`（本地文件） 和 `logs/7.3-rc2/build-result.json`（本地文件）。本次新增 16 个工具包及 6 个依赖，已有包版本保持不变，只有 `root.img` 和 `sparse_root.img` 的哈希改变。**该软件更新尚未刷入手机，手机仍运行前述已确认 SSH 生效的版本。**
