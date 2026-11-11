#include "internal/longest_segment.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <CGAL/intersections.h>

#include "internal/cabello/certify.hpp"

namespace potato_peeling::internal {
    namespace {
        // Parameters t along o + t d where the line meets dP. An edge lying on the
        // line contributes both endpoints, so the walk below cannot run through it.
        std::vector<FT> boundary_params(const Polygon &p, const Point &o, const Vector &d) {
            const Line line(o, o + d);
            const FT dd = d.squared_length();
            const auto param = [&](const Point &z) { return ((z - o) * d) / dd; };
            std::vector<FT> ts;
            for (std::size_t e = 0; e < p.size(); ++e) {
                const auto hit = CGAL::intersection(line, Segment(p[e], p[(e + 1) % p.size()]));
                if (!hit) continue;
                if (const Point *z = get_if<Point>(*hit)) {
                    ts.push_back(param(*z));
                } else if (const Segment *s = get_if<Segment>(*hit)) {
                    ts.push_back(param(s->source()));
                    ts.push_back(param(s->target()));
                }
            }
            std::sort(ts.begin(), ts.end());
            ts.erase(std::unique(ts.begin(), ts.end()), ts.end());
            return ts;
        }
    }  // namespace

    Segment longest_segment(const Polygon &p) {
        const std::size_t n = p.size();
        Segment best(p[0], p[1]);
        FT best_len2 = best.squared_length();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                const Point &vi = p[i], &vj = p[j];
                if (!cabello::segment_in_polygon(p, vi, vj)) continue;
                const Vector d = vj - vi;
                const std::vector<FT> ts = boundary_params(p, vi, d);

                // Between consecutive crossings the line is wholly in or out of P,
                // so an interval is inside iff its midpoint is. [0, 1] (the two
                // vertices) is inside; grow from it while the neighbours are.
                const auto inside = [&](const std::size_t k) {
                    return cabello::point_in_polygon(p, vi + ((ts[k] + ts[k + 1]) / FT(2)) * d);
                };
                std::size_t lo = std::lower_bound(ts.begin(), ts.end(), FT(0)) - ts.begin();
                std::size_t hi = std::lower_bound(ts.begin(), ts.end(), FT(1)) - ts.begin();
                while (lo > 0 && inside(lo - 1)) --lo;
                while (hi + 1 < ts.size() && inside(hi)) ++hi;

                const Segment s(vi + ts[lo] * d, vi + ts[hi] * d);
                if (const FT len2 = s.squared_length(); len2 > best_len2) {
                    best = s;
                    best_len2 = len2;
                }
            }
        }
        return best;
    }

}  // namespace potato_peeling::internal
