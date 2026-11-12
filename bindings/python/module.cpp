// potato_peeling._core: the C++ API for the Python package (python/potato_peeling).
//
// Coordinates arrive as float, int, or an exact (numerator, denominator) pair of
// ints, and exact values leave as (numerator, denominator) pairs of Python ints;
// the Python layer builds Fractions. Every call releases the GIL. The caller's
// should_stop (the Python layer adds the timeout to it) and Ctrl-C stop a call
// through the C++ should_stop.

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>
#include <nanobind/stl/vector.h>

#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace nb = nanobind;
namespace pp = potato_peeling;

namespace {
    using ET = pp::FT::ET;

    std::string decimal(nb::handle i) {
        return nb::cast<std::string>(nb::str(i));
    }

    pp::FT to_ft(nb::handle h) {
        if (PyFloat_Check(h.ptr())) {
            const double d = PyFloat_AsDouble(h.ptr());
            if (!std::isfinite(d)) throw pp::invalid_polygon("coordinate is not finite");
            return pp::FT(d);
        }
        if (PyLong_Check(h.ptr()) && !PyBool_Check(h.ptr())) {
            int overflow = 0;
            const long long v = PyLong_AsLongLongAndOverflow(h.ptr(), &overflow);
            if (!overflow && v >= -(1LL << 53) && v <= (1LL << 53))
                return pp::FT(static_cast<double>(v));
            return pp::FT(ET(decimal(h)));
        }
        if (PyTuple_Check(h.ptr()) && PyTuple_Size(h.ptr()) == 2) {
            const nb::tuple t = nb::borrow<nb::tuple>(h);
            return pp::FT(ET(decimal(t[0]) + "/" + decimal(t[1])));
        }
        throw nb::type_error(
            "a coordinate must be a float, an int or a (numerator, denominator) pair");
    }

    pp::Point to_point(nb::handle h) {
        const nb::sequence s = nb::borrow<nb::sequence>(h);
        if (nb::len(s) != 2) throw nb::type_error("a point must have two coordinates");
        return {to_ft(s[0]), to_ft(s[1])};
    }

    std::vector<pp::Point> to_points(nb::handle h) {
        std::vector<pp::Point> out;
        for (nb::handle p : nb::borrow<nb::sequence>(h)) out.push_back(to_point(p));
        return out;
    }

    pp::Polygon to_polygon(nb::handle h) {
        const std::vector<pp::Point> pts = to_points(h);
        return {pts.begin(), pts.end()};
    }

    nb::object py_int(const std::string &s) {
        return nb::steal(PyLong_FromString(s.c_str(), nullptr, 10));
    }

    nb::tuple rational(const pp::FT &v) {
        typename CGAL::Fraction_traits<ET>::Numerator_type num, den;
        typename CGAL::Fraction_traits<ET>::Decompose()(CGAL::exact(v), num, den);
        std::ostringstream n, d;
        n << num;
        d << den;
        return nb::make_tuple(py_int(n.str()), py_int(d.str()));
    }

    nb::tuple point_out(const pp::Point &p) {
        const nb::tuple x = rational(p.x()), y = rational(p.y());
        return nb::make_tuple(x[0], x[1], y[0], y[1]);
    }

    nb::tuple skull_out(const pp::Skull &s) {
        nb::list vertices;
        for (const pp::Point &v : s.polygon) vertices.append(point_out(v));
        return nb::make_tuple(vertices, rational(s.area));
    }

    // should_stop for one call: the caller's should_stop and Ctrl-C. Python runs
    // signal handlers only when asked, so every 20 ms this holds the GIL briefly to
    // ask and to call should_stop (the library calls it only from the calling thread).
    class Stopper {
    public:
        explicit Stopper(nb::object should_stop) : should_stop_(std::move(should_stop)) {}

        bool operator()() {
            const auto now = clock::now();
            if (!stopped_ && now - last_ >= std::chrono::milliseconds(20)) {
                last_ = now;
                nb::gil_scoped_acquire acquire;
                if (PyErr_CheckSignals() != 0)
                    error_.emplace();  // takes the pending error
                else if (!should_stop_.is_none())
                    poll();
                stopped_ = stopped_ || error_.has_value();
            }
            return stopped_;
        }

