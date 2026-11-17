/**
 * potato-peeling: the largest convex polygon inside a simple polygon.
 *
 * Results are exact (Fractions of BigInts), with float views; every answer is
 * certified convex and contained in the input. Solves run off the JavaScript thread
 * and return Promises; every error, bad input or options included, rejects them.
 */

import { CertificationError, InvalidPolygonError } from './errors.js';
import { Fraction } from './fraction.js';
import { native, type Flag, type Raw, type RawPoint, type RawSkull } from './native.js';

export { CertificationError, Fraction, InvalidPolygonError };

/** A coordinate: a number (read exactly, as the double it is), a bigint or a Fraction. */
export type Coordinate = number | bigint | Fraction;
/** A point [x, y]; any array of two coordinates (checked when called). */
export type Point = readonly Coordinate[];

/** A GeoJSON Polygon geometry, or a Feature with one. */
export type GeoJSONPolygon =
  | { type: 'Polygon'; coordinates: readonly (readonly (readonly number[])[])[] }
  | { type: 'Feature'; geometry: GeoJSONPolygon };

/** A polygon: its vertices, a GeoJSON Polygon or Feature, or a Skull. Either
 * orientation; a closing copy of the first vertex and repeated or collinear vertices
 * are removed. Holes and multipolygons are not supported. */
export type PolygonLike = readonly Point[] | GeoJSONPolygon | Skull;

/** A convex polygon inside P: counter-clockwise, without collinear vertices, with
 * exact vertices and area. */
export class Skull {
  constructor(
    readonly vertices: readonly (readonly [Fraction, Fraction])[],
    readonly area: Fraction,
  ) {}

  /** The vertices rounded to the nearest doubles. Rounding can move a vertex outside
   * P or break strict convexity by half an ulp; `vertices` is exact. */
  get coords(): [number, number][] {
    return this.vertices.map(([x, y]) => [x.toNumber(), y.toNumber()]);
  }

  /** The area rounded to the nearest double. */
  get areaNumber(): number {
    return this.area.toNumber();
  }

  /** A GeoJSON Polygon with the rounded coordinates. */
  toGeoJSON(): { type: 'Polygon'; coordinates: [number, number][][] } {
    const ring = this.coords;
    return { type: 'Polygon', coordinates: [[...ring, ring[0]!]] };
  }
}

/** Stopping a solve. `signal` aborts it (the Promise rejects with the signal's
 * reason); after `timeout` milliseconds it rejects with a TimeoutError. refine, and
 * convexSkull once refining, resolve with the answer so far instead. The solve
 * stops within about 0.1 s. */
export interface StopOptions {
  signal?: AbortSignal;
  timeout?: number;
}

export interface ConvexSkullOptions extends StopOptions {
  /** Random seed (non-negative, below 2^64). */
  seed?: number | bigint;
  /** 0 uses every hardware thread; the answer does not depend on it. */
  threads?: number;
}

/** The best general-purpose method: certified, and close to optimal in practice.
 * What it runs may improve between versions (currently approximateConvexSkull, then
 * refine with adjoin moves). */
export async function convexSkull(polygon: PolygonLike, options: ConvexSkullOptions = {}): Promise<Skull> {
  const p = ring(polygon);
  const seed = toSeed(options.seed ?? 0);
  const threads = toThreads(options.threads ?? 0);
  return run(options, (flag) => native.convexSkull(p, seed, threads, flag), skull);
}

/** The exact optimum (Chang & Yap 1986). Practical up to a few dozen vertices. */
export async function exactConvexSkull(polygon: PolygonLike, options: StopOptions = {}): Promise<Skull> {
  const p = ring(polygon);
  return run(options, (flag) => native.exactConvexSkull(p, flag), skull);
}

/** A triangulation of P for meshConvexSkull: `triangles` index `points`. Points other
 * than P's vertices may lie inside P or on its edges. */
