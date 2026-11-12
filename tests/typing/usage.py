"""Type-checked, not run by pytest: the public API as users call it must pass
mypy --strict and pyright against the installed package."""
import threading
from fractions import Fraction

import numpy as np

import potato_peeling as pp


def area_ratio(p: pp.PolygonLike) -> float:
    skull: pp.Skull = pp.convex_skull(p, seed=1, timeout=10.0)
    best: Fraction = pp.exact_convex_skull(p).area
    return float(skull.area / best)


L = [(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)]
ratio: float = area_ratio(L)
arr = np.array(L, dtype=np.float64)
coords: list[tuple[float, float]] = pp.approximate_convex_skull(arr, epsilon=0.05).coords
start = pp.approximate_convex_skull(L)
refined: pp.Skull = pp.refine(L, start, adjoin_rounds=2)
u: Fraction = pp.upper_bound([(Fraction(1, 3), 0), (2, 0), (2, 1), (0, 1)])
tris: list[tuple[int, int, int]] = pp.triangulate(L)
mesh = pp.mesh_convex_skull(arr, pp.Mesh(arr, np.array(tris)))
inside: bool = pp.segment_in_polygon(L, (0, 0), arr[2])
flags: list[bool] = pp.reflex_vertices(L)
(a, b) = pp.longest_segment(L)
x: Fraction = a[0]
geo = {"type": "Polygon", "coordinates": [L]}
pp.vertex_convex_skull(geo)
cancel: pp.CancelLike = threading.Event()
try:
    pp.exact_convex_skull(L, timeout=5.0, cancel=cancel)
except pp.Cancelled:
    pass
pp.refine(L, start, cancel=lambda: False)

try:
    pp.exact_convex_skull([(0, 0), (1, 1)])
except pp.InvalidPolygonError as e:
    message: str = str(e)
except pp.CertificationError:
    raise
