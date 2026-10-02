import { getOverrideReview } from './override-review.js';
import { planHolidayRange } from './holiday-range.js';
import { exportConfig, importConfig } from './config-transfer.js';
import { applyWeekday, effectiveSubjects } from './effective-day.js';
import { DraftJournal, mergeDraft, acceptSave, deviceStatus, requestJSON, StatusChecker } from './edit-session.js';

const $ = selector => document.querySelector(selector);
const DAYS = ['一', '二', '三', '四', '五', '六'];
const PERIODS = ['上午', '下午', '晚上'];
const LESSON_LABELS = ['第1节', '第2节', '第3节', '第4节', '第5节', '第6节',
  '第7节', '第8节', '晚自习1', '晚自习2', '晚自习3', '晚自习4'];
const TITLES = { todos: '待办事项', schedule: '每周课程', overrides: '临时安排', routine: '每日作息', settings: '显示设置', history: '配置历史' };
const FAILURE_LABELS = {
  wifi: 'Wi‑Fi 连接失败', connect: '站点连接或证书校验失败',
  auth: '设备授权失败', service: '云端响应异常', data: '配置数据无效',
  storage: '设备本地保存失败', config: '远程连接参数缺失',
  time: '设备时间未校准', ack: '同步回报失败',
};
let saved = null;
let draft = null;
let device = null;
let dayIndex = 0;
let activeTab = 'todos';
let toastTimer;
let historyVersions = [];

let draftStorage;
try { draftStorage = localStorage; } catch { /* Private browsing can disable storage. */ }
const journal = new DraftJournal(draftStorage);
let cloudRevision = 0;
let stateEpoch = 0;
let draftStorageWarned = false;
async function api(path, options = {}) {
  try { return await requestJSON(path, options); }
  catch (cause) {
    if (cause.status === 401 && path.startsWith('/api/admin/')) {
      if (saved && draft) journal.save(saved, draft);
      $('#workspace').hidden = true;
      $('#login-view').hidden = false;
      $('#login-error').textContent = '登录已过期，请重新登录；未保存修改已保留。';
      $('#login-error').hidden = false;
      $('#save-bar').hidden = true;
    }
    throw cause;
  }
}
const statusChecker = new StatusChecker(() => api('/api/admin/status'), payload => {
  device = payload.device; cloudRevision = payload.revision;
  renderStatus(); updateDraftNotice();
}, () => stateEpoch);
function checkStatus() { return statusChecker.check(); }
function updateDraftNotice(message = '') {
  const conflict = saved && cloudRevision !== saved.revision;
  $('#draft-notice').textContent = message || (conflict ?
    `云端已更新到版本 ${cloudRevision}；本机编辑基于版本 ${saved.revision}。修改已保留，请先核对新版本。` : '');
  $('#draft-notice').hidden = !$('#draft-notice').textContent;
  $('#rebase-draft').hidden = !conflict;
}
function editsPending() {
  return saved && draft && JSON.stringify({ settings: saved.settings, todos: saved.todos }) !==
    JSON.stringify({ settings: draft.settings, todos: draft.todos });
}

function toast(message) {
  const node = $('#toast');
  node.textContent = message;
  node.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { node.hidden = true; }, 4500);
}

function parseSchedule(value = '') {
  const rows = value.split(';').filter(Boolean).slice(1);
  return DAYS.map((day, index) => {
    const cells = rows[index]?.split(',') || [];
    return cells[0] === day ? Array.from({ length: 12 }, (_, i) => cells[i + 1] || '') : Array(12).fill('');
  });
}

function setSchedule(grid) {
  draft.settings.studySchedule = '444;' + DAYS.map((day, index) =>
    [day, ...grid[index].map(subject => subject.trim().replaceAll(/[;,]/g, '、') || '无课')].join(',') + ';').join('');
  markDirty();
}

function parseRoutine(value = '') {
  return value.split(';').filter(Boolean).map(entry => {
    const [times, ...label] = entry.split(',');
    const [start, end] = times.split('-');
    return { start, end, label: label.join(',') };
  });
}

function setRoutine(items) {
  draft.settings.dailyRoutine = items.map(({ start, end, label }) =>
    `${start}-${end},${label.trim().replaceAll(/[;,]/g, '、')};`).join('');
  markDirty();
}

function parseReading(value = '') {
  const map = new Map(value.split(';').filter(Boolean).map(item => item.split(':')));
  return DAYS.map(day => map.get(day) || '');
}

function pendingTodos() { return draft.todos.filter(item => !item.done).length; }

