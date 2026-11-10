#pragma once

// Cabello, Cibulka, Kyncl, Saumell & Valtr, "Peeling potatoes near-optimally in
// near-linear time", arXiv:1406.1368v3 (2017), Section 4: a randomized
// (1-eps)-approximation of the maximum-area convex subset of a simple polygon.
// Cited as [C17 pN], N the page number.
//
// The answer is always convex and contained in P; both are certified exactly in
// EPECK (certify.hpp). Its area is only probabilistically close to optimal.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "internal/common.hpp"
#include "potato_peeling/options.hpp"

namespace potato_peeling::internal::cabello {

    // C1 = (6*C')^{3/2} with C' <= 5, the Renyi-Sulanke missed-area constant for
    // the unit disc [C17 p5].
    inline constexpr double C1 = 164.3168;

    // Lemma 12 [C17 p15], with C2 = U/A(P) derived per polygon.
    inline double c3(const double c2) {
        return 6.0 * 324000.0 * c2 * c2;
    }

    struct Options {
        double epsilon = 0.1;
        // Line 4 repeats 3*log2(1/delta) times: ceil(3*log2 3) = 5 at delta = 1/3.
        double delta = 1.0 / 3.0;
        std::uint64_t seed = 0;

        // Sample-size scales. At 1, with no max_edges, the run is paper-literal,
        // which is infeasible on real polygons. The two sizes are scaled
        // separately because they differ by orders of magnitude:
        //
        //   line 9  |R_ab| = 96*C1*C2/(eps/2)^{3/2}   ~ 10^6
        //   line 10 |S_ab| = 288*C2/eps               ~ 10^4
        //
        // One shared scale small enough for |R_ab| would leave |S_ab| near 1, and
        // the apex of every clique is drawn from S_ab. Line 2's r = 60*area(P)/A(P)
        // is small and always used as is.
        double sample_scale = 3e-4;  // line 9, |R_ab|
        double s_scale = 3e-2;       // line 10, |S_ab|
        // Search at most this many edges of G(P,R) per iteration, chosen uniformly
        // at random; nullopt searches them all.
        std::optional<std::size_t> max_edges = 64;

        // 0 = every hardware thread.
        unsigned threads = 1;

        // How line 1 gets A(P).
        LowerBound lower_bound = LowerBound::aronov;
    };

    struct Result {
        // CCW, exact. Every vertex is a sampled point with double coordinates.
        Polygon polygon;
        FT area{0};  // exact rational area of `polygon`
        bool convex{false};
        bool contained{false};
        std::string message;  // non-empty iff certification failed
    };

    // Algorithm LargePotato [C17 p15, Figure 5], on a cleaned polygon (clean_polygon).
    Result large_potato(const Polygon &p, const Options &opt);

}  // namespace potato_peeling::internal::cabello
