# 配置模板

`settings.example.json` 与云端初始模板一致，可作为导入结构参考，不是现成学校课表。首次固件不写入这份模板。

- `studySchedule`：`444;` 后 6 条周一至周六记录，每条 12 个科目，`;` 分日、`,` 分科目。占位「待配置」不算有效课程。
- `dailyRoutine`：12 个有序 `HH:mm-HH:mm,第N节`／`晚自习N`；可插入早读、午休等作息，不能重叠。
- `earlyReading`：6 天 `一:科目;` 形式，可用「未注明」表示不附科目。
- `countdownLabel` 最多 8 字；日期 `yyyyMMdd`。本机配置标签按较短限制显示。
- `weatherLocation` 是和风天气 9 位 Location ID；模板是北京示例，不自动定位。
- `defaultPage`：0 月历、1 日间周课表、2 课程＋倒计时、3 待办。
- `dateOverrides` 最多 14 天；普通 `{date, dayOff, lessons:[{slot,subject}]}`，独立时间增加 `mode:"custom"` 及每段 `start`／`end`。全天放假不能同时有课程。

配置导入／导出入口在手机页，不导出设备密码／Token／证书。导入后先核对再保存，不会自动刷固件或修改 Wi-Fi。
