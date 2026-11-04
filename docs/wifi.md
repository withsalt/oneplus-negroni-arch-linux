# Wi-Fi 默认设置

WCN6855 使用主线 `ath11k_pci` / mac80211。固件构建默认安装 `iw`、`wireless-regdb` 及监管数据库签名，并应用以下配置：

| 配置 | 默认值 | 源文件 |
| --- | --- | --- |
| Wi-Fi 省电 | 关闭，适用于长期插电使用 | `config/wifi/30-negroni-wifi.conf` |
| MAC 地址 | NetworkManager `stable`，随设备和连接配置固定 | 同上 |
| 国家域 | 中国大陆 `CN` | `config/wifi/wireless-regdom` |
| 国家域应用时机 | PHY 出现时再次发送提示，覆盖仅重载 ath11k 的情况 | `config/wifi/86-negroni-regulatory.rules` |

`stable` 避免使用固件回退 MAC，但不恢复原厂 NV 地址。重新生成 machine-id、NetworkManager 主机密钥或连接配置可能改变地址；路由器的 MAC 白名单和 DHCP 绑定需要相应更新。

设备迁往其他国家或地区时，应修改 `/etc/conf.d/wireless-regdom` 并执行 `sudo set-wireless-regdom`。修改后用 `iw reg get` 同时查看 global 和 self-managed PHY 的国家域；只改变 global 不足以证明网卡已经切换。

## 已有连接

全局默认值安装到 `/etc/NetworkManager/conf.d/30-negroni-wifi.conf`。连接配置中的显式值优先；已有连接可用以下命令更新，UUID 通过 `nmcli connection show` 查询：

```sh
sudo nmcli connection modify uuid UUID wifi.powersave 2 wifi.cloned-mac-address stable
sudo nmcli general reload conf
sudo nmcli connection up uuid UUID
```

重连会中断该 Wi-Fi 上的 SSH，新 MAC 也可能获得不同的 DHCP 地址，应通过另一条维护连接操作。

## 构建与限制

`./build.sh 7.3` 为新 rootfs 和已有镜像续建安装配置及依赖，按内容指纹更新镜像，并在封装前检查配置、权限和数据库文件。内核配置启用 ath11k 用户国家域提示所需的依赖，保留监管数据库签名检查，详见[构建集成说明](build-integration.md)。Android CNSS2 / qcacld 的 INI 参数不由此驱动读取。

ath11k 将新的国家规则与固件初始规则取交集。本机切换 CN 后，2.4 GHz 仍只开放信道 1–11；路由器使用 12/13 时可能无法发现，可先固定到 1/6/11。完整恢复需继续核准板级数据和固件初始地区设置。

PMU 供电描述与 PCIe IOMMU 参数格式仍待核准；本次配置修正不代表此前偶发不可用已经根治。
