import test from 'node:test';
import assert from 'node:assert/strict';
const flow = await import('../public/product-flow.js').catch(() => null);
test('editing preserves identity, completion and other fields; cancel leaves draft untouched', () => {
  assert.ok(flow, 'product flow is missing');
  const todo = {id:'keep',title:'旧',due:'',priority:'normal',done:true,createdAt:123};
  const draft = {todos:[todo]};
  const edit = new flow.TodoEditSession(); edit.begin(todo); edit.update('title','新');
  assert.equal(draft.todos[0].title,'旧'); edit.cancel(); assert.equal(draft.todos[0].title,'旧');
  edit.begin(todo);edit.update('title','新');edit.update('due','2026-10-08');edit.update('priority','high');edit.apply(draft);
  assert.deepEqual(draft.todos[0],{...todo,title:'新',due:'2026-10-08',priority:'high'});
});
test('invalid edit remains available and valid edit uses device projection order', () => {
  assert.ok(flow, 'product flow is missing');
  const draft={todos:Array.from({length:7},(_,i)=>({id:String(i),title:'任务',due:`2026-10-${10+i}`,priority:'normal',done:false}))};
  const edit=new flow.TodoEditSession();edit.begin(draft.todos[6]);edit.update('title','');
  assert.throws(()=>edit.apply(draft));assert.equal(edit.value.title,'');
  edit.update('title','提前');edit.update('due','2026-10-08');edit.apply(draft);
  assert.deepEqual(flow.orderedTodos(draft.todos).slice(0,6).map(t=>t.id),['6','0','1','2','3','4']);
});
test('sync result distinguishes draft, cloud, matching receipt and stale evidence', () => {
  assert.ok(flow, 'product flow is missing');
  const now=Date.parse('2026-10-07T12:00:00Z'),saved={revision:3,updatedAt:'2026-10-07T10:00:00Z'};
  assert.equal(flow.syncResult({saved,revision:3,dirty:true,now}).kind,'draft');
  assert.equal(flow.syncResult({saved,revision:3,now}).kind,'waiting');
  const device={reportedRevision:3,seenAt:'2026-10-07T11:00:00Z'};
  const current=flow.syncResult({saved,revision:3,device,now});assert.equal(current.kind,'confirmed');assert.match(current.action,/实物/);
  assert.equal(flow.syncResult({saved,revision:4,device,now}).kind,'connected');
  assert.equal(flow.syncResult({saved,revision:3,device:{...device,seenAt:'2026-10-07T01:00:00Z'},now}).kind,'stale');
});
test('an untouched template is not reported as a user cloud save',()=>{
 assert.equal(flow.syncResult({saved:{revision:0},revision:0}).kind,'template');
});
test('diagnostic whitelist rejects secrets, arbitrary labels, ids and configuration content', () => {
  assert.ok(flow, 'product flow is missing');
  const result=flow.safeDiagnostic({revision:3,dirty:true,device:{reportedRevision:2,seenAt:'2026-10-07T11:00:00Z',id:'unique',token:'secret',email:'name@example.com',lastFailure:{code:'secret'},runtime:{firmware:'2.5.6',driver:'secret',activePage:3,reportedAt:'2026-10-07T11:00:00Z'}},settings:{studySchedule:'姓名'}});
  assert.equal(result.lastError,'未知'); assert.equal(result.deviceRevision,2);
  assert.doesNotMatch(JSON.stringify(result),/secret|unique|example|姓名/);
  assert.equal(flow.safeDiagnostic({device:{runtime:{firmware:'token secret'}}}).firmware,'未知');
});
