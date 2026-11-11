#pragma once

// Test polygons: hand-made shapes and random star polygons on a small integer grid
// (many collinear vertices, equal heights and parallel edges).

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "potato_peeling/types.hpp"

namespace test_polygons {

    namespace pp = potato_peeling;

    inline pp::Polygon make(const std::vector<std::pair<double, double>> &xy) {
        pp::Polygon p;
        for (const auto &[x, y] : xy) p.push_back(pp::Point(x, y));
        return p;
    }

    inline pp::Polygon l_shape() {
        return make({{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}});
    }

    inline pp::Polygon comb() {
        return make({{0, 0},
                     {7, 0},
                     {7, 3},
                     {6, 3},
                     {6, 1},
                     {5, 1},
                     {5, 3},
                     {4, 3},
                     {4, 1},
                     {3, 1},
                     {3, 3},
                     {2, 3},
                     {2, 1},
                     {1, 1},
                     {1, 3},
                     {0, 3}});
    }

    inline pp::Polygon spiky() {
        return make({{0, 0}, {10, 0}, {10, 10}, {6, 4}, {5, 10}, {4, 4}, {0, 10}});
    }

    // A polygon with irrational-looking float coordinates (not on a grid).
    inline pp::Polygon wobbly() {
        return make({{0.1, 0.0}, {3.3, 0.2}, {2.9, 1.7}, {1.75, 1.05}, {1.6, 2.95}, {0.05, 2.1}});
    }

    // Vertices at distinct angles around the origin, sorted by angle; rejected
    // (empty result) unless simple and not convex after cleaning. `clean` is the
    // caller's cleaning function (the public API cleans inside each method).
    template <class Clean>
    std::vector<pp::Polygon> grid_stars(unsigned seed, std::size_t count, int grid, int max_n,
                                        std::size_t max_size, Clean clean) {
        std::vector<pp::Polygon> out;
        std::mt19937 gen(seed);
        while (out.size() < count) {
            const int n = 5 + static_cast<int>(gen() % static_cast<unsigned>(max_n - 4));
            std::vector<std::pair<double, pp::Point>> v;
            for (int k = 0; k < n; ++k) {
                const int x = static_cast<int>(gen() % static_cast<unsigned>(grid)) - grid / 2;
                const int y = static_cast<int>(gen() % static_cast<unsigned>(grid)) - grid / 2;
                if (x != 0 || y != 0) v.emplace_back(std::atan2(y, x), pp::Point(x, y));
            }
            std::sort(v.begin(), v.end(),
                      [](const auto &a, const auto &b) { return a.first < b.first; });
            pp::Polygon p;
            for (std::size_t k = 0; k < v.size(); ++k)
                if (k == 0 || v[k].first != v[k - 1].first) p.push_back(v[k].second);
            try {
                const pp::Polygon c = clean(p);
                if (!c.is_convex() && c.size() <= max_size) out.push_back(c);
            } catch (const std::invalid_argument &) {}
        }
        return out;
    }

}  // namespace test_polygons
