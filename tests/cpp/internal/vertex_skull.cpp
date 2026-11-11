// vertex_convex_skull equals a brute force over every subset of P's vertices.

#include <doctest/doctest.h>

#include <vector>

#include "../polygons.hpp"
#include "internal/geometry.hpp"
#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;
namespace pi = potato_peeling::internal;
namespace tp = test_polygons;

namespace {
    // The hull of every vertex subset that lies in P; the largest area.
    pp::FT brute(const pp::Polygon &p) {
        const std::size_t n = p.size();
        pp::FT best(0);
        for (unsigned long mask = 7; mask < (1ul << n); ++mask) {
            std::vector<pp::Point> pts;
            for (std::size_t i = 0; i < n; ++i)
                if (mask >> i & 1) pts.push_back(p[i]);
            const std::vector<pp::Point> hull = pi::convex_hull(pts);
            if (hull.size() < 3) continue;
            const pp::Polygon q(hull.begin(), hull.end());
            const pp::FT a = q.area();
            if (a > best && pp::is_convex_subset(p, q)) best = a;
        }
        return best;
    }

    void check(const std::vector<pp::Polygon> &polygons) {
        for (const pp::Polygon &p : polygons) {
            const pp::Skull s = pp::vertex_convex_skull(p);
            CHECK(s.area == brute(p));
            CHECK(pp::is_convex_subset(p, s.polygon));
            CHECK(s.area >= pp::mesh_convex_skull(p).area);
        }
    }
}  // namespace

TEST_CASE("vertex_convex_skull: hand-made polygons") {
    check({tp::l_shape(), tp::comb(), tp::spiky(), tp::wobbly()});
}

TEST_CASE("vertex_convex_skull: random grid stars") {
    check(tp::grid_stars(1, 300, 7, 12, 12, pi::clean_polygon));
}

TEST_SUITE("slow") {
    TEST_CASE("vertex_convex_skull: many random grid stars") {
        check(tp::grid_stars(11, 3000, 7, 15, 16, pi::clean_polygon));
    }
}
