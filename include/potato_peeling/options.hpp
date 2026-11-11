#pragma once

// Options for the methods.
//
// threads = 0 uses every hardware thread. The answer does not depend on the
// thread count; set 1 when running many polygons in parallel yourself.
//
// should_stop, when set, is called from the calling thread between units of
// work; once it returns true the method throws cancelled (refine instead
// returns its answer so far, which is certified).

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

namespace potato_peeling {

    using StopCallback = std::function<bool()>;

    // The options every method takes; exact_convex_skull, mesh_convex_skull and
    // vertex_convex_skull take only these.
    struct BaseOptions {
        StopCallback should_stop;
    };

    // convex_skull: the best general-purpose method. What it runs may improve
    // between versions; currently approximate_convex_skull, then refine with
    // adjoin moves.
    struct ConvexSkullOptions : BaseOptions {
        std::uint64_t seed = 0;
        unsigned threads = 0;
    };

    // How approximate_convex_skull gets A(P), a lower bound on the skull's area
    // that sets its sample sizes (a smaller A(P) means larger samples): the area of
    // the largest triangle on P's vertices inside P (the default: better answers,
    // and faster overall despite its O(n^3) visibility tests), or of
    // mesh_convex_skull.
    enum class LowerBound { aronov, vertex_triangle };

    // With sample_scale = s_scale = 1 and no max_edges, approximate_convex_skull
    // is the paper's (1 - epsilon)-approximation with failure probability delta;
    // the defaults are far smaller, and the answer is certified but carries no
    // such bound.
    struct ApproximateOptions : BaseOptions {
        double epsilon = 0.1;
        double delta = 1.0 / 3.0;  // failure probability
        std::uint64_t seed = 0;
        double sample_scale = 3e-4;                 // scale of |R_ab|; 1 = the paper's size
        double s_scale = 3e-2;                      // scale of |S_ab|; 1 = the paper's size
        std::optional<std::size_t> max_edges = 64;  // nullopt = search every edge
        unsigned threads = 0;
        LowerBound lower_bound = LowerBound::vertex_triangle;
    };

    // upper_bound: the search stops once within rel_tol * area(P) (finite and >= 0,
    // where 0 searches until the caps, else std::invalid_argument); `pairs` pivots are paired for the
    // two-pivot bound (0 = none); width_dirs uniform slice directions are tried
    // besides P's edge directions; edge_samples adds pivots inside each edge. The
    // caps bound the work per pivot and per pair: stopping early loosens the
    // bound but never makes it invalid.
    struct UpperBoundOptions : BaseOptions {
        double rel_tol = 1e-3;
        unsigned pairs = 6;
        unsigned width_dirs = 180;
        unsigned edge_samples = 0;
        std::size_t max_evals_1d = 200000;
        std::size_t max_evals_2d = 4000;
    };

    struct RefineOptions : BaseOptions {
        unsigned max_sweeps = 12;
        unsigned rot_steps = 9;
        double rot_span = 0.30;      // radians
        unsigned split_steps = 0;    // 0 = no split moves
        unsigned adjoin_rounds = 0;  // 0 = no adjoin moves
    };

}  // namespace potato_peeling
