# 第三方许可与公开发行核查

核查日期：2026-10-11；补录：2026-10-12。原源码核查基点：`df9a9a186660600c56594402c4ba9fb755b5d0d2`，本次从 `5d9c277` 继续核对实际分发内容。2.3.2-rc.3 已移除 Siji 引用和三张参考图，公开发行结论仍受第 5 节的未解决事项约束。

附件原文位于 [licenses](../licenses/)，逐文件出处、上游证据提交号、转换方式及 SHA-256 见 [PROVENANCE.json](../licenses/PROVENANCE.json)。证据提交号用于固定本次查阅的上游内容，不等同于本项目最初转换字形时所用的提交号。本文不修改冻结 RC2 的二进制、清单或签名。

## 1. 项目与驱动

本项目源自 [JADE-Jerry/jcalendar](https://github.com/JADE-Jerry/jcalendar)，仓库保留 [GPLv3 原文](../LICENSE)。上游 README 同时写有「禁止商用」与 GPLv3 授权，固定证据见 [上游 README](../licenses/jcalendar-upstream-README.md)。本次核查记录该不一致，没有取得作者对两者关系的说明。

`lib/GxEPD2` 的版本信息为 1.6.4，包含本项目继承的驱动修改，版权人及文件级声明以源码为准；附件保留 [GPLv3 原文](../licenses/GxEPD2-LICENSE.txt)。不能把该目录视为未经修改的官方发布包。

`src/webserver_parsing.cpp` 为 [Arduino-ESP32 2.0.17 Parsing.cpp](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/WebServer/src/Parsing.cpp) 的项目内覆盖，保留 Ivan Grokhotkov、Hristo Gochkov 的版权及 LGPL-2.1-or-later 声明，并附 [LGPL 2.1 完整原文](../licenses/Arduino-ESP32-WebServer-LGPL-2.1.txt)。修改仅为不完整 multipart 的断开／停发退出、尾部读取等待及相应空上传处理，原 SDK 全局安装未改。对应源文件随本项目提供，出处与原始 SHA 见 PROVENANCE。

## 2. 固件库

下列附件从本地 `z21` 构建环境的已安装依赖原样提取。固定 Git 依赖以 `platformio.ini` 指定提交为准；注册表依赖以列出的安装版本为准。附件哈希固定所查阅文本，不代替固件发布清单。

| 组件 | 版本或固定提交 | 授权与附件 |
| --- | --- | --- |
| U8g2_for_Adafruit_GFX | 1.8.0 | 代码 BSD-2-Clause；部分 X11 字体另有 Adobe / DEC 声明，见 [原文](../licenses/U8g2-for-Adafruit-GFX-LICENSE.txt)；其他字体见第 3 节 |
| ArduinoJson | 7.4.3 | [MIT](../licenses/ArduinoJson-LICENSE.txt)，Benoit Blanchon |
| WiFiManager | 2.0.17 | [MIT](../licenses/WiFiManager-LICENSE.txt)，tzapu |
| OneButton | 2.6.2 | [BSD-3-Clause](../licenses/OneButton-LICENSE.txt)，Matthias Hertel |
| Nongli | `b14baf1f7d2a5643e27d783a95a4ae7d93873488` | [MIT](../licenses/Nongli-LICENSE.txt)，JADE Software Co., Ltd. |
| RX8010SJ | `fa9d6a9d37230abe4406d5ece60c18d5473e81f7`（元数据 1.0.3） | [GPLv3](../licenses/RX8010SJ-LICENSE.txt) |
| Adafruit GFX Library（间接依赖） | 1.12.6 | [BSD-2-Clause](../licenses/Adafruit-GFX-LICENSE.txt)；另保留 [README 致谢](../licenses/Adafruit-GFX-README.txt) 中要求保留的文字 |
| Adafruit BusIO（间接依赖） | 1.17.4 | [MIT](../licenses/Adafruit-BusIO-LICENSE.txt)，Adafruit Industries |
| ArduinoUZlib（构建配置保留的依赖） | `20aff95cd80c141f80bdbf66895409a0046d2c2f` | 解码器及校验代码按源文件的 zlib 类许可声明；`defl_static.c` 含 [PuTTY 来源的 MIT 声明](../licenses/ArduinoUZlib-defl-static-notice.txt)。包中 Arduino 包装层没有独立授权声明，不能把解码器授权自动套用于整个包 |
| BoundedUZlib（实际调用的受限解码器） | 由上述 ArduinoUZlib 固定提交派生 | [解码器声明](../licenses/BoundedUZlib-zlib-notice.txt)、[Adler-32 声明](../licenses/BoundedUZlib-adler32-notices.txt)、[CRC-32 声明](../licenses/BoundedUZlib-crc32-notices.txt)；原作者 Joergen Ibsen、Paul Sokolovsky 及源文件列出的其他贡献者；改动说明见 [README](../lib/BoundedUZlib/README.md) |

这份清单覆盖本项目显式库、所查阅的图形间接依赖及字形。ESP32 平台、Arduino / ESP-IDF SDK 和工具链仍保留其各自发行包中的许可、版权与源代码提供要求；本文没有将平台发行包整体重新授权。

## 3. 实际使用的字形与图标

依据 `src/screen_ink.cpp` 的字体引用和本地生成文件识别字体家族。U8g2 的代码授权不能包罗所有字体授权。

| 项目文件或符号 | 来源、版权与授权 | 本次记录 |
| --- | --- | --- |
| `src/u8g2_font_qweather_icon_16.c` | QWeather，图标 **CC BY 4.0**；图标库代码 MIT | 字形是 QWeather Icons 的子集及位图格式转换（本文件头记载 132/233 个字形），须保留 QWeather 署名、来源链接、许可链接及转换说明；[官方声明](https://github.com/qwd/Icons/blob/d7d4ef4d8334f9e70a75a088cb77804bf81daea4/README.md)、[代码 MIT](../licenses/QWeather-MIT-LICENSE.txt)、[图标 CC BY 4.0 原文](../licenses/CC-BY-4.0-legalcode.txt) |
| `include/subject_font.h` | Noto Sans SC Bold，SIL OFL 1.1 | 由 `tools/generate_subject_font.py` 转为课程名称位图子集；[OFL](../licenses/NotoSansSC-OFL.txt) 保留 Adobe 版权和 Reserved Font Name `Source`。本次以本机 Noto Sans SC `Version 2.04;241114210130;non-release`、SHA `763146584cf0710223441356b4395e279021b0806c196614377a7a0174ae074a` 逐字复现现有头文件（忽略换行差异）。[输入元数据与结果](../licenses/NotoSansSC-reproduction-metadata.json)提供可匹配输入，不声称恢复了当时实际用过的文件 |
| `u8g2_font_wqy12_t_gb2312`、`u8g2_font_wqy16_t_gb2312` | WenQuanYi Project Board of Trustees、Qianqian Fang，2004–2010；U8g2 声明 GPLv2，带字体嵌入例外 | [U8g2 官方说明](https://github.com/olikraus/u8g2/wiki/fntgrpwqy)、[12 px BDF 声明](../licenses/WenQuanYi-U8g2-12px-BDF-header.txt)、[16 px BDF 声明](../licenses/WenQuanYi-U8g2-16px-BDF-header.txt)。原作者发行包的版本 0.9.9.8 另写 GPLv2 or later，见 [12 px 原声明](../licenses/WenQuanYi-original-wenquanyi_9pt.bdf-header.txt)、[16 px 原声明](../licenses/WenQuanYi-original-wenquanyi_12pt.bdf-header.txt)；完整 [GPLv2](../licenses/WenQuanYi-COPYING.txt) 和 [嵌入例外](../licenses/WenQuanYi-original-README.txt) 一并保留 |
| `u8g2_font_fub14_tn`、`fub17_tn`、`fub20_tn`、`fub25_tn` | FreeUniversal，Stephen Wilson 2009；原 Sil-Sophia，SIL International 1994–2008；SIL OFL 1.1 | [U8g2 官方说明](https://github.com/olikraus/u8g2/wiki/fntgrpfreeuniversal)、[从上游 Bold TTF 提取的完整版权和 OFL](../licenses/FreeUniversal-OFL-from-font.txt)。保留字体内原始授权元数据，包括 Reserved Font Name `SILSophia` |
| `u8g2_font_open_iconic_all_2x_t`、`open_iconic_all_4x_t` | Open Iconic，Waybury 2014；字体 SIL OFL 1.1 | [字体许可原文](../licenses/OpenIconic-FONT-LICENSE.txt)。适用字体授权，不把项目中 SVG 图标的其他授权混用于该字形数据 |
| `u8g2_font_tenthinnerguys_tf` | TenThinnerGuys，Geoff；Public Domain | [U8g2 官方声明](https://github.com/olikraus/u8g2/wiki/fntgrpgeoff) 及 [固定版本附件](../licenses/u8g2-fntgrpgeoff.md) |
| 历史 `u8g2_font_siji_t_6x10`，当前已移除 | Siji，Stark；作者 README 与 BDF 均声明 GPLv2 | [作者 README](../licenses/Siji-README.md)、[作者 BDF 声明](../licenses/Siji-author-BDF-header.txt)、[GPLv2 原文](../licenses/Siji-GPL-2.0.txt)。U8g2 将 Siji 与 X11 6×10 字体合并；后者在 [合并 BDF 声明](../licenses/Siji-U8g2-BDF-header.txt) 中写为 Public Domain |
| `u8g2_font_helvB18_tf` | Adobe Systems Incorporated 1984–1989、1994；Digital Equipment Corporation 1988、1994 | X11 字体授权，完整文字在 [U8g2 附带 LICENSE](../licenses/U8g2-for-Adafruit-GFX-LICENSE.txt)；另保留 [官方字体家族说明](../licenses/u8g2-fntgrpadobex11.md) |

文泉驿原作者包来自 [官方 SourceForge 发布目录](https://sourceforge.net/projects/wqy/files/wqy-bitmapfont/1.0.0-RC1/)。已下载归档的 SHA-256 为 `c29b2c2f5db73ff74d11a7dca9608414813da1c4240b7b6c829a58757b35ebe6`。U8g2 保存的 BDF 与原作者包的字形区不是逐字节相同，不能仅凭相同版本号认定转换来源已完全对齐。其嵌入例外针对使用字体的文档，没有在本文中扩大解释为固件链接例外。

2.3.2-rc.3 的电量图标改用项目内 GFX 图元，原五档电压阈值保留；废弃注释中的 Siji 引用一并移除。原许可附件保留用于解释历史来源，不作为当前固件的字体组件。

Siji 作者声明没有「or later」。GPLv2 原文末尾的「如何将条款用于新程序」是供作者填写的示例，不能替代 Siji 作者的版本选择。按 [GNU 官方兼容性说明](https://www.gnu.org/licenses/gpl-faq.html#AllCompatibility)，GPLv2-only 与 GPLv3 的组合存在兼容性问题；本次未取得适用于该固件组合的额外许可或例外。

OFL 字体及其转换子集继续受 OFL 约束。项目 GPLv3 不替代 OFL；发行时一并附带对应版权、许可和必要来源说明。

## 4. 远程服务依赖与历史参考图

`remote/package-lock.json` 仅列出 `@edgeone/pages-blob` 0.0.18。已从 [npm 固定版本包](https://registry.npmjs.org/@edgeone/pages-blob/-/pages-blob-0.0.18.tgz) 提取 [package.json 原文](../licenses/EdgeOne-pages-blob-package.json)，其中声明 `license: MIT`；压缩包 SHA-512 与锁文件 `integrity` 完全一致，归档 SHA-256 见出处清单。该 npm 包仅包含 4 个文件，没有单独的 LICENSE、版权人声明或源码仓库字段。本次不补造版权人；分发该 SDK 时还需取得并附带其完整版权和许可通知。

下列三图在 2.3.2-rc.3 从当前源码删除；硬件说明改为文字，没有替换为另一组来源不明的图片。

| 历史图片 | 已知来源 | 当前处理 |
| --- | --- | --- |
| `assets/img/LOLIN32_Lite.png` | 沿用 jcalendar 工程中的开发板参考图 | 删除，不再随当前源码分发；原作者及授权仍未知 |
| `assets/img/battery_adc.png` | 沿用 jcalendar 工程中的电池 ADC 参考图 | 删除，不再随当前源码分发；原作者及授权仍未知 |
| `assets/img/flash_download_tool_guide.png` | 沿用 jcalendar 工程中的刷机参考图 | 删除，不再随当前源码分发；原作者及授权仍未知 |

记录上游仓库来源不等于取得图片的版权授权。三图原 SHA-256 在出处清单的 removedReferenceImages 中保留；删除不表示取得授权，也不追改历史冻结包。

## 5. 尚未满足的公开发行条件

1. **上游授权文字**：取得 jcalendar 作者对 GPLv3 与 README 商用表述关系的明确说明，并保存可验证的授权证据。
2. **Siji 字形：当前分发缺口已移除**。2.3.2-rc.3 不再引用该字体，电量图标直接绘制；Z15 最终 ELF 符号检查确认没有链接 Siji。历史包不因此获得新的授权结论。
3. **文泉驿来源与例外**：对齐当前 U8g2 转换字形与原作者许可版本，确认固件发行所依赖的版本选择；不能仅依赖文档嵌入例外。
4. **三张图片：当前分发缺口已移除**。已删除原文件和使用处，保留接线文字与历史 SHA；不在新源码包中附回这些文件。
5. **生成来源留档**：课程字形已补齐可精确复现的输入版本、SHA 与字体内授权元数据。QWeather 最初转换输入版本仍未恢复；保留已核的 CC BY 4.0、来源、署名与转换说明，不能把缺少历史输入版本直接等同于没有图标授权。
6. **发行包附件**：取得 EdgeOne SDK 完整版权和许可通知；如发行包纳入 ArduinoUZlib 包装层，补齐该层授权。清理上述事项后，把相应许可证、NOTICE、源代码提供方式和出处清单纳入新的公开发行包。冻结 RC2 没有因此获得新的授权结论，也没有被重新签署。

本次完成许可分类、原文附件、课程字形复现，以及 Siji／参考图的实际移除。上游授权文字、文泉驿版本和实际分发内容所需完整通知仍未解决，不能表述为「公开发行许可已全部通过」。

## 6. 待权利人确认的具体问题（未发送）

以下是可用于询问的草稿，不是授权证据，也没有向外部发送。

### jcalendar 作者

贵仓库 LICENSE 及 README 许可证章节为 GPLv3，README 又有“禁止商用”的表述。我们计划提供保留版权、修改说明和完整对应源码的公开固件衍生版本。请明确：该版本是否可以按 GPLv3 分发？“禁止商用”是使用意愿，还是另有适用的授权条件？希望取得可公开引用的说明，避免在发行文件中错误表达您的授权。

### 文泉驿／U8g2 字体维护者

当前使用 U8g2 的 wqy12／wqy16 GB2312 转换字形。其 BDF 写 GPL v2 with font embedding exception；文泉驿官方 1.0.0-RC1 的同版本 BDF 写 GPL v2 or later，字形区并非逐字节相同。请确认 U8g2 这两份 BDF 的修改出处及适用的 GPL 版本：能否按 GPLv3 提供完整对应转换来源？需要明确的版本依据，不把文档字体嵌入例外解释为固件链接例外。

### EdgeOne Pages Blob SDK 维护者

`@edgeone/pages-blob` 0.0.18 的 npm package.json 声明 MIT，但该版本归档没有 LICENSE 或版权通知。若公开云端构建包包含该 SDK，请提供适用于此版本的完整版权和许可通知，或指向其正式源码及 LICENSE。我们不会把 EdgeOne 示例项目的 MIT 文本代用为 SDK 本身的授权。

公开固件应用不含这个 Node SDK；该问题适用于实际包含 SDK 字节的云端包。最终附件及完整对应源码应按实际发行内容准备，不能由本核查文档替代。
