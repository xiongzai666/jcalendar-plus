import test from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {DEFAULT_SETTINGS} from '../cloud-functions/lib/defaults.js';
import {validateSettings} from '../cloud-functions/lib/state.js';
import {handleRequest} from '../cloud-functions/lib/app.js';
import {randomBytes,scryptSync,createHash} from 'node:crypto';
const salt=randomBytes(16), token=randomBytes(32).toString('hex');
const env={ADMIN_PASSWORD_SCRYPT:`${salt.toString('hex')}:${scryptSync('demo-password',salt,64).toString('hex')}`,
 SESSION_SECRET:randomBytes(32).toString('hex'),DEVICE_TOKEN_SHA256:createHash('sha256').update(token).digest('hex')};
const store={async get(){return null;}};
test('public cloud defaults are a valid unpersonalized template',()=>{
 assert.equal(validateSettings(DEFAULT_SETTINGS),null);
 assert.equal(DEFAULT_SETTINGS.countdownLabel,'待配置');
 assert.ok(DEFAULT_SETTINGS.studySchedule.split(';').slice(1,-1).every(row=>row.split(',').slice(1).every(course=>course==='待配置')));
 assert.deepEqual(DEFAULT_SETTINGS.dateOverrides,[]);
});
test('production origin must be explicitly configured',async()=>{
 const request=new Request('https://config.example.test/api/session',{method:'POST',headers:{origin:'https://config.example.test','content-type':'application/json'},body:JSON.stringify({password:'demo-password'})});
 assert.equal((await handleRequest(request,env,store)).status,503);
 assert.equal((await handleRequest(request,{...env,PUBLIC_ORIGIN:'https://config.example.test'},store)).status,200);
});
test('invalid production origin is a setup error even without Origin header',async()=>{
 for(const origin of ['', 'http://config.example.test','https://config.example.test/path','*']){
  const request=new Request('https://config.example.test/api/session');
  assert.equal((await handleRequest(request,{...env,PUBLIC_ORIGIN:origin},store)).status,503);
 }
});
