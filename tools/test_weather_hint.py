"""Exercise public current/daily precipitation wording against the real policy body."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
weather=(root/'src/weather.cpp').read_text(encoding='utf8')
body=weather[weather.index('String weather_dismissal_hint('):weather.index('int8_t weather_type()')]
source=r'''
#include <cassert>
#include <ctime>
#include "graphics_stub/host_string.h"
#include "weather_cache_core.h"
WeatherCacheData cached={};
struct Day{String textDay,textNight;}dailyWeather[3];
using DailyWeather=Day;
struct Current{String text;}_weather_now;
bool weather_now_available(){return !_weather_now.text.isEmpty();}
'''+body+r'''
int main(){
 const time_t now=1800000000;cached.nowReadAt=cached.dailyReadAt=now;
 _weather_now.text="晴";dailyWeather[0].textDay="多云";dailyWeather[0].textNight="雾";
 assert(weather_dismissal_hint(false,now).isEmpty());
 assert(weather_dismissal_hint(true,now).isEmpty());
 _weather_now.text="小雨";assert(weather_dismissal_hint(false,now)=="当前有雨，带伞");
 _weather_now.text="雪";assert(weather_dismissal_hint(true,now)=="当前有雪，带伞");
 _weather_now.text="晴";dailyWeather[0].textDay="雨夹雪";
 assert(weather_dismissal_hint(false,now)=="预报雨雪，带伞");
 cached.dailyFailed=true;assert(weather_dismissal_hint(false,now).isEmpty());
 cached.nowFailed=true;assert(weather_dismissal_hint(false,now)=="天气未更新");
 puts("Public dry/current/daily rain/snow and stale-weather policy passed");
}
'''
with tempfile.TemporaryDirectory(prefix='public-weather-hint-') as temp:
    out=Path(temp);(out/'test.cpp').write_text(source,encoding='utf8')
    cxx=os.environ.get('J_CALENDAR_CXX','g++');compiler=[cxx,'c++'] if Path(cxx).stem=='zig' else [cxx]
    binary=out/'test.exe'
    subprocess.run(compiler+['-std=c++11','-I'+str(root/'test'),'-I'+str(root/'include'),str(out/'test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
