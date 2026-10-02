import assert from 'node:assert/strict';
import { createHash, randomBytes, scryptSync } from 'node:crypto';
import test from 'node:test';
import { readFileSync } from 'node:fs';
import { handleRequest } from '../cloud-functions/lib/app.js';
import { DEFAULT_SETTINGS } from '../cloud-functions/lib/defaults.js';
import { validateSettings, validateUpdate } from '../cloud-functions/lib/state.js';

const token = randomBytes(32).toString('base64url');
const salt = randomBytes(16);
const env = {
  ADMIN_PASSWORD_SCRYPT: `${salt.toString('hex')}:${scryptSync('test-passphrase', salt, 64).toString('hex')}`,
  SESSION_SECRET: randomBytes(32).toString('hex'),
  DEVICE_TOKEN_SHA256: createHash('sha256').update(token).digest('hex'),
  PUBLIC_ORIGIN: 'https://config.example.test',
};

function request(path, method = 'GET', body, headers = {}) {
  return new Request(`https://config.example.test${path}`, {
    method,
    headers: { ...(body ? { 'Content-Type': 'application/json' } : {}), ...headers },
    body: body ? JSON.stringify(body) : undefined,
  });
}

function fakeStore() {
  const values = new Map();
  return {
    async get(key) { return values.get(key) || null; },
    async setJSON(key, value) { values.set(key, structuredClone(value)); },
    async delete(key) { values.delete(key); },

  };
}

test('independent date times validate pairs, chronology and legacy mode',()=>{
  const config=structuredClone(DEFAULT_SETTINGS);
  config.dateOverrides=[{date:'2026-10-04',dayOff:false,mode:'custom',lessons:[
    {slot:1,subject:'自习',start:'08:00',end:'09:30'},
    {slot:2,subject:'自习',start:'09:45',end:'11:30'}]}];
  assert.equal(validateSettings(config),null);
  const bad=structuredClone(config);bad.dateOverrides[0].lessons[1].start='09:15';
  assert.match(validateSettings(bad),/重叠|顺序/);
  const partial=structuredClone(config);delete partial.dateOverrides[0].lessons[0].end;
  assert.match(validateSettings(partial),/时间/);
  const wrong=structuredClone(config);wrong.dateOverrides[0].mode='unknown';
  assert.match(validateSettings(wrong),/模式|格式/);
  const legacy=structuredClone(config);delete legacy.dateOverrides[0].mode;
  legacy.dateOverrides[0].lessons=[{slot:1,subject:'英语'}];
  assert.equal(validateSettings(legacy),null);
});
test('independent time payload requires schema capability and preserves old config clients',async()=>{
  const store=fakeStore(),cookie=await login(store);
  const settings=structuredClone(DEFAULT_SETTINGS);
  settings.dateOverrides=[{date:'2026-10-04',dayOff:false,mode:'custom',lessons:[{slot:1,subject:'自习',start:'08:00',end:'09:30'}]}];
  const saved=await handleRequest(request('/api/admin/state','PUT',{revision:0,settings,todos:[]},{cookie}),env,store);
  assert.equal(saved.status,200);
  const headers={authorization:`Bearer ${token}`};
  const old=await handleRequest(request('/api/device/state?revision=0','GET',undefined,headers),env,store);
  assert.equal(old.status,426);
  const fresh=await handleRequest(request('/api/device/state?revision=0&maxSchema=2','GET',undefined,headers),env,store);
  assert.equal(fresh.status,200);assert.equal((await fresh.json()).schema,2);
});

test('device current revision follows rollback separately from historical maximum', async () => {
  const store = fakeStore(), cookie = await login(store);
  await handleRequest(request('/api/admin/state', 'PUT', { revision: 0, settings: DEFAULT_SETTINGS, todos: [] }, { cookie }), env, store);
  const headers = { authorization: `Bearer ${token}` };
  await handleRequest(request('/api/device/ack', 'POST', { revision: 1 }, headers), env, store);
  await handleRequest(request('/api/device/state?revision=0', 'GET', undefined, headers), env, store);
  const result = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  const device = (await result.json()).device;
  assert.equal(device.reportedRevision, 0);
  assert.equal(device.appliedRevision, 0);
  assert.equal(device.highestRevision, 1);
});

