const copy = value => structuredClone(value);
const equal = (a,b) => JSON.stringify(a) === JSON.stringify(b);
const cleanState = value => ({ schema: 1, revision: value.revision,
  settings: copy(value.settings), todos: copy(value.todos) });
const object = value => value && typeof value === 'object' && !Array.isArray(value);
const draftShape = value => object(value) && value.schema === 1 && Number.isInteger(value.revision) && value.revision >= 0 &&
  object(value.settings) && Object.entries(value.settings).every(([key,field]) =>
    key === 'defaultPage' ? [0,1,2,3].includes(field) : key === 'dateOverrides' ?
      Array.isArray(field) && field.every(day => object(day) && typeof day.date === 'string' &&
        typeof day.dayOff === 'boolean' && (day.mode === undefined || day.mode === 'custom') && Array.isArray(day.lessons) &&
        day.lessons.every(lesson => object(lesson) && Number.isInteger(lesson.slot) && typeof lesson.subject === 'string' &&
          (lesson.start === undefined || typeof lesson.start === 'string') &&
          (lesson.end === undefined || typeof lesson.end === 'string'))) :
      typeof field === 'string') &&
  Array.isArray(value.todos) && value.todos.length <= 30 && value.todos.every(todo => object(todo) &&
    typeof todo.id === 'string' && typeof todo.title === 'string' && typeof todo.due === 'string' &&
    typeof todo.done === 'boolean' && ['normal','high'].includes(todo.priority));
const cleanTodoEdit = value => object(value) && typeof value.id === 'string' &&
  typeof value.title === 'string' && typeof value.due === 'string' && ['normal','high'].includes(value.priority) ?
  {id:value.id,title:value.title,due:value.due,priority:value.priority} : null;
export class DraftJournal {
  constructor(storage) {
    this.storage = storage; this.key = 'jcalendar.draft.v1';
    this.memory = null; this.loaded = false; this.persistent = false;
  }
  save(base, draft, todoEdit = null) {
    this.loaded = true;
    this.memory = !todoEdit && equal(base.settings, draft.settings) && equal(base.todos, draft.todos) ? null :
      { version: 1, savedAt: Date.now(), base: cleanState(base), draft: cleanState(draft), todoEdit: cleanTodoEdit(todoEdit) };
    try {
      if (this.memory) this.storage.setItem(this.key, JSON.stringify(this.memory));
      else this.storage.removeItem(this.key);
      this.persistent = true; return true;
    } catch { this.persistent = false; return false; }
  }
  read() {
    if (this.loaded) return copy(this.memory);
    this.loaded = true;
    try {
      const record = JSON.parse(this.storage.getItem(this.key));
      if (record?.version !== 1 || ![record.base, record.draft].every(draftShape)) return null;
      this.memory = { version: 1, savedAt: record.savedAt,
        base: cleanState(record.base), draft: cleanState(record.draft), todoEdit: cleanTodoEdit(record.todoEdit) };
      this.persistent = true; return copy(this.memory);
    } catch { return null; }
  }
  clear() {
    this.memory = null; this.loaded = true;
    try { this.storage.removeItem(this.key); this.persistent = true; }
    catch { this.persistent = false; }
  }
}
// Conflicts are deliberately returned to the UI for explicit approval. Local
// edits win only after that approval; polling never calls this function.
export function mergeDraft(base, draft, cloud) {
  const result = copy(cloud), conflicts = [];
  for (const key of new Set([...Object.keys(base.settings), ...Object.keys(draft.settings)])) {
    const before = base.settings[key], local = draft.settings[key], remote = cloud.settings[key];
    if (equal(before, local)) continue;
    if (!equal(before, remote) && !equal(local, remote)) conflicts.push(`settings.${key}`);
    result.settings[key] = copy(local);
  }
  if (!equal(base.todos, draft.todos)) {
    const before = new Map(base.todos.map(todo => [todo.id,todo]));
    const local = new Map(draft.todos.map(todo => [todo.id,todo]));
    const remote = new Map(cloud.todos.map(todo => [todo.id,todo]));
    let conflict = false;
    for (const id of new Set([...before.keys(),...local.keys()])) {
      const a = before.get(id), b = local.get(id), c = remote.get(id);
      if (equal(a,b)) continue;
      if (!equal(a,c) && !equal(b,c)) conflict = true;
      if (b === undefined) remote.delete(id); else remote.set(id,copy(b));
    }
    if (conflict) conflicts.push('todos');
    result.todos = [...remote.values()];
  }
  return { state: result, conflicts };
}
export function acceptSave(submitted, current, cloud) { return mergeDraft(submitted, current, cloud).state; }
export function deviceStatus(device, cloudRevision) {
  if (!device) return 'unknown';
  const current = device.reportedRevision ?? device.appliedRevision ?? 0;
  return current === cloudRevision ? 'current' : 'pending';
}
export class StatusChecker {
  constructor(read, apply, epoch) { this.read = read; this.apply = apply; this.epoch = epoch; this.pending = null; }
  check() {
    if (this.pending) return this.pending;
    const began = this.epoch();
    this.pending = Promise.resolve().then(() => this.read()).then(report => {
      if (began === this.epoch()) this.apply(report);
    }).finally(() => { this.pending = null; });
    return this.pending;
  }
}
export async function requestJSON(path, options = {}, { timeout = 15000, fetcher = fetch } = {}) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(new Error('请求超时；保存结果待确认，请先检查云端版本')), timeout);
  try {
    const response = await fetcher(path, { credentials: 'same-origin', cache: 'no-store', ...options,
      signal: controller.signal,
      headers: { ...(options.body ? { 'Content-Type': 'application/json' } : {}), ...options.headers } });
    const payload = response.status === 204 ? null : await response.json().catch(() => {
      if (controller.signal.aborted) throw controller.signal.reason;
      return null;
    });
    if (!response.ok) {
      const cause = new Error(payload?.error || '网络请求失败，请稍后重试');
      cause.status = response.status; throw cause;
    }
    if (response.status !== 204 && payload === null) throw new Error('服务响应不完整，请重新检查状态');
    return payload;
  } finally { clearTimeout(timer); }
}
