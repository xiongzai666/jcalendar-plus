#pragma once
#include <cstring>
#include <nongli.h>
#include "solar_term_data.h"

namespace festival_countdown {
struct Settings {
    const char* mode="auto-festival";
    const char* target="";
    int term=0;
    const char* label="";
    const char* date="";
};
struct Result { const char* label; int days; bool found; };
struct Festival { const char* id; const char* label; int solar; int lunar; };
static const Festival FESTIVALS[] = {
    {"new-year","元旦",101,0},{"labour","劳动节",501,0},{"national","国庆节",1001,0},
    {"spring-festival","春节",0,101},{"lantern","元宵节",0,115},
    {"dragon-boat","端午节",0,505},{"qixi","七夕节",0,707},
    {"mid-autumn","中秋节",0,815},{"double-ninth","重阳节",0,909},
    {"qingming","清明节",0,0},
};
inline bool leap(int y) { return y%4==0 && (y%100!=0 || y%400==0); }
inline int monthLength(int y,int m) {
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return m>=1 && m<=12 ? days[m-1]+(m==2 && leap(y)) : 0;
}
inline bool validDate(int y,int m,int d) {
    return y>=2025 && y<=2099 && d>=1 && d<=monthLength(y,m);
}
inline int ordinal(int y,int m,int d) {
    const int previous=y-1;
    int result=previous*365+previous/4-previous/100+previous/400+d;
    for (int i=1;i<m;++i) result+=monthLength(y,i);
    return result;
}
inline int solarTermDay(int year,int term) {
    return year>=2025 && year<=2099 && term>=0 && term<24 ? SOLAR_TERM_DAYS[year-2025][term] : 0;
}
inline void solarTermOrdinals(int year,int* result) {
    if (year<2025 || year>2099) { nl_year_jq(year,result); return; }
    for (int i=0;i<24;++i) result[i]=ordinal(year,i/2+1,solarTermDay(year,i))-ordinal(year,1,1)+1;
}
inline Result resolve(const Settings& settings,int year,int month,int day) {
    const Result missing={"暂无日期",0,false};
    if (!validDate(year,month,day)) return missing;
    const int today=ordinal(year,month,day);
    if (!strcmp(settings.mode,"custom")) {
        if (strlen(settings.date)!=8) return missing;
        for (int i=0;i<8;++i) if (settings.date[i]<'0' || settings.date[i]>'9') return missing;
        const char* p=settings.date;
        const int y=(p[0]-'0')*1000+(p[1]-'0')*100+(p[2]-'0')*10+p[3]-'0';
        const int m=(p[4]-'0')*10+p[5]-'0',d=(p[6]-'0')*10+p[7]-'0';
        if (!validDate(y,m,d)) return missing;
        return {settings.label,ordinal(y,m,d)-today,true};
    }
    const bool anyTerm=!strcmp(settings.mode,"auto-term"),oneTerm=!strcmp(settings.mode,"term");
    if (anyTerm || oneTerm) {
        if (oneTerm && (settings.term<0 || settings.term>=24)) return missing;
        for (int y=year;y<=year+1 && y<=2099;++y) for (int i=0;i<24;++i) {
            if (oneTerm && i!=settings.term) continue;
            const int days=ordinal(y,i/2+1,solarTermDay(y,i))-today;
            if (days>=0) return {nl_jq_text[i],days,true};
        }
        return missing;
    }
    const bool automatic=!strcmp(settings.mode,"auto-festival");
    if (!automatic && strcmp(settings.mode,"festival")) return missing;
    int lunarDays[31]={},cachedYear=0,cachedMonth=0;
    // A lunar year can contain 384 days; 370 misses an annual target in leap years.
    for (int offset=0;offset<=400 && year<=2099;++offset) {
        if (year!=cachedYear || month!=cachedMonth) {
            nl_month_days(year,month,lunarDays);cachedYear=year;cachedMonth=month;
        }
        const int lunar=lunarDays[day-1],solar=month*100+day;
        for (const auto& festival:FESTIVALS) {
            if (!automatic && strcmp(settings.target,festival.id)) continue;
            const bool qingming=!strcmp(festival.id,"qingming") && month==4 && day==solarTermDay(year,6);
            if ((festival.solar && festival.solar==solar) || (festival.lunar && lunar>0 && festival.lunar==lunar) || qingming)
                return {festival.label,offset,true};
        }
        if (++day>monthLength(year,month)) { day=1;if (++month>12) { month=1;++year; } }
    }
    return missing;
}
}
