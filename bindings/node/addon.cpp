// potato_peeling.node: the C++ API for the JavaScript package (js/).
//
// Coordinates arrive as numbers (read exactly) or exact "num/den" strings, and exact
// values leave as [numerator, denominator] decimal strings; the JavaScript layer
// builds BigInt fractions. Each solve runs on a thread of its own (not libuv's small
// pool, which long solves would starve) and settles a Promise. A cancel flag, set
// from JavaScript, stops it through should_stop.

#include <atomic>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <array>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <napi.h>

#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;

namespace {
    using ET = pp::FT::ET;
    using Flag = std::shared_ptr<std::atomic<bool>>;

    // ── values in ───────────────────────────────────────────────────────────

    pp::FT to_ft(const Napi::Value &v) {
        if (v.IsNumber()) {
            const double d = v.As<Napi::Number>().DoubleValue();
            if (!std::isfinite(d)) throw pp::invalid_polygon("coordinate is not finite");
            return pp::FT(d);
        }
        if (v.IsString()) return pp::FT(ET(v.As<Napi::String>().Utf8Value()));
        throw Napi::TypeError::New(v.Env(), "a coordinate must be a number or a fraction");
    }

    pp::Point to_point(const Napi::Value &v) {
        if (!v.IsArray() || v.As<Napi::Array>().Length() != 2)
            throw Napi::TypeError::New(v.Env(), "a point must be [x, y]");
        const Napi::Array a = v.As<Napi::Array>();
        return {to_ft(a.Get(0u)), to_ft(a.Get(1u))};
    }

    pp::Polygon to_polygon(const Napi::Value &v) {
        if (!v.IsArray())
            throw Napi::TypeError::New(v.Env(), "a polygon must be an array of points");
        const Napi::Array a = v.As<Napi::Array>();
        pp::Polygon out;
        for (std::uint32_t i = 0; i < a.Length(); ++i) out.push_back(to_point(a.Get(i)));
        return out;
    }

    std::vector<pp::Point> to_points(const Napi::Value &v) {
        if (!v.IsArray()) throw Napi::TypeError::New(v.Env(), "points must be an array");
        const Napi::Array a = v.As<Napi::Array>();
        std::vector<pp::Point> out;
        for (std::uint32_t i = 0; i < a.Length(); ++i) out.push_back(to_point(a.Get(i)));
        return out;
    }

    // [[i, j, k], ...] as indices into the mesh's points.
    std::vector<std::array<std::size_t, 3>> to_triangles(const Napi::Value &v) {
        if (!v.IsArray()) throw Napi::TypeError::New(v.Env(), "triangles must be an array");
        const Napi::Array a = v.As<Napi::Array>();
        std::vector<std::array<std::size_t, 3>> out;
        for (std::uint32_t i = 0; i < a.Length(); ++i) {
            const Napi::Value t = a.Get(i);
            if (!t.IsArray() || t.As<Napi::Array>().Length() != 3)
                throw Napi::TypeError::New(v.Env(), "a triangle must be [i, j, k]");
            std::array<std::size_t, 3> tri{};
            for (std::uint32_t k = 0; k < 3; ++k)
                tri[k] = static_cast<std::size_t>(
                    t.As<Napi::Array>().Get(k).As<Napi::Number>().Int64Value());
            out.push_back(tri);
        }
        return out;
    }

    unsigned to_unsigned(const Napi::Value &v) {
        return v.As<Napi::Number>().Uint32Value();
    }

    double to_double(const Napi::Value &v) {
        return v.As<Napi::Number>().DoubleValue();
    }

    std::size_t to_size(const Napi::Value &v) {
        return static_cast<std::size_t>(v.As<Napi::Number>().Int64Value());
    }

    Flag to_flag(const Napi::Value &v) {
        if (v.IsUndefined() || v.IsNull()) return nullptr;
        return *v.As<Napi::External<Flag>>().Data();
    }

    pp::StopCallback stop_on(const Flag &flag) {
        if (!flag) return {};
        return [flag] { return flag->load(std::memory_order_relaxed); };
    }

    // ── values out ──────────────────────────────────────────────────────────

    Napi::Array rational(const Napi::Env env, const pp::FT &v) {
        typename CGAL::Fraction_traits<ET>::Numerator_type num, den;
        typename CGAL::Fraction_traits<ET>::Decompose()(CGAL::exact(v), num, den);
        std::ostringstream n, d;
        n << num;
        d << den;
        Napi::Array out = Napi::Array::New(env, 2);
        out.Set(0u, n.str());
        out.Set(1u, d.str());
        return out;
    }

