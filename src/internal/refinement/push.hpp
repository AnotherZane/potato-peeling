#pragma once

// PUSH: move one half-plane outward as far as Q stays a convex subset of P.
//
// Q grows with the offset c, so the feasible offsets form an interval and its end
// is the optimum for this coordinate. The offset is bisected in double; only the
// evaluation at it is exact, which keeps the lazy-exact DAGs shallow.

#include <cstddef>
#include <vector>

#include "internal/common.hpp"
#include "internal/refinement/evaluator.hpp"
#include "internal/refinement/halfplanes.hpp"

namespace potato_peeling::internal::refinement {

    struct PushResult {
        bool ok{false};  // some offset in [lo, hi] is feasible
        FT c;            // the largest feasible offset found
        FT area;         // Q's area there
        std::vector<Point> pts;
    };

    // Largest feasible offset for half-plane i in [lo, hi]: hi if feasible, else
    // `rounds` steps of bisection.
    PushResult push(const Evaluator &ev, std::vector<HalfPlane> hp, std::size_t i,
                    const std::vector<Point> &box, double lo, double hi, unsigned rounds);

    // The same on all the workers, with the same answer. Each round evaluates, in
    // parallel, every midpoint the next d bisection steps could reach
    // (2^d - 1 <= threads), then follows the path the sequential bisection takes:
    // the same offsets in the same order.
    PushResult push(Workers &workers, const std::vector<HalfPlane> &hp, std::size_t i, double lo,
                    double hi, unsigned rounds);

}  // namespace potato_peeling::internal::refinement
