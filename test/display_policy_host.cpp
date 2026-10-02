#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include "display_policy_core.h"
#include "page_navigation_core.h"
int main() {
    int page=2;
    for(int expected : {1,0,3,2}) {page=nextPageInOrder(page,true);assert(page==expected);}
    for(int expected : {3,0,1,2}) {page=nextPageInOrder(page,false);assert(page==expected);}
    assert(pageOnStartup(0,2,false)==2); // Power-on / restart opens the configured home page.
    assert(pageOnStartup(0,2,true)==0); // Sleep wake retains the user's selected page.
    assert(pageOnStartup(3,9,false)==2);
    assert(pageOnStartup(9,2,true)==2);
    assert(!strcmp(compactWindLabel("西西北风"),"西北风"));
    assert(!strcmp(compactWindLabel("北东北风"),"东北风"));
    assert(!strcmp(compactWindLabel("无风"),"无风"));
    for(int number : {20,43,60}) {
        const auto layout=festivalInlineLayout(250,150,48,number,16);
        assert(layout.unitX-layout.numberX-number==2);
        assert(layout.labelX>=250&&layout.unitX+16<=400);
    }
    tm friday={};friday.tm_year=126;friday.tm_mon=9;friday.tm_mday=2;friday.tm_wday=5;
    tm column={};
    assert(weekColumnDate(friday,0,column)&&column.tm_mon==8&&column.tm_mday==28);
    assert(weekColumnDate(friday,3,column)&&column.tm_mon==9&&column.tm_mday==1);
    assert(weekColumnDate(friday,5,column)&&column.tm_mon==9&&column.tm_mday==3);
    assert(!weekColumnDate(friday,6,column));
    puts("Display policy: page order, cold/warm startup, compact wind, unit gap and week dates passed");
}
