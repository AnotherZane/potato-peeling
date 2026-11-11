#pragma once

// The largest convex polygon in P whose vertices are vertices of P.
//
// Build the visibility graph H over P's vertices (closed region: boundary
// contact allowed). A convex polygon on P's vertices lies in P exactly when its
// consecutive vertices see each other (certify.hpp), so the answer is the best
// of phi(H, s) over every vertex s as the highest point [C17 p10, citing
// Fischer and Bautista-Santiago et al.]. phi's convexity tests are exact; it
// compares areas in doubles, so among candidates whose areas agree to rounding
// it may keep either. O(n^3) visibility tests plus n calls to phi.

#include "internal/common.hpp"

namespace potato_peeling::internal::cabello {

    // For a cleaned polygon (clean_polygon); CCW. A convex P is returned as is.
    // Checks `stop` per visibility row and per apex.
    Polygon vertex_convex_skull(const Polygon &p, const StopFn &stop);

}  // namespace potato_peeling::internal::cabello
