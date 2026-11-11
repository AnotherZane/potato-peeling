"""potato-peeling: the largest convex polygon inside a simple polygon.

Results are exact (fractions.Fraction); every answer is certified convex and
contained in the input. See the README for the methods and their trade-offs.
"""
from importlib.metadata import PackageNotFoundError, version as _version

from ._core import CertificationError, InvalidPolygonError
from ._api import (Mesh, Skull, approximate_convex_skull, convex_skull, exact_convex_skull,
                   is_convex_subset, longest_segment, mesh_convex_skull, reflex_vertices,
                   refine, segment_in_polygon, triangulate, upper_bound, vertex_convex_skull)

__all__ = [
    "Skull", "Mesh", "InvalidPolygonError", "CertificationError",
    "convex_skull", "exact_convex_skull", "mesh_convex_skull", "approximate_convex_skull",
    "refine", "vertex_convex_skull", "upper_bound",
    "is_convex_subset", "segment_in_polygon", "triangulate", "reflex_vertices",
    "longest_segment",
]

try:
    __version__ = _version("potato-peeling")
except PackageNotFoundError:  # pragma: no cover
    __version__ = "unknown"
