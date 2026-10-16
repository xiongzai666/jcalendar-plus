export const FESTIVALS = [
  ['new-year','元旦'], ['spring-festival','春节'], ['lantern','元宵节'],
  ['qingming','清明节'], ['labour','劳动节'], ['dragon-boat','端午节'],
  ['qixi','七夕节'], ['mid-autumn','中秋节'], ['national','国庆节'], ['double-ninth','重阳节'],
];
export const SOLAR_TERMS = ['小寒','大寒','立春','雨水','惊蛰','春分','清明','谷雨',
  '立夏','小满','芒种','夏至','小暑','大暑','立秋','处暑','白露','秋分','寒露','霜降','立冬','小雪','大雪','冬至'];

// Optional as a whole so existing configurations and backups keep their default.
export function validateFestivalCountdown(value) {
  if (value === undefined) return null;
  if (!value || typeof value !== 'object' || Array.isArray(value)) return '右下角倒计时设置格式错误';
  const fields = { 'auto-festival':['mode'], 'auto-term':['mode'],
    festival:['mode','target'], term:['mode','target'], custom:['mode','label','date'] };
  const keys = Object.hasOwn(fields,value.mode) ? fields[value.mode] : null;
  if (!keys || Object.keys(value).length !== keys.length || keys.some(key=>!Object.hasOwn(value,key)))
    return '右下角倒计时模式或字段无效';
  if (value.mode === 'festival' && !FESTIVALS.some(([id])=>id===value.target)) return '请选择有效的节日';
  if (value.mode === 'term' && (!Number.isInteger(value.target) || value.target<0 || value.target>=24))
    return '请选择有效的节气';
  if (value.mode === 'custom') {
    if (typeof value.label !== 'string' || !value.label.trim() || [...value.label].length>4 ||
        /[\u0000-\u001f\u007f\uD800-\uDFFF]|[\u{10000}-\u{10ffff}]/u.test(value.label))
      return '右下角名称需为 1–4 字，不支持表情符号';
    const match=typeof value.date==='string' && /^(\d{4})(\d{2})(\d{2})$/.exec(value.date);
    if (!match) return '请选择有效的右下角倒计时日期';
    const [year,month,day]=match.slice(1).map(Number),date=new Date(Date.UTC(year,month-1,day));
    if (year<2025 || year>2099 || date.getUTCFullYear()!==year || date.getUTCMonth()!==month-1 || date.getUTCDate()!==day)
      return '右下角日期需为 2025–2099 年内的有效日期';
  }
  return null;
}
