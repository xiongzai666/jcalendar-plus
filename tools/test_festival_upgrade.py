"""Execute the production request-revision selection with legacy and migrated NVS."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/remote_sync.cpp').read_text(encoding='utf8')
sync=source[source.index('RemoteSyncResult remote_sync(bool retryAttempt)'):]
statement=re.search(r'const uint32_t revision\s*=.*?;',sync,re.S).group()
with tempfile.TemporaryDirectory(prefix='festival-upgrade-') as directory:
    folder=Path(directory);(folder/'Preferences.h').write_text('#pragma once\n')
    code='''#include <cassert>
#include <cstdint>
#include <string>
#include <map>
#include <cstdio>
#include "_preference.h"
struct StoredSettings {
 std::map<std::string,unsigned> values;
 bool isKey(const char* key){return values.count(key);}
 unsigned getUInt(const char* key,unsigned fallback){return values.count(key)?values[key]:fallback;}
};
unsigned requestedRevision(StoredSettings& pref){'''+statement+'''return revision;}
int main(){
 StoredSettings legacy;legacy.values[PREF_REMOTE_REV]=25;
 assert(requestedRevision(legacy)==0); // Re-fetch the same cloud revision after upgrading old firmware.
 StoredSettings migrated=legacy;migrated.values[PREF_FESTIVAL_CD]=1;
 assert(requestedRevision(migrated)==25); // Normal polling must still use its cache.
 StoredSettings fresh;assert(requestedRevision(fresh)==0);
 puts("Festival upgrade: fetch legacy v25 in full once, then retain revision-based polling");
}
'''
    (folder/'test.cpp').write_text(code,encoding='utf8')
    cpp=os.environ.get('J_CALENDAR_CXX','g++');compiler=[cpp,'c++'] if Path(cpp).stem=='zig' else [cpp]
    binary=folder/'test.exe'
    subprocess.run(compiler+['-std=c++11','-I'+str(folder),'-I'+str(root/'include'),str(folder/'test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
