#pragma once

// Options for the methods.

#include <cstddef>
#include <cstdint>
#include <optional>

namespace potato_peeling {

    // convex_skull: the best general-purpose method. What it runs may improve
    // between versions; currently approximate_convex_skull, then refine with
    // adjoin moves.
    struct ConvexSkullOptions {
        std::uint64_t seed = 0;
        unsigned threads = 1;
    };

    // With sample_scale = s_scale = 1 and no max_edges, approximate_convex_skull
    // is the paper's (1 - epsilon)-approximation with failure probability delta;
    // the defaults are far smaller, and the answer is certified but carries no
    // such bound.
    struct ApproximateOptions {
        double epsilon = 0.1;
        double delta = 1.0 / 3.0;  // failure probability
        std::uint64_t seed = 0;
        double sample_scale = 3e-4;                 // scale of |R_ab|; 1 = the paper's size
        double s_scale = 3e-2;                      // scale of |S_ab|; 1 = the paper's size
        std::optional<std::size_t> max_edges = 64;  // nullopt = search every edge
        unsigned threads = 1;
    };

    struct RefineOptions {
        unsigned max_sweeps = 12;
        unsigned rot_steps = 9;
        double rot_span = 0.30;      // radians
        unsigned split_steps = 0;    // 0 = no split moves
        unsigned adjoin_rounds = 0;  // 0 = no adjoin moves
    };

}  // namespace potato_peeling
