import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Script } from 'node:vm';
import { TodoEditSession } from '../public/product-flow.js';
import { DraftJournal } from '../public/edit-session.js';

// Run the shipped load function; only DOM/rendering and remote transport are fixtures.
const source = await readFile(new URL('../public/app.js', import.meta.url), 'utf8');
const load = source.slice(source.indexOf('async function load('), source.indexOf("$('#login-form').addEventListener"));
const state = revision => ({ schema: 1, revision, settings: { countdownLabel: '高考', dateOverrides: [] }, todos: [] });
async function reload({ pending, discard = false }) {
  const base = state(7), draft = structuredClone(base); if (pending) draft.settings.countdownLabel = '期末';
  const journal = new DraftJournal(undefined), nodes = new Map(); let confirmations = 0;
  const context = {
    saved: base, draft, journal, todoEditor: new TodoEditSession(), structuredClone, stateEpoch: 0, cloudRevision: 7, activeTab: 'todos', device: null,
    api: async () => ({ state: state(8), device: null }),
    editsPending: () => pending,
    $: selector => { if (!nodes.has(selector)) nodes.set(selector, {}); return nodes.get(selector); },
    render() {}, updateDraftNotice() {}, confirm() { confirmations++; return false; },
  };
  await new Script(`${load}\nload({discard:${discard}});`).runInNewContext(context);
  return { context, confirmations };
}
test('same-page relogin preserves unsaved edits and their base without discarding them', async () => {
  const { context, confirmations } = await reload({ pending: true });
  assert.equal(context.saved.revision, 7); assert.equal(context.cloudRevision, 8);
  assert.equal(context.draft.settings.countdownLabel, '期末'); assert.equal(confirmations, 0);
});
test('explicit discard and clean reload use the current cloud version', async () => {
  for (const options of [{ pending: true, discard: true }, { pending: false }]) {
    const { context } = await reload(options);
    assert.equal(context.saved.revision, 8); assert.equal(context.draft.settings.countdownLabel, '高考');
  }
});