function localDateKey(date) {
  return `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
}

function todoDueLabel(todo) {
  if (!todo.due) return { text: '无截止日期', urgent: false };
  if (todo.done) return { text: `截止 ${todo.due}`, urgent: false };
  const now = new Date();
  const today = localDateKey(now);
  const tomorrow = localDateKey(new Date(now.getFullYear(), now.getMonth(), now.getDate() + 1));
  const date = `${Number(todo.due.slice(5, 7))}月${Number(todo.due.slice(8, 10))}日`;
  if (todo.due < today) return { text: `已逾期 · ${date}`, urgent: true };
  if (todo.due === today) return { text: `今天截止 · ${date}`, urgent: true };
  if (todo.due === tomorrow) return { text: `明天截止 · ${date}`, urgent: false };
  return { text: `截止 ${todo.due}`, urgent: false };
}

function markDirty() {
  const dirty = editsPending();
  if (saved && draft && !journal.save(saved, draft) && !draftStorageWarned) {
    draftStorageWarned = true; toast('浏览器无法保存本机草稿，请在离开页面前保存到云端');
  }
  const overrideChanged = saved && JSON.stringify(draft.settings.dateOverrides || []) !==
    JSON.stringify(saved.settings.dateOverrides || []);
  $('#save-bar').hidden = !dirty;
  $('#draft-status').hidden = !dirty;
  $('#save-bar strong').textContent = overrideChanged ? '临时安排尚未保存' : '修改尚未保存';
  $('#save').textContent = overrideChanged && activeTab !== 'overrides' ? '核对临时安排' : '保存到云端';
}

function renderStatus() {
  const applied = device?.reportedRevision ?? device?.appliedRevision ?? 0;
  const revision = cloudRevision;
  const seenAt = device?.seenAt ? Date.parse(device.seenAt) : NaN;
  const stale = Number.isFinite(seenAt) && Date.now() - seenAt > 4 * 60 * 60 * 1000;
  const recentlyChecked = Number.isFinite(seenAt) && saved?.updatedAt &&
    seenAt >= Date.parse(saved.updatedAt);
  $('#sync-headline').textContent = !device ? '设备尚未报告同步' :
    stale ? '设备超过 4 小时未连接' :
    deviceStatus(device, revision) === 'current' ? '设备已同步最新配置' :
    recentlyChecked ? '设备已连接，配置尚未确认' : '已保存，等待设备联网';
  const seen = device?.seenAt ? new Date(device.seenAt).toLocaleString('zh-CN', { hour12: false }) : '暂无';
  const network = device?.lastNetwork === 'backup' ? '备用 Wi‑Fi' :
    device?.lastNetwork === 'primary' ? '主 Wi‑Fi' : '未知网络';
  $('#sync-detail').textContent = `云端版本 ${revision} · 设备版本 ${applied}`;
  $('#sync-network').textContent = `最近连接 ${seen} · ${network}`;
  const note = $('#sync-diagnostic');
  const lastFailure = device?.lastFailure;
  const recentRecovery = lastFailure?.recoveredAt &&
    Date.now() - Date.parse(lastFailure.recoveredAt) < 24 * 60 * 60 * 1000;
  note.textContent = stale ? '设备离线期间无法回报原因；可短按上键手动检查。' :
    recentlyChecked && applied < revision ? '设备已联网但未确认新配置，查看下方同步记录。' :
    recentRecovery ? `上次故障已恢复：连续 ${lastFailure.count} 次失败，最后原因是${FAILURE_LABELS[lastFailure.code] || '未知原因'}。` : '';
  note.hidden = !note.textContent;
  renderSyncLog(); renderRuntimeSummary();
}

function renderRuntimeSummary() {
  const container = $('#runtime-summary'); container.replaceChildren();
  const runtime = device?.runtime;
  if (!runtime) { container.append(element('p','','等待新版固件报告')); return; }
  const stamp = value => value ? new Date(value * 1000).toLocaleString('zh-CN',{hour12:false}) : '暂无';
  const values = [
    ['固件版本', runtime.firmware],
    ['实况读取', `${stamp(runtime.nowReadAt)}${runtime.nowFailed ? ' · 最近获取失败' : ''}`],
    ['日预报读取', `${stamp(runtime.dailyReadAt)}${runtime.dailyFailed ? ' · 最近获取失败' : ''}`],
    ['预计联网', stamp(runtime.nextNetworkAt)],
    ['上次所在页面', ['月历','日间周课表','课程与倒计时','待办事项'][runtime.activePage]],
    ['摘要上报', new Date(runtime.reportedAt).toLocaleString('zh-CN',{hour12:false})],
  ];
  for (const [name, value] of values) container.append(element('p','', `${name}：${value}`));
}

function renderSyncLog() {
  const list = $('#sync-log-list');
  list.replaceChildren();
  const events = Array.isArray(device?.events) ? device.events : [];
  if (!events.length) {
    list.append(element('li', 'sync-log-empty', '暂无记录；设备下一次联网后开始记录。'));
    return;
  }
  for (const event of [...events].reverse()) {
    const row = element('li');
    const at = element('time', '', new Date(event.at).toLocaleString('zh-CN', { hour12: false }));
    const failure = event.recovered;
    const eventText = event.kind === 'applied' ? `已应用配置 v${event.revision}` :
      event.kind === 'fetched' ? `已读取云端配置 v${event.revision}，等待设备确认` :
        `配置检查完成 · v${event.revision}`;
    const message = failure
      ? `连接已恢复；此前连续 ${failure.count} 次失败，最后原因是${FAILURE_LABELS[failure.code] || '未知原因'}。${eventText}`
      : eventText;
    const via = event.network === 'backup' ? '备用 Wi‑Fi · ' :
      event.network === 'primary' ? '主 Wi‑Fi · ' : '';
    row.append(at, element('span', '', `${via}${event.retry ? '自动补试 · ' : ''}${message}`));
    list.append(row);
  }
}

function element(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function renderTodos() {
  const list = $('#todo-list');
  list.replaceChildren();
  $('#todo-count').textContent = `${pendingTodos()} 项待办`;
  if (!draft.todos.length) {
    const empty = element('div', 'empty-state');
    empty.append(element('strong', '', '清单已清空'), element('span', '', '在上方添加一件需要记住的事。'));
    list.append(empty);
    return;
  }
  const ordered = [...draft.todos].sort((a, b) => Number(a.done) - Number(b.done) ||
    (a.due || '9999').localeCompare(b.due || '9999') ||
    Number(b.priority === 'high') - Number(a.priority === 'high'));
  for (const todo of ordered) {
    const row = element('div', `todo-row${todo.done ? ' done' : ''}`);
    const checkbox = element('input', 'todo-check');
    checkbox.type = 'checkbox';
    checkbox.checked = todo.done;
    checkbox.setAttribute('aria-label', `完成 ${todo.title}`);
    checkbox.addEventListener('change', () => { todo.done = checkbox.checked; renderTodos(); markDirty(); });
    const content = element('div', 'todo-main');
    content.append(element('div', 'todo-title', todo.title));
    const meta = element('div', 'todo-meta');
    const due = todoDueLabel(todo);
    meta.append(element('span', `todo-due${due.urgent ? ' urgent' : ''}`, due.text));
    if (todo.priority === 'high') meta.append(element('span', 'priority', '重要'));
    content.append(meta);
    const remove = element('button', 'remove-button', '删除');
    remove.type = 'button';
    remove.setAttribute('aria-label', `删除 ${todo.title}`);
    remove.addEventListener('click', () => {
      draft.todos = draft.todos.filter(item => item.id !== todo.id);
      renderTodos(); markDirty();
    });
    row.append(checkbox, content, remove);
    list.append(row);
  }
}

function renderSchedule() {
  const grid = parseSchedule(draft.settings.studySchedule);
  const dayTabs = $('#day-tabs');
  dayTabs.replaceChildren();
  DAYS.forEach((day, index) => {
    const button = element('button', `day-tab${index === dayIndex ? ' active' : ''}`, `周${day}`);
    button.type = 'button';
    button.setAttribute('aria-pressed', String(index === dayIndex));
    button.addEventListener('click', () => { dayIndex = index; renderSchedule(); });
    dayTabs.append(button);
  });
  const editor = $('#schedule-editor');
  editor.replaceChildren();
  PERIODS.forEach((name, groupIndex) => {
    const section = element('section', 'period-group');
    section.append(element('h3', '', `${name} · 第 ${groupIndex * 4 + 1}–${groupIndex * 4 + 4} 节`));
    for (let slot = 0; slot < 4; slot++) {
      const index = groupIndex * 4 + slot;
      const label = element('label', 'course-field');
      label.append(element('span', '', `${index + 1}`));
      const input = element('input');
      input.type = 'text';
      input.maxLength = 12;
      input.value = grid[dayIndex][index];
      input.setAttribute('aria-label', `周${DAYS[dayIndex]}第 ${index + 1} 节`);
      input.addEventListener('input', () => { grid[dayIndex][index] = input.value; setSchedule(grid); });
      label.append(input);
      section.append(label);
    }
    editor.append(section);
  });
}

function renderOverrides() {
  const list = $('#override-list');
  list.replaceChildren();
  const days = draft.settings.dateOverrides;
  if (!days.length) {
    list.append(element('div', 'empty-state', '还没有临时安排。添加日期后，可设置放假、临时调课或独立作息。'));
    return;
  }
  const regular = parseSchedule(draft.settings.studySchedule);
  for (const day of [...days].sort((a, b) => a.date.localeCompare(b.date))) {
    const card = element('section', 'override-card');
    const review = element('div', 'override-review');
    const refreshReview = () => {
      const result = getOverrideReview(day, parseSchedule(draft.settings.studySchedule));
      review.replaceChildren();
      review.append(element('strong', 'override-review-label', '保存前核对'));
      review.append(element('div', 'override-review-date', result.date));
      review.append(element('div', 'override-review-status', result.status));
      const details = element('ul', 'override-review-details');
      for (const detail of result.details) details.append(element('li', '', detail));
      review.append(details);
      if (result.note) review.append(element('p', 'override-review-note', result.note));
    };
    const head = element('div', 'override-head');
    const title = element('h3', '', day.date);
    const remove = element('button', 'remove-button', '删除日期');
    remove.type = 'button';
    remove.setAttribute('aria-label', `删除 ${day.date} 的临时安排`);
    remove.addEventListener('click', () => {
      draft.settings.dateOverrides = days.filter(item => item !== day);
      renderOverrides(); markDirty();
    });
    head.append(title, remove);
    const dayOffLabel = element('label', 'override-off');
    const dayOff = element('input');
    dayOff.type = 'checkbox'; dayOff.checked = day.dayOff;
    dayOff.addEventListener('change', () => {
      day.dayOff = dayOff.checked;
      if (day.dayOff) day.lessons = [];
      renderOverrides(); markDirty();
    });
    dayOffLabel.append(dayOff, element('span', '', '当天放假，无课程和上课提醒'));
    card.append(head, dayOffLabel, review);
    refreshReview();
    if (!day.dayOff) {
      const modeLabel = element('label', 'override-off');
      const mode = element('input');
      mode.type = 'checkbox'; mode.checked = day.mode === 'custom';
      mode.addEventListener('change', () => {
        if (mode.checked) {
          const subjects = effectiveSubjects(draft.settings, day.date, day);
          const clock = parseRoutine(draft.settings.dailyRoutine);
          day.mode = 'custom';
          day.lessons = subjects.flatMap((subject,index) => {
            const times = clock.find(row => row.label === LESSON_LABELS[index]);
            return subject && times ? [{ slot: index+1, subject, start: times.start, end: times.end }] : [];
          });
        } else {
          delete day.mode;
          day.lessons = day.lessons.map(({ slot, subject }) => ({ slot, subject }));
        }
        renderOverrides(); markDirty();
      });
      modeLabel.append(mode, element('span', '', '独立作息：自定义科目和时间'));
      card.append(modeLabel, element('p', 'override-intro', day.mode === 'custom'
        ? '当天只使用下面的时段。每组最多 4 段；未安排的时段无课程，不沿用常规早读、小练。'
        : '当前沿用常规时间；未填写的科目沿用每周课表。启用独立作息后，可自行增删时段。'));
      if (day.mode === 'custom') {
        PERIODS.forEach((name, group) => {
          const section = element('section', 'custom-period');
          const head = element('div', 'override-head');
          head.append(element('h4', '', name));
          const rows = [...day.lessons].filter(row => Math.floor((row.slot-1)/4) === group)
            .sort((a,b) => a.slot-b.slot);
          const add = element('button', 'secondary-button', '＋ 添加时段');
          add.type = 'button'; add.disabled = rows.length >= 4;
          add.addEventListener('click', () => {
            const slot = [1,2,3,4].map(index => group*4+index).find(slot => !rows.some(row => row.slot === slot));
            const clock = parseRoutine(draft.settings.dailyRoutine).find(row => row.label === LESSON_LABELS[slot-1]);
            day.lessons.push({ slot, subject: '自习', start: clock?.start || '', end: clock?.end || '' });
            renderOverrides(); markDirty();
          });
          head.append(add); section.append(head);
          if (!rows.length) section.append(element('p', 'override-intro', `${name}未安排课程`));
          rows.forEach((lesson,index) => {
            const row = element('div', 'custom-lesson');
            [['subject','科目'],['start','开始'],['end','结束']].forEach(([key,label]) => {
              const field = element('label', 'field');
              field.append(element('span', '', key === 'subject' ? `${name} ${index+1} · 科目` : label));
              const input = element('input'); input.type = key === 'subject' ? 'text' : 'time';
              input.value = lesson[key] || ''; input.required = true;
              if (key === 'subject') input.maxLength = 12;
              input.setAttribute('aria-label', `${day.date} ${name}第${index+1}段${label}`);
              input.addEventListener('input', () => {
                lesson[key] = key === 'subject' ? input.value.trim().replaceAll(/[;,]/g,'、') : input.value;
                refreshReview(); markDirty();
              });
              if (key === 'subject') input.addEventListener('blur', () => { input.value = lesson.subject; });
              field.append(input); row.append(field);
            });
            const remove = element('button', 'remove-button', '删除'); remove.type = 'button';
            remove.setAttribute('aria-label', `删除 ${day.date} ${name}第${index+1}段`);
            remove.addEventListener('click', () => {
              day.lessons = day.lessons.filter(item => item !== lesson);
              day.lessons.filter(item => Math.floor((item.slot-1)/4) === group).sort((a,b) => a.slot-b.slot)
                .forEach((item,index) => { item.slot = group*4+index+1; });
              renderOverrides(); markDirty();
            });
            row.append(remove); section.append(row);
          });
          card.append(section);
        });
        list.append(card);
        continue;
      }
      const weekday = new Date(`${day.date}T12:00:00`).getDay();
      const grid = element('div', 'override-grid');
      for (let slot = 1; slot <= 12; slot++) {
        const field = element('label', 'field');
        field.append(element('span', '', `${slot <= 4 ? '上午' : slot <= 8 ? '下午' : '晚上'} ${((slot - 1) % 4) + 1}`));
        const input = element('input');
        input.type = 'text'; input.maxLength = 12;
        input.value = day.lessons.find(item => item.slot === slot)?.subject || '';
        input.placeholder = weekday > 0 && weekday <= 6 ? regular[weekday - 1][slot - 1] : '无常规课程';
        input.addEventListener('input', () => {
          const subject = input.value.trim().replaceAll(/[;,]/g, '、');
          day.lessons = day.lessons.filter(item => item.slot !== slot);
          if (subject) day.lessons.push({ slot, subject });
          refreshReview();
          markDirty();
        });
        input.addEventListener('blur', () => { input.value = input.value.trim().replaceAll(/[;,]/g, '、'); });
        field.append(input); grid.append(field);
      }
      card.append(grid);
    }
    list.append(card);
  }
}

function renderRoutine() {
  const items = parseRoutine(draft.settings.dailyRoutine);
  const editor = $('#routine-editor');
  editor.replaceChildren();
  items.forEach((item, index) => {
    const row = element('div', 'routine-row');
    const isLesson = LESSON_LABELS.includes(item.label);
    [['start', '开始时间'], ['end', '结束时间'], ['label', '活动名称']].forEach(([key, label]) => {
      const input = element('input');
      input.type = key === 'label' ? 'text' : 'time';
      if (key === 'label') input.maxLength = 16;
      input.value = item[key];
      input.setAttribute('aria-label', `第 ${index + 1} 项${label}`);
      if (key === 'label' && isLesson) {
        input.readOnly = true;
        input.title = '课程节次名称固定；可修改上课与下课时间';
      }
      input.addEventListener('change', () => { items[index][key] = input.value; setRoutine(items); });
      row.append(input);
    });
    const remove = element('button', 'remove-button', '×');
    remove.type = 'button';
    remove.setAttribute('aria-label', `删除第 ${index + 1} 项作息`);
    if (isLesson) {
      remove.disabled = true;
      remove.title = '课程节次不可删除；可修改时间';
    }
    remove.addEventListener('click', () => { items.splice(index, 1); setRoutine(items); renderRoutine(); });
    row.append(remove);
    editor.append(row);
  });
}

function renderSettings() {
  const settings = draft.settings;
  $('#countdown-label').value = settings.countdownLabel || '';
  $('#countdown-date').value = settings.countdownDate?.replace(/^(\d{4})(\d{2})(\d{2})$/, '$1-$2-$3') || '';
  $('#weather-location').value = settings.weatherLocation || '';
  $('#default-page').value = String(settings.defaultPage ?? 2);
  const editor = $('#early-reading-editor');
  editor.replaceChildren();
  const reading = parseReading(settings.earlyReading);
  DAYS.forEach((day, index) => {
    const label = element('label', 'field');
    label.append(element('span', '', `周${day}`));
    const input = element('input');
    input.type = 'text';
    input.maxLength = 12;
    input.value = reading[index];
    input.addEventListener('input', () => {
      reading[index] = input.value;
      draft.settings.earlyReading = DAYS.map((name, i) => `${name}:${reading[i].trim() || '未注明'};`).join('');
      markDirty();
    });
    label.append(input);
    editor.append(label);
  });
}

function render() {
  renderStatus(); renderTodos(); renderSchedule(); renderOverrides(); renderRoutine(); renderSettings(); markDirty();
}

function renderHistory() {
  const list = $('#history-list');
  list.replaceChildren();
  if (!historyVersions.length) {
    list.append(element('div', 'empty-state', '暂无历史版本。首次保存后开始记录。'));
    return;
  }
  for (const version of historyVersions) {
    const row = element('div', 'history-row');
    const content = element('div', 'history-main');
    const current = version.revision === saved.revision;
    content.append(element('strong', '', `版本 ${version.revision}${current ? ' · 当前使用' : ''}`));
    const time = version.updatedAt ? new Date(version.updatedAt).toLocaleString('zh-CN', { hour12: false }) : '首次配置';
    const pending = version.todos.filter(todo => !todo.done).length;
    content.append(element('span', 'history-meta', `${time} · ${pending} 项待办 · ${version.settings.dateOverrides?.length || 0} 天临时安排`));
    if (!current) {
      const changes = [];
      if (version.settings.studySchedule !== saved.settings.studySchedule ||
          version.settings.earlyReading !== saved.settings.earlyReading) changes.push('课程');
      if (version.settings.dailyRoutine !== saved.settings.dailyRoutine) changes.push('作息');
      if (JSON.stringify(version.settings.dateOverrides || []) !==
          JSON.stringify(saved.settings.dateOverrides || [])) changes.push('临时安排');
      if (JSON.stringify(version.todos) !== JSON.stringify(saved.todos)) changes.push('待办');
      if (['countdownLabel', 'countdownDate', 'weatherLocation', 'defaultPage']
          .some(key => version.settings[key] !== saved.settings[key])) changes.push('显示设置');
      content.append(element('span', 'history-meta',
        changes.length ? `相较当前：${changes.join('、')}不同` : '内容与当前一致'));
    }
    if (version.restoredFrom) content.append(element('span', 'history-meta', `由版本 ${version.restoredFrom} 恢复`));
    row.append(content);
    if (!current) {
      const button = element('button', 'secondary-button', '恢复此版本');
      button.type = 'button';
      button.setAttribute('aria-label', `恢复版本 ${version.revision}`);
      button.addEventListener('click', () => restoreVersion(version, button));
      row.append(button);
    }
    list.append(row);
  }
}

async function loadHistory() {
  $('#history-list').replaceChildren(element('div', 'empty-state', '正在读取历史版本…'));
  try {
    const result = await api('/api/admin/history');
    historyVersions = result.versions;
    renderHistory();
  } catch (cause) {
    $('#history-list').replaceChildren(element('div', 'empty-state', '读取失败。请检查网络后重新进入“历史”页。'));
    throw cause;
  }
}

async function restoreVersion(version, button) {
  if (!$('#save-bar').hidden) {
    toast('请先保存或重新加载当前未保存的修改');
    return;
  }
  const time = version.updatedAt ? new Date(version.updatedAt).toLocaleString('zh-CN', { hour12: false }) : `版本 ${version.revision}`;
  if (!confirm(`将课程、作息、设置和待办恢复到 ${time} 的内容？\n当前配置会保留在历史中，恢复后设备下次联网同步。`)) return;
  button.disabled = true;
  const submitted = structuredClone(draft);
  try {
    const result = await api('/api/admin/restore', {
      method: 'POST', body: JSON.stringify({ revision: saved.revision, restoreRevision: version.revision }),
    });
    draft = acceptSave(submitted, draft, result.state);
    saved = result.state; cloudRevision = saved.revision; stateEpoch++;
    updateDraftNotice();
    render();
    await loadHistory();
    toast(`已恢复版本 ${version.revision}，设备下次联网时应用`);
  } catch (cause) { toast(cause.message); button.disabled = false; }
}

function switchTab(tab) {
  activeTab = tab;
  $('#page-title').textContent = TITLES[tab];
  document.querySelectorAll('.tab').forEach(button => {
    const active = button.dataset.tab === tab;
    button.classList.toggle('active', active);
    if (active) button.setAttribute('aria-current', 'page');
    else button.removeAttribute('aria-current');
  });
  for (const name of Object.keys(TITLES)) $(`#${name}-panel`).hidden = name !== tab;
  if (tab === 'overrides' && draft) renderOverrides();
  if (draft) markDirty();
  if (tab === 'history') loadHistory().catch(cause => toast(cause.message));
}

