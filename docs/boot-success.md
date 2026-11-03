# A/B 启动成功标记

ABL 会消耗未标记成功槽位的启动重试次数，即使 Linux 曾正常运行，也可能在后续重启时进入 recovery 或改选另一槽。`negroni-boot-success.service` 在图形系统、SSH 和 NetworkManager 就绪后标记已确认的 Linux 槽位成功。

## 设备配置

先按[刷机说明](flashing.md)确认固件升级历史和实际 Linux 启动槽。服务仅支持已确认 ColorOS 14、从未升级到 15 的 Negroni；曾升级到 OOS/COS 15.0.0.1302+ 的 qbootctl 限制继续适用。

全新构建和增量更新均安装并启用服务，但不生成设备确认配置。使用 `sudoedit /etc/negroni-boot-success.json` 创建配置，将 `slot` 设为核实后的 `a` 或 `b`：

```json
{
  "firmware_history": "coloros14-never15",
  "slot": "a"
}
```

不要直接沿用示例槽位。配置必须为 root 所有的普通文件，且其他用户不可写：

```sh
sudo chown root:root /etc/negroni-boot-success.json
sudo chmod 600 /etc/negroni-boot-success.json
sudo systemctl enable --now negroni-boot-success.service
sudo journalctl -b -u negroni-boot-success.service
sudo qbootctl
```

确认 Linux 槽的 `Active=1 / Successful=1 / Bootable=1`。更新安装器保留已有配置；切槽或更改启动布局后需重新核对。

## 标记条件

服务校验设备身份、配置权限、用户空间服务和两槽状态。已有成功位时跳过写入；未配置时跳过服务。

写入前核对 bootloader 的 `slot_suffix`。若固件没有提供该属性，则读取两个 boot 分区，要求运行内核的完整版本字符串唯一匹配配置槽。槽位不一致、两个槽使用相同内核或无法识别时拒绝写入。

服务只调用 `qbootctl -m` 标记成功并读回核验，不切换槽位。失败信息保留在 journal 中，应在继续重启前解决配置或识别问题，避免耗尽重试次数。
