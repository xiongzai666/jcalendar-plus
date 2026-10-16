import test from 'node:test';
import assert from 'node:assert/strict';
import { validateSettings } from '../cloud-functions/lib/state.js';
import { DEFAULT_SETTINGS } from '../cloud-functions/lib/defaults.js';

test('routine errors identify invalid times, overlapping rows and lesson order', () => {
  const check = dailyRoutine => validateSettings({ ...DEFAULT_SETTINGS, dailyRoutine });
  for (const [replacement, message] of [
    ['08:00-08:00,第1节;', /第 1 项.*结束时间.*开始时间/],
    ['25:00-08:45,第1节;', /第 1 项.*时间/],
    ['08:00-09:00,第1节;', /第 2 项.*第 1 项.*重叠/],
    ['10:00-10:15,第1节;', /第 2 项.*按时间整理/],
    ['08:00-08:45,;', /第 1 项.*名称/],
  ]) assert.match(check(DEFAULT_SETTINGS.dailyRoutine.replace('08:00-08:45,第1节;', replacement)), message);
  assert.match(check(DEFAULT_SETTINGS.dailyRoutine.replace('第1节;', '第2节;')), /第 1 项.*第1节/);
  assert.match(check(DEFAULT_SETTINGS.dailyRoutine.replace('08:00-08:45,第1节;', '')), /第1节/);
  assert.equal(check(DEFAULT_SETTINGS.dailyRoutine), null);
});
