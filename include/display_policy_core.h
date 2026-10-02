#pragma once
#include <cstring>
#include <ctime>
// Screen labels use eight compact directions; raw API bearings are retained.
inline const char* compactWindLabel(const char* value) {
    const char* full[]={"北东北风","东东北风","东东南风","南东南风","南西南风","西西南风","西西北风","北西北风"};
    const char* compact[]={"东北风","东风","东南风","南风","西南风","西风","西北风","北风"};
    for(unsigned i=0;i<8;++i)if(!strcmp(value,full[i]))return compact[i];
    return value;
}
struct FestivalInlineLayout { int labelX,numberX,unitX; };
inline FestivalInlineLayout festivalInlineLayout(int left,int width,int label,int number,int unit) {
    const int start=left+(width-label-8-number-2-unit)/2;
    return {start,start+label+8,start+label+8+number+2};
}
inline bool weekColumnDate(const tm& current,int column,tm& result) {
    if(current.tm_year<125||column<0||column>5)return false;
    result=current;result.tm_mday+=column-(current.tm_wday+6)%7;
    result.tm_hour=12;result.tm_min=result.tm_sec=0;result.tm_isdst=-1;
    return mktime(&result)!=time_t(-1);
}