export interface Mesh {
  points: readonly Point[];
  /** Index triples [i, j, k] into `points`. */
  triangles: readonly (readonly number[])[];
}

/** The best union of triangles of a triangulation of P (Aronov, van Kreveld, Löffler &
 * Silveira 2011): P's constrained Delaunay triangulation, or `mesh`, which must tile
 * P exactly (InvalidPolygonError otherwise). */
export async function meshConvexSkull(
  polygon: PolygonLike,
  mesh?: Mesh | null,
  options: StopOptions = {},
): Promise<Skull> {
  const p = ring(polygon);
  const pts = mesh ? points(mesh.points) : null;
  const tris = mesh ? mesh.triangles.map(triangle) : null;
  return run(options, (flag) => native.meshConvexSkull(p, pts, tris, flag), skull);
}

export interface ApproximateOptions extends StopOptions {
  epsilon?: number;
  delta?: number;
  seed?: number | bigint;
  sampleScale?: number;
  sScale?: number;
  /** null searches every edge. */
  maxEdges?: number | null;
  /** 0 uses every hardware thread; the answer does not depend on it. */
  threads?: number;
  /** How A(P), which sets the sample sizes, is found: "vertex_triangle" (the largest
   * triangle on P's vertices inside P; the default, better and faster on the whole)
   * or "aronov" (meshConvexSkull's area). */
  lowerBound?: 'vertex_triangle' | 'aronov';
}

/** Randomised approximation (Cabello, Cibulka, Kynčl, Saumell & Valtr 2017). With
 * sampleScale = sScale = 1 and maxEdges null it is the paper's
 * (1 - epsilon)-approximation with failure probability delta; the defaults are far
 * smaller, and the answer is certified but carries no such bound. */
export async function approximateConvexSkull(
  polygon: PolygonLike,
  options: ApproximateOptions = {},
): Promise<Skull> {
  const p = ring(polygon);
  const {
    epsilon = 0.1, delta = 1 / 3, sampleScale = 3e-4, sScale = 3e-2, maxEdges = 64,
    lowerBound = 'vertex_triangle',
  } = options;
  const seed = toSeed(options.seed ?? 0);
  const threads = toThreads(options.threads ?? 0);
  for (const [name, v] of [['epsilon', epsilon], ['delta', delta], ['sampleScale', sampleScale], ['sScale', sScale]] as const)
    number(name, v);
  if (maxEdges !== null) count('maxEdges', maxEdges);
  if (typeof lowerBound !== 'string') throw new TypeError('lowerBound must be a string');
  return run(
    options,
    (flag) => native.approximateConvexSkull(p, epsilon, delta, seed, sampleScale, sScale, maxEdges, threads, lowerBound, flag),
    skull,
  );
}

export interface RefineOptions extends StopOptions {
  maxSweeps?: number;
  rotSteps?: number;
  /** Radians. */
  rotSpan?: number;
  /** 0: no split moves. */
  splitSteps?: number;
  /** 0: no adjoin moves. */
  adjoinRounds?: number;
  /** 0 uses every hardware thread; the answer does not depend on it. */
  threads?: number;
}

/** Local ascent from `start` (a convex polygon inside P, or a Skull); never returns
 * less than start. Stopped by `signal` or `timeout`, it resolves with the answer so
 * far. */
export async function refine(polygon: PolygonLike, start: PolygonLike, options: RefineOptions = {}): Promise<Skull> {
  const p = ring(polygon);
  const q = ring(start);
  const { maxSweeps = 12, rotSteps = 9, rotSpan = 0.3, splitSteps = 0, adjoinRounds = 0 } = options;
  for (const [name, v] of [['maxSweeps', maxSweeps], ['rotSteps', rotSteps], ['splitSteps', splitSteps], ['adjoinRounds', adjoinRounds]] as const)
    count(name, v, 0xffffffff);
  number('rotSpan', rotSpan);
  const threads = toThreads(options.threads ?? 0);
  return run(
    options,
    (flag) => native.refine(p, q, maxSweeps, rotSteps, rotSpan, splitSteps, adjoinRounds, threads, flag),
    skull,
  );
}

