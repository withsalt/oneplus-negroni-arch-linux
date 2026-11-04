# 7.3 设备修复与源码恢复

当前 `linux` 子模块固定到 `7.3` 修复提交 `23a6b6d834e93dd54d9ec6f188761a208e1e1ed3`。修复从重新同步的上游 `476a4a8984395a77b6b6508f849dfb7b786d86b3` 直接修改，新增提交均只有一个父提交；没有 merge 或 cherry-pick `7.3-rc2`。之前的本地合并版本不再作为固定内核。

新增提交为 `835c18d2739`（电池驱动及限充）、`3ca85b51d20`（CPU 参数）和 `1eaec99b04a`（补齐触觉驱动 trace 头文件）。

USB/PD 修复包含 `f4f7868ffd28`（USB2 门控、参考时钟恢复、PHY 掩码与板级参数）、`439ec8c36683`（DP C/E 保留 USB2）和 `0354cd55ba6a`（OTG GPIO、PD/USB 充电）。

角色切换修复包含 `b75515b1a61`（按目标角色设置 QSCRATCH，处理 NONE）和 `fc09f1b2e7e`（注册 xHCI 前完成 PHY 模式设置并处理失败）。当前实机限制见[设备状态](../../docs/status.md)。恢复 bundle 包含上述修复及原有电池/CPU 修复和独立 rc2 引用，仍使用下述两个已发布的前置提交。

充电状态修复 `0466d55fd348` 为 Negroni 按当前输入、电池电流和已确认限充状态计算最终状态，修正固件直接报告放电、实际却在充电的问题；读取失败或固件重连时拒绝旧数据。源码与使用行为见[充电说明](../../docs/charge-limit.md)。

USB 失电恢复修复 `23a6b6d834e` 在完整关闭 DWC3 时通知 xHCI 重建，覆盖 HOST 与原生 OTG 主机分支，并在每轮 PM prepare 清除旧标志；本机受控对比中，缺失通知时出现非法 DMA、外设全部丢失，修复后连续 3 次相同测试通过。此前热插拔的 `IOVA=0xc` 故障仍未确认消除，见[设备状态](../../docs/status.md)。

## 重做范围

| rc2 原始修复 | 7.3 实现 |
| --- | --- |
| `593ad628e5b838457d6a316cc08ba5dcc924a01f`：Negroni 电池电流极性 | 在当前 `oplus_battmgr` 的电池属性出口转换符号，仅适用于 `oplus,negroni` |
| `2c092365fd5c72efcedc29a578601b0a41ce45c4`：可选充电限制 | 重新接入新驱动的请求、通知、初始化与恢复路径；默认关闭，75/80% 滞回，停充保留 USB 输入，提供 sysfs 控制、失败重试及挂起限制 |
| `05e876f60a304a400bc1b44da2de3b155c2bc829`：CPU 能力与功耗 | 在当前 `sm8450.dtsi` 补齐 8 核的 16 个参数，沿用一加公开 waipio 设备树的实测值 |

7.3 的 Negroni 已切换到 `oplus_battmgr`。适配同时修正充电开关与 USB 查询的 opcode、SM8450 USB 属性编号及电池温度的重复缩放；将等待回复的通知处理放到工作队列，统一在固件请求锁下管理充电状态和许可回复。限充期间的 VOOC 通知和错误恢复不能关闭 USB 输入；被拦截的 VoocPHY 启用请求在放行后补发；电量计初始化失败每 5 秒重试，连接代次变化、固件掉线或停止时不误报成功。

未修改的上游 Oplus 驱动对象编译失败，包含不存在的厂商结构成员和函数、指针成员访问错误以及未定义的 `chip`。本次移除未调用且缺少依赖的厂商辅助函数，并修正有效调用点。`sm8450.config` 启用 Oplus 驱动及共享 Qualcomm 模块。触觉驱动补齐下述 trace 头文件；其余 7.3 GPU、haptics/PBS 驱动、Negroni 设备树及 Kconfig/Makefile 沿用当前上游。

## 恢复与历史复现

`local-7.3.bundle` 提供两个独立引用：

- `refs/heads/7.3`：当前线性修复；前置提交为 `476a4a8984395a77b6b6508f849dfb7b786d86b3`。
- `refs/heads/7.3-rc2`：保留上述三个历史提交的原 ID，仅供原始修复与 CPU 对比复现；前置提交为 `e8291a2765b80f94c047d28ca514465c7716c374`。此引用不属于新 `7.3` 的祖先历史。

