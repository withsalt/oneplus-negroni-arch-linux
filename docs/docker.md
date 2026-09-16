# Docker 内核支持

固件构建已集成 Docker 普通 bridge 网络所需的内核配置，覆盖全新构建和已有镜像续建。它修复了 Arch Linux ARM 的 iptables（nf_tables 后端）在创建 Docker 网络时报告 `addrtype` 或 `MASQUERADE` 扩展不支持的问题。

镜像提供内核支持，Docker 软件按需安装，daemon 不会因这项配置常驻运行。在手机上执行：

```sh
sudo pacman -Syu docker docker-compose
sudo systemctl enable --now docker.service
sudo docker run --rm hello-world
docker compose version
```

若希望普通用户直接操作 Docker，可执行 `sudo usermod -aG docker "$USER"`，再注销并重新登录。`docker` 组具有接近 root 的系统控制权限。

## 构建与续建

从项目根目录运行：

```sh
./build.sh 7.3-rc2
```

[configure-docker-kernel.sh](../scripts/configure-docker-kernel.sh) 在首次生成 `defconfig` 和 `sm8450.config` 后，以及每次已有配置续建时，都合并 [docker.config](../config/kernel/docker.config)，运行 `olddefconfig` 并检查最终配置。自定义内核设置会保留，不需要删除 `.config` 或重新安装 rootfs。

配置新增以下模块：

| 配置 | 模块与用途 |
| --- | --- |
| `CONFIG_NFT_COMPAT=m` | `nft_compat`，提供 Xtables 扩展兼容层 |
| `CONFIG_NFT_FIB_IPV4=m` | `nft_fib_ipv4`，IPv4 地址类型及路由查询 |
| `CONFIG_NFT_FIB_IPV6=m` | `nft_fib_ipv6`，IPv6 地址类型及路由查询 |
| `CONFIG_NFT_FIB_INET=m` | `nft_fib_inet`，inet family 查询 |
| `CONFIG_NETFILTER_XT_TARGET_MASQUERADE=m` | `xt_MASQUERADE`，容器出网 NAT |
| `CONFIG_NETFILTER_XT_NAT=m` | `xt_nat`，SNAT / DNAT 与端口映射 |

Kconfig 自动选择共用的 `nft_fib`。构建同时检查现有 namespaces、cgroups、seccomp、veth、bridge、overlayfs 和 conntrack 等基础功能。

所有模块通过正常内核构建安装到 `kernel/` 目录，使用 depmod 索引和内核自动加载别名；不依赖手机临时修复使用的 `updates/docker/` 或手工 modprobe。构建后的模块配置及产物指纹会触发现有 `update-kernel-image.sh`，更新 root.img、重建 initramfs，并重新封装镜像。

本次维持 `7.3.0-rc2-sm8450+` release 名称，但启用 IPv6 FIB 会改变内核符号导出，因此新模块应与本次内核一起使用，不能只按 release 字符串判断二进制兼容性。源码提交保持不变，实际配置和模块哈希写入构建报告。

## 检查与结果

```sh
bash scripts/check.sh --with-kernel
python3 scripts/verify-docker-kernel.py linux/output logs/7.3-rc2
python3 scripts/verify-docker-image.py images/7.3-rc2 logs/7.3-rc2
(cd images/7.3-rc2 && sha256sum -c SHA256SUMS)
```

内核检查验证必需配置、`fib6_lookup` 导出、模块 vermagic、依赖文件和自动加载别名。镜像检查通过 debugfs 提取真实镜像内容，比对内核及模块哈希，再用 `modprobe --show-depends` 离线验证镜像自己的依赖索引；不会向构建主机加载 ARM 模块。旧临时模块覆盖正式模块、依赖缺失或索引过期都会被拒绝。

结果记录到本地：

- `logs/7.3-rc2/docker-kernel-verification.json`
- `logs/7.3-rc2/docker-image-verification.json`
- `logs/7.3-rc2/build-result.json` 的 `docker_kernel`、`docker_image` 和对应 `validation` 字段

回归测试使用固定内核的真实 Kconfig，检查首次配置、重复执行不改变配置、旧配置自动补齐且保留自定义设置，以及缺少网络命名空间时明确失败。`--with-kernel` 因此还需要构建指南中的 ARM64 交叉编译器、flex 和 bison。

2026-09-16 已完成完整内核链接、旧 rootfs 增量更新和固件封装，26 个模块及其依赖、ext4、sparse 往返、启动镜像内容和全部七项 SHA-256 校验均通过。再次执行 `./build.sh 7.3-rc2` 后镜像文件与时间戳均未变化。另验证了旧镜像、缺失依赖、缺失二进制索引及临时模块覆盖均被拒绝。汇总记录为本地 `logs/7.3-rc2/docker-firmware-verification.json`。

相较修复前，`boot.img`、`root.img` 和 `sparse_root.img` 的内容改变；`vendor_boot.img`、`recovery.img`、`vbmeta.img` 和 `initramfs-linux.img` 内容一致。修复前镜像及日志保存在本地 `backups/docker-firmware-20260916/`。

手机此前补装 IPv4 模块后，hello-world、DNS、容器间通信、对外连接、局域网端口映射和 Docker 服务重启已通过验证。这不代表新固件已通过实机冷启动测试；新固件及 IPv6 容器网络仍需在部署后验证。Swarm、加密 overlay / IPsec 和 rootless Docker 不在此次验收范围内。

## 已有手机系统

本次构建输出完整镜像，不自动升级手机。刷入 `sparse_root.img` 会覆盖 `userdata`，详见[刷机说明](flashing.md)。需要保留现有系统和容器数据时，应另行协调更新启动内核与模块。

安装过临时修复的系统存在 `/usr/lib/modules/7.3.0-rc2-sm8450+/updates/docker/`。正式升级时需要核实并移除这次临时修复拥有的旧模块，再执行 depmod，避免它们优先于正式模块加载。不要删除未知第三方模块；仅复制新的 boot.img 或仅复制新模块都不能代替匹配更新。

实施前的根因与路径比较见[调研记录](docker-firmware-plan.md)。
