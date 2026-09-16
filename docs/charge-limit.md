# OnePlus 10 Pro 充电限制

已将可选充电限制加入当前 Arch Linux ARM 固件源码，镜像统一位于 `images/7.3-rc2/`，日志位于 `logs/7.3-rc2/`。内核改动已提交到本地 `7.3-rc2` 分支的 `2c092365fd5c72efcedc29a578601b0a41ce45c4`，版本为 `7.3.0-rc2-sm8450+`。本次未刷入手机，也未在手机上启用服务。

## 行为

| 条件 | 行为 |
| --- | --- |
| 默认启动，服务未运行 | 限制关闭，保持原来的充电行为 |
| 服务运行，电量 ≥ 80% | 请求停止电池充电，保留 USB 输入通路 |
| 服务运行，电量 ≤ 75% | 解除本策略的停充要求，仍服从固件温控等限制 |
| 电量在 75% 与 80% 之间 | 保持上一状态；首次开启时在此区间先停充 |
| 停止服务 | 关闭限制，请求恢复正常充电；固件离线时等待重连后恢复 |
| 电量读取或命令失败 | 显示错误并重试；无有效读数时请求停充，不报告已经成功 |

上下限均可配置，要求 `0 <= START_THRESHOLD < END_THRESHOLD <= 100`。边界按整数 SOC 判断，75% 恢复、80% 停止；大约每 5 秒检查一次，通知和配置变化会触发额外检查。这是软件阈值控制，不能保证实际电池电量绝不短暂超过设定值。

停充使用一加电池充电开关 `BATT_OPLUS_CHG_EN`，不把 USB 输入电流设为零，不主动改变 PD 电压，也不强制电池放电。DCP/SuperVOOC 路径会先请求关闭 VoocPHY 快充。设计目的是停充后继续利用适配器供电；**当前尚未测量新驱动下的实际输入功率和电池电流，不能把命令确认等同于已验证稳定直供。** SuperVOOC 下的电量计连续更新和无线充电也尚未实测。

启用服务期间，systemd 抑制器与驱动挂起检查会阻止系统 suspend/hibernate，确保主机持续监测电量；熄屏和 CPU idle 仍可用。停止服务即可解除这一限制。即使拔掉电源，只要服务仍启用，挂起限制也仍存在。当前没有使用未经验证的 ADSP 自主阈值功能。

如果目前电量为 100%，开启后只会请求停充，不会主动放电到 80%；持续外接供电时，电量可能长时间高于 80%。

## 使用方式

以下命令在运行新固件的手机上执行。镜像中的服务已安装但默认 `disabled`，驱动策略也默认关闭。

仅本次启动服务：

```sh
sudo systemctl start negroni-charge-limit.service
```

启动服务并让以后开机自动运行：

```sh
sudo systemctl enable --now negroni-charge-limit.service
```

修改 `/etc/negroni-charge-limit.conf`：

```ini
START_THRESHOLD=75
END_THRESHOLD=80
```

修改后执行 `sudo systemctl restart negroni-charge-limit.service`。查看状态：

```sh
systemctl status negroni-charge-limit.service --no-pager
/usr/local/libexec/negroni-charge-limit status
journalctl -u negroni-charge-limit.service -b --no-pager
```

`enabled=1 state=holding error=0` 表示固件已确认停充命令；`released` 表示本策略已放行；`pending` 表示尚未成功应用，应结合 `error` 和日志检查。命令状态不是电流测量结果，UI 的“未充电”图标也不能单独证明电源直供。

停止限制并取消开机自启：

```sh
sudo systemctl disable --now negroni-charge-limit.service
/usr/local/libexec/negroni-charge-limit status
```

## 源码与重新构建

- 内核子模块中的 `linux/drivers/power/supply/qcom_battmgr.c`（驱动）与 `qcom_battmgr_charge_limit.h`（同目录下的阈值状态机）：仅对 `oplus,negroni` 提供控制；统一约束初始化、热插拔、ADSP 充电许可回复和快充重试中的重新充电动作。先运行初始化脚本，再在本地查看这些源码。
- [服务脚本](../config/charge-limit/negroni-charge-limit)、[默认配置](../config/charge-limit/negroni-charge-limit.conf)、[systemd 单元](../config/systemd/negroni-charge-limit.service)：配置校验、启动就绪检查、设备重建后的配置恢复、退出时解除限制。
- [安装脚本](../scripts/configure-charge-limit.sh)与[镜像更新脚本](../scripts/update-kernel-image.sh)：全新 rootfs 和已有镜像都会安装以上文件，并确认服务默认关闭。
- [构建入口](../build.sh)：每次调用 Kbuild 检查源码变化；通过内核产物和服务源文件的校验值识别旧 rootfs，更新模块与启动文件后重新封装，避免完成标记跳过新驱动。

继续使用当前已修改源码构建：

```sh
./build.sh 7.3-rc2
```

该命令更新 `images/7.3-rc2/`，日志写入 `logs/7.3-rc2/`。已有 rootfs 可以按阶段更新：

```sh
LOG_DIR=logs/7.3-rc2 bash scripts/build-kernel.sh
sudo env IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/update-kernel-image.sh
IMAGE_DIR=images/7.3-rc2 LOG_DIR=logs/7.3-rc2 bash scripts/pack-images.sh
```

`linux/` 仓库仅提交内核驱动与新增状态机头文件，提交尚未推送远端。服务、配置、构建脚本、测试和文档已纳入本父仓库，`linux/` 作为子模块；首次克隆后运行 `bash scripts/init-submodules.sh`，从随仓库提供的 Git bundle 恢复未推送提交。迁移到新工作区时，使用内核提交 `2c092365fd5c72efcedc29a578601b0a41ce45c4` 并保留这些配套文件；也可在父提交 `593ad628e5b838457d6a316cc08ba5dcc924a01f` 上应用 [导出的提交补丁](../artifacts/charge-limit-implementation/qcom-battmgr-charge-limit.patch)。构建报告记录实际源码提交；干净工作区的 `kernel-local.patch` 为空。

## 已完成验证及真机验证范围

已完成完整交叉编译和模块安装、状态机边界与滞回测试、实际驱动函数的模拟传输测试、8 项服务测试、systemd 单元检查。ARM64 rootfs 中的 Python 脚本编译及 initramfs 生成成功。

最终镜像检查核对驱动模块、内核、服务、配置与源码一致，模块版本匹配、服务无启用链接，默认阈值 75/80。封装过程检查 ext4、Android 启动镜像解包结果、sparse 往返一致性及 SHA-256。结果见 `logs/7.3-rc2/build-result.json`（本地文件）和`logs/7.3-rc2/charge-limit-verification.json`（本地文件）。

真机仍需先用当前普通 USB PD 电源验证停充、恢复、拔插和停止服务后的行为，同时观察 USB `online`、电压/电流、电池 `current_now` 和电量随时间的变化；最好结合 USB 功率表与有负载时的电池电流。随后再单独验证 SuperVOOC。调研依据及原厂代码引用见 [调研报告](../artifacts/charge-limit-research-20260916/README.md)。日常取消限制只需停止服务。
