import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  convexSkull, exactConvexSkull, Fraction, InvalidPolygonError, type PolygonLike, Skull,
} from '../dist/index.js';
import { best, frac, L, small } from './helpers.js';

test('exactConvexSkull reproduces the thesis optima', async () => {
  for (const e of small) {
    const s = await exactConvexSkull(e.coords);
    assert.ok(s.area.equals(frac(e.exact.area)), `pid ${e.pid}`);
  }
});

test('convexSkull never beats the optimum', async () => {
  for (const e of small) {
    const s = await convexSkull(e.coords);
    assert.ok(s.area.compare(best(e)) <= 0, `pid ${e.pid}`);
    assert.ok(s.areaNumber > 0.5 * best(e).toNumber(), `pid ${e.pid}`);
  }
});

test('a Skull: exact and rounded views, GeoJSON', async () => {
  const s = await exactConvexSkull(L);
  assert.ok(s instanceof Skull);
  assert.equal(s.area.toString(), '2');
  assert.equal(s.areaNumber, 2);
  assert.equal(s.vertices.length, 4);
  assert.deepEqual([...s.coords].sort(), [[0, 0], [0, 1], [2, 0], [2, 1]]);
  const ring = s.toGeoJSON().coordinates[0]!;
  assert.deepEqual(ring[0], ring.at(-1));
});

test('input forms give the same answer', async () => {
  const expected = (await exactConvexSkull(L)).area;
  const closed = [...L, L[0]!];
  const forms: PolygonLike[] = [
    closed,
    [...L].reverse(),
    { type: 'Polygon', coordinates: [closed] },
    { type: 'Feature', geometry: { type: 'Polygon', coordinates: [closed] } },
    L.map(([x, y]) => [BigInt(x!), new Fraction(y!)]),
    await exactConvexSkull(L), // a Skull
  ];
  for (const f of forms) assert.ok((await exactConvexSkull(f)).area.equals(expected));
  // exact rational coordinates: a third of the L
  const third = L.map(([x, y]) => [new Fraction(x!, 3), new Fraction(y!, 3)]);
  assert.equal((await exactConvexSkull(third)).area.toString(), '2/9');
});

test('invalid input', async () => {
  await assert.rejects(exactConvexSkull([[0, 0], [1, 1]]), InvalidPolygonError);
  await assert.rejects(exactConvexSkull([[0, 0], [2, 0], [0, 2], [2, 2]]), InvalidPolygonError);
  const holed: PolygonLike = {
    type: 'Polygon',
    coordinates: [[[0, 0], [4, 0], [0, 4], [0, 0]], [[1, 1], [2, 1], [1, 2], [1, 1]]],
  };
  await assert.rejects(exactConvexSkull(holed), InvalidPolygonError);
  await assert.rejects(exactConvexSkull([[0, NaN], [1, 0], [0, 1]]), InvalidPolygonError);
  await assert.rejects(exactConvexSkull([[0, 0, 0], [1, 0], [0, 1]]), TypeError);
  // @ts-expect-error: a string is not a coordinate
  await assert.rejects(exactConvexSkull([[0, '1'], [1, 0], [0, 1]]), TypeError);
  // @ts-expect-error: not a polygon
  await assert.rejects(exactConvexSkull('square'), TypeError);
  await assert.rejects(convexSkull(L, { threads: -1 }), RangeError);
  await assert.rejects(convexSkull(L, { seed: 1.5 }), RangeError);
  await assert.rejects(convexSkull(L, { seed: 2n ** 64n }), RangeError);
  await assert.rejects(convexSkull(L, { timeout: -1 }), RangeError);
});

test('solves run concurrently and agree', async () => {
  const p = small[0]!.coords;
  const one = await convexSkull(p, { threads: 1 });
  const all = await Promise.all(Array.from({ length: 8 }, () => convexSkull(p, { threads: 1 })));
  for (const s of all) assert.ok(s.area.equals(one.area));
  assert.ok((await convexSkull(p, { threads: 0 })).area.equals(one.area));
});
