// potato_peeling._core: the C++ API for the Python package (python/potato_peeling).
//
// Coordinates arrive as float, int, or an exact (numerator, denominator) pair of
// ints, and exact values leave as (numerator, denominator) pairs of Python ints;
// the Python layer builds Fractions. Every call releases the GIL. A call with a
// timeout, or interrupted by Ctrl-C, stops through should_stop.

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
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

    // should_stop for one call: the timeout, and Ctrl-C (Python runs signal
    // handlers only when asked, so this asks every 20 ms, holding the GIL briefly).
    class Stopper {
    public:
        explicit Stopper(const std::optional<double> timeout) {
            if (timeout)
                deadline_ = clock::now()
                            + std::chrono::duration_cast<clock::duration>(
                                std::chrono::duration<double>(*timeout));
        }

        bool operator()() {
            const auto now = clock::now();
            if (deadline_ && now >= *deadline_) timed_out = true;
            if (!signal_ && now - last_ >= std::chrono::milliseconds(20)) {
                last_ = now;
                nb::gil_scoped_acquire acquire;
                if (PyErr_CheckSignals() != 0) signal_.emplace();  // takes the pending error
            }
            return timed_out || signal_.has_value();
        }

        // After the call, with the GIL held: re-raise a signal's exception.
        void raise_signal() {
            if (signal_) throw nb::python_error(*signal_);
        }

        bool timed_out = false;

    private:
        using clock = std::chrono::steady_clock;
        std::optional<clock::time_point> deadline_;
        clock::time_point last_ = clock::now();
        std::optional<nb::python_error> signal_;
    };

    // Runs f(should_stop) without the GIL. A timeout raises TimeoutError, unless
    // f returned anyway (refine returns its state when stopped); a signal always
    // re-raises its exception (KeyboardInterrupt for Ctrl-C).
    template <class F> auto run(const std::optional<double> timeout, F &&f) {
        Stopper stopper(timeout);
        const pp::StopCallback stop = [&stopper] { return stopper(); };
        std::optional<decltype(f(stop))> out;
        try {
            nb::gil_scoped_release release;
            out.emplace(f(stop));
        } catch (const pp::cancelled &) {}
        stopper.raise_signal();
        if (!out) {
            PyErr_SetString(PyExc_TimeoutError, "timed out");
            throw nb::python_error();
        }
        return *out;
    }
}  // namespace

NB_MODULE(_core, m) {
    m.doc() = "potato_peeling._core: the C++ library; use the potato_peeling package instead";

    nb::exception<pp::invalid_polygon>(m, "InvalidPolygonError", PyExc_ValueError);
    nb::exception<pp::certification_error>(m, "CertificationError", PyExc_RuntimeError);

    m.def(
        "convex_skull",
        [](nb::handle p, const std::uint64_t seed, const unsigned threads,
           const std::optional<double> timeout) {
            const pp::Polygon poly = to_polygon(p);
            return skull_out(run(timeout, [&](const pp::StopCallback &stop) {
                pp::ConvexSkullOptions o;
                o.seed = seed;
                o.threads = threads;
                o.should_stop = stop;
                return pp::convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("seed"), nb::arg("threads"), nb::arg("timeout").none());

    m.def(
        "exact_convex_skull",
        [](nb::handle p, const std::optional<double> timeout) {
            const pp::Polygon poly = to_polygon(p);
            return skull_out(run(timeout, [&](const pp::StopCallback &stop) {
                pp::BaseOptions o;
                o.should_stop = stop;
                return pp::exact_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("timeout").none());

    m.def(
        "mesh_convex_skull",
        [](nb::handle p, nb::handle points,
           const std::optional<std::vector<std::array<std::size_t, 3>>> &triangles,
           const std::optional<double> timeout) {
            const pp::Polygon poly = to_polygon(p);
            std::optional<pp::Mesh> mesh;
            if (triangles) mesh = pp::Mesh{to_points(points), *triangles};
            return skull_out(run(timeout, [&](const pp::StopCallback &stop) {
                pp::BaseOptions o;
                o.should_stop = stop;
                return mesh ? pp::mesh_convex_skull(poly, *mesh, o)
                            : pp::mesh_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("points").none(), nb::arg("triangles").none(),
        nb::arg("timeout").none());

    m.def(
        "approximate_convex_skull",
        [](nb::handle p, const double epsilon, const double delta, const std::uint64_t seed,
           const double sample_scale, const double s_scale,
           const std::optional<std::size_t> max_edges, const unsigned threads,
           const std::string &lower_bound, const std::optional<double> timeout) {
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
            return skull_out(run(timeout, [&](const pp::StopCallback &stop) {
                o.should_stop = stop;
                return pp::approximate_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("epsilon"), nb::arg("delta"), nb::arg("seed"),
        nb::arg("sample_scale"), nb::arg("s_scale"), nb::arg("max_edges").none(),
        nb::arg("threads"), nb::arg("lower_bound"), nb::arg("timeout").none());

    m.def(
        "refine",
        [](nb::handle p, nb::handle start, const unsigned max_sweeps, const unsigned rot_steps,
           const double rot_span, const unsigned split_steps, const unsigned adjoin_rounds,
           const std::optional<double> timeout) {
            const pp::Polygon poly = to_polygon(p), st = to_polygon(start);
            return skull_out(run(timeout, [&](const pp::StopCallback &stop) {
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
        nb::arg("timeout").none());

    m.def(
        "vertex_convex_skull",
        [](nb::handle p, const std::optional<double> timeout) {
            const pp::Polygon poly = to_polygon(p);
            return skull_out(run(timeout, [&](const pp::StopCallback &stop) {
                pp::BaseOptions o;
                o.should_stop = stop;
                return pp::vertex_convex_skull(poly, o);
            }));
        },
        nb::arg("polygon"), nb::arg("timeout").none());

    m.def(
        "upper_bound",
        [](nb::handle p, const double rel_tol, const unsigned pairs, const unsigned width_dirs,
           const unsigned edge_samples, const std::size_t max_evals_1d,
           const std::size_t max_evals_2d, const std::optional<double> timeout) {
            const pp::Polygon poly = to_polygon(p);
            return rational(run(timeout, [&](const pp::StopCallback &stop) {
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
        nb::arg("timeout").none());

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
