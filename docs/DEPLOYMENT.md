# 手机网页和云端部署

每位使用者自行部署服务；仓库和固件没有维护者生产域名。服务当前为单设备设计。

## 本地预览

```sh
npm ci --prefix remote
cd remote
npm run dev
```

浏览器访问 `http://localhost:8787`，演示密码 `local-demo-only`。状态在内存中，进程停止即丢失。不要把此 HTTP 演示暴露到互联网。`LOCAL_ADMIN_PASSWORD` / `LOCAL_DEVICE_TOKEN` 仅用于本地测试；生产不使用这些默认值。

## EdgeOne Pages

1. 将本仓库作为部署来源，项目根目录选 `remote`，Node.js 22；构建命令 `npm ci && npm run build`，输出目录 `public`。`remote/edgeone.json` 提供同等配置。
2. 启用云函数与 Pages Blob 存储；入口 `cloud-functions/api/[[default]].js` 用 `getStore`，存储名 `jcalendar`，按平台当前控制台配置绑定。
3. 运行 `npm run prepare:secrets`，输入至少 12 字符管理密码，把生成结果仅放入生产环境变量／密码管理器。生成过程会显示设备 Token，不要保存到聊天、构建日志、仓库或截图。
4. 设置下列环境变量并重新部署，使云函数可读取新值：

| 名称 | 用途 |
| --- | --- |
| `ADMIN_PASSWORD_SCRYPT` | 管理密码的加盐派生值 |
| `SESSION_SECRET` | Cookie 签名秘密 |
| `DEVICE_TOKEN_SHA256` | 设备 Token 的 SHA256，生产不存原文 |
| `PUBLIC_ORIGIN` | 必填：网页实际 HTTPS 源，例如 `https://config.example.com`，不能有路径或末尾斜杠 |

`remote/.env.example` 只是变量名说明，不能当有效生产配置。缺少秘密或有效生产源时 API 返回 503；`/api/health` 是存活检测，不代表已完成安全配置。

5. 配置自己的 HTTPS 域名和证书；DNS 记录以平台控制台为准。登录网页可用、API 正常，再配置设备。

## 设备端关联

- **域名**：只填你自己的域名，不带 `https://`、路径或端口。
- **设备 Token**：填步骤 3 生成的原文；此 Token 只能读取设备配置／回报状态，不能当手机管理密码。
- **根证书 PEM**：填服务器证书对应的可信根证书，保持设备 TLS 校验。

```sh
cd remote
node tools/show-device-ca.mjs config.example.com
```

该命令要求 Node 能直连目标 443；它先校验系统信任再输出可识别的根证书。输出的是公有证书，找不到正确根时不要关闭校验。不要把网站叶证书误当根证书。

保存本机配置后设备重启；联网校时、获取配置、刷新、回报版本。手机网页首次数据是待配置模板：示例目标日期 `20270101`、示例北京 Location ID `101010100`、课程「待配置」，必须按自己的用途修改。已有 Blob 配置不受默认模板变更影响。

## 网络及时间

设备联网时先使用主 Wi-Fi，失败可试备用；未联网仍使用缓存。远程读取按计划唤醒，手机保存不会通过互联网直接唤醒睡眠中的 ESP32。调试可按上键手动唤醒触发同步，不要期待网页「重新检查」强制设备联网。

时钟异常可能导致 HTTPS 证书检查失败；先解决 Wi-Fi/NTP。天气需自己的和风天气 Host/API Key；城市 ID 单独配置，远程服务不代替天气授权。
