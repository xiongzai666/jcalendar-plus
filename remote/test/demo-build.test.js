import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {pathToFileURL} from 'node:url';
import {buildDemo} from '../tools/build-demo.mjs';
import {validateUpdate} from '../cloud-functions/lib/state.js';
import {createDemoSeed} from '../demo/model.js';
test('build is deployable under a repository subpath and contains no cloud authentication or Node validation dependencies',async()=>{
 const dir=await mkdtemp(join(tmpdir(),'jcalendar-demo-'));
 try{
  await buildDemo(dir);
  const html=await readFile(join(dir,'index.html'),'utf8');
  assert.doesNotMatch(html,/(?:src|href)="\/(?!\/)/);
  assert.match(html,/演示/);assert.match(html,/恢复示例/);
  const app=await readFile(join(dir,'app.js'),'utf8');assert.match(app,/demo\/session.js/);
  assert.doesNotMatch(app,/保存到云端/);
  const validation=await readFile(join(dir,'config-validation.js'),'utf8');
  assert.doesNotMatch(validation,/Buffer\.|process\.|node:|loadState|STATE_KEY/);
  const fetchBefore=globalThis.fetch;let networkCalls=0;
  globalThis.fetch=()=>{networkCalls++;throw Error('Demo must not send API requests');};
  try{
   const session=await import(pathToFileURL(join(dir,'demo/session.js')));
   assert.deepEqual(await session.requestJSON('/api/session'),{authenticated:true});
   const {state}=await session.requestJSON('/api/admin/state');state.todos=[];
   const saved=await session.requestJSON('/api/admin/state',{method:'PUT',body:JSON.stringify(state)});
   assert.equal(saved.state.revision,2);assert.equal(networkCalls,0);
  }finally{globalThis.fetch=fetchBefore;}
  const portable=await import('data:text/javascript;base64,'+Buffer.from(validation).toString('base64'));
  const seed=createDemoSeed(new Date('2026-10-02T08:00:00Z'));
  assert.equal(portable.validateUpdate(seed),validateUpdate(seed));
  for(const change of [{countdownDate:'20250229'},{countdownLabel:'😀'},{weatherLocation:'bad'}]){
   const bad={...seed,settings:{...seed.settings,...change}};
   assert.equal(portable.validateUpdate(bad),validateUpdate(bad));
  }
 }finally{await rm(dir,{recursive:true,force:true});}
});
