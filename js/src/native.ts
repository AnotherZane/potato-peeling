// Loads the compiled addon: the package for this platform (@potato-peeling/<platform>,
// an optional dependency), or a local build from the repository (cmake-js).

import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);

export type Raw = [string, string];
export type RawSkull = { vertices: [Raw, Raw][]; area: Raw };
export type Coordinate = number | string;
export type RawPoint = [Coordinate, Coordinate];
export type Flag = object;

export interface Native {
  newFlag(): Flag;
  setFlag(flag: Flag): void;
  convexSkull(p: RawPoint[], seed: bigint, threads: number, flag?: Flag): Promise<RawSkull>;
  exactConvexSkull(p: RawPoint[], flag?: Flag): Promise<RawSkull>;
  meshConvexSkull(p: RawPoint[], points: RawPoint[] | null, triangles: number[][] | null, flag?: Flag): Promise<RawSkull>;
  approximateConvexSkull(
    p: RawPoint[], epsilon: number, delta: number, seed: bigint, sampleScale: number,
    sScale: number, maxEdges: number | null, threads: number, lowerBound: string, flag?: Flag,
  ): Promise<RawSkull>;
  refine(
    p: RawPoint[], start: RawPoint[], maxSweeps: number, rotSteps: number, rotSpan: number,
    splitSteps: number, adjoinRounds: number, threads: number, flag?: Flag,
  ): Promise<RawSkull>;
  vertexConvexSkull(p: RawPoint[], flag?: Flag): Promise<RawSkull>;
  upperBound(
    p: RawPoint[], relTol: number, pairs: number, widthDirs: number, edgeSamples: number,
    maxEvals1d: number, maxEvals2d: number, flag?: Flag,
  ): Promise<Raw>;
  isConvexSubset(p: RawPoint[], q: RawPoint[]): boolean;
  segmentInPolygon(p: RawPoint[], a: RawPoint, b: RawPoint): boolean;
  triangulate(p: RawPoint[]): [number, number, number][];
  reflexVertices(p: RawPoint[]): boolean[];
  longestSegment(p: RawPoint[]): [[Raw, Raw], [Raw, Raw]];
}

function load(): Native {
  const platformPackage = `@potato-peeling/${process.platform}-${process.arch}`;
  const tried: string[] = [];
  for (const id of [platformPackage, '../../build/node/Release/potato_peeling.node']) {
    try {
      return require(id) as Native;
    } catch (e) {
      tried.push(`${id}: ${(e as Error).message.split('\n')[0]}`);
    }
  }
  throw new Error(
    `potato-peeling: no native addon for ${process.platform}-${process.arch}.\n  ${tried.join('\n  ')}`,
  );
}

export const native: Native = load();
