"""Bad polygons, bad coordinates and bad options."""
import math

import pytest

import potato_peeling as pp

from conftest import L_SHAPE

METHODS = {
    "convex_skull": pp.convex_skull,
    "exact_convex_skull": pp.exact_convex_skull,
    "mesh_convex_skull": pp.mesh_convex_skull,
    "approximate_convex_skull": pp.approximate_convex_skull,
    "vertex_convex_skull": pp.vertex_convex_skull,
    "upper_bound": pp.upper_bound,
    "refine": lambda p: pp.refine(p, [(0, 0), (1, 0), (0, 1)]),
    "longest_segment": pp.longest_segment,
}

BAD_POLYGONS = {
    "empty": [],
    "two vertices": [(0, 0), (1, 0)],
    "repeated vertex only": [(1, 1)] * 5,
    "collinear": [(0, 0), (1, 1), (2, 2), (3, 3)],
    "self-intersecting": [(0, 0), (2, 2), (2, 0), (0, 2)],
    "touching itself": [(0, 0), (4, 0), (4, 4), (2, 0), (0, 4)],
    "nan": [(0, 0), (1, 0), (math.nan, 1)],
    "inf": [(0, 0), (math.inf, 0), (0, 1)],
    "hole": {"type": "Polygon", "coordinates": [[(0, 0), (4, 0), (4, 4), (0, 4)],
                                                [(1, 1), (2, 1), (2, 2)]]},
    "multipolygon": {"type": "MultiPolygon", "coordinates": [[[(0, 0), (1, 0), (0, 1)]]]},
}


@pytest.mark.parametrize("method", METHODS)
@pytest.mark.parametrize("bad", BAD_POLYGONS)
def test_invalid_polygon(method, bad):
    with pytest.raises(pp.InvalidPolygonError):
        METHODS[method](BAD_POLYGONS[bad])


def test_error_classes():
    assert issubclass(pp.InvalidPolygonError, ValueError)
    assert issubclass(pp.CertificationError, RuntimeError)
    assert not issubclass(pp.CertificationError, ValueError)


@pytest.mark.parametrize("bad", [
    [(0, 0), (True, 0), (0, 1)],
    [(0, 0), (1, 0, 0), (0, 1)],
    [(0, 0), (1,), (0, 1)],
    [(0, 0), ("1", 0), (0, 1)],
    [(0, 0), (None, 0), (0, 1)],
    42,
])
def test_bad_coordinates(bad):
    with pytest.raises(TypeError):
        pp.exact_convex_skull(bad)


def test_shapely_holes_and_parts():
    shapely = pytest.importorskip("shapely")
    from shapely.geometry import MultiPolygon, Polygon
    holed = Polygon([(0, 0), (4, 0), (4, 4), (0, 4)], [[(1, 1), (2, 1), (2, 2)]])
    with pytest.raises(pp.InvalidPolygonError):
        pp.convex_skull(holed)
    with pytest.raises(pp.InvalidPolygonError):
        pp.convex_skull(MultiPolygon([Polygon(L_SHAPE)]))


def test_refine_rejects_start_outside():
    with pytest.raises(pp.InvalidPolygonError):
        pp.refine(L_SHAPE, [(0, 0), (2, 0), (2, 2), (0, 2)])


def test_refine_rejects_non_convex_start():
    with pytest.raises(pp.InvalidPolygonError):
        pp.refine(L_SHAPE, [(0, 0), (2, 0), (1, 0.5), (2, 1), (0, 1)])


def test_bad_mesh_type():
    with pytest.raises((TypeError, ValueError)):
        pp.mesh_convex_skull(L_SHAPE, ([(0, 0)], [(0, 1)]))


@pytest.mark.parametrize("kw", [
    dict(epsilon=0), dict(epsilon=1), dict(epsilon=2), dict(delta=0), dict(delta=1.5),
    dict(sample_scale=0), dict(sample_scale=-1), dict(s_scale=0), dict(lower_bound="x"),
    dict(epsilon=math.nan), dict(sample_scale=math.nan), dict(s_scale=math.nan),
])
def test_bad_approximate_options(kw):
    with pytest.raises(ValueError):
        pp.approximate_convex_skull(L_SHAPE, **kw)


def test_max_edges_zero():
    with pytest.raises(ValueError):
        pp.approximate_convex_skull(L_SHAPE, max_edges=0)


@pytest.mark.parametrize("rel_tol", [-1, -1e-9, math.nan, math.inf])
def test_bad_upper_bound_tolerance(rel_tol):
    with pytest.raises(ValueError):
        pp.upper_bound(L_SHAPE, rel_tol=rel_tol)


def test_upper_bound_zero_tolerance():
    u = pp.upper_bound(L_SHAPE, rel_tol=0)
    assert pp.exact_convex_skull(L_SHAPE).area <= u <= 3


@pytest.mark.parametrize("kw", [dict(threads=-1), dict(seed=-1)])
def test_negative_counts(kw):
    with pytest.raises((TypeError, ValueError, OverflowError)):
        pp.approximate_convex_skull(L_SHAPE, **kw)


def test_keyword_only_options():
    with pytest.raises(TypeError):
        pp.approximate_convex_skull(L_SHAPE, 0.2)
