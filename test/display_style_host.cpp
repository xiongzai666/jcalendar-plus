// Only hardware, NVS and network input are substituted. Drawing bodies are extracted unchanged.
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>
#include <ArduinoJson.h>
#include "graphics_stub/host_string.h"
#include "U8g2_for_Adafruit_GFX.h"
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
#include "subject_font.h"
#include "display_policy_core.h"
#include "calendar_override_core.h"
#ifdef HAS_RADAR
#include "codex_radar_core.h"
#endif
#include "festival_countdown_core.h"
#include "effective_schedule_core.h"
#include "holiday.h"
#include "nongli.h"
#define FONT_TEXT u8g2_font_wqy16_t_gb2312
#define FONT_SUB u8g2_font_wqy12_t_gb2312
#define GxEPD_BLACK 0
#define GxEPD_RED 1
#define GxEPD_WHITE 0xffff
namespace ArduinoJson {template<>struct Converter<String>{static String fromJson(JsonVariantConst v){return String(v.as<const char*>());}static bool checkJson(JsonVariantConst v){return v.is<const char*>();}};}
#ifdef _WIN32
tm* localtime_r(const time_t* t,tm* out){return localtime_s(out,t)==0?out:nullptr;}
#endif
struct Panel:Adafruit_GFX {
    int width(){return 400;}int height(){return 300;}
    void setPartialWindow(int,int,int,int){}void firstPage(){}void nextPage(){}
    void fillScreen(uint16_t c){std::fill(pixels,pixels+120000,c);}
    void fillRoundRect(int x,int y,int w,int h,int,uint16_t c){fillRect(x,y,w,h,c);}
    void drawCircle(int x,int y,int r,uint16_t c){for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx)if(dx*dx+dy*dy<=r*r)drawPixel(x+dx,y+dy,c);}
}display;
struct Text {std::string value;int x,y,width,minX,minY,maxX,maxY;const uint8_t* font;};
struct Fonts:U8G2_FOR_ADAFRUIT_GFX {
    std::vector<Text> texts;const uint8_t* active=nullptr;
    void setFont(const uint8_t* f){active=f;U8G2_FOR_ADAFRUIT_GFX::setFont(f);}
    void drawUTF8(int x,int y,const char* s){
        display.beginGlyph();U8G2_FOR_ADAFRUIT_GFX::drawUTF8(x,y,s);display.recording=false;
        texts.push_back({s,x,y,getUTF8Width(s),display.minX,display.minY,display.maxX,display.maxY,active});
    }
    void print(const char* s){int x=getCursorX(),y=getCursorY();drawUTF8(x,y,s);setCursor(x+getUTF8Width(s),y);}
    void print(const String& s){print(s.c_str());}
    void printf(const char* fmt,...){char b[120];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);print(b);}
}u8g2Fonts;
tm tmInfo={};time_t clockNow=0,_screen_snapshot_time=0;
String week_str[]={"日","一","二","三","四","五","六"};
String nl10_str[]={"初","十","廿","卅"},nl_str[]={"十","一","二","三","四","五","六","七","八","九","十"},nl_mon_str[]={"","正","二","三","四","五","六","七","八","九","十","冬","腊"};
int lunarDates[31]={},jqAccDate[24]={},_week_1st=0;
int jrLength=1,jrDate[]={1001};String jrText[]={"国庆节"};
String todayLunarYear="丙午年 马",todayLunarDay="九月初八",_tag_days_str;
uint16_t todayColor=0;
String _festival_countdown="{\"mode\":\"auto-term\"}",_cd_day_date="20270607",_cd_day_label="高考";
bool lowBattery=false,oldWeather=false,customClock=false;
std::string _todo_json="[]";String _remote_sync_code;int _todo_count=0;uint64_t _remote_sync_at=0;bool _remote_sync_failed=false;
const char* subjects[]={"化学","数学","英语","物理","语文","地理","物理","放假","数学","英语","物理","自习"};
struct EffectiveDayContext {EffectiveDay day;String routine;String courses[12];struct{bool dayOff=false;}override;};
LessonClock getLessonClock(){return defaultLessonClock();}
static const char* weeklySubjects[6][8]={
 {"语文","数学","语文","英语","物理","化学","体育","班会"},
 {"语文","数学","语文","英语","物理","化学","体育","班会"},
 {"语文","数学","语文","英语","物理","化学","体育","班会"},
 {"语文","数学","语文","英语","物理","化学","体育","班会"},
 {"语文","数学","语文","英语","物理","化学","体育","班会"},
 {"语文","数学","语文","英语","物理","化学","体育","班会"}
};
EffectiveDayContext effectiveDayForDate(const tm& date){
    EffectiveDayContext c;c.day={getLessonClock(),0,false};
    for(int i=0;i<12;++i){
        c.courses[i]=i<8&&date.tm_wday>=1&&date.tm_wday<=6?weeklySubjects[date.tm_wday-1][i]:"放假";
        if(effectiveSubject(c.courses[i].c_str()))c.day.mask|=1u<<i;
    }
    if(customClock&&date.tm_wday==3){c.day.independent=true;c.courses[0]="机器人";c.day.clock.start[0]=520;}
    return c;
}
void loadDaytimeCourses(String c[6][8]){for(int col=0;col<6;++col)for(int row=0;row<8;++row)c[col][row]=subjects[(row+col)%8];}
struct Window{int startMinute=-1,endMinute=-1;};
Window dismissalWeatherWindow(const EffectiveDay&,int){return {};}
bool weather_dismissal_enabled(){return false;}
String weather_dismissal_hint(bool,time_t,time_t=0,time_t=0){return "";}
String nextScheduledLesson(){return "周一 07:40 地理";}
String nextDailyActivity(int,const EffectiveDayContext&){return "下一项 小练 12:00";}
struct DailyWeather{String date,textDay,windDirDay="西南风";int tempMax=25,tempMin=19,humidity=80,windScaleDay=2,iconDay=502;};
struct DailyForecast{DailyWeather weather[3];int length=3;}daily;
struct Weather{String text="雾",time,windDir="西南风";int temp=24,humidity=80,windScale=2,icon=502;}live;
DailyForecast* weather_data_daily(){return &daily;}Weather* weather_data_now(){return &live;}
int weather_type(){return 1;}int weather_status(){return 1;}
bool weather_daily_available(int){return true;}bool weather_now_available(){return true;}
bool weather_is_fallback(){return oldWeather;}
struct Preferences {
    void begin(const char*,bool=false){}void end(){}
    size_t getBytesLength(const char*){return 0;}void getBytes(const char*,void*,size_t){}
};
#define PREF_NAMESPACE "test"
#define PREF_HOLIDAY "holiday"
#define PREF_DATE_OVERRIDES "overrides"
const char* preferenceString(Preferences&,const char*,const char*,int){return "[]";}
struct Logger{void print(const char*){}}Serial;
extern const uint8_t u8g2_font_qweather_icon_16[];
bool isNight(const String&){return false;}
#include "weather_font.h"
#include "timetable.inc"
#include "calendar.inc"
std::string radarJson;
const char* codexRadarJson(){return radarJson.c_str();}
bool codexRadarOffline(){return oldWeather;}const char* codexRadarError(){return "";}
time_t fixedTime(time_t*){return clockNow;}
#define time fixedTime
#include "radar.inc"
#undef time
static void reset(){display.reset();u8g2Fonts.texts.clear();u8g2Fonts.setFontMode(1);u8g2Fonts.setForegroundColor(0);u8g2Fonts.setBackgroundColor(0xffff);}
static void output(const std::string& path){
    std::ofstream f(path+".ppm",std::ios::binary);f<<"P6\n400 300\n255\n";
    for(auto c:display.pixels){char rgb[]={char(c==0xffff?255:c==1?180:0),char(c==0xffff?255:0),char(c==0xffff?255:0)};f.write(rgb,3);}
    for(auto& t:u8g2Fonts.texts)if(t.maxX>=0){
        if(t.minX<0||t.maxX>=400||t.minY<0||t.maxY>=300)printf("Overflow: %s %d,%d..%d,%d\n",t.value.c_str(),t.minX,t.minY,t.maxX,t.maxY);
        assert(t.minX>=0&&t.maxX<400&&t.minY>=0&&t.maxY<300);
    }
}
static void verifyTypography(){
    int errors=0;
    reset();drawDaytimeWeekly();
    const int top=63+2*29;
    int bottoms[2]={-1,-1};
    for(int part=0;part<2;++part)for(int y=top+1;y<top+29;++y)
        for(int x=part?80:53;x<(part?108:80);++x)
            if(display.pixels[y*400+x]==GxEPD_BLACK)bottoms[part]=std::max(bottoms[part],y);
    printf("Weekly word ink bottoms: %d / %d\n",bottoms[0],bottoms[1]);
    if(bottoms[0]!=bottoms[1]){puts("FAIL: adjacent course glyphs do not share an ink bottom");++errors;}
    int courseInk=0;
    for(int y=top+1;y<top+29;++y)for(int x=53;x<108;++x)if(display.pixels[y*400+x]==GxEPD_BLACK)++courseInk;
    printf("Weekly bold course ink: %d\n",courseInk);
    if(courseInk<300){puts("FAIL: selected bold course style missing");++errors;}
    bool boldTime=false;for(auto& t:u8g2Fonts.texts)if(t.value=="09:50")boldTime=t.font==u8g2_font_helvB12_tf;
    if(!boldTime){puts("FAIL: selected bold time style missing");++errors;}
    if(display.pixels[179*400+60]!=GxEPD_RED||display.pixels[180*400+60]!=GxEPD_RED||display.pixels[181*400+60]!=GxEPD_WHITE){puts("FAIL: selected 2px midday divider missing");++errors;}
    if(display.pixels[80*400+394]!=GxEPD_RED||display.pixels[80*400+393]!=GxEPD_RED||display.pixels[80*400+392]!=GxEPD_WHITE){puts("FAIL: selected 2px today border missing");++errors;}
    _cd_day_label="高考";reset();drawTimetableCountdown();
    bool title=false,label=false;
    for(auto& t:u8g2Fonts.texts){
        if(t.value=="高考倒计时")title=t.font==FONT_TEXT;
        if(t.value=="霜降")label=t.font==FONT_TEXT;
    }
    if(!title||!label){puts("FAIL: main and corner names do not use the surrounding native Chinese font");++errors;}
    Text day={},first={},second={},temperature={};
    for(auto& t:u8g2Fonts.texts){
        if(t.value=="今天")day=t;
        if(t.value=="晴间")first=t;
        if(t.value=="多云")second=t;
        if(t.value=="24°")temperature=t;
    }
    auto inkBounds=[](int top,int bottom){
        Text t={};t.minX=400;t.minY=300;t.maxX=t.maxY=-1;
        for(int y=top;y<bottom;++y)for(int x=251;x<299;++x)if(display.pixels[y*400+x]==GxEPD_BLACK){t.minX=std::min(t.minX,x);t.maxX=std::max(t.maxX,x);t.minY=std::min(t.minY,y);t.maxY=std::max(t.maxY,y);}
        return t;
    };
    first=inkBounds(190,211);second=inkBounds(211,232);
    const int gaps[]={first.minY-day.maxY-1,second.minY-first.maxY-1,temperature.minY-second.maxY-1};
    printf("Long weather clear gaps: %d/%d/%d; raster widths %d/%d\n",gaps[0],gaps[1],gaps[2],first.maxX-first.minX+1,second.maxX-second.minX+1);
    if(first.maxX-first.minX+1!=28||second.maxX-second.minX+1!=29||gaps[0]<3||gaps[1]<3||gaps[2]<3){puts("FAIL: requested 15.5 target weather raster not present");++errors;}
    todayLunarYear="丙午年 马";todayLunarDay="九月初八";reset();draw_cal_year(false);
    for(auto& t:u8g2Fonts.texts)if(t.value==todayLunarYear.c_str()||t.value==todayLunarDay.c_str()){
        printf("Lunar auxiliary ink height: %d\n",t.maxY-t.minY+1);
        if(t.maxY-t.minY+1>12){puts("FAIL: auxiliary lunar text dominates the header");++errors;}
    }
    fflush(stdout);assert(errors==0);
    puts("Typography: selected bold weekly style, equal course bottoms, long-weather spacing and quiet lunar hierarchy passed.");
}
int main(int argc,char** argv){
    assert(argc==2);std::string folder=argv[1];u8g2Fonts.begin(display);
    tmInfo.tm_year=126;tmInfo.tm_mon=9;tmInfo.tm_mday=17;tmInfo.tm_hour=7;tmInfo.tm_min=0;tmInfo.tm_isdst=-1;
    clockNow=mktime(&tmInfo);_screen_snapshot_time=clockNow;
    nl_month_days(2026,10,lunarDates);nl_year_jq(2026,jqAccDate);
    daily.weather[0].textDay="晴间多云";live.text="晴间多云";daily.weather[1].textDay="阵雨";daily.weather[2].textDay="中雨";
    reset();drawTimetableCountdown();output(folder+"/01-courses");
    tmInfo.tm_hour=18;reset();drawTimetableCountdown();output(folder+"/01-courses-ended");tmInfo.tm_hour=7;
    reset();drawDaytimeWeekly();output(folder+"/02-weekly");
    customClock=true;reset();drawDaytimeWeekly();output(folder+"/weekly-custom");customClock=false;
    init_cal_layout_size();reset();draw_cal_days(false);draw_cal_year(false);draw_cal_header();draw_weather(false);draw_cd_day("高考","20270607");output(folder+"/03-calendar");
    for(const char* label:{"高考","八个汉字倒计时名"})for(const char* date:{"20261017","20270607","20991231"}){
        reset();draw_cd_day(label,date);
        for(auto& t:u8g2Fonts.texts){assert(t.x>=140&&t.x+t.width<=298);assert(t.y==57);}
        draw_weather(false);output(folder+"/calendar-countdown-"+std::to_string(strlen(label))+"-"+date);
    }
    todayLunarDay="闰腊月三十";reset();draw_cal_year(false);
    for(auto& t:u8g2Fonts.texts)if(t.value==todayLunarDay.c_str())assert(t.x+t.width<300);
    oldWeather=true;reset();drawTimetableCountdown();output(folder+"/weather-old");oldWeather=false;
    _cd_day_label="八个汉字倒计时名";daily.weather[1].tempMax=-10;daily.weather[1].tempMin=-15;daily.weather[2].tempMax=-20;daily.weather[2].tempMin=-25;
    reset();drawTimetableCountdown();output(folder+"/courses-boundary");
    _todo_count=3;_remote_sync_at=clockNow;_todo_json=R"([{"title":"完成数学练习","due":"2026-10-17","priority":"high"},{"title":"整理错题","due":"2026-10-18","priority":"normal"},{"title":"准备英语听写","due":"","priority":"normal"}])";
    reset();drawTodoPage();output(folder+"/04-public-todo");
#ifdef HAS_RADAR
    JsonDocument d;deserializeJson(d,R"({"accountLabel":"示例账号","profilesCount":1,"radar":{"status":"completed","eventStatus":"completed","fetchStatus":"ok","type":"global","precision":"datetime","timeKind":"published"},"account":{"status":"ok","five":{"remaining":100},"week":{"remaining":3},"cards":{"available":0},"loop":{"state":"running"}}})");
    d["radar"]["at"]=clockNow-86400;d["radar"]["lastSuccessAt"]=clockNow;d["account"]["sampledAt"]=clockNow;
    d["account"]["loop"]["checkedAt"]=clockNow;d["account"]["loop"]["lastSuccessAt"]=clockNow-600;d["account"]["loop"]["nextAt"]=clockNow+3600;
    d["account"]["five"]["resetsAt"]=clockNow+3600;d["account"]["week"]["resetsAt"]=clockNow+604800;
    serializeJson(d,radarJson);reset();drawCodexRadarPage();output(folder+"/04-quota");
    oldWeather=true;reset();drawCodexRadarPage();output(folder+"/quota-old");
#endif
    oldWeather=false;verifyTypography();
    for(bool old:{false,true})for(const char* name:{"晴间多云","强雷阵雨","雷阵雨伴有冰雹","小到中雨"}){
        oldWeather=old;live.text=name;reset();drawTimetableCountdown();
        std::string rendered;
        int previousBottom=old?195:189;
        for(auto& t:u8g2Fonts.texts)if(t.x>250&&t.x<299&&t.y>195&&t.y<232){
            assert(t.minX>250&&t.maxX<299&&t.minY>previousBottom);
            rendered+=t.value;previousBottom=t.maxY;
        }
        if(!rendered.empty())assert(rendered==name);
        else {
            int ink=0;for(int y=196;y<232;++y)for(int x=251;x<299;++x)if(display.pixels[y*400+x]==GxEPD_BLACK)++ink;
            assert(!old&&ink>80);
        }
        assert(previousBottom<232);
        output(folder+"/weather-"+std::to_string(old)+"-"+std::to_string(strlen(name)));
    }
    oldWeather=false;puts("Long weather complete and separated for current and old-data states passed.");
    puts("Production drawing bodies, native fonts, long countdown, custom timetable and page identities passed.");
}
