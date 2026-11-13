#include "internal/refinement/halfplanes.hpp"

#include "internal/geometry.hpp"

namespace potato_peeling::internal::refinement {

    namespace {
        // Clip a CONVEX path by one half-plane. Exact.
        std::vector<Point> clip_convex(const std::vector<Point> &pts, const HalfPlane &h) {
            std::vector<Point> out;
            out.reserve(pts.size() + 2);
            const std::size_t n = pts.size();
            for (std::size_t i = 0; i < n; ++i) {
                const Point &p = pts[i];
                const Point &q = pts[(i + 1) % n];
                const FT sp = h.a * p.x() + h.b * p.y() - h.c;
                const FT sq = h.a * q.x() + h.b * q.y() - h.c;
                if (sp <= FT(0)) out.push_back(p);
                if ((sp < FT(0) && sq > FT(0)) || (sq < FT(0) && sp > FT(0))) {
                    const FT t = sp / (sp - sq);
                    out.emplace_back(p.x() + t * (q.x() - p.x()), p.y() + t * (q.y() - p.y()));
                }
            }
            return out;
        }
    }  // namespace

    std::vector<HalfPlane> halfplanes_of(const Polygon &q) {
        std::vector<HalfPlane> hp;
        const std::size_t n = q.size();
        hp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            const Point &p = q[i];
            const Point &r = q[(i + 1) % n];
            const FT ex = r.x() - p.x(), ey = r.y() - p.y();
            const FT a = ey, b = -ex;  // outward for CCW
            hp.push_back({a, b, a * p.x() + b * p.y()});
        }
        return hp;
    }

    std::vector<Point> realise(const std::vector<HalfPlane> &hp, const std::vector<Point> &box) {
        std::vector<Point> pts = box;
        for (const auto &h : hp) {
            pts = clip_convex(pts, h);
            if (pts.size() < 3) return {};
        }
        std::vector<Point> out;
        out.reserve(pts.size());
        for (const auto &p : pts)
            if (out.empty() || p != out.back()) out.push_back(p);
        if (out.size() > 1 && out.front() == out.back()) out.pop_back();
        return out;
    }

    std::size_t prune(std::vector<HalfPlane> &hp, const std::vector<Point> &box) {
        const FT area = path_area(realise(hp, box));
        std::size_t dropped = 0;
        for (std::size_t i = hp.size(); i-- > 0;) {
            if (hp.size() <= 3) break;
            std::vector<HalfPlane> without;
            without.reserve(hp.size() - 1);
            for (std::size_t k = 0; k < hp.size(); ++k)
                if (k != i) without.push_back(hp[k]);
            if (path_area(realise(without, box)) == area) {
                hp.swap(without);
                ++dropped;
            }
        }
        return dropped;
    }

    Point rotation_anchor(const std::vector<Point> &pts, const HalfPlane &h) {
        std::vector<Point> on;
        for (const auto &p : pts)
            if (h.a * p.x() + h.b * p.y() == h.c) on.push_back(p);
        if (on.size() >= 2) return CGAL::midpoint(on.front(), on.back());
        if (on.size() == 1) return on.front();

        FT cx{0}, cy{0};
        for (const auto &p : pts) {
            cx += p.x();
            cy += p.y();
        }
        const FT k(static_cast<int>(pts.size()));
        cx /= k;
        cy /= k;
        const FT nn = h.a * h.a + h.b * h.b;
        if (nn == FT(0)) return {cx, cy};
        const FT t = (h.c - h.a * cx - h.b * cy) / nn;
        return {cx + h.a * t, cy + h.b * t};
    }

}  // namespace potato_peeling::internal::refinement
