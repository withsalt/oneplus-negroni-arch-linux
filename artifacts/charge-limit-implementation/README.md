# 实现与构建记录

2026-09-16 已完成源码修改、内核编译和固件封装。手机未刷写，服务未在手机上启用；新镜像默认关闭限制，默认阈值 75/80。

- [操作与实现说明](../../docs/charge-limit.md)
- `images/7.3-rc2`（本地文件）及 `images/7.3-rc2/SHA256SUMS`（本地文件）
- `logs/7.3-rc2/build-result.json`（本地文件）
- `logs/7.3-rc2/charge-limit-verification.json`（本地文件）
- `logs/7.3-rc2/kernel-build.log`（本地文件）、`logs/7.3-rc2/kernel-image-update.log`（本地文件）、`logs/7.3-rc2/image-pack.log`（本地文件）
- [内核提交补丁](qcom-battmgr-charge-limit.patch)，提交 `2c092365fd5c72efcedc29a578601b0a41ce45c4`，父提交 `593ad628e5b838457d6a316cc08ba5dcc924a01f`，包含新增头文件
- [源码补丁与构建配置包](charge-limit-source.tar.gz)，不包含登录凭据

固件与日志统一使用 `7.3-rc2/`。本地内核分支仅提交两个驱动文件，工作区干净，未推送远端；服务、配置、构建脚本、测试与文档已纳入父仓库，`linux/` 作为子模块。未推送内核提交的完整 Git 对象另存于 [bundle](../../patches/linux/README.md)。

交叉编译与模块安装成功，未新增编译警告；原文件两条 GPIO 指针比较警告仍存在。此前在本次实现过程中执行并通过以下检查：

| 检查 | 覆盖 |
| --- | --- |
| `tests/charge-limit-policy.c` | 默认关闭、上下边界、滞回、异常 SOC、自定义阈值、关闭策略 |
| `tests/test-charge-limit-driver.py` | 提取实际驱动函数，以模拟 ADSP 检查 PD/VOOC 命令顺序、USB 输入保留、失败重试、重连和禁止重新充电 |
| `tests/test-charge-limit-service.py` | 8 项配置解析、写入顺序、设备重建、未就绪和退出恢复测试 |
| `systemd-analyze verify` | 使用临时副本指向构建机上的真实脚本，单元检查通过 |
| ARM64 rootfs 检查 | Python 脚本编译、depmod、mkinitcpio 成功 |
| 最终镜像检查 | 模块/内核/配置/服务与源码一致，模块版本匹配，服务默认关闭，ext4、sparse 往返、启动镜像解包比对通过 |

未验证新固件真机启动、实际停充/恢复及外接电源供电表现。`holding` 代表命令确认，不能代替电流测量；SuperVOOC 电量计行为与无线充电也仍需实测。启用限制期间会阻止系统挂起，熄屏仍可用。
