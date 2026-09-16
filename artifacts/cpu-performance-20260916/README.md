# CPU 性能与调度检查（2026-09-16）

设备：OnePlus 10 Pro / Negroni，SM8450，运行 `7.3.0-rc2-sm8450+`。用户已安装外部散热器。认证使用项目本地凭据文件，报告不包含凭据。

完整复现入口：[CPU 性能与调度指南](../../docs/cpu-performance.md)。本报告保存实测数据与当时操作记录；通用构建和部署步骤以指南为准。此后两项优化已进入默认固件构建，下面关于尚未集成的描述反映最初现场部署时的状态。

默认集成后的构建及镜像校验摘要：[default-build.json](default-build.json)。默认构建已验证，新完整镜像未再次刷入手机。

## 已在手机应用

安装并启用 `negroni-cpu-performance.service`：

- 全局 CPU Boost 从 `0` 改为 `1`，超大核 policy7 的 Boost 随之启用。
- 三组 CPU 的 governor 从 `schedutil` 改为 `performance`。
- `scaling_max_freq` 设为启用 Boost 后各 policy 的 `cpuinfo_max_freq`。
- 启动时保存原 governor、频率上下限、全局/各 policy Boost；停止服务恢复该快照。已在手机实际启动、停止，并逐字段比较恢复结果与原始快照，完全一致。
- 已于 2026-09-16 写入大小核设备树修复并重启，确认性能服务自动启动、Boost 和三组 `performance` 生效。

| 核心 | 核数 | 修改前频率上限 | 修改后频率上限 | 90 秒全核负载的硬件频率读数 |
| --- | ---: | ---: | ---: | ---: |
| Cortex-A510，CPU0–3 | 4 | 1.7856 GHz | 1.7856 GHz | 1.7856 GHz |
| Cortex-A710，CPU4–6 | 3 | 2.4960 GHz | 2.4960 GHz | 主要为 2.4192 GHz |
| Cortex-X2，CPU7 | 1 | 2.8416 GHz | 2.9952 GHz | 2.8416 GHz |

单核测试中 CPU7 的硬件频率读数达到 2.9952 GHz。此为解锁芯片原有 Boost 档位，不是增加硬件频率表之外的超频档位。

手机上的文件：

- `/usr/local/sbin/negroni-cpu-performance`
- `/etc/systemd/system/negroni-cpu-performance.service`
- `/var/lib/negroni-cpu-performance/original-20260916.json`：首次修改前的原始配置。
- `/run/negroni-cpu-performance/state.json`：当前这次服务启动的恢复快照。

查看与恢复：

```sh
sudo /usr/local/sbin/negroni-cpu-performance status
sudo systemctl disable --now negroni-cpu-performance.service
```

恢复后，本次实测为三组 `schedutil`、Boost 关闭，频率上下限回到原值。再次启用：

```sh
sudo systemctl enable --now negroni-cpu-performance.service
```

已部署版本的脚本整理到 [config/cpu-performance](../../config/cpu-performance/negroni-cpu-performance)，服务位于 [config/systemd](../../config/systemd/negroni-cpu-performance.service)。本次为手机现场配置：性能服务尚未集成到项目 rootfs 构建默认值；大小核修复已写入手机 `vendor_boot_a`，本地 `images/7.3-rc2/` 的完整固件产物仍是修改前版本，后续重刷旧 `vendor_boot.img` 会覆盖本修复。

## 基准与温度

OpenSSL 3.6.4，SHA-256，16384 字节数据块，`-elapsed` 按实际时间计数。单线程固定在 CPU7，全核使用 `-multi 8`。测试期间每约 1.5 秒记录频率、温度、cooling state。不是通用整机性能评分。

| 测试 | 原配置 | 性能配置 | 变化 |
| --- | ---: | ---: | ---: |
| CPU7 单线程，15 秒 | 1784.61 MB/s | 1878.08 MB/s | +5.24% |
| 8 线程，15 秒 | 6841.50 MB/s | 6846.03 MB/s | +0.07%，可视为无明显变化 |
| 8 线程，90 秒 | 未测 | 6846.16 MB/s | 稳定完成，退出码 0 |

