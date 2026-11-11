# C++ tests, with doctest (MIT), for what the Python tests cannot reach.
#   test_public:   the shared library through its installed headers only
#   test_internal: brute-force oracles on the internals (links the objects)
# Test suites named "slow" run only under the ctest label slow:
#   ctest --test-dir build -LE slow    (default set)
#   ctest --test-dir build -L slow

find_package(doctest 2.4 CONFIG QUIET)
if (NOT TARGET doctest::doctest)
    include(FetchContent)
    FetchContent_Declare(doctest
            URL https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.tar.gz
            URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
            SOURCE_SUBDIR no-cmake-project)
    FetchContent_MakeAvailable(doctest)
    add_library(potato_peeling_doctest INTERFACE)
    target_include_directories(potato_peeling_doctest SYSTEM INTERFACE ${doctest_SOURCE_DIR})
    add_library(doctest::doctest ALIAS potato_peeling_doctest)
endif ()

add_executable(test_public
        main.cpp
        public/errors.cpp
        public/kernels.cpp
        public/simple.cpp
        public/simple_no_cgal.cpp
        public/stop.cpp)
target_link_libraries(test_public PRIVATE potato_peeling::potato_peeling doctest::doctest)

add_executable(test_internal
        main.cpp
        internal/longest_segment.cpp
        internal/mesh.cpp
        internal/vertex_skull.cpp)
target_include_directories(test_internal PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_internal PRIVATE potato_peeling_core doctest::doctest)

add_test(NAME public COMMAND test_public -tse=slow)
add_test(NAME internal COMMAND test_internal -tse=slow)
add_test(NAME internal_slow COMMAND test_internal -ts=slow)
set_tests_properties(internal_slow PROPERTIES LABELS slow)
