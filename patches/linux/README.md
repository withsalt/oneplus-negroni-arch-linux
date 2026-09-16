# 尚未推送的内核提交

`local-7.3-rc2.bundle` 保留两个本地提交的原始 Git 对象和提交 ID：

- `593ad628e5b838457d6a316cc08ba5dcc924a01f`：修正 Negroni 电池电流符号。
- `2c092365fd5c72efcedc29a578601b0a41ce45c4`：增加默认关闭的充电阈值限制。

bundle 的前置提交为上游已有的 `e8291a2765b80f94c047d28ca514465c7716c374`，父仓库的 `linux` gitlink 固定到第二个本地提交。bundle 保留原提交 ID，避免重放补丁后 gitlink 无法匹配。完整本地内核对象库、分支、远端引用和构建输出也已复制到迁移后的工作区。

首次克隆父仓库后运行 `bash scripts/init-submodules.sh`。该脚本取得前置提交并导入 bundle，然后按父仓库固定提交初始化子模块。普通 `git clone --recurse-submodules` 在这两个提交尚未推送到内核远端时可能失败，此时仍可运行初始化脚本补齐对象。已初始化的 `linux/` 若有未提交修改，脚本会停止以保留修改。

本次迁移没有推送任一远端。后续发布内核提交后可以直接使用标准子模块初始化；此 bundle 仍可作为恢复副本。

## 可选的 CPU 设备树修复

`sm8450-cpu-capacity.patch` 为 SM8450 的 8 核补充能力和功耗模型参数，基于上述固定提交，参数来自一加公开的 waipio 设备树。它作为独立部署配方保存，尚未应用到子模块 gitlink 或默认内核构建。

运行 `python3 scripts/build-cpu-performance-dtb.py` 会在临时源码副本中应用补丁，生成并校验原始和优化后的 DTB / vendor_boot 镜像。该脚本只依赖固定 Git 对象和宿主工具，不依赖之前的内核编译目录。参数来源、实测范围、安装和恢复步骤见 [CPU 性能与调度指南](../../docs/cpu-performance.md)。