async function load({ discard = false } = {}) {
  const payload = await api('/api/admin/state');
  saved = payload.state;
  stateEpoch++;
  saved.settings.dateOverrides ||= [];
  $('#template-notice').hidden = saved.revision !== 0;
  cloudRevision = saved.revision;
  draft = structuredClone(saved);
  const record = discard ? null : journal.read();
  let restored = false;
  if (record && confirm('发现这台手机未保存的修改，是否恢复继续编辑？')) {
    saved = record.base; draft = record.draft;
    draft.settings.dateOverrides ||= []; restored = true;
  } else journal.clear();
  $('#import-status').hidden = true;
  device = payload.device;
  render(); updateDraftNotice(restored && cloudRevision === saved.revision ? '已恢复本机草稿，尚未写入云端。' : '');
  if (activeTab === 'history') await loadHistory();
  $('#login-view').hidden = true;
  $('#workspace').hidden = false;
  $('#sign-out').hidden = false;
}

$('#login-form').addEventListener('submit', async event => {
  event.preventDefault();
  const button = event.currentTarget.querySelector('button');
  button.disabled = true;
  $('#login-error').hidden = true;
  try {
    await api('/api/session', { method: 'POST', body: JSON.stringify({ password: $('#password').value }) });
    $('#password').value = '';
    await load();
  } catch (cause) {
    $('#login-error').textContent = cause.message;
    $('#login-error').hidden = false;
  } finally { button.disabled = false; }
});

