# 贡献指南

欢迎提交构建问题、设备验证记录、文档改进和代码修复。项目当前聚焦 OnePlus 10 Pro NE2210 与固定的 `7.3-rc2` 内核，其他机型和分支的结果请单独注明。

## 开发准备

按[构建指南](docs/building.md)克隆项目并执行 `bash scripts/init-submodules.sh`。只修改文档或服务脚本时，无需构建完整镜像。

```sh
bash scripts/check.sh
bash scripts/check.sh --with-kernel
git diff --check
```

检查需要 Bash、Python 3、Git；内核测试另需主机 C 编译器（`cc`）、ARM64 交叉编译器、flex 和 bison。`--with-kernel` 会编译并执行阈值状态机测试，以及从真实驱动提取函数、模拟 ADSP 传输的测试，并在独立输出目录中检查 Docker 配置的首次生成、续建、幂等性和必需项拒绝行为。服务测试覆盖配置解析、写入顺序、设备重建和退出恢复。

修改 CPU 默认安装或镜像检查时，运行真实 rootfs 安装/校验测试（需要 `systemctl`、`mkfs.ext4`、`debugfs`）：

```sh
sudo python3 -B tests/test-cpu-performance-image.py
# 支持无特权 user namespace 的主机也可用：
unshare --user --map-root-user python3 -B tests/test-cpu-performance-image.py
```

该测试检查全新安装、已有镜像补装、开机启用、封装失效，以及缺失链接、错误权限和内容变化的拒绝行为。CI 还会用固定内核源码从干净目录编译并核对 8 核设备树参数。

GitHub Actions 在推送和 Pull Request 中恢复固定子模块，并执行同一组检查。CI 不构建整个 ARM64 内核或 rootfs，也不代表实机验证通过。

## 改动范围

- 镜像配置放在 `config/`，构建与验证逻辑放在 `scripts/`，说明放在 `docs/`；日期相关记录放在 `docs/archive/`。
- Shell 使用 Bash 与 `set -euo pipefail`，路径和变量正确引用；Python 使用标准库，新增依赖需说明理由。
- 修改驱动或服务行为时运行对应测试，并记录尚未覆盖的真机行为。修改文档或格式无需编造无关测试。
- 内核改动先在 `linux/` 中提交，再更新父仓库 gitlink；确保提交可从远端或随仓库提供的 bundle 恢复，并同步初始化脚本中的前置提交与恢复说明。
- 配置变更应说明对全新构建和已有镜像续建的影响；更新用户可见行为时同步使用文档。

不要提交登录凭据、认证辅助脚本、SSH 私钥、设备分区备份或镜像产物。原始输出放在本地 `logs/`；分享必要片段前移除密码、序列号和个人网络信息。`artifacts/` 仅保存经过整理、对结论有帮助的资料。

## 报告问题

使用 GitHub Issue 模板提供设备型号、固件升级历史、构建主机、父仓库/内核提交、复现步骤、预期与实际结果。日志请提供相关片段，并标注是构建、模拟测试还是实机输出。

构建问题优先提供失败阶段及 `logs/7.3-rc2/` 中对应日志。实机问题同时说明镜像版本、槽位和启动现象；不要只用“最新版本”代替提交号。

## 提交 Pull Request

说明解决的问题、最终行为和验证方式；没有进行的实机验证请明确写出。提交前检查相对链接、`git diff --check` 和子模块状态，避免混入本地产物或无关子模块变化。

第三方文件保留原版权与许可声明；仓库现有许可状态见[来源与许可](docs/licensing.md)。
