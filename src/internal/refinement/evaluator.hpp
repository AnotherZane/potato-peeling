#pragma once

// Certifying candidate Qs against P, on one thread or many.

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "internal/common.hpp"
#include "internal/parallel.hpp"
#include "internal/refinement/halfplanes.hpp"

namespace potato_peeling::internal::refinement {

    // area(Q) if Q is a convex subset of P, else nullopt. Exact, via certify.
    class Evaluator {
    public:
        explicit Evaluator(const Polygon &p) : p_(p) {}

        // Q given by half-planes, realised within `box`; with Q's vertices.
        std::optional<std::pair<FT, std::vector<Point>>>
        operator()(const std::vector<HalfPlane> &hp, const std::vector<Point> &box) const;

        // Q as the hull of a point set: adjoin works with vertices rather than
        // half-planes.
        std::optional<std::pair<FT, Polygon>> points(const std::vector<Point> &cand) const;

    private:
        const Polygon &p_;
    };

    // The thread pool, and per thread what it evaluates against: P (as an
    // Evaluator) and the box. Thread 0, the caller, uses copies of the originals;
    // the others get copies rebuilt from the exact values, since threads sharing
    // CGAL's lazy numbers contend on their reference counts. The values are equal,
    // so the answers are the same.
    class Workers {
    public:
        Workers(const Polygon &p, const std::vector<Point> &box, unsigned threads);

        unsigned size() const { return pool_.size(); }

        // See ThreadPool::run; task(i, w) runs on thread w.
        template <class Task> void run(const std::size_t count, const Task &task) {
            pool_.run(count, task);
        }

        const Evaluator &ev(const std::size_t w) const { return ev_[w]; }
        const std::vector<Point> &box(const std::size_t w) const { return box_[w]; }
        // P's vertex i.
        const Point &vertex(const std::size_t w, const std::size_t i) const { return poly_[w][i]; }

        // The caller's x, as thread w should use it.
        template <class T> static T local(const T &x, const std::size_t w) {
            return w > 0 ? fresh(x) : x;
        }

    private:
        ThreadPool pool_;
        std::vector<Polygon> poly_;
        std::vector<std::vector<Point>> box_;
        std::vector<Evaluator> ev_;
    };

}  // namespace potato_peeling::internal::refinement
