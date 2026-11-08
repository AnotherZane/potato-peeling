#include "internal/geometry.hpp"

#include <CDT.h>

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

    std::vector<std::array<std::size_t, 3>> triangulate_polygon(const std::vector<Point> &ring) {
        std::vector<std::array<double, 2>> d;
        d.reserve(ring.size());
        for (const Point &p : ring) {
            const double x = CGAL::to_double(p.x()), y = CGAL::to_double(p.y());
            if (FT(x) != p.x() || FT(y) != p.y())
                throw invalid_polygon("triangulate_polygon: coordinate not exactly a double");
            d.push_back({x, y});
        }
        return triangulate_polygon(d);
    }
}  // namespace potato_peeling::internal
