# BoundedUZlib

本目录是项目修改过的解码器与校验代码，来自 ArduinoUZlib 固定提交 `20aff95cd80c141f80bdbf66895409a0046d2c2f`，上游为 Joergen Ibsen 的 tinf 和 Paul Sokolovsky 的 uzlib。各源文件保留原作者版权及许可声明。

## 项目修改

- 公开符号增加 `bounded_uzlib_` 前缀。
- 启用可选的解码表和块结束检查。
- 在将符号用作表索引前，检查字面量、距离解码错误及输入耗尽。

原第三方依赖保持。当前只编译 inflate、gzip 头解析和校验代码；[bounded_gzip.h](../../include/bounded_gzip.h) 负责限制输入、分配和输出，并检查头部 CRC、数据 CRC、输出长度及输入是否完整消费。

固件与主机测试均编译本目录已检入的 C 源码，便于复现，无需修改被 Git 忽略的依赖目录。
