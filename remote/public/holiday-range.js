const DAY_MS = 24 * 60 * 60 * 1000;

function dayValue(value) {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(value)) return NaN;
  const [year, month, day] = value.split('-').map(Number);
  const time = Date.UTC(year, month - 1, day);
  const date = new Date(time);
  return date.getUTCFullYear() === year && date.getUTCMonth() === month - 1 &&
    date.getUTCDate() === day ? time : NaN;
}

export function planHolidayRange(startDate, endDate, existing, limit = 14) {
  const start = dayValue(startDate);
  const end = dayValue(endDate);
  if (!Number.isFinite(start) || !Number.isFinite(end) || end < start)
    throw new Error('请选择有效的起止日期，结束日期不能早于开始日期');
  const total = (end - start) / DAY_MS + 1;
  if (total > limit) throw new Error(`一次最多设置 ${limit} 天，请缩短日期范围`);
  const byDate = new Map(existing.map(day => [day.date, day]));
  const additions = [];
  const conflicts = [];
  let alreadyOff = 0;
  for (let time = start; time <= end; time += DAY_MS) {
    const date = new Date(time).toISOString().slice(0, 10);
    const previous = byDate.get(date);
    if (previous?.dayOff) alreadyOff++;
    else if (previous) conflicts.push(date);
    else additions.push({ date, dayOff: true, lessons: [] });
  }
  if (existing.length + additions.length > limit)
    throw new Error(`临时安排最多保留 ${limit} 天；请先清理已过期日期`);
  return { additions, conflicts, alreadyOff, total };
}
