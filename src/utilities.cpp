#include "potato_peeling/utilities.hpp"

#include <utility>

#include "internal/cabello/certify.hpp"
#include "internal/geometry.hpp"

namespace potato_peeling {
    namespace {
        void require_simple(const Polygon &p) {
            if (p.size() < 3 || !p.is_simple()) throw invalid_polygon("polygon is not simple");
        }
    }  // namespace

    bool is_convex_subset(const Polygon &p, const Polygon &q) {
        return internal::cabello::is_convex_subset(internal::clean_polygon(p), q);
    }

    bool segment_in_polygon(const Polygon &p, const Point &a, const Point &b) {
        return internal::cabello::segment_in_polygon(internal::clean_polygon(p), a, b);
    }

    std::vector<std::array<std::size_t, 3>> triangulate(const Polygon &p) {
        require_simple(p);
        const std::vector<Point> ring(p.vertices_begin(), p.vertices_end());
        auto triangles = internal::triangulate_polygon(ring);
        for (auto &t : triangles)
            if (CGAL::orientation(ring[t[0]], ring[t[1]], ring[t[2]]) == CGAL::CLOCKWISE)
                std::swap(t[1], t[2]);
        return triangles;
    }

    std::vector<bool> reflex_vertices(const Polygon &p) {
        require_simple(p);
        return internal::reflex_vertices_flags(p);
    }

}  // namespace potato_peeling
