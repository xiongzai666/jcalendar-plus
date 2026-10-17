"""Exercise the source-bound review failures with real text validation and fonts."""
import os, re, shutil, subprocess, sys, unittest
from pathlib import Path

ROOT = Path(sys.argv.pop(1)).resolve() if len(sys.argv)>1 and not sys.argv[1].startswith('-') else Path(__file__).resolve().parents[1]
OUT = ROOT / '.pio/review-fixes-host'
OUT.mkdir(parents=True, exist_ok=True)
MAIN = (ROOT/'src/main.cpp').read_text(encoding='utf8')
SCREEN = (ROOT/'src/screen_ink.cpp').read_text(encoding='utf8')
VIEW = (ROOT/'src/timetable_view.inc').read_text(encoding='utf8')

def function(source, name):
    start = source.index('void '+name+'(')
    brace = source.index('{', start); end = brace+1; depth = 1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end += 1
    return source[start:end]

class ReviewFixes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        environment = os.environ.get('J_CALENDAR_ENVIRONMENT', 'z15')
        library = ROOT/'.pio/libdeps'/environment/'U8g2_for_Adafruit_GFX/src'
        fonts = (library/'u8g2_fonts.c').read_text(encoding='latin1')
        names = ['u8g2_font_wqy16_t_gb2312', 'u8g2_font_wqy12_t_gb2312']
        declarations = [re.search(r'const uint8_t '+n+r'\[.*?=\s*(?:"(?:\\.|[^"\\])*"\s*)+;', fonts, re.S).group() for n in names]
        (OUT/'fonts.cpp').write_text('#include "U8g2_for_Adafruit_GFX.h"\n'+'\n'.join(declarations), encoding='utf8')
        (OUT/'Preferences.h').write_text('#pragma once\n', encoding='utf8')
        (OUT/'Adafruit_GFX.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
class Print {public:virtual size_t write(uint8_t)=0;virtual ~Print(){};};
class Adafruit_GFX {public:
 void drawPixel(int,int,uint16_t){}
 void drawFastHLine(int16_t,int16_t,int16_t,uint16_t){}
 void drawFastVLine(int16_t,int16_t,int16_t,uint16_t){}
};
''', encoding='utf8')
        source = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
#include "remote_contract.h"
#include "U8g2_for_Adafruit_GFX.h"
#include "_preference.h"
class String {
 std::string s;
public:
 String(){} String(const char* p):s(p?p:""){} String(std::string p):s(p){}
 String(int n):s(std::to_string(n)){}
 const char* c_str()const{return s.c_str();}size_t length()const{return s.size();}bool isEmpty()const{return s.empty();}
 char operator[](size_t i)const{return s[i];}char charAt(size_t i)const{return s[i];}
 String substring(size_t a,size_t b)const{return s.substr(a,b-a);}bool equals(const String& v)const{return s==v.s;}
 int toInt()const{return atoi(s.c_str());}bool operator!=(const String& v)const{return s!=v.s;}
 String& operator+=(const String& v){s+=v.s;return *this;}friend String operator+(String a,const String& b){return a+=b;}
};
std::map<std::string,std::string> values;
struct Preferences {
 bool begin(const char*,bool=false){return true;}void end(){}
 String getString(const char* k,const char* d=""){return values.count(k)?values[k]:d;}
 size_t putString(const char* k,const String& v){values[k]=v.c_str();return v.length();}
 void putBool(const char* k,bool v){values[k]=v?"1":"0";}
 void putUInt(const char* k,unsigned v){values[k]=std::to_string(v);}
 bool remove(const char* k){return values.erase(k);}
};
struct Param {std::string v;int cap=100;
 Param(){}Param(const char*,const char*,const char* d,int n):v(d),cap(n){}
 void setValue(const char* p,int n){cap=n;v=std::string(p).substr(0,n);}const char* getValue(){return v.c_str();}
};
struct Logger{void println(const char*){}}Serial;
unsigned long millis(){return 10;}unsigned long _idle_millis=0,_param_saved_millis=0;bool _restart_after_param_save=false;
void refreshInitialWifiSetupState(bool){}
String normalizeRemoteCertificate(const String&){return "";}
Adafruit_GFX display;
struct Draw {std::string text;int x,width;};std::vector<Draw> drawn;
struct Fonts:U8G2_FOR_ADAFRUIT_GFX {
 int16_t drawUTF8(int16_t x,int16_t y,const char* p){drawn.push_back({p,x,getUTF8Width(p)});return 0;}
}u8g2Fonts;
#define FONT_TEXT u8g2_font_wqy16_t_gb2312
#define FONT_SUB u8g2_font_wqy12_t_gb2312
const int GxEPD_BLACK=0;
void centered(const String& v,int x,int y){u8g2Fonts.drawUTF8(x-u8g2Fonts.getUTF8Width(v.c_str())/2,y,v.c_str());}
bool _network_round_connected=false;
struct Wifi{bool connected=false;bool isConnected(){return connected;}void mode(int){connected=false;}}WiFi;
const int WIFI_OFF=0;
'''
        params = set(re.findall(r'para_\w+', function(MAIN,'saveParamsCallback')))
        source += '\n'.join('Param '+p+';' for p in sorted(params) if p!='para_cd_day_label')
        declaration = re.search(r'WiFiManagerParameter para_cd_day_label\(.*?\);', MAIN).group().replace('WiFiManagerParameter','Param')
        source += '\n'+declaration+'\n'+function(MAIN,'saveParamsCallback')
        setter = function(SCREEN,'si_set_network_connected') if 'void si_set_network_connected(' in SCREEN else 'void si_set_network_connected(bool) {}'
        source += '\n'+setter
        title = VIEW[VIEW.index('    // Exam countdown.'):VIEW.index('    int daysLeft = -1;', VIEW.index('    // Exam countdown.'))]
        month = re.search(r'        if \(\(y\.equals.*?\n        }',SCREEN,re.S).group()
        tv = SCREEN[SCREEN.index('                timeval tv'):SCREEN.index('\n                if (isSetOK)',SCREEN.index('                timeval tv'))]
        line = next(x for x in SCREEN.splitlines() if 'drawUTF8(42, 130' in x)
        capture = MAIN[MAIN.index('bool connected = connectPreferredWiFi();')+len('bool connected = connectPreferredWiFi();'):MAIN.index('    if (connected) {', MAIN.index('bool connected = connectPreferredWiFi();'))]
        source += '\nvoid drawTitle(String _cd_day_label){int rightCenter=325;'+title+'}\n'
        source += 'bool tag(int month){tm tmInfo={};tmInfo.tm_year=126;tmInfo.tm_mon=month-1;String y="2026",m=month==0?"00":month<10?"0"+String(month):String(month),d="14";char t=\'a\',tags[32]={};'+month+'return tags[14]==\'a\';}\n'
        # The wildcard tag must also match a different current month.
        source += 'bool wildcard(){tm tmInfo={};tmInfo.tm_year=126;tmInfo.tm_mon=8;String y="0000",m="00",d="14";char t=\'a\',tags[32]={};'+month+'return tags[14]==\'a\';}\n'
        # Substitute only the OS clock boundary; execute the actual weather fallback statements.
        source += '#define timeval ReviewTimeval\nstruct ReviewTimeval{long tv_sec,tv_usec;};bool captured=false;int settimeofday(ReviewTimeval* v,void*){captured=v->tv_sec==1800000000&&v->tv_usec==0;return 0;}\n'
        source += 'void weather(){long set=1800000000;bool isSetOK=false;'+tv+'assert(captured);}\n'
        source += 'void network(bool connected){WiFi.connected=connected;'+capture+'WiFi.mode(WIFI_OFF);'+line+'}\n'
        roundtrip = re.search(r'para_cd_day_label\.setValue\(cddLabel\.c_str\(\), \d+\);',MAIN).group()
        fallback = re.search(r'_cd_day_label\.isEmpty\(\) \? String\("([^"]+)"\)', title).group(1)
        source += '\nconst char* fallbackLabel="'+fallback+'";\n'
        source += r'''
int main(int argc,char** argv){assert(argc==2);std::string test=argv[1];u8g2Fonts.begin(display);u8g2Fonts.setFont(FONT_TEXT);
 if(test=="clock")weather();
 if(test=="source"){
  values[PREF_REMOTE_HOST]="a.example";values[PREF_REMOTE_TOKEN]="identity-A";
  para_remote_host.v="b.example";para_remote_token.v="identity-B";
  values[PREF_REMOTE_REV]="7";values[PREF_REMOTE_PENDING]="8";saveParamsCallback();
  assert(!values.count(PREF_REMOTE_REV)&&!values.count(PREF_REMOTE_PENDING));
  values[PREF_REMOTE_REV]="9";values[PREF_REMOTE_PENDING]="10";saveParamsCallback();
  assert(values[PREF_REMOTE_REV]=="9"&&values[PREF_REMOTE_PENDING]=="10");
  para_remote_token.v="identity-C";saveParamsCallback();assert(!values.count(PREF_REMOTE_REV)&&!values.count(PREF_REMOTE_PENDING));
 }
 if(test=="label"){
  String cddLabel="八个汉字倒计时名";
'''+roundtrip+r'''
  saveParamsCallback();assert(values[PREF_CD_DAY_LABLE]==cddLabel.c_str());
  para_cd_day_label.v=std::string("八个汉字倒计时名").substr(0,23);saveParamsCallback();
  assert(values[PREF_CD_DAY_LABLE]==cddLabel.c_str());
 }
 if(test=="title")for(const char* label:{"高考","八个汉字倒计时名",""}){
  drawn.clear();drawTitle(label);std::string text;
  for(auto& d:drawn){assert(d.x>=251&&d.x+d.width<=399);text+=d.text;printf("%s: x=%d width=%d\n",d.text.c_str(),d.x,d.width);}
  assert(text==std::string(*label?label:fallbackLabel)+"倒计时");
 }
 if(test=="network"){
  network(true);assert(drawn.back().text=="已联网，时间尚未校准");
  network(false);assert(drawn.back().text=="暂时无法连接 Wi-Fi");
 }
 if(test=="tags"){assert(tag(1));assert(tag(9));assert(tag(10));assert(wildcard());}
 puts("review behavior passed");
}
'''
        (OUT/'checks.cpp').write_text(source,encoding='utf8')
        cxx=os.environ.get('J_CALENDAR_CXX') or shutil.which('g++')
        assert cxx, 'Set J_CALENDAR_CXX or install g++'
        compiler=[cxx,'c++'] if Path(cxx).stem=='zig' else [cxx]
        cmd=compiler+['-std=c++11','-ftrivial-auto-var-init=pattern','-I'+str(OUT),'-I'+str(ROOT/'include'),'-I'+str(ROOT/'.pio/libdeps'/environment/'ArduinoJson/src'),'-I'+str(library),str(OUT/'checks.cpp'),str(OUT/'fonts.cpp'),str(library/'U8g2_for_Adafruit_GFX.cpp'),'-o',str(OUT/'checks.exe')]
        subprocess.run(cmd,check=True)

    def run_case(self, name):
        r=subprocess.run([str(OUT/'checks.exe'),name],capture_output=True,text=True,encoding='utf8')
        self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        print(name+': '+r.stdout.strip())
    def test_clock(self):self.run_case('clock')
    def test_source(self):self.run_case('source')
    def test_label(self):self.run_case('label')
    def test_title(self):self.run_case('title')
    def test_network(self):self.run_case('network')
    def test_tags(self):self.run_case('tags')

if __name__=='__main__':unittest.main()
