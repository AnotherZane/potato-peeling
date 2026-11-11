#include "internal/cabello/cabello.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <atomic>
#include <thread>
#include <utility>
#include <vector>

#include "internal/cabello/rng.hpp"
#include "internal/cabello/graph.hpp"
#include "internal/cabello/kernel.hpp"
#include "internal/cabello/a_bound.hpp"
#include "internal/cabello/certify.hpp"
#include "internal/cabello/convex_clique.hpp"
#include "internal/cabello/sampling.hpp"
#include "internal/cabello/visibility.hpp"
#include "internal/geometry.hpp"

namespace potato_peeling::internal::cabello {
    namespace {
        std::size_t sized(const double literal, const double scale, const std::size_t floor_value) {
            const double v = std::ceil(literal * scale);
            if (!std::isfinite(v) || v < static_cast<double>(floor_value)) return floor_value;
            // Cap sizes that could not be allocated.
            constexpr double cap = 5e7;
            return static_cast<std::size_t>(std::min(v, cap));
        }

        struct EdgeResult {
            double area{0};
            std::vector<Pt> points;
        };

        // Lines 9-14 for one edge ab of G(P,R). Draws only from substreams
        // addressed by (iteration, edge), so the result does not depend on which
        // thread runs it or when.
        EdgeResult search_edge(const Options &opt, const VisibilityOracle &oracle, const Pt &a,
                               const Pt &b, const double gamma_area, const std::size_t r_ab,
                               const std::size_t s_ab, const std::uint64_t iteration,
                               const std::uint64_t edge_index) {
            EdgeResult out;

            const auto box = gamma(a, b, gamma_area);
            // y(a) == y(b) leaves Gamma undefined. Probability zero.
            if (!box) return out;

            // Lines 9-11: sample in Gamma, keep what lands in P.
            auto gen_r = substream(opt.seed, Stream::SampleRab, iteration, edge_index);
            auto gen_s = substream(opt.seed, Stream::SampleSab, iteration, edge_index);

            std::vector<Pt> pts;
            pts.reserve(r_ab + s_ab);
            for (std::size_t k = 0; k < r_ab; ++k) {
                if (Pt z = sample_uniform(*box, gen_r); oracle.inside(z)) pts.push_back(z);
            }

            const std::size_t n_r = pts.size();  // R_ab occupies pts[0, n_r)

            for (std::size_t k = 0; k < s_ab; ++k) {
                if (Pt z = sample_uniform(*box, gen_s); oracle.inside(z)) pts.push_back(z);
            }
            if (pts.size() < 3) return out;

            const PointGraph g = visibility_graph(oracle, pts);

            // Lines 12-14: the highest vertex ranges over S_ab only [C17 p16].
            for (std::size_t s = n_r; s < pts.size(); ++s) {
                const Clique u = phi(g, s);
                if (u.area > out.area) {
                    out.area = u.area;
                    out.points.clear();
                    out.points.reserve(u.vertices.size());
                    for (const std::size_t v : u.vertices) out.points.push_back(g.point(v));
                }
            }
            return out;
        }
    }  // namespace

