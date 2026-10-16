import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {randomBytes,scryptSync} from 'node:crypto';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,sep,join} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {handleRequest} from '../cloud-functions/lib/app.js';

test('desktop and phone save all corner modes through real authenticated API and reload without changing the main goal',
 {skip:!process.env.PRODUCT_BROWSER_MODULE},async()=>{
 const {chromium}=await import(pathToFileURL(process.env.PRODUCT_BROWSER_MODULE));
 const browser=await chromium.launch({headless:true,...(process.env.PRODUCT_BROWSER_EXECUTABLE?{executablePath:process.env.PRODUCT_BROWSER_EXECUTABLE}:{})});
 const root=fileURLToPath(new URL('../public/',import.meta.url)),salt=randomBytes(16),values=new Map();
 const env={ADMIN_PASSWORD_SCRYPT:`${salt.toString('hex')}:${scryptSync('local-demo-only',salt,64).toString('hex')}`,SESSION_SECRET:randomBytes(32).toString('hex'),DEVICE_TOKEN_SHA256:randomBytes(32).toString('hex')};
 const store={async get(k){return values.get(k)||null;},async setJSON(k,v){values.set(k,structuredClone(v));},async delete(k){values.delete(k);}};
 const server=createServer(async(req,res)=>{
  try{
   const url=new URL(req.url,env.PUBLIC_ORIGIN);
   if(url.pathname.startsWith('/api/')){
    const chunks=[];for await(const c of req)chunks.push(c);
    const response=await handleRequest(new Request(url,{method:req.method,headers:req.headers,...(chunks.length?{body:Buffer.concat(chunks)}:{})}),env,store);
    res.writeHead(response.status,Object.fromEntries(response.headers)).end(Buffer.from(await response.arrayBuffer()));return;
   }
   const file=resolve(root,`.${url.pathname==='/'?'/index.html':url.pathname}`);
   if(!file.startsWith(root.endsWith(sep)?root:root+sep))throw Error('path');
   res.writeHead(200,{'Content-Type':file.endsWith('.js')?'text/javascript':file.endsWith('.css')?'text/css':'text/html'}).end(await readFile(file));
  }catch{res.writeHead(404).end();}
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));env.PUBLIC_ORIGIN=`http://127.0.0.1:${server.address().port}`;
 try{
  for(const width of [1280,390]){
   values.clear();const context=await browser.newContext({viewport:{width,height:960}}),page=await context.newPage(),errors=[];
   page.on('pageerror',e=>errors.push(e.message));
   await page.goto(env.PUBLIC_ORIGIN);await page.locator('#password').fill('local-demo-only');await page.locator('#login-form button[type=submit]').click();
   await page.locator('#workspace').waitFor({state:'visible'});await page.locator('[data-tab=settings]').click();
   const main=await page.locator('#countdown-label').inputValue(),date=await page.locator('#countdown-date').inputValue();
   assert.equal(await page.locator('#festival-mode').inputValue(),'auto-festival');
   for(const mode of ['festival','term','auto-term','custom','auto-festival']){
    await page.locator('#festival-mode').selectOption(mode);
    let wanted={mode};
    if(mode==='festival'){await page.locator('#festival-target').selectOption('mid-autumn');wanted.target='mid-autumn';}
    if(mode==='term'){assert.equal(await page.locator('#festival-target option').count(),24);await page.locator('#festival-target').selectOption('23');wanted.target=23;}
    if(mode==='custom'){
     await page.locator('#festival-label').fill('宝宝生日');await page.locator('#festival-date').fill('2028-02-29');await page.locator('#festival-date').press('Tab');
     wanted={mode,label:'宝宝生日',date:'20280229'};
     await page.locator('#festival-label').evaluate(n=>{n.value='五个汉字名';n.dispatchEvent(new Event('input',{bubbles:true}));});
     assert.equal(await page.locator('#festival-error').isVisible(),true);
     await page.locator('#save').click();assert.equal(values.get('devices/main/state.json').settings.festivalCountdown.mode,'auto-term');
     await page.locator('#festival-label').fill('宝宝生日');
    }
    const saved=page.waitForResponse(r=>r.url().endsWith('/api/admin/state')&&r.request().method()==='PUT');
    await page.locator('#save').click();assert.equal((await saved).status(),200);await page.locator('#save-bar').waitFor({state:'hidden'});
    assert.deepEqual(values.get('devices/main/state.json').settings.festivalCountdown,wanted);
    await page.reload();await page.locator('#workspace').waitFor({state:'visible'});await page.locator('[data-tab=settings]').click();
    assert.equal(await page.locator('#festival-mode').inputValue(),mode);
    assert.equal(await page.locator('#countdown-label').inputValue(),main);assert.equal(await page.locator('#countdown-date').inputValue(),date);
    if(mode==='custom'){
     assert.equal(await page.locator('#festival-label').inputValue(),'宝宝生日');assert.equal(await page.locator('#festival-date').inputValue(),'2028-02-29');
     if(process.env.PRODUCT_BROWSER_OUTPUT){await mkdir(process.env.PRODUCT_BROWSER_OUTPUT,{recursive:true});await page.screenshot({path:join(process.env.PRODUCT_BROWSER_OUTPUT,`festival-${width}.png`),fullPage:true});}
    }
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth),false);
   }
   assert.deepEqual(errors,[]);await context.close();
  }
 }finally{await browser.close();await new Promise(r=>server.close(r));}
});
