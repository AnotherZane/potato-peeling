// Provisional bindings: the methods, each returning the exact vertices and
// area as "num/den" strings. To be replaced by the designed Python API.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include "casters.hpp"
#include "potato_peeling/potato_peeling.hpp"

namespace nb = nanobind;
namespace pp = potato_peeling;

namespace {
    std::string rational(const pp::FT &v) {
        std::ostringstream o;
        o << CGAL::exact(v);
        return o.str();
    }

    nb::dict to_dict(const pp::Skull &s) {
        nb::list vertices;
        for (const pp::Point &v : s.polygon)
            vertices.append(nb::make_tuple(rational(v.x()), rational(v.y())));
        nb::dict out;
        out["vertices"] = vertices;
        out["area"] = rational(s.area);
        return out;
    }
}  // namespace

NB_MODULE(_core, m) {
    m.doc() = "potato_peeling._core (provisional bindings)";

    m.def(
        "convex_skull",
        [](const pp::Polygon &p, const std::uint64_t seed, const unsigned threads) {
            pp::ConvexSkullOptions o;
            o.seed = seed;
            o.threads = threads;
            pp::Skull s;
            {
                nb::gil_scoped_release release;
                s = pp::convex_skull(p, o);
            }
            return to_dict(s);
        },
        nb::arg("polygon"), nb::kw_only(), nb::arg("seed") = 0, nb::arg("threads") = 0u);

    m.def(
        "exact_convex_skull",
        [](const pp::Polygon &p) {
            pp::Skull s;
            {
                nb::gil_scoped_release release;
                s = pp::exact_convex_skull(p);
            }
            return to_dict(s);
        },
        nb::arg("polygon"));

    m.def(
        "mesh_convex_skull",
        [](const pp::Polygon &p) {
            pp::Skull s;
            {
                nb::gil_scoped_release release;
                s = pp::mesh_convex_skull(p);
            }
            return to_dict(s);
        },
        nb::arg("polygon"));

    m.def(
        "approximate_convex_skull",
        [](const pp::Polygon &p, const double epsilon, const double delta, const std::uint64_t seed,
           const double sample_scale, const double s_scale,
           const std::optional<std::size_t> max_edges, const unsigned threads) {
            pp::ApproximateOptions o;
            o.epsilon = epsilon;
            o.delta = delta;
            o.seed = seed;
            o.sample_scale = sample_scale;
            o.s_scale = s_scale;
            o.max_edges = max_edges;
            o.threads = threads;
            pp::Skull s;
            {
                nb::gil_scoped_release release;
                s = pp::approximate_convex_skull(p, o);
            }
            return to_dict(s);
        },
        nb::arg("polygon"), nb::kw_only(), nb::arg("epsilon") = 0.1, nb::arg("delta") = 1.0 / 3.0,
        nb::arg("seed") = 0, nb::arg("sample_scale") = 3e-4, nb::arg("s_scale") = 3e-2,
        nb::arg("max_edges").none() = std::size_t{64}, nb::arg("threads") = 0u);

    m.def(
        "refine",
        [](const pp::Polygon &p, const pp::Polygon &start, const unsigned max_sweeps,
           const unsigned rot_steps, const double rot_span, const unsigned split_steps,
           const unsigned adjoin_rounds) {
            pp::RefineOptions o;
            o.max_sweeps = max_sweeps;
            o.rot_steps = rot_steps;
            o.rot_span = rot_span;
            o.split_steps = split_steps;
            o.adjoin_rounds = adjoin_rounds;
            pp::Skull s;
            {
                nb::gil_scoped_release release;
                s = pp::refine(p, start, o);
            }
            return to_dict(s);
        },
        nb::arg("polygon"), nb::arg("start"), nb::kw_only(), nb::arg("max_sweeps") = 12u,
        nb::arg("rot_steps") = 9u, nb::arg("rot_span") = 0.30, nb::arg("split_steps") = 0u,
        nb::arg("adjoin_rounds") = 0u);
}