    Napi::Array point_out(const Napi::Env env, const pp::Point &p) {
        Napi::Array out = Napi::Array::New(env, 2);
        out.Set(0u, rational(env, p.x()));
        out.Set(1u, rational(env, p.y()));
        return out;
    }

    // {vertices: [[[xn, xd], [yn, yd]], ...], area: [n, d]}
    Napi::Object skull_out(const Napi::Env env, const pp::Skull &s) {
        Napi::Array vertices = Napi::Array::New(env, s.polygon.size());
        std::uint32_t i = 0;
        for (const pp::Point &v : s.polygon) vertices.Set(i++, point_out(env, v));
        Napi::Object out = Napi::Object::New(env);
        out.Set("vertices", vertices);
        out.Set("area", rational(env, s.area));
        return out;
    }

    // ── solves off the JavaScript thread ────────────────────────────────────

    // A rejection the JavaScript layer maps to its own error by `code`.
    Napi::Value error_value(const Napi::Env env, const std::exception_ptr &e) {
        const auto make = [&](const char *code, const std::string &message) {
            Napi::Error err = Napi::Error::New(env, message);
            err.Set("code", code);
            return err.Value();
        };
        try {
            std::rethrow_exception(e);
        } catch (const pp::invalid_polygon &x) {
            return make("INVALID_POLYGON", x.what());
        } catch (const pp::certification_error &x) {
            return make("CERTIFICATION", x.what());
        } catch (const pp::cancelled &x) {
            return make("CANCELLED", x.what());
        } catch (const std::invalid_argument &x) {
            return make("INVALID_ARGUMENT", x.what());
        } catch (const std::exception &x) {
            return make("INTERNAL", x.what());
        } catch (...) {
            return make("INTERNAL", "unknown error");
        }
    }

    // What a solve hands back to the JavaScript thread: a function building the result.
    using Deliver = std::function<Napi::Value(Napi::Env)>;

    struct Job {
        std::function<Deliver()> compute;
        Napi::Promise::Deferred deferred;
        Napi::ThreadSafeFunction tsfn;
        Deliver deliver;
        std::exception_ptr error;
    };

    // Runs compute() on a new thread; the returned Promise settles with its result.
    Napi::Value run(const Napi::Env env, std::function<Deliver()> compute) {
        auto job = std::make_unique<Job>(
            Job{std::move(compute), Napi::Promise::Deferred::New(env), {}, {}, {}});
        const Napi::Promise promise = job->deferred.Promise();
        job->tsfn = Napi::ThreadSafeFunction::New(
            env, Napi::Function::New(env, [](const Napi::CallbackInfo &) {}), "potato_peeling", 0,
            1);
        Job *raw = job.get();
        try {
            std::thread([raw] {
                try {
                    raw->deliver = raw->compute();
                } catch (...) {
                    raw->error = std::current_exception();
                }
                // The callback runs on the JavaScript thread and frees the job, so
                // only the copied handle is used after this call.
                const Napi::ThreadSafeFunction tsfn = raw->tsfn;
                tsfn.BlockingCall(raw, [](const Napi::Env env, Napi::Function, Job *done) {
                    const std::unique_ptr<Job> owned(done);
                    if (done->error)
                        done->deferred.Reject(error_value(env, done->error));
                    else
                        done->deferred.Resolve(done->deliver(env));
                });
                tsfn.Release();
            }).detach();
        } catch (const std::system_error &) {
            job->tsfn.Release();
            throw;
        }
        job.release();  // owned by the thread until its callback runs
        return promise;
    }

    // A solve's entry point: argument errors from JavaScript's side (Napi::Error)
    // throw at once; the library's (an invalid polygon) reject the Promise, as they
    // would from the solve.
    template <class F> Napi::Value solve(const Napi::CallbackInfo &info, F &&start) {
        try {
            return start();
        } catch (const Napi::Error &) {
            throw;
        } catch (...) {
            const Napi::Promise::Deferred d = Napi::Promise::Deferred::New(info.Env());
            d.Reject(error_value(info.Env(), std::current_exception()));
            return d.Promise();
        }
    }

    // A synchronous utility: the library's errors throw with the same codes.
    template <class F> Napi::Value sync(const Napi::CallbackInfo &info, F &&f) {
        try {
            return f();
        } catch (const Napi::Error &) {
            throw;
        } catch (...) {
            throw Napi::Error(info.Env(), error_value(info.Env(), std::current_exception()));
        }
    }

    Deliver skull_result(pp::Skull s) {
        return [s = std::move(s)](const Napi::Env env) { return Napi::Value(skull_out(env, s)); };
    }

