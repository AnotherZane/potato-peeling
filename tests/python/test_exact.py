"""The exact methods against the fixture: the thesis's Chang & Yap optima, and the
library's own answers for the mesh skull, the vertex skull and the longest segment.
Exact methods must keep their answers between versions."""
import pytest

import potato_peeling as pp

from conftest import (FIXTURE, assert_certified, canon, fixture_params, frac, frac_points,
                      polygon, shoelace, solved)

SUBOPTIMAL = [e for e in FIXTURE if "best_known" in e]


@pytest.mark.parametrize("entry", fixture_params())
def test_exact_convex_skull_matches_the_thesis(entry):
    p = polygon(entry)
    s = solved("exact_convex_skull", p, entry["pid"])
    assert_certified(p, s)
    assert s.area == frac(entry["exact"]["area"])
    assert canon(s.vertices) == frac_points(entry["exact"]["vertices"])


@pytest.mark.parametrize("entry", fixture_params())
def test_mesh_convex_skull(entry):
    p = polygon(entry)
    s = solved("mesh_convex_skull", p, entry["pid"])
    assert_certified(p, s)
    assert s.area == frac(entry["mesh_area"])


@pytest.mark.parametrize("entry", fixture_params())
def test_vertex_convex_skull(entry):
    p = polygon(entry)
    s = solved("vertex_convex_skull", p, entry["pid"])
    assert_certified(p, s)
    assert s.area == frac(entry["vertex_area"])
    corners = set(frac_points(p))
    assert all(v in corners for v in s.vertices)


@pytest.mark.parametrize("entry", fixture_params())
def test_longest_segment(entry):
    p = polygon(entry)
    a, b = pp.longest_segment(p)
    assert (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 == frac(entry["longest_segment_sq"])
    assert pp.segment_in_polygon(p, a, b)


# ── the seven optima the thesis shows suboptimal ─────────────────────────────


@pytest.mark.parametrize("entry", fixture_params(SUBOPTIMAL))
def test_best_known_body_is_certified_and_larger(entry):
    p = polygon(entry)
    body = entry["best_known"]
    q = frac_points(body["vertices"])
    assert pp.is_convex_subset(p, q)
    assert shoelace(q) == frac(body["area"]) > frac(entry["exact"]["area"])


@pytest.mark.parametrize("entry", fixture_params(SUBOPTIMAL))
def test_upper_bound_above_best_known(entry):
    u = solved("upper_bound", polygon(entry), entry["pid"])
    assert u >= frac(entry["best_known"]["area"])


@pytest.mark.parametrize("entry", fixture_params(SUBOPTIMAL))
@pytest.mark.xfail(strict=True, reason="Chang & Yap as published misses these optima (thesis Ch6)")
def test_exact_reaches_best_known(entry):
    s = solved("exact_convex_skull", polygon(entry), entry["pid"])
    assert s.area >= frac(entry["best_known"]["area"])
