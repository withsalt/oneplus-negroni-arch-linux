# 屏幕与休眠默认设置

固件为 Plasma Mobile 预置以下默认值，插电（`AC`）、电池（`Battery`）和低电量（`LowBattery`）三种模式一致：

| 设置项 | 默认值 |
| --- | --- |
| 降低亮度时间 | 关闭 |
| 关闭屏幕时间 | 5 分钟 |
| 休眠时间 | 关闭 |

配置源文件为 [config/powerdevil/powerdevilrc](../config/powerdevil/powerdevilrc)，安装到 rootfs 的 `/etc/xdg/powerdevilrc`。这是可由用户覆盖的系统默认值；首次使用新镜像时生效，之后可在设置界面修改。已有 `~/.config/powerdevilrc` 中的对应设置优先，构建脚本不会改写用户配置。

## 配置与适用范围

当前镜像使用 PowerDevil 6.7.5，配置采用 Plasma 6 的分组和秒数单位：

- 各模式的 `[Display]` 分组设置 `DimDisplayWhenIdle=false`、`DimDisplayIdleTimeoutSec=0`，关闭空闲调暗。
- 同一分组设置 `TurnOffDisplayWhenIdle=true`、`TurnOffDisplayIdleTimeoutSec=300`，空闲 5 分钟后熄屏。
- 各模式的 `[SuspendAndShutdown]` 分组设置 `AutoSuspendIdleTimeoutSec=0`，关闭空闲自动休眠。保留休眠动作的原有默认值，使用户以后在手机界面选回休眠时间时仍可启用。

锁屏后的独立熄屏时间沿用上游默认值，PowerDevil 6.7.5 为 60 秒，配置键为 `TurnOffDisplayIdleTimeoutWhenLockedSec`。本配置对应设置界面的三个空闲计时选项，不改变手动电源键、自动锁屏、低电量临界保护或低电量模式的亮度策略。

配置依据：[移动端设置界面](https://github.com/KDE/powerdevil/blob/v6.7.5/kcm_mobile/mobilepower.cpp)、[配置定义](https://github.com/KDE/powerdevil/blob/v6.7.5/PowerDevilProfileSettings.kcfg)、[自动休眠实现](https://github.com/KDE/powerdevil/blob/v6.7.5/daemon/actions/bundled/suspendsession.cpp)。其中移动端界面把休眠时间 `0` 显示为“关闭 / Never”，守护进程也不会为该值注册空闲休眠计时器。

## 全新构建与续建

`finalize-rootfs.sh` 调用 `configure-screen-defaults.sh` 安装默认配置。`build.sh` 通过 `screen-defaults-sources.sha256` 检测配置及安装、更新、校验脚本的变化；缺少记录或内容改变时，自动更新已有 `root.img`，使 `images.complete` 失效并重新封装。安装仅写入系统默认配置，无需重新编译内核或执行 ARM 程序。

已有完整镜像也可单独更新并重新打包：

```sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/update-screen-defaults-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

`pack-images.sh` 必须通过 `verify-screen-defaults-image.py` 的离线检查：直接读取 ext4 镜像中的配置，核对源文件一致性、root 所有权和 `0644` 权限，并记录三种模式的配置值。缺失或不匹配时停止打包。验证写入 `screen-defaults-verification.json`，并纳入 `build-result.json`。

配置随 `root.img` / `sparse_root.img` 交付，仅更新 `boot.img` 不会带入。离线校验不代表真机上的计时与设置界面已验证；实际刷机请按[刷机说明](flashing.md)操作。

2026-09-16 已更新并重新封装本地 `images/7.3-rc2/` 镜像，ext4 检查、配置内容/权限校验、Android sparse 还原校验及启动镜像检查通过。使用镜像内的 ARM64 `kreadconfig6` 和依赖库，在 QEMU 下读到了三种模式的全部 15 项预期值，并验证用户配置覆盖及系统默认值回退；记录位于 `logs/7.3-rc2/screen-defaults-work/kconfig-verification.json`。本次因现有 sudo 认证助手认证失败，使用 `debugfs` 修改未挂载的镜像副本，验证后替换；原始 root 镜像和报告保存在 `logs/7.3-rc2/pre-screen-defaults/`。镜像尚未刷入手机，常规构建流程仍使用上述挂载安装脚本。
