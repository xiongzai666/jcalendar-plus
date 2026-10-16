export const LESSON_LABELS = ['第1节', '第2节', '第3节', '第4节', '第5节', '第6节',
  '第7节', '第8节', '晚自习1', '晚自习2', '晚自习3', '晚自习4'];
const validTime = value => /^([01]\d|2[0-3]):[0-5]\d$/.test(value);

// Shared by the editor and API so a draft has the same requirements as a cloud save.
export function routineIssues(value) {
  const issues = [];
  const add = (rows, fields, message) => issues.push({ rows, fields, message });
  if (typeof value !== 'string') return [{ rows: [], fields: [], message: '作息格式错误' }];
  const entries = value.split(';');
  if (entries.at(-1) === '') entries.pop();
  if (!entries.length || entries.length > 64)
    return [{ rows: [], fields: [], message: '作息需包含 1–64 项及完整的 12 节课程' }];
  let previous = null, nextLesson = 0;
  entries.forEach((entry, index) => {
    const match = /^(\d\d:\d\d)-(\d\d:\d\d),(.*)$/.exec(entry);
    const name = `第 ${index + 1} 项`;
    if (!match) { add([index], ['start', 'end', 'label'], `${name}作息格式错误，请填写起止时间和活动名称`); return; }
    const [, start, end, label] = match;
    if (!label.trim() || [...label].length > 16 || /[\u0000-\u001f\u007f\uD800-\uDFFF]|[\u{10000}-\u{10ffff}]/u.test(label))
      add([index], ['label'], `${name}活动名称需为 1–16 字，且不含不支持的字符`);
    const invalidTimes = ['start', 'end'].filter(key => !validTime(key === 'start' ? start : end));
    if (invalidTimes.length) add([index], invalidTimes, `${name}时间无效，请填写 00:00–23:59`);
    else {
      if (end <= start) add([index], ['start', 'end'], `${name}“${label}”的结束时间须晚于开始时间`);
      if (previous && start < previous.end) {
        if (start < previous.start) add([index], ['start'], `${name}“${label}”未按时间排列，请点击“按时间整理”`);
        else add([previous.index, index], ['start', 'end'], `${name}“${label}”与第 ${previous.index + 1} 项“${previous.label}”时间重叠，请调整起止时间`);
      }
      previous = { start, end, label, index };
    }
    if (LESSON_LABELS.includes(label)) {
      if (label !== LESSON_LABELS[nextLesson]) add([index], ['start', 'end'], `${name}课程顺序错误，此处应为${LESSON_LABELS[nextLesson] || '非课程活动'}，请核对课程时间和节次`);
      nextLesson++;
    }
  });
  if (nextLesson !== LESSON_LABELS.length) add([], [], '作息需包含按顺序排列的全部 12 节课程，请补齐缺少的节次');
  return issues;
}