$('#sign-out').addEventListener('click', async () => {
  if (editsPending() && !confirm('未保存修改会保留在这台手机，确定退出吗？')) return;
  if (saved && draft) journal.save(saved, draft);
  await api('/api/session', { method: 'DELETE' });
  saved = draft = device = null;
  stateEpoch++;
  historyVersions = [];
  $('#workspace').hidden = true;
  $('#login-view').hidden = false;
  $('#sign-out').hidden = true;
  $('#save-bar').hidden = true;
});

$('#reload').addEventListener('click', async () => {
  try { await checkStatus(); toast('已检查云端与设备状态，编辑内容保持不变'); }
  catch (cause) { toast(cause.message); }
});
$('#rebase-draft').addEventListener('click', async () => {
  try {
    const payload = await api('/api/admin/state');
    const merged = mergeDraft(saved, draft, payload.state);
    const names = { countdownLabel: '倒计时名称', countdownDate: '倒计时日期',
      weatherLocation: '天气城市', studySchedule: '每周课表', dailyRoutine: '每日作息',
      earlyReading: '早读', defaultPage: '默认页', dateOverrides: '临时安排', todos: '待办' };
    const labels = merged.conflicts.map(path => names[path.replace('settings.', '')] || path);
    const message = labels.length ?
      `这些内容在云端和本机都被修改：${labels.join('、')}。\n确认将这些内容保留为本机草稿？其他云端修改会合并，核对后仍需保存。` :
      '云端新修改可以与本机草稿合并。确认载入并保留本机编辑？核对后仍需保存。';
    if (!confirm(message)) return;
    saved = payload.state; draft = merged.state; device = payload.device; stateEpoch++;
    cloudRevision = saved.revision; render(); updateDraftNotice('已合并到新版本，请核对并保存。');
  } catch (cause) { toast(cause.message); }
});

