# Docker 固件集成调研方案

调研日期：2026-09-16。状态：推荐的构建配置方案已实施，当前用法与验证范围见 [Docker 内核支持](docker.md)。下文保留实施前的调查结果与方案比较，其中“当前镜像”指调研时尚未修复的镜像。

## 结论

可以在本项目构建的 Arch Linux ARM 固件中修复 Docker 启动问题。所需功能已存在于固定版本 Linux 源码中，主要工作是启用内核配置、保证已有配置续建也会应用这些选项，并检查模块确实进入最终 rootfs。

推荐在父仓库维护 Docker 内核配置片段，使用现有增量内核构建和镜像更新流程生成新固件。现有 rootfs 可复用，不需要重新安装整个桌面系统。配置启用 IPv6 FIB 时需要重新构建匹配的内核和模块，不能把本次手机上的补充模块直接当作完整的新固件实现。

本方案以普通 Docker / Compose 的 bridge 网络为验收范围。Docker 软件是否预装、是否默认启动是独立的镜像产品设置；建议本次先保证固件安装 Docker 后即可正常运行。

## 当前项目的证据

| 检查对象 | 已确认的情况 | 对方案的影响 |
| --- | --- | --- |
| [README](../README.md)、[构建指南](building.md) | 固定内核为 `05e876f60a304a400bc1b44da2de3b155c2bc829`，使用 `7.3-rc2` 构建目录 | 可基于现有源码和缓存续建 |
| [build-kernel.sh](../scripts/build-kernel.sh) | 仅当 `linux/output/.config` 不存在时才应用 `defconfig` 和 `sm8450.config` | 只修改首次配置片段不能保证旧工作区更新 |
| 当前 `linux/output/.config` | 缺少 nftables compatibility、FIB 及 Xtables NAT target 配置 | 与本次 Docker 启动错误吻合 |
| 当前 `images/7.3-rc2/root.img` | 只读检查确认缺少五个已用于手机修复的模块，也没有 `dockerd`；镜像内核配置与正式构建配置一致 | 实机修复尚未进入待发布固件 |
| [kernel-artifacts-id.py](../scripts/kernel-artifacts-id.py) | 指纹覆盖 `.config`、Image.gz、System.map、DTB 和安装后的全部模块 | 新配置及模块会触发已有镜像增量更新 |
| [build.sh](../build.sh)、[update-kernel-image.sh](../scripts/update-kernel-image.sh) | 自动更新 rootfs 内核及模块、执行 depmod、重建 initramfs 并使封装标记失效 | 可以复用现有更新链路 |
| [pack-images.sh](../scripts/pack-images.sh) | 重新生成启动镜像、sparse rootfs、校验值及构建报告 | 可按项目标准输出完整匹配产物 |

手机已通过临时补充 `nft_compat`、`nft_fib`、`nft_fib_ipv4`、`xt_MASQUERADE`、`xt_nat` 恢复 Docker，并验证 hello-world、容器 DNS、容器间通信、对外连接及局域网端口映射。该结果证明故障定位及 IPv4 修复方向，不能替代新固件启动测试。

## 两条可行路径

| 路径 | 做法 | 适用范围 |
| --- | --- | --- |
| 对当前镜像追加补丁 | 在确认内核与模块完全匹配后，把这次编译的五个模块加入 rootfs，生成依赖索引并重新封装 | 快速修复当前精确内核版本；仍需解决源码构建如何复现，且不补齐 IPv6 FIB |
| 固化构建配置（推荐） | 每次构建合并受版本控制的配置片段，正常编译内核与模块，并验证最终镜像 | 同时覆盖全新构建和旧镜像续建，可随项目维护 |

本地镜像追加模块只会修复镜像文件。把 rootfs 刷入 `userdata` 会覆盖当前手机系统，不能把它当作保留现有数据的增量升级。

## 推荐实现

### 1. 增加内核配置片段

拟新增 `config/kernel/docker.config`，基础内容为：

```text
CONFIG_NFT_COMPAT=m
CONFIG_NFT_FIB_IPV4=m
CONFIG_NFT_FIB_IPV6=m
CONFIG_NFT_FIB_INET=m
CONFIG_NETFILTER_XT_TARGET_MASQUERADE=m
CONFIG_NETFILTER_XT_NAT=m
```

`CONFIG_NFT_FIB=m` 会由 Kconfig 自动选中。前述 IPv4 实机修复的最小需求是 COMPAT、IPv4 FIB、MASQUERADE 和 NAT；正式固件建议同时补齐 IPv6 和 inet family 的 FIB 支持。配置支持不等于已经验证 IPv6 容器网络。

现有 namespaces、cgroups、seccomp、veth、bridge、overlayfs、conntrack 等基础功能已启用，继续作为构建检查前置条件。保持 Docker 默认的 iptables 后端及系统现有 nftables 实现。

