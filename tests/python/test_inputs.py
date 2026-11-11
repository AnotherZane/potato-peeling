"""The polygon forms the package accepts, and the Skull result type."""
import dataclasses
import json
from decimal import Decimal
from fractions import Fraction

import pytest

import potato_peeling as pp

from conftest import HAND_MADE, L_SHAPE, TRIANGLE, area, canon, frac_points

EXPECTED = pp.exact_convex_skull(L_SHAPE)

FORMS = {
    "tuples": L_SHAPE,
    "lists": [list(p) for p in L_SHAPE],
    "floats": [(float(x), float(y)) for x, y in L_SHAPE],
    "fractions": [(Fraction(x), Fraction(y)) for x, y in L_SHAPE],
    "decimals": [(Decimal(x), Decimal(y)) for x, y in L_SHAPE],
    "mixed": [(0, 0.0), (Fraction(2), 0), (2, Decimal("1.0")), (1.0, 1), (1, 2), (0, 2)],
    "closed": L_SHAPE + [L_SHAPE[0]],
    "repeated vertices": [L_SHAPE[0], L_SHAPE[0], *L_SHAPE[1:3], L_SHAPE[2], *L_SHAPE[3:]],
    "collinear vertices": [(0, 0), (1, 0), (2, 0), (2, 1), (Fraction(3, 2), 1), (1, 1), (1, 2),
                           (0, 2), (0, 1)],
    "tuple of tuples": tuple(L_SHAPE),
    "geojson": {"type": "Polygon", "coordinates": [[list(p) for p in L_SHAPE + [L_SHAPE[0]]]]},
    "geojson 3d": {"type": "Polygon", "coordinates": [[[x, y, 7] for x, y in L_SHAPE]]},
}


class GeoThing:
    __geo_interface__ = {"type": "Polygon", "coordinates": [L_SHAPE]}


FORMS["__geo_interface__"] = GeoThing()


@pytest.mark.parametrize("form", FORMS)
def test_forms_give_the_same_answer(form):
    assert pp.exact_convex_skull(FORMS[form]) == EXPECTED


def test_clockwise():
    # the L has several optimal regions; which one is returned depends on vertex order
    s = pp.exact_convex_skull(L_SHAPE[::-1])
    assert s.area == EXPECTED.area
    assert pp.is_convex_subset(L_SHAPE, s.vertices)


def test_skull_as_input():
    assert pp.exact_convex_skull(EXPECTED) == EXPECTED
    p = HAND_MADE["comb"]
    approx = pp.approximate_convex_skull(p)
    assert pp.refine(p, approx) == pp.refine(p, list(approx.vertices))


def test_floats_are_read_exactly():
    # 0.1 is the double nearest 1/10, not 1/10
    s = pp.exact_convex_skull([(0, 0), (0.1, 0), (0, 0.1)])
    assert (Fraction(0.1), Fraction(0)) in s.vertices
    assert s.area == Fraction(0.1) ** 2 / 2
    t = pp.exact_convex_skull([(0, 0), (Fraction(1, 10), 0), (0, Decimal("0.1"))])
    assert t.area == Fraction(1, 200)


def test_large_integers_are_exact():
    big = 10 ** 30 + 1
    p = [(x * big, y * big) for x, y in L_SHAPE]
    assert pp.exact_convex_skull(p).area == EXPECTED.area * big * big


def test_numpy():
    np = pytest.importorskip("numpy")
    assert pp.exact_convex_skull(np.array(L_SHAPE, dtype=np.float64)) == EXPECTED
    assert pp.exact_convex_skull(np.array(L_SHAPE, dtype=np.int64)) == EXPECTED
    assert pp.exact_convex_skull([(np.float32(x), np.int32(y)) for x, y in L_SHAPE]) == EXPECTED


def test_shapely_round_trip():
    shapely = pytest.importorskip("shapely")
    from shapely.geometry import Polygon, shape
    assert pp.exact_convex_skull(Polygon(L_SHAPE)) == EXPECTED
    out = shape(EXPECTED)
    assert out.is_valid
    assert out.area == pytest.approx(float(EXPECTED.area))


def test_skull_type():
    s = pp.exact_convex_skull(TRIANGLE)
    assert canon(s.vertices) == frac_points(canon(TRIANGLE))
    assert s.area == area(TRIANGLE) and isinstance(s.area, Fraction)
    assert s.coords == [(float(x), float(y)) for x, y in s.vertices]
    gi = s.__geo_interface__
    assert gi["type"] == "Polygon"
    ring = gi["coordinates"][0]
    assert ring[0] == ring[-1] and ring[:-1] == s.coords
    json.dumps(gi)
    with pytest.raises(dataclasses.FrozenInstanceError):
        s.area = Fraction(0)
    assert hash(s) == hash(pp.exact_convex_skull(TRIANGLE))


def test_utilities():
    p = L_SHAPE
    assert pp.reflex_vertices(p) == [False, False, False, True, False, False]
    assert pp.reflex_vertices(p[::-1]) == [False, False, True, False, False, False]
    tris = pp.triangulate(p)
    assert len(tris) == len(p) - 2
    assert sum(area([p[i] for i in t]) for t in tris) == area(p)
    for t in tris:
        a, b, c = (p[i] for i in t)
        assert (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]) > 0
    assert pp.segment_in_polygon(p, (0, 0), (2, 0))          # along the boundary
    assert pp.segment_in_polygon(p, (0, 2), (2, 0))          # through the reflex corner
    assert not pp.segment_in_polygon(p, (0, 2), (2, 1))      # crosses the notch
    assert not pp.segment_in_polygon(p, (2, 1), (1, 2))      # outside
    assert pp.is_convex_subset(p, [(0, 0), (2, 0), (2, 1), (0, 1)])
    assert not pp.is_convex_subset(p, [(0, 0), (2, 0), (2, 2), (0, 2)])
    assert not pp.is_convex_subset(p, L_SHAPE)               # not convex
    a, b = pp.longest_segment(p)
    assert (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 == 8      # the diagonal through (1, 1)
    assert pp.longest_segment(TRIANGLE) in (((0, 0), (3, 0)), ((3, 0), (0, 0)))


def test_upper_bound_type():
    u = pp.upper_bound(L_SHAPE)
    assert isinstance(u, Fraction)
    assert EXPECTED.area <= u <= area(L_SHAPE)