test('runtime summary is bounded, retained and available without configuration content', async () => {
  const store = fakeStore(), cookie = await login(store);
  const summary = { firmware: '2.1.0', nowReadAt: 1790812800, dailyReadAt: 1790812800,
    nowFailed: false, dailyFailed: true, nextNetworkAt: 1790820000, activePage: 2 };
  const headers = { authorization: `Bearer ${token}` };
  const response = await handleRequest(request('/api/device/state?revision=0&runtime=' + encodeURIComponent(JSON.stringify(summary)), 'GET', undefined, headers), env, store);
  assert.equal(response.status, 204);
  const report = await handleRequest(request('/api/admin/status', 'GET', undefined, { cookie }), env, store);
  const result = await report.json();
  assert.equal(result.state, undefined);
  assert.equal(result.device.runtime.firmware, '2.1.0');
  assert.equal(result.device.runtime.dailyFailed, true);
  const acknowledged = await handleRequest(request('/api/device/ack', 'POST',
    { revision: 0, runtime: { ...summary, nowReadAt: 1790813000, dailyReadAt: 1790813000, dailyFailed: false } }, headers), env, store);
  assert.equal(acknowledged.status, 204);
  const fresh = await handleRequest(request('/api/admin/status', 'GET', undefined, { cookie }), env, store);
  assert.equal((await fresh.json()).device.runtime.dailyFailed, false);
  const bad = await handleRequest(request('/api/device/state?revision=0&runtime=' + encodeURIComponent(JSON.stringify({ ...summary, wifiPassword: 'never-upload' })), 'GET', undefined, headers), env, store);
  assert.equal(bad.status, 400);
});

test('contract rejects impossible dates, unsupported four-byte glyphs, byte overflow and uint32 overflow', () => {
  for (const settings of [
    { ...DEFAULT_SETTINGS, countdownDate: '20250229' },
    { ...DEFAULT_SETTINGS, countdownLabel: '😀'.repeat(8) },
    { ...DEFAULT_SETTINGS, countdownLabel: '😀' },
    { ...DEFAULT_SETTINGS, earlyReading: '一:语文;二:英语;三:语文;四:英语;五:语文;六:' + '语'.repeat(100) + ';' },
  ]) assert.notEqual(validateSettings(settings), null);
  assert.notEqual(validateUpdate({ revision: 4294967295, settings: DEFAULT_SETTINGS, todos: [] }), null);
});

test('firmware shared contract fixture is also accepted by cloud validation', () => {
  const fixture = JSON.parse(readFileSync(new URL('../../test/fixtures/remote-config.json', import.meta.url), 'utf8'));
  assert.equal(validateSettings(fixture.settings), null);
  assert.equal(validateUpdate({ revision: 0, settings: fixture.settings, todos: fixture.todos }), null);
});

test('cloud rejects delimiter forms that firmware would refuse', () => {
  for (const settings of [
    { ...DEFAULT_SETTINGS, earlyReading: DEFAULT_SETTINGS.earlyReading.replace('一:待配置;', '一:待配置:;') },
    { ...DEFAULT_SETTINGS, studySchedule: DEFAULT_SETTINGS.studySchedule.replace('444;', '444;;') },
    { ...DEFAULT_SETTINGS, dailyRoutine: DEFAULT_SETTINGS.dailyRoutine.replace(';', ';;') },
    { ...DEFAULT_SETTINGS, countdownDate: 20270101 },
  ]) assert.notEqual(validateSettings(settings), null);
});

async function login(store) {
  const response = await handleRequest(request('/api/session', 'POST',
    { password: 'test-passphrase' }), env, store);
  assert.equal(response.status, 200);
  return response.headers.get('set-cookie').split(';')[0];
}

