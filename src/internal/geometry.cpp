#include "internal/geometry.hpp"

#include <CDT.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

namespace potato_peeling::internal {
    std::vector<bool> reflex_vertices_flags(const Polygon &polygon) {
        const std::size_t n = polygon.size();

        std::vector<bool> reflex_flags(n, false);

        const bool is_ccw = polygon.is_counterclockwise_oriented();
        const CGAL::Orientation reflex_turn = is_ccw ? CGAL::RIGHT_TURN : CGAL::LEFT_TURN;

        for (std::size_t i = 0; i < n; ++i) {
            const Point &prev = polygon[(i + n - 1) % n];
            const Point &curr = polygon[i];
            const Point &next = polygon[(i + 1) % n];

            if (CGAL::orientation(prev, curr, next) == reflex_turn) {
                reflex_flags[i] = true;
            }
        }

        return reflex_flags;
    }

    std::vector<Point> convex_hull(std::vector<Point> pts) {
        std::sort(pts.begin(), pts.end());
        pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
        if (pts.size() < 3) return pts;
        std::vector<Point> h;
        const auto add = [&h](const std::size_t base, const Point &q) {
            while (h.size() >= base + 2
                   && CGAL::orientation(h[h.size() - 2], h.back(), q) != CGAL::LEFT_TURN)
                h.pop_back();
            h.push_back(q);
        };
        for (const auto &q : pts) add(0, q);
        h.pop_back();
        const std::size_t upper = h.size();
        for (auto it = pts.rbegin(); it != pts.rend(); ++it) add(upper, *it);
        h.pop_back();
        return h;
    }

    double nearest_double(const FT &v) {
        // Within an ulp of v whatever the lazy approximation holds. Each step below
        // must get strictly closer, so it ends; out of range, inf is the answer.
        double d = CGAL::to_double(CGAL::exact(v));
        if (!std::isfinite(d)) return d;
        const auto distance = [&](const double x) { return CGAL::abs(FT(x) - v); };
        for (const double toward :
             {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
            for (;;) {
                const double next = std::nextafter(d, toward);
                if (!std::isfinite(next)) break;
                const CGAL::Comparison_result c = CGAL::compare(distance(next), distance(d));
                if (c == CGAL::SMALLER) {
                    d = next;
                } else {
                    if (c == CGAL::EQUAL) {
                        std::uint64_t bits;
                        std::memcpy(&bits, &d, sizeof bits);
                        if (bits & 1) d = next;
                    }
                    break;
                }
            }
        }
        return d;
    }

    void remove_collinear_vertices(Polygon &polygon) {
        bool changed = true;

        while (changed) {
            changed = false;

            const std::size_t n = polygon.size();
            if (n <= 3) {
                return;
            }

            Polygon cleaned;

            for (std::size_t i = 0; i < n; ++i) {
                const Point &prev = polygon[(i + n - 1) % n];
                const Point &curr = polygon[i];
                const Point &next = polygon[(i + 1) % n];

                const bool removable = CGAL::collinear(prev, curr, next)
                                       && CGAL::collinear_are_ordered_along_line(prev, curr, next);

                if (!removable) {
                    cleaned.push_back(curr);
                } else {
                    changed = true;
                }
            }

            polygon = cleaned;
        }
    }

    Polygon counterclockwise(const Polygon &p) {
        Polygon q = p;
        if (q.is_clockwise_oriented()) q.reverse_orientation();
        return q;
    }

    FT path_area(const std::vector<Point> &path) {
        if (path.size() < 3) return FT(0);
        FT s{0};
        for (std::size_t i = 0; i < path.size(); ++i) {
            const Point &a = path[i];
            const Point &b = path[(i + 1) % path.size()];
            s += a.x() * b.y() - b.x() * a.y();
        }
        return CGAL::abs(s) / FT(2);
    }

    Polygon remove_repeated_vertices(const Polygon &input) {
        std::vector<Point> pts;
        pts.reserve(input.size());
        for (const Point &v : input)
            if (pts.empty() || v != pts.back()) pts.push_back(v);
        while (pts.size() > 1 && pts.front() == pts.back()) pts.pop_back();
        return {pts.begin(), pts.end()};
    }

    Polygon clean_polygon(const Polygon &input) {
        Polygon poly = remove_repeated_vertices(input);
        if (poly.size() < 3) throw invalid_polygon("polygon has fewer than 3 distinct vertices");
        if (!poly.is_simple()) throw invalid_polygon("polygon is not simple");
        remove_collinear_vertices(poly);
        if (poly.size() < 3 || !poly.is_simple())
            throw invalid_polygon("polygon is degenerate after removing collinear vertices");
        return poly;
    }

    std::vector<std::array<std::size_t, 3>>
    triangulate_polygon(const std::vector<std::array<double, 2>> &ring) {
        const std::size_t n = ring.size();
        CDT::Triangulation<double> cdt(CDT::VertexInsertionOrder::AsProvided);
        std::vector<CDT::V2d<double>> vertices;
        vertices.reserve(n);
        for (const auto &p : ring) vertices.emplace_back(p[0], p[1]);
        cdt.insertVertices(vertices);
        std::vector<CDT::Edge> edges;
        edges.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            edges.emplace_back(static_cast<CDT::VertInd>(i),
                               static_cast<CDT::VertInd>((i + 1) % n));
        cdt.insertEdges(edges);
        cdt.eraseOuterTrianglesAndHoles();
        if (cdt.vertices.size() != n)
            throw invalid_polygon(
                "triangulate_polygon: the vertex set changed (degenerate polygon)");

        std::vector<std::array<std::size_t, 3>> out;
        out.reserve(cdt.triangles.size());
        for (const auto &t : cdt.triangles)
            out.push_back({t.vertices[0], t.vertices[1], t.vertices[2]});
        return out;
    }

    namespace {
        // Twice the signed area of a closed path (positive when counter-clockwise).
        FT path_area_signed(const std::vector<Point> &path) {
            FT s{0};
            for (std::size_t i = 0; i < path.size(); ++i) {
                const Point &a = path[i];
                const Point &b = path[(i + 1) % path.size()];
                s += a.x() * b.y() - b.x() * a.y();
            }
            return s;
        }
    }  // namespace

    std::vector<std::array<std::size_t, 3>> triangulate_polygon(const std::vector<Point> &ring) {
        std::vector<std::array<double, 2>> d;
        d.reserve(ring.size());
        bool exact = true;
        for (const Point &p : ring) {
            const double x = nearest_double(p.x()), y = nearest_double(p.y());
            if (!std::isfinite(x) || !std::isfinite(y)) return ear_clip(ring);
            exact = exact && FT(x) == p.x() && FT(y) == p.y();
            d.push_back({x, y});
        }
        if (exact) return triangulate_polygon(d);
        try {
            auto t = triangulate_polygon(d);
            if (tiles_polygon(ring, t)) return t;
        } catch (const invalid_polygon &) {
            // rounding merged or crossed vertices
        }
        return ear_clip(ring);
    }

    bool tiles_polygon(const std::vector<Point> &ring,
                       const std::vector<std::array<std::size_t, 3>> &triangles) {
        const std::size_t n = ring.size();
        const bool ccw = path_area_signed(ring) > 0;
        std::set<std::pair<std::size_t, std::size_t>> directed;
        for (std::array<std::size_t, 3> t : triangles) {
            for (const std::size_t v : t)
                if (v >= n) return false;
            const CGAL::Orientation o = CGAL::orientation(ring[t[0]], ring[t[1]], ring[t[2]]);
            if (o == CGAL::COLLINEAR) return false;
            if (o == CGAL::CLOCKWISE) std::swap(t[1], t[2]);
            for (int k = 0; k < 3; ++k)
                if (!directed.insert({t[k], t[(k + 1) % 3]}).second) return false;
        }
        std::set<std::pair<std::size_t, std::size_t>> boundary;
        for (const auto &[a, b] : directed)
            if (!directed.count({b, a})) boundary.insert({a, b});
        if (boundary.size() != n) return false;
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t j = (i + 1) % n;
            if (!boundary.count(ccw ? std::make_pair(i, j) : std::make_pair(j, i))) return false;
        }
        return true;
    }