document.querySelectorAll('.tab').forEach(button => button.addEventListener('click', () => switchTab(button.dataset.tab)));

$('#add-todo').addEventListener('submit', event => {
  event.preventDefault();
  if (draft.todos.length >= 30) { toast('待办最多 30 条，请先清理已完成的事项'); return; }
  const title = $('#todo-title').value.trim();
  if (!title) return;
  draft.todos.push({
    id: crypto.randomUUID().replaceAll('-', ''), title, due: $('#todo-due').value,
    priority: $('#todo-priority').value, done: false,
  });
  $('#todo-title').value = ''; $('#todo-due').value = ''; $('#todo-priority').value = 'normal';
  renderTodos(); markDirty();
});

$('#add-routine').addEventListener('click', () => {
  const items = parseRoutine(draft.settings.dailyRoutine);
  if (items.length >= 64) { toast('作息最多 64 项'); return; }
  const start = items.at(-1)?.end || '06:00';
  const endMinute = Math.min(1439, Number(start.slice(0, 2)) * 60 + Number(start.slice(3)) + 10);
  const end = `${String(Math.floor(endMinute / 60)).padStart(2, '0')}:${String(endMinute % 60).padStart(2, '0')}`;
  items.push({ start, end, label: '新活动' });
  setRoutine(items); renderRoutine();
});