test('admin writes a versioned state; device can read but cannot edit it', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  const payload = {
    revision: 0,
    settings: DEFAULT_SETTINGS,
    todos: [{ id: 'todo-001', title: '准备数学试卷', due: '', priority: 'high', done: false }],
  };
  const saved = await handleRequest(request('/api/admin/state', 'PUT', payload,
    { cookie, origin: 'https://config.example.test' }), env, store);
  assert.equal(saved.status, 200);
  assert.equal((await saved.json()).state.revision, 1);

  const denied = await handleRequest(request('/api/admin/state', 'PUT', payload,
    { authorization: `Bearer ${token}` }), env, store);
  assert.equal(denied.status, 401);

  const device = await handleRequest(request('/api/device/state?revision=0', 'GET', undefined,
    { authorization: `Bearer ${token}` }), env, store);
  assert.equal(device.status, 200);
  assert.equal((await device.json()).todos[0].title, '准备数学试卷');

  const unchanged = await handleRequest(request('/api/device/state?revision=1', 'GET', undefined,
    { authorization: `Bearer ${token}` }), env, store);
  assert.equal(unchanged.status, 204);

  const stale = await handleRequest(request('/api/admin/state', 'PUT', payload,
    { cookie }), env, store);
  assert.equal(stale.status, 409);
});

test('rejects malformed schedules, cross-origin writes and wrong device tokens', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  const bad = await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0,
    settings: { studySchedule: '444;一,数学;' },
    todos: [],
  }, { cookie }), env, store);
  assert.equal(bad.status, 400);

  const crossOrigin = await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0, settings: DEFAULT_SETTINGS, todos: [],
  }, { cookie, origin: 'https://attacker.example' }), env, store);
  assert.equal(crossOrigin.status, 403);

  const device = await handleRequest(request('/api/device/state?revision=0', 'GET', undefined,
    { authorization: 'Bearer invalid' }), env, store);
  assert.equal(device.status, 401);
});

test('lesson times come from twelve ordered rows in the editable routine', () => {
  const changed = DEFAULT_SETTINGS.dailyRoutine
    .replace('13:30-13:40,课间', '13:30-13:45,课间')
    .replace('13:40-14:25,第5节', '13:45-14:25,第5节');
  assert.equal(validateSettings({ ...DEFAULT_SETTINGS, dailyRoutine: changed }), null);
  assert.match(validateSettings({ ...DEFAULT_SETTINGS,
    dailyRoutine: changed.replace('第5节', '课外活动') }), /12 节课程/);
  assert.match(validateSettings({ ...DEFAULT_SETTINGS,
    dailyRoutine: changed.replace('第5节', '第4节') }), /12 节课程/);
});

test('import validation requires admin login and does not write cloud state', async () => {
  const store = fakeStore();
  const payload = { revision: 0, settings: DEFAULT_SETTINGS, todos: [] };
  const denied = await handleRequest(request('/api/admin/validate', 'POST', payload), env, store);
  assert.equal(denied.status, 401);
  const cookie = await login(store);
  const valid = await handleRequest(request('/api/admin/validate', 'POST', payload,
    { cookie }), env, store);
  assert.equal(valid.status, 200);
  const state = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  assert.equal((await state.json()).state.revision, 0);
});

test('accepts the configured public origin when the platform rewrites the function URL', async () => {
  const store = fakeStore();
  const response = await handleRequest(new Request('http://internal.edgeone.local/api/session', {
    method: 'POST',
    headers: { Origin: 'https://config.example.test', 'Content-Type': 'application/json' },
    body: JSON.stringify({ password: 'test-passphrase' }),
  }), env, store);
  assert.equal(response.status, 200);

  const denied = await handleRequest(new Request('http://internal.edgeone.local/api/session', {
    method: 'POST',
    headers: { Origin: 'https://attacker.example', 'Content-Type': 'application/json' },
    body: JSON.stringify({ password: 'test-passphrase' }),
  }), env, store);
  assert.equal(denied.status, 403);
});

