import { DEFAULT_SETTINGS } from './defaults.js';

export const STATE_KEY = 'devices/main/state.json';
export const ACK_KEY = 'devices/main/ack.json';
export const HISTORY_LIMIT = 10;
export const historyKey = revision => `devices/main/history/${revision}.json`;

export const EMPTY_STATE = Object.freeze({
  schema: 1,
  revision: 0,
  updatedAt: null,
  settings: DEFAULT_SETTINGS,
  todos: [],
});

const DAYS = ['一', '二', '三', '四', '五', '六'];
const LESSON_LABELS = ['第1节', '第2节', '第3节', '第4节', '第5节', '第6节',
  '第7节', '第8节', '晚自习1', '晚自习2', '晚自习3', '晚自习4'];
const timeValue = value => Number(value.slice(0, 2)) * 60 + Number(value.slice(3, 5));
const validTime = value => /^([01]\d|2[0-3]):[0-5]\d$/.test(value);
const displayText = value => typeof value === 'string' && !/[\u0000-\u001f\u007f\uD800-\uDFFF]|[\u{10000}-\u{10ffff}]/u.test(value);
const byteLimit = (value, limit) => typeof value === 'string' && Buffer.byteLength(value, 'utf8') <= limit;
const records = value => {
  const parts = value.split(';');
  if (parts.at(-1) === '') parts.pop();
  return parts;
};

function validDate(value, compact = false) {
  if (typeof value !== "string") return false;
  const match = compact
    ? /^(\d{4})(\d{2})(\d{2})$/.exec(value)
    : /^(\d{4})-(\d{2})-(\d{2})$/.exec(value);
  if (!match) return false;
  const year = Number(match[1]);
  const month = Number(match[2]);
  const day = Number(match[3]);
  const date = new Date(Date.UTC(year, month - 1, day));
  return year >= 2025 && year <= 2099 && date.getUTCFullYear() === year && date.getUTCMonth() === month - 1 && date.getUTCDate() === day;
}

function validSchedule(value) {
  if (typeof value !== 'string' || !value.startsWith('444;')) return false;
  const rows = records(value).slice(1);
  if (rows.length !== 6) return false;
  return rows.every((record, index) => {
    const fields = record.split(',');
    return fields.length === 13 && fields[0] === DAYS[index] &&
      fields.slice(1).every(course => displayText(course) && course.trim().length > 0 && [...course].length <= 12);
  });
}

function validRoutine(value) {
  if (typeof value !== 'string') return false;
  const entries = records(value);
  if (entries.length === 0 || entries.length > 64) return false;
  let previousEnd = -1;
  let nextLesson = 0;
  for (const entry of entries) {
    const match = /^(\d\d:\d\d)-(\d\d:\d\d),(.+)$/.exec(entry);
    if (!match || !validTime(match[1]) || !validTime(match[2]) ||
        !displayText(match[3]) || !match[3].trim() || [...match[3]].length > 16) return false;
    const start = timeValue(match[1]);
    const end = timeValue(match[2]);
    if (start < previousEnd || end <= start) return false;
    previousEnd = end;
    if (LESSON_LABELS.includes(match[3])) {
      if (match[3] !== LESSON_LABELS[nextLesson]) return false;
      nextLesson++;
    }
  }
  return nextLesson === LESSON_LABELS.length;
}

function validEarlyReading(value) {
  if (typeof value !== 'string') return false;
  const entries = records(value);
  return entries.length === 6 && entries.every((entry, index) => {
    const fields = entry.split(':');
    const [day, subject] = fields;
    return fields.length === 2 && day === DAYS[index] && subject?.trim() && displayText(subject) && [...subject].length <= 12;
  });
}