$('#add-override').addEventListener('submit', event => {
  event.preventDefault();
  const date = $('#override-date').value;
  const days = draft.settings.dateOverrides;
  if (days.some(day => day.date === date)) { toast('这个日期已有安排，请直接编辑'); return; }
  if (days.length >= 14) { toast('临时安排最多 14 天，请清理旧日期'); return; }
  days.push({ date, dayOff: false, lessons: [] });
  $('#override-date').value = '';
  renderOverrides(); markDirty();
});

$('#apply-weekday').addEventListener('submit', event => {
  event.preventDefault();
  try {
    const date = $('#apply-date').value, days = draft.settings.dateOverrides;
    const existing = days.findIndex(day => day.date === date);
    if (existing < 0 && days.length >= 14) { toast('临时安排最多 14 天'); return; }
    if (existing >= 0 && !confirm('这个日期已有临时安排（可能包含放假），确认替换为所选星期的全部 12 节？')) return;
    const day = applyWeekday(draft.settings, date, Number($('#apply-day').value));
    if (existing >= 0) days[existing] = day; else days.push(day);
    renderOverrides(); markDirty(); toast('已套用到草稿，请核对并保存到云端');
  } catch (cause) { toast(cause.message); }
});

$('#add-holiday-range').addEventListener('submit', event => {
  event.preventDefault();
  try {
    const days = draft.settings.dateOverrides;
    const plan = planHolidayRange($('#holiday-start').value, $('#holiday-end').value, days);
    days.push(...plan.additions);
    renderOverrides(); markDirty();
    const skipped = plan.conflicts.length
      ? `；${plan.conflicts.length} 天已有调课未覆盖，请逐日核对` : '';
    toast(`已加入 ${plan.additions.length} 个放假日${skipped}`);
  } catch (cause) { toast(cause.message); }
});