    Result large_potato(const Polygon &p, const Options &opt) {
        Result res;

        if (!(opt.epsilon > 0.0 && opt.epsilon < 1.0))
            throw std::invalid_argument("cabello: epsilon must lie in (0,1)");
        if (!(opt.delta > 0.0 && opt.delta < 1.0))
            throw std::invalid_argument("cabello: delta must lie in (0,1)");
        if (opt.sample_scale <= 0.0 || opt.s_scale <= 0.0)
            throw std::invalid_argument("cabello: sample scales must be positive");

        const Polygon &cleaned = p;

        // A convex P is its own answer.
        if (cleaned.is_convex()) {
            res.polygon = cleaned;
            res.area = CGAL::abs(cleaned.area());
            CGAL::exact(res.area);
            res.convex = true;
            res.contained = true;
            return res;
        }

        const Poly q = to_inexact(cleaned);
        const double area_p = std::abs(CGAL::to_double(q.area()));
        if (area_p <= 0.0) return res;

        const VisibilityOracle oracle(q);
        const Triangulation tri = triangulate(q);
        if (tri.empty()) return res;

        // Line 1.
        const double A = a_bound(cleaned, opt.lower_bound, opt.should_stop);
        if (A <= 0.0) return res;
        // U = area(P) replaces the paper's C2*A(P). Every use of C2*A(P) only needs
        // an upper bound on A*(P), and area(P) is one, so C2 becomes U/A(P).
        const double U = area_p;
        const double C2 = U / A;
        const double C3 = c3(C2);

        // The paper's sizes. P is not rescaled to unit area, so area(P) appears
        // explicitly where the paper has 1.
        const double literal_r = 60.0 * area_p / A;                                     // line 2
        const double literal_r_ab = 96.0 * C1 * C2 / std::pow(opt.epsilon / 2.0, 1.5);  // line 9
        const double literal_s_ab = 288.0 * C2 / opt.epsilon;                           // line 10

        const std::size_t r = sized(literal_r, 1.0, 3);
        const std::size_t r_ab = sized(literal_r_ab, opt.sample_scale, 3);
        const std::size_t s_ab = sized(literal_s_ab, opt.s_scale, 1);

        // Line 4: repeat 3*log2(1/delta) times.
        const auto iterations
            = static_cast<std::size_t>(std::ceil(3.0 * std::log2(1.0 / opt.delta)));

        const double edge_budget = C3 * static_cast<double>(cleaned.size());

        double best_area = 0;
        std::vector<Pt> best_points;

        for (std::size_t it = 0; it < iterations; ++it) {
            check_stop(opt.should_stop);

            // Line 5.
            auto gen_r = substream(opt.seed, Stream::SampleR, it);
            const std::vector<Pt> R
                = sample_uniform(tri, r, gen_r, [&](const Pt &z) { return oracle.inside(z); });

            // Lines 6-7. With C3 around 10^7 the edge budget is never hit in
            // practice, but it bounds the worst case.
            const PointGraph g = visibility_graph(oracle, R);

            std::vector<std::pair<std::size_t, std::size_t>> edges;
            for (std::size_t i = 0; i < R.size(); ++i)
                for (std::size_t j = i + 1; j < R.size(); ++j)
                    if (g.edge(i, j)) edges.emplace_back(i, j);

            if (static_cast<double>(edges.size()) > edge_budget)
                continue;  // line 6 fails: this iteration does no work

            // Search a uniformly random subset of at most max_edges edges. The edge
            // list is built in a fixed order, so the subset depends only on the seed.
            if (opt.max_edges && edges.size() > *opt.max_edges) {
                auto gen_e = substream(opt.seed, Stream::EdgeSubsample, it);
                for (std::size_t k = 0; k < *opt.max_edges; ++k) {
                    const std::size_t pick = k + gen_e() % (edges.size() - k);
                    std::swap(edges[k], edges[pick]);
                }
                edges.resize(*opt.max_edges);
            }

            // Lines 8-14. The calling thread works alongside threads - 1 others and
            // is the only one that calls should_stop; the others see `stopped`.
            std::vector<EdgeResult> per_edge(edges.size());
            const unsigned threads
                = opt.threads ? opt.threads : std::max(1u, std::thread::hardware_concurrency());
            std::atomic<std::size_t> next{0};
            std::atomic<bool> stopped{false};
            const auto work = [&](const bool caller) {
                for (;;) {
                    if (caller && opt.should_stop && opt.should_stop()) stopped = true;
                    if (stopped) return;
                    const std::size_t e = next.fetch_add(1);
                    if (e >= edges.size()) return;
                    per_edge[e] = search_edge(opt, oracle, R[edges[e].first], R[edges[e].second], U,
                                              r_ab, s_ab, it, e);
                }
            };
            std::vector<std::thread> pool;
            for (unsigned t = 1; t < threads; ++t) pool.emplace_back(work, false);
            try {
                work(true);
            } catch (...) {
                stopped = true;
                for (auto &th : pool) th.join();
                throw;
            }
            for (auto &th : pool) th.join();
            if (stopped) throw cancelled("stopped by should_stop");

            // Reduce in index order; strict > keeps the earliest edge on a tie.
            for (std::size_t e = 0; e < per_edge.size(); ++e) {
                const EdgeResult &er = per_edge[e];
                if (er.area > best_area) {
                    best_area = er.area;
                    best_points = er.points;
                }
            }

            // Early exit: U >= A*(P), so reaching (1-eps)*U is already good enough.
            // Checked between iterations only, so threads do not affect it.
            if (best_area >= (1.0 - opt.epsilon) * U) break;
        }

        std::vector<Point> exact_points;
        exact_points.reserve(best_points.size());
        for (const auto &z : best_points) exact_points.push_back(to_exact(z));

        const Certificate cert = certify(cleaned, exact_points);
        res.polygon = cert.polygon;
        res.area = cert.area;
        res.convex = cert.convex;
        res.contained = cert.contained;
        res.message = cert.message;
        return res;
    }

}  // namespace potato_peeling::internal::cabello
