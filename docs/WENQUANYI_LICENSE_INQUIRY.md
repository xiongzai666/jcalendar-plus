# 文泉驿转换字体许可询问草稿

日期：2026-10-13。状态：**已取消发送**。维护者决定跳过文泉驿转换字体的 GPL 版本确认；保留此草稿为历史记录，本文不是维护者回复或授权证据。

拟发布位置：`olikraus/u8g2` 的 GitHub issue。只询问字体 GPL 版本，不再向 jcalendar 作者发送商用限制询问。本次不发布此询问；不继续等待发送授权或维护者回复。

## 标题

Clarify GPL version of WenQuanYi wqy12/wqy16 font data for GPLv3 firmware

## 正文

Hello, we are preparing the corresponding source and license notices for an open-source GPLv3 ESP32 firmware that uses `u8g2_font_wqy12_t_gb2312` and `u8g2_font_wqy16_t_gb2312` through U8g2_for_Adafruit_GFX 1.8.0.

We found the following source and license records:

- The [official font wiki](https://github.com/olikraus/u8g2/wiki/fntgrpwqy) points to `larryli/u8g2_wqy`. Its [build script](https://github.com/larryli/u8g2_wqy/blob/34d1bf7e054ef5a331c45248cd238edc49498897/build.sh) maps the 9 pt and 12 pt BDFs to wqy12 and wqy16 GB2312.
- The BDF headers in both that repository and U8g2 say “GPL v2 with font embedding exception.” Reviewed U8g2 sources: [9 pt](https://github.com/olikraus/u8g2/blob/d6c8499c5f2707cac8eccd09fd8f677d12b17977/tools/font/bdf/wenquanyi_9pt.bdf) and [12 pt](https://github.com/olikraus/u8g2/blob/d6c8499c5f2707cac8eccd09fd8f677d12b17977/tools/font/bdf/wenquanyi_12pt.bdf).
- WenQuanYi's [official 1.0.0-RC1 release](https://sourceforge.net/projects/wqy/files/wqy-bitmapfont/1.0.0-RC1/) contains the same stated font version, 0.9.9.8, but its README and BDF notices explicitly specify GPL version 2 **or later**, with the document embedding exception.
- We found your [BDF boundary corrections discussion](https://github.com/larryli/u8g2_wqy/issues/1) and [confirmation that the fonts were fixed in U8g2](https://github.com/larryli/u8g2_wqy/issues/1#issuecomment-357793435). Comparing the fixed repository versions above by encoding, the 9 pt files have 16 differing glyph blocks and the 12 pt files have 109; this identifies changes, not their licensing terms. These are reviewed source snapshots, not proof of the original generator inputs for the installed font arrays.

Could you clarify:

1. Is “GPL v2” in the U8g2 WenQuanYi BDF headers shorthand for the original **GPLv2-or-later**, or is it intended to mean **GPLv2-only**? If the former, could the notice be made explicit or a canonical license record be linked?
2. What GPL version applies to the BDF corrections and font conversion/mapping contributions used for these two fonts? Is there a source/notice establishing that the combined font data may be distributed with GPLv3 firmware?

We are keeping the original copyright and license notices and preparing corresponding source. We are not treating the MIT license of the conversion-script repository as a replacement for the font license, or treating the document embedding exception as an exception for firmware linking. Thank you for helping us record the applicable terms accurately.

## 发布回执

未发布，无 issue URL；2026-10-13 按维护者决定取消。GPL 版本尚未确认。
