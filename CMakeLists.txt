cmake_minimum_required(VERSION 3.24)
project(potato_peeling VERSION 0.1.0 LANGUAGES CXX)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

find_package(CGAL 6.0 REQUIRED)
find_package(Boost 1.74 CONFIG REQUIRED)

# CDT (MPL-2.0): an installed package if there is one, else this pinned release.
include(FetchContent)
set(CDT_ENSURE_PRECISE_MATH_IN_CONSTRUCTIONS ON CACHE BOOL "" FORCE)
FetchContent_Declare(CDT
        URL https://github.com/artem-ogre/CDT/archive/refs/tags/2.0.1.tar.gz
        URL_HASH SHA256=5e3c45715e55b5f2d4390082c8c1ac026f635ddee233516fdcf94b197954367d
        SOURCE_SUBDIR CDT
        FIND_PACKAGE_ARGS CONFIG)
FetchContent_MakeAvailable(CDT)

# ── C++ core ──────────────────────────────────────────────────────────────────
add_library(potato_peeling_geometry STATIC src/geometry.cpp)
target_include_directories(potato_peeling_geometry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(potato_peeling_geometry PUBLIC cxx_std_17)
target_link_libraries(potato_peeling_geometry PUBLIC CGAL::CGAL PRIVATE Boost::headers CDT::CDT)
# Fail the build if any GPL-licensed CGAL package is included.
target_compile_definitions(potato_peeling_geometry PUBLIC CGAL_LICENSE_ERROR)

add_library(potato_peeling_chang_and_yap STATIC
        src/algorithms/chang_and_yap/chang_and_yap.cpp
        src/algorithms/chang_and_yap/memprobe.cpp
)
target_link_libraries(potato_peeling_chang_and_yap PUBLIC potato_peeling_geometry)

add_library(potato_peeling_aronov STATIC
        src/algorithms/aronov/mesh.cpp
        src/algorithms/aronov/predicates.cpp
        src/algorithms/aronov/sweep.cpp
        src/algorithms/aronov/aronov.cpp
)
target_link_libraries(potato_peeling_aronov PUBLIC potato_peeling_geometry)

add_library(potato_peeling_cabello STATIC
        src/algorithms/cabello/kernel.cpp
        src/algorithms/cabello/certify.cpp
        src/algorithms/cabello/sampling.cpp
        src/algorithms/cabello/visibility.cpp
        src/algorithms/cabello/convex_clique.cpp
        src/algorithms/cabello/a_bound.cpp
        src/algorithms/cabello/cabello.cpp
)
target_link_libraries(potato_peeling_cabello PUBLIC potato_peeling_geometry potato_peeling_aronov)

add_library(potato_peeling_refinement STATIC src/refinement/refinement.cpp)
target_link_libraries(potato_peeling_refinement PUBLIC potato_peeling_geometry potato_peeling_cabello)

# ── Python bindings: potato_peeling._core ─────────────────────────────────────
if (SKBUILD)
    find_package(Python COMPONENTS Interpreter Development.Module Development.SABIModule REQUIRED)
    execute_process(
            COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
    find_package(nanobind 3.0 CONFIG REQUIRED)

    nanobind_add_module(_core
            STABLE_ABI
            NB_STATIC
            bindings/python/module.cpp
            bindings/python/algorithms/chang_and_yap.cpp
            bindings/python/algorithms/aronov.cpp
            bindings/python/algorithms/cabello.cpp
            bindings/python/refinement.cpp
    )
    target_link_libraries(_core PRIVATE
            potato_peeling_chang_and_yap
            potato_peeling_aronov
            potato_peeling_cabello
            potato_peeling_refinement
    )
    install(TARGETS _core DESTINATION potato_peeling COMPONENT python)
endif ()
