import { effectiveSubject } from './effective-day.js';
const PERIODS = ['上午', '下午', '晚上'];
const WEEKDAYS = ['日', '一', '二', '三', '四', '五', '六'];

function slotLabel(slot) {
  return `${PERIODS[Math.floor((slot - 1) / 4)]}第${((slot - 1) % 4) + 1}节`;
}

export function getOverrideReview(day, weeklyCourses) {
  const weekday = new Date(`${day.date}T12:00:00`).getDay();
  const regular = weekday === 0 ? Array(12).fill('') : weeklyCourses[weekday - 1];
  const changes = new Map(day.lessons.map(({ slot, subject }) => [slot, subject]));
  const hasCourses = regular.some((subject, index) => effectiveSubject(changes.get(index + 1) || subject));
  const lastSlot = 12;

  if (day.dayOff) {
    const details = [];
    for (let first = 1; first <= lastSlot; first += 4) {
      const subjects = regular.slice(first - 1, Math.min(first + 3, lastSlot)).filter(effectiveSubject);
      if (subjects.length) details.push(`${PERIODS[Math.floor((first - 1) / 4)]}：原定 ${subjects.join('、')}，均停课`);
    }
    return {
      date: `${day.date} · 周${WEEKDAYS[weekday]}`,
      status: '全天放假 · 当天课程不显示',
      details: details.length ? details : ['这一天原本没有常规课程。'],
    };
  }

  if (day.mode === 'custom') {
    let previousEnd = '';
    const rows = [...day.lessons].sort((a,b) => a.slot - b.slot);
    const invalid = rows.some(row => {
      const validTime = value => /^([01]\d|2[0-3]):[0-5]\d$/.test(value || '');
      const invalid = !effectiveSubject(row.subject) || !validTime(row.start) || !validTime(row.end) ||
        row.start >= row.end || row.start < previousEnd;
      previousEnd = row.end;
      return invalid;
    });
    return {
      date: `${day.date} · 周${WEEKDAYS[weekday]}`,
      status: invalid ? '时间或科目待修正 · 请检查空项、重叠与先后顺序' :
        rows.length ? `独立作息 · 共 ${rows.length} 段课程` : '独立作息 · 当天未安排课程',
      details: rows.map(row => `${slotLabel(row.slot)}：${row.start || '未填'}–${row.end || '未填'} ${row.subject || '未填科目'}`),
      note: '仅显示以上安排；其他时段不继承常规课程、早读或小练。',
    };
  }

  const details = [];
  for (let slot = 1; slot <= lastSlot; slot++) {
    const before = effectiveSubject(regular[slot - 1]) ? regular[slot - 1] : '';
    const after = changes.get(slot) || regular[slot - 1];
    if (changes.has(slot) && after !== before && (before || effectiveSubject(after))) {
      details.push(`${slotLabel(slot)}：${before || '原无课'} → ${after}`);
    }
  }
  return {
    date: `${day.date} · 周${WEEKDAYS[weekday]}`,
    status: weekday === 0 ? (hasCourses ? '周日临时上课' : '周日休息') : !hasCourses ? '当天无课' :
      `上课 · 其余节次沿用周${WEEKDAYS[weekday]}课表`,
    details: details.length ? details : [weekday === 0
      ? '未安排课程；设备照常显示周日休息。'
      : '未更改课程；此日期与常规安排相同。'],
    note: '',
  };
}
