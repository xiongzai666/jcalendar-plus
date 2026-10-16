import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { buildDemo } from '../tools/build-demo.mjs';

test('generated demo state language describes browser storage and simulated device evidence', async t => {
  const dir = await mkdtemp(join(tmpdir(), 'jcalendar-demo-status-'));
  t.after(() => rm(dir, { recursive: true, force: true }));
  await buildDemo(dir);
  const product = await import(pathToFileURL(join(dir, 'product-flow.js')));
  const session = await import(pathToFileURL(join(dir, 'demo/session.js')));
  const response = await session.requestJSON('/api/admin/state');
  const status = product.syncResult({ saved: response.state, revision: response.state.revision, device: response.device });
  assert.doesNotMatch(status.headline + status.detail + status.evidence, /云端|(?<!模拟)设备已确认/);
  assert.match(status.headline + status.detail, /模拟/);
  const stale=product.syncResult({saved:response.state,revision:response.state.revision,device:{...response.device,seenAt:new Date(Date.now()-5*3600000).toISOString()}});
  assert.doesNotMatch(stale.action,/短按|实物/);assert.match(stale.action,/演示/);
  const confirmed=product.syncResult({saved:response.state,revision:response.state.revision,device:response.device});
  assert.match(confirmed.action,/模拟/);assert.doesNotMatch(confirmed.action,/请核对实物/);
  const draft = product.syncResult({ saved: response.state, revision: response.state.revision, device: response.device, dirty: true });
  assert.match(draft.headline + draft.action, /浏览器/);
});
