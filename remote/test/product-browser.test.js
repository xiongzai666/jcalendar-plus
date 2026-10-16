import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {join,resolve} from 'node:path';
import {DEFAULT_SETTINGS} from '../cloud-functions/lib/defaults.js';
// Opt-in browser QA: supply a locally installed Playwright module. No download or production request.
const modulePath=process.env.PRODUCT_BROWSER_MODULE;
test('real desktop and mobile editor protects drafts, focus, conflicts, login failures and diagnostic privacy', {skip:!modulePath}, async()=>{
 const {chromium}=await import(pathToFileURL(modulePath));
 const browser=await chromium.launch({headless:true,...(process.env.PRODUCT_BROWSER_EXECUTABLE?{executablePath:process.env.PRODUCT_BROWSER_EXECUTABLE}:{})});
 const root=fileURLToPath(new URL('../public/',import.meta.url));
 let state={schema:1,revision:7,updatedAt:new Date().toISOString(),settings:{...DEFAULT_SETTINGS,countdownLabel:'示例',countdownDate:'20270101'},todos:Array.from({length:7},(_,i)=>({id:`virtual-${i}`,title:`任务${i}`,due:`2026-10-${10+i}`,priority:'normal',done:false}))};
 let saveMode='ok',hold=null,pendingSave=null,cloudRevision=7,statusAuth=false;
 const device={reportedRevision:7,seenAt:new Date().toISOString(),runtime:{firmware:'2.5.6',nowReadAt:0,dailyReadAt:0,nextNetworkAt:1,activePage:3,reportedAt:new Date().toISOString()},token:'never-copy',id:'never-copy',email:'never-copy@example.com'};
 const server=createServer(async(req,res)=>{
  try {
   const path=new URL(req.url,'http://localhost').pathname;
   if(path.startsWith('/api/')) {
    let text='';for await(const chunk of req)text+=chunk;
    let value={};let status=200;
    if(path==='/api/session')value={authenticated:true};
    else if(path==='/api/admin/status'){if(statusAuth){status=401;value={error:'虚拟登录过期'};}else value={revision:cloudRevision,device};}
    else if(path==='/api/admin/state'&&req.method==='PUT') {
     pendingSave=JSON.parse(text);
     if(saveMode==='hold')await new Promise(r=>hold=r);
     if(saveMode==='auth'){status=401;value={error:'登录已过期'};}
     else if(saveMode==='conflict'){status=409;value={error:'云端版本已变化'};cloudRevision=8;}
     else if(saveMode==='fail'){status=503;value={error:'虚拟服务不可用'};}
     else {state={...state,...pendingSave,revision:state.revision+1};cloudRevision=state.revision;value={state};}
    } else if(path==='/api/admin/state')value={state,device};
    else if(path==='/api/admin/radar')value={settings:{revision:3,accountId:'virtual',refreshMinutes:60},profiles:[{id:'virtual',label:'虚拟账号'}],snapshot:{id:'virtual',sampledAt:Math.floor(Date.now()/1000),status:'ok',five:{remaining:0},week:{remaining:50},loop:{state:'failed',reason:'trigger'}},radar:{source:'did',checkedAt:Math.floor(Date.now()/1000)},deviceAck:null,fourthPage:{configuredMode:'todos',reportedMode:'todos',switchSupported:true}};
    else value={versions:[]};
    res.writeHead(status,{'Content-Type':'application/json'}).end(JSON.stringify(value));return;
   }
   const filename=resolve(root,`.${path==='/'?'/index.html':path}`);
   if(!filename.startsWith(root))throw Error('path');
   res.writeHead(200,{'Content-Type':path.endsWith('.js')?'text/javascript':path.endsWith('.css')?'text/css':'text/html'}).end(await readFile(filename));
  }catch{res.writeHead(404).end();}
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const url=`http://127.0.0.1:${server.address().port}`;
 try {
  for(const viewport of [{width:1280,height:900},{width:390,height:844}]) {
   state.todos=Array.from({length:7},(_,i)=>({id:`virtual-${i}`,title:`任务${i}`,due:`2026-10-${10+i}`,priority:'normal',done:i===5}));
   saveMode='ok';pendingSave=null;
   const context=await browser.newContext({viewport});const page=await context.newPage();const errors=[];
   page.on('pageerror',error=>errors.push(error.message));
   await page.goto(url);await page.locator('#workspace').waitFor({state:'visible'});
   await page.getByRole('button',{name:'编辑 任务6',exact:true}).click();
   await page.locator('#todo-edit-title').fill('新标题');await page.locator('#todo-edit-due').fill('2026-10-08');
   await page.getByRole('button',{name:'课程',exact:true}).click();assert.equal(await page.locator('#todos-panel').isVisible(),true);
   await page.locator('#save').click();assert.equal(pendingSave,null);
   await page.getByRole('button',{name:'取消',exact:true}).click();assert.equal(await page.getByRole('button',{name:'编辑 任务6',exact:true}).count(),1);
   await page.getByRole('button',{name:'编辑 任务6',exact:true}).click();await page.locator('#todo-edit-title').fill('提前任务');await page.locator('#todo-edit-due').fill('2026-10-08');await page.locator('#todo-edit-priority').selectOption('high');
   if(process.env.PRODUCT_BROWSER_OUTPUT){await mkdir(process.env.PRODUCT_BROWSER_OUTPUT,{recursive:true});await page.screenshot({path:join(process.env.PRODUCT_BROWSER_OUTPUT,`editor-${viewport.width}.png`),fullPage:true});}
   await page.getByRole('button',{name:'应用到草稿',exact:true}).click();
   assert.equal(await page.locator('.todo-row').first().getAttribute('data-todo-id'),'virtual-6');
   assert.equal(await page.getByRole('button',{name:'编辑 提前任务',exact:true}).evaluate(n=>n===document.activeElement),true);
   for(const mode of ['fail','conflict','auth']) {
    saveMode=mode;await page.locator('#save').click();await page.waitForTimeout(100);
    if(mode==='auth') {await page.locator('#password').fill('virtual-test');await page.getByRole('button',{name:'登录',exact:true}).click();await page.locator('#workspace').waitFor({state:'visible'});}
    assert.equal(await page.getByRole('button',{name:'编辑 提前任务',exact:true}).count(),1);
    assert.equal(await page.locator('#save-bar').isVisible(),true);
   }
   // A save response may arrive while the next edit is being typed.
   saveMode='hold';await page.locator('#save').click();await page.getByRole('button',{name:'编辑 提前任务',exact:true}).click();await page.locator('#todo-edit-title').fill('正在输入');
   await page.locator('#todo-edit-title').evaluate(n=>n.setSelectionRange(2,2));saveMode='ok';hold();await page.waitForTimeout(100);
   assert.equal(await page.locator('#todo-edit-title').inputValue(),'正在输入');
   assert.equal(await page.locator('#todo-edit-title').evaluate(n=>n===document.activeElement),true,'save response must retain editor focus');
   assert.equal(await page.locator('#todo-edit-title').evaluate(n=>n.selectionStart),2);
   // Session expiry during an unapplied edit must preserve that separate editor with its old base.
   statusAuth=true;await page.locator('#reload').click();await page.locator('#login-view').waitFor({state:'visible'});
   statusAuth=false;await page.locator('#password').fill('virtual-test');await page.getByRole('button',{name:'登录',exact:true}).click();await page.locator('#workspace').waitFor({state:'visible'});
   assert.equal(await page.locator('#todo-edit-title').inputValue(),'正在输入');
   assert.equal(await page.evaluate(()=>{const event=new Event('beforeunload',{cancelable:true});window.dispatchEvent(event);return event.defaultPrevented;}),true);
   await page.getByRole('button',{name:'取消',exact:true}).click();
   await page.locator('#device-details > summary').click();
   await page.locator('#diagnostic-details summary').click();
   const diagnostic=await page.locator('#diagnostic-output').textContent();assert.doesNotMatch(diagnostic,/never-copy|virtual-|任务|example.com/);
   assert.match(await page.locator('#sync-headline').textContent(),/已保存|已确认|新配置/);
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),true);
   const output=process.env.PRODUCT_BROWSER_OUTPUT;
   if(output){await mkdir(output,{recursive:true});await page.screenshot({path:join(output,`todos-${viewport.width}.png`),fullPage:true});}
   if(await page.locator('[data-tab="radar"]').count()){await page.locator('[data-tab="radar"]').click();await page.getByRole('heading',{name:'UsageLoop',exact:true}).waitFor();assert.match(await page.locator('#radar-panel').textContent(),/执行失败/);if(output)await page.screenshot({path:join(output,`radar-${viewport.width}.png`),fullPage:true});}
   state.todos=Array.from({length:30},(_,i)=>({id:`virtual-limit-${i}`,title:`事项${i}`,due:'',priority:'normal',done:i>=6}));
   page.once('dialog',dialog=>dialog.accept());await page.reload();await page.locator('#workspace').waitFor({state:'visible'});
   assert.match(await page.locator('#todo-count').textContent(),/30/);assert.match(await page.locator('#todo-capacity').textContent(),/24.*已完成/);
   await page.locator('#todo-title').fill('超限');await page.getByRole('button',{name:'添加待办',exact:true}).click();assert.equal(await page.locator('.todo-row').count(),30);
   assert.deepEqual(errors,[]);await context.close();
  }
 }finally{await browser.close();await new Promise(r=>server.close(r));}
});
