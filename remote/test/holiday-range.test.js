import test from 'node:test';
import assert from 'node:assert/strict';
import { planHolidayRange } from '../public/holiday-range.js';

test('holiday range crosses month end and includes both endpoints', () => {
  const plan = planHolidayRange('2026-09-29', '2026-10-02', []);
  assert.deepEqual(plan.additions.map(day => day.date),
    ['2026-09-29', '2026-09-30', '2026-10-01', '2026-10-02']);
  assert.ok(plan.additions.every(day => day.dayOff && day.lessons.length === 0));
});

test('existing class changes are reported and never overwritten', () => {
  const plan = planHolidayRange('2026-10-01', '2026-10-03', [
    { date: '2026-10-01', dayOff: false, lessons: [{ slot: 1, subject: '英语' }] },
    { date: '2026-10-02', dayOff: true, lessons: [] },
  ]);
  assert.deepEqual(plan.conflicts, ['2026-10-01']);
  assert.equal(plan.alreadyOff, 1);
  assert.deepEqual(plan.additions.map(day => day.date), ['2026-10-03']);
});

test('invalid, reversed and over-limit ranges fail without additions', () => {
  assert.throws(() => planHolidayRange('2026-02-30', '2026-03-02', []));
  assert.throws(() => planHolidayRange('2026-10-02', '2026-10-01', []));
  assert.throws(() => planHolidayRange('2026-10-01', '2026-10-20', []));
  assert.throws(() => planHolidayRange('2026-10-01', '2026-10-03',
    Array.from({ length: 13 }, (_, i) => ({ date: `2026-09-${String(i + 1).padStart(2, '0')}` }))));
});