        // After the call, with the GIL held: re-raise an exception from a signal or
        // from should_stop.
        void raise_error() {
            if (error_) throw nb::python_error(*error_);
        }

    private:
        using clock = std::chrono::steady_clock;

        void poll() {
            PyObject *result = PyObject_CallNoArgs(should_stop_.ptr());
            const int truth = result ? PyObject_IsTrue(result) : -1;
            Py_XDECREF(result);
            if (truth < 0)
                error_.emplace();
            else
                stopped_ = truth != 0;
        }

        nb::object should_stop_;
        clock::time_point last_ = clock::now();
        bool stopped_ = false;
        std::optional<nb::python_error> error_;
    };

    // Runs f(should_stop) without the GIL. When stopped, f returns if it can (refine
    // returns its state); otherwise Cancelled is raised, or the exception from a
    // signal (KeyboardInterrupt for Ctrl-C) or from should_stop.
    template <class F> auto run(nb::object should_stop, F &&f) {
        Stopper stopper(std::move(should_stop));
        const pp::StopCallback stop = [&stopper] { return stopper(); };
        std::optional<decltype(f(stop))> out;
        try {
            nb::gil_scoped_release release;
            out.emplace(f(stop));
        } catch (const pp::cancelled &) {}
        stopper.raise_error();
        if (!out) throw pp::cancelled("cancelled by should_stop");
        return *out;
    }
}  // namespace

