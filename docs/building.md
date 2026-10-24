# 构建指南

本指南针对 OnePlus 10 Pro NE2210 的 `7.3` 配置。已使用的构建环境为 Ubuntu 26.04 x86_64、8 个逻辑 CPU、约 14 GiB 内存；其他环境尚未验证。预留足够空间存放 Linux 源码及输出、ARM chroot、包缓存和多个镜像，其中原始 rootfs 本身为 5 GiB。

构建流程需要网络，以及用于 loop 挂载、私有挂载命名空间和 chroot 的 sudo 权限。ARM64 用户空间通过 QEMU/binfmt 执行。所有命令从项目根目录执行。

## 1. 安装宿主依赖

```sh
sudo apt-get update
sudo apt-get install -y \
  git curl ca-certificates python3 build-essential bc cpio kmod rsync zstd xz-utils \
  e2fsprogs util-linux file gcc-aarch64-linux-gnu flex bison libssl-dev libelf-dev \
  device-tree-compiler pahole qemu-user qemu-user-binfmt binfmt-support \
  mkbootimg android-sdk-libsparse-utils
```

刷机主机另需 `adb` 和 `fastboot`。完整构建主机不能是禁止 mount/unshare 的受限容器。

## 2. 初始化源码

```sh
git clone https://github.com/withsalt/oneplus-negroni-arch-linux.git
cd oneplus-negroni-arch-linux
bash scripts/init-submodules.sh
git submodule status
```

已经克隆时，从初始化脚本开始即可。脚本按父仓库固定提交恢复 `linux/`、`tools/avb/` 和 `tools/msm-firmware-loader/`；基于上游 `7.3` 重做的设备修复及独立保留的历史 rc2 提交通过 [Git bundle](../patches/linux/README.md) 恢复。若 `linux/` 有未提交改动，脚本会停止，需先保存改动。

预期内核 HEAD 为 `3ca85b51d20fc9287aec159998e6b871887bb839`，它直接基于上游 `7.3` 修改，未合并 `7.3-rc2`。不要使用 `git submodule update --remote` 替代固定版本初始化。

## 3. 下载 rootfs

```sh
mkdir -p downloads logs/7.3 images/7.3
curl -L --fail --retry 3 -C - \
  -o downloads/ArchLinuxARM-aarch64-latest.tar.gz \
  https://ca.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz
curl -L --fail \
  -o downloads/ArchLinuxARM-aarch64-latest.tar.gz.md5 \
  https://ca.us.mirror.archlinuxarm.org/os/ArchLinuxARM-aarch64-latest.tar.gz.md5
(cd downloads && md5sum -c ArchLinuxARM-aarch64-latest.tar.gz.md5)
sha256sum downloads/ArchLinuxARM-aarch64-latest.tar.gz > logs/rootfs-input.sha256
```

官方 `latest` 归档和 Arch/XEC 软件仓库会变化，校验失败时先核对归档和校验文件是否对应同一版本。源码提交固定不代表软件包输入固定；需要复现某次结果时，应同时保留归档、包缓存、版本清单与校验值。历史输入记录见[构建归档](archive/build-notes-2026-09-16.md#参考资料与固定输入)。

## 4. 创建本地登录凭据

在全新工作区执行一次，生成权限为 `0600` 的 `image-credentials.json`。命令拒绝覆盖已有文件，也不会输出密码：

```sh
python3 - <<'PY'
import json
import os
import secrets
import string

credentials = {
    "username": "oneplus",
    "password": "".join(secrets.choice(string.digits) for _ in range(10)),
    "root_password": secrets.token_urlsafe(15),
}
fd = os.open("image-credentials.json", os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(fd, "w") as stream:
    json.dump(credentials, stream, indent=2)
    stream.write("\n")
PY
```

当前账号创建、SSH 和桌面配置均使用 `oneplus`，请保留这个用户名。密码在 rootfs 配置阶段写入镜像；仅修改 JSON 不会改变已完成镜像或手机上的密码。该文件已被 Git 忽略，请单独妥善保存。

## 5. 构建与续建

```sh
./build.sh 7.3
```

需要限制主编译阶段的并行度时，可使用 `JOBS=4 ./build.sh 7.3`。构建入口会获取分支并切换到已有本地分支，**不会自动将已有分支重置到远端最新提交**；构建前可用 `git -C linux rev-parse HEAD` 核对源码。

构建依次完成：

1. 首次生成 `defconfig` 与 `sm8450.config`，每次构建合并 `config/kernel/negroni.config` 和 `config/kernel/docker.config` 并解析依赖，再编译内核、DTB 和模块；强制核对 Docker 配置、模块及自动加载别名和 8 核能力、功耗参数。
2. 创建 `alarm/` 构建 chroot 与 `images/7.3/root.img`，安装系统和 Plasma Mobile。
3. 配置账号、SDDM、SSH、GPU 服务、默认启用的 CPU 性能服务、屏幕默认设置、默认关闭的充电限制和 initramfs。
4. 更新有变化的内核产物、CPU / 充电服务、屏幕默认设置和常用工具。
5. 核对镜像内 Docker 模块及依赖索引，封装 Android 镜像，检查 ext4、sparse 往返、启动镜像内容并生成 SHA-256 与构建报告。

中断后可重新执行同一命令。各阶段使用 `logs/7.3/` 下的完成标记；每次仍会运行增量 Kbuild，内核、CPU / 充电配置或屏幕默认设置更新会使镜像封装失效并重新生成。SSH、GPU、SDDM 等配置并非全部通过内容指纹自动更新，修改后应按[集成说明](build-integration.md)处理，或在全新工作区构建。

镜像和日志按分支分目录，**`linux/output/` 与 `alarm/` 仍共享**。切换其他内核分支请使用独立克隆与构建输出；不要同时运行多个构建，也不要将删除完成标记当成无条件安全的重建方式。无参数 `./build.sh` 使用 `images/` 和 `logs/`，建议始终明确传入 `7.3`。

## 6. 检查产物

```sh
(cd images/7.3 && sha256sum -c SHA256SUMS)
python3 -m json.tool logs/7.3/build-result.json
```

| 本地文件 | 用途 |
| --- | --- |
| `images/7.3/boot.img` | 内核、initramfs 和根设备命令行 |
| `images/7.3/vendor_boot.img` | 设备树 |
| `images/7.3/recovery.img` | recovery ramdisk |
| `images/7.3/vbmeta.img` | AVB 元数据，Flags=2 |
| `images/7.3/sparse_root.img` | 写入 userdata 的 sparse 根文件系统 |
| `images/7.3/root.img` | 5 GiB 原始 ext4 根文件系统 |
| `logs/7.3/build-result.json` | 内核提交、配置、验证结果与镜像大小/哈希 |

脚本不会自动刷写手机。构建和离线检查通过后，继续阅读[刷机说明](flashing.md)。CPU Boost / performance 与大小核调度修复默认随新固件生效，启停及旧镜像续建见 [CPU 性能说明](cpu-performance.md)。
