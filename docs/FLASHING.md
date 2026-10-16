# 编译与刷机

## 先选驱动

本版提供经典 ESP32、4 MB Flash 的 `z15`、`z21`、`z98`、`1680` 四种驱动包。映射见 [驱动选择头文件](../include/GxEPD2_display_selection_new_style.h)；未知屏幕应先确认控制器。本版不提供 ESP32-C3／S3 通用包。

```sh
python -m pip install platformio==6.2.0
python -m platformio run -e z15
```

使用预编译固件时，从 [发行页](https://github.com/xiongzai666/jcalendar-plus/releases) 下载匹配驱动的 ZIP，先核对 `SHA256SUMS.txt`，再解压。自行编译的输出位于 `.pio/build/ENV`，其中 `ENV` 为所选环境；首次构建需要联网下载依赖。

## 分区与刷写地址

| 段 | 地址 | 用途 |
| --- | --- | --- |
| bootloader.bin | `0x1000` | ESP32 引导 |
| partitions.bin | `0x8000` | 本项目分区表 |
| boot_app0.bin | `0xe000` | 初始 OTA 选择数据 |
| firmware.bin | `0x10000` | 首个应用分区 app0 |

`min_spiffs.csv`：NVS `0x9000`、app0 `0x10000`、app1 `0x1f0000`；两个应用各 `0x1e0000`。下载包内 manifest 和 SHA256 是该包的依据。

## 首次通过 USB 刷机

使用数据线、确认串口驱动和端口。以下仅用于全新设备；擦除会清空 Wi-Fi、Token、课程等。未知成品先保存自己的私密备份，禁止把备份上传仓库。

```sh
python -m pip install esptool==4.11.0
python -m esptool --chip esp32 --port PORT erase_flash
python -m esptool --chip esp32 --port PORT write_flash 0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
```

也可首次使用 `python -m platformio run -e z15 -t upload`；地址由当前 PlatformIO 环境确定，确认板子分区匹配。

开机后等待刷新结束，双击上键，或长按约 5 秒后松开，进入配置热点。热点随机密码显示在屏幕上。固件未预置个人课表、目标、天气账号或远程域名。

## 保留配置升级与回退

连接配置热点，在网页 Update 上传所选驱动的 **firmware.bin**。不要上传 ZIP、完整备份或其他段。不擦 NVS，公开版初始化不会重写已存课表。

升级前保存网页配置导出和自己的原固件私密备份。OTA 后活动分区可能是 app1，不能认为串口写 `0x10000` 就更新了正在启动的固件；优先使用本机网页更新。

从 `2.3.2-rc.1` 起，应用镜像包含固定驱动标识。Update 在写入前核对驱动与版本，上传中断、文件不完整或一次提交多个文件均不激活新分区。仅在完整请求结束、ESP32 镜像校验通过后切换启动分区；不写 NVS 或 SPIFFS。

`2.3.1-rc.2` 及更早的镜像没有此驱动标识，新版 Update 会拒绝它们。从旧版升级到新版可使用旧版 Update，并事先按发布包核对驱动和 SHA256；从新版回退旧版须经串口，核对当时的活动分区后恢复已验证应用或自己的当前完整备份。不要为回退而擦除配置。该检查用于避免误选固件，不是固件签名认证。

未知原项目／成品分区升级需先核对分区和 Flash 大小，不能套用保留数据承诺。bootloader／分区改变需单独制定迁移，不直接擦除用户配置。

## 刷机后检查

校时和联网成功后等刷新结束，依次切四页。确认上下键、热点、本机页面、云端版本应用及天气缓存。编译通过不代替以上实机验收。
