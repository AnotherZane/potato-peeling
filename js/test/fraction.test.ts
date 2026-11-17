import assert from 'node:assert/strict';
import { test } from 'node:test';

import { Fraction } from '../dist/index.js';

test('lowest terms, positive denominator', () => {
  const f = new Fraction(6n, -4n);
  assert.equal(f.num, -3n);
  assert.equal(f.den, 2n);
  assert.equal(f.toString(), '-3/2');
  assert.equal(new Fraction(0, 5).toString(), '0');
  assert.equal(new Fraction(7).toString(), '7');
  assert.throws(() => new Fraction(1, 0), RangeError);
  assert.throws(() => new Fraction(0.5), RangeError);
});

test('compare and equals', () => {
  assert.equal(new Fraction(1, 3).compare(new Fraction(2, 6)), 0);
  assert.equal(new Fraction(-1, 2).compare(new Fraction(1, 3)), -1);
  assert.equal(new Fraction(2, 3).compare(new Fraction(1, 2)), 1);
  assert.ok(new Fraction(2, 4).equals(new Fraction(1, 2)));
});

test('toNumber rounds to nearest, ties to even', () => {
  assert.equal(new Fraction(1, 3).toNumber(), 1 / 3);
  assert.equal(new Fraction(-2, 3).toNumber(), -2 / 3);
  const two53 = 2n ** 53n;
  assert.equal(new Fraction(two53 + 1n).toNumber(), 2 ** 53); // tie: down to even
  assert.equal(new Fraction(two53 + 3n).toNumber(), 2 ** 53 + 4); // tie: up to even
  assert.equal(new Fraction(1n, 2n ** 1074n).toNumber(), 2 ** -1074); // smallest subnormal
  assert.equal(new Fraction(1n, 2n ** 1075n).toNumber(), 0); // tie with 0: to even
  assert.equal(new Fraction(3n, 2n ** 1076n).toNumber(), 2 ** -1074);
  assert.equal(new Fraction(2n ** 1024n).toNumber(), Infinity);
  assert.equal(new Fraction(10n ** 400n + 1n, 10n ** 399n).toNumber(), 10);
});

test('toNumber is exact on doubles', () => {
  let s = 12345;
  const rnd = () => (s = (s * 16807) % 2147483647) / 2147483647;
  for (let i = 0; i < 2000; i++) {
    const x = (rnd() - 0.5) * 10 ** Math.floor(rnd() * 600 - 300);
    const [m, e] = decompose(x);
    const f = e >= 0 ? new Fraction(m << BigInt(e)) : new Fraction(m, 1n << BigInt(-e));
    assert.equal(f.toNumber(), x);
  }
});

/** x as m * 2^e with m an integer. */
function decompose(x: number): [bigint, number] {
  let e = 0;
  while (!Number.isInteger(x)) {
    x *= 2;
    e -= 1;
  }
  return [BigInt(x), e];
}
