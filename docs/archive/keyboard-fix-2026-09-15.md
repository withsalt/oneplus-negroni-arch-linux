# OnePlus 10 Pro 登录界面虚拟键盘修复说明

> 历史记录：保留当时的构建与设备状态；文中的本地文件不随 Git 分发。当前操作入口见[文档索引](../README.md)。

日期：2026-09-15。设备：OnePlus 10 Pro NE2210（oneplus-negroni）。

**修复结果：用户已确认虚拟键盘能够弹出并输入。修正已写入手机的 SDDM 配置，当前可直接登录，不需要再次刷入或立即重启。**

## 1. 故障现象与适用环境

手机刷入 Arch Linux ARM + Plasma Mobile 后，已能显示 SDDM 的 Breeze 登录界面，但点击密码框不会弹出键盘；点击左下角的“Virtual Keyboard／虚拟键盘”按钮也没有反应，导致无法输入密码。

本次环境：

| 项目 | 实际配置 |
| --- | --- |
| 系统 | Arch Linux ARM |
| 内核 | 7.2.0-sm8450+ |
| 桌面 | Plasma Mobile 6.7.5 |
| 登录管理器 | SDDM 0.21.0 |
| 登录主题 | Breeze |
| 登录界面显示服务 | Wayland，由 KWin 提供 |
| 已安装的键盘包 | plasma-keyboard 6.7.5、qt6-virtualkeyboard 6.11.2 |
| 设备管理通道 | 已解锁的 bootloader fastboot；当前 Linux 未配置 adb/SSH |

本说明针对上述配置；临时修复镜像是为这套已验证可启动的内核、DTB 和 rootfs 制作的，不是其他机型或其他系统的通用修复包。

## 2. 原因

原 `/etc/sddm.conf.d/10-mobile.conf` 内容为：

```ini
[General]
DisplayServer=wayland
InputMethod=qtvirtualkeyboard

[Theme]
Current=breeze

[Wayland]
CompositorCommand=kwin_wayland --drm --no-lockscreen --no-global-shortcuts --locale1
```

这组设置没有正确连接 Wayland 登录界面与虚拟键盘：

1. Breeze 在 Wayland 下加载 `VirtualKeyboard_wayland.qml`，通过 `KWinVirtualKeyboard` 控制由 KWin 管理的输入法。
2. KWin 启动命令没有 `--inputmethod` 参数，因此没有启动供登录界面使用的 Plasma Keyboard。
3. `InputMethod=qtvirtualkeyboard` 是不适合本次 KWin Wayland 配置的设置。ArchWiki 明确说明：这种配置应通过 KWin 的 `--inputmethod` 启动键盘，并避免在 SDDM 的 General 节设置该输入法。

镜像内已经安装所需键盘包，因此本次通过修改配置解决，没有重新安装软件包。

## 3. 修复后的配置

手机上的配置文件仍为 `/etc/sddm.conf.d/10-mobile.conf`，内容更新为：

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

其中，`InputMethod=` 清空此前的错误配置；`--inputmethod /usr/bin/plasma-keyboard` 让 KWin 启动虚拟键盘；`QT_WAYLAND_SHELL_INTEGRATION=layer-shell` 配置登录界面的 Wayland 层集成。

本地配置副本：[keyboard-fix/10-mobile.conf](../../config/sddm/10-mobile.conf)。

## 4. 本次实际修复过程

由于无法在登录界面输入密码，且系统没有 adb/SSH，本次使用 **fastboot 临时启动修复镜像**，在正常登录界面启动前更新配置。

### 4.1 制作修复镜像

构建脚本 `keyboard-fix/build_repair.py`（本地文件） 读取本次成功刷入的 `flash-prep/boot.img` 和 `flash-prep/initramfs-linux.img`，向 initramfs 增加以下内容：

| 新增项 | 用途 |
| --- | --- |
| `/etc/negroni-keyboard-repair.conf` | 修正后的 SDDM 配置 |
| `/usr/bin/negroni-keyboard-repair` | 备份旧配置并安装新配置的脚本 |
| `negroni-keyboard-repair.service` | 在根文件系统挂载后执行修复 |
| `initrd.target.wants` 中的服务链接 | 在本次临时启动期间启用服务 |

服务依赖 `sysroot.mount`，在 `initrd-cleanup.service` 和 `initrd-switch-root.target` 之前运行。修复脚本先确认目标存在 SDDM 配置、Plasma Keyboard 程序、Arch Linux ARM 标识和 `oneplus` 账号，再更新配置。

构建时保留原内核和启动命令行，并逐项比对原 initramfs 文件内容及元数据。生成的修复 boot 镜像为 Android header v4。

### 4.2 进入 Fastboot 并临时启动

用户在登录界面关机后，按住音量上、音量下和电源键进入 Fastboot，并保持 USB 连接。执行前重新确认：

```text
product: taro
unlocked: yes
is-userspace: no
current-slot: a
```

本次在 Windows PowerShell 中执行的临时启动命令等价于：

```powershell
& 'D:\Program Files\platform-tools\fastboot.exe' `
  -s e2d3522f boot `
  'D:\Temp\oneplus-negroni\keyboard-fix\repair-boot.img'
```

