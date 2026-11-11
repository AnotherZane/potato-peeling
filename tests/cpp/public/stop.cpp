// should_stop: every method stops when asked and calls it only from the calling
// thread; refine returns its certified state; thread counts do not change answers.

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <cmath>
#include <thread>

#include "../polygons.hpp"
#include "potato_peeling/potato_peeling.hpp"
#include "potato_peeling/utilities.hpp"

namespace pp = potato_peeling;
namespace tp = test_polygons;

namespace {
    // A deterministic irregular star; its exact solve takes far longer than any test.
    pp::Polygon slow_star(int n) {
        pp::Polygon p;
        for (int k = 0; k < n; ++k) {
            const double t = 2 * std::acos(-1.0) * k / n;
            const double r = 10 + (k % 2 ? 7 : 0) + ((k * 7919 + 104729) % 13) / 4.0;
            p.push_back(pp::Point(std::round(r * std::cos(t) * 1000) / 1000,
                                  std::round(r * std::sin(t) * 1000) / 1000));
        }
        return p;
    }

    // Stops after `after` calls; records whether any call came from another thread.
    struct Stopper {
        explicit Stopper(int after) : after(after) {}
        int after;
        std::thread::id caller = std::this_thread::get_id();
        std::atomic<int> calls{0};
        std::atomic<bool> foreign{false};
        pp::StopCallback callback() {
            return [this] {
                if (std::this_thread::get_id() != caller) foreign = true;
                return ++calls > after;
            };
        }
    };
}  // namespace

TEST_CASE("every method stops when asked") {
    const pp::Polygon p = slow_star(32);
    for (const int after : {0, 5}) {
        CAPTURE(after);
        {
            Stopper s(after);
            CHECK_THROWS_AS(pp::exact_convex_skull(p, {s.callback()}), pp::cancelled);
            CHECK(!s.foreign);
        }
        {
            Stopper s(after);
            CHECK_THROWS_AS(pp::mesh_convex_skull(p, {s.callback()}), pp::cancelled);
        }
        {
            Stopper s(after);
            CHECK_THROWS_AS(pp::vertex_convex_skull(p, {s.callback()}), pp::cancelled);
        }
        {
            Stopper s(after);
            pp::ApproximateOptions o;
            o.threads = 4;
            o.should_stop = s.callback();
            CHECK_THROWS_AS(pp::approximate_convex_skull(p, o), pp::cancelled);
            CHECK(!s.foreign);
        }
        {
            Stopper s(after);
            pp::UpperBoundOptions o;
            o.should_stop = s.callback();
            CHECK_THROWS_AS(pp::upper_bound(p, o), pp::cancelled);
        }
    }
}

TEST_CASE("Cabello stops inside a long edge search") {
    // at the paper's sample sizes one edge search runs for minutes
    for (const unsigned threads : {1u, 4u}) {
        int calls = 0;
        pp::ApproximateOptions o;
        o.epsilon = 0.5;
        o.sample_scale = 1;
        o.s_scale = 1;
        o.max_edges = std::nullopt;
        o.threads = threads;
        const auto t0 = std::chrono::steady_clock::now();
        o.should_stop = [&] {
            ++calls;
            return std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(300);
        };
        CHECK_THROWS_AS(pp::approximate_convex_skull(tp::l_shape(), o), pp::cancelled);
        CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::seconds(2));
    }
}

TEST_CASE("refine returns its certified state when stopped") {
    const pp::Polygon p = slow_star(32);
    const pp::Polygon start = tp::make({{0, 0}, {1, 0}, {0, 1}});
    for (const int after : {0, 1, 10, 100}) {
        Stopper s(after);
        pp::RefineOptions o;
        o.adjoin_rounds = 8;
        o.should_stop = s.callback();
        const pp::Skull r = pp::refine(p, start, o);
        CHECK(pp::is_convex_subset(p, r.polygon));
        CHECK(r.area >= start.area());
        CHECK(!s.foreign);
    }
}

TEST_CASE("convex_skull stopped while refining returns an answer") {
    const pp::Polygon p = tp::comb();
    // Count the calls Cabello makes, then stop just after it.
    Stopper count(1 << 30);
    pp::ApproximateOptions a;
    a.threads = 1;
    a.should_stop = count.callback();
    const pp::Skull approx = pp::approximate_convex_skull(p, a);
    Stopper s(count.calls);
    pp::ConvexSkullOptions o;
    o.threads = 1;
    o.should_stop = s.callback();
    const pp::Skull r = pp::convex_skull(p, o);
    CHECK(pp::is_convex_subset(p, r.polygon));
    CHECK(r.area >= approx.area);
}

TEST_CASE("a should_stop that never stops changes nothing") {
    const pp::Polygon p = tp::spiky();
    const pp::ExactOptions o{[] { return false; }};
    CHECK(pp::exact_convex_skull(p, o).area == pp::exact_convex_skull(p).area);
}

TEST_CASE("Cabello's answer does not depend on the thread count") {
    for (const pp::Polygon &p : {tp::comb(), tp::spiky(), slow_star(24)}) {
        pp::ApproximateOptions o;
        o.threads = 1;
        const pp::Skull one = pp::approximate_convex_skull(p, o);
        for (const unsigned t : {0u, 2u, 4u, 8u}) {
            o.threads = t;
            const pp::Skull r = pp::approximate_convex_skull(p, o);
            CHECK(r.area == one.area);
            CHECK(r.polygon == one.polygon);
        }
    }
}
