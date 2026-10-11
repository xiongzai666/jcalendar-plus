#pragma once
#include "Arduino.h"
#include <functional>
#include <string>
enum { HTTP_POST, UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status=0; uint8_t buf[1436]={}; size_t currentSize=0,totalSize=0; std::string name,filename,type; };
class WebServer {
 public:
  HTTPUpload file;
  std::function<void()> finish,chunk;
  int code=0;
  HTTPUpload& upload() { return file; }
  void on(const char*,int,std::function<void()> fn,std::function<void()> ufn) { finish=fn;chunk=ufn; }
  void send(int status,const char*,const char*) { code=status; }
};
