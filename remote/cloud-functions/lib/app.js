import { createSession, hasSecrets, SESSION_COOKIE, validDevice, validSession, verifyPassword } from './auth.js';
import { ACK_KEY, HISTORY_LIMIT, historyKey, loadState, STATE_KEY, validateUpdate } from './state.js';

const json = (data, status = 200, extra = {}) => new Response(JSON.stringify(data), {
  status,
  headers: { 'Content-Type': 'application/json; charset=utf-8', 'Cache-Control': 'no-store', ...extra },
});
const noContent = () => new Response(null, { status: 204, headers: { 'Cache-Control': 'no-store' } });
const error = (message, status) => json({ error: message }, status);

async function bodyJSON(request) {
  const text = await request.text();
  if (Buffer.byteLength(text, 'utf8') > 16384) throw new Error('请求内容过大');
  return JSON.parse(text);
}

function configuredOrigin(env) {
  try {
    const url = new URL(env.PUBLIC_ORIGIN);
    if (url.origin !== env.PUBLIC_ORIGIN || url.protocol !== 'https:') return null;
    return url.origin;
  } catch { return null; }
}
const FAILURE_CODES = new Set(['wifi', 'connect', 'auth', 'service', 'data', 'storage', 'config', 'time', 'ack']);
const SYNC_LOG_LIMIT = 12;
const NETWORKS = new Set(['primary', 'backup']);
function validRuntime(value) {
  const keys = ['firmware','nowReadAt','dailyReadAt','nowFailed','dailyFailed','nextNetworkAt','activePage'];
  return value && typeof value === 'object' && !Array.isArray(value) &&
    Object.keys(value).every(key => keys.includes(key)) && keys.every(key => key in value) &&
    typeof value.firmware === 'string' && /^[a-zA-Z0-9._+-]{1,64}$/.test(value.firmware) &&
    ['nowReadAt','dailyReadAt','nextNetworkAt'].every(key => Number.isSafeInteger(value[key]) && value[key] >= 0 && value[key] <= 4102444800) &&
    typeof value.nowFailed === 'boolean' && typeof value.dailyFailed === 'boolean' &&
    [0,1,2,3].includes(value.activePage);
}

async function saveVersion(store, current, next) {
  if (current.revision > 0) await store.setJSON(historyKey(current.revision), current);
  await store.setJSON(historyKey(next.revision), next);
  await store.setJSON(STATE_KEY, next);
  const expiredRevision = next.revision - HISTORY_LIMIT;
  if (expiredRevision > 0) {
    try { await store.delete(historyKey(expiredRevision)); }
    catch (cause) { console.error('History cleanup failed:', cause?.name || 'Error'); }
  }
}

function sameOrigin(request, env) {
  const origin = request.headers.get('origin');
  if (!origin) return true;
  const publicOrigin = configuredOrigin(env);
  if (origin === publicOrigin) return true;
  const requestUrl = new URL(request.url);
  return ['localhost', '127.0.0.1'].includes(requestUrl.hostname) && origin === requestUrl.origin;
}

