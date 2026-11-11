// Exceptions thrown inside the shared library are caught by their exact types (and
// their standard bases) in the caller.

#include <doctest/doctest.h>

#include <stdexcept>
#include <type_traits>
#include <vector>

#include "../polygons.hpp"
#include "potato_peeling/errors.hpp"
#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;
namespace tp = test_polygons;

TEST_CASE("invalid_polygon") {
    const std::vector<pp::Polygon> bad{
        tp::make({}),
        tp::make({{0, 0}, {1, 0}}),
        tp::make({{0, 0}, {1, 1}, {2, 2}}),
        tp::make({{0, 0}, {2, 2}, {2, 0}, {0, 2}}),
        tp::make({{1, 1}, {1, 1}, {1, 1}}),
    };
    for (const pp::Polygon &p : bad) {
        CHECK_THROWS_AS(pp::exact_convex_skull(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::mesh_convex_skull(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::vertex_convex_skull(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::approximate_convex_skull(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::convex_skull(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::upper_bound(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::longest_segment(p), pp::invalid_polygon);
        CHECK_THROWS_AS(pp::exact_convex_skull(p), std::invalid_argument);
    }
}

TEST_CASE("refine rejects a start outside P") {
    CHECK_THROWS_AS(pp::refine(tp::l_shape(), tp::make({{0, 0}, {2, 0}, {2, 2}, {0, 2}})),
                    pp::invalid_polygon);
}

TEST_CASE("refine rejects a non-convex start") {
    CHECK_THROWS_AS(pp::refine(tp::l_shape(), tp::make({{0, 0}, {2, 0}, {1, 0.5}, {2, 1}, {0, 1}})),
                    pp::invalid_polygon);
}

TEST_CASE("invalid mesh") {
    const pp::Polygon l = tp::l_shape();
    pp::Mesh mesh{{l.vertices_begin(), l.vertices_end()}, pp::triangulate(l)};
    mesh.triangles.pop_back();
    CHECK_THROWS_AS(pp::mesh_convex_skull(l, mesh), pp::invalid_polygon);
}

TEST_CASE("cancelled") {
    const pp::BaseOptions stop{[] { return true; }};
    CHECK_THROWS_AS(pp::exact_convex_skull(tp::comb(), stop), pp::cancelled);
    CHECK_THROWS_AS(pp::exact_convex_skull(tp::comb(), stop), std::runtime_error);
}

TEST_CASE("bad options throw std::invalid_argument") {
    pp::ApproximateOptions o;
    o.epsilon = 2;
    CHECK_THROWS_AS(pp::approximate_convex_skull(tp::comb(), o), std::invalid_argument);
    pp::ApproximateOptions e;
    e.max_edges = 0;
    CHECK_THROWS_AS(pp::approximate_convex_skull(tp::comb(), e), std::invalid_argument);
    pp::UpperBoundOptions u;
    u.rel_tol = -1;
    CHECK_THROWS_AS(pp::upper_bound(tp::comb(), u), std::invalid_argument);
}

TEST_CASE("error classes") {
    CHECK(std::is_base_of_v<std::invalid_argument, pp::invalid_polygon>);
    CHECK(std::is_base_of_v<std::logic_error, pp::certification_error>);
    CHECK(std::is_base_of_v<std::runtime_error, pp::cancelled>);
}