设备返回：

```text
Sending 'boot.img' (27724 KB)                      OKAY [  0.064s]
Booting                                            OKAY [  0.294s]
Finished. Total time: 0.373s
```

这是已完成操作的记录，当前无需重跑。`fastboot boot` 将镜像临时加载到内存运行，本次没有用 `fastboot flash` 覆盖 boot 分区。

### 4.3 持久修改配置

临时启动期间，修复脚本将 `/sysroot` 重新挂载为可写，保存原配置，再以 root:root、0644 权限原子替换目标配置，最后同步到存储。

手机上的文件位置：

| 文件 | 作用 |
| --- | --- |
| `/etc/sddm.conf.d/10-mobile.conf` | 当前生效的修正配置 |
| `/var/lib/negroni-keyboard-repair/10-mobile.conf.before` | 首次修复前的配置备份 |
| `/var/lib/negroni-keyboard-repair/applied.txt` | 修复应用时间，UTC |

之后继续正常启动，SDDM 读取新配置，KWin 启动 Plasma Keyboard。用户确认：“键盘出来了，可以输入”。

## 5. 验证结果与影响范围

| 检查项 | 结果 |
| --- | --- |
| 修复镜像的内核及命令行 | 与原 boot 一致 |
| 原 initramfs 文件内容及元数据 | 逐项比对一致 |
| 修复脚本 Shell 语法 | 检查通过 |
| fastboot 临时加载及启动 | 均返回 OKAY |
| 登录界面虚拟键盘 | 用户确认已弹出且可以输入 |
| 密码、登录认证、userdata 格式 | 本次未改变 |
| ABL、boot/recovery 等持久启动分区 | 本次修复未刷写 |
| 再次正常重启后的键盘表现 | 未另行实测；配置已写入持久 rootfs，正常重启会继续读取 |

修复镜像大小为 **28,389,376 字节**，SHA-256 为：

```text
c222afb30684b38994b2555605cf6d48700d71a13ddbfb012ae3e518dc8bec2e
```

依据：`keyboard-fix/verification.json`（本地文件）、`keyboard-fix/boot-log.json`（本地文件）。本次确认的是登录界面的键盘输入；登录后的桌面输入法、中文输入等属于后续功能验证范围。

## 6. 是否需要重启或重新刷入

**当前不需要。** 可以直接使用 `oneplus` 账号和原密码登录。密码仍保存在本地 `image-credentials.json`，本说明不复制密码。

临时的是修复镜像的启动方式；它对手机 SDDM 配置文件的修改已经持久保存。因此以后正常重启不需要再次加载修复镜像，也不需要将修复 boot 常驻刷入手机。

## 7. 后续重建与再次刷机

**更新：修复已接入 `build.sh`，并生成单独的 `images-ready/` 完整镜像集。** 新镜像首次启动即使用修正配置；当前只有产物时的重新打包命令及完整源码构建接入方式见 [构建集成说明](../build-integration.md)。下述旧镜像限制仍适用于原 `images/` 和 `flash-prep/`。

电脑上的 `images/root.img`、`images/sparse_root.img` 以及 `flash-prep/root.img`、`flash-prep/sparse_root.img` **尚未合入此次键盘修复**。它们与手机当前 rootfs 的键盘配置已有差异。

未来处理方式：

- 如果重刷这些旧 rootfs，需要重新应用本修复；只重启手机不会丢失修复。
- 如果重新构建，应将第 3 节配置写入新 rootfs 的 `/etc/sddm.conf.d/10-mobile.conf`，保持 root:root、0644，并确认 `plasma-keyboard` 及 Wayland 所需依赖存在。
- 修改本地 rootfs 后应重新生成 sparse 镜像、更新 SHA256SUMS，并进行文件系统检查和首次登录输入验证。本次仅修改 SDDM 配置，不要求重新编译内核。

原配置备份用于排查和对照。恢复它会重新引入本次已发现的键盘问题，当前没有执行回退。

## 8. 文件与参考资料

| 本地文件 | 内容 |
| --- | --- |
| [10-mobile.conf](../../config/sddm/10-mobile.conf) | 正确的 SDDM/KWin 配置 |
| `keyboard-fix/repair-keyboard.sh`（本地文件） | 写入配置和保存备份的脚本 |
| `keyboard-fix/negroni-keyboard-repair.service`（本地文件） | initramfs 修复服务 |
| `keyboard-fix/build_repair.py`（本地文件） | 临时修复镜像构建脚本 |
| `keyboard-fix/verification.json`（本地文件） | 构建校验和用户验证结果 |
| `keyboard-fix/boot-log.json`（本地文件） | 设备状态及临时启动输出 |

上游说明：[ArchWiki：SDDM 的 Wayland 虚拟键盘设置](https://wiki.archlinux.org/title/SDDM#Virtual_keyboards)。设备背景：[postmarketOS Wiki：OnePlus 10 Pro](https://wiki.postmarketos.org/wiki/OnePlus_10_Pro_%28oneplus-negroni%29)。
