import test from 'node:test';
import assert from 'node:assert/strict';
import { DraftJournal, mergeDraft, requestJSON, acceptSave, deviceStatus, StatusChecker } from '../public/edit-session.js';
const state = (revision = 1) => ({ schema: 1, revision,
  settings: { countdownLabel: '示例目标', countdownDate: '20270101' }, todos: [] });
const storage = () => { const map = new Map(); return {
  getItem: key => map.get(key) ?? null, setItem: (key,value) => map.set(key,value), removeItem: key => map.delete(key) } };
test('journal preserves draft and base revision across reload, never credentials', () => {
  const mem = storage(), journal = new DraftJournal(mem);
  const base = state(), draft = state(); draft.settings.countdownLabel = '期末';
  draft.password = 'must-not-persist';
  journal.save(base, draft);
  const record = new DraftJournal(mem).read();
  assert.equal(record.base.revision, 1);
  assert.equal(record.draft.settings.countdownLabel, '期末');
  assert.equal(record.draft.password, undefined);
});
test('corrupt or inaccessible local storage does not crash editing', () => {
  const mem = storage(); mem.setItem('jcalendar.draft.v1', '{bad');
  assert.equal(new DraftJournal(mem).read(), null);
  assert.equal(new DraftJournal({ setItem() { throw Error('quota'); } }).save(state(), state()), false);
});
test('rebase merges unrelated changes and identifies same-field conflict', () => {
  const base = state(), draft = state(), cloud = state(2);
  draft.settings.countdownLabel = '期末'; cloud.settings.countdownDate = '20270608';
  let result = mergeDraft(base, draft, cloud);
  assert.deepEqual(result.conflicts, []);
  assert.equal(result.state.settings.countdownLabel, '期末');
  assert.equal(result.state.settings.countdownDate, '20270608');
  cloud.settings.countdownLabel = '月考';
  result = mergeDraft(base, draft, cloud);
  assert.deepEqual(result.conflicts, ['settings.countdownLabel']);
  assert.equal(result.state.revision, 2);
});
test('edits typed during a save remain unsaved after its response', () => {
  const submitted = state(), current = state(), cloud = state(2);
  submitted.settings.countdownLabel = cloud.settings.countdownLabel = '期末';
  current.settings.countdownLabel = '月考';
  assert.equal(acceptSave(submitted, current, cloud).settings.countdownLabel, '月考');
});
test('restoring another version keeps newly typed todos while accepting the restored list', () => {
  const before = state(), current = state(), restored = state(2);
  const todo = id => ({ id, title: id, due: '', done: false, priority: 'normal' });
  before.todos = [todo('old')]; current.todos = [todo('old'),todo('typed')]; restored.todos = [todo('restored')];
  assert.deepEqual(acceptSave(before, current, restored).todos.map(item => item.id), ['restored','typed']);
});
test('journal refuses structurally damaged todos', () => {
  const mem = storage();
  const base = state(), draft = state(); draft.todos = [null];
  mem.setItem('jcalendar.draft.v1', JSON.stringify({ version: 1, base, draft }));
  assert.equal(new DraftJournal(mem).read(), null);
});
test('device rollback is shown as behind even if its historical highest revision is latest', () => {
  assert.equal(deviceStatus({ reportedRevision: 0, appliedRevision: 3, seenAt: '2026-10-01T00:00:00Z' }, 3), 'pending');
  assert.equal(deviceStatus({ reportedRevision: 3 }, 3), 'current');
});
test('a status response started before a save cannot roll the page back to an older cloud revision', async () => {
  let resolve, epoch = 0, shown = 0, reads = 0;
  const checker = new StatusChecker(() => { reads++; return new Promise(done => { resolve = done; }); },
    report => { shown = report.revision; }, () => epoch);
  const pending = checker.check(), duplicate = checker.check();
  await Promise.resolve();
  epoch++; shown = 2;
  resolve({ revision: 1 }); await Promise.all([pending,duplicate]);
  assert.equal(shown, 2); assert.equal(reads, 1);
});
test('request timeout is distinguishable and does not automatically retry writes', async () => {
  let calls = 0;
  await assert.rejects(requestJSON('/api/admin/state', { method: 'PUT' },
    { timeout: 10, fetcher: (_, options) => new Promise((_,reject) => {
      calls++; options.signal.addEventListener('abort', () => reject(options.signal.reason));
    }) }), /超时/);
  assert.equal(calls, 1);
});
