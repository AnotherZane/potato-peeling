#pragma once

#include <array>
#include <cstddef>

#include <vector>
#include "internal/common.hpp"

// Polygon helpers shared by the algorithms.

namespace potato_peeling::internal {
    inline constexpr double PI = 3.14159265358979323846;

    // Orders points by their projection onto a direction.
    struct SortAlongLine {
        Direction direction;

        explicit SortAlongLine(const Direction &dir) : direction(dir) {}

        bool operator()(const Point &p1, const Point &p2) const {
            const auto t1 = direction.dx() * p1.x() + direction.dy() * p1.y();
            const auto t2 = direction.dx() * p2.x() + direction.dy() * p2.y();
            return t1 < t2;
        }
    };

    // One flag per vertex: true at reflex corners, for either orientation.
    std::vector<bool> reflex_vertices_flags(const Polygon &polygon);

    // Convex hull of a point set: CCW, without collinear points, starting at the
    // lexicographically smallest point (Andrew's monotone chain).
    std::vector<Point> convex_hull(std::vector<Point> pts);

    // The double nearest to v, ties to even (as Python's float(Fraction)).
    double nearest_double(const FT &v);

    // Removes every vertex lying on the segment between its neighbours.
    void remove_collinear_vertices(Polygon &polygon);

    // Removes repeated vertices, including a closing copy of the first.
    Polygon remove_repeated_vertices(const Polygon &input);

    // The input check every method runs first: removes repeated vertices and
    // collinear vertices, keeping the orientation and the first remaining vertex.
    // Throws invalid_polygon unless the result is a simple polygon with at least
    // 3 vertices.
    Polygon clean_polygon(const Polygon &input);

    // Copy of p oriented counter-clockwise.
    Polygon counterclockwise(const Polygon &p);

    // |shoelace| / 2 of a closed path.
    FT path_area(const std::vector<Point> &path);

    // Constrained Delaunay triangulation of a simple polygon given by its vertices, as
    // index triples into `ring` (orientation unspecified). Vertices are inserted in the
    // order given, so the result is deterministic. Throws if a vertex was added or lost.
    std::vector<std::array<std::size_t, 3>>
    triangulate_polygon(const std::vector<std::array<double, 2>> &ring);

    // As above for exact points. Coordinates that are not doubles are rounded for
    // CDT and the result is checked exactly (tiles_polygon); if rounding broke it,
    // the polygon is ear-clipped instead, so the result is then not Delaunay.
    std::vector<std::array<std::size_t, 3>> triangulate_polygon(const std::vector<Point> &ring);

    // True iff the triangles (index triples into `ring`, either orientation) tile the
    // simple polygon `ring` exactly: none degenerate, and once each is made CCW, no
    // directed edge occurs twice and the unpaired edges are P's edges, each once, in
    // P's CCW direction. Every point is then covered by as many triangles as P's
    // boundary winds around it: once inside, never outside.
    bool tiles_polygon(const std::vector<Point> &ring,
                       const std::vector<std::array<std::size_t, 3>> &triangles);

    // An ear-clipping triangulation of a simple polygon, in exact arithmetic. O(n^3).
    std::vector<std::array<std::size_t, 3>> ear_clip(const std::vector<Point> &ring);
}  // namespace potato_peeling::internal
