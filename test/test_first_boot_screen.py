"""Run actual firmware bootstrap reader and screen task; fake only ESP/Preferences/epaper hardware."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

def function(source, name, fallback=None):
    start = source.find('void '+name+'(')
    if start < 0:
        if fallback is not None: return fallback
        raise AssertionError('Production function missing: '+name)
    brace = source.index('{', start); depth = 1; index = brace+1
    while depth:
        if source[index] == '{': depth += 1
        elif source[index] == '}': depth -= 1
        index += 1
    return source[start:index]

class FirstBootScreenTests(unittest.TestCase):
    def test_empty_saved_network_draws_setup_and_saved_or_unknown_network_keeps_offline_page(self):
        cpp = os.environ.get('J_CALENDAR_CXX') or shutil.which('g++')
        self.assertTrue(cpp, 'Set J_CALENDAR_CXX for the firmware drawing integration test')
        library = ROOT/'.pio/libdeps'/os.environ.get('J_CALENDAR_ENVIRONMENT','z15')/'U8g2_for_Adafruit_GFX/src'
        self.assertTrue(library.is_dir(), 'Build/install the selected firmware dependencies first')
        screen = (ROOT/'src/screen_ink.cpp').read_text(encoding='utf-8')
        main = (ROOT/'src/main.cpp').read_text(encoding='utf-8')
        # RC1 had no stored-network reader/setter. Its real screen task reproduces the missing guide.
        reader = function(main,'refreshInitialWifiSetupState','void refreshInitialWifiSetupState(bool) {}')
        setter = function(screen,'si_set_initial_wifi_setup','void si_set_initial_wifi_setup(bool) {}')
        task = function(screen,'task_screen')
        setup = function(main,'setup')
        network = 'void startupNetworkPhase() {\n'+setup[setup.index('    Serial.println("Wm begin...");'):-1]+'\n}'
        # Upstream includes non-UTF8 copyright comments; selected font arrays use ASCII escapes.
        font_source = (library/'u8g2_fonts.c').read_text(encoding='latin-1')
        fonts = []
        for name in ['u8g2_font_wqy16_t_gb2312','u8g2_font_wqy12_t_gb2312']:
            match = re.search(r'const uint8_t '+name+r'\[.*?=\s*(?:"(?:\\.|[^"\\])*"\s*)+;',font_source,re.S)
            self.assertIsNotNone(match, 'Real firmware font missing: '+name)
            fonts.append(match.group())
        with tempfile.TemporaryDirectory(prefix='jcalendar-first-boot-') as directory:
            temp = Path(directory)
            (temp/'Adafruit_GFX.h').write_text('''#pragma once
#include <cstddef>
#include <cstdint>
#include <cassert>
class Print { public: virtual size_t write(uint8_t) { return 1; } };
class Adafruit_GFX { public:
 void drawFastHLine(int x,int y,int n,uint16_t) { assert(x>=0&&y>=0&&x+n<=400&&y<300); }
 void drawFastVLine(int x,int y,int n,uint16_t) { assert(x>=0&&y>=0&&x<400&&y+n<=300); }
};
''',encoding='utf-8')
            (temp/'fonts.cpp').write_text('#include "U8g2_for_Adafruit_GFX.h"\n'+'\n'.join(fonts),encoding='utf-8')
            stubs = r'''
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include "U8g2_for_Adafruit_GFX.h"
#define FONT_TEXT u8g2_font_wqy16_t_gb2312
#define FONT_SUB u8g2_font_wqy12_t_gb2312
const int GxEPD_WHITE=0,GxEPD_RED=1,GxEPD_BLACK=2,ROTATION=0;
const char* PREF_NAMESPACE="config";const char* PREF_BACKUP_SSID="BACKUP_SSID";const char* PREF_SI_CAL_DATE="SI_CAL_DATE";
struct String { std::string value; String(const char* s=""):value(s){} bool isEmpty() const{return value.empty();} size_t length()const{return value.size();} };
struct wifi_config_t { struct { unsigned char ssid[33]; } sta; };
const int WIFI_IF_STA=0,ESP_OK=0; bool primaryKnown=true,hasPrimary=false,backupKnown=true;const char* backup="";
int esp_wifi_get_config(int, wifi_config_t* out) { if(!primaryKnown)return -1;if(hasPrimary)std::strcpy((char*)out->sta.ssid,"saved");return ESP_OK; }
const char* PREF_REMOTE_TOKEN="REMOTE_TOKEN";
struct Preferences { bool begin(const char*,bool=false){return backupKnown;} String getString(const char* key,const char* fallback=""){return String(backupKnown&&std::strcmp(key,PREF_BACKUP_SSID)==0?backup:fallback);} void end(){} void putInt(const char*,int){} };
bool connectedFixture=false,_wifi_flag=false,_retry_wake_this_boot=false;int _remote_retry_state=0;unsigned long _wifi_failed_millis=0;
struct Manager {void setHostname(const char*){}void setEnableConfigPortal(bool){}void setConnectTimeout(int){}} wm;
bool connectPreferredWiFi(){return connectedFixture;}void led_fast(){}void led_on(){}void led_slow(){}void noteRemoteSyncFailure(const char*){}
bool remoteRetryIsDue(int,time_t,bool){return false;}enum class RemoteSyncResult{Failed};void remoteRetryRecord(int&,RemoteSyncResult,bool,time_t){}
unsigned long millis(){return 250;}void _sntp_exec(int){}void weather_exec(int){}
const int WIFI_OFF=0;struct Wifi {void mode(int){}} WiFi;
struct Display:Adafruit_GFX { void init(int){}void setRotation(int){}void setFullWindow(){}void firstPage(){}void fillScreen(int){}bool nextPage(){return false;}void powerOff(){}void hibernate(){} } display;
struct Text { std::string text;int width; };
struct RecordingFonts:U8G2_FOR_ADAFRUIT_GFX { std::vector<Text> drawn;
 int16_t drawUTF8(int16_t x,int16_t y,const char* text){int width=getUTF8Width(text);assert(x+width<=400);drawn.push_back({text,width});return U8G2_FOR_ADAFRUIT_GFX::drawUTF8(x,y,text);}
} u8g2Fonts;
struct SerialLike {void println(const char*){}} Serial;
int voltage;bool lowBattery;int _calendar_status=2,_si_type=2,_screen_status=0;
bool _initial_wifi_setup=false,recovery=false;void* SCREEN_HANDLER=nullptr;
struct {int tm_year=126,tm_mon=9,tm_mday=8;} tmInfo;
String _cd_day_label,_cd_day_date;
int readBatteryVoltage(){return 4000;}bool isBatteryLow(int){return false;}void delay(int){}void init_cal_layout_size(){}
bool remote_config_unavailable(){return recovery;}void drawTimetableCountdown(){}void drawDaytimeWeekly(){}void drawTodoPage(){}
void draw_cal_days(bool){}void draw_cal_header(){}void draw_cal_year(bool){}void draw_cd_day(String,String){}
int weather_status(){return 2;}void draw_weather(bool){}void draw_status(bool){}void vTaskDelete(void*){}
'''
            checks = r'''
bool has(const char* text){for(const auto& line:u8g2Fonts.drawn)if(line.text==text)return true;return false;}
void run(bool connected,bool pk,bool hp,bool bk,const char* bs,bool setup,int calendar=2){
 primaryKnown=pk;hasPrimary=hp;backupKnown=bk;backup=bs;_initial_wifi_setup=false;_calendar_status=calendar;connectedFixture=connected;
 startupNetworkPhase();u8g2Fonts.drawn.clear();task_screen(nullptr);
 if(setup){
  if(!has("设备待配置")){for(const auto& line:u8g2Fonts.drawn)std::fprintf(stderr,"Observed: %s\n",line.text.c_str());}
  assert(has("设备待配置")&&!has("时间待同步"));
  assert(has("长按上键 5 秒后松开")&&has("连接 J-Calendar 热点")&&has("打开 192.168.4.1")&&has("密码见设备配置页面"));
  for(const auto& line:u8g2Fonts.drawn)std::printf("Setup real-font width %d: %s\n",line.width,line.text.c_str());
 }else {assert(!has("设备待配置"));if(calendar==2)assert(has("时间待同步"));}
}
int main(){
 run(false,true,false,true,"",true);
 run(false,true,true,true,"",false);
 run(false,true,false,true,"saved-backup",false);
 run(false,false,false,true,"",false);
 run(false,true,false,false,"",false);
 run(true,false,false,false,"",false);
 run(false,true,false,true,"",true,1);
 std::puts("Actual bootstrap reader -> epaper task: empty, saved primary/backup, failed reads, connected and valid-clock cases passed");
}
'''
            path=temp/'first_boot.cpp';path.write_text(stubs+'\n'+setter+'\n'+reader+'\n'+network+'\n'+task+'\n'+checks,encoding='utf-8')
            binary=temp/('first_boot.exe' if os.name=='nt' else 'first_boot')
            compiler=[cpp,'c++'] if Path(cpp).stem=='zig' else [cpp]
            command=compiler+['-std=c++11','-I'+str(temp),'-I'+str(library),str(path),str(temp/'fonts.cpp'),str(library/'U8g2_for_Adafruit_GFX.cpp'),'-o',str(binary)]
            compiled=subprocess.run(command,capture_output=True,text=True,encoding='utf-8')
            self.assertEqual(compiled.returncode,0,compiled.stderr)
            result=subprocess.run([str(binary)],capture_output=True,text=True,encoding='utf-8')
            self.assertEqual(result.returncode,0,result.stdout+'\n'+result.stderr)
            print(result.stdout)

if __name__=='__main__':unittest.main()
