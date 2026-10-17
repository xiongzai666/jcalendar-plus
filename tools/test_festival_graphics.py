"""Run the actual corner renderer with real font decoding and assert the 150x51 bounds."""
from pathlib import Path
import os,re,subprocess,tempfile,sys
root=Path(__file__).resolve().parents[1]
environment=os.environ.get('J_CALENDAR_ENVIRONMENT','z15')
deps=root/'.pio/libdeps'/environment
lib=deps/'U8g2_for_Adafruit_GFX/src'
view=(root/'src/timetable_view.inc').read_text(encoding='utf8')
body=view[view.index('static void drawFestivalCountdown('):view.index('static bool examCountdown(')]
fonts=(lib/'u8g2_fonts.c').read_text(encoding='latin1')
with tempfile.TemporaryDirectory(prefix='festival-graphics-') as temp:
    out=Path(temp)
    (out/'Adafruit_GFX.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
#include <algorithm>
class Print {public:virtual size_t write(uint8_t)=0;virtual ~Print(){};};
class Adafruit_GFX {public:
 uint16_t pixels[400*300];Adafruit_GFX(){std::fill(pixels,pixels+400*300,65535);}
 void drawPixel(int x,int y,uint16_t c){if(x>=0&&x<400&&y>=0&&y<300)pixels[y*400+x]=c;}
 void drawFastHLine(int16_t x,int16_t y,int16_t length,uint16_t color){for(int i=0;i<length;++i)if(x+i>=0&&x+i<400&&y>=0&&y<300)pixels[y*400+x+i]=color;}
 void drawFastVLine(int16_t x,int16_t y,int16_t length,uint16_t color){for(int i=0;i<length;++i)if(x>=0&&x<400&&y+i>=0&&y+i<300)pixels[(y+i)*400+x]=color;}
};
''',encoding='utf8')
    names=['u8g2_font_wqy16_t_gb2312','u8g2_font_fub25_tn','u8g2_font_fub17_tn','u8g2_font_helvB12_tn']
    definitions=[re.search(r'const uint8_t '+name+r'\[.*?=\s*(?:"(?:\\.|[^"\\])*"\s*)+;',fonts,re.S).group() for name in names]
    (out/'fonts.cpp').write_text('#include "U8g2_for_Adafruit_GFX.h"\n'+'\n'.join(definitions),encoding='utf8')
    source=r'''
#include <cassert>
#include <string>
#include <vector>
#include <ctime>
#include <cstdio>
#include <fstream>
#include <ArduinoJson.h>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
#include "readable_font.h"
#include "festival_countdown_core.h"
#include "display_policy_core.h"
#include "U8g2_for_Adafruit_GFX.h"
class String {std::string value;public:String(const char* p=""):value(p?p:""){}String(int n):value(std::to_string(n)){}const char* c_str()const{return value.c_str();}};
#define FONT_TEXT u8g2_font_wqy16_t_gb2312
#define GxEPD_BLACK 0
#define GxEPD_RED 1
Adafruit_GFX display;
struct Text{int x,y,width;std::string value;};
struct Fonts:U8G2_FOR_ADAFRUIT_GFX{std::vector<Text> drawn;
 int16_t drawUTF8(int16_t x,int16_t y,const char* s){drawn.push_back({x,y,getUTF8Width(s),s});return U8G2_FOR_ADAFRUIT_GFX::drawUTF8(x,y,s);}
}u8g2Fonts;
String _festival_countdown;tm tmInfo={};
void drawViewText(int x,int y,const String& s){u8g2Fonts.drawUTF8(x,y,s.c_str());}
void centered(const String& s,int x,int y){drawViewText(x-u8g2Fonts.getUTF8Width(s.c_str())/2,y,s);}
'''+body+r'''
int main(int argc,char** argv){
 u8g2Fonts.begin(display);tmInfo.tm_year=125;tmInfo.tm_mon=0;tmInfo.tm_mday=1;
 for(const char* date:{"20250101","20241231","20250102","20250110","20250410","20270927","20520518","20991231"}){
  if(std::string(date)=="20241231"){tmInfo.tm_year=126;date="20250101";}else tmInfo.tm_year=125;
  std::string config="{\"mode\":\"custom\",\"label\":\"宝宝生日\",\"date\":\""+std::string(date)+"\"}";
  _festival_countdown=config.c_str();u8g2Fonts.drawn.clear();std::fill(display.pixels,display.pixels+400*300,65535);
  drawFestivalCountdown(250);assert(u8g2Fonts.drawn.size()>=2);
  for(const auto& text:u8g2Fonts.drawn){assert(text.x>=251&&text.x+text.width<=399&&text.y>=249&&text.y<=299);}
  for(int y=0;y<300;++y)for(int x=0;x<400;++x)if(display.pixels[y*400+x]!=65535)assert(x>=251&&x<=399&&y>=249&&y<=299);
  if(std::string(date)=="20250101")assert(u8g2Fonts.drawn[1].value==(tmInfo.tm_year==125?"今天":"已到期"));
 }
 if(argc==2){std::ofstream f(argv[1],std::ios::binary);f<<"P6\n400 300\n255\n";for(auto pixel:display.pixels){char rgb[3]={char(pixel==65535||pixel==1?255:0),char(pixel==65535?255:0),char(pixel==65535?255:0)};f.write(rgb,3);}}
 puts("Actual corner fonts and pixels: four Chinese characters, 1–5 digits, today and expired remain within 150x51");
}
'''
    (out/'test.cpp').write_text(source,encoding='utf8')
    cpp=os.environ.get('J_CALENDAR_CXX','g++');compiler=[cpp,'c++'] if Path(cpp).stem=='zig' else [cpp]
    binary=out/'test.exe'
    subprocess.run(compiler+['-std=c++11','-I'+str(out),'-I'+str(root/'include'),'-I'+str(lib),'-I'+str(deps/'ArduinoJson/src'),'-I'+str(deps/'Nongli/src'),str(out/'test.cpp'),str(out/'fonts.cpp'),str(lib/'U8g2_for_Adafruit_GFX.cpp'),str(deps/'Nongli/src/nongli.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)]+sys.argv[1:],check=True)