90 秒全核测试最高 CPU 温度 **83.1°C**，电池最高 **35.7°C**，未触发测试停止条件（CPU 85°C / 电池 43°C）。所有 CPU cooling device 在采样中均为 `cur_state=0`。原有热区及临界温度保护保持生效。以上仅覆盖短时 SHA-256 负载，不代表更长时间或其他负载的稳定性保证。

整理后的数据：[benchmark-summary.json](benchmark-summary.json)。原始采样、初始状态、部署/恢复记录保存在本地 `logs/cpu-performance-20260916/`。

## 限制来源

1. **CPU Boost 确实默认关闭**，本次已解除；`performance` 消除按负载升频的等待。
2. **未发现 CPU 配额或关核限制**：8 核在线，检查到的 cgroup `cpu.max` 都是 `max 100000`，根 cpuset 为 `0-7`，内核未启用 `CONFIG_UCLAMP_TASK`。不存在需要解除的 uclamp 上限。
3. **未发现正在运行的 Android thermal-engine、TLP、tuned、power-profiles-daemon 或独立 CPU powercap 接口**。当前运行的是主线风格 Arch Linux，而非 Android 的厂商性能策略栈。
4. **硬件/固件约束依然存在**：`performance` 请求 2.496 / 2.9952 GHz 时，全核负载的 `cpuinfo_cur_freq` 主要为 2.4192 / 2.8416 GHz。驱动通过 EPSS/LMH 状态读取硬件给出的频率，Linux CPU cooling state 为 0。不能据此认定具体是电流、功耗、固件温控还是其他硬件策略；关闭 Linux 温控也不能保证解除这一层限制。未发现已验证的运行时功耗墙解除接口。
5. **修改前调度器缺少异构能力信息，现已修复**：原 CPU0–7 的 `cpu_capacity` 全为 1024，原始源码及设备树缺少 `capacity-dmips-mhz` 与 `dynamic-power-coefficient`。应用设备树并重启后，小核 / 大核 / 超大核正确识别为 276 / 849 / 1024，三组 Energy Model 注册成功。原配置的 `sched_energy_aware` 读取为空；内核在不满足 EAS 条件时采用这一行为，不能把空值当作一个已启用开关。

因此，当前已开启可用的最高软件性能策略，但不等于持续全核达到标称最高频率，也不能称为彻底解除硬件功耗限制。

## 大小核调度修复（已写入并重启验证）

依据一加公开的 SM8450 / waipio 设备树：

[OnePlusOSS waipio.dtsi，固定提交 877203a7cf9b79973d44a9db716490f95d820110](https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_sm8450/blob/877203a7cf9b79973d44a9db716490f95d820110/kernel_platform/qcom/proprietary/devicetree/qcom/waipio.dtsi)

| 核心 | capacity-dmips-mhz | dynamic-power-coefficient |
| --- | ---: | ---: |
| CPU0–3 | 1024 | 100 |
| CPU4–6 | 2253 | 251 |
| CPU7 | 2386 | 396 |

已应用的设备树改动对应 [sm8450-cpu-capacity.patch](../../patches/linux/sm8450-cpu-capacity.patch)。参数按单位频率的能力填写，内核结合启动时的最大频率归一化。重启后实测得到 **276 / 849 / 1024** 三档 capacity，与预期一致。三组 CPU 的 Energy Model 已出现于 `/sys/kernel/debug/energy_model/cpu{0,4,7}`，具有非零功耗表；`performance` governor 下不宣称 EAS 一定启用。

此次应用的构建文件保存在本地：

- `logs/cpu-performance-20260916/candidate/sm8450-oneplus-negroni.dtb`
- `logs/cpu-performance-20260916/candidate/vendor_boot-cpu-capacity.img`
- 镜像 SHA-256：`203b7d18ae310cbb576b88962062e1d500b555aef04da5275fb33750109cb554`

验证已完成：

