#pragma once

// Q as an intersection of half-planes, and the exact helpers refinement builds on.

#include <cstddef>
#include <vector>

#include "internal/common.hpp"

namespace potato_peeling::internal::refinement {

    // a*x + b*y <= c, outward normal (a,b).
    struct HalfPlane {
        FT a, b, c;
    };

    // A double from the exact value. CGAL::to_double alone reads a lazy number's
    // current interval, which tightens once anything forces the exact value, so the
    // search would depend on what had been evaluated before.
    inline double approx(const FT &x) {
        return CGAL::to_double(CGAL::exact(x));
    }

    // A copy rebuilt from the exact value, sharing nothing with the original, for
    // another thread (see Workers).
    inline FT fresh(const FT &x) {
        return FT(CGAL::exact(x));
    }
    inline Point fresh(const Point &p) {
        return {fresh(p.x()), fresh(p.y())};
    }
    inline HalfPlane fresh(const HalfPlane &h) {
        return {fresh(h.a), fresh(h.b), fresh(h.c)};
    }
    template <class T> std::vector<T> fresh(const std::vector<T> &v) {
        std::vector<T> out;
        out.reserve(v.size());
        for (const T &x : v) out.push_back(fresh(x));
        return out;
    }

    // The half-planes of a CCW convex polygon, one per edge.
    std::vector<HalfPlane> halfplanes_of(const Polygon &q);

    // Intersect all the half-planes, starting from a bounding box: Q's vertices,
    // or empty if fewer than 3 remain. Exact.
    std::vector<Point> realise(const std::vector<HalfPlane> &hp, const std::vector<Point> &box);

    // Drop half-planes that constrain nothing: removing one leaves the realised
    // area unchanged. Returns how many were dropped.
    std::size_t prune(std::vector<HalfPlane> &hp, const std::vector<Point> &box);

    // The point on h's line to rotate about: the midpoint of Q's edge on it. If no
    // edge of Q (vertices `pts`) lies on it, the foot of the perpendicular from the
    // centroid.
    Point rotation_anchor(const std::vector<Point> &pts, const HalfPlane &h);

}  // namespace potato_peeling::internal::refinement
