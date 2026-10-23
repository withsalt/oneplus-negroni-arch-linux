# 内核合并与恢复

当前 `linux` 子模块固定到 `7.3` 的充电恢复修复提交 `1ebe1068669e9f55325bc84fe18f8f0aa2e5a92e`。它基于合并提交 `2c66a117babfa1c90b44efaee96ce9da7c109fab`，该合并的两个父提交为：

- `476a4a8984395a77b6b6508f849dfb7b786d86b3`：本次拉取的最新远端 `7.3`。
- `05e876f60a304a400bc1b44da2de3b155c2bc829`：原 `7.3-rc2`，包含本项目的三个设备修复。

`local-7.3.bundle` 保存当前修复、合并提交及以下三个原始提交，保留原提交 ID：

- `593ad628e5b838457d6a316cc08ba5dcc924a01f`：Negroni 电池电流符号修正。
- `2c092365fd5c72efcedc29a578601b0a41ce45c4`：默认关闭的充电阈值限制。
- `05e876f60a304a400bc1b44da2de3b155c2bc829`：SM8450 CPU 能力与功耗模型。

新 bundle 需要两个已发布的前置提交：`476a4a8984395a77b6b6508f849dfb7b786d86b3` 和 `e8291a2765b80f94c047d28ca514465c7716c374`。首次克隆后运行 `bash scripts/init-submodules.sh`：脚本读取 `.gitmodules` 中的 `7.3` 分支，按 bundle 头部取得全部前置提交、验证并导入 bundle，然后检出父仓库固定版本。合并尚未推送时，普通 `git clone --recurse-submodules` 可能无法取得它，应使用此脚本恢复。本次没有推送任何远端。

已有 `linux/` 若存在未提交修改，初始化脚本会停止。已有本地 `7.3` 仅在可快进到固定提交时更新，分叉分支保留；固定源码仍会被恢复到 detached HEAD。

## 合并处理

两条分支的设备支持经历了重放和重构。冲突采用 `7.3` 的设备树、配置、新电池驱动和 haptics/PBS 实现；保留较新内核已实现的 A840 IFPC / preemption，去除旧分支重复的 haptics Kconfig / Makefile 项，并补回 SM8450 的 16 个 CPU 参数。

Negroni 已切换到 `oplus_battmgr`。合并将电流符号、75/80 阈值、失败重试、重连后采样、快充拦截及挂起限制接入该驱动；SM8450 USB 属性编号按原固件协议转换。移除未被调用、缺少依赖而无法编译的厂商辅助函数，初始化充电互斥锁，避免 GLINK 接收回调同步等待自身响应，并在客户端释放前取消工作项。常规构建补齐新电池模块，服务使用 `/sys/class/power_supply/battery`，镜像校验同时检查两个电池模块。

后续修复统一使用固件请求锁管理有效充电状态与许可回复，覆盖 VOOC 通知和错误恢复中的 USB 输入暂停，记录并补发被拦截的 VoocPHY 启用请求，并在电量计初始化失败后每 5 秒重试。固件掉线或驱动停止时不继续初始化；初始化期间连接代次变化也不会被误记为成功。

已通过 `bash scripts/check.sh --with-kernel`、两种电池驱动的 ARM64 对象编译、Negroni DTB 编译及 8 核参数校验。新增的充电许可、USB 输入保留、VOOC 补发和初始化重试回归测试均能在修复前触发失败，修复后通过。新分支尚未执行完整固件构建或实机验证，历史实测仍对应 `7.3-rc2`。

## 历史恢复与 CPU 复现

`local-7.3-rc2.bundle` 保留旧恢复副本，前置提交为 `e8291a2765b80f94c047d28ca514465c7716c374`；当前初始化使用新 bundle。三个设备修复现在也已发布在远端 `7.3-rc2`。

`sm8450-cpu-capacity.patch` 基于 `2c092365fd5c`，参数来自一加公开的 waipio 设备树。改动原始提交为 `05e876f60a304`，已经合入当前 `7.3`，默认 Kbuild 会生成包含修复的 DTB。

运行 `python3 scripts/build-cpu-performance-dtb.py` 会在临时源码副本中应用补丁，生成并校验原始和优化后的 DTB / vendor_boot 镜像。该脚本仍使用历史固定对象，所需提交由新 bundle 一并保留。参数来源、实测范围、安装和恢复步骤见 [CPU 性能与调度指南](../../docs/cpu-performance.md)。
