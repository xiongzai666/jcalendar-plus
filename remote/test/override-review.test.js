import test from 'node:test';
import assert from 'node:assert/strict';
import { getOverrideReview } from '../public/override-review.js';

const weekly = Array.from({ length: 6 }, (_, day) =>
  Array.from({ length: 12 }, (_, slot) => `周${day + 1}科目${slot + 1}`));
weekly[5].fill('放假', 6);

test('independent review includes supplied times and never says inherited courses', () => {
  const day = { date: '2026-10-04', dayOff: false, mode: 'custom', lessons: [
    {slot:1,subject:'自习',start:'08:00',end:'09:30'},
    {slot:5,subject:'自习',start:'14:00',end:'15:30'}] };
  const result = getOverrideReview(day,weekly);
  assert.equal(result.status,'独立作息 · 共 2 段课程');
  assert.deepEqual(result.details,['上午第1节：08:00–09:30 自习','下午第1节：14:00–15:30 自习']);
  assert.match(result.note,/不继承/);
  day.lessons[1].start='09:15';
  assert.match(getOverrideReview(day,weekly).status,/待修正/);
});

test('weekday changes show the original and replacement subjects by period', () => {
  const review = getOverrideReview({ date: '2026-09-28', dayOff: false,
    lessons: [{ slot: 2, subject: '英语' }, { slot: 9, subject: '物理' }] }, weekly);
  assert.equal(review.date, '2026-09-28 · 周一');
  assert.deepEqual(review.details, [
    '上午第2节：周1科目2 → 英语',
    '晚上第1节：周1科目9 → 物理',
  ]);
});

test('holiday lists only lessons normally shown on Saturday', () => {
  const review = getOverrideReview({ date: '2026-09-26', dayOff: true, lessons: [] }, weekly);
  assert.equal(review.status, '全天放假 · 当天课程不显示');
  assert.equal(review.details.length, 2);
  assert.match(review.details[1], /^下午：原定 周6科目5、周6科目6，均停课$/);
});

test('a Saturday seventh lesson does not enable other holiday slots', () => {
  const review = getOverrideReview({ date: '2026-09-26', dayOff: false,
    lessons: [{ slot: 7, subject: '自习' }] }, weekly);
  assert.ok(review.details.includes('下午第3节：原无课 → 自习'));
  assert.equal(review.details.length, 1);
  assert.equal(review.note, '');
});

test('no-course replacements are presented as cancellations', () => {
  const review = getOverrideReview({ date: '2026-09-28', dayOff: false,
    lessons: [{ slot: 4, subject: '无课' }] }, weekly);
  assert.deepEqual(review.details, ['上午第4节：周1科目4 → 无课']);
});
test('Sunday no-course overrides remain a rest day in the review', () => {
  const review = getOverrideReview({ date: '2026-09-27', dayOff: false,
    lessons: [{ slot: 1, subject: '无课' }] }, weekly);
  assert.equal(review.status, '周日休息');
});

test('Sunday without a course remains a rest day; with one course it shows that course', () => {
  const rest = getOverrideReview({ date: '2026-09-27', dayOff: false, lessons: [] }, weekly);
  assert.equal(rest.status, '周日休息');
  const classDay = getOverrideReview({ date: '2026-09-27', dayOff: false,
    lessons: [{ slot: 1, subject: '语文' }] }, weekly);
  assert.equal(classDay.status, '周日临时上课');
  assert.deepEqual(classDay.details, ['上午第1节：原无课 → 语文']);
});
