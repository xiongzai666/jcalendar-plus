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