test('device acknowledgements report the applied version', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0, settings: DEFAULT_SETTINGS, todos: [],
  }, { cookie }), env, store);
  const ack = await handleRequest(request('/api/device/ack', 'POST', { revision: 1 },
    { authorization: `Bearer ${token}` }), env, store);
  assert.equal(ack.status, 204);
  const admin = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  assert.equal((await admin.json()).device.appliedRevision, 1);
});

test('sync log records recovery without exposing device data and stays bounded', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0, settings: DEFAULT_SETTINGS, todos: [],
  }, { cookie }), env, store);
  const recovered = { code: 'wifi', count: 3, at: 1790442000 };
  const first = await handleRequest(request('/api/device/ack', 'POST',
    { revision: 1, lastFailure: recovered }, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(first.status, 204);
  let admin = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  let device = (await admin.json()).device;
  assert.deepEqual(device.events[0].recovered, recovered);
  assert.equal(device.events[0].kind, 'applied');
  assert.equal(device.lastFailure.code, 'wifi');

  const invalid = await handleRequest(request('/api/device/ack', 'POST',
    { revision: 1, lastFailure: { code: 'password', count: 1, at: 0 } },
    { authorization: `Bearer ${token}` }), env, store);
  assert.equal(invalid.status, 400);
  for (let i = 0; i < 15; ++i) {
    const ack = await handleRequest(request('/api/device/ack', 'POST', { revision: 1 },
      { authorization: `Bearer ${token}` }), env, store);
    assert.equal(ack.status, 204);
  }
  admin = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  device = (await admin.json()).device;
  assert.equal(device.events.length, 12);
  assert.equal(device.lastFailure.code, 'wifi');
  assert.equal(JSON.stringify(device.events).includes('password'), false);
});

test('unchanged device check confirms revision and reports an offline failure in one request', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0, settings: DEFAULT_SETTINGS, todos: [],
  }, { cookie }), env, store);
  const check = await handleRequest(request(
    '/api/device/state?revision=1&failureCode=connect&failureCount=2&failureAt=1790442000',
    'GET', undefined, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(check.status, 204);
  const admin = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  const device = (await admin.json()).device;
  assert.equal(device.appliedRevision, 1);
  assert.equal(device.events.at(-1).kind, 'checked');
  assert.deepEqual(device.events.at(-1).recovered, { code: 'connect', count: 2, at: 1790442000 });

  const invalid = await handleRequest(request(
    '/api/device/state?revision=1&failureCode=unknown&failureCount=2&failureAt=1790442000',
    'GET', undefined, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(invalid.status, 400);
});

test('a delayed retry is identified in the existing sync log', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0, settings: DEFAULT_SETTINGS, todos: [],
  }, { cookie }), env, store);
  const check = await handleRequest(request(
    '/api/device/state?revision=1&retry=1&failureCode=connect&failureCount=1&failureAt=1790442000',
    'GET', undefined, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(check.status, 204);
  const admin = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  const event = (await admin.json()).device.events.at(-1);
  assert.equal(event.retry, true);
  assert.equal(event.kind, 'checked');
  assert.equal(event.recovered.count, 1);

  const invalid = await handleRequest(request('/api/device/state?revision=1&retry=false',
    'GET', undefined, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(invalid.status, 400);
});

test('device reports its last successful network without sharing Wi-Fi credentials', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  const check = await handleRequest(request('/api/device/state?revision=0&network=backup',
    'GET', undefined, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(check.status, 204);
  const admin = await handleRequest(request('/api/admin/state', 'GET', undefined, { cookie }), env, store);
  const device = (await admin.json()).device;
  assert.equal(device.lastNetwork, 'backup');
  assert.equal(device.events.at(-1).network, 'backup');
  assert.equal(JSON.stringify(device).includes('SSID'), false);
  const invalid = await handleRequest(request('/api/device/state?revision=0&network=unknown',
    'GET', undefined, { authorization: `Bearer ${token}` }), env, store);
  assert.equal(invalid.status, 400);
});

test('date-specific class changes and days off survive the device projection', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  const settings = { ...DEFAULT_SETTINGS, dateOverrides: [
    { date: '2026-10-01', dayOff: true, lessons: [] },
    { date: '2026-10-08', dayOff: false, lessons: [{ slot: 5, subject: '地理' }] },
  ] };
  const saved = await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 0, settings, todos: [],
  }, { cookie }), env, store);
  assert.equal(saved.status, 200);
  const device = await handleRequest(request('/api/device/state?revision=0', 'GET', undefined,
    { authorization: `Bearer ${token}` }), env, store);
  assert.deepEqual((await device.json()).settings.dateOverrides, settings.dateOverrides);
  const invalid = await handleRequest(request('/api/admin/state', 'PUT', {
    revision: 1, settings: { ...settings, dateOverrides: [
      { date: '2026-10-08', dayOff: true, lessons: [{ slot: 5, subject: '地理' }] },
    ] }, todos: [],
  }, { cookie }), env, store);
  assert.equal(invalid.status, 400);
});