- `git apply --check` 通过，补丁可应用到当前固定内核源码。
- 使用与现有 DTB 相同的内核 DTC 编译成功。
- 逐节点、逐属性比较二进制设备树：原有属性全部相同，恰好新增 8 核的 16 个能力/功耗属性。
- `vendor_boot` header 除 DTB 字节长度字段外完全相同，无其他非零区段。候选镜像与原镜像均为 180224 字节。
- 手机 `vendor_boot_a` 的原始镜像前缀与本地当前固件逐字节一致。
- 已备份 `vendor_boot_a` 前 1 MiB（覆盖候选镜像全部写入区域）到本地 `logs/cpu-performance-20260916/vendor_boot_a-before-first-1MiB.img`。

用户确认应用及重启后，于 2026-09-16 22:12（北京时间）写入 Linux 的 `vendor_boot_a`（`/dev/sde24`）前 180224 字节。写入前再次确认分区前 1 MiB 与备份相同；写入后逐字节回读候选镜像，并确认后续字节保持原值。备份同时保存于手机 `/var/lib/negroni-cpu-performance/vendor_boot_a-before-first-1MiB.img`。随后正常重启，通过 SSH 重连，boot ID 确认已改变。

重启后验证：

- 8 核在线，设备树的全部 16 个新增属性正确，capacity 为 276 / 849 / 1024。
- `negroni-cpu-performance.service` 为 enabled / active，Boost 为 1，三组 governor 为 `performance`。
- SDDM、GPU runtime PM 规避服务、充电限制服务正常启动；SSH 和 Plasma 桌面进程可见。
- 单核及 8 线程 SHA-256 各 15 秒均以退出码 0 完成；吞吐分别为 1825.99 / 6719.93 MB/s，全核最高 CPU 温度 75.6°C、电池 33.7°C，CPU cooling state 均为 0。
- 本轮单核测试的 15 次采样中，CPU7 14 次为 2.8416 GHz、1 次为 2.9952 GHz。频率仍取决于硬件/固件约束，因此这次调度修复不被描述成额外的固定吞吐提升。
- 计算任务先限制于 CPU0，1 秒后放开到 CPU0–7；2 秒以后的 100 次采样全部位于 CPU7，验证负载可以迁移到更高能力的 CPU。该测试验证迁移行为，不是修复前后的定量调度性能对比。
- 发现的 `msm-modem-uim-selection.service` 失败在前一次启动也存在；本次日志为无 SIM，前次为无可用 modem。此服务不在 CPU 修复范围内。

本地内核工作树未直接改动，补丁和构建输入保存在归档及 `logs/`。原始应用、回读、重启后状态及测试记录见 `logs/cpu-performance-20260916/apply-device-tree.txt`、`verify-after-reboot.txt`、`post-reboot-benchmark.txt` 和 `migration.txt`。

整理为可复现流程时，用 [独立构建脚本](../../scripts/build-cpu-performance-dtb.py) 从固定 Git 对象和仓库补丁重新编译，原始及优化后的镜像 SHA-256 均与现场版本完全一致；没有依赖先前的 `linux/output/` 或本地候选文件。构建记录见 [reproduction-build.json](reproduction-build.json)。

### 如需恢复原设备树

以下在手机上执行，会恢复此前备份的原设备树；最后一步会重启。此操作独立于性能服务的启停。

```sh
sudo sha256sum /var/lib/negroni-cpu-performance/vendor_boot_a-before-first-1MiB.img
# 应为 3bbdb69c75de15a0af0dd15e0b36e77a919c7271d417b3a2cc33904acb30eb15
sudo dd if=/var/lib/negroni-cpu-performance/vendor_boot_a-before-first-1MiB.img \
  of=/dev/disk/by-partlabel/vendor_boot_a bs=1M count=1 conv=fsync,notrunc
sudo reboot
```

手机无法启动时，修改前的原始 `images/7.3-rc2/vendor_boot.img` 仍可在 bootloader fastboot 下刷回 `vendor_boot_a`；应先核对设备和槽位，按项目刷机说明操作。
