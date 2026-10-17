"""Native 20px Medium subset; unknown custom text keeps the complete U8g2 font."""
from pathlib import Path
import argparse
from PIL import Image, ImageFont, ImageDraw
ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--font",type=Path,required=True,help="Noto Sans SC variable TTF")
FONT = parser.parse_args().font
GLYPHS = "地理数学语文英化物班会自习体育社团周考放假待配置日休息今天高中倒计时元旦春节清明劳动端午儿童国庆重阳冬至立春雨水惊蛰分谷夏小满芒种暑秋处白露寒霜降雪大宝宝生"
font = ImageFont.truetype(str(FONT), 20)
font.set_variation_by_name("Medium")
lines = ["// Generated from Noto Sans SC Medium, native 20px. SIL Open Font License.",
         "#pragma once", "#include <stdint.h>",
         "struct ReadableGlyph { uint32_t codepoint; uint8_t left, top, right, bottom; uint8_t pixels[60]; };",
         "static const ReadableGlyph READABLE_GLYPHS[] PROGMEM = {"]
for char in dict.fromkeys(GLYPHS):
    canvas = Image.new("1", (20, 24))
    ImageDraw.Draw(canvas).text((0,20),char,font=font,anchor="ls",fill=1)
    box = canvas.getbbox()
    assert box and box[0]>=0 and box[2]<=20 and box[1]>0 and box[3]<24, (char,box)
    bits = [canvas.getpixel((x,y)) for y in range(24) for x in range(20)]
    data = [sum(bool(v) << (7-j) for j,v in enumerate(bits[i:i+8])) for i in range(0,len(bits),8)]
    lines.append("    {0x%04X, %d, %d, %d, %d, {%s}}," % (ord(char),box[0],box[1],box[2]-1,box[3]-1,", ".join("0x%02X" % v for v in data)))
lines += ["};"]
(ROOT/"include/readable_font_data.h").write_text("\n".join(lines)+"\n",encoding="utf8")
print("Native20 glyphs:", len(dict.fromkeys(GLYPHS)), "bitmap bytes:", len(dict.fromkeys(GLYPHS))*60)
