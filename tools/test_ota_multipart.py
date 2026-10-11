"""Compile the actual multipart parser against a deterministic TCP boundary."""
import argparse,os,re,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--parser',type=Path,default=ROOT/'src/webserver_parsing.cpp');args=p.parse_args()
source=args.parser.read_text(encoding='utf8')
out=ROOT/'host-build/ota-multipart';out.mkdir(parents=True,exist_ok=True)
def function(name):
    start=re.search(r'(?:bool|int|void) WebServer::'+name+r'\(',source).start()
    begin=source.index('{',start);depth=1;end=begin+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end].replace('WebServer::','MultipartServer::')
(out/'multipart-parser.inc').write_text('\n'.join(function(n) for n in ['_uploadWriteByte','_uploadReadByte','_parseForm','_parseFormUploadAborted']),encoding='utf8')
cxx=os.environ.get('J_CALENDAR_CXX') or shutil.which('clang++') or shutil.which('g++')
if not cxx:raise SystemExit('Set J_CALENDAR_CXX to a C++ compiler')
cmd=[cxx,'c++'] if Path(cxx).stem=='zig' else [cxx]
exe=out/'test.exe'
subprocess.run(cmd+['-std=c++11','-DSI_DRIVER=15','-Itest/ota_upload_stub','-Iinclude','-I'+str(out),'test/ota_multipart_host.cpp','src/local_ota.cpp','-o',str(exe)],cwd=ROOT,check=True)
subprocess.run([str(exe)],check=True,timeout=5)
