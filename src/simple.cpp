#include "potato_peeling/simple.hpp"

#include <cmath>

#include "internal/geometry.hpp"
#include "potato_peeling/potato_peeling.hpp"

namespace potato_peeling::simple {
    namespace {
        Polygon to_exact(const Ring &ring) {
            Polygon out;
            for (const auto &[x, y] : ring) {
                if (!std::isfinite(x) || !std::isfinite(y))
                    throw invalid_polygon("coordinate is not finite");
                out.push_back(Point(x, y));
            }
            return out;
        }

        SimpleSkull to_simple(const Skull &s) {
            SimpleSkull out;
            out.polygon.reserve(s.polygon.size());
            for (const Point &v : s.polygon)
                out.polygon.push_back(
                    {internal::nearest_double(v.x()), internal::nearest_double(v.y())});
            out.area = internal::nearest_double(s.area);
            return out;
        }
    }  // namespace

    SimpleSkull convex_skull(const Ring &p, const ConvexSkullOptions &options) {
        return to_simple(potato_peeling::convex_skull(to_exact(p), options));
    }

    SimpleSkull exact_convex_skull(const Ring &p) {
        return to_simple(potato_peeling::exact_convex_skull(to_exact(p)));
    }

    SimpleSkull mesh_convex_skull(const Ring &p) {
        return to_simple(potato_peeling::mesh_convex_skull(to_exact(p)));
    }

    SimpleSkull mesh_convex_skull(const Ring &p, const Mesh &mesh) {
        const Polygon points = to_exact(mesh.points);
        return to_simple(potato_peeling::mesh_convex_skull(
            to_exact(p), {{points.vertices_begin(), points.vertices_end()}, mesh.triangles}));
    }

    SimpleSkull approximate_convex_skull(const Ring &p, const ApproximateOptions &options) {
        return to_simple(potato_peeling::approximate_convex_skull(to_exact(p), options));
    }

    double upper_bound(const Ring &p, const UpperBoundOptions &options) {
        const FT bound = potato_peeling::upper_bound(to_exact(p), options);
        const double d = internal::nearest_double(bound);
        return FT(d) < bound ? std::nextafter(d, HUGE_VAL) : d;
    }

    SimpleSkull refine(const Ring &p, const Ring &start, const RefineOptions &options) {
        return to_simple(potato_peeling::refine(to_exact(p), to_exact(start), options));
    }

}  // namespace potato_peeling::simple
