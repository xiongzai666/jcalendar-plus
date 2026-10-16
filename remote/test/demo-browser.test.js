import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {mkdtemp,readFile,rm,mkdir} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join,resolve,sep} from 'node:path';
import {pathToFileURL} from 'node:url';
import {buildDemo} from '../tools/build-demo.mjs';
const modulePath=process.env.PRODUCT_BROWSER_MODULE;
test('built Pages demo supports compact navigation, safe routine sorting and local save/reload on phone and desktop',{skip:!modulePath},async()=>{
 const output=await mkdtemp(join(tmpdir(),'jcalendar-demo-browser-'));
 await buildDemo(output);
 const {chromium}=await import(pathToFileURL(modulePath));
 const browser=await chromium.launch({headless:true,...(process.env.PRODUCT_BROWSER_EXECUTABLE?{executablePath:process.env.PRODUCT_BROWSER_EXECUTABLE}:{})});
 const server=createServer(async(req,res)=>{
  try{
   const path=new URL(req.url,'http://localhost').pathname;
   if(!path.startsWith('/jcalendar-plus/'))throw Error('subpath');
   const name=path.slice('/jcalendar-plus/'.length)||'index.html',file=resolve(output,name);
   if(!file.startsWith(output+sep))throw Error('path');
   res.writeHead(200,{'Content-Type':file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':file.endsWith('.svg')?'image/svg+xml':'text/html'}).end(await readFile(file));
  }catch{res.writeHead(404).end();}
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 const base=`http://127.0.0.1:${server.address().port}`;
 try{
  for(const width of [1280,390]){
   const context=await browser.newContext({viewport:{width,height:900}}),page=await context.newPage(),errors=[],unexpected=[];
   page.on('pageerror',e=>errors.push(e.message));
   page.on('request',r=>{if(!r.url().startsWith(base+'/jcalendar-plus/')||r.url().includes('/api/'))unexpected.push(r.url());});
   page.once('dialog',d=>d.accept());
   await page.goto(base+'/jcalendar-plus/');await page.locator('#workspace').waitFor({state:'visible'});
   assert.match(await page.title(),/交互演示/);
   assert.equal(await page.locator('#device-details').count(),1);
   assert.equal(await page.locator('#device-details').getAttribute('open'),null);
   assert.match(await page.locator('#sync-headline').innerText(),/模拟/);
   assert.equal(await page.locator('#sync-network').isVisible(),false);
   assert.equal(await page.locator('.tab').count(),6);
   assert.equal(await page.locator('.tab').evaluateAll(ns=>new Set(ns.map(n=>n.offsetTop)).size),1);
   assert.equal(await page.getByRole('heading',{level:1}).count(),1);
   await page.locator('#help-toggle').click();assert.equal(await page.locator('#help-content').isVisible(),true);await page.locator('#help-toggle').click();
   for(const tab of ['schedule','overrides','settings','history','todos','routine']){
    await page.locator(`[data-tab="${tab}"]`).click();assert.equal(await page.locator(`#${tab}-panel`).isVisible(),true);
    assert.equal(await page.locator(`[data-tab="${tab}"]`).evaluate(n=>{const a=n.getBoundingClientRect(),b=n.parentElement.getBoundingClientRect();return a.left>=b.left-1&&a.right<=b.right+1;}),true);
   }
   assert.equal(await page.locator('.routine-name').count(),12);
   assert.equal(await page.locator('.routine-row:has(.routine-name) input[type=text],.routine-row:has(.routine-name) button').count(),0);
   assert.equal(await page.locator('.routine-row input[type=time]').count(),24);
   const screenshots=process.env.PRODUCT_BROWSER_OUTPUT;
   if(screenshots){await mkdir(screenshots,{recursive:true});await page.screenshot({path:join(screenshots,`demo-normal-${width}.png`)});}
   await page.locator('#add-routine').click();
   const last=page.locator('.routine-row').last();
   for(const [i,time] of [[0,'07:00'],[1,'07:20']]){await last.locator('input[type=time]').nth(i).fill(time);await last.locator('input[type=time]').nth(i).press('Tab');}
   await last.locator('input[type=text]').fill('示例活动');await last.locator('input[type=text]').press('Tab');
   await page.locator('#sort-routine').click();assert.equal(await page.locator('.routine-row.has-error').count(),0);
   const first=page.locator('.routine-row').first();assert.equal(await first.locator('input[type=text]').inputValue(),'示例活动');
   await first.locator('input[type=time]').nth(1).fill('08:10');await first.locator('input[type=time]').nth(1).press('Tab');
   assert.equal(await page.locator('.routine-row.has-error').count(),2);
   await page.locator('#save').click();assert.equal(await page.evaluate(()=>JSON.parse(localStorage.getItem('jcalendar.demo.state.v1')).current.revision),1);
   assert.equal(await page.locator('#save-bar').isVisible(),true);
   if(screenshots)await page.screenshot({path:join(screenshots,`demo-error-${width}.png`)});
   await first.locator('input[type=time]').nth(1).fill('07:20');await first.locator('input[type=time]').nth(1).press('Tab');
   await page.locator('#save').click();await page.locator('#save-bar').waitFor({state:'hidden'});
   assert.equal(await page.evaluate(()=>JSON.parse(localStorage.getItem('jcalendar.demo.state.v1')).current.revision),2);
   await page.reload();await page.locator('#workspace').waitFor({state:'visible'});await page.locator('[data-tab="routine"]').click();
   assert.equal(await page.locator('.routine-row').first().locator('input[type=text]').inputValue(),'示例活动');
   assert.equal(await page.locator('.routine-row').count(),13);
   assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
   await page.locator('#device-details > summary').click();await page.locator('#diagnostic-details summary').click();
   assert.match(await page.locator('#diagnostic-output').textContent(),/physicalDisplay/);
   assert.match(await page.locator('#device-details').innerText(),/模拟/);
   assert.deepEqual(errors,[]);assert.deepEqual(unexpected,[]);
   await context.close();
  }
 }finally{await browser.close();await new Promise(r=>server.close(r));await rm(output,{recursive:true,force:true});}
});
