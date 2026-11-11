// longest_segment is contained in P with both endpoints on its boundary, equals an
// exact search over the line through every pair of vertices (visible or not, a
// wider search than the library's), and no random chord between boundary points
// is longer.

#include <doctest/doctest.h>

#include <algorithm>
#include <random>
#include <vector>

#include <CGAL/intersections.h>

#include "../polygons.hpp"
#include "internal/common.hpp"
#include "internal/geometry.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;
namespace pi = potato_peeling::internal;
namespace tp = test_polygons;
using Vector = pp::Kernel::Vector_2;

namespace {
    // The longest run of P's interior along any line through two vertices
    // (squared length).
    pp::FT widest(const pp::Polygon &p) {
        pp::FT best(0);
        const std::size_t n = p.size();
        for (std::size_t i = 0; i < n; ++i)
            for (std::size_t j = i + 1; j < n; ++j) {
                const pp::Point &o = p[i];
                const Vector d = p[j] - o;
                std::vector<pp::FT> ts;
                for (std::size_t e = 0; e < n; ++e) {
                    const auto hit = CGAL::intersection(pp::Kernel::Line_2(o, o + d),
                                                        pp::Segment(p[e], p[(e + 1) % n]));
                    if (!hit) continue;
                    if (const pp::Point *z = pi::get_if<pp::Point>(*hit)) {
                        ts.push_back((*z - o) * d / (d * d));
                    } else if (const pp::Segment *s = pi::get_if<pp::Segment>(*hit)) {
                        ts.push_back((s->source() - o) * d / (d * d));
                        ts.push_back((s->target() - o) * d / (d * d));
                    }
                }
                std::sort(ts.begin(), ts.end());
                ts.erase(std::unique(ts.begin(), ts.end()), ts.end());
                const auto inside = [&](std::size_t k) {
                    return p.bounded_side(o + ((ts[k] + ts[k + 1]) / 2) * d)
                           != CGAL::ON_UNBOUNDED_SIDE;
                };
                // every maximal run of consecutive inside intervals
                std::size_t k = 0;
                while (k + 1 < ts.size()) {
                    if (!inside(k)) {
                        ++k;
                        continue;
                    }
                    std::size_t m = k;
                    while (m + 1 < ts.size() && inside(m)) ++m;
                    best = std::max(best, (ts[m] - ts[k]) * (ts[m] - ts[k]) * (d * d));
                    k = m;
                }
            }
        return best;
    }

    void check(const std::vector<pp::Polygon> &polygons) {
        std::mt19937 gen(100);
        for (const pp::Polygon &p : polygons) {
            const pp::Segment s = pp::longest_segment(p);
            const pp::FT len2 = s.squared_length();
            CHECK(pp::segment_in_polygon(p, s.source(), s.target()));
            // compared to bool first: doctest would print a CGAL enum through
            // whatever operator<< it converts to (on some CGAL/Boost versions
            // Boost's __float128 one, which needs libquadmath)
            const bool ends_on_boundary = p.bounded_side(s.source()) == CGAL::ON_BOUNDARY
                                          && p.bounded_side(s.target()) == CGAL::ON_BOUNDARY;
            CHECK(ends_on_boundary);
            CHECK(widest(p) == len2);
            const auto on_boundary = [&] {
                const std::size_t e = gen() % p.size();
                const pp::FT t = pp::FT(static_cast<int>(gen() % 1000)) / pp::FT(1000);
                return p[e] + t * (p[(e + 1) % p.size()] - p[e]);
            };
            for (int r = 0; r < 200; ++r) {
                const pp::Point a = on_boundary(), b = on_boundary();
                if (CGAL::squared_distance(a, b) > len2) CHECK(!pp::segment_in_polygon(p, a, b));
            }
        }
    }
}  // namespace

TEST_CASE("longest_segment: hand-made polygons") {
    check({tp::l_shape(), tp::comb(), tp::spiky(), tp::wobbly()});
}

TEST_CASE("longest_segment: random grid stars") {
    check(tp::grid_stars(2, 300, 9, 16, 16, pi::clean_polygon));
}

TEST_SUITE("slow") {
    TEST_CASE("longest_segment: many random grid stars") {
        check(tp::grid_stars(12, 2000, 9, 30, 30, pi::clean_polygon));
    }
}
