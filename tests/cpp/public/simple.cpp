// simple.hpp: every value is the exact answer correctly rounded to nearest (ties
// to even); upper_bound rounds up. Checked against the exact API with exact
// arithmetic, independently of the library's rounding code.

#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

#include "../polygons.hpp"
#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/simple.hpp"

namespace pp = potato_peeling;
namespace ps = potato_peeling::simple;
namespace tp = test_polygons;

namespace {
    bool even_mantissa(double d) {
        std::uint64_t bits;
        std::memcpy(&bits, &d, sizeof bits);
        return (bits & 1u) == 0;
    }

    // d is the double nearest x, ties to even.
    bool nearest(const pp::FT &x, double d) {
        if (!std::isfinite(d)) return false;
        const pp::FT e = CGAL::abs(x - pp::FT(d));
        for (const double n : {std::nextafter(d, -INFINITY), std::nextafter(d, INFINITY)}) {
            const pp::FT f = CGAL::abs(x - pp::FT(n));
            if (f < e || (f == e && !even_mantissa(d))) return false;
        }
        return true;
    }

    ps::Ring ring(const pp::Polygon &p) {
        ps::Ring r;
        for (const pp::Point &v : p) r.push_back({CGAL::to_double(v.x()), CGAL::to_double(v.y())});
        return r;
    }

    void check(const pp::Skull &exact, const ps::SimpleSkull &simple) {
        REQUIRE(simple.polygon.size() == exact.polygon.size());
        CHECK(nearest(exact.area, simple.area));
        for (std::size_t i = 0; i < exact.polygon.size(); ++i) {
            CHECK(nearest(exact.polygon[i].x(), simple.polygon[i][0]));
            CHECK(nearest(exact.polygon[i].y(), simple.polygon[i][1]));
        }
    }
}  // namespace

TEST_CASE("nearest() itself") {
    CHECK(nearest(pp::FT(1) / pp::FT(3), 1.0 / 3.0));
    CHECK(!nearest(pp::FT(1) / pp::FT(3), std::nextafter(1.0 / 3.0, 1.0)));
    // 1 + 2^-53 lies halfway between 1 and its successor: ties to even gives 1
    const pp::FT half_ulp = pp::FT(1) + pp::FT(std::ldexp(1.0, -53));
    CHECK(nearest(half_ulp, 1.0));
    CHECK(!nearest(half_ulp, std::nextafter(1.0, 2.0)));
}

TEST_CASE("simple results are the exact results rounded to nearest") {
    for (const pp::Polygon &p : {tp::l_shape(), tp::comb(), tp::spiky(), tp::wobbly()}) {
        const ps::Ring r = ring(p);
        check(pp::exact_convex_skull(p), ps::exact_convex_skull(r));
        check(pp::mesh_convex_skull(p), ps::mesh_convex_skull(r));
        check(pp::vertex_convex_skull(p), ps::vertex_convex_skull(r));
        check(pp::approximate_convex_skull(p), ps::approximate_convex_skull(r));
        const pp::Polygon start = pp::approximate_convex_skull(p).polygon;
        pp::RefineOptions o;
        o.max_sweeps = 3;
        check(pp::refine(p, start, o), ps::refine(r, ring(start), o));
    }
    check(pp::convex_skull(tp::l_shape()), ps::convex_skull(ring(tp::l_shape())));
}

TEST_CASE("simple mesh_convex_skull with a caller's mesh") {
    const ps::Ring square{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    const ps::Mesh mesh{{{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0.5, 0}},
                        {{0, 4, 3}, {4, 1, 2}, {4, 2, 3}}};
    CHECK(ps::mesh_convex_skull(square, mesh).area == 1.0);
}

TEST_CASE("simple upper_bound rounds up") {
    for (const pp::Polygon &p : {tp::l_shape(), tp::spiky(), tp::wobbly()}) {
        const pp::FT u = pp::upper_bound(p);
        const double d = ps::upper_bound(ring(p));
        CHECK(pp::FT(d) >= u);
        CHECK(pp::FT(std::nextafter(d, -INFINITY)) < u);
    }
}

TEST_CASE("non-finite coordinates throw invalid_polygon") {
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS(ps::exact_convex_skull({{0, 0}, {1, 0}, {nan, 1}}), pp::invalid_polygon);
    CHECK_THROWS_AS(ps::convex_skull({{0, 0}, {inf, 0}, {0, 1}}), pp::invalid_polygon);
    CHECK_THROWS_AS(ps::upper_bound({{0, 0}, {1, 0}, {0, -inf}}), pp::invalid_polygon);
}
