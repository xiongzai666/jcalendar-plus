# 开发与维护

## 目录与职责

| 目录 | 职责 |
| --- | --- |
| `src` / `include` | ESP32 固件、渲染、网络、课程策略、持久化 |
| `lib/GxEPD2` | 随项目保留的屏幕驱动 |
| `remote/public` | 手机页面，草稿、核对、导入导出 |
| `remote/cloud-functions` | 登录、配置、同步、历史和日志 API |
| `test` / `remote/test` | C++ 策略、Python 发布、Node 前后端测试 |
| `tools` / `remote/tools` | 验证、发布、凭据生成与证书检查 |

## 统一验证

```sh
npm ci --prefix remote
python tools/verify.py --environment z15
python tools/verify.py --environment z21
python tools/verify.py --environment z98
python tools/verify.py --environment 1680
```

门禁：Python 发布工具测试、Node 测试、全部业务 JS 语法和本地 import、网页 SHA256 清单、目标编译、16 组 C++ 主机测试、分区容量。CI 使用 Node 22、Python 3.11、PlatformIO 6.2.0；固定 registry 版本和 Git 依赖提交见 `platformio.ini`。

Windows 可设置 `J_CALENDAR_NODE`、`J_CALENDAR_CXX` 到 Node 与 Zig 的路径；不要求本机路径写入源码。`--skip-build` 只复用已与当前源匹配的构建，不能证明改动后的固件编译过。

## 配置默认值

- 固件首次启动仅初始化缺失的默认页，不覆盖课表、倒计时、天气模式和已有 Token。
- `include/school_schedule_data.h` 的默认课程为空；作息是可替换示例。
- `remote/cloud-functions/lib/defaults.js` 和 `examples/settings.example.json` 是一致的可编辑模板，不是个人课表；已部署存储存在时不会用模板替换它。
- 对学校逻辑加测试时创建显式样例，避免依赖产品默认值。

## 发布步骤

1. 修改 `include/version.h` 和 CHANGELOG。
2. 四种环境新鲜编译，跑统一门禁；目标分区 `0x1e0000`，达到 95% 需审查剩余空间。
3. 检查默认服务／凭据、敏感路径、示例配置和文档链接。只提交明确源码，不提交 `.pio`、`.env`、个人备份。
4. 提交已验证源，保持工作树干净，然后生成包：

```sh
python tools/release_bundle.py --boot-app0 PATH_TO_PLATFORMIO_FRAMEWORK/tools/partitions/boot_app0.bin
```

`PATH_TO_PLATFORMIO_FRAMEWORK` 是本机 PlatformIO 安装的 `framework-arduinoespressif32` 包目录。工具生成 `.releases/jcalendar-plus-VERSION-ENV.zip` 与总 SHA256 文件；可用 `--environment` 单独选目标。打包不编译，要求输入就是步骤 2 验证过的构建。

每包包含 4 个明确地址的段、清单、内部 SHA256 与刷机说明。应用更新用 `firmware.bin`。包中不应出现 NVS、完整 Flash 或用户配置。来源提交在 manifest 中；只发布选定公开分支，禁止镜像推送继承的私有历史。

5. 发布前独立复核包、许可证处理状态和 UI。新首启界面需要实体设备验收；编译和主机测试不能替代实屏检查。
6. GitHub 发布／部署需要维护者明确批准，本地提交不代表批准发布。

## 字体维护

普通文字使用 U8g2 字体；常见科目使用小范围位图。修改任意科目仍可回退普通字体，勿为字形无限扩大固件。

```sh
python tools/generate_subject_font.py --font /path/to/NotoSansSC-VF.ttf
```

此可选操作需 Pillow 与对应可变字体。保留字体来源／授权说明，生成后编译并检查实屏。无须重生成字体即可构建固件。

## 服务限制

单设备 Blob 状态为当前架构。保留已有版本冲突检查，未实现分布式事务或并发保存保证；不要宣传为多人协同服务。日志不存 SSID／密码／Token；上报窗口有限，离线期间的详情需等待恢复联网。
