#include "internal/cabello/vertex_skull.hpp"

#include <cstddef>
#include <vector>

#include "internal/cabello/certify.hpp"
#include "internal/cabello/convex_clique.hpp"
#include "internal/cabello/graph.hpp"
#include "internal/cabello/kernel.hpp"

namespace potato_peeling::internal::cabello {

    Polygon vertex_convex_skull(const Polygon &p, const StopFn &stop) {
        if (p.is_convex()) {
            Polygon out = p;
            if (out.is_clockwise_oriented()) out.reverse_orientation();
            return out;
        }

        const std::size_t n = p.size();
        std::vector<Pt> pts;
        pts.reserve(n);
        for (const Point &v : p) pts.push_back(to_inexact(v));
        PointGraph h(pts);
        for (std::size_t i = 0; i < n; ++i) {
            check_stop(stop);
            for (std::size_t j = i + 1; j < n; ++j)
                if (segment_in_polygon(p, p[i], p[j])) h.add_edge(i, j);
        }

        Clique best;
        for (std::size_t s = 0; s < n; ++s) {
            check_stop(stop);
            if (Clique u = phi(h, s); u.area > best.area) best = std::move(u);
        }

        std::vector<Point> verts;
        verts.reserve(best.vertices.size());
        for (const std::size_t v : best.vertices) verts.push_back(p[v]);
        const Certificate cert = certify(p, verts);
        if (!cert.ok()) throw certification_error("vertex_convex_skull: " + cert.message);
        return cert.polygon;
    }

}  // namespace potato_peeling::internal::cabello