export function validateSettings(settings) {
  if (!settings || typeof settings !== 'object' || Array.isArray(settings)) return '设置格式错误';
  const allowed = new Set(['countdownLabel', 'countdownDate', 'weatherLocation',
    'studySchedule', 'dailyRoutine', 'earlyReading', 'defaultPage', 'dateOverrides']);
  if (Object.keys(settings).some(key => !allowed.has(key))) return '设置包含未知字段';
  if ([...allowed].filter(key => key !== 'dateOverrides').some(key => !(key in settings))) return '设置缺少必要字段';
  if (!displayText(settings.countdownLabel) || !byteLimit(settings.countdownLabel, 24)) return '倒计时名称含不支持的字符或过长';
  if (!byteLimit(settings.studySchedule, 4000) || !byteLimit(settings.dailyRoutine, 4000) ||
      !byteLimit(settings.earlyReading, 300)) return '课表、作息或早读内容过长';
  if ('countdownLabel' in settings && (typeof settings.countdownLabel !== 'string' ||
      [...settings.countdownLabel.trim()].length < 1 || [...settings.countdownLabel].length > 8))
    return '倒计时名称需为 1–8 字';
  if ('countdownDate' in settings && !validDate(settings.countdownDate, true))
    return '倒计时日期无效';
  if ('weatherLocation' in settings && !/^\d{9}$/.test(settings.weatherLocation))
    return '天气 Location ID 应为 9 位数字';
  if ('studySchedule' in settings && !validSchedule(settings.studySchedule))
    return '课程表应包含周一至周六、每天 12 节';
  if ('dailyRoutine' in settings && !validRoutine(settings.dailyRoutine))
    return '作息时间有重叠，或缺少按顺序排列的 12 节课程';
  if ('earlyReading' in settings && !validEarlyReading(settings.earlyReading))
    return '早读安排格式错误';
  if ('defaultPage' in settings && ![0, 1, 2, 3].includes(settings.defaultPage))
    return '默认页面无效';
  if ('dateOverrides' in settings) {
    const days = settings.dateOverrides;
    if (!Array.isArray(days) || days.length > 14) return '临时安排最多 14 天';
    if (Buffer.byteLength(JSON.stringify(days), 'utf8') > 3500)
      return '临时安排内容过多，请减少日期或课程文字';
    const dates = new Set();
    for (const day of days) {
      if (!day || typeof day !== 'object' || Array.isArray(day) ||
          Object.keys(day).some(key => !['date', 'dayOff', 'lessons', 'mode'].includes(key)) ||
          (day.mode !== undefined && day.mode !== 'custom') ||
          !validDate(day.date) || dates.has(day.date) || typeof day.dayOff !== 'boolean' ||
          !Array.isArray(day.lessons) || day.lessons.length > 12) return '临时安排格式错误或日期重复';
      dates.add(day.date);
      const slots = new Set();
      for (const lesson of day.lessons) {
        if (!lesson || typeof lesson !== 'object' || Array.isArray(lesson) ||
            Object.keys(lesson).some(key => !(day.mode === 'custom'
              ? ['slot', 'subject', 'start', 'end'] : ['slot', 'subject']).includes(key)) ||
            !Number.isInteger(lesson.slot) || lesson.slot < 1 || lesson.slot > 12 ||
            slots.has(lesson.slot) || typeof lesson.subject !== 'string' ||
            [...lesson.subject.trim()].length < 1 || [...lesson.subject].length > 12 ||
            !displayText(lesson.subject) || /[;,]/.test(lesson.subject)) return '临时课程节次或科目无效';
        slots.add(lesson.slot);
        if (day.mode === 'custom' && (typeof lesson.start !== 'string' || typeof lesson.end !== 'string' ||
            !validTime(lesson.start) || !validTime(lesson.end) || timeValue(lesson.end) <= timeValue(lesson.start)))
          return '独立课程的开始与结束时间无效';
      }
      if (day.mode === 'custom') {
        let previousEnd = -1;
        for (const lesson of [...day.lessons].sort((a,b) => a.slot - b.slot)) {
          if (timeValue(lesson.start) < previousEnd) return '独立课程时间有重叠或顺序错误';
          previousEnd = timeValue(lesson.end);
        }
      }
      if (day.dayOff && day.lessons.length) return '放假日期不能同时设置课程';
    }
  }
  return null;
}

export function validateTodos(todos) {
  if (!Array.isArray(todos) || todos.length > 30) return '待办最多 30 条';
  const ids = new Set();
  for (const todo of todos) {
    if (!todo || typeof todo !== 'object' || Array.isArray(todo)) return '待办格式错误';
    if (typeof todo.id !== 'string' || !/^[a-zA-Z0-9_-]{6,40}$/.test(todo.id) || ids.has(todo.id))
      return '待办 ID 无效或重复';
    ids.add(todo.id);
    if (!displayText(todo.title) || [...todo.title.trim()].length < 1 ||
        [...todo.title].length > 16) return '待办标题需为 1–16 字';
    if (todo.due !== '' && (typeof todo.due !== 'string' || !validDate(todo.due)))
      return '待办截止日期无效';
    if (!['normal', 'high'].includes(todo.priority) || typeof todo.done !== 'boolean')
      return '待办优先级或完成状态无效';
  }
  return null;
}

export function validateUpdate(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body) ||
      !Number.isSafeInteger(body.revision) || body.revision < 0 || body.revision >= 4294967295) return '配置版本无效';
  return validateSettings(body.settings) || validateTodos(body.todos) ||
    (Buffer.byteLength(JSON.stringify({ schema: 1, revision: body.revision + 1,
      settings: body.settings, pendingCount: 30, todos: body.todos }), 'utf8') > 11500
      ? '配置内容过大，请清理旧待办或临时安排' : null);
}

export async function loadState(store) {
  return await store.get(STATE_KEY, { type: 'json', consistency: 'strong' }) || structuredClone(EMPTY_STATE);
}
