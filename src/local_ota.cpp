#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include "local_ota.h"
#include "ota_image_core.h"
#include "ota_identity.h"

namespace {
OtaImageGuard guard(OTA_PROJECT_NAME,0);
bool active=false,writing=false,completed=false,failed=false,requestOpen=false;
const char* message="请选择固件文件";
void reject(const char* reason) {
  if (writing) Update.abort();
  writing=false;failed=true;completed=false;message=reason;
}
void upload(WebServer& server) {
  HTTPUpload& file=server.upload();
  if (file.status==UPLOAD_FILE_START) {
    if (requestOpen) { reject("每次只能上传一个固件文件");return; }
    if (writing) Update.abort();
    requestOpen=true;
    active=true;writing=false;completed=false;failed=false;message="上传尚未完成";
    const esp_partition_t* partition=esp_ota_get_next_update_partition(nullptr);
    guard=OtaImageGuard(OTA_PROJECT_NAME,partition ? partition->size : 0);
    if (!partition) reject("没有可用升级分区");
  } else if (file.status==UPLOAD_FILE_ABORTED) {
    reject("上传已中断，请重新上传");active=false;requestOpen=false;
  } else if (file.status==UPLOAD_FILE_WRITE) {
    if (!active || failed) return;
    const size_t before=guard.size();
    if (!guard.feed(file.buf,file.currentSize)) { reject(guard.error());return; }
    if (!writing && guard.ready()) {
      // Only an inactive application partition is selected. No configuration writes.
      if (!Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH)) { reject("无法开始升级");return; }
      writing=true;
      uint8_t* prefix=const_cast<uint8_t*>(guard.prefix());
      if (Update.write(prefix,OtaImageGuard::PrefixSize)!=OtaImageGuard::PrefixSize) {
        reject("固件写入失败");return;
      }
      const size_t buffered=OtaImageGuard::PrefixSize-before;
      if (file.currentSize>buffered &&
          Update.write(file.buf+buffered,file.currentSize-buffered)!=file.currentSize-buffered)
        reject("固件写入失败");
    } else if (writing && Update.write(file.buf,file.currentSize)!=file.currentSize) {
      reject("固件写入失败");
    }
  } else if (file.status==UPLOAD_FILE_END) {
    if (!active || failed) return;
    active=false;
    if (!writing || !guard.complete() || file.totalSize!=guard.size()) {
      reject("固件不完整，请重新上传");return;
    }
    // FILE_END precedes the final multipart boundary and the request callback.
    completed=true;
  }
}
}
void installLocalOta(WebServer& server) {
  // WiFiManager invokes this before registering its own /u route. WebServer
  // appends handlers and uses the first match, so vendor upload cannot bypass us.
  server.on("/u",HTTP_POST,[&server]() {
    if (!completed || failed || !writing)
      reject(failed ? message : "固件不完整，请重新上传");
    else if (!Update.end(true)) reject("固件校验失败");
    else { writing=false;message="升级成功，正在重启"; }
    const bool success=completed && !failed;
    server.send(success ? 200 : 400,"text/plain; charset=utf-8",message);
    active=false;completed=false;requestOpen=false;
    if (success) { delay(500);ESP.restart(); }
  },[&server]() { upload(server); });
}
