# OnePlus 10 Pro 充电限制

当前 `7.3` 源码将可选充电限制适配到 `oplus_battmgr`，镜像输出目录为 `images/7.3/`，日志为 `logs/7.3/`。服务使用 `/sys/class/power_supply/battery`，默认仍关闭。2026-09-16 的服务启停与 USB PD 停充实测基于旧 `7.3-rc2` 驱动；新驱动的模拟测试和交叉编译已通过，完整固件与真机行为待验证。

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

停充使用一加电池充电开关 `BATT_CHG_EN`（属性 24），不把 USB 输入电流设为零，不主动改变 PD 电压，也不强制电池放电。DCP/SuperVOOC 路径会先请求关闭 VoocPHY 快充。设计目的是停充后继续利用适配器供电；USB PD 真机采样中，停充时电池报告电流为 0、USB 输入保持在线。**这些是驱动遥测值，尚未用功率表或负载测试验证长期稳定直供。** SuperVOOC 下的电量计连续更新和无线充电也尚未实测。

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

- 内核子模块中的 `linux/drivers/power/supply/oplus_battmgr.c`（设备驱动）、`qcom_battmgr.c`（共享传输）与 `qcom_battmgr_charge_limit.h`（同目录下的阈值状态机）：仅对 `oplus,negroni` 提供控制；统一约束初始化、热插拔、ADSP 充电许可回复和快充重试中的重新充电动作。先运行初始化脚本，再在本地查看这些源码。
- [服务脚本](../config/charge-limit/negroni-charge-limit)、[默认配置](../config/charge-limit/negroni-charge-limit.conf)、[systemd 单元](../config/systemd/negroni-charge-limit.service)：配置校验、启动就绪检查、设备重建后的配置恢复、退出时解除限制。
- [安装脚本](../scripts/configure-charge-limit.sh)与[镜像更新脚本](../scripts/update-kernel-image.sh)：全新 rootfs 和已有镜像都会安装以上文件，并确认服务默认关闭。
- [构建入口](../build.sh)：每次调用 Kbuild 检查源码变化；通过内核产物和服务源文件的校验值识别旧 rootfs，更新模块与启动文件后重新封装，避免完成标记跳过新驱动。

继续使用当前源码构建：

```sh
./build.sh 7.3
```

该命令更新 `images/7.3/`，日志写入 `logs/7.3/`。已有 rootfs 可以按阶段更新：

```sh
LOG_DIR=logs/7.3 bash scripts/build-kernel.sh
sudo env IMAGE_DIR=images/7.3 LOG_DIR=logs/7.3 bash scripts/update-kernel-image.sh
IMAGE_DIR=images/7.3 LOG_DIR=logs/7.3 bash scripts/pack-images.sh
```

内核提交由父仓库 gitlink 固定，并与合并历史一起保存在 [Git bundle](../patches/linux/README.md) 中。首次克隆后运行 `bash scripts/init-submodules.sh`。每次构建补齐 `BATTERY_OPLUS_BATTMGR=m`，镜像校验同时核对 Oplus 驱动及其共享 Qualcomm 模块。构建报告记录实际源码提交；干净工作区的 `kernel-local.patch` 为空。

旧 `7.3-rc2` 充电限制提交为 `2c092365fd5c72efcedc29a578601b0a41ce45c4`，历史补丁见 [导出的提交补丁](../artifacts/charge-limit-implementation/qcom-battmgr-charge-limit.patch)。它已通过合并历史保留，不能直接应用到重构后的 `7.3` 驱动。

## 已完成验证及真机验证范围

以下完整镜像与实机结果对应 2026-09-16 的 `7.3-rc2`。当前 `7.3` 已通过服务、阈值策略、真实驱动函数模拟、USB 属性映射及驱动交叉编译检查，尚未构建完整镜像或刷入手机。

2026-10-24 的源码修复补齐了恢复路径：有效充电状态与 ADSP 许可回复保持一致；限充停充时，VOOC 通知与错误恢复不会把 USB 输入电流设为零；初始化阶段被拦截的 VoocPHY 启用请求在策略放行后补发；电量计初始化失败后每 5 秒重试，直到成功、固件掉线或驱动停止。新增测试运行实际通知及后台任务，并检查固件禁止充电、命令失败和重连期间的保守行为。这些结果属于主机模拟验证。

已完成完整交叉编译和模块安装、状态机边界与滞回测试、实际驱动函数的模拟传输测试、8 项服务测试、systemd 单元检查。ARM64 rootfs 中的 Python 脚本编译及 initramfs 生成成功。

最终镜像检查核对驱动模块、内核、服务、配置与源码一致，模块版本匹配、服务无启用链接，默认阈值 75/80。封装过程检查 ext4、Android 启动镜像解包结果、sparse 往返一致性及 SHA-256。结果见 `logs/7.3-rc2/build-result.json`（本地文件）和`logs/7.3-rc2/charge-limit-verification.json`（本地文件）。

### 服务启动等待问题与真机结果（2026-09-16）

原单元使用 `Type=notify`，但 `systemd-inhibit` 会从子进程环境中清除 `NOTIFY_SOCKET`，使 Python 服务无法发送 `READY=1` 和看门狗通知。表现为驱动已经停充，而 `systemctl start` 一直等待；达到 75 秒启动超时后服务被重启。按 Ctrl+C 只结束客户端等待，不会停止后台服务。

单元现在先由 systemd 将 `${NOTIFY_SOCKET}` 展开为参数，再通过抑制器之后的 `/usr/bin/env` 恢复子进程环境。保留 `Type=notify`、30 秒看门狗和睡眠抑制器。真机上通过 `systemd-analyze verify`，并确认 Python 子进程收到通知地址。

USB PD 电源、电池 88%、阈值 75/80 下的实测结果：

| 检查 | 结果 |
| --- | --- |
| 首次启动 / 停止后再次启动 | 0.326 秒 / 0.916 秒，均正常返回 |
| 持续运行 | 首次运行 190 秒以上无重启；再次启动后连续四次看门狗时间戳递增，状态为 `active/running`、`NRestarts=0` |
| 停止服务 | 0.513 秒返回；`enabled=0 state=released error=0`，睡眠抑制器解除 |
| 停止后电池 | `Charging`，驱动报告电流约 563 mA |
| 再次启动后电池 | `Not charging`，报告电流 0，`enabled=1 state=holding error=0` |
| 停充时 USB 输入 | `online=1`，约 9.01 V / 159 mA |

验证后手机服务保持运行，开机自启仍为 `disabled`。本地原始镜像与 sparse 镜像同步更新服务单元，默认仍不启用；手机通过 SSH 应用同一修复，无需重刷或重启。详细记录位于本地 `logs/charge-notify-fix-20260916/deploy-result.json` 和 `verify-live-result.json`。

仍需验证电量自然降到 75% 后自动恢复、从低电量充到 80% 时自动停充、USB 拔插、负载变化及长时间稳定性；最好结合 USB 功率表。SuperVOOC 和无线充电需单独实测。调研依据及原厂代码引用见 [调研报告](../artifacts/charge-limit-research-20260916/README.md)。日常取消限制只需停止服务。