/** The largest convex polygon in P whose vertices are vertices of P (Fischer;
 * Bautista-Santiago et al.). Never below meshConvexSkull(polygon). */
export async function vertexConvexSkull(polygon: PolygonLike, options: StopOptions = {}): Promise<Skull> {
  const p = ring(polygon);
  return run(options, (flag) => native.vertexConvexSkull(p, flag), skull);
}

export interface UpperBoundOptions extends StopOptions {
  relTol?: number;
  pairs?: number;
  widthDirs?: number;
  edgeSamples?: number;
  maxEvals1d?: number;
  maxEvals2d?: number;
}

/** A certified upper bound on the area of P's convex skull (pivot, two-pivot and
 * width relaxations, computed exactly). */
export async function upperBound(polygon: PolygonLike, options: UpperBoundOptions = {}): Promise<Fraction> {
  const p = ring(polygon);
  const { relTol = 1e-3, pairs = 6, widthDirs = 180, edgeSamples = 0, maxEvals1d = 200_000, maxEvals2d = 4_000 } = options;
  number('relTol', relTol);
  for (const [name, v] of [['pairs', pairs], ['widthDirs', widthDirs], ['edgeSamples', edgeSamples]] as const)
    count(name, v, 0xffffffff);
  count('maxEvals1d', maxEvals1d);
  count('maxEvals2d', maxEvals2d);
  return run(
    options,
    (flag) => native.upperBound(p, relTol, pairs, widthDirs, edgeSamples, maxEvals1d, maxEvals2d, flag),
    fraction,
  );
}

// ── utilities (synchronous) ─────────────────────────────────────────────────

/** Whether q is a convex polygon contained in P. */
export function isConvexSubset(polygon: PolygonLike, q: PolygonLike): boolean {
  return utility(() => native.isConvexSubset(ring(polygon), ring(q)));
}

/** Whether the segment ab lies in P (its boundary included). */
export function segmentInPolygon(polygon: PolygonLike, a: Point, b: Point): boolean {
  return utility(() => {
    const [pa, pb] = points([a, b]);
    return native.segmentInPolygon(ring(polygon), pa!, pb!);
  });
}

/** A triangulation of P: counter-clockwise index triples into P's vertices. */
export function triangulate(polygon: PolygonLike): [number, number, number][] {
  return utility(() => native.triangulate(ring(polygon)));
}

/** One flag per vertex of P: true at reflex corners. */
export function reflexVertices(polygon: PolygonLike): boolean[] {
  return utility(() => native.reflexVertices(ring(polygon)));
}

/** The longest segment inside P, with exact endpoints. */
export function longestSegment(polygon: PolygonLike): [[Fraction, Fraction], [Fraction, Fraction]] {
  return utility(() => {
    const [a, b] = native.longestSegment(ring(polygon));
    return [[fraction(a[0]), fraction(a[1])], [fraction(b[0]), fraction(b[1])]];
  });
}

// ── inputs ──────────────────────────────────────────────────────────────────

function coordinate(c: Coordinate): number | string {
  if (typeof c === 'number') {
    if (!Number.isFinite(c)) throw new InvalidPolygonError('coordinate is not finite');
    return c;
  }
  if (typeof c === 'bigint') return c.toString();
  if (c instanceof Fraction) return `${c.num}/${c.den}`;
  throw new TypeError('a coordinate must be a number, a bigint or a Fraction');
}

function points(pts: readonly Point[]): RawPoint[] {
  if (!Array.isArray(pts)) throw new TypeError('a polygon must be an array of points');
  return pts.map((pt) => {
    if (!Array.isArray(pt) || pt.length !== 2) throw new TypeError('a point must be [x, y]');
    return [coordinate(pt[0]!), coordinate(pt[1]!)];
  });
}

