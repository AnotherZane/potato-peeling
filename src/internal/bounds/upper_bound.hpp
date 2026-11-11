#pragma once

// A certified upper bound U(P) >= A*(P), the largest area of a convex subset.
//
// PIVOT. A point u of dP is not interior to an optimal Q* (it is not interior
// to P), so a line through u has Q* on one side:
//
//     A* <= U(u) := max over theta of area(P ∩ H+(u, theta)),  A* <= min_u U(u).
//
// The area is Lipschitz in theta with constant R_u^2 / 2 (R_u the largest
// distance from u to P), so branch and bound on theta converges to U(u) from
// above, and every intermediate value is already a bound.
//
// TWO PIVOTS. The same for two pivots at once, on the box of their two angles,
// with per-sector Lipschitz constants from P's convex hull (RadialBound).
//
// WIDTH. For every direction w, A* <= the integral of P's longest slice
// segment perpendicular to w (Q*'s slice is one interval inside one segment).
//
// Areas are exact; only the search angles are doubles, and every term entering
// the slack is rounded so that the slack is never understated.

#include <cstddef>

#include "internal/common.hpp"

namespace potato_peeling::internal::bounds {

    struct Options {
        double rel_tol = 1e-3;      // stop once within rel_tol * area(P)
        unsigned edge_samples = 0;  // extra pivots per edge
        unsigned pairs = 6;         // two-pivot bound over this many pivots; 0 = none
        unsigned width_dirs = 180;  // uniform directions added to the edge directions
        std::size_t max_evals_1d = 200000;
        std::size_t max_evals_2d = 4000;
        StopFn should_stop;  // checked per sample, split and width direction
    };

    struct Result {
        FT bound;  // min(pivot, width)
        FT pivot;  // min over single pivots and pairs; area(P) when convex
        FT width;
    };

    // `p` is a cleaned polygon (clean_polygon), either orientation.
    Result upper_bound(const Polygon &p, const Options &options);

}  // namespace potato_peeling::internal::bounds
