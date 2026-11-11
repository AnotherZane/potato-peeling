#pragma once

// Exact polygon helpers. P is a simple polygon, either orientation, otherwise
// they throw invalid_polygon. is_convex_subset, segment_in_polygon and
// longest_segment accept repeated and collinear vertices, as the methods do;
// triangulate and
// reflex_vertices index P's own vertices, so P must not repeat one.

#include <array>
#include <cstddef>
#include <vector>

#include "potato_peeling/errors.hpp"
#include "potato_peeling/export.hpp"
#include "potato_peeling/types.hpp"

namespace potato_peeling {

    // True iff Q is a convex polygon (at least 3 vertices, non-zero area, either
    // orientation) contained in the closed region of P.
    POTATO_PEELING_EXPORT bool is_convex_subset(const Polygon &p, const Polygon &q);

    // True iff the segment [a, b] lies in the closed region of P.
    POTATO_PEELING_EXPORT bool segment_in_polygon(const Polygon &p, const Point &a, const Point &b);

    // Constrained Delaunay triangulation of P, as counter-clockwise index triples
    // into its vertices. Coordinates must be exactly representable as doubles.
    POTATO_PEELING_EXPORT std::vector<std::array<std::size_t, 3>> triangulate(const Polygon &p);

    // The longest segment contained in P, exact. Its line passes through two
    // vertices of P; its endpoints are usually inside edges. O(n^3).
    POTATO_PEELING_EXPORT Segment longest_segment(const Polygon &p);

    // One flag per vertex of P, in order: true at reflex corners.
    POTATO_PEELING_EXPORT std::vector<bool> reflex_vertices(const Polygon &p);

}  // namespace potato_peeling
