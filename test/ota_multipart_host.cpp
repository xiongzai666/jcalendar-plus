// Real SDK/project multipart parser, with only the TCP and Arduino boundaries simulated.
#include <Update.h>
#include <WebServer.h>
#include "local_ota.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#define F(x) x
#define FPSTR(x) x
#define log_v(...) ((void)0)
#define log_e(...) ((void)0)
#define WEBSERVER_MAX_POST_ARGS 32
#define HTTP_UPLOAD_BUFLEN 1436
#define HTTP_MAX_POST_WAIT 5000
TestUpdate Update;
TestEsp ESP;
class String {
  std::string value;
public:
  String(const char* v=""):value(v){}
  String(std::string v):value(v){}
  size_t length() const{return value.size();}
  bool startsWith(String s)const{return value.compare(0,s.length(),s.value)==0;}
  bool equalsIgnoreCase(String s)const{
    auto a=value,b=s.value;
    std::transform(a.begin(),a.end(),a.begin(),::tolower);
    std::transform(b.begin(),b.end(),b.begin(),::tolower);return a==b;
  }
  int indexOf(char c)const{auto p=value.find(c);return p==std::string::npos?-1:int(p);}
  String substring(size_t first)const{return first>length()?String():String(value.substr(first));}
  String substring(size_t first,size_t last)const{return value.substr(first,last-first);}
  bool operator==(String s)const{return value==s.value;}
  bool operator!=(String s)const{return !(*this==s);}
  String& operator+=(String s){value+=s.value;return *this;}
  friend String operator+(String a,String b){return a.value+b.value;}
  const char* c_str()const{return value.c_str();}
  operator std::string()const{return value;}
};
static const char* filename="filename";
static const char* Content_Type="Content-Type";
namespace mime {enum {txt};struct Entry{const char* mimeType;};static Entry mimeTable[]={{"text/plain"}};}
class WiFiClient {
  std::string bytes;size_t position=0;bool eofSeen=false;unsigned emptyReads=0;
public:
  bool keepOpen=false;
  explicit WiFiClient(std::string input):bytes(input){}
  int available(){return int(bytes.size()-position);}
  bool connected(){return !eofSeen||keepOpen;}
  void setTimeout(unsigned){}
  int read(){if(position<bytes.size())return static_cast<unsigned char>(bytes[position++]);eofSeen=true;return -1;}
  String readStringUntil(char delim){
    std::string out;int c;
    while((c=read())>=0){if(c==delim)return out;out+=char(c);}
    testMillis()+=1000;
    if(++emptyReads>30)throw std::runtime_error("multipart parser keeps reading after EOF/timeout");
    return out;
  }
};
struct RequestArgument {String key,value;};
struct Handler {
  WebServer& server;
  bool canUpload(String){return true;}
  void upload(class MultipartServer& server,String,HTTPUpload& file);
};
class MultipartServer {
public:
  WebServer callbacks;Handler handler{callbacks};Handler* _currentHandler=&handler;
  String _currentUri="/u";
  RequestArgument* _postArgs=nullptr;int _postArgsLen=0;
  RequestArgument* _currentArgs=nullptr;int _currentArgCount=0;
  std::unique_ptr<HTTPUpload> _currentUpload;
  bool hasArg(const char*){return false;}String arg(const char*){return "";}
  bool _parseForm(WiFiClient&,String,uint32_t);
  bool _parseFormUploadAborted();void _uploadWriteByte(uint8_t);int _uploadReadByte(WiFiClient&);
  ~MultipartServer(){delete[] _postArgs;delete[] _currentArgs;}
};
void Handler::upload(MultipartServer&,String,HTTPUpload& file){server.file=file;server.chunk();}
#include "multipart-parser.inc"
static std::string image(){
  std::string b(400,'\0');b[0]=char(0xe9);b[1]=1;b[23]=1;b[26]=0x40;b[27]=0x3f;b[28]=0x44;b[29]=1;
  b[32]=0x32;b[33]=0x54;b[34]=char(0xcd);b[35]=char(0xab);
  std::memcpy(&b[288],"JOTA",4);std::strcpy(&b[292],"jcalendar-plus-z15");std::strcpy(&b[324],"2.3.2-rc.1");return b;
}
static void scenario(bool terminal,bool livePeer){
  MultipartServer server;installLocalOta(server.callbacks);Update.activated=false;testMillis()=0;
  std::string body="--bound\r\nContent-Disposition: form-data; name=\"update\"; filename=\"firmware.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n"+image()+"\r\n--bound";
  if(terminal)body+="--\r\n";
  WiFiClient client(body);client.keepOpen=livePeer;
  bool ok=server._parseForm(client,"bound",body.size()+(terminal?0:4));
  if(!terminal){
    if(ok||Update.open||Update.activated||millis()>12000)throw std::runtime_error("unfinished multipart not bounded/aborted");
  }else{
    if(!ok||Update.activated)throw std::runtime_error("valid request parser changed activation order");
    server.callbacks.finish();
    if(server.callbacks.code!=200||!Update.activated)throw std::runtime_error("valid upload rejected");
  }
}
int main(){
  try{
    scenario(true,false);scenario(false,false);scenario(false,true);
    MultipartServer server;installLocalOta(server.callbacks);
    WiFiClient headers("--bound\r\n");headers.keepOpen=true;testMillis()=0;
    if(server._parseForm(headers,"bound",200)||millis()>12000)
      throw std::runtime_error("incomplete form headers not bounded");
    MultipartServer form;
    const std::string fields="--bound\r\nContent-Disposition: form-data; name=\"a\"\r\n\r\nalpha\r\n--bound\r\nContent-Disposition: form-data; name=\"b\"\r\n\r\nbeta\r\n--bound--\r\n";
    WiFiClient parameters(fields);testMillis()=0;
    if(!form._parseForm(parameters,"bound",fields.size())||form._currentArgCount!=2||form._currentArgs[1].value!="beta")
      throw std::runtime_error("ordinary configuration form changed");
  }
  catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
  std::puts("Real multipart parser: valid request, EOF after FILE_END, idle peer bounded and Update aborted");
}
