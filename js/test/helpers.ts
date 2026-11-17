import { readFileSync } from 'node:fs';

import { Fraction } from '../dist/index.js';

/** A polygon of tests/data/optima.json, with the thesis's exact answers. */
export interface Entry {
  pid: number;
  n: number;
  tier: 'default' | 'slow';
  coords: [number, number][];
  exact: { area: string };
  best_known?: { area: string };
  mesh_area: string;
  vertex_area: string;
}

export const fixture: Entry[] = JSON.parse(
  readFileSync(new URL('../../tests/data/optima.json', import.meta.url), 'utf8'),
).polygons;

/** The default-tier polygons with at most 10 vertices. */
export const small = fixture.filter((e) => e.tier === 'default' && e.n <= 10);

export const L = [[0, 0], [2, 0], [2, 1], [1, 1], [1, 2], [0, 2]];

/** "num/den" (or "num") as a Fraction. */
export function frac(s: string): Fraction {
  const [n, d = '1'] = s.split('/');
  return new Fraction(BigInt(n!), BigInt(d));
}

/** The best area known for an entry. */
export function best(e: Entry): Fraction {
  return frac((e.best_known ?? e.exact).area);
}

/** A star with n vertices: every method takes seconds on 300. */
export function star(n: number): [number, number][] {
  let s = 1;
  const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
  return Array.from({ length: n }, (_, i) => {
    const r = 30 + 70 * rnd();
    return [r * Math.cos((2 * Math.PI * i) / n), r * Math.sin((2 * Math.PI * i) / n)];
  });
}
