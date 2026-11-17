import assert from 'node:assert/strict';
import { test } from 'node:test';

import { convexSkull, exactConvexSkull } from '../dist/index.js';
import { L, star } from './helpers.js';

const LATE = 1000; // ms a stop may take to act (the library checks every ~20 ms)
const STAR = star(300);

test('abort rejects with the signal reason', async () => {
  const c = new AbortController();
  const t = performance.now();
  setTimeout(() => c.abort(), 200);
  await assert.rejects(exactConvexSkull(STAR, { signal: c.signal }), { name: 'AbortError' });
  assert.ok(performance.now() - t < 200 + LATE);
  const reason = new Error('stop');
  const c2 = new AbortController();
  setTimeout(() => c2.abort(reason), 50);
  await assert.rejects(exactConvexSkull(STAR, { signal: c2.signal }), (e) => e === reason);
});

test('an aborted signal rejects at once', async () => {
  await assert.rejects(exactConvexSkull(STAR, { signal: AbortSignal.abort() }), { name: 'AbortError' });
});

test('timeout rejects with TimeoutError', async () => {
  const t = performance.now();
  await assert.rejects(exactConvexSkull(STAR, { timeout: 200 }), { name: 'TimeoutError' });
  assert.ok(performance.now() - t < 200 + LATE);
});

test('AbortSignal.timeout works too', async () => {
  await assert.rejects(exactConvexSkull(STAR, { signal: AbortSignal.timeout(100) }), { name: 'TimeoutError' });
});

test('convexSkull stopped returns or rejects quickly', async () => {
  for (const timeout of [1, 500]) {
    const t = performance.now();
    try {
      await convexSkull(STAR, { timeout });
    } catch (e) {
      assert.equal((e as Error).name, 'TimeoutError');
    }
    assert.ok(performance.now() - t < timeout + LATE);
  }
});

test('a generous timeout changes nothing', async () => {
  const s = await exactConvexSkull(L, { timeout: 60000, signal: new AbortController().signal });
  assert.equal(s.area.toString(), '2');
});
