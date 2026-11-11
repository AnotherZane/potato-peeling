// simple.hpp is usable without CGAL: including it pulls in no CGAL header.

#include "potato_peeling/simple.hpp"

#ifdef CGAL_VERSION_NR
#error "simple.hpp includes a CGAL header"
#endif

#include <doctest/doctest.h>

TEST_CASE("simple.hpp alone") {
    const potato_peeling::simple::Ring triangle{{0, 0}, {2, 0}, {0, 2}};
    CHECK(potato_peeling::simple::exact_convex_skull(triangle).area == 2.0);
}
