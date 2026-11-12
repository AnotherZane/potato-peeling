"""The Python API over potato_peeling._core."""
from __future__ import annotations

import math
from dataclasses import dataclass
from decimal import Decimal
from fractions import Fraction
from typing import (Any, List, Mapping, NamedTuple, Optional, Protocol, Sequence, SupportsFloat,
                    Tuple, Union)

from . import _core
from ._core import InvalidPolygonError

class _ArrayLike(Protocol):
    """A NumPy array, read through tolist()."""

    def tolist(self) -> Any: ...


class _GeoInterface(Protocol):
    """A shapely polygon or other object with __geo_interface__."""

    @property
    def __geo_interface__(self) -> Mapping[str, Any]: ...


# SupportsFloat admits NumPy scalars.
Number = Union[int, float, Fraction, Decimal, SupportsFloat]
Point = Union[Sequence[Number], _ArrayLike]
ExactPoint = Tuple[Fraction, Fraction]
# A polygon in any accepted form: points, an (n, 2) array, a GeoJSON mapping, an
# object with __geo_interface__, or a Skull.
PolygonLike = Union[Sequence[Point], _ArrayLike, Mapping[str, Any], _GeoInterface]


@dataclass(frozen=True)
class Skull:
    """A convex polygon inside P: counter-clockwise, without collinear vertices,
    with exact vertices and area."""

    vertices: Tuple[ExactPoint, ...]
    area: Fraction

    @property
    def coords(self) -> List[Tuple[float, float]]:
        """The vertices rounded to the nearest floats. Rounding can move a vertex
        outside P or break strict convexity by half an ulp; `vertices` is exact."""
        return [(float(x), float(y)) for x, y in self.vertices]

    @property
    def __geo_interface__(self) -> dict:
        ring = self.coords
        return {"type": "Polygon", "coordinates": [ring + ring[:1]]}


class Mesh(NamedTuple):
    """A triangulation of P for mesh_convex_skull: `triangles` index `points`.
    Points other than P's vertices may lie inside P or on its edges."""

    points: Union[Sequence[Point], _ArrayLike]
    triangles: Union[Sequence[Sequence[int]], _ArrayLike]


# ── inputs ──────────────────────────────────────────────────────────────────


def _coord(v: Any):
    if isinstance(v, bool):
        raise TypeError("a coordinate cannot be a bool")
    if isinstance(v, float):
        if not math.isfinite(v):
            raise InvalidPolygonError("coordinate is not finite")
        return v
    if isinstance(v, int):
        return v
    if isinstance(v, Fraction):
        return v.numerator if v.denominator == 1 else (v.numerator, v.denominator)
    if isinstance(v, Decimal):
        if not v.is_finite():
            raise InvalidPolygonError("coordinate is not finite")
        return _coord(Fraction(v))
    if hasattr(v, "item"):  # NumPy scalar
        return _coord(v.item())
    raise TypeError(f"unsupported coordinate type {type(v).__name__}")


def _point(p: Any):
    if len(p) != 2:
        raise TypeError("a point must have two coordinates")
    return (_coord(p[0]), _coord(p[1]))


def _ring(polygon: Any) -> list:
    """Any accepted polygon form as a list of exact coordinate pairs."""
    if isinstance(polygon, Skull):
        return [(_coord(x), _coord(y)) for x, y in polygon.vertices]
    gi = getattr(polygon, "__geo_interface__", None)
    if gi is None and isinstance(polygon, dict) and "type" in polygon:
        gi = polygon
    if gi is not None:
        if gi.get("type") != "Polygon":
            raise InvalidPolygonError(f"expected a Polygon, got {gi.get('type')}")
        rings = gi["coordinates"]
        if len(rings) != 1:
            raise InvalidPolygonError("polygons with holes are not supported")
        return [_point(p[:2]) for p in rings[0]]
    if hasattr(polygon, "tolist") and not isinstance(polygon, (list, tuple)):
        polygon = polygon.tolist()  # NumPy array, shape (n, 2)
    return [_point(p) for p in polygon]


def _skull(raw) -> Skull:
    vertices, (an, ad) = raw
    return Skull(tuple((Fraction(xn, xd), Fraction(yn, yd)) for xn, xd, yn, yd in vertices),
                 Fraction(an, ad))


# ── methods ─────────────────────────────────────────────────────────────────


def convex_skull(polygon: PolygonLike, *, seed: int = 0, threads: int = 0,
                 timeout: Optional[float] = None) -> Skull:
    """The best general-purpose method: certified, and close to optimal in
    practice. What it runs may improve between versions (currently
    approximate_convex_skull, then refine with adjoin moves).

    threads: 0 uses every hardware thread; the answer does not depend on it.
    timeout: seconds; stopped while refining, the answer so far is returned,
    earlier TimeoutError is raised.
    """
    return _skull(_core.convex_skull(_ring(polygon), seed, threads, timeout))


def exact_convex_skull(polygon: PolygonLike, *, timeout: Optional[float] = None) -> Skull:
    """The exact optimum (Chang & Yap 1986). Practical up to a few dozen
    vertices; raises TimeoutError after `timeout` seconds."""
    return _skull(_core.exact_convex_skull(_ring(polygon), timeout))


