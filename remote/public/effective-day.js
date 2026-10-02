const DAYS = ['一', '二', '三', '四', '五', '六'];
const LABELS = ['第1节','第2节','第3节','第4节','第5节','第6节','第7节','第8节',
  '晚自习1','晚自习2','晚自习3','晚自习4'];
export const effectiveSubject = value => !['', '放假', '无课', '待配置', '—'].includes((value || '').trim());
const minute = value => Number(value.slice(0, 2)) * 60 + Number(value.slice(3, 5));
const time = value => `${String(Math.floor(value / 60)).padStart(2,'0')}:${String(value % 60).padStart(2,'0')}`;
export function weekdayForDate(date) { return new Date(`${date}T12:00:00`).getDay(); }
export function weeklySubjects(settings) {
  const records = settings.studySchedule.split(';').filter(Boolean).slice(1);
  return DAYS.map(day => {
    const fields = records.find(row => row.startsWith(`${day},`))?.split(',') || [];
    return Array.from({ length: 12 }, (_, i) => fields[i + 1] || '');
  });
}
export function effectiveSubjects(settings, date, override = settings.dateOverrides?.find(day => day.date === date)) {
  const weekday = weekdayForDate(date);
  const courses = weekday && override?.mode !== 'custom' ? [...weeklySubjects(settings)[weekday - 1]] : Array(12).fill('');
  if (override?.dayOff) return Array(12).fill('');
  for (const lesson of override?.lessons || []) courses[lesson.slot - 1] = lesson.subject;
  return courses.map(value => effectiveSubject(value) ? value.trim() : '');
}
export function applyWeekday(settings, date, weekday) {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(date) || !Number.isInteger(weekday) || weekday < 1 || weekday > 6)
    throw new Error('请选择日期和周一至周六的课表');
  return { date, dayOff: false, lessons: weeklySubjects(settings)[weekday - 1]
    .map((subject, index) => ({ slot: index + 1, subject: subject || '无课' })) };
}
export function previewDay(settings, date, at) {
  const weekday = weekdayForDate(date), subjects = effectiveSubjects(settings, date);
  const override = settings.dateOverrides?.find(day => day.date === date);
  const independent = override?.mode === 'custom';
  let routine = settings.dailyRoutine.split(';').filter(Boolean).map(entry => {
    const [times, label] = entry.split(',');
    const [start, end] = times.split('-');
    return { start: minute(start), end: minute(end), label, slot: LABELS.indexOf(label) };
  });
  const lessons = LABELS.map(label => routine.find(row => row.label === label));
  if (lessons.some(row => !row)) throw new Error('作息需要完整的 12 节课程');
  if (independent) {
    const rows = [...override.lessons].sort((a,b) => a.slot - b.slot);
    let previousEnd = -1;
    const slots = new Set();
    for (const row of rows) {
      if (!Number.isInteger(row.slot) || row.slot < 1 || row.slot > 12 || slots.has(row.slot) ||
          !/^([01]\d|2[0-3]):[0-5]\d$/.test(row.start || '') ||
          !/^([01]\d|2[0-3]):[0-5]\d$/.test(row.end || '') || minute(row.end) <= minute(row.start))
        throw new Error('请填写有效的开始与结束时间');
      if (minute(row.start) < previousEnd) throw new Error('课程时间有重叠或顺序错误');
      previousEnd = minute(row.end); slots.add(row.slot);
      lessons[row.slot - 1] = { start: minute(row.start), end: minute(row.end),
        label: LABELS[row.slot - 1], slot: row.slot - 1 };
    }
    const activeRows = lessons.filter((row,i) => subjects[i]);
    routine = [];
    activeRows.forEach((row,index) => {
      if (index && activeRows[index - 1].end < row.start) {
        const previous = activeRows[index - 1];
        const group = Math.floor(previous.slot / 4);
        routine.push({ start: previous.end, end: row.start, slot: -1,
          label: group === Math.floor(row.slot / 4) ? '课间' : group === 0 ? '午休' : '晚间休息' });
      }
      routine.push(row);
    });
  }
  const finishes = [0,1,2].map(group => {
    for (let i = group * 4 + 3; i >= group * 4; i--) if (subjects[i]) return lessons[i].end;
    return -1;
  });
  const sectionAt = start => start < Math.min(lessons[4].start, 720) ? 0 :
    start < Math.min(lessons[8].start, 1070) ? 1 : 2;
  const now = minute(at);
  const active = lessons.findIndex((row,i) => subjects[i] && row.start <= now && now < row.end);
  const section = independent && active >= 0 ? Math.floor(active / 4) : sectionAt(now);
  const shown = finishes[section] >= 0 ? section :
    [section + 1, section + 2, section - 1, section - 2].find(i => i >= 0 && i < 3 && finishes[i] >= 0) ?? 0;
  const nextItems = routine.filter(row => {
    if (row.slot >= 0) return Boolean(subjects[row.slot]);
    if (independent) return true;
    if (!weekday || ['中午放学','下午放学','放学'].includes(row.label)) return false;
    const group = sectionAt(row.start);
    if (finishes[group] < 0 || row.start >= finishes[group]) return false;
    if (row.label === '课间') {
      const indices = [group*4,group*4+1,group*4+2,group*4+3].filter(i => subjects[i]);
      return (row.start < lessons[group*4].start || indices.some(i => lessons[i].end === row.start)) &&
        indices.some(i => lessons[i].start > row.start);
    }
    return true;
  }).map(row => ({ ...row, label: row.slot >= 0 ? subjects[row.slot] :
    ['晨会/跑操','晨会或跑操'].includes(row.label) ? (weekday === 1 ? '晨会' : '跑操') : row.label }));
  finishes.forEach((finish, group) => {
    if (finish >= 0 && (!independent || !finishes.slice(group + 1).some(n => n >= 0))) nextItems.push({ start: finish, label:
      finishes.slice(group + 1).some(n => n >= 0) ? ['中午放学','下午放学','放学'][group] : '放学' });
  });
  nextItems.sort((a,b) => a.start - b.start);
  const next = nextItems.find(row => row.start > now);
  return { section: shown, rest: !lessons.some((row,i) => subjects[i] && row.end > now),
    active: active >= 0 ? subjects[active] : '',
    courses: lessons.slice(shown*4,shown*4+4).map((row,i) => ({
      start: time(row.start), end: time(row.end), subject: subjects[shown*4+i] || '无课',
      effective: Boolean(subjects[shown*4+i]), current: active === shown*4+i }))
      .filter(row => !independent || row.effective),
    next: next ? { label: next.label, time: time(next.start) } : null };
}
