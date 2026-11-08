#pragma once

// The largest convex polygon inside a simple polygon P (P's convex skull).
//
// Input: a simple polygon without holes, either orientation; repeated and
// collinear vertices are removed first. Fewer than 3 distinct vertices, or a
// self-intersecting polygon, throws invalid_polygon. A convex P returns P.
//
// Every result is certified exactly: convex and contained in P. A failed check
// throws certification_error. The same input and options give the same output
// on a given build.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "potato_peeling/errors.hpp"
#include "potato_peeling/export.hpp"
#include "potato_peeling/types.hpp"

namespace potato_peeling {

    // Exact optimum. Chang & Yap (1986). Practical up to a few dozen vertices.
    POTATO_PEELING_EXPORT Skull exact_convex_skull(const Polygon &p);

    // Optimum among unions of triangles of P's constrained Delaunay
    // triangulation. Aronov, van Kreveld, Löffler & Silveira (2011).
    POTATO_PEELING_EXPORT Skull mesh_convex_skull(const Polygon &p);

    // Randomised approximation. Cabello, Cibulka, Kynčl, Saumell & Valtr (2017).
    // With sample_scale = s_scale = 1 and no max_edges it is the paper's
    // (1 - epsilon)-approximation with failure probability delta; the defaults
    // are far smaller, and the answer is certified but carries no such bound.
    struct ApproximateOptions {
        double epsilon = 0.1;
        double delta = 1.0 / 3.0;  // failure probability
        std::uint64_t seed = 0;
        double sample_scale = 3e-4;                 // scale of |R_ab|; 1 = the paper's size
        double s_scale = 3e-2;                      // scale of |S_ab|; 1 = the paper's size
        std::optional<std::size_t> max_edges = 64;  // nullopt = search every edge
        unsigned threads = 1;
    };
    POTATO_PEELING_EXPORT Skull approximate_convex_skull(const Polygon &p,
                                                         const ApproximateOptions &options = {});

    // Local ascent from `start`, a convex polygon inside P; never returns less
    // than start. A start that is not a convex subset of P throws invalid_polygon.
    struct RefineOptions {
        unsigned max_sweeps = 12;
        unsigned rot_steps = 9;
        double rot_span = 0.30;      // radians
        unsigned split_steps = 0;    // 0 = no split moves
        unsigned adjoin_rounds = 0;  // 0 = no adjoin moves
    };
    POTATO_PEELING_EXPORT Skull refine(const Polygon &p, const Polygon &start,
                                       const RefineOptions &options = {});

}  // namespace potato_peeling
