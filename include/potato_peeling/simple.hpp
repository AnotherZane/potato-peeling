#pragma once

// The methods on plain double coordinates, for callers that do not use CGAL.

#include <array>
#include <cstddef>
#include <vector>

#include "potato_peeling/errors.hpp"
#include "potato_peeling/export.hpp"
#include "potato_peeling/options.hpp"

namespace potato_peeling::simple {

    // A polygon's vertices, either orientation. Coordinates must be finite.
    using Ring = std::vector<std::array<double, 2>>;

    // The exact result, each value rounded to nearest. A rounded vertex moves by
    // at most half an ulp, so the rounded polygon can leave P or lose strict
    // convexity by that much; the exact API keeps the guarantee.
    struct SimpleSkull {
        Ring polygon;  // counter-clockwise, no collinear vertices (before rounding)
        double area;
    };

    POTATO_PEELING_EXPORT SimpleSkull convex_skull(const Ring &p,
                                                   const ConvexSkullOptions &options = {});

    POTATO_PEELING_EXPORT SimpleSkull exact_convex_skull(const Ring &p);

    POTATO_PEELING_EXPORT SimpleSkull mesh_convex_skull(const Ring &p);

    // As potato_peeling::Mesh.
    struct Mesh {
        Ring points;
        std::vector<std::array<std::size_t, 3>> triangles;
    };

    POTATO_PEELING_EXPORT SimpleSkull mesh_convex_skull(const Ring &p, const Mesh &mesh);

    POTATO_PEELING_EXPORT SimpleSkull approximate_convex_skull(const Ring &p,
                                                               const ApproximateOptions &options
                                                               = {});

    // The exact bound rounded up, so it stays an upper bound.
    POTATO_PEELING_EXPORT double upper_bound(const Ring &p, const UpperBoundOptions &options = {});

    POTATO_PEELING_EXPORT SimpleSkull refine(const Ring &p, const Ring &start,
                                             const RefineOptions &options = {});

}  // namespace potato_peeling::simple
