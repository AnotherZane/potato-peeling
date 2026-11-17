/** An exact rational number: num / den in lowest terms, den > 0. For reading exact
 * results (and giving exact inputs); it has no arithmetic. */
export class Fraction {
  readonly num: bigint;
  readonly den: bigint;

  constructor(num: bigint | number, den: bigint | number = 1n) {
    let n = toBigInt(num, 'numerator');
    let d = toBigInt(den, 'denominator');
    if (d === 0n) throw new RangeError('denominator is zero');
    if (d < 0n) {
      n = -n;
      d = -d;
    }
    const g = gcd(n < 0n ? -n : n, d);
    this.num = n / g;
    this.den = d / g;
  }

  /** The nearest double (ties to even), as Python's float(Fraction) gives. */
  toNumber(): number {
    return nearestDouble(this.num, this.den);
  }

  /** "num/den", or "num" when den is 1. */
  toString(): string {
    return this.den === 1n ? `${this.num}` : `${this.num}/${this.den}`;
  }

  /** -1, 0 or 1 as this is less than, equal to or greater than other. */
  compare(other: Fraction): number {
    const a = this.num * other.den;
    const b = other.num * this.den;
    return a < b ? -1 : a > b ? 1 : 0;
  }

  equals(other: Fraction): boolean {
    return this.num === other.num && this.den === other.den;
  }
}

function toBigInt(x: bigint | number, what: string): bigint {
  if (typeof x === 'bigint') return x;
  if (!Number.isSafeInteger(x)) throw new RangeError(`${what} must be a safe integer or a bigint`);
  return BigInt(x);
}

function gcd(a: bigint, b: bigint): bigint {
  while (b !== 0n) [a, b] = [b, a % b];
  return a === 0n ? 1n : a;
}

function bitLength(x: bigint): number {
  return x === 0n ? 0 : x.toString(2).length;
}

// The double nearest n/d (d > 0), ties to even, including subnormal and overflowing
// results. q = floor(|n| 2^s / d) has 55 or 56 bits; k low bits are dropped so that
// 53 remain, or fewer where the result is subnormal (unit 2^-1074), and the rest
// (with the division's remainder as a sticky bit) decide the rounding.
function nearestDouble(n: bigint, d: bigint): number {
  if (n === 0n) return 0;
  const negative = n < 0n;
  const a = negative ? -n : n;
  const s = 55 - (bitLength(a) - bitLength(d));
  const scaledA = s >= 0 ? a << BigInt(s) : a;
  const scaledD = s >= 0 ? d : d << BigInt(-s);
  const q = scaledA / scaledD;
  const sticky = scaledA % scaledD !== 0n;
  const k = Math.max(bitLength(q) - 53, s - 1074);
  let m = q >> BigInt(k);
  const dropped = q & ((1n << BigInt(k)) - 1n);
  const half = 1n << BigInt(k - 1);
  if (dropped > half || (dropped === half && (sticky || (m & 1n) === 1n))) m += 1n;
  // m has at most 53 bits, so Number(m) is exact; scale in two steps so that
  // neither factor underflows or overflows on its own.
  const e = k - s;
  const half1 = Math.trunc(e / 2);
  const value = Number(m) * 2 ** half1 * 2 ** (e - half1);
  return negative ? -value : value;
}