    std::vector<std::array<std::size_t, 3>> ear_clip(const std::vector<Point> &ring) {
        std::vector<std::size_t> left(ring.size());
        for (std::size_t i = 0; i < left.size(); ++i) left[i] = i;
        if (path_area_signed(ring) < 0) std::reverse(left.begin(), left.end());
        std::vector<std::array<std::size_t, 3>> out;
        while (left.size() > 3) {
            const std::size_t m = left.size();
            bool clipped = false;
            for (std::size_t k = 0; k < m && !clipped; ++k) {
                const std::size_t a = left[(k + m - 1) % m], b = left[k], c = left[(k + 1) % m];
                if (CGAL::orientation(ring[a], ring[b], ring[c]) != CGAL::LEFT_TURN) continue;
                // an ear: no other remaining vertex in the closed triangle
                bool empty = true;
                for (const std::size_t v : left) {
                    if (v == a || v == b || v == c) continue;
                    if (CGAL::orientation(ring[a], ring[b], ring[v]) != CGAL::RIGHT_TURN
                        && CGAL::orientation(ring[b], ring[c], ring[v]) != CGAL::RIGHT_TURN
                        && CGAL::orientation(ring[c], ring[a], ring[v]) != CGAL::RIGHT_TURN) {
                        empty = false;
                        break;
                    }
                }
                if (!empty) continue;
                out.push_back({a, b, c});
                left.erase(left.begin() + static_cast<std::ptrdiff_t>(k));
                clipped = true;
            }
            if (!clipped) throw invalid_polygon("ear_clip: no ear (polygon not simple)");
        }
        if (CGAL::orientation(ring[left[0]], ring[left[1]], ring[left[2]]) == CGAL::COLLINEAR)
            throw invalid_polygon("ear_clip: degenerate polygon");
        out.push_back({left[0], left[1], left[2]});
        return out;
    }
}  // namespace potato_peeling::internal
