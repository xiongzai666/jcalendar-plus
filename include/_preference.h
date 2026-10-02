#ifndef ___PREFERENCE_H__
#define ___PREFERENCE_H__

#include <Preferences.h>
#define PREF_NAMESPACE "J_CALENDAR"

// Preferences KEY定义
// !!!preferences key限制15字符
#define PREF_SI_CAL_DATE "SI_CAL_DATE" // 屏幕当前显示的日期
#define PREF_SI_WEEK_1ST "SI_WEEK_1ST" // 每周第一天，0: 周日（默认），1:周一
#define PREF_SI_TYPE "SI_TYPE" // 屏幕显示类型

#define PREF_QWEATHER_HOST "QWEATHER_HOST" // QWEATHER HOST
#define PREF_QWEATHER_KEY "QWEATHER_KEY" // QWEATHER KEY/TOKEN
#define PREF_QWEATHER_TYPE "QWEATHER_TYPE" // 0: 每日天气，1: 实时天气
#define PREF_QWEATHER_LOC "QWEATHER_LOC" // 地理位置
#define PREF_CD_DAY_DATE "CD_DAY_DATE" // 倒计日
#define PREF_CD_DAY_LABLE "CD_DAY_LABLE" // 倒计日名称
#define PREF_TAG_DAYS "TAG_DAYS" // tag day
#define PREF_STUDY_SCHEDULE "STUDY_SCH" // 课程表
#define PREF_LAYOUT_INIT "LAYOUT_INIT" // 已初始化新版课表布局
#define PREF_SCH_SAMPLE "SCH_SAMPLE" // 当前课程表为测试数据
#define PREF_SCH_REV "SCH_REV" // 用户提供的课表版本
#define PREF_SCH_PREV "SCH_PREV" // 迁移前的自定义课表备份
#define PREF_DAILY_ROUTINE "DAILY_ROUTINE" // 完整日常作息
#define PREF_EARLY_READING "EARLY_READ" // 周一至周六早读科目
#define PREF_LIVE_REV "LIVE_REV" // 实时天气与课程边界刷新版本
#define PREF_WEATHER_CACHE "WX_CACHE" // 最近一次成功获取的三日天气
#define PREF_WEATHER_GEO "WX_GEO" // V1 坐标缓存，按 API Host 和 Location ID 匹配
#define PREF_REMOTE_HOST "RC_HOST" // 远程配置域名
#define PREF_REMOTE_TOKEN "RC_TOKEN" // 设备专用令牌
#define PREF_REMOTE_CA "RC_CA" // 远程站点根证书 PEM
#define PREF_REMOTE_REV "RC_REV" // 已应用的云端版本
#define PREF_REMOTE_PENDING "RC_PENDING" // 分步写入时待恢复的云端版本
#define PREF_TODO_JSON "TODO_JSON" // 缓存的未完成待办
#define PREF_TODO_COUNT "TODO_COUNT" // 未完成待办总数
#define PREF_REMOTE_PAGE "RC_PAGE" // 上次下发的默认页
#define PREF_BACKUP_SSID "WIFI2_SSID" // 备用 Wi-Fi 名称
#define PREF_BACKUP_PASS "WIFI2_PASS" // 备用 Wi-Fi 密码，仅存设备 NVS
#define PREF_WIFI_LAST "WIFI_LAST" // 上次成功连接：1 主网络，2 备用网络
#define PREF_AP_PASSWORD "AP_PASSWORD" // 本设备随机生成的配置热点密码
#define PREF_DATE_OVERRIDES "DATE_OVERRIDES" // 指定日期调课／放假
#define PREF_REMOTE_SYNC_AT "RC_SYNC_AT" // 最近一次成功检查云端的时间戳
#define PREF_REMOTE_SYNC_ERR "RC_SYNC_ERR" // 最近一次检查是否失败
#define PREF_REMOTE_ERR_CODE "RC_ERR_CODE" // 最近一次同步失败类别
#define PREF_REMOTE_ERR_AT "RC_ERR_AT" // 最近一次失败时间戳
#define PREF_REMOTE_ERR_COUNT "RC_ERR_COUNT" // 连续失败次数

// 假期信息，tm年，假期日(int8)，假期日(int8)...
#define PREF_HOLIDAY "HOLIDAY"

#endif