无需修改 Linux 驱动源码或更新内核子模块提交：配置片段可直接由父仓库管理。

### 2. 修正续建时的配置合并

调整 `scripts/build-kernel.sh`，在首次生成基础配置后，以及每次已有配置续建时，都执行以下逻辑：

1. 用内核自带 `scripts/kconfig/merge_config.sh -m` 把片段合并到目标输出目录的 `.config`。
2. 运行相同 `ARCH=arm64` 和输出目录下的 `make olddefconfig`，解析依赖。
3. 明确校验最终配置中的必需项；选项被依赖条件移除时应停止构建。
4. 执行现有增量 Kbuild、模块安装和产物指纹生成。

无需删除 `.config`，也不需要清空 rootfs 完成标记。后续删除片段中的一项不会自动禁用已有配置中的对应选项；需要关闭时应显式写入禁用项。

### 3. 更新镜像并增加 Docker 专项验证

复用 `update-kernel-image.sh` 的复制、depmod、initramfs 与重新封装流程。建议新增 `scripts/verify-docker-kernel.py` 和 `scripts/verify-docker-image.py`，分别检查构建配置与最终镜像，接入 `build-kernel.sh`、`pack-images.sh` 和 `write-build-report.py`。

验证应包含：必需配置、模块文件及 vermagic、模块与构建产物哈希一致、模块依赖索引，以及镜像内核配置与本次构建一致。IPv6 FIB 还应确认新内核导出的符号满足模块依赖。

正常构建的模块已有自动加载别名，建议先使用内核自动加载并在冷启动中验证；若仍需显式预加载，再把配置纳入全新 rootfs 和已有镜像更新流程。当前手机的 `docker-netfilter.conf` 是临时补充方案的一部分，不是官方构建必需条件的已验证结论。

现有更新脚本用 `cp -a` 合并模块目录。对安装过临时补丁的系统，须避免旧 `updates/docker/` 模块优先于正式模块被加载。建议新内核采用可区分的 release 后缀，以保留旧模块目录；若保持相同 release，应在迁移时核实并清理本次修复拥有的临时模块，再重新生成依赖索引。不要清理未知第三方模块。

### 4. Docker 软件预装作为独立选项

当前仓库未预装 Docker。只启用上述内核功能时，用户仍按常规方式安装 `docker` 和 `docker-compose`，然后启用服务。

若要求开箱即用，可另增 Docker 包清单与 rootfs 配置阶段，覆盖全新构建和已有镜像续建，记录包版本及服务默认状态。建议预装时默认不启动 daemon，按需启用；是否默认加入 `docker` 组也应明确记录。镜像只有 5 GiB，增加软件时应检查剩余空间。

## 已完成的配置验证

本轮在独立输出目录中验证了两条路径：

1. `defconfig` → `sm8450.config` → Docker 片段 → `olddefconfig`。
2. 复制当前正式 `.config` → Docker 片段 → `olddefconfig`。

两条路径中六个配置项均保留为 `m`，`NFT_FIB` 自动选中为 `m`，另外检查的 29 个普通容器基础配置均保持启用。正式 `linux/output/.config` 的哈希未变化。

原始结果保存在本地 `logs/docker-firmware-research-20260916/` 下的 `result.json`、`base-requirements.json`、配置差异和命令日志中。没有进行完整新内核编译、正式镜像写入或实机刷写。

## 实施后的验收

- 构建：全新配置及旧配置续建均通过必需项检查；完整链接内核及模块成功；再次续建结果稳定。
- 离线镜像：正式模块、配置及依赖索引正确；现有 ext4、启动镜像内容、sparse 往返和 SHA-256 检查通过；构建报告记录 Docker 兼容验证。
- 实机：冷启动后无需手工 modprobe 即可启动 Docker；hello-world、默认及用户自建 bridge、DNS、出网、局域网端口映射和 Compose 双服务通信通过。
- IPv6：若作为支持范围发布，再测试 IPv6 bridge、路由和端口发布。Swarm、加密 overlay / IPsec、rootless Docker 另行验证。

QEMU 用户态 chroot 使用宿主内核，只能验证软件与镜像配置，不能证明手机新内核的 Docker 网络工作正常。

## 产物与手机升级

推荐方案会输出匹配的新内核、模块、root.img、sparse_root.img 和启动镜像。IPv6 FIB 的 `fib6_lookup` 导出受 `CONFIG_NFT_FIB_IPV6` 控制，因此需要新内核；不能只复制 IPv6 模块到当前运行内核。

正式镜像按现有流程统一重新打包，实际需刷哪些启动分区应根据产物差异和当前设备布局决定。手机当前 Docker 已可用，可以先完成固件生成与离线验证。若后续要求保留手机现有数据，应设计协调更新模块与 boot 的升级步骤，而不直接刷写完整 `userdata`。
