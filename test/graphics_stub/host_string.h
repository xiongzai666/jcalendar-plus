#pragma once
#include <string>
class String {
    std::string value;
public:
    String(const char* s=""):value(s?s:""){} String(const std::string& s):value(s){}
    String(int n):value(std::to_string(n)){}
    size_t length()const{return value.size();} bool isEmpty()const{return value.empty();}
    int toInt()const{return std::atoi(value.c_str());}
    char charAt(size_t i)const{return value[i];}
    bool equals(const String& s)const{return value==s.value;}
    const char* c_str()const{return value.c_str();} char operator[](size_t i)const{return value[i];}
    String substring(size_t a,size_t b)const{return value.substr(a,b-a);}
    String substring(size_t a)const{return value.substr(a);}
    bool operator==(const char* s)const{return s?value==s:value.empty();}
    bool operator<(const String& s)const{return value<s.value;}
    String operator+(const String& s)const{return value+s.value;}
    String& operator+=(const String& s){value+=s.value;return *this;}
};
inline String operator+(const char* left,const String& right){return String(left)+right;}