首次克隆运行 `bash scripts/init-submodules.sh`。脚本获取 bundle 的已发布前置提交、验证 bundle、导入两个分支到 `refs/remotes/migration/`，然后恢复父仓库固定的内核提交。即使当前固定提交已经存在，也会补齐缺失的历史引用所需对象。内核新提交通过父仓库的 bundle 分发，无需依赖它已推送到内核远端。

已有 `linux/` 若存在未提交修改，初始化脚本会停止。已有本地 `7.3` 仅在可快进时更新；旧合并分支若与新提交分叉则保留，固定源码检出为 detached HEAD。之后可不带分支参数执行 `./build.sh`，或先自行保存并调整本地分支；`./build.sh 7.3` 会拒绝缺少当前固定修复的旧分支。

`local-7.3-rc2.bundle` 继续保留原始恢复副本。`sm8450-cpu-capacity.patch` 基于旧 `2c092365fd5c`，参数来源、实测范围及恢复步骤见 [CPU 性能与调度指南](../../docs/cpu-performance.md)。`python3 scripts/build-cpu-performance-dtb.py` 使用独立的历史对象，在临时目录应用补丁生成对比 DTB / vendor_boot，不改变当前内核工作树。

## 验证与 code review（2026-10-24）

修改完成后复核了协议映射、限充边界、固件许可、重连代次、请求锁、工作项生命周期及恢复脚本。Review 发现退出过程中回调仍可能提交任务，以及恢复任务可能在限充任务取消后重新排队；现以事件自旋锁串行化回调排队和停止标记，等待所有生产者退出后最后取消限充任务，同时保留命令回复以完成正在等待的请求。新增退出回归覆盖生产者完成时再次排队和停止后的通知/回复处理及阈值写入拒绝。复核后未发现阻塞提交的问题。

已通过：

- `bash scripts/check.sh --with-kernel`：8 项服务测试、4 项子模块恢复测试、阈值策略、7 组真实驱动函数回归、3 项全新/续建 Docker 配置测试。
- ARM64 的 Qualcomm/Oplus 电池驱动对象编译和 Negroni DTB 编译；核对全部 8 核参数。Oplus 驱动仍有未使用函数和缺少原型的上游警告。
- 新增遥测回归验证电流极性、温度单位、USB 查询 opcode、停充状态与离线阈值读取；分别回退前三项修复均触发断言失败。回退任务取消顺序也会触发退出回归失败。
- 历史 CPU DTB / vendor_boot 对比构建；恢复 bundle 的独立前置提交导入和初始化脚本验证。
- 父仓库与内核 `git diff --check`；新内核提交区间无 merge，rc2 头不是新 7.3 的祖先。

截至 2026-10-24，尚未构建完整新固件或进行真机验证；历史实测仍对应 `7.3-rc2`。

## 完整构建修复与验证（2026-11-01）

完整编译发现 `qcom-hv-haptics.c` 引用的 `include/trace/events/qcom_haptics.h` 缺失。`1eaec99b04a` 从[一加公开 SM8450 内核的固定提交](https://github.com/OnePlusOSS/android_kernel_oneplus_sm8450/blob/449760504b3a75728eb60479f5b34dc51cb263ce/include/trace/events/qcom_haptics.h) 补齐四个 trace 事件，保留版权与 GPL-2.0-only 声明，并将 `__assign_str()` 适配为当前内核的单参数形式。恢复 bundle 已包含此提交。

构建脚本同时补齐未跟踪头文件的本地补丁记录，并按 `CONFIG_PHY_QCOM_QMP_UFS` 区分内置与模块配置：内置驱动不要求 initramfs 再包含 `.ko`，模块配置仍强制检查对应文件。

包含上述修复的 `7.3.0-sm8450+` 完整内核、设备树、模块及 641 包 rootfs 已构建完成；Docker、CPU、屏幕默认值、充电限制、ext4、sparse 往返、启动镜像内容和 SHA-256 离线校验通过，`bash scripts/check.sh --with-kernel` 也通过。该次镜像在提交前构建，报告保留 `3ca85b51d20` 与本地头文件补丁的真实记录。镜像尚未刷入，真机启动待验证。
