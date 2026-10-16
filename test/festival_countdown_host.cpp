#include <cassert>
#include <cstring>
#include <cstdio>
#include "festival_countdown_core.h"
using namespace festival_countdown;
static void check(const Settings& settings,int y,int m,int d,const char* label,int days) {
    const auto result=resolve(settings,y,m,d);
    assert(result.found && !strcmp(result.label,label) && result.days==days);
}
int main() {
    Settings s;
    check(s,2026,12,31,"元旦",1);
    check(s,2027,1,1,"元旦",0);
    s.mode="festival";s.target="spring-festival";
    check(s,2026,2,16,"春节",1);check(s,2026,2,17,"春节",0);
    check(s,2026,2,18,"春节",353);
    check(s,2025,1,30,"春节",383); // Leap lunar year exceeds the old 370-day search.
    s.target="dragon-boat";check(s,2026,6,18,"端午节",1);
    s.target="mid-autumn";check(s,2026,9,24,"中秋节",1);
    s.target="qingming";check(s,2026,4,4,"清明节",1);
    s.mode="term";s.term=23;
    check(s,2026,12,21,"冬至",1);check(s,2026,12,22,"冬至",0);
    s.mode="auto-term";check(s,2026,12,23,"小寒",13);
    check(s,2026,10,17,"霜降",6);
    // Nongli's old approximate formula yields 4 April; HKO specifies 5 April.
    s.mode="term";s.term=6;check(s,2026,4,4,"清明",1);
    s.mode="custom";s.label="宝宝生日";s.date="20280229";
    check(s,2028,2,28,"宝宝生日",1);check(s,2028,2,29,"宝宝生日",0);
    check(s,2028,3,1,"宝宝生日",-1);
    s.date="20991231";check(s,2025,1,1,"宝宝生日",27392);
    s.date="20270229";assert(!resolve(s,2026,1,1).found);
    s.mode="term";s.term=24;assert(!resolve(s,2026,1,1).found);
    s.mode="auto-term";assert(!resolve(s,2099,12,31).found);
    puts("Festival countdown: lunar festivals, HKO solar terms, today, next year, leap date and expiry passed");
}
