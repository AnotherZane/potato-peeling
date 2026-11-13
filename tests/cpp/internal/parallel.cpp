// ThreadPool runs every task of every batch once and passes a task's exception to
// the caller, after which the pool still works.

#include <doctest/doctest.h>

#include <atomic>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "internal/parallel.hpp"

namespace pi = potato_peeling::internal;

TEST_CASE("ThreadPool runs every task of every batch once") {
    for (const unsigned threads : {1u, 2u, 8u, 64u}) {
        pi::ThreadPool pool(threads);
        CHECK(pool.size() >= 1);
        CHECK(pool.size() <= threads);
        for (int batch = 0; batch < 50; ++batch) {
            for (const std::size_t count : {0u, 1u, 7u, 100u}) {
                std::vector<std::atomic<int>> runs(count);
                std::atomic<bool> bad_worker{false};
                pool.run(count, [&](const std::size_t i, const std::size_t w) {
                    ++runs[i];
                    if (w >= pool.size()) bad_worker = true;
                });
                for (const auto &r : runs) CHECK(r == 1);
                CHECK(!bad_worker);
            }
        }
    }
}

TEST_CASE("ThreadPool rethrows a task's exception and stays usable") {
    for (const unsigned threads : {1u, 4u}) {
        pi::ThreadPool pool(threads);
        CHECK_THROWS_AS(pool.run(50,
                                 [](const std::size_t i, std::size_t) {
                                     if (i == 17) throw std::runtime_error("task 17");
                                 }),
                        std::runtime_error);
        std::atomic<int> runs{0};
        pool.run(20, [&](std::size_t, std::size_t) { ++runs; });
        CHECK(runs == 20);
    }
}
