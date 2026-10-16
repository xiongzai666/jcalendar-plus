import test from 'node:test';
import assert from 'node:assert/strict';
import { DEFAULT_SETTINGS } from '../cloud-functions/lib/defaults.js';
import { validateSettings } from '../cloud-functions/lib/state.js';
import { exportConfig, importConfig } from '../public/config-transfer.js';

test('right-corner countdown accepts five modes and preserves the independent main goal', () => {
  for (const value of [
    {mode:'auto-festival'}, {mode:'auto-term'},
    {mode:'festival',target:'spring-festival'}, {mode:'festival',target:'qingming'},
    ...Array.from({length:24},(_,target)=>({mode:'term',target})),
    {mode:'custom',label:'宝宝生日',date:'20280229'},
  ]) {
    const settings={...structuredClone(DEFAULT_SETTINGS),festivalCountdown:value};
    assert.equal(validateSettings(settings),null,JSON.stringify(value));
    const restored=importConfig(JSON.stringify(exportConfig({revision:1,settings,todos:[]})));
    assert.deepEqual(restored.settings.festivalCountdown,value);
    assert.equal(restored.settings.countdownLabel,DEFAULT_SETTINGS.countdownLabel);
    assert.equal(restored.settings.countdownDate,DEFAULT_SETTINGS.countdownDate);
  }
  const legacy=structuredClone(DEFAULT_SETTINGS);delete legacy.festivalCountdown;
  assert.equal(validateSettings(legacy),null);
});

test('right-corner countdown refuses unknown targets, impossible dates and unsupported or long names', () => {
  for(const value of [null,[],{mode:'bad'},{mode:'term',target:24},{mode:'term',target:'0'},
    {mode:'festival',target:'bad'}, {mode:'custom',label:'五个汉字名',date:'20270101'},
    {mode:'custom',label:'　 ',date:'20270101'},{mode:'custom',label:'😀',date:'20270101'},
    {mode:'custom',label:'生日',date:'20270229'}, {mode:'custom',label:'生日',date:'21000101'},
    {mode:'auto-festival',extra:true}, {mode:'festival'}, {mode:'custom',label:'生日'}]) {
    assert.ok(validateSettings({...DEFAULT_SETTINGS,festivalCountdown:value}),JSON.stringify(value));
  }
});
