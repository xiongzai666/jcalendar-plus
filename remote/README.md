# 手机配置站点

前端 `public`、云函数 `cloud-functions`、Node 测试 `test`。部署说明见 [完整指南](../docs/DEPLOYMENT.md)，开发和验证见 [维护指南](../MAINTENANCE.md)。

```sh
npm ci
npm test
npm run build
npm run dev
```

本地演示只监听 loopback、内存存储、密码 `local-demo-only`；生产必须独立设置 4 项安全环境变量并使用 HTTPS。导出文件只包含可编辑配置和待办，不能代替设备 Flash 备份。

服务用于单设备，无分布式并发事务保证。`defaults.js` 是待配置模板，持久化状态存在时不会覆盖。

## 首次使用、草稿与状态

[首次使用与自助诊断](../docs/FIRST_USE.md) 提供本机／远程两条路径。网页登录前可展开同类导引；待办编辑沿用本机草稿，先应用／取消再统一保存。状态分开列出云端保存和设备回报时间，匹配版本不代表物理屏幕显示。脱敏摘要采用固定白名单，未知字段不推测。

## 隔离浏览器回归

`npm test` 运行 Node 回归；`product-browser.test.js` 默认跳过浏览器部分。已安装本地 Playwright 时，设置 `PRODUCT_BROWSER_MODULE` 为其 `index.mjs` 绝对路径，然后执行 `node --test test/product-browser.test.js`。可选 `PRODUCT_BROWSER_OUTPUT` 指定截图目录。测试只启动 loopback 假服务与虚拟账号，不下载安装浏览器，不调用生产接口。桌面 1280×900、手机 390×844 覆盖编辑、取消、前 6 条、保存／登录／冲突保护、输入焦点与诊断脱敏。
