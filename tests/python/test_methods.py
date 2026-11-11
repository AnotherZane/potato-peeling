"""Invariants every method must keep, on the fixture and on hand-made polygons:
certified results, mesh <= vertex <= exact <= upper bound <= area(P), refinement never
loses area, determinism, and independence of how P is written down.

convex_skull is the slowest method here (refinement with adjoin moves), so on the
default tier it runs only on polygons with n <= 8 and the hand-made ones."""
from fractions import Fraction

import pytest

import potato_peeling as pp

from conftest import (CONVEX, CROSS, FIXTURE, HAND_MADE, area, assert_certified, canon,
                      fixture_params, frac, polygon, solved)

# On the default tier, the polygons with n <= 11; the rest run with the slow tier.
CASES = fixture_params(
    [dict(e, tier="slow") if e["tier"] == "default" and e["n"] > 11 else e for e in FIXTURE]
) + [pytest.param(name, id=name) for name in HAND_MADE]
SMALL = [e for e in FIXTURE if e["tier"] == "default" and e["n"] <= 10]
CONVEX_SKULL_CASES = fixture_params(
    [dict(e, tier="slow") if e["tier"] == "default" and e["n"] > 8 else e for e in FIXTURE]
) + [pytest.param(name, id=name) for name in HAND_MADE]


def get(case, method):
    if isinstance(case, dict):
        return solved(method, polygon(case), case["pid"])
    return solved(method, HAND_MADE[case], case)


def p_of(case):
    return polygon(case) if isinstance(case, dict) else HAND_MADE[case]


def best_known(case):
    if isinstance(case, dict) and "best_known" in case:
        return frac(case["best_known"]["area"])
    return None


_refine_cache = {}


def refined(case):
    """refine from approximate_convex_skull's answer, once per polygon."""
    k = case["pid"] if isinstance(case, dict) else case
    if k not in _refine_cache:
        _refine_cache[k] = pp.refine(p_of(case), get(case, "approximate_convex_skull"),
                                     max_sweeps=3)
    return _refine_cache[k]


@pytest.mark.parametrize("case", CASES)
def test_results_are_certified(case):
    p = p_of(case)
    for method in ("exact_convex_skull", "mesh_convex_skull", "vertex_convex_skull",
                   "approximate_convex_skull"):
        assert_certified(p, get(case, method))
    assert_certified(p, refined(case))


@pytest.mark.parametrize("case", CASES)
def test_bounds_are_ordered(case):
    u = get(case, "upper_bound")
    mesh, vertex, exact = (get(case, m).area for m in
                           ("mesh_convex_skull", "vertex_convex_skull", "exact_convex_skull"))
    assert mesh <= vertex <= exact <= u <= area(p_of(case))
    assert get(case, "approximate_convex_skull").area <= u
    assert refined(case).area <= u
    bk = best_known(case)
    if bk is not None:
        assert bk <= u


@pytest.mark.parametrize("case", CASES)
def test_approximations_below_exact(case):
    # Chang & Yap misses the optimum on a few polygons (the fixture's best_known); an
    # approximation beating the exact answer elsewhere would be a new such case.
    if best_known(case) is not None:
        pytest.skip("exact answer known to be suboptimal")
    exact = get(case, "exact_convex_skull").area
    assert get(case, "approximate_convex_skull").area <= exact
    assert refined(case).area <= exact


@pytest.mark.parametrize("case", CASES)
def test_refinement_never_loses(case):
    p = p_of(case)
    assert refined(case).area >= get(case, "approximate_convex_skull").area
    exact = get(case, "exact_convex_skull")
    s = pp.refine(p, exact, max_sweeps=2)
    assert_certified(p, s)
    assert s.area >= exact.area


@pytest.mark.parametrize("case", CONVEX_SKULL_CASES)
def test_convex_skull(case):
    p = p_of(case)
    s = get(case, "convex_skull")
    assert_certified(p, s)
    # it refines approximate_convex_skull's answer (same seed)
    assert s.area >= get(case, "approximate_convex_skull").area
    assert s.area <= get(case, "upper_bound")
    if best_known(case) is None:
        assert s.area <= get(case, "exact_convex_skull").area


@pytest.mark.parametrize("case", fixture_params(SMALL[:3]) + [pytest.param(n, id=n) for n in ("L", "spiky")])
def test_deterministic(case):
    p = p_of(case)
    assert pp.approximate_convex_skull(p, threads=1) == get(case, "approximate_convex_skull")
    assert pp.convex_skull(p, threads=2) == get(case, "convex_skull")
    assert pp.refine(p, get(case, "approximate_convex_skull"), max_sweeps=3) == refined(case)