    std::uint64_t to_seed(const Napi::Value &v) {
        bool lossless = false;
        const std::uint64_t seed = v.As<Napi::BigInt>().Uint64Value(&lossless);
        if (!lossless) throw Napi::RangeError::New(v.Env(), "seed must fit in 64 bits");
        return seed;
    }

    // ── exports ─────────────────────────────────────────────────────────────

    Napi::Value new_flag(const Napi::CallbackInfo &info) {
        return Napi::External<Flag>::New(info.Env(),
                                         new Flag(std::make_shared<std::atomic<bool>>(false)),
                                         [](Napi::Env, Flag *f) { delete f; });
    }

    Napi::Value set_flag(const Napi::CallbackInfo &info) {
        (*info[0].As<Napi::External<Flag>>().Data())->store(true);
        return info.Env().Undefined();
    }

    // convexSkull(polygon, seed: bigint, threads: number, flag?)
    Napi::Value convex_skull(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]);
            pp::ConvexSkullOptions o;
            o.seed = to_seed(info[1]);
            o.threads = info[2].As<Napi::Number>().Uint32Value();
            o.should_stop = stop_on(to_flag(info[3]));
            return run(info.Env(), [p, o] { return skull_result(pp::convex_skull(p, o)); });
        });
    }

    // exactConvexSkull(polygon, flag?)
    Napi::Value exact_convex_skull(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]);
            pp::BaseOptions o;
            o.should_stop = stop_on(to_flag(info[1]));
            return run(info.Env(), [p, o] { return skull_result(pp::exact_convex_skull(p, o)); });
        });
    }

    // meshConvexSkull(polygon, points | null, triangles | null, flag?)
    Napi::Value mesh_convex_skull(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]);
            std::optional<pp::Mesh> mesh;
            if (!info[2].IsNull() && !info[2].IsUndefined())
                mesh = pp::Mesh{to_points(info[1]), to_triangles(info[2])};
            pp::BaseOptions o;
            o.should_stop = stop_on(to_flag(info[3]));
            return run(info.Env(), [p, mesh, o] {
                return skull_result(mesh ? pp::mesh_convex_skull(p, *mesh, o)
                                         : pp::mesh_convex_skull(p, o));
            });
        });
    }

    // approximateConvexSkull(polygon, epsilon, delta, seed: bigint, sampleScale, sScale,
    //                        maxEdges | null, threads, lowerBound, flag?)
    Napi::Value approximate_convex_skull(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]);
            pp::ApproximateOptions o;
            o.epsilon = to_double(info[1]);
            o.delta = to_double(info[2]);
            o.seed = to_seed(info[3]);
            o.sample_scale = to_double(info[4]);
            o.s_scale = to_double(info[5]);
            if (!info[6].IsNull()) o.max_edges = to_size(info[6]);
            o.threads = to_unsigned(info[7]);
            const std::string lower_bound = info[8].As<Napi::String>().Utf8Value();
            if (lower_bound == "aronov")
                o.lower_bound = pp::LowerBound::aronov;
            else if (lower_bound == "vertex_triangle")
                o.lower_bound = pp::LowerBound::vertex_triangle;
            else
                throw Napi::RangeError::New(info.Env(),
                                            "lowerBound must be 'vertex_triangle' or 'aronov'");
            o.should_stop = stop_on(to_flag(info[9]));
            return run(info.Env(),
                       [p, o] { return skull_result(pp::approximate_convex_skull(p, o)); });
        });
    }

    // refine(polygon, start, maxSweeps, rotSteps, rotSpan, splitSteps, adjoinRounds,
    //        threads, flag?)
    Napi::Value refine(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]), start = to_polygon(info[1]);
            pp::RefineOptions o;
            o.max_sweeps = to_unsigned(info[2]);
            o.rot_steps = to_unsigned(info[3]);
            o.rot_span = to_double(info[4]);
            o.split_steps = to_unsigned(info[5]);
            o.adjoin_rounds = to_unsigned(info[6]);
            o.threads = to_unsigned(info[7]);
            o.should_stop = stop_on(to_flag(info[8]));
            return run(info.Env(), [p, start, o] { return skull_result(pp::refine(p, start, o)); });
        });
    }

    // vertexConvexSkull(polygon, flag?)
    Napi::Value vertex_convex_skull(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]);
            pp::BaseOptions o;
            o.should_stop = stop_on(to_flag(info[1]));
            return run(info.Env(), [p, o] { return skull_result(pp::vertex_convex_skull(p, o)); });
        });
    }

    // upperBound(polygon, relTol, pairs, widthDirs, edgeSamples, maxEvals1d, maxEvals2d,
    //            flag?) -> [n, d]
    Napi::Value upper_bound(const Napi::CallbackInfo &info) {
        return solve(info, [&] {
            const pp::Polygon p = to_polygon(info[0]);
            pp::UpperBoundOptions o;
            o.rel_tol = to_double(info[1]);
            o.pairs = to_unsigned(info[2]);
            o.width_dirs = to_unsigned(info[3]);
            o.edge_samples = to_unsigned(info[4]);
            o.max_evals_1d = to_size(info[5]);
            o.max_evals_2d = to_size(info[6]);
            o.should_stop = stop_on(to_flag(info[7]));
            return run(info.Env(), [p, o]() -> Deliver {
                const pp::FT bound = pp::upper_bound(p, o);
                return [bound](const Napi::Env env) { return Napi::Value(rational(env, bound)); };
            });
        });
    }

    // ── utilities (synchronous) ─────────────────────────────────────────────

    Napi::Value is_convex_subset(const Napi::CallbackInfo &info) {
        return sync(info, [&] {
            const bool r = pp::is_convex_subset(to_polygon(info[0]), to_polygon(info[1]));
            return Napi::Value(Napi::Boolean::New(info.Env(), r));
        });
    }

    Napi::Value segment_in_polygon(const Napi::CallbackInfo &info) {
        return sync(info, [&] {
            const bool r
                = pp::segment_in_polygon(to_polygon(info[0]), to_point(info[1]), to_point(info[2]));
            return Napi::Value(Napi::Boolean::New(info.Env(), r));
        });
    }

    Napi::Value triangulate(const Napi::CallbackInfo &info) {
        return sync(info, [&] {
            const auto tris = pp::triangulate(to_polygon(info[0]));
            Napi::Array out = Napi::Array::New(info.Env(), tris.size());
            for (std::uint32_t i = 0; i < tris.size(); ++i) {
                Napi::Array t = Napi::Array::New(info.Env(), 3);
                for (std::uint32_t k = 0; k < 3; ++k) t.Set(k, static_cast<double>(tris[i][k]));
                out.Set(i, t);
            }
            return Napi::Value(out);
        });
    }

    Napi::Value reflex_vertices(const Napi::CallbackInfo &info) {
        return sync(info, [&] {
            const std::vector<bool> flags = pp::reflex_vertices(to_polygon(info[0]));
            Napi::Array out = Napi::Array::New(info.Env(), flags.size());
            for (std::uint32_t i = 0; i < flags.size(); ++i)
                out.Set(i, static_cast<bool>(flags[i]));
            return Napi::Value(out);
        });
    }

    // -> [a, b], each [[xn, xd], [yn, yd]]
    Napi::Value longest_segment(const Napi::CallbackInfo &info) {
        return sync(info, [&] {
            const pp::Segment s = pp::longest_segment(to_polygon(info[0]));
            Napi::Array out = Napi::Array::New(info.Env(), 2);
            out.Set(0u, point_out(info.Env(), s.source()));
            out.Set(1u, point_out(info.Env(), s.target()));
            return Napi::Value(out);
        });
    }

    Napi::Object init(const Napi::Env env, Napi::Object exports) {
        exports.Set("newFlag", Napi::Function::New(env, new_flag));
        exports.Set("setFlag", Napi::Function::New(env, set_flag));
        exports.Set("convexSkull", Napi::Function::New(env, convex_skull));
        exports.Set("exactConvexSkull", Napi::Function::New(env, exact_convex_skull));
        exports.Set("meshConvexSkull", Napi::Function::New(env, mesh_convex_skull));
        exports.Set("approximateConvexSkull", Napi::Function::New(env, approximate_convex_skull));
        exports.Set("refine", Napi::Function::New(env, refine));
        exports.Set("vertexConvexSkull", Napi::Function::New(env, vertex_convex_skull));
        exports.Set("upperBound", Napi::Function::New(env, upper_bound));
        exports.Set("isConvexSubset", Napi::Function::New(env, is_convex_subset));
        exports.Set("segmentInPolygon", Napi::Function::New(env, segment_in_polygon));
        exports.Set("triangulate", Napi::Function::New(env, triangulate));
        exports.Set("reflexVertices", Napi::Function::New(env, reflex_vertices));
        exports.Set("longestSegment", Napi::Function::New(env, longest_segment));
        return exports;
    }
}  // namespace

NODE_API_MODULE(potato_peeling, init)
