# 公开候选构建与验收

本文件覆盖 A01、A02 与 A13 的公开部分。`2.3.1-rc.1` 候选在本地准备；未部署到 EdgeOne、未公开发布、未刷机。主机测试不能证明按键、屏幕、NVS、OTA 或功耗表现。已有公开 `v2.3.0` 下载仍对应旧版，不能认为已更新为本候选。

## 版本与来源

公开源码盘点基线为 `d7999584477f9d6e562e1f8fb5fbeda162fe3e65`，私有对比基线为 `e8f80c16b1c90b3e0946d16041f41b90df9cca6b`。公开初始化保留已有配置；私有域名、学校数据、Radar、账号与分钟天气没有回移。

来源提交与各文件 SHA256 优先于目录名或版本号。网页演示更新不等于固件升级。当前设备、生产网页与真实 Blob 服务来源需要维护者另行确认；本地候选不证明生产已经使用该版本。

| 部件 | 本地证据 | 兼容与回退 |
| --- | --- | --- |
| 手机页面与云函数 | 清洁快照、CLI 编译、完整入口隔离验收、静态资源散列、冻结 ZIP | 一人一设备；回退网页/函数前核对合同，独立日期作息需 schema 2 |
| 四个固件驱动 | 每驱动编译输出、分区核对、段 SHA、依赖来源 | 经典 ESP32、4 MB Flash；保持相同分区并使用本机 Update；禁止盲写 app0 |
| GitHub Pages 演示 | 生成模块导入、浏览器存储与模拟状态测试 | 独立浏览器数据；不连接云端、设备或天气服务 |
| 实际部署、设备与备份 | 尚无本轮操作证据 | 另行记录部署 ID、源码提交、设备型号、旧分区与私密备份位置；不公开备份 |

## 真实云端编译产物

`npm run build` 仍是静态和语法检查。发布前额外执行实际编译验收：

```sh
npm ci --prefix remote
# 安装或指定固定版本 EdgeOne CLI 1.6.41 的 edgeone-bin/edgeone.js
export EDGEONE_CLI_ENTRY=/absolute/path/node_modules/edgeone/edgeone-bin/edgeone.js
npm run build:candidate --prefix remote
```

Windows 可用 PowerShell 设置 `$env:EDGEONE_CLI_ENTRY`。脚本使用当前 Node 的目录执行子进程 npm；海外依赖默认经 `http://127.0.0.1:7897`，loopback 不代理。无本地代理的环境显式设置空 `HTTP_PROXY` 和 `HTTPS_PROXY`。

输出位于忽略的 `.releases/public-candidates/edgeone-*`：`receipt.json`、`artifact.zip`、`build.log` 和清洁源码/编译快照。收据包含来源提交、是否存在未提交改动、源文件散列、Node/CLI/Blob SDK 版本、编译产物散列和 ZIP SHA。源码变动或增删文件会拒绝签发候选；ZIP 改动会拒绝散列验证。

脚本不读取工程 `.env`，不继承服务认证变量、不复制设备数据、不上传。该 ZIP 的收据明确 `fixtureOnly: true`、`deployable: false`，仅供编译验收，不能直接替代带自己生产配置的正式部署。格式为 CLI 所需的 `remote/.edgeone/*` 与 `remote/edgeone.json`。

验收运行完整 CLI server entry，包含 Node 路由包装与真实 `@edgeone/pages-blob` SDK。仅 HTTP 监听与 Blob 传输被隔离到本地内存 fixture，其他网络请求禁止。依次验证登录、未授权拒绝、保存/存储、旧 revision 冲突、最多六条待办投影、已完成项排除、schema 1/2 与 426、设备确认、状态与历史权限、历史恢复产生新 revision 且旧版本保留。此证据没有覆盖平台实际 Blob 服务、预览域名、跨进程一致性或分布式写入。

## 固件构建与包

```sh
python -m pip install platformio==6.2.0
python tools/verify.py --environment z15
# 先提交已验证源码，工程必须干净；runner执行四驱动并记录输入/输出SHA
python tools/build_firmware.py
# Windows中文路径可为上一步传入 --project-dir ASCII_JUNCTION
# boot_app0路径来自本机已安装框架
python tools/release_bundle.py --boot-app0 /path/to/framework-arduinoespressif32/tools/partitions/boot_app0.bin
```

各包仅包含 bootloader、partitions、boot_app0、firmware、刷写说明、manifest 和 SHA256SUMS。打包会拒绝缺少或超大段，以及 app0/app1 地址或容量不符的分区表。两应用分区均 `0x1e0000`（1,966,080 字节）；manifest 的 `ota.imageFraction` 是应用文件占比，`deviceUpgradeVerified: false` 表示 OTA 未实测。

runner 在提交后的同一工作树构建四驱动，核对构建前后的固件源码输入与 HEAD，并生成 `.pio/build/build-receipt.json`。打包必须同时匹配该收据的提交、当前 include/src/lib/PlatformIO 配置散列与各段内容，拒绝把旧二进制署名为新 HEAD。全套 `verify.py` 可先构建/验证未提交修复；最终收据则要求提交后重新执行 runner。

本轮 Windows 链接器在包含中文的工程路径为 `1680` 写入 `firmware.map` 时返回 `Invalid argument`，不能算四驱动通过。修复方式是创建指向同一隔离工作树的临时 ASCII junction，再使用 `platformio run --project-dir ASCII_PATH` 构建；没有复制或覆盖其他工程。最终门禁要求四驱动均成功，仍失败则停止签发该驱动包。Linux CI 不需要这个路径别名。

