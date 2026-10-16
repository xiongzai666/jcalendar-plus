import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash, scryptSync } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { loadEdgeOneHandler } from '../test-support/edgeone-handler.mjs';

const bundle = process.env.JC_EDGEONE_BUNDLE;
const token = 'fixture-device-token-0123456789abcdef';
const salt = Buffer.from('0123456789abcdef0123456789abcdef', 'hex');
const env = {
  ADMIN_PASSWORD_SCRYPT: `${salt.toString('hex')}:${scryptSync('fixture-password-only', salt, 64).toString('hex')}`,
  SESSION_SECRET: 'fixture-session-secret-0123456789abcdef0123456789abcdef',
  DEVICE_TOKEN_SHA256: createHash('sha256').update(token).digest('hex'),
  PUBLIC_ORIGIN: 'https://config.example.test',
};

test('complete CLI server entry authenticates, persists, projects, acknowledges and restores isolated history',
  { skip: !bundle && 'Run npm run build:candidate to verify the actual CLI output' }, async () => {
  const handle = await loadEdgeOneHandler(bundle);
  const values = new Map();
  const store = {
    async get(key) { return values.has(key) ? structuredClone(values.get(key)) : null; },
    async setJSON(key, value) { values.set(key, structuredClone(value)); },
    async delete(key) { values.delete(key); },
  };
  const request = (path, method = 'GET', body, headers = {}) => new Request(env.PUBLIC_ORIGIN + path, {
    method, headers: { ...(body ? { 'content-type': 'application/json' } : {}), ...headers },
    body: body ? JSON.stringify(body) : undefined,
  });
  const call = (path, method, body, headers) => handle(request(path, method, body, headers), env, store);
  assert.equal((await call('/api/health')).status, 200);
  assert.equal((await call('/api/admin/history')).status, 401);
  assert.equal((await call('/api/device/state?revision=0', 'GET', undefined, { authorization: 'Bearer wrong' })).status, 401);
  const logged = await call('/api/session', 'POST', { password: 'fixture-password-only' });
  assert.equal(logged.status, 200);
  const cookie = logged.headers.get('set-cookie').split(';')[0];
  const admin = { cookie }, device = { authorization: `Bearer ${token}` };
  const fixture = JSON.parse(await readFile(new URL('../../test/fixtures/remote-config.json', import.meta.url), 'utf8'));
  const settings = fixture.settings;
  settings.festivalCountdown={mode:"term",target:23};
  const todos = Array.from({ length: 8 }, (_, i) => ({ id: `fixture-${i}`, title: `任务${i}`, due: '', priority: 'normal', done: i === 7 }));
  const saved = await call('/api/admin/state', 'PUT', { revision: 0, settings, todos }, admin);
  assert.equal(saved.status, 200); assert.equal((await saved.json()).state.revision, 1);
  assert.equal(values.get('devices/main/state.json').todos.length, 8);
  const projected = await call('/api/device/state?revision=0&maxSchema=2', 'GET', undefined, device);
  assert.equal(projected.status, 200); const projection = await projected.json();
  assert.deepEqual(projection.settings.festivalCountdown,{mode:"term",target:23});
  assert.equal(projection.revision, 1); assert.equal(projection.pendingCount, 7); assert.equal(projection.todos.length, 6);
  assert.ok(projection.todos.every(item => !item.done));
  assert.equal((await call('/api/admin/history', 'GET', undefined, device)).status, 401);
  assert.equal((await call('/api/admin/state', 'PUT', { revision: 1, settings, todos: [] }, device)).status, 401);
  assert.equal((await call('/api/admin/state', 'PUT', { revision: 0, settings, todos: [] }, admin)).status, 409);
  assert.equal((await call('/api/device/ack', 'POST', { revision: 1 }, device)).status, 204);
  assert.equal((await call('/api/device/state?revision=1', 'GET', undefined, device)).status, 204);
  const custom = structuredClone(settings);
  custom.dateOverrides = [{ date: '2026-10-09', dayOff: false, mode: 'custom', lessons: [{ slot: 1, subject: '自习', start: '08:00', end: '09:30' }] }];
  assert.equal((await call('/api/admin/state', 'PUT', { revision: 1, settings: custom, todos: [] }, admin)).status, 200);
  assert.equal((await call('/api/device/state?revision=1', 'GET', undefined, device)).status, 426);
  const modern = await call('/api/device/state?revision=1&maxSchema=2', 'GET', undefined, device);
  assert.equal(modern.status, 200); assert.equal((await modern.json()).schema, 2);
  const history = await call('/api/admin/history', 'GET', undefined, admin);
  assert.deepEqual((await history.json()).versions.map(item => item.revision), [2, 1]);
  const restored = await call('/api/admin/restore', 'POST', { revision: 2, restoreRevision: 1 }, admin);
  assert.equal(restored.status, 200); assert.equal((await restored.json()).state.revision, 3);
  assert.equal(values.get('devices/main/state.json').todos.length, 8);
  assert.equal(values.get('devices/main/history/2.json').settings.dateOverrides[0].mode, 'custom');
  const status = await call('/api/admin/status', 'GET', undefined, admin);
  assert.equal((await status.json()).device.appliedRevision, 1);
});
