#include <cassert>
#include <map>
#include <string>
#include "public_defaults.h"
struct FakePreferences {
 std::map<std::string,int> ints;
 bool isKey(const char* key){return ints.count(key);}
 size_t putInt(const char* key,int value){ints[key]=value;return sizeof(int);}
};
int main(){
 FakePreferences fresh; initializePublicPreferences(fresh);
 assert(fresh.ints.at("SI_TYPE")==2);assert(fresh.ints.size()==1);
 fresh.ints["SI_TYPE"]=3;initializePublicPreferences(fresh);assert(fresh.ints.at("SI_TYPE")==3);
 assert(std::string(DEFAULT_STUDY_SCHEDULE).empty());
 assert(std::string(DEFAULT_REMOTE_HOST).empty());
}
