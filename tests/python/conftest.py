"""Shared data and checks for the Python tests.

The fixture (tests/data/optima.json) holds Natural Earth polygons with their exact
convex skulls from the thesis. Polygons in its "slow" tier only run with -m slow
(or -m "" for everything).
"""
import json
import math
import os
from fractions import Fraction

import pytest

import potato_peeling as pp

_DATA = os.path.join(os.path.dirname(__file__), "..", "data", "optima.json")
with open(_DATA) as _f:
    FIXTURE = json.load(_f)["polygons"]


def fixture_params(entries=None):
    """pytest params for fixture entries, the slow tier marked slow."""
    entries = FIXTURE if entries is None else entries
    return [pytest.param(e, id=f"{e['pid']}-{e['name']}",
                         marks=[pytest.mark.slow] if e["tier"] == "slow" else [])
            for e in entries]


def polygon(entry):
    return [tuple(c) for c in entry["coords"]]


def frac(s):
    return Fraction(s)


def frac_points(vs):
    return [(Fraction(x), Fraction(y)) for x, y in vs]


def shoelace(vs):
    vs = [(Fraction(x), Fraction(y)) for x, y in vs]
    return sum(vs[i - 1][0] * vs[i][1] - vs[i][0] * vs[i - 1][1] for i in range(len(vs))) / 2


def cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def canon(vs):
    """CCW, without repeated or collinear vertices, starting at the smallest vertex."""
    out = []
    for p in frac_points(vs):
        if not out or out[-1] != p:
            out.append(p)
    while len(out) > 1 and out[0] == out[-1]:
        out.pop()
    changed = True
    while changed and len(out) > 3:
        changed = False
        for i in range(len(out)):
            if cross(out[i - 1], out[i], out[(i + 1) % len(out)]) == 0:
                del out[i]
                changed = True
                break
    if shoelace(out) < 0:
        out.reverse()
    k = out.index(min(out))
    return out[k:] + out[:k]


def area(vs):
    return abs(shoelace(vs))


def assert_certified(p, skull):
    """The Skull contract: convex, inside P, CCW, no repeated or collinear
    vertices, exact area equal to its vertices' shoelace area."""
    v = skull.vertices
    assert len(v) >= 3
    assert all(isinstance(c, Fraction) for xy in v for c in xy)
    assert isinstance(skull.area, Fraction)
    assert all(cross(v[i - 2], v[i - 1], v[i]) > 0 for i in range(len(v))), "not strictly convex CCW"
    assert skull.area == shoelace(v)
    assert pp.is_convex_subset(p, v)


# ── answers computed once per session ───────────────────────────────────────

_cache = {}


def solved(method, p, key):
    """pp.<method>(p), computed once per (method, key) for the whole session."""
    k = (method, key)
    if k not in _cache:
        _cache[k] = getattr(pp, method)(p)
    return _cache[k]


# ── hand-made polygons ──────────────────────────────────────────────────────

L_SHAPE = [(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)]
COMB = [(0, 0), (7, 0), (7, 3), (6, 3), (6, 1), (5, 1), (5, 3), (4, 3), (4, 1), (3, 1),
        (3, 3), (2, 3), (2, 1), (1, 1), (1, 3), (0, 3)]
# symmetric, with several optimal regions; its long chords run along edges of P
CROSS = [(1, 0), (2, 0), (2, 1), (3, 1), (3, 2), (2, 2), (2, 3), (1, 3), (1, 2), (0, 2),
         (0, 1), (1, 1)]
# collinear vertices on several edges, and a reflex corner on a long edge's line
ARROW = [(0, 0), (2, 0), (4, 0), (4, 2), (3, 2), (3, 4), (2, 6), (1, 4), (1, 2), (0, 2), (0, 1)]
SPIKY = [(0, 0), (10, 0), (10, 10), (6, 4), (5, 10), (4, 4), (0, 10)]
THIN = [(0, 0), (100, 0), (100, 1), (51, 1), (50, 40), (49, 1), (0, 1)]
HAND_MADE = {"L": L_SHAPE, "comb": COMB, "arrow": ARROW, "spiky": SPIKY, "thin": THIN}
SQUARE = [(0, 0), (1, 0), (1, 1), (0, 1)]
TRIANGLE = [(0, 0), (3, 0), (1, 2)]
CONVEX = {"square": SQUARE, "triangle": TRIANGLE,
          "hexagon": [(2, 0), (4, 1), (4, 3), (2, 4), (0, 3), (0, 1)]}


def star(n, seed=1):
    """A deterministic irregular star with n vertices; with n = 32 the exact solve
    takes well over 20 s."""
    pts = []
    for k in range(n):
        t = 2 * math.pi * k / n
        r = 10 + (7 if k % 2 else 0) + ((k * 7919 + seed * 104729) % 13) / 4
        pts.append((round(r * math.cos(t), 3), round(r * math.sin(t), 3)))
    return pts


SLOW = star(32)
