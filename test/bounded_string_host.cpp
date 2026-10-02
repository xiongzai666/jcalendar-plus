#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#include "bounded_string_core.h"

int main() {
    std::string source(3500,'x'), output;
    const auto reader = [&](char* target, size_t capacity) -> size_t {
        if(source.size()+1>capacity)return 0;
        memcpy(target,source.c_str(),source.size()+1);return source.size()+1;
    };
    const auto use = [&](const char* text,size_t length){output.assign(text,length);};
    assert(readBoundedString(3500,reader,use)&&output==source);
    output="unchanged";source.push_back('x');
    assert(!readBoundedString(3500,reader,use)&&output=="unchanged");
    assert(!readBoundedString(8,[](char*,size_t){return size_t(0);},use));
    assert(!readBoundedString(8,[](char* target,size_t){memcpy(target,"bad",3);return size_t(3);},use));
    assert(!readBoundedString(8,[](char*,size_t){return size_t(10);},use));
    source="";assert(readBoundedString(8,reader,use)&&output.empty());
    puts("Bounded string: heap-backed maximum config, oversize, failed read and terminator checks passed");
}