test('configuration history restores a prior snapshot as a new revision', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  const firstTodos = [{ id: 'todo-001', title: '第一项', due: '', priority: 'normal', done: false }];
  const secondTodos = [{ id: 'todo-002', title: '第二项', due: '', priority: 'high', done: false }];
  const put = (revision, todos) => handleRequest(request('/api/admin/state', 'PUT',
    { revision, settings: DEFAULT_SETTINGS, todos }, { cookie }), env, store);
  assert.equal((await put(0, firstTodos)).status, 200);
  assert.equal((await put(1, secondTodos)).status, 200);

  const unauthenticated = await handleRequest(request('/api/admin/history'), env, store);
  assert.equal(unauthenticated.status, 401);
  const deniedRestore = await handleRequest(request('/api/admin/restore', 'POST',
    { revision: 2, restoreRevision: 1 }), env, store);
  assert.equal(deniedRestore.status, 401);
  const history = await handleRequest(request('/api/admin/history', 'GET', undefined, { cookie }), env, store);
  assert.deepEqual((await history.json()).versions.map(version => version.revision), [2, 1]);

  const stale = await handleRequest(request('/api/admin/restore', 'POST',
    { revision: 1, restoreRevision: 1 }, { cookie }), env, store);
  assert.equal(stale.status, 409);
  const restored = await handleRequest(request('/api/admin/restore', 'POST',
    { revision: 2, restoreRevision: 1 }, { cookie }), env, store);
  assert.equal(restored.status, 200);
  const state = (await restored.json()).state;
  assert.equal(state.revision, 3);
  assert.equal(state.restoredFrom, 1);
  assert.deepEqual(state.todos, firstTodos);
  const after = await handleRequest(request('/api/admin/history', 'GET', undefined, { cookie }), env, store);
  assert.deepEqual((await after.json()).versions.map(version => version.revision), [3, 2, 1]);

  const device = await handleRequest(request('/api/device/state?revision=2', 'GET', undefined,
    { authorization: `Bearer ${token}` }), env, store);
  assert.deepEqual((await device.json()).todos, firstTodos);
});

test('history retains only the ten latest revisions', async () => {
  const store = fakeStore();
  const cookie = await login(store);
  for (let revision = 0; revision < 12; ++revision) {
    const response = await handleRequest(request('/api/admin/state', 'PUT',
      { revision, settings: DEFAULT_SETTINGS, todos: [] }, { cookie }), env, store);
    assert.equal(response.status, 200);
  }
  const history = await handleRequest(request('/api/admin/history', 'GET', undefined, { cookie }), env, store);
  assert.deepEqual((await history.json()).versions.map(version => version.revision),
    [12, 11, 10, 9, 8, 7, 6, 5, 4, 3]);
  const tooOld = await handleRequest(request('/api/admin/restore', 'POST',
    { revision: 12, restoreRevision: 2 }, { cookie }), env, store);
  assert.equal(tooOld.status, 400);
});