function ring(polygon: PolygonLike): RawPoint[] {
  if (polygon instanceof Skull) return points(polygon.vertices);
  if (Array.isArray(polygon)) return points(polygon as readonly Point[]);
  const g = polygon as GeoJSONPolygon;
  if (g?.type === 'Feature') return ring(g.geometry);
  if (g?.type === 'Polygon') {
    if (g.coordinates.length !== 1)
      throw new InvalidPolygonError('polygons with holes are not supported');
    return points(g.coordinates[0]!.map((c) => {
      if (c.length < 2) throw new TypeError('a GeoJSON position needs x and y');
      return [c[0]!, c[1]!];
    }));
  }
  throw new TypeError('a polygon must be an array of points, a GeoJSON Polygon or Feature, or a Skull');
}

function toSeed(seed: number | bigint): bigint {
  const s = typeof seed === 'bigint' ? seed : Number.isSafeInteger(seed) ? BigInt(seed) : -1n;
  if (s < 0n || s >= 1n << 64n) throw new RangeError('seed must be an integer in [0, 2^64)');
  return s;
}

function number(name: string, v: unknown): void {
  if (typeof v !== 'number' || Number.isNaN(v)) throw new TypeError(`${name} must be a number`);
}

function count(name: string, v: unknown, max = Number.MAX_SAFE_INTEGER): void {
  if (!Number.isInteger(v) || (v as number) < 0 || (v as number) > max)
    throw new RangeError(`${name} must be a non-negative integer`);
}

function triangle(t: readonly number[]): number[] {
  if (!Array.isArray(t) || t.length !== 3) throw new TypeError('a triangle must be [i, j, k]');
  for (const i of t) count('a triangle index', i);
  return [...t];
}

function toThreads(threads: number): number {
  if (!Number.isInteger(threads) || threads < 0 || threads > 0xffffffff)
    throw new RangeError('threads must be a non-negative integer');
  return threads;
}

// ── results and stopping ────────────────────────────────────────────────────

function fraction([n, d]: Raw): Fraction {
  return new Fraction(BigInt(n), BigInt(d));
}

function skull(raw: RawSkull): Skull {
  return new Skull(
    raw.vertices.map(([x, y]) => [fraction(x), fraction(y)] as const),
    fraction(raw.area),
  );
}

// Starts a solve with a cancel flag that the signal and the timeout set, and maps the
// addon's rejections to errors.
async function run<R, T>(options: StopOptions, start: (flag: Flag) => Promise<R>, result: (raw: R) => T): Promise<T> {
  const { signal, timeout } = options;
  if (timeout !== undefined && !(timeout >= 0)) throw new RangeError('timeout must be >= 0');
  signal?.throwIfAborted();
  const flag = native.newFlag();
  let timedOut = false;
  const abort = () => native.setFlag(flag);
  signal?.addEventListener('abort', abort, { once: true });
  const timer =
    timeout === undefined
      ? undefined
      : setTimeout(() => {
          timedOut = true;
          native.setFlag(flag);
        }, timeout);
  try {
    return result(await start(flag));
  } catch (e) {
    throw translate(e, signal, timedOut);
  } finally {
    clearTimeout(timer);
    signal?.removeEventListener('abort', abort);
  }
}

function utility<T>(f: () => T): T {
  try {
    return f();
  } catch (e) {
    throw translate(e, undefined, false);
  }
}

function translate(e: unknown, signal: AbortSignal | undefined, timedOut: boolean): unknown {
  const err = e as Error & { code?: string };
  switch (err?.code) {
    case 'INVALID_POLYGON':
      return new InvalidPolygonError(err.message);
    case 'CERTIFICATION':
      return new CertificationError(err.message);
    case 'INVALID_ARGUMENT':
      return new RangeError(err.message);
    case 'CANCELLED':
      if (signal?.aborted) return signal.reason;
      if (timedOut) return new DOMException('timed out', 'TimeoutError');
      return err;
    default:
      return e;
  }
}
