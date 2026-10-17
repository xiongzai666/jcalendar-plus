# 发行包许可证与对应源码

本项目采用 GPLv3，不附加非商业限制。第三方字体、库和平台按各自许可处理，见包内 `NOTICE.md`、`docs/THIRD_PARTY_LICENSES.md` 与 `licenses/PROVENANCE.json`。

## 源码获取与编译

每个驱动包的 `manifest.json` 记录完整 `sourceCommit`。同次 GitHub 预发布提供 `jcalendar-plus-2.3.2-rc.6-source.zip`，包含该提交的项目源码、编译和安装工具、许可附件及本次解析的固件库源码。也可在 [公开仓库](https://github.com/xiongzai666/jcalendar-plus) 按完整提交检出；不要用旧版源码对应新版二进制。

源码包根目录的 `SOURCE_MANIFEST.json` 记录项目提交、逐文件 SHA-256 和第三方来源。`third_party/libraries/` 保留构建中解析的库源码和原始声明；检入驱动、受限解码器及项目内修改直接位于 `lib/` 和 `src/`。ArduinoUZlib 未声明授权的包装层不随源码包补发；固件实际使用的解码器对应源码在 `lib/BoundedUZlib`。各平台依赖的名称、版本、来源和元数据哈希另在驱动包 manifest 中记录。

依照 [维护指南](../MAINTENANCE.md) 安装 Python、PlatformIO 6.2.0 和所选环境依赖后编译。源码包附带的库可供核查和复现；`platformio.ini` 仍按固定版本／提交获取依赖，不改现有编译方式。Arduino-ESP32 对应 2.0.17，实际 PlatformIO 框架包版本以 manifest 为准；已安装框架源码与构建脚本另附于 `third_party/framework-arduinoespressif32/`、`third_party/platform-espressif32/`。官方对应平台源码及许可见 [Arduino-ESP32 2.0.17](https://github.com/espressif/arduino-esp32/tree/2.0.17)、[ESP-IDF 4.4.7](https://github.com/espressif/esp-idf/tree/v4.4.7)；ESP-IDF 和工具链分别遵守自身声明，不能用本项目许可证替代。

固件仅限经典 ESP32、4 MB Flash 和对应 400 × 300 三色屏。首次刷机按包内 `FLASHING.md` 操作；保留配置升级及串口回退先读 [刷机指南](FLASHING.md)。不提供用户 NVS、私密凭据、全 Flash 备份或个人课程配置。

## 本次许可处理与证据范围

- 项目 GPLv3 政策已确认；原作者及各组件版权保留。
- 文泉驿转换字体的 GPLv2／GPLv2-or-later 差异按维护者决定跳过进一步确认，已记录但未澄清。原字体和转换库声明随包提供，不称取得额外授权。
- 本次不分发编译后的云端包或 EdgeOne SDK 字节。项目源码包含 `remote` 的源码和锁文件；自行安装 SDK 时由对应包提供其许可，源码包没有代造 SDK 版权通知。
- Z15 复用有效首配／OTA／离线证据、rc.3 指定异常网络和电量验证，以及 [10 月 13 日 OTA 中止恢复](OTA_RECOVERY.md) 实测。倒计时及升级缓存已有定向验证；rc.6 原生 16 像素长天气、所选粗体周课表和天气提示经过真实字体及绘图回归，同显示方案的私有 Z15 已由用户确认视频验收完成，此公开 rc.6 镜像尚未安装实机。其他驱动只有构建验证；未承诺全天稳定性或续航。旧候选的紧邻 GET 重置记录保留，不能代替修复后的恢复证据。