export async function handleRequest(request, env, store) {
  const url = new URL(request.url);
  const path = url.pathname;
  const method = request.method;

  if (path === '/api/health' && method === 'GET') return json({ ok: true });
  const local = ['localhost', '127.0.0.1'].includes(url.hostname);
  if (!local && !configuredOrigin(env)) return error('请配置 PUBLIC_ORIGIN 为本站 HTTPS 源', 503);
  if (!hasSecrets(env)) return error('服务尚未完成安全配置', 503);
  if (!sameOrigin(request, env)) return error('请求来源无效', 403);

  try {
    if (path === '/api/session' && method === 'POST') {
      const body = await bodyJSON(request);
      if (!verifyPassword(body?.password, env.ADMIN_PASSWORD_SCRYPT))
        return error('密码错误', 401);
      return json({ ok: true }, 200, {
        'Set-Cookie': `jc_session=${createSession(env.SESSION_SECRET)}; ${SESSION_COOKIE}`,
      });
    }
    if (path === '/api/session' && method === 'GET')
      return json({ authenticated: validSession(request, env.SESSION_SECRET) });
    if (path === '/api/session' && method === 'DELETE')
      return json({ ok: true }, 200, {
        'Set-Cookie': 'jc_session=; Path=/api; HttpOnly; Secure; SameSite=Strict; Max-Age=0',
      });

    if (path === '/api/admin/validate' && method === 'POST') {
      if (!validSession(request, env.SESSION_SECRET)) return error('请先登录', 401);
      const validation = validateUpdate(await bodyJSON(request));
      return validation ? error(validation, 400) : json({ ok: true });
    }

    if (path === '/api/admin/status' && method === 'GET') {
      if (!validSession(request, env.SESSION_SECRET)) return error('请先登录', 401);
      const [state, ack] = await Promise.all([loadState(store), store.get(ACK_KEY, { type: 'json', consistency: 'strong' })]);
      return json({ revision: state.revision, updatedAt: state.updatedAt, device: ack || null });
    }

    if (path === '/api/admin/state') {
      if (!validSession(request, env.SESSION_SECRET)) return error('请先登录', 401);
      if (method === 'GET') {
        const [state, ack] = await Promise.all([
          loadState(store),
          store.get(ACK_KEY, { type: 'json', consistency: 'strong' }),
        ]);
        return json({ state, device: ack || null });
      }
      if (method === 'PUT') {
        const body = await bodyJSON(request);
        const validation = validateUpdate(body);
        if (validation) return error(validation, 400);
        const current = await loadState(store);
        if (body.revision !== current.revision)
          return json({ error: '云端已有新修改，请重新加载', revision: current.revision }, 409);
        const next = {
          schema: 1,
          revision: current.revision + 1,
          updatedAt: new Date().toISOString(),
          settings: { ...body.settings, dateOverrides: body.settings.dateOverrides || [] },
          todos: body.todos,
        };
        await saveVersion(store, current, next);
        return json({ state: next });
      }
    }

    if (path === '/api/admin/history' && method === 'GET') {
      if (!validSession(request, env.SESSION_SECRET)) return error('请先登录', 401);
      const current = await loadState(store);
      const versions = [current];
      for (let revision = current.revision - 1;
           revision >= 1 && versions.length < HISTORY_LIMIT; --revision) {
        const old = await store.get(historyKey(revision), { type: 'json', consistency: 'strong' });
        if (old && old.revision === revision) versions.push(old);
      }
      return json({ versions });
    }

    if (path === '/api/admin/restore' && method === 'POST') {
      if (!validSession(request, env.SESSION_SECRET)) return error('请先登录', 401);
      const body = await bodyJSON(request);
      const current = await loadState(store);
      if (!Number.isSafeInteger(body?.revision) || body.revision !== current.revision)
        return json({ error: '云端已有新修改，请重新加载', revision: current.revision }, 409);
      if (!Number.isSafeInteger(body?.restoreRevision) ||
          body.restoreRevision < Math.max(1, current.revision - HISTORY_LIMIT + 1) ||
          body.restoreRevision >= current.revision)
        return error('所选历史版本不可恢复', 400);
      const old = await store.get(historyKey(body.restoreRevision), { type: 'json', consistency: 'strong' });
      if (!old || old.revision !== body.restoreRevision) return error('历史版本不存在', 404);
      const validation = validateUpdate({ revision: current.revision, settings: old.settings, todos: old.todos });
      if (validation) return error(`历史版本无法恢复：${validation}`, 400);
      const next = {
        schema: 1, revision: current.revision + 1, updatedAt: new Date().toISOString(),
        restoredFrom: body.restoreRevision,
        settings: old.settings, todos: old.todos,
      };
      await saveVersion(store, current, next);
      return json({ state: next });
    }

    if (path === '/api/device/state' && method === 'GET') {
      if (!validDevice(request, env.DEVICE_TOKEN_SHA256)) return error('设备未授权', 401);
      const state = await loadState(store);
      const deviceRevision = Number(url.searchParams.get('revision'));
      if (!url.searchParams.has('revision') || !Number.isSafeInteger(deviceRevision) || deviceRevision < 0 || deviceRevision > 4294967295)
        return error('设备版本无效', 400);
      const maxSchemaText = url.searchParams.get('maxSchema') ?? '1';
      if (!['1', '2'].includes(maxSchemaText)) return error('设备协议版本无效', 400);
      const projectionSchema = state.settings.dateOverrides?.some(day => day.mode === 'custom') ? 2 : 1;
      if (Number(maxSchemaText) < projectionSchema)
        return error('独立日期作息需要升级设备固件至 2.2.0 或更新版本', 426);
      const retryParam = url.searchParams.get('retry');
      if (retryParam !== null && retryParam !== '1') return error('补试标记无效', 400);
      const retry = retryParam === '1';
      const network = url.searchParams.get('network');
      if (network !== null && !NETWORKS.has(network)) return error('网络标记无效', 400);
      const runtimeText = url.searchParams.get('runtime');
      let runtime = null;
      if (runtimeText !== null) {
        if (runtimeText.length > 700) return error('设备摘要过长', 400);
        runtime = JSON.parse(runtimeText);
        if (!validRuntime(runtime)) return error('设备摘要格式无效', 400);
      }
      const failureCode = url.searchParams.get('failureCode');
      let failure = null;
      if (failureCode !== null) {
        const count = Number(url.searchParams.get('failureCount'));
        const at = Number(url.searchParams.get('failureAt'));
        if (!FAILURE_CODES.has(failureCode) || !Number.isInteger(count) || count < 1 || count > 255 ||
            !Number.isSafeInteger(at) || at < 0 || at > 4102444800)
          return error('故障记录格式无效', 400);
        failure = { code: failureCode, count, at };
      }
      const seenAt = new Date().toISOString();
      const oldAck = await store.get(ACK_KEY, { type: 'json', consistency: 'strong' });
      const alreadyReported = failure && oldAck?.lastFailure?.code === failure.code &&
        oldAck.lastFailure.count === failure.count && oldAck.lastFailure.at === failure.at;
      const event = {
        at: seenAt,
        kind: deviceRevision === state.revision ? 'checked' : 'fetched',
        revision: state.revision,
      };
      if (retry) event.retry = true;
      if (network) event.network = network;
      if (failure && !alreadyReported) event.recovered = failure;
      const events = [...(Array.isArray(oldAck?.events) ? oldAck.events : []), event].slice(-SYNC_LOG_LIMIT);
      await store.setJSON(ACK_KEY, {
        ...oldAck,
        seenAt,
        runtime: runtime ? { ...runtime, reportedAt: seenAt } : oldAck?.runtime || null,
        lastNetwork: network || oldAck?.lastNetwork || null,
        reportedRevision: deviceRevision,
        appliedRevision: deviceRevision,
        highestRevision: Math.max(oldAck?.highestRevision || oldAck?.appliedRevision || 0, deviceRevision),
        events,
        lastFailure: failure && !alreadyReported
          ? { ...failure, recoveredAt: seenAt } : oldAck?.lastFailure || null,
      });
      if (deviceRevision === state.revision) return noContent();
      const pending = state.todos.filter(item => !item.done).sort((a, b) =>
        (a.due || '9999').localeCompare(b.due || '9999') ||
        Number(b.priority === 'high') - Number(a.priority === 'high'));
      return json({
        schema: projectionSchema,
        revision: state.revision,
        settings: state.settings,
        pendingCount: pending.length,
        todos: pending.slice(0, 6),
      });
    }

    if (path === '/api/device/ack' && method === 'POST') {
      if (!validDevice(request, env.DEVICE_TOKEN_SHA256)) return error('设备未授权', 401);
      const body = await bodyJSON(request);
      const state = await loadState(store);
      if (!Number.isSafeInteger(body?.revision) || body.revision < 0 || body.revision > state.revision)
        return error('确认版本无效', 400);
      if (body.retry !== undefined && body.retry !== true)
        return error('补试标记无效', 400);
      if (body.network !== undefined && !NETWORKS.has(body.network))
        return error('网络标记无效', 400);
      if (body.runtime !== undefined && !validRuntime(body.runtime)) return error('设备摘要格式无效', 400);
      const failure = body.lastFailure;
      if (failure !== undefined && (!failure || typeof failure !== 'object' ||
          !FAILURE_CODES.has(failure.code) ||
          !Number.isInteger(failure.count) || failure.count < 1 || failure.count > 255 ||
          !Number.isSafeInteger(failure.at) || failure.at < 0 || failure.at > 4102444800))
        return error('故障记录格式无效', 400);
      const oldAck = await store.get(ACK_KEY, { type: 'json', consistency: 'strong' });
      const seenAt = new Date().toISOString();
      const event = {
        at: seenAt,
        kind: body.revision > (oldAck?.appliedRevision || 0) ? 'applied' : 'checked',
        revision: body.revision,
      };
      if (body.retry) event.retry = true;
      if (body.network) event.network = body.network;
      if (failure) event.recovered = { code: failure.code, count: failure.count, at: failure.at };
      const previousEvents = Array.isArray(oldAck?.events) ? oldAck.events : [];
      const combine = body.runtime && body.revision === oldAck?.reportedRevision &&
        previousEvents.at(-1)?.kind === 'checked';
      const events = combine ? [...previousEvents.slice(0,-1), { ...previousEvents.at(-1), ...event }]
        : [...previousEvents, event].slice(-SYNC_LOG_LIMIT);
      await store.setJSON(ACK_KEY, {
        seenAt,
        runtime: body.runtime ? { ...body.runtime, reportedAt: seenAt } : oldAck?.runtime || null,
        lastNetwork: body.network || oldAck?.lastNetwork || null,
        reportedRevision: body.revision,
        appliedRevision: body.revision,
        highestRevision: Math.max(oldAck?.highestRevision || oldAck?.appliedRevision || 0, body.revision),
        events,
        lastFailure: failure ? { ...event.recovered, recoveredAt: seenAt } : oldAck?.lastFailure || null,
      });
      return noContent();
    }

    return error('接口不存在', 404);
  } catch (cause) {
    if (cause?.status === 409 || cause?.status === 503) return error(cause.message, cause.status);
    if (cause instanceof SyntaxError) return error('JSON 格式无效', 400);
    if (cause?.message === '请求内容过大') return error(cause.message, 413);
    console.error('API request failed:', cause?.name || 'Error');
    return error('服务暂时不可用', 500);
  }
}
