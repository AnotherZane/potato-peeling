// mesh_convex_skull over a mesh equals a brute force over every subset of its
// triangles. The triangles are interior-disjoint, so a subset's union is convex
// exactly when the hull of its vertices has the same area as the subset.

#include <doctest/doctest.h>

#include <array>
#include <map>
#include <vector>

#include "../polygons.hpp"
#include "internal/geometry.hpp"
#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;
namespace pi = potato_peeling::internal;
namespace tp = test_polygons;

namespace {
    pp::FT triangle_area(const pp::Mesh &m, const std::array<std::size_t, 3> &t) {
        return CGAL::abs(CGAL::area(m.points[t[0]], m.points[t[1]], m.points[t[2]]));
    }

    pp::FT brute(const pp::Mesh &m) {
        const std::size_t n = m.triangles.size();
        pp::FT best(0);
        for (unsigned long mask = 1; mask < (1ul << n); ++mask) {
            pp::FT sum(0);
            std::vector<pp::Point> pts;
            for (std::size_t i = 0; i < n; ++i)
                if (mask >> i & 1) {
                    sum += triangle_area(m, m.triangles[i]);
                    for (const std::size_t v : m.triangles[i]) pts.push_back(m.points[v]);
                }
            if (sum <= best) continue;
            const std::vector<pp::Point> hull = pi::convex_hull(pts);
            if (pp::Polygon(hull.begin(), hull.end()).area() == sum) best = sum;
        }
        return best;
    }

    pp::Mesh cdt(const pp::Polygon &p) {
        return {{p.vertices_begin(), p.vertices_end()}, pp::triangulate(p)};
    }

    pp::Mesh centroid_split(const pp::Mesh &m) {
        pp::Mesh out{m.points, {}};
        for (const auto &[a, b, c] : m.triangles) {
            out.points.push_back(CGAL::centroid(m.points[a], m.points[b], m.points[c]));
            const std::size_t g = out.points.size() - 1;
            out.triangles.push_back({a, b, g});
            out.triangles.push_back({b, c, g});
            out.triangles.push_back({c, a, g});
        }
        return out;
    }

    pp::Mesh midpoint_split(const pp::Mesh &m) {
        pp::Mesh out{m.points, {}};
        std::map<std::pair<std::size_t, std::size_t>, std::size_t> mid;
        const auto at = [&](std::size_t i, std::size_t j) {
            const auto key = std::minmax(i, j);
            const auto it = mid.find(key);
            if (it != mid.end()) return it->second;
            out.points.push_back(CGAL::midpoint(m.points[i], m.points[j]));
            return mid[key] = out.points.size() - 1;
        };
        for (const auto &[a, b, c] : m.triangles) {
            const std::size_t ab = at(a, b), bc = at(b, c), ca = at(c, a);
            out.triangles.push_back({a, ab, ca});
            out.triangles.push_back({ab, b, bc});
            out.triangles.push_back({ca, bc, c});
            out.triangles.push_back({ab, bc, ca});
        }
        return out;
    }

    void check(const std::vector<pp::Polygon> &polygons, std::size_t max_triangles) {
        for (const pp::Polygon &p : polygons) {
            const pp::Mesh base = cdt(p);
            const pp::FT coarse = pp::mesh_convex_skull(p, base).area;
            CHECK(coarse == pp::mesh_convex_skull(p).area);
            for (const pp::Mesh &m : {base, centroid_split(base), midpoint_split(base)}) {
                if (m.triangles.size() > max_triangles) continue;
                const pp::Skull s = pp::mesh_convex_skull(p, m);
                CHECK(s.area == brute(m));
                CHECK(s.area >= coarse);
                CHECK(pp::is_convex_subset(p, s.polygon));
            }
        }
    }
}  // namespace

TEST_CASE("mesh_convex_skull: hand-made polygons") {
    check({tp::l_shape(), tp::spiky(), tp::wobbly()}, 16);
}

TEST_CASE("mesh_convex_skull: random grid stars") {
    check(tp::grid_stars(3, 100, 7, 8, 7, pi::clean_polygon), 15);
}

TEST_SUITE("slow") {
    TEST_CASE("mesh_convex_skull: many random grid stars") {
        check(tp::grid_stars(13, 500, 9, 10, 9, pi::clean_polygon), 20);
    }
}

TEST_CASE("triangulation of coordinates that are not doubles") {
    // P / 3 shifted by 1/7: CDT on rounded values, checked exactly
    for (const pp::Polygon &p : {tp::l_shape(), tp::comb(), tp::spiky(), tp::wobbly()}) {
        pp::Polygon q;
        for (const pp::Point &v : p) q.push_back(pp::Point(v.x() / 3, v.y() / 3 + pp::FT(1) / 7));
        const std::vector<pp::Point> ring(q.vertices_begin(), q.vertices_end());
        const auto t = pi::triangulate_polygon(ring);
        CHECK(pi::tiles_polygon(ring, t));
        CHECK(pp::mesh_convex_skull(q).area * 9 == pp::mesh_convex_skull(p).area);
    }
}

TEST_CASE("ear_clip and tiles_polygon") {
    std::vector<pp::Polygon> polygons = tp::grid_stars(4, 200, 9, 20, 20, pi::clean_polygon);
    polygons.push_back(tp::wobbly());
    for (pp::Polygon p : polygons) {
        for (const bool reversed : {false, true}) {
            if (reversed) p.reverse_orientation();
            const std::vector<pp::Point> ring(p.vertices_begin(), p.vertices_end());
            const auto t = pi::ear_clip(ring);
            CHECK(t.size() == ring.size() - 2);
            CHECK(pi::tiles_polygon(ring, t));
            CHECK(pi::tiles_polygon(ring, pp::triangulate(p)));
            // broken triangulations are rejected
            auto gap = t;
            gap.pop_back();
            CHECK(!pi::tiles_polygon(ring, gap));
            auto twice = t;
            twice.push_back(t.front());
            CHECK(!pi::tiles_polygon(ring, twice));
            auto bad = t;
            bad.front()[0] = ring.size();
            CHECK(!pi::tiles_polygon(ring, bad));
        }
    }
}
