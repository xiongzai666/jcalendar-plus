import test from 'node:test';
import assert from 'node:assert/strict';
import {createDemoAPI, DEMO_STATE_KEY, createDemoSeed} from '../demo/model.js';
import {validateUpdate} from '../cloud-functions/lib/state.js';
const now=()=>new Date('2026-10-02T08:00:00Z');
function storage(){const data=new Map([['unrelated','keep']]);return {getItem:key=>data.get(key)||null,setItem:(key,value)=>data.set(key,value),removeItem:key=>data.delete(key),data};}
const create=store=>createDemoAPI({storage:store,validateUpdate,now});
const state=async api=>(await api.request('/api/admin/state')).state;
test('demo seeds a valid sample with simulated status and no account needed',async()=>{
 const api=create(storage()),seed=createDemoSeed(now());
 assert.equal(validateUpdate(seed),null);
 assert.deepEqual(await api.request('/api/session'),{authenticated:true});
 const response=await api.request('/api/admin/status');
 assert.equal(response.device.simulated,true);assert.equal(response.revision,1);
});
test('save persists only to this browser and revisions preserve restore history',async()=>{
 const store=storage(),api=create(store),first=await state(api);const changed=structuredClone(first);changed.todos[0].title='演示修改';
 const saved=await api.request('/api/admin/state',{method:'PUT',body:JSON.stringify(changed)});
 assert.equal(saved.device.reportedRevision,2);assert.equal(saved.device.simulated,true);
 assert.equal((await state(create(store))).todos[0].title,'演示修改');
 assert.notEqual((await state(create(storage()))).todos[0].title,'演示修改');
 await api.request('/api/admin/restore',{method:'POST',body:JSON.stringify({revision:2,restoreRevision:1})});
 const restored=await state(api);assert.equal(restored.revision,3);assert.equal(restored.todos[0].title,first.todos[0].title);
 assert.equal((await api.request('/api/admin/history')).versions.length,3);
});
test('invalid writes and stale versions never replace saved demo data',async()=>{
 const api=create(storage()),first=await state(api);const invalid=structuredClone(first);invalid.settings.countdownDate='20250229';
 await assert.rejects(api.request('/api/admin/state',{method:'PUT',body:JSON.stringify(invalid)}),e=>e.status===400);
 const wrong=structuredClone(first);wrong.revision=0;
 await assert.rejects(api.request('/api/admin/state',{method:'PUT',body:JSON.stringify(wrong)}),e=>e.status===409);
 assert.deepEqual(await state(api),first);
});
test('reset removes only demo records and unavailable storage remains usable',async()=>{
 const store=storage(),api=create(store);const first=await state(api);first.todos=[];
 await api.request('/api/admin/state',{method:'PUT',body:JSON.stringify(first)});
 await api.request('/api/demo/reset',{method:'POST'});
 assert.equal((await state(api)).revision,1);assert.equal(store.data.get('unrelated'),'keep');
 const blocked={getItem(){throw Error('blocked');},setItem(){throw Error('blocked');},removeItem(){throw Error('blocked');}};
 const memory=create(blocked);const draft=await state(memory);draft.todos=[];
 await memory.request('/api/admin/state',{method:'PUT',body:JSON.stringify(draft)});
 assert.equal((await state(memory)).todos.length,0);assert.equal(memory.persistent,false);
});
test('corrupt saved history is discarded and unknown URLs are refused locally',async()=>{
 const store=storage();store.setItem(DEMO_STATE_KEY,JSON.stringify({version:1,current:{revision:9},history:[]}));
 const api=create(store);assert.equal((await state(api)).revision,1);
 await assert.rejects(api.request('https://example.com/api/admin/state'),e=>e.status===404);
});
