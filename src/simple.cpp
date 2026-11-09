#include "potato_peeling/simple.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

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

        // The double nearest to v, ties to even. CGAL's approximation of an exact
        // value is within an ulp or so; step while a neighbour is exactly closer.
        double nearest(const FT &v) {
            CGAL::exact(v);
            double d = CGAL::to_double(v);
            const auto distance = [&](const double x) { return CGAL::abs(FT(x) - v); };
            for (const double toward : {std::numeric_limits<double>::infinity(),
                                        -std::numeric_limits<double>::infinity()}) {
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

        SimpleSkull to_simple(const Skull &s) {
            SimpleSkull out;
            out.polygon.reserve(s.polygon.size());
            for (const Point &v : s.polygon)
                out.polygon.push_back({nearest(v.x()), nearest(v.y())});
            out.area = nearest(s.area);
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

    SimpleSkull approximate_convex_skull(const Ring &p, const ApproximateOptions &options) {
        return to_simple(potato_peeling::approximate_convex_skull(to_exact(p), options));
    }

    SimpleSkull refine(const Ring &p, const Ring &start, const RefineOptions &options) {
        return to_simple(potato_peeling::refine(to_exact(p), to_exact(start), options));
    }

}  // namespace potato_peeling::simple
