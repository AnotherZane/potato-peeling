#include "internal/cabello/sampling.hpp"
#include "internal/cabello/rng.hpp"
#include "internal/geometry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace potato_peeling::internal::cabello {
    Triangulation triangulate(const Poly &p) {
        Triangulation out;
        if (p.size() < 3) return out;

        std::vector<std::array<double, 2>> ring;
        ring.reserve(p.size());
        for (const Pt &v : p.container()) ring.push_back({v.x(), v.y()});

        double running = 0;
        for (const auto &f : triangulate_polygon(ring)) {
            const std::array<Pt, 3> t{p[f[0]], p[f[1]], p[f[2]]};
            const double a = std::abs(CGAL::to_double(CGAL::area(t[0], t[1], t[2])));
            if (a <= 0) continue;  // a zero-area face carries no probability mass
            running += a;
            out.triangles.push_back(t);
            out.prefix.push_back(running);
        }
        out.total_area = running;
        return out;
    }

    Pt sample_in_triangle(const std::array<Pt, 3> &t, std::mt19937_64 &gen) {
        // [C17 p14]: sample the parallelogram spanned by two edges, which holds two
        // congruent copies of the triangle, and reflect the far copy back.
        double u = canonical(gen);
        double v = canonical(gen);
        if (u + v > 1.0) {
            u = 1.0 - u;
            v = 1.0 - v;
        }
        const Vec e1 = t[1] - t[0];
        const Vec e2 = t[2] - t[0];
        return t[0] + u * e1 + v * e2;
    }

    Pt sample_uniform(const Triangulation &tri, std::mt19937_64 &gen) {
        if (tri.empty()) throw std::invalid_argument("sample_uniform: empty triangulation");

        const double x = canonical(gen) * tri.total_area;
        // Smallest j with x <= prefix[j], the paper's binary search.
        const auto it = std::lower_bound(tri.prefix.begin(), tri.prefix.end(), x);
        const std::size_t j = std::min<std::size_t>(
            static_cast<std::size_t>(it - tri.prefix.begin()), tri.size() - 1);
        return sample_in_triangle(tri.triangles[j], gen);
    }

    std::vector<Pt> sample_uniform(const Triangulation &tri, const std::size_t count,
                                   std::mt19937_64 &gen,
                                   const std::function<bool(const Pt &)> &inside) {
        std::vector<Pt> out;
        out.reserve(count);

        // t0 + u*e1 + v*e2 lies in the triangle exactly, but its rounded value can
        // land just outside P. Such a point would fail every visibility test, so
        // it is re-drawn from the same substream.
        std::size_t wasted = 0;
        while (out.size() < count) {
            Pt p = sample_uniform(tri, gen);
            if (inside(p)) {
                out.push_back(p);
            } else if (++wasted > 64 + 4 * count) {
                throw std::runtime_error(
                    "sample_uniform: too many points fell outside P; the triangulation and "
                    "the containment predicate disagree");
            }
        }
        return out;
    }

    std::optional<Parallelogram> gamma(const Pt &a, const Pt &b, const double A) {
        const double dy = CGAL::to_double(a.y() - b.y());
        if (dy == 0.0) return std::nullopt;  // Gamma is undefined

        // [C17 p8]. a' = 2a - b, b' = 2b - a, translated horizontally by
        // +-d with d = 2A/|y(a) - y(b)|.
        const double d = 2.0 * A / std::abs(dy);
        const Pt ap = a + (a - b);  // 2a - b
        const Pt bp = b + (b - a);  // 2b - a

        Parallelogram g;
        g.origin = Pt(ap.x() - d, ap.y());
        g.e1 = Vec(2.0 * d, 0.0);  // a'-(d,0) -> a'+(d,0)
        g.e2 = bp - ap;            // = 3(b - a)
        // |e1 x e2| = 2d * 3|y(b)-y(a)| = 12A, by construction.
        g.area = std::abs(CGAL::to_double(g.e1.x() * g.e2.y() - g.e1.y() * g.e2.x()));
        return g;
    }

    Pt sample_uniform(const Parallelogram &g, std::mt19937_64 &gen) {
        const double u = canonical(gen);
        const double v = canonical(gen);
        return g.origin + u * g.e1 + v * g.e2;
    }

}  // namespace potato_peeling::internal::cabello