依赖收据保留 `.piopm` 的解析名称、版本、来源与元数据 SHA，PlatformIO 配置散列和检入库逐文件 SHA。默认 PlatformIO home 为 `~/.platformio`，可指定 `--platformio-home`。工具收据仅选取本项目的 espressif32 平台、Arduino 框架、ESP32 工具链与 esptool，库收据仅选取该驱动目录，使用范围同时核对构建日志中的依赖图。当前直接依赖固定版本/提交，GFX 和 BusIO 等传递依赖仍需结合收据复现，不能声称完整锁定所有上游解析。

## 公共差异筛选

| 差异/私有来源 | 公开源码核实与决定 | 验证范围 |
| --- | --- | --- |
| 清洁首启与模板 | 保留公开实现，不能被个人派生默认覆盖 | public_defaults host 与云端测试；空 NVS 实机待验 |
| 编译包冻结与入口验收 | 采用等价公共工具；排除私有站点、凭据、部署/回退 ID | 冻结散列/路径/配置篡改测试、实际 CLI 完整入口 |
| 网络截止，`66cc117` | 采用有界主 DNS、逐连接 TCP/TLS 时间片、过期连接拒绝；修复原 DeadlineClient 到期仍调用 connect 的回归 | rollover、晚期时间片、慢正文/头、DNS host 与四驱动编译；真实网络时长待验 |
| 天气资源，`7f12c9c` | 采用协作取消、任务启动门控、配网页排除；原外部 vTaskDelete 不展开 C++ 析构 | 取消后读写/连接拒绝、任务创建失败/活动任务保留、已有 owned_task 清理顺序；物理按键/堆待验 |
| gzip 资源，`7f12c9c` | 采用独立 BoundedUZlib 与保留版权的源码。原依赖在短包、错误头和分配失败时 exit，且先按不可信 ISIZE 分配 | 同一检入解码器：截断、伪造尺寸、CRC、空包、输出上限、重复解码、非法符号索引 |
| 全唤醒规划/报告预留 | 暂缓。公开已有课程、定时/重试与天气唤醒语义；私有后续同时依赖分钟天气/Radar，本轮不整套回移 | 保留公开原策略；全天实机周期与功耗为后续门禁 |
| 草稿/待办恢复 | 本轮手机改进由对应产品测试覆盖；无私人存储键/个人默认回移 | 会话恢复、版本冲突与待办编辑/草稿测试 |
| Radar 第四页、双槽缓存与 VPS 账号 | 不适用公开固定待办页；不据 Radar 的 NVS 故障推断公开天气/待办也有同样缺陷 | 不进入候选 |

BoundedUZlib 来源为原固定 ArduinoUZlib 提交 `20aff95cd80c141f80bdbf66895409a0046d2c2f` 的私有项目受限改版；源文件保留原许可与版权。它新增符号前缀、错误返回与输入耗尽检查。没有加入 Radar/分钟天气库或个人输入。

## 实机验收与回退记录

每项都记录日期、设备/屏幕型号、来源提交、固件 SHA、步骤、实际结果和保留的原配置。下表当前均为待验。

| 场景 | 具体操作和通过条件 | 回退/数据边界 |
| --- | --- | --- |
| 空 NVS 首次成功 | 在专用空白设备进入热点，随机密码可见；配 Wi-Fi、自己的 Token/根证书；填写网页模板并同步；四页显示本人设置 | 擦除仅限专用空白设备；不擦日常设备。清洁模板不等于实测首启 |
| 同分区 OTA | 导出配置并保留旧固件；本机 Update 上传匹配驱动 firmware.bin；确认实际启动版本、配置保留、设备 ack 与屏幕内容 | 使用相同分区旧 firmware.bin 在本机 Update 回退；不把串口 0x10000 当作 OTA 活动分区；不擦 NVS |
| OTA 中断/错包 | 专用测试设备验证超大/错误驱动/传输中断结果；确认可启动与可恢复 | 保留可串口恢复的旧同分区包；本轮仅准备，不实施断电或刷写 |
| 配网时取消天气 | 慢网络下双击进入配置；重复执行并观察任务、TLS、堆与热点可用性；取消超时必须延迟打开而不并发启动天气 | 原配置保留；清理未完成时再次尝试，不强杀任务、不清设置 |
| 离线/备用网络 | 断主网、验证备用；断所有网络确认缓存标识；恢复后检查 revision/回报与屏幕 | 不删除课表/Token；恢复原 Wi-Fi 设置 |
| 四驱动物理显示 | 分别核对对应控制器、上下键、四页顺序、字体、颜色、BUSY、睡眠与唤醒 | 错驱动可编译但显示可能不对，停止该驱动验收并恢复已知匹配固件 |
| 全天资源与功耗 | 对比同设备/电池/网络条件 24 小时运行，记录唤醒次数、DNS/TLS截止、空闲/任务栈/最小堆和实际电量/电流 | 使用已知固件恢复；没有主机测试可替代此证据 |

云端恢复历史只恢复用户配置和待办，不代表回退固件、Token、Wi-Fi、设备 NVS 或物理屏幕。生产回退需使用维护者已核实的部署记录，不能从私有对比工程复制部署 ID。
