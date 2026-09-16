# 工作区迁移（2026-09-16）

> 历史记录：保留当时的构建与设备状态；文中的本地文件不随 Git 分发。当前操作入口见[文档索引](../README.md)。

源目录：`/home/ubuntu/oneplus-negroni`。目标仓库：`/home/ubuntu/oneplus-negroni-arch-linux`，远端为 <https://github.com/withsalt/oneplus-negroni-arch-linux>。

构建入口、脚本、配置、测试、键盘修复、调研资料和文档纳入父仓库。`linux/` 使用 <https://github.com/withsalt/linux> 子模块，固定在 `2c092365fd5c72efcedc29a578601b0a41ce45c4`；本地 `7.2`、`7.3-rc2` 分支与原有对象库均保留。迁移时 `7.3-rc2` 比本地记录的 `origin/7.3-rc2` 多两个提交，没有未提交的内核修改。两个提交另外存入 [Git bundle](../../patches/linux/README.md)，新克隆无需等待内核提交推送即可恢复。

已有的 `tools/avb` 和 `tools/msm-firmware-loader` 也按现有提交注册为子模块，保留工具来源和版本。

`downloads/`、`images/`、`logs/`、`analysis/validation/`、内核构建缓存和本地凭据一起复制到目标目录。`alarm/` 含 root 所有的受限文件（顶层权限 0555），当前无法完整复制或移动；目标目录通过符号链接复用原目录的 `alarm/`，其所有权和权限未变。它是可从下载归档重建的宿主构建缓存，不是手机最终 rootfs。凭据、认证辅助文件、镜像、日志和生成物通过 `.gitignore` 保持本地保存，不进入父仓库提交。镜像内已有的账户密码保持对应关系；`image-credentials.json` 权限保持 `0600`。原目录中的其余文件保留为迁移备份；两个工作区共享原位置的 `alarm/`，不要同时运行构建。删除原目录前，需以 root 权限迁走这个缓存或在目标目录重新解包创建它。

历史日志、报告和缓存可能记录旧绝对路径，历史记录保留原文。内核生成的 Makefile 和指向旧工作区的绝对符号链接改为目标路径；后续 Kbuild 会根据新路径更新其余生成依赖。首次续建可能重新编译部分文件。迁移不重建或刷写固件。

## 使用迁移后的工作区

```sh
cd /home/ubuntu/oneplus-negroni-arch-linux
./build.sh 7.3-rc2
```

已有子模块与构建输入已经迁入，无需重新下载。迁移检查结果保存在本地 `migration-verification.json`，包含文件比对、镜像校验和子模块引用验证。

## 从父仓库重新克隆

```sh
git clone https://github.com/withsalt/oneplus-negroni-arch-linux
cd oneplus-negroni-arch-linux
bash scripts/init-submodules.sh
```

之后按[构建指南](../building.md)安装宿主依赖、下载 rootfs 归档、创建本地镜像凭据并构建。镜像、下载缓存、历史日志和凭据不会随 Git 克隆传输。
