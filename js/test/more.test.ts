import assert from 'node:assert/strict';
import { test } from 'node:test';

import {
  approximateConvexSkull, convexSkull, exactConvexSkull, Fraction, InvalidPolygonError,
  isConvexSubset, longestSegment, meshConvexSkull, reflexVertices, refine, segmentInPolygon,
  triangulate, upperBound, vertexConvexSkull,
} from '../dist/index.js';
import { best, frac, L, small, star } from './helpers.js';

test('meshConvexSkull and vertexConvexSkull reproduce the fixture', async () => {
  for (const e of small) {
    assert.ok((await meshConvexSkull(e.coords)).area.equals(frac(e.mesh_area)), `pid ${e.pid}`);
    assert.ok((await vertexConvexSkull(e.coords)).area.equals(frac(e.vertex_area)), `pid ${e.pid}`);
  }
});

test('meshConvexSkull with a caller mesh', async () => {
  const e = small[0]!;
  const mesh = { points: e.coords, triangles: triangulate(e.coords) };
  assert.ok((await meshConvexSkull(e.coords, mesh)).area.equals(frac(e.mesh_area)));
  const gap = { points: e.coords, triangles: mesh.triangles.slice(1) };
  await assert.rejects(meshConvexSkull(e.coords, gap), InvalidPolygonError);
});

test('convexSkull is approximateConvexSkull refined with adjoin moves', async () => {
  for (const e of small.slice(0, 4)) {
    const a = await approximateConvexSkull(e.coords, { seed: 3 });
    const r = await refine(e.coords, a, { adjoinRounds: 8 });
    const c = await convexSkull(e.coords, { seed: 3 });
    assert.ok(r.area.equals(c.area), `pid ${e.pid}`);
    assert.ok(r.area.compare(a.area) >= 0);
    assert.equal(r.vertices.length, c.vertices.length);
  }
});

test('upperBound is at least the optimum', async () => {
  for (const e of small) assert.ok((await upperBound(e.coords)).compare(best(e)) >= 0, `pid ${e.pid}`);
});

test('refine stopped returns its answer so far', async () => {
  const p = star(120);
  const t = performance.now();
  const r = await refine(p, [[0, 0], [1, 0], [0, 1]], { maxSweeps: 10000, adjoinRounds: 50, timeout: 200 });
  assert.ok(performance.now() - t < 1200);
  assert.ok(isConvexSubset(p, r));
  assert.ok(r.area.compare(new Fraction(1, 2)) >= 0);
});

test('utilities', () => {
  assert.deepEqual(reflexVertices(L), [false, false, false, true, false, false]);
  assert.equal(triangulate(L).length, 4);
  assert.ok(isConvexSubset(L, [[0, 0], [1, 0], [1, 1], [0, 1]]));
  assert.ok(!isConvexSubset(L, [[0, 0], [2, 0], [2, 2], [0, 2]]));
  assert.ok(segmentInPolygon(L, [2, 0], [0, 2])); // through the reflex corner
  assert.ok(!segmentInPolygon(L, [0, 0], [2, 2]));
  const [[ax, ay], [bx, by]] = longestSegment(L);
  const dx = ax.toNumber() - bx.toNumber();
  const dy = ay.toNumber() - by.toNumber();
  assert.equal(dx * dx + dy * dy, 8);
  assert.throws(() => reflexVertices([[0, 0], [1, 1]]), InvalidPolygonError);
});

test('bad options', async () => {
  await assert.rejects(approximateConvexSkull(L, { epsilon: 2 }), RangeError);
  // @ts-expect-error: not a lower bound
  await assert.rejects(approximateConvexSkull(L, { lowerBound: 'best' }), RangeError);
  await assert.rejects(approximateConvexSkull(L, { maxEdges: 0 }), RangeError);
  // @ts-expect-error: epsilon is a number
  await assert.rejects(approximateConvexSkull(L, { epsilon: 'small' }), TypeError);
  await assert.rejects(upperBound(L, { relTol: -1 }), RangeError);
  await assert.rejects(refine(L, [[0, 0], [2, 0], [2, 2], [0, 2]]), InvalidPolygonError);
  await assert.rejects(refine(L, L, { maxSweeps: -1 }), RangeError);
  await assert.rejects(meshConvexSkull(L, { points: L, triangles: [[0, 1]] }), TypeError);
  assert.equal((await exactConvexSkull(L)).area.toString(), '2');
});
