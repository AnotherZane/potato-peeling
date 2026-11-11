"""mesh_convex_skull over a caller's mesh: the default triangulation passed explicitly,
refined meshes with Steiner points, and every kind of invalid mesh."""
from fractions import Fraction

import pytest

import potato_peeling as pp

from conftest import FIXTURE, HAND_MADE, SQUARE, assert_certified, fixture_params, polygon

SMALL = [e for e in FIXTURE if e["tier"] == "default" and e["n"] <= 12]
CASES = fixture_params(SMALL) + [pytest.param(HAND_MADE[k], id=k) for k in ("L", "arrow", "spiky")]


def as_polygon(case):
    return polygon(case) if isinstance(case, dict) else case


def centroid_split(points, triangles):
    """Each triangle into three, at its centroid."""
    pts = [tuple(Fraction(c) for c in p) for p in points]
    out = []
    for a, b, c in triangles:
        pts.append(tuple((pts[a][i] + pts[b][i] + pts[c][i]) / 3 for i in range(2)))
        g = len(pts) - 1
        out += [(a, b, g), (b, c, g), (c, a, g)]
    return pts, out


def midpoint_split(points, triangles):
    """Each triangle into four at its edge midpoints, shared between neighbours."""
    pts = [tuple(Fraction(c) for c in p) for p in points]
    mid = {}

    def m(i, j):
        k = (min(i, j), max(i, j))
        if k not in mid:
            pts.append(tuple((pts[i][d] + pts[j][d]) / 2 for d in range(2)))
            mid[k] = len(pts) - 1
        return mid[k]

    out = []
    for a, b, c in triangles:
        ab, bc, ca = m(a, b), m(b, c), m(c, a)
        out += [(a, ab, ca), (ab, b, bc), (ca, bc, c), (ab, bc, ca)]
    return pts, out


@pytest.mark.parametrize("case", CASES)
def test_default_triangulation_as_a_mesh(case):
    p = as_polygon(case)
    mesh = pp.Mesh(p, pp.triangulate(p))
    assert pp.mesh_convex_skull(p, mesh) == pp.mesh_convex_skull(p)
    assert pp.mesh_convex_skull(p, (mesh.points, mesh.triangles)) == pp.mesh_convex_skull(p)


@pytest.mark.parametrize("case", CASES)
@pytest.mark.parametrize("split", [centroid_split, midpoint_split])
def test_refined_mesh_never_smaller(case, split):
    p = as_polygon(case)
    base = pp.mesh_convex_skull(p)
    points, triangles = split(p, pp.triangulate(p))
    s = pp.mesh_convex_skull(p, (points, triangles))
    assert_certified(p, s)
    assert base.area <= s.area <= pp.exact_convex_skull(p).area


def test_either_orientation_and_steiner_point_on_an_edge():
    # square with a Steiner point m on its bottom edge
    p = SQUARE
    points = p + [(Fraction(1, 2), 0)]
    triangles = [(0, 4, 3), (4, 1, 2), (4, 2, 3)]
    assert pp.mesh_convex_skull(p, (points, triangles)).area == 1
    flipped = [(a, c, b) for a, b, c in triangles]
    assert pp.mesh_convex_skull(p, (points, flipped)).area == 1


def _l_mesh():
    p = HAND_MADE["L"]
    return p, list(p), pp.triangulate(p)


def _invalid_meshes():
    p, pts, tris = _l_mesh()
    yield "gap", pts, tris[1:]
    yield "duplicate triangle", pts, tris + tris[:1]
    a, b, c = tris[0]
    yield "overlap", pts + [(Fraction(1, 2), Fraction(1, 2))], tris + [(a, b, len(pts))]
    # (2,1), (1,2) and the reflex corner (1,1): the notch, outside P
    yield "outside P", pts, tris + [(3, 2, 4)]
    yield "bad index", pts, tris[:-1] + [(0, 1, 99)]
    yield "unused point", pts + [(Fraction(1, 2), Fraction(1, 2))], tris
    yield "repeated point", pts + [pts[0]], tris
    yield "no triangles", pts, []
    # T-junction: split one triangle at the midpoint of a shared edge, not its neighbour
    shared = None
    for t in tris:
        for i in range(3):
            e = (t[i], t[(i + 1) % 3])
            if any((e[1], e[0]) == (u[j], u[(j + 1) % 3]) for u in tris for j in range(3)):
                shared, tri = e, t
                break
        if shared:
            break
    i, j = shared
    k = next(v for v in tri if v not in shared)
    mpt = tuple((Fraction(pts[i][d]) + Fraction(pts[j][d])) / 2 for d in range(2))
    m = len(pts)
    rest = [t for t in tris if t != tri]
    yield "T-junction", pts + [mpt], rest + [(i, m, k), (m, j, k)]
    # zero-area triangle along an edge with a Steiner point on it
    sq = list(SQUARE) + [(Fraction(1, 2), 0)]
    yield "zero-area triangle", sq, [(0, 4, 3), (4, 1, 2), (4, 2, 3), (0, 4, 1)]


@pytest.mark.parametrize("name,points,triangles",
                         [pytest.param(*m, id=m[0]) for m in _invalid_meshes()])
def test_invalid_mesh_rejected(name, points, triangles):
    p = SQUARE if name == "zero-area triangle" else HAND_MADE["L"]
    with pytest.raises(pp.InvalidPolygonError):
        pp.mesh_convex_skull(p, (points, triangles))