@pytest.mark.parametrize("entry", fixture_params([e for e in SMALL if e["n"] <= 9]))
@pytest.mark.parametrize("change", ["reversed", "rotated", "translated", "scaled"])
def test_exact_methods_ignore_presentation(entry, change):
    p = polygon(entry)
    if change == "reversed":
        q, f = p[::-1], 1
    elif change == "rotated":
        q, f = p[3:] + p[:3], 1
    elif change == "translated":  # as Fractions, so the shift is exact
        q, f = [(Fraction(x) + Fraction(1, 3), Fraction(y) - 1000) for x, y in p], 1
    else:
        q, f = [(4 * x, 4 * y) for x, y in p], 16
    assert pp.exact_convex_skull(q).area == f * frac(entry["exact"]["area"])
    if change != "translated":  # see test_mesh_with_rational_coordinates
        assert pp.mesh_convex_skull(q).area == f * frac(entry["mesh_area"])
    assert pp.vertex_convex_skull(q).area == f * frac(entry["vertex_area"])
    a, b = pp.longest_segment(q)
    assert (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 == f * frac(entry["longest_segment_sq"])


@pytest.mark.parametrize("name", HAND_MADE)
def test_mesh_with_rational_coordinates(name):
    # coordinates that are not doubles: the triangulation is computed on rounded
    # values and checked exactly; P / 3 has the same mesh skull scaled by 1/9
    p = HAND_MADE[name]
    q = [(Fraction(x, 3), Fraction(y, 3) + Fraction(1, 7)) for x, y in p]
    s = pp.mesh_convex_skull(q)
    assert_certified(q, s)
    assert s.area == pp.mesh_convex_skull(p).area / 9
    tris = pp.triangulate(q)
    assert sum(area([q[i] for i in t]) for t in tris) == area(q)
    assert_certified(q, pp.approximate_convex_skull(q, lower_bound="aronov"))


@pytest.mark.parametrize("name", CONVEX)
def test_convex_polygon_returns_itself(name):
    p = CONVEX[name]
    expected = canon(p)
    for s in (pp.exact_convex_skull(p), pp.mesh_convex_skull(p), pp.vertex_convex_skull(p),
              pp.approximate_convex_skull(p), pp.convex_skull(p), pp.refine(p, p)):
        assert canon(s.vertices) == expected
        assert s.area == area(p)
    assert pp.upper_bound(p) == area(p)


def test_cross_other_methods():
    p = CROSS
    u = pp.upper_bound(p)
    for s in (pp.mesh_convex_skull(p), pp.vertex_convex_skull(p),
              pp.approximate_convex_skull(p), pp.convex_skull(p)):
        assert_certified(p, s)
        assert s.area <= u


# Grid shapes whose chords run along edges of P: a chord can have convex ends and
# pass through reflex corners (the cross's x = 1 through (1,1) and (1,2)).
ALONG_EDGES = {
    "cross": CROSS,
    "T": [(0, 2), (0, 3), (3, 3), (3, 2), (2, 2), (2, 0), (1, 0), (1, 2)],
    "H": [(0, 0), (1, 0), (1, 1), (2, 1), (2, 0), (3, 0), (3, 3), (2, 3), (2, 2), (1, 2),
          (1, 3), (0, 3)],
    "stairs": [(0, 0), (3, 0), (3, 1), (2, 1), (2, 2), (1, 2), (1, 3), (0, 3)],
}


@pytest.mark.parametrize("name", ALONG_EDGES)
def test_exact_with_chords_along_edges(name):
    p = ALONG_EDGES[name]
    areas = set()
    for r in range(len(p)):
        for q in (p[r:] + p[:r], (p[r:] + p[:r])[::-1]):
            s = pp.exact_convex_skull(q)
            assert_certified(q, s)
            areas.add(s.area)
    assert len(areas) == 1
    a = areas.pop()
    assert pp.vertex_convex_skull(p).area <= a <= pp.upper_bound(p)
    assert pp.convex_skull(p).area <= a


@pytest.mark.parametrize("seed", [0, 1, 2, 12345])
def test_seeds_give_certified_answers(seed):
    p = HAND_MADE["comb"]
    assert_certified(p, pp.approximate_convex_skull(p, seed=seed))
    assert_certified(p, pp.convex_skull(p, seed=seed))


def test_lower_bound_choices():
    p = HAND_MADE["spiky"]
    for lb in ("vertex_triangle", "aronov"):
        assert_certified(p, pp.approximate_convex_skull(p, lower_bound=lb))


@pytest.mark.parametrize("threads", [1, 4])
def test_timeout_at_paper_sizes(threads):
    # one edge search at the paper's sizes runs for minutes; the stop acts inside it
    import time
    t = time.monotonic()
    with pytest.raises(TimeoutError):
        pp.approximate_convex_skull(HAND_MADE["L"], epsilon=0.5, sample_scale=1, s_scale=1,
                                    max_edges=None, threads=threads, timeout=0.5)
    assert time.monotonic() - t < 1.5


@pytest.mark.parametrize("kw", [dict(split_steps=4), dict(adjoin_rounds=2),
                                dict(rot_steps=1, max_sweeps=1), dict(rot_span=1.0)])
def test_refine_options(kw):
    p = HAND_MADE["arrow"]
    start = [(0, 0), (1, 0), (0, 1)]
    s = pp.refine(p, start, **kw)
    assert_certified(p, s)
    assert s.area >= area(start)
