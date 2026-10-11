# 来源与授权说明

## 来源

本项目源自 [JADE-Jerry/jcalendar](https://github.com/JADE-Jerry/jcalendar)。公开副本保留了原有固件结构、驱动、字体数据和作者版权头。

本次整理输入为原项目开发分支在 2.2.2 状态的当前源码（包含此前尚未提交的功能），其可见 Git 基点为 `8100f8f`。该基点不是本公开副本的完整功能提交号，正式发布包以自己的 `sourceCommit` 为准。

`lib/GxEPD2` 为随上游工程保存的驱动代码，保留目录内原有许可证／作者信息。PlatformIO 拉取的 U8g2、ArduinoJson、WiFiManager、OneButton、nongli、ArduinoUZlib、RX8010SJ，以及 npm 依赖，分别保留各自授权；项目 LICENSE 不替代其授权。

`lib/BoundedUZlib` 为 ArduinoUZlib 固定提交 `20aff95cd80c141f80bdbf66895409a0046d2c2f` 中 tinf/uzlib 解码器的受限改版，源文件保留 Joergen Ibsen 与 Paul Sokolovsky 的版权和许可证。新增错误边界和符号前缀、改版来源及用途见 [库说明](lib/BoundedUZlib/README.md)；不能将其表述为原创解码器或已完成全部第三方授权核查。

`src/webserver_parsing.cpp` 源自 Arduino-ESP32 2.0.17 的 WebServer 解析实现，保留原 LGPL-2.1-or-later 版权头。项目改动为不完整 multipart 的有界退出，完整许可证及对应来源见第三方许可记录和 `licenses/Arduino-ESP32-WebServer-LGPL-2.1.txt`。

`assets/img` 中的三张硬件／刷机参考图沿用原项目；已知上游仓库来源，尚未确认原作者、原始出处与素材授权。不能据此推定三图已取得 GPLv3 授权。

QWeather 图标版权所属 QWeather，图标采用 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)，代码采用 MIT。本项目的 `u8g2_font_qweather_icon_16.c` 是 [QWeather Icons](https://github.com/qwd/Icons) 的字形子集及位图格式转换；该转换说明不表示 QWeather 对本项目的认可或背书。

课程名称位图字形由 Noto Sans SC Bold 转换为 `SUBJECT_GLYPHS`，保留 SIL OFL 1.1。FreeUniversal 和 Open Iconic 字体也受 OFL 约束。WenQuanYi、Siji、TenThinnerGuys、Helvetica 的授权分别处理，不能用 U8g2 代码的 BSD 授权概括所有字形。

## 许可证和未完成的核查

仓库 [LICENSE](LICENSE) 是原项目的 GPLv3 原文，没有加入新的禁止商用条款。维护者希望个人学习使用，此意愿不是许可证修改，也不代表已获全部第三方素材的新授权。

2026-10-11 已补齐 [第三方许可分类与核查记录](docs/THIRD_PARTY_LICENSES.md)、[许可原文附件](licenses/) 和 [出处及 SHA-256 清单](licenses/PROVENANCE.json)。保留原作者声明与各组件自己的许可。

公开发行仍需解决上游 README 商用表述与 GPLv3 的关系、Siji 字体的 GPL 版本兼容性、文泉驿转换来源与许可版本、三张参考图的授权，以及生成字形的输入留档和发行包通知。详细证据与处理条件见核查记录第 5 节。本说明不能作为公开发行许可已经全部通过的证明；冻结 RC2 的签名与发布清单未因本次文档整理改变。