$('#remove-expired-overrides').addEventListener('click', () => {
  const today = localDateKey(new Date());
  const days = draft.settings.dateOverrides;
  const expired = days.filter(day => day.date < today);
  if (!expired.length) { toast('没有已过期的临时安排'); return; }
  if (!confirm(`清理 ${expired.length} 个已过期日期？保存到云端后才会生效。`)) return;
  draft.settings.dateOverrides = days.filter(day => day.date >= today);
  renderOverrides(); markDirty();
  toast(`已移除 ${expired.length} 个过期日期；请保存到云端`);
});

[['countdown-label', 'countdownLabel'], ['weather-location', 'weatherLocation']].forEach(([id, key]) => {
  $(`#${id}`).addEventListener('input', event => { draft.settings[key] = event.target.value; markDirty(); });
});
$('#countdown-date').addEventListener('change', event => {
  draft.settings.countdownDate = event.target.value.replaceAll('-', ''); markDirty();
});
$('#default-page').addEventListener('change', event => {
  draft.settings.defaultPage = Number(event.target.value); markDirty();
});

$('#export-config').addEventListener('click', () => {
  const backup = exportConfig(saved);
  const blob = new Blob([JSON.stringify(backup, null, 2)], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = `jcalendar-config-${localDateKey(new Date())}-v${saved.revision}.json`;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  toast('已下载当前云端配置备份');
});

$('#select-import').addEventListener('click', () => $('#import-file').click());
$('#import-file').addEventListener('change', async event => {
  const file = event.target.files?.[0];
  event.target.value = '';
  if (!file) return;
  if (!$('#save-bar').hidden &&
      !confirm('导入会替换当前尚未保存的修改，继续吗？')) return;
  try {
    if (file.size > 50000) throw new Error('备份文件过大');
    const imported = importConfig(await file.text());
    await api('/api/admin/validate', {
      method: 'POST', body: JSON.stringify({ revision: saved.revision, ...imported }),
    });
    draft.settings = structuredClone(imported.settings);
    draft.todos = structuredClone(imported.todos);
    render();
    const pending = draft.todos.filter(todo => !todo.done).length;
    const info = $('#import-status');
    info.textContent = `备份已载入：${pending} 项未完成待办、${draft.settings.dateOverrides.length} 天临时安排。` +
      ($('#save-bar').hidden ? '内容与当前云端一致，无需保存。' : '请核对各页后保存到云端。');
    info.hidden = false;
    toast($('#save-bar').hidden ? '备份内容与云端一致，无需保存' :
      '备份校验通过，尚未写入云端');
  } catch (cause) { toast(cause.message); }
});

$('#save').addEventListener('click', async () => {
  if (activeTab !== 'overrides' && JSON.stringify(draft.settings.dateOverrides || []) !==
      JSON.stringify(saved.settings.dateOverrides || [])) {
    switchTab('overrides');
    $('#overrides-panel').scrollIntoView({ behavior: 'smooth', block: 'start' });
    toast('请先核对临时安排，再点“保存到云端”');
    return;
  }
  const submitted = structuredClone(draft);
  const button = $('#save');
  button.disabled = true;
  try {
    const result = await api('/api/admin/state', {
      method: 'PUT',
      body: JSON.stringify({ revision: saved.revision, settings: submitted.settings, todos: submitted.todos }),
    });
    draft = acceptSave(submitted, draft, result.state);
    saved = result.state; cloudRevision = saved.revision; stateEpoch++;
    updateDraftNotice();
    $('#import-status').hidden = true;
    render();
    if (activeTab === 'history') await loadHistory();
    toast('已保存到云端，设备下次联网时应用');
  } catch (cause) {
    if (cause.status === 409) await checkStatus().catch(() => {});
    toast(cause.message);
  } finally { button.disabled = false; }
});

window.addEventListener('beforeunload', event => {
  if (editsPending()) { event.preventDefault(); event.returnValue = ''; }
});

document.addEventListener('visibilitychange', () => {
  if (!document.hidden && draft && !$('#workspace').hidden) {
    renderTodos(); checkStatus().catch(cause => toast(cause.message));
  }
});
function refreshTodosAtMidnight() {
  const now = new Date();
  const next = new Date(now.getFullYear(), now.getMonth(), now.getDate() + 1);
  setTimeout(() => {
    if (draft) renderTodos();
    refreshTodosAtMidnight();
  }, Math.max(1000, next.getTime() - now.getTime() + 1000));
}
refreshTodosAtMidnight();

api('/api/session').then(result => {
  if (result.authenticated) return load();
}).catch(() => { /* Login form provides the recovery path. */ });

setInterval(() => {
  if (!document.hidden && draft && !$('#workspace').hidden)
    checkStatus().catch(() => { /* Keep the last report; a manual check shows errors. */ });
}, 30000);