NB_MODULE(_core, m) {
    m.doc() = "potato_peeling._core: the C++ library; use the potato_peeling package instead";

    nb::exception<pp::invalid_polygon>(m, "InvalidPolygonError", PyExc_ValueError);
    nb::exception<pp::certification_error>(m, "CertificationError", PyExc_RuntimeError);
    nb::exception<pp::cancelled>(m, "Cancelled", PyExc_Exception);

    m.def(
        "convex_skull",
        [](nb::handle p, const std::uint64_t seed, const unsigned threads, nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p);
            return skull_out(run(should_stop, [&](const pp::StopCallback &stop) {
                pp::ConvexSkullOptions o;
                o.seed = seed;
                o.threads = threads;
                o.should_stop = stop;
                return pp::convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("seed"), nb::arg("threads"), nb::arg("should_stop").none());

    m.def(
        "exact_convex_skull",
        [](nb::handle p, nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p);
            return skull_out(run(should_stop, [&](const pp::StopCallback &stop) {
                pp::BaseOptions o;
                o.should_stop = stop;
                return pp::exact_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("should_stop").none());

    m.def(
        "mesh_convex_skull",
        [](nb::handle p, nb::handle points,
           const std::optional<std::vector<std::array<std::size_t, 3>>> &triangles,
           nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p);
            std::optional<pp::Mesh> mesh;
            if (triangles) mesh = pp::Mesh{to_points(points), *triangles};
            return skull_out(run(should_stop, [&](const pp::StopCallback &stop) {
                pp::BaseOptions o;
                o.should_stop = stop;
                return mesh ? pp::mesh_convex_skull(poly, *mesh, o)
                            : pp::mesh_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("points").none(), nb::arg("triangles").none(),
        nb::arg("should_stop").none());

    m.def(
        "approximate_convex_skull",
        [](nb::handle p, const double epsilon, const double delta, const std::uint64_t seed,
           const double sample_scale, const double s_scale,
           const std::optional<std::size_t> max_edges, const unsigned threads,
           const std::string &lower_bound, nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p);
            pp::ApproximateOptions o;
            o.epsilon = epsilon;
            o.delta = delta;
            o.seed = seed;
            o.sample_scale = sample_scale;
            o.s_scale = s_scale;
            o.max_edges = max_edges;
            o.threads = threads;
            if (lower_bound == "aronov")
                o.lower_bound = pp::LowerBound::aronov;
            else if (lower_bound == "vertex_triangle")
                o.lower_bound = pp::LowerBound::vertex_triangle;
            else
                throw nb::value_error("lower_bound must be 'aronov' or 'vertex_triangle'");
            return skull_out(run(should_stop, [&](const pp::StopCallback &stop) {
                o.should_stop = stop;
                return pp::approximate_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("epsilon"), nb::arg("delta"), nb::arg("seed"),
        nb::arg("sample_scale"), nb::arg("s_scale"), nb::arg("max_edges").none(),
        nb::arg("threads"), nb::arg("lower_bound"), nb::arg("should_stop").none());

    m.def(
        "refine",
        [](nb::handle p, nb::handle start, const unsigned max_sweeps, const unsigned rot_steps,
           const double rot_span, const unsigned split_steps, const unsigned adjoin_rounds,
           nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p), st = to_polygon(start);
            return skull_out(run(should_stop, [&](const pp::StopCallback &stop) {
                pp::RefineOptions o;
                o.max_sweeps = max_sweeps;
                o.rot_steps = rot_steps;
                o.rot_span = rot_span;
                o.split_steps = split_steps;
                o.adjoin_rounds = adjoin_rounds;
                o.should_stop = stop;
                return pp::refine(poly, st, o);
            }));
        },
        nb::arg("polygon"), nb::arg("start"), nb::arg("max_sweeps"), nb::arg("rot_steps"),
        nb::arg("rot_span"), nb::arg("split_steps"), nb::arg("adjoin_rounds"),
        nb::arg("should_stop").none());

    m.def(
        "vertex_convex_skull",
        [](nb::handle p, nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p);
            return skull_out(run(should_stop, [&](const pp::StopCallback &stop) {
                pp::BaseOptions o;
                o.should_stop = stop;
                return pp::vertex_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("should_stop").none());

    m.def(
        "upper_bound",
        [](nb::handle p, const double rel_tol, const unsigned pairs, const unsigned width_dirs,
           const unsigned edge_samples, const std::size_t max_evals_1d,
           const std::size_t max_evals_2d, nb::object should_stop) {
            const pp::Polygon poly = to_polygon(p);
            return rational(run(should_stop, [&](const pp::StopCallback &stop) {
                pp::UpperBoundOptions o;
                o.rel_tol = rel_tol;
                o.pairs = pairs;
                o.width_dirs = width_dirs;
                o.edge_samples = edge_samples;
                o.max_evals_1d = max_evals_1d;
                o.max_evals_2d = max_evals_2d;
                o.should_stop = stop;
                return pp::upper_bound(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("rel_tol"), nb::arg("pairs"), nb::arg("width_dirs"),
        nb::arg("edge_samples"), nb::arg("max_evals_1d"), nb::arg("max_evals_2d"),
        nb::arg("should_stop").none());

    m.def("is_convex_subset", [](nb::handle p, nb::handle q) {
        const pp::Polygon a = to_polygon(p), b = to_polygon(q);
        nb::gil_scoped_release release;
        return pp::is_convex_subset(a, b);
    });

    m.def("segment_in_polygon", [](nb::handle p, nb::handle a, nb::handle b) {
        const pp::Polygon poly = to_polygon(p);
        const pp::Point x = to_point(a), y = to_point(b);
        nb::gil_scoped_release release;
        return pp::segment_in_polygon(poly, x, y);
    });

    m.def("triangulate", [](nb::handle p) {
        const pp::Polygon poly = to_polygon(p);
        nb::gil_scoped_release release;
        return pp::triangulate(poly);
    });

    m.def("reflex_vertices", [](nb::handle p) {
        const pp::Polygon poly = to_polygon(p);
        nb::gil_scoped_release release;
        return pp::reflex_vertices(poly);
    });

    m.def("longest_segment", [](nb::handle p) {
        const pp::Polygon poly = to_polygon(p);
        pp::Segment s;
        {
            nb::gil_scoped_release release;
            s = pp::longest_segment(poly);
        }
        return nb::make_tuple(point_out(s.source()), point_out(s.target()));
    });
}
