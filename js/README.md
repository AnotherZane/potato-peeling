# potato-peeling

Algorithms for the [potato-peeling](https://en.wikipedia.org/wiki/Potato_peeling) problem: find the largest-area convex polygon contained in a simple polygon (its *convex skull*). Exact results, as BigInt fractions.

The Node.js package of [potato-peeling](https://github.com/AnotherZane/potato-peeling), a C++ library also available for [Python](https://pypi.org/project/potato-peeling/). The methods, their algorithms and guarantees are described in the [main README](https://github.com/AnotherZane/potato-peeling#methods).

> [!WARNING]
> Alpha release: the API may still change before 0.1.

## Install

```
npm install potato-peeling
```

Node.js 20 or later, on Linux (x64, arm64; glibc and musl, e.g. Alpine), macOS (arm64, x64) and Windows (x64, arm64). On musl the library runs several times slower (about 10x in our tests), which we attribute to musl's memory allocator: exact arithmetic allocates heavily. The native addon for your platform comes as an optional dependency (`@potato-peeling/<platform>-<arch>`); nothing is compiled on install.

## Quick start

```js
import * as pp from 'potato-peeling';

const p = [[0, 0], [6, 1], [8, 5], [4, 4], [2, 8]];

const skull = await pp.convexSkull(p);      // fast, certified, close to optimal
skull.areaNumber;                           // 21.6868...
skull.coords;                               // [[0, 0], [6, 1], [7.908..., 4.816...], ...]

const best = await pp.exactConvexSkull(p);  // the optimum
best.area.toString();                       // '109/5', i.e. 21.8
best.vertices;                              // (8, 5), (4/5, 16/5), (0, 0), (6, 1) as Fractions

(await pp.upperBound(p)).toNumber();        // 21.8255...: no convex polygon in P is larger
```

## Methods

| Function | Answer |
|---|---|
| `convexSkull(p, {seed, threads})` | the general-purpose method: certified, typically within 0.5% of optimal |
| `exactConvexSkull(p)` | the exact optimum; practical up to a few dozen vertices |
| `meshConvexSkull(p, mesh?)` | the optimum among unions of triangles of P's triangulation, or of `mesh` (`{points, triangles}`) |
| `approximateConvexSkull(p, {epsilon, delta, seed, sampleScale, sScale, maxEdges, threads, lowerBound})` | randomised approximation (Cabello et al.), deterministic for a given `seed` |
| `refine(p, start, {maxSweeps, rotSteps, rotSpan, splitSteps, adjoinRounds, threads})` | improves `start`, a convex polygon inside P; never returns less |
| `vertexConvexSkull(p)` | the largest convex polygon whose vertices are vertices of P |
| `upperBound(p, {relTol, pairs, widthDirs, edgeSamples, maxEvals1d, maxEvals2d})` | a certified upper bound on the optimum's area, as a `Fraction` |

These return Promises; the solve runs on a thread of its own, so the event loop stays free, and several solves can run at once. Utilities, synchronous: `isConvexSubset(p, q)`, `segmentInPolygon(p, a, b)`, `triangulate(p)`, `reflexVertices(p)`, `longestSegment(p)`.

Options and their defaults are those of the Python package, in camelCase. `threads: 0`, the default, uses every core; the answer does not depend on it. Pass `threads: 1` when running many solves at once.

## Inputs and results

A polygon is an array of `[x, y]` points, a GeoJSON Polygon or Feature, or a `Skull`; either orientation, without holes. Coordinates may be numbers (read exactly: `0.1` is the double nearest 0.1), bigints or `Fraction`s.

A `Skull` has `vertices` and `area` as `Fraction`s (counter-clockwise, without collinear vertices), `coords` and `areaNumber` rounded to the nearest doubles, and `toGeoJSON()`. A `Fraction` holds `num` and `den` as BigInts in lowest terms, with `toNumber()`, `toString()` (`'num/den'`), `compare()` and `equals()`; it has no arithmetic, so build a fraction library's value from `num` and `den` if you need one.

## Stopping and errors

Every method takes `signal`, an `AbortSignal`, and `timeout` in milliseconds. Aborted, a solve rejects with the signal's reason (an `AbortError` by default); timed out, with a `TimeoutError`. `refine`, and `convexSkull` while refining, resolve with the best answer so far instead. A solve stops within about 0.1 s.

```js
const controller = new AbortController();
const best = pp.exactConvexSkull(p, { signal: controller.signal, timeout: 60_000 });
controller.abort();  // e.g. from a cancel button
```

All errors reject the Promise (utilities throw): `InvalidPolygonError` for input that is not a simple polygon, `CertificationError` if a result fails its own check (a bug), `TypeError` or `RangeError` for bad options.

## Licence

LGPL-3.0-or-later; see [`COPYING.LESSER`](https://github.com/AnotherZane/potato-peeling/blob/main/COPYING.LESSER). The addon links GMP and MPFR (LGPL-3.0-or-later) as separate shared libraries; third-party notices are in [`THIRD-PARTY-NOTICES.txt`](https://github.com/AnotherZane/potato-peeling/blob/main/THIRD-PARTY-NOTICES.txt). Built on [CGAL](https://www.cgal.org); if you use this library in published work, please also cite CGAL ([how to cite](https://doc.cgal.org/latest/Manual/how_to_cite_cgal.html)).
