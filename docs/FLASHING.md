# 编译与刷机

## 先选驱动

本版只列经典 ESP32、4 MB Flash 的 `z15` / `z21` / `z98` / `1680`。屏幕型号映射以 `include/GxEPD2_display_selection_new_style.h` 为准；未知型号先确认控制器。不要选择 ESP32-C3/S3 通用包。

```sh
python -m pip install platformio==6.2.0
python -m platformio run -e z15
```

源码包不附带过时固件；构建输出在 `.pio/build/ENV`。有网络依赖，首次构建需下载固定版本。

## 分区与刷写地址

| 段 | 地址 | 用途 |
| --- | --- | --- |
| bootloader.bin | `0x1000` | ESP32 引导 |
| partitions.bin | `0x8000` | 本项目分区表 |
| boot_app0.bin | `0xe000` | 初始 OTA 选择数据 |
| firmware.bin | `0x10000` | 首个应用分区 app0 |

`min_spiffs.csv`：NVS `0x9000`、app0 `0x10000`、app1 `0x1f0000`；两个应用各 `0x1e0000`。下载包内 manifest 和 SHA256 是该包的依据。

## 全新设备

使用数据线、确认串口驱动和端口。以下仅用于全新设备；擦除会清空 Wi-Fi、Token、课程等。未知成品先保存自己的私密备份，禁止把备份上传仓库。

```sh
python -m pip install esptool==4.11.0
python -m esptool --chip esp32 --port PORT erase_flash
python -m esptool --chip esp32 --port PORT write_flash 0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

也可首次使用 `python -m platformio run -e z15 -t upload`；地址由当前 PlatformIO 环境确定，确认板子分区匹配。

首次开机长按唤醒键进入配置，设备热点随机密码显示于屏幕。固件未预置个人课表、目标、天气帐号或远程域名。

## 已有本版设备升级

连接配置热点，在网页 Update 上传所选驱动的 **firmware.bin**。不要上传 ZIP、完整备份或其他段。不擦 NVS，公开版初始化不会重写已存课表。

升级前保存网页配置导出和自己的原固件私密备份。OTA 后活动分区可能是 app1，不能认为串口写 `0x10000` 就更新了正在启动的固件；优先使用本机网页更新。

未知原项目／成品分区升级需先核对分区和 Flash 大小，不能套用保留数据承诺。bootloader／分区改变需单独制定迁移，不直接擦除用户配置。

## 刷后检查

校时和联网成功后等刷新结束，依次切四页。确认上下键、热点、本机页面、云端版本应用及天气缓存。编译通过不代替以上实机验收。
