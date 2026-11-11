#pragma once

// The longest segment contained in a simple polygon.
//
// A longest segment lies on a line through two vertices of P, both on the
// segment. If neither endpoint were a vertex and no vertex lay on the open
// segment, the endpoints would sit inside two edges, and tilting the segment
// (away from where the edges' lines meet, or either way if they are parallel)
// would lengthen it, so it would not be longest. So at a maximum two contacts
// are vertices; they are on the segment, hence see each other. The candidates
// are the visible vertex pairs, each extended as far as the line stays in P.
// A continuum of tied maxima has a member with two vertex contacts, so the
// length is always found. O(n^2) candidate lines, each against the n edges.

#include "internal/common.hpp"

namespace potato_peeling::internal {

    // For a cleaned polygon (clean_polygon).
    Segment longest_segment(const Polygon &p);

}  // namespace potato_peeling::internal
