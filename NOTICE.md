# 来源与授权说明

## 来源

本项目源自 [JADE-Jerry/jcalendar](https://github.com/JADE-Jerry/jcalendar)。公开副本保留了原有固件结构、驱动、字体数据和作者版权头。

公开副本最初由原开发分支的 2.2.2 源码整理而来，包含当时尚未提交的功能；可见 Git 基点为 `8100f8f`。该基点只用于说明整理来源，各发行包的完整源码以包内 `sourceCommit` 为准。

`lib/GxEPD2` 为随上游工程保存的驱动代码，保留目录内原有许可证／作者信息。PlatformIO 拉取的 U8g2、ArduinoJson、WiFiManager、OneButton、nongli、ArduinoUZlib、RX8010SJ，以及 npm 依赖，分别保留各自授权；项目 LICENSE 不替代其授权。

`lib/BoundedUZlib` 为 ArduinoUZlib 固定提交 `20aff95cd80c141f80bdbf66895409a0046d2c2f` 中 tinf/uzlib 解码器的受限改版，源文件保留 Joergen Ibsen 与 Paul Sokolovsky 的版权和许可证。新增错误边界和符号前缀、改版来源及用途见 [库说明](lib/BoundedUZlib/README.md)；它是上游解码器的修改版；第三方许可核查范围见下文。

`src/webserver_parsing.cpp` 源自 Arduino-ESP32 2.0.17 的 WebServer 解析实现，保留原 LGPL-2.1-or-later 版权头。项目改动为不完整 multipart 的有界退出，完整许可证及对应来源见第三方许可记录和 `licenses/Arduino-ESP32-WebServer-LGPL-2.1.txt`。

2.3.2-rc.3 已移除三张授权未确认的上游硬件／刷机参考图；接线说明改为文字。历史文件的 SHA 继续留在出处清单，不将移除解释为已取得授权。

QWeather 图标版权所属 QWeather，图标采用 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)，代码采用 MIT。本项目的 `u8g2_font_qweather_icon_16.c` 是 [QWeather Icons](https://github.com/qwd/Icons) 的字形子集及位图格式转换；此转换说明不代表 QWeather 对本项目的认可或背书。

课程首页和周课表的常用课程名称位图字形由 Noto Sans SC Bold 转换为 `SUBJECT_GLYPHS`，保留 SIL OFL 1.1。周课表未知课程回退、倒计时名称与其他正文使用现有文泉驿点阵字库。FreeUniversal 和 Open Iconic 字体也受 OFL 约束。当前电量图标由 GFX 图元直接绘制，已移除 Siji 引用；其原许可附件只作为历史证据保留。WenQuanYi、TenThinnerGuys、Helvetica 的授权分别处理，不能用 U8g2 代码的 BSD 授权概括所有字形。

## 许可证和未完成的核查

仓库 [LICENSE](LICENSE) 沿用原项目的 GPLv3 原文。2026-10-13，维护者确认采用 GPLv3，取消此前个人学习／非商业使用的偏好，不附加非商业限制。上游 README 中的额外商用限制按 [GPLv3 第 7 节](https://www.gnu.org/licenses/gpl-3.0.html#section7) 处理；本项目保留上游版权和来源，不将此处理表述为取得作者的新授权。各第三方组件仍分别遵守其原许可。

2026-10-11 已补齐 [第三方许可分类与核查记录](docs/THIRD_PARTY_LICENSES.md)、[许可原文附件](licenses/) 和 [出处及 SHA-256 清单](licenses/PROVENANCE.json)。保留原作者声明与各组件自己的许可。

当前 rc.4 固件包及对应源码包已附带实际分发组件的版权、许可与来源材料；包内容见 [发行源码说明](docs/RELEASE_SOURCE.md)。文泉驿转换字体的 GPL 版本差异仍未澄清，维护者于 2026-10-13 决定跳过进一步确认；跳过不等于取得新的许可证据。Siji 与三张参考图已移除，课程字形的可复现输入已补齐。EdgeOne SDK 的完整通知要求适用于包含其字节的云端包，当前发行物不含该 SDK。各项证据及尚未确认的范围见第三方许可记录第 5 节；历史冻结包、清单和签名保持。

## 节气日期数据

`include/solar_term_data.h` 的日期事实整理自 [香港天文台公历与农历日期对照表](https://www.hko.gov.hk/en/gts/time/conversion.htm)，范围为 2025–2099 年，于 2026-10-17 取得。原始逐年文本为 `https://www.hko.gov.hk/tc/gts/time/calendar/text/files/T{year}c.txt`；时区为 UTC+8。远期临近午夜的节气日期采用该表公布值。