MeshLike = Union[Mesh, Tuple[Union[Sequence[Point], _ArrayLike],
                             Union[Sequence[Sequence[int]], _ArrayLike]]]


def mesh_convex_skull(polygon: PolygonLike, mesh: Optional[MeshLike] = None,
                      *, timeout: Optional[float] = None) -> Skull:
    """The best union of triangles of a triangulation of P (Aronov, van
    Kreveld, Löffler & Silveira 2011): P's constrained Delaunay triangulation,
    or `mesh`, which must tile P exactly (InvalidPolygonError otherwise)."""
    if mesh is None:
        return _skull(_core.mesh_convex_skull(_ring(polygon), None, None, timeout))
    points, triangles = mesh
    return _skull(_core.mesh_convex_skull(_ring(polygon), [_point(p) for p in points],
                                          [tuple(t) for t in triangles], timeout))


def approximate_convex_skull(polygon: PolygonLike, *, epsilon: float = 0.1, delta: float = 1 / 3,
                             seed: int = 0, sample_scale: float = 3e-4, s_scale: float = 3e-2,
                             max_edges: Optional[int] = 64, threads: int = 0,
                             lower_bound: str = "vertex_triangle",
                             timeout: Optional[float] = None) -> Skull:
    """Randomised approximation (Cabello, Cibulka, Kynčl, Saumell & Valtr 2017).
    With sample_scale = s_scale = 1 and max_edges=None it is the paper's
    (1 - epsilon)-approximation with failure probability delta; the defaults
    are far smaller, and the answer is certified but carries no such bound.

    lower_bound: how A(P), which sets the sample sizes, is found:
    "vertex_triangle" (the largest triangle on P's vertices inside P; the
    default, better and faster on the whole) or "aronov" (mesh_convex_skull's
    area).
    """
    return _skull(_core.approximate_convex_skull(
        _ring(polygon), epsilon, delta, seed, sample_scale, s_scale, max_edges, threads,
        lower_bound, timeout))


def refine(polygon: PolygonLike, start: PolygonLike, *, max_sweeps: int = 12, rot_steps: int = 9, rot_span: float = 0.30,
           split_steps: int = 0, adjoin_rounds: int = 0,
           timeout: Optional[float] = None) -> Skull:
    """Local ascent from `start` (a convex polygon inside P, or a Skull); never
    returns less than start. After `timeout` seconds the answer so far is
    returned."""
    return _skull(_core.refine(_ring(polygon), _ring(start), max_sweeps, rot_steps, rot_span,
                               split_steps, adjoin_rounds, timeout))


def vertex_convex_skull(polygon: PolygonLike, *, timeout: Optional[float] = None) -> Skull:
    """The largest convex polygon in P whose vertices are vertices of P (Fischer;
    Bautista-Santiago et al.). Never below mesh_convex_skull(polygon)."""
    return _skull(_core.vertex_convex_skull(_ring(polygon), timeout))


def upper_bound(polygon: PolygonLike, *, rel_tol: float = 1e-3, pairs: int = 6, width_dirs: int = 180,
                edge_samples: int = 0, max_evals_1d: int = 200_000, max_evals_2d: int = 4_000,
                timeout: Optional[float] = None) -> Fraction:
    """A certified upper bound on the area of P's convex skull (pivot, two-pivot
    and width relaxations, computed exactly)."""
    n, d = _core.upper_bound(_ring(polygon), rel_tol, pairs, width_dirs, edge_samples,
                             max_evals_1d, max_evals_2d, timeout)
    return Fraction(n, d)


# ── utilities ───────────────────────────────────────────────────────────────


def is_convex_subset(polygon: PolygonLike, q: PolygonLike) -> bool:
    """True iff q is a convex polygon contained in P."""
    return _core.is_convex_subset(_ring(polygon), _ring(q))


def segment_in_polygon(polygon: PolygonLike, a: Point, b: Point) -> bool:
    """True iff the segment [a, b] lies in P (boundary included)."""
    return _core.segment_in_polygon(_ring(polygon), _point(a), _point(b))


def triangulate(polygon: PolygonLike) -> List[Tuple[int, int, int]]:
    """The constrained Delaunay triangulation of P, as counter-clockwise index
    triples into its vertices (which must not repeat)."""
    return [tuple(t) for t in _core.triangulate(_ring(polygon))]


def reflex_vertices(polygon: PolygonLike) -> List[bool]:
    """One flag per vertex of P (which must not repeat): True at reflex corners."""
    return list(_core.reflex_vertices(_ring(polygon)))


def longest_segment(polygon: PolygonLike) -> Tuple[ExactPoint, ExactPoint]:
    """The longest segment contained in P, exact."""
    (an, ad, bn, bd), (cn, cd, dn, dd) = _core.longest_segment(_ring(polygon))
    return (Fraction(an, ad), Fraction(bn, bd)), (Fraction(cn, cd), Fraction(dn, dd))
