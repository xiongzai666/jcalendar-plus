import test from 'node:test';
import assert from 'node:assert/strict';
import { exportConfig, importConfig } from '../public/config-transfer.js';
import { DEFAULT_SETTINGS } from '../cloud-functions/lib/defaults.js';

test('export contains editable data and imports it without metadata or credentials', () => {
  const state = { revision: 7, settings: DEFAULT_SETTINGS,
    todos: [{ id: 'todo-001', title: '试卷', due: '', priority: 'normal', done: false }] };
  const backup = exportConfig(state);
  assert.deepEqual(importConfig(JSON.stringify(backup)),
    { settings: DEFAULT_SETTINGS, todos: state.todos });
  assert.deepEqual(Object.keys(backup).sort(),
    ['exportedAt', 'format', 'revision', 'settings', 'todos', 'version']);
});

test('malformed or foreign files cannot be imported', () => {
  assert.throws(() => importConfig('{'));
  assert.throws(() => importConfig(JSON.stringify({ format: 'other', version: 1 })));
  assert.throws(() => importConfig('x'.repeat(50001)));
});
