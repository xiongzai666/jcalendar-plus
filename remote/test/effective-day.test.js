import test from 'node:test';
import assert from 'node:assert/strict';
import { effectiveSubjects, previewDay, applyWeekday } from '../public/effective-day.js';
import { DEFAULT_SETTINGS } from '../cloud-functions/lib/defaults.js';
// This explicit synthetic fixture tests populated schedules, independent of setup templates.
const settings = structuredClone(DEFAULT_SETTINGS);
settings.studySchedule = '444;' + ['一','二','三','四','五','六'].map((day,index) => day+','+Array.from({length:12},(_,slot)=>index===5&&slot>=6?'放假':'示例课').join(',')+';').join('');
settings.dailyRoutine = "06:40-06:55,晨会/跑操;06:55-07:35,早读;07:35-07:55,课间;07:55-08:40,第1节;08:40-08:50,课间;08:50-09:35,第2节;09:35-09:50,课间;09:50-10:35,第3节;10:35-10:45,课间;10:45-11:25,第4节;11:25-12:00,中午放学;12:00-12:40,小练;12:40-13:30,午休;13:30-13:40,课间;13:40-14:25,第5节;14:25-14:35,课间;14:35-15:20,第6节;15:20-15:35,课间;15:35-16:20,第7节;16:20-16:30,课间;16:30-17:15,第8节;17:15-17:50,下午放学;17:50-18:20,晚读;18:20-19:10,晚自习1;19:10-19:20,课间;19:20-20:10,晚自习2;20:10-20:20,课间;20:20-21:10,晚自习3;21:10-21:20,课间;21:20-22:00,晚自习4;";
const selfStudy = {date:'2026-10-04',dayOff:false,mode:'custom',lessons:[
  {slot:1,subject:'自习',start:'08:00',end:'09:30'},
  {slot:2,subject:'自习',start:'09:45',end:'11:30'},
  {slot:5,subject:'自习',start:'14:00',end:'15:30'},
  {slot:6,subject:'自习',start:'15:45',end:'17:00'},
  {slot:9,subject:'自习',start:'18:10',end:'20:10'},
  {slot:10,subject:'自习',start:'20:20',end:'22:00'}]};
test('independent Sunday uses six supplied periods, gaps and actual dismissal',()=>{
  const config={...settings,dateOverrides:[selfStudy]};
  const morning=previewDay(config,selfStudy.date,'09:00');
  assert.equal(morning.active,'自习');
  assert.deepEqual(morning.next,{label:'课间',time:'09:30'});
  assert.equal(morning.courses.length,2);
  assert.deepEqual(morning.courses.map(x=>[x.start,x.end]),[['08:00','09:30'],['09:45','11:30']]);
  assert.deepEqual(previewDay(config,selfStudy.date,'11:20').next,{label:'午休',time:'11:30'});
  assert.deepEqual(previewDay(config,selfStudy.date,'13:00').next,{label:'自习',time:'14:00'});
  assert.deepEqual(previewDay(config,selfStudy.date,'21:30').next,{label:'放学',time:'22:00'});
  assert.equal(previewDay(config,selfStudy.date,'22:00').rest,true);
});
test('independent weekday never inherits unlisted courses or reading activities',()=>{
  const day={...selfStudy,date:'2026-10-05'};
  const config={...settings,dateOverrides:[day]};
  assert.equal(effectiveSubjects(config,day.date).filter(Boolean).length,6);
  assert.deepEqual(previewDay(config,day.date,'06:00').next,{label:'自习',time:'08:00'});
});
test('partial Saturday only has its seventh lesson added and finishes at 16:20', () => {
  const day = { date: '2026-09-26', dayOff: false, lessons: [{ slot: 7, subject: '英语' }] };
  const view = previewDay({ ...settings, dateOverrides: [day] }, day.date, '16:00');
  assert.equal(view.active, '英语');
  assert.equal(view.next.label, '放学');
  assert.equal(view.next.time, '16:20');
  assert.equal(view.courses.filter(x => x.effective).length, 3);
  assert.equal(previewDay({ ...settings, dateOverrides: [day] }, day.date, '16:20').rest, true);
});
test('Sunday single lesson has no implicit routine or extra lessons', () => {
  const config = { ...settings, dateOverrides: [{ date: '2026-09-27', dayOff: false,
    lessons: [{ slot: 2, subject: '数学' }] }] };
  const view = previewDay(config, '2026-09-27', '08:49');
  assert.deepEqual(view.next, { label: '数学', time: '08:50' });
  assert.equal(previewDay(config, '2026-09-27', '09:35').rest, true);
});
test('lunch routine is still shown before afternoon lessons', () => {
  const view = previewDay(settings, '2026-09-28', '12:01');
  assert.deepEqual(view.next, { label: '午休', time: '12:40' });
});
test('weekday application snapshots all twelve slots including no-course slots', () => {
  const result = applyWeekday(settings, '2026-10-04', 6);
  assert.equal(result.dayOff, false);
  assert.equal(result.lessons.length, 12);
  assert.equal(result.lessons[6].subject, '放假');
  assert.equal(effectiveSubjects(settings, result.date, result).filter(Boolean).length, 6);
  assert.equal(settings.dateOverrides.length, 0);
});
test('cancellation removes a final class and advances dismissal', () => {
  const config = { ...settings, dateOverrides: [{ date: '2026-09-28', dayOff: false,
    lessons: [{ slot: 4, subject: '无课' }] }] };
  assert.deepEqual(previewDay(config, '2026-09-28', '10:15').next,
    { label: '中午放学', time: '10:35' });
});
