import test from 'node:test';
import assert from 'node:assert/strict';
import {DraftJournal} from '../public/edit-session.js';
test('unapplied todo fields recover with the original base even when the draft itself is clean',()=>{
 const mem=new Map(),storage={setItem:(k,v)=>mem.set(k,v),getItem:k=>mem.get(k),removeItem:k=>mem.delete(k)};
 const base={schema:1,revision:7,settings:{countdownLabel:'目标'},todos:[{id:'a',title:'旧',due:'',priority:'normal',done:false}]};
 const pending={id:'a',title:'未应用',due:'2026-10-08',priority:'high'};
 const journal=new DraftJournal(storage);journal.save(base,base,pending);
 const recovered=new DraftJournal(storage).read();assert.ok(recovered);
 assert.equal(recovered.base.revision,7);assert.deepEqual(recovered.todoEdit,pending);
 assert.equal(recovered.draft.todos[0].title,'旧');
 journal.save(base,base,null);assert.equal(new DraftJournal(storage).read(),null);
});
test('storage failures preserve unapplied edits in current-page memory',()=>{
 const base={schema:1,revision:1,settings:{},todos:[]},pending={id:'a',title:'未应用',due:'',priority:'normal'};
 const journal=new DraftJournal(undefined);assert.equal(journal.save(base,base,pending),false);
 assert.deepEqual(journal.read()?.todoEdit,pending);
});
