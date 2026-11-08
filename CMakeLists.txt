cmake_minimum_required(VERSION 3.24)
project(potato_peeling VERSION 0.1.0 LANGUAGES CXX)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif ()

# Shared by default, so LGPL users can relink against a modified library.
option(BUILD_SHARED_LIBS "Build potato_peeling as a shared library" ON)

include(GNUInstallDirs)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)

find_package(CGAL 6.0 REQUIRED)
find_package(Boost 1.74 CONFIG REQUIRED)

# CDT (MPL-2.0), header-only and private to the build: an installed package if
# there is one, else this pinned release. The release is only unpacked (its
# CMake project, with its install rules, is not added).
include(FetchContent)
FetchContent_Declare(CDT
        URL https://github.com/artem-ogre/CDT/archive/refs/tags/2.0.1.tar.gz
        URL_HASH SHA256=5e3c45715e55b5f2d4390082c8c1ac026f635ddee233516fdcf94b197954367d
        SOURCE_SUBDIR no-cmake-project
        FIND_PACKAGE_ARGS CONFIG)
FetchContent_MakeAvailable(CDT)
if (NOT TARGET CDT::CDT)
    add_library(potato_peeling_cdt INTERFACE)
    target_include_directories(potato_peeling_cdt SYSTEM INTERFACE ${cdt_SOURCE_DIR}/CDT/include)
    add_library(CDT::CDT ALIAS potato_peeling_cdt)
endif ()

# ── C++ core, compiled once ───────────────────────────────────────────────────
# The library, the Python module and the tests all link these objects.
add_library(potato_peeling_core OBJECT
        src/internal/geometry.cpp
        src/potato_peeling.cpp
        src/utilities.cpp
        src/internal/chang_and_yap/chang_and_yap.cpp
        src/internal/aronov/mesh.cpp
        src/internal/aronov/predicates.cpp
        src/internal/aronov/sweep.cpp
        src/internal/aronov/aronov.cpp
        src/internal/cabello/kernel.cpp
        src/internal/cabello/certify.cpp
        src/internal/cabello/sampling.cpp
        src/internal/cabello/visibility.cpp
        src/internal/cabello/convex_clique.cpp
        src/internal/cabello/a_bound.cpp
        src/internal/cabello/cabello.cpp
        src/internal/refinement/refinement.cpp
)
target_include_directories(potato_peeling_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(potato_peeling_core PUBLIC cxx_std_17)
target_link_libraries(potato_peeling_core PUBLIC CGAL::CGAL PRIVATE Boost::headers CDT::CDT)
target_compile_definitions(potato_peeling_core PRIVATE
        # Fail the build if any GPL-licensed CGAL package is included.
        CGAL_LICENSE_ERROR
        CDT_ENSURE_PRECISE_MATH_IN_CONSTRUCTIONS)

# The objects are compiled for the library, so they export under its name.
set_target_properties(potato_peeling_core PROPERTIES DEFINE_SYMBOL potato_peeling_EXPORTS)
generate_export_header(potato_peeling_core
        BASE_NAME potato_peeling
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/potato_peeling/export.hpp
        STATIC_DEFINE POTATO_PEELING_STATIC_DEFINE)

set(POTATO_PEELING_SHARED OFF)
if (BUILD_SHARED_LIBS AND NOT SKBUILD)
    set(POTATO_PEELING_SHARED ON)
endif ()
if (POTATO_PEELING_SHARED)
    target_compile_definitions(potato_peeling_core PRIVATE potato_peeling_EXPORTS)
else ()
    target_compile_definitions(potato_peeling_core PUBLIC POTATO_PEELING_STATIC_DEFINE)
endif ()

# ── The C++ library: potato_peeling::potato_peeling ───────────────────────────
if (NOT SKBUILD)
    add_library(potato_peeling $<TARGET_OBJECTS:potato_peeling_core>)
    add_library(potato_peeling::potato_peeling ALIAS potato_peeling)
    target_include_directories(potato_peeling PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
    target_compile_features(potato_peeling PUBLIC cxx_std_17)
    target_link_libraries(potato_peeling PUBLIC CGAL::CGAL)
    if (NOT POTATO_PEELING_SHARED)
        target_compile_definitions(potato_peeling PUBLIC POTATO_PEELING_STATIC_DEFINE)
    endif ()
    # The ABI may change with every minor release while the version is 0.x.
    set_target_properties(potato_peeling PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

    install(TARGETS potato_peeling EXPORT potato_peelingTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT runtime
            NAMELINK_COMPONENT development
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT development)
    install(DIRECTORY include/potato_peeling ${CMAKE_CURRENT_BINARY_DIR}/include/potato_peeling
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT development)

    set(config_dir ${CMAKE_INSTALL_LIBDIR}/cmake/potato_peeling)
    install(EXPORT potato_peelingTargets NAMESPACE potato_peeling::
            DESTINATION ${config_dir} COMPONENT development)
    configure_package_config_file(cmake/potato_peelingConfig.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/potato_peelingConfig.cmake
            INSTALL_DESTINATION ${config_dir})
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/potato_peelingConfigVersion.cmake
            COMPATIBILITY SameMinorVersion)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/potato_peelingConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/potato_peelingConfigVersion.cmake
            DESTINATION ${config_dir} COMPONENT development)
endif ()

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
    )
    target_link_libraries(_core PRIVATE potato_peeling_core)
    target_compile_definitions(_core PRIVATE CGAL_LICENSE_ERROR)
    install(TARGETS _core DESTINATION potato_peeling COMPONENT python)
endif ()
