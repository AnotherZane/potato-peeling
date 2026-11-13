#include "internal/refinement/refinement.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <CGAL/Bbox_2.h>

#include "internal/geometry.hpp"
#include "internal/refinement/evaluator.hpp"
#include "internal/refinement/halfplanes.hpp"
#include "internal/refinement/push.hpp"

namespace potato_peeling::internal::refinement {

    namespace {
        // A trial move of rotate or split: Q's half-planes after it, and Q.
        struct Trial {
            std::vector<HalfPlane> hp;
            FT area;
            std::vector<Point> pts;
        };

        // The best trial with more than `area`, taken in order with strict >, so the
        // first on a tie: what a sequential search over the trials would keep.
        std::optional<Trial> best_trial(std::vector<std::optional<Trial>> &trials, const FT &area) {
            std::optional<Trial> best;
            for (auto &t : trials)
                if (t && t->area > (best ? best->area : area)) best = std::move(t);
            return best;
        }
    }  // namespace

    Result refine(const Polygon &p, const Polygon &start, const Options &opts) {
        Result res;
        const Polygon poly = counterclockwise(p);
        const Polygon q0 = counterclockwise(start);
        if (q0.size() < 3) throw invalid_polygon("refine: start has fewer than 3 vertices");

        CGAL::Bbox_2 bb;
        for (const auto &v : poly) {
            const double x = approx(v.x()), y = approx(v.y());
            bb += CGAL::Bbox_2(x, y, x, y);
        }
        const double pad = (bb.xmax() - bb.xmin()) + (bb.ymax() - bb.ymin()) + 1.0;
        const std::vector<Point> box
            = {Point(bb.xmin() - pad, bb.ymin() - pad), Point(bb.xmax() + pad, bb.ymin() - pad),
               Point(bb.xmax() + pad, bb.ymax() + pad), Point(bb.xmin() - pad, bb.ymax() + pad)};

        const Evaluator ev(poly);
        Workers workers(poly, box, resolve_threads(opts.threads));
        std::vector<HalfPlane> hp = halfplanes_of(q0);

        auto init = ev(hp, box);
        if (!init) throw invalid_polygon("refine: start is not a convex subset of P");
        FT area = init->first;
        std::vector<Point> pts = std::move(init->second);

        // A convex P is its own answer. The start has still been validated above.
        if (poly.is_convex()) {
            res.area = CGAL::abs(poly.area());
            res.polygon = poly;
            return res;
        }
        const std::size_t hp_cap = hp.size() * std::max(1u, opts.max_growth);

        const double c_hi_span = pad * pad * 8.0;  // generous "no constraint" offset

        bool stopped = false;
        const auto stop_now = [&] {
            if (!stopped && opts.should_stop && opts.should_stop()) stopped = true;
            return stopped;
        };

        for (unsigned sweep = 0; sweep < opts.max_sweeps && !stop_now(); ++sweep) {
            bool improved = false;
            std::vector<bool> pinned_edge;
            pinned_edge.reserve(hp.size());

            for (std::size_t i = 0; i < hp.size() && !stop_now(); ++i) {
                const double norm = std::sqrt(approx(hp[i].a * hp[i].a + hp[i].b * hp[i].b));
                const double c_now = approx(hp[i].c);

                // ---- PUSH: exact bisection on the offset ---------------------
                bool pinned = true;
                if (const PushResult pr
                    = push(workers, hp, i, c_now, c_now + norm * c_hi_span, opts.push_rounds);
                    pr.ok && pr.area > area) {
                    hp[i].c = pr.c;
                    area = pr.area;
                    pts = pr.pts;
                    improved = true;
                    pinned = false;
                }
                pinned_edge.push_back(pinned);

                // ---- ROTATE: 1-D search on the normal, each trial re-pushed ---
                //
                // The trials are independent: run them in parallel, then take the
                // best in angle order (strict >, so the first on a tie), as a
                // sequential search would.
                const double phi = std::atan2(approx(hp[i].b), approx(hp[i].a));
                const Point anchor = rotation_anchor(pts, hp[i]);
                const int half = static_cast<int>(opts.rot_steps) / 2;
                std::vector<double> angles;
                for (int k = -half; k <= half; ++k)
                    if (k != 0) angles.push_back(phi + opts.rot_span * k / half);
                std::vector<std::optional<Trial>> trials(angles.size());
                workers.run(angles.size(), [&](const std::size_t t, const std::size_t w) {
                    const FT na(std::cos(angles[t])), nb(std::sin(angles[t]));
                    if (na == FT(0) && nb == FT(0)) return;
                    std::vector<HalfPlane> trial = Workers::local(hp, w);
                    const Point a = Workers::local(anchor, w);
                    trial[i] = {na, nb, na * a.x() + nb * a.y()};
                    const double c0 = approx(trial[i].c);
                    PushResult pr = push(workers.ev(w), trial, i, workers.box(w), c0 - pad,
                                         c0 + pad, opts.push_rounds);
                    if (!pr.ok) return;
                    trial[i].c = pr.c;
                    trials[t] = Trial{std::move(trial), pr.area, std::move(pr.pts)};
                });
                if (auto best = best_trial(trials, area)) {
                    hp = std::move(best->hp);
                    area = best->area;
                    pts = std::move(best->pts);
                    improved = true;
                }
            }

            // ---- SPLIT: replace one edge with two, each pushed ---------------
            //
            // See Options::split_steps.
            if (opts.split_steps && hp.size() < hp_cap && !stopped) {
                const std::size_t n_before = hp.size();
                // `i` walks the current hp, `orig` the edges before this pass
                // (which pinned_edge indexes). They diverge once a split is
                // accepted, so `i` is advanced explicitly.
                std::size_t i = 0;
                for (std::size_t orig = 0; orig < n_before && hp.size() < hp_cap && !stop_now();
                     ++orig, ++i) {
                    // Only pinned edges are worth splitting; an edge the push just
                    // advanced was not at its limit.
                    if (orig < pinned_edge.size() && !pinned_edge[orig]) continue;
                    const double phi = std::atan2(approx(hp[i].b), approx(hp[i].a));
                    const Point anchor = rotation_anchor(pts, hp[i]);

                    // Independent trials, in parallel, as for rotate.
                    std::vector<std::optional<Trial>> trials(opts.split_steps);
                    workers.run(opts.split_steps, [&](const std::size_t t, const std::size_t w) {
                        const std::vector<HalfPlane> cur = Workers::local(hp, w);
                        const Point a = Workers::local(anchor, w);
                        const double d
                            = opts.split_span * static_cast<double>(t + 1) / opts.split_steps;
                        std::vector<HalfPlane> trial;
                        trial.reserve(hp.size() + 1);
                        for (std::size_t j = 0; j < cur.size(); ++j) {
                            if (j != i) {
                                trial.push_back(cur[j]);
                                continue;
                            }
                            for (const double th : {phi - d, phi + d}) {
                                const FT na(std::cos(th)), nb(std::sin(th));
                                trial.push_back({na, nb, na * a.x() + nb * a.y()});
                            }
                        }
                        // push BOTH new half-planes; each can advance past the
                        // contact point that pinned the single edge
                        FT got{0};
                        std::vector<Point> got_pts;
                        for (const std::size_t idx : {i, i + 1}) {
                            const double c0 = approx(trial[idx].c);
                            // Coarse; the next sweep's push refines it.
                            PushResult pr = push(workers.ev(w), trial, idx, workers.box(w),
                                                 c0 - pad, c0 + pad, opts.split_push_rounds);
                            if (!pr.ok) return;
                            trial[idx].c = pr.c;
                            got = pr.area;
                            got_pts = std::move(pr.pts);
                        }
                        trials[t] = Trial{std::move(trial), got, std::move(got_pts)};
                    });
                    if (auto best = best_trial(trials, area)) {
                        hp = std::move(best->hp);
                        area = best->area;
                        pts = std::move(best->pts);
                        improved = true;
                        ++i;  // one edge became two; step over the second
                    }
                }
            }

            // Splitting can leave redundant half-planes.
            prune(hp, box);

            // ---- ADJOIN: absorb a corner of P, adding a VERTEX ---------------
            //
            // See Options::adjoin_rounds. The candidates are P's corners, one
            // certify call each. Runs after split, because it rebuilds the
            // half-plane list and would invalidate pinned_edge.
            //
            // A sequential sweep tests each corner against the current Q and
            // accepts any that gains. Here a batch of corners is tested in parallel
            // against the same Q, the first gain in corner order is accepted, and
            // the sweep resumes after it: the same tests, so the same answer.
            if (opts.adjoin_rounds) {
                for (unsigned round = 0; round < opts.adjoin_rounds && !stopped; ++round) {
                    bool gained = false;
                    std::size_t vi = 0;
                    while (vi < poly.size() && hp.size() < hp_cap && !stop_now()) {
                        std::vector<std::size_t> batch;
                        for (; vi < poly.size() && batch.size() < workers.size(); ++vi)
                            // already a vertex of Q: the hull would be unchanged
                            if (std::find(pts.begin(), pts.end(), poly[vi]) == pts.end())
                                batch.push_back(vi);
                        std::vector<std::optional<std::pair<FT, Polygon>>> got(batch.size());
                        workers.run(batch.size(), [&](const std::size_t t, const std::size_t w) {
                            std::vector<Point> cand = Workers::local(pts, w);
                            cand.push_back(workers.vertex(w, batch[t]));
                            got[t] = workers.ev(w).points(cand);
                        });
                        for (std::size_t t = 0; t < batch.size(); ++t) {
                            if (!got[t] || got[t]->first <= area) continue;
                            area = got[t]->first;
                            pts.assign(got[t]->second.vertices_begin(),
                                       got[t]->second.vertices_end());
                            hp = halfplanes_of(got[t]->second);
                            gained = true;
                            improved = true;
                            vi = batch[t] + 1;
                            break;
                        }
                    }
                    if (!gained) break;
                }
            }

            if (!improved) break;
        }

        res.area = area;
        res.polygon.clear();
        for (const auto &v : pts) res.polygon.push_back(v);
        if (res.polygon.is_clockwise_oriented()) res.polygon.reverse_orientation();
        return res;
    }

}  // namespace potato_peeling::internal::refinement
