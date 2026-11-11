// The method templates for polygons over other CGAL kernels give the same answers
// as the EPECK overloads.

#include <doctest/doctest.h>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Exact_rational.h>
#include <CGAL/Simple_cartesian.h>

#include <vector>

#include "../polygons.hpp"
#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;
namespace tp = test_polygons;

namespace {
    template <class K> CGAL::Polygon_2<K> convert(const pp::Polygon &p) {
        CGAL::Polygon_2<K> out;
        for (const pp::Point &v : p)
            out.push_back(typename K::Point_2(CGAL::to_double(v.x()), CGAL::to_double(v.y())));
        return out;
    }

    bool same(const pp::Skull &a, const pp::Skull &b) {
        return a.area == b.area && a.polygon == b.polygon;
    }

    // The EPECK answers, computed once for all kernels.
    struct Reference {
        pp::Polygon p, start;
        pp::Skull exact, mesh, vertex, approx, refined;
        pp::FT upper;
    };

    pp::RefineOptions short_refine() {
        pp::RefineOptions o;
        o.max_sweeps = 3;
        return o;
    }

    const std::vector<Reference> &references() {
        static const std::vector<Reference> refs = [] {
            std::vector<Reference> out;
            for (const pp::Polygon &p : {tp::l_shape(), tp::spiky(), tp::wobbly()}) {
                Reference r;
                r.p = p;
                r.exact = pp::exact_convex_skull(p);
                r.mesh = pp::mesh_convex_skull(p);
                r.vertex = pp::vertex_convex_skull(p);
                r.approx = pp::approximate_convex_skull(p);
                r.upper = pp::upper_bound(p);
                // a start inside P with double coordinates: Cabello's answer is made
                // of sample points, which are doubles
                r.start = r.approx.polygon;
                r.refined = pp::refine(p, r.start, short_refine());
                out.push_back(r);
            }
            return out;
        }();
        return refs;
    }

    template <class K> void check_kernel() {
        for (const Reference &r : references()) {
            const CGAL::Polygon_2<K> q = convert<K>(r.p);
            CHECK(pp::to_polygon(q) == r.p);
            CHECK(same(pp::exact_convex_skull(q), r.exact));
            CHECK(same(pp::mesh_convex_skull(q), r.mesh));
            CHECK(same(pp::vertex_convex_skull(q), r.vertex));
            CHECK(same(pp::approximate_convex_skull(q), r.approx));
            CHECK(pp::upper_bound(q) == r.upper);
            const CGAL::Polygon_2<K> start_k = convert<K>(r.start);
            REQUIRE(pp::to_polygon(start_k) == r.start);
            CHECK(same(pp::refine(q, start_k, short_refine()), r.refined));
            CHECK(same(pp::refine(q, r.start, short_refine()), r.refined));
            CHECK(same(pp::refine(r.p, start_k, short_refine()), r.refined));
            const pp::Mesh mesh{{r.p.vertices_begin(), r.p.vertices_end()}, pp::triangulate(r.p)};
            CHECK(same(pp::mesh_convex_skull(q, mesh), r.mesh));
        }
        const pp::Polygon l = tp::l_shape();
        CHECK(same(pp::convex_skull(convert<K>(l)), pp::convex_skull(l)));
    }
}  // namespace

TEST_CASE("Exact_predicates_inexact_constructions_kernel") {
    check_kernel<CGAL::Exact_predicates_inexact_constructions_kernel>();
}

TEST_CASE("Simple_cartesian<double>") {
    check_kernel<CGAL::Simple_cartesian<double>>();
}

TEST_CASE("Simple_cartesian<Exact_rational>") {
    check_kernel<CGAL::Simple_cartesian<CGAL::Exact_rational>>();
}

TEST_CASE("clockwise input over another kernel") {
    using K = CGAL::Exact_predicates_inexact_constructions_kernel;
    CGAL::Polygon_2<K> q = convert<K>(tp::comb());
    q.reverse_orientation();
    CHECK(same(pp::exact_convex_skull(q), pp::exact_convex_skull(tp::comb())));
}
