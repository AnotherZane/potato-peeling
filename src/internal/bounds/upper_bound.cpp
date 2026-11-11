#include "internal/bounds/upper_bound.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include "internal/geometry.hpp"

// The search makes its decisions (angles, sectors, rankings) in doubles and
// evaluates areas exactly. None of the double arithmetic can make the result
// invalid: every quantity that enters a slack term is rounded up. Ties in the
// heaps and sorts are broken by a total order, so the result does not depend on
// the standard library.

namespace potato_peeling::internal::bounds {
    namespace {
        constexpr double PI_D = 3.141592653589793;  // π rounded to a double
        constexpr double TAU = 2.0 * PI_D;

        // Widens every angular gap. A sample at nominal angle t uses the rounded
        // direction (cos t, sin t), whose angle is about 1e-16 from t; without
        // this the gap, and so the slack, could be understated by that much.
        const FT ANG_EPS = FT(1) / (FT(1000000) * FT(1000000));  // 1/10^12

        // An angle in [0, 2π] (2π itself only by rounding a tiny negative angle).
        double wrap_angle(const double a) {
            const double m = std::fmod(a, TAU);
            return m < 0.0 ? m + TAU : m;
        }

        // A double within an ulp of v, for steering the search.
        double approx(const FT &v) {
            return CGAL::to_double(CGAL::exact(v));
        }

        FT shoelace(const std::vector<Point> &pts) {
            const std::size_t n = pts.size();
            if (n < 3) return FT(0);
            FT s(0);
            for (std::size_t i = 0; i < n; ++i) {
                const Point &a = pts[i], &b = pts[(i + 1) % n];
                s += a.x() * b.y() - b.x() * a.y();
            }
            return s / FT(2);
        }

        // Sutherland-Hodgman clip of a closed path against the closed half-plane
        // left of the directed line (u, d). For a concave subject the extra edges
        // lie on the clip line and add no area.
        std::vector<Point> clip_halfplane(const std::vector<Point> &pts, const Point &u,
                                          const Vector &d) {
            const auto side
                = [&](const Point &p) { return d.x() * (p.y() - u.y()) - d.y() * (p.x() - u.x()); };
            std::vector<Point> out;
            const std::size_t n = pts.size();
            for (std::size_t i = 0; i < n; ++i) {
                const Point &a = pts[i], &b = pts[(i + 1) % n];
                const FT sa = side(a), sb = side(b);
                if (sa >= FT(0)) out.push_back(a);
                if ((sa > FT(0) && sb < FT(0)) || (sa < FT(0) && sb > FT(0))) {
                    const FT t = sa / (sa - sb);
                    out.emplace_back(a.x() + t * (b.x() - a.x()), a.y() + t * (b.y() - a.y()));
                }
            }
            return out;
        }

        Vector direction(const double theta) {
            return {FT(std::cos(theta)), FT(std::sin(theta))};
        }

        FT halfplane_area(const std::vector<Point> &pts, const Point &u, const double theta) {
            return CGAL::abs(shoelace(clip_halfplane(pts, u, direction(theta))));
        }

        FT halfplane_area2(const std::vector<Point> &pts, const Point &u, const double a,
                           const Point &v, const double b) {
            std::vector<Point> path = clip_halfplane(pts, u, direction(a));
            if (path.size() < 3) return FT(0);
            path = clip_halfplane(path, v, direction(b));
            if (path.size() < 3) return FT(0);
            return CGAL::abs(shoelace(path));
        }

        FT max_radius_sq(const std::vector<Point> &pts, const Point &u) {
            FT best = CGAL::squared_distance(pts.front(), u);
            for (const Point &p : pts) best = std::max(best, CGAL::squared_distance(p, u));
            return best;
        }

        // The Lipschitz constant of the swept area. As the line through u turns by
        // dθ, the area of P ∩ H+ changes by at most (r²/2) dθ, where r is the
        // largest distance from u to a point of P in the swept directions, forward
        // or backward along the line. P's farthest point from u is a vertex, so
        // R_u² / 2 bounds it everywhere; over a narrow range of directions the
        // farthest point is usually much closer, which this class bounds per
        // sector of directions (nsec sectors of 2π/nsec).
        //
        // Per sector it takes P's convex hull (which contains P, so is no closer):
        // the farthest hull point within a sector is a hull vertex inside it or
        // where one of the sector's two bounding rays leaves the hull. Computed in
        // doubles; the caller scales the result by (1 + 1e-9), which covers their
        // rounding.
        class RadialBound {
        public:
            RadialBound(const std::vector<Point> &pts, const Point &u, const int nsec = 256)
                : nsec_(nsec), sec_(static_cast<std::size_t>(nsec), 0.0) {
                const double glob = approx(max_radius_sq(pts, u));
                const std::vector<Point> hull = convex_hull(pts);
                const double ux = approx(u.x()), uy = approx(u.y());
                // hull half-planes n.x <= c, outward normals (hull is CCW)
                std::vector<std::array<double, 3>> hp;
                for (std::size_t i = 0; i < hull.size(); ++i) {
                    const Point &a = hull[i], &b = hull[(i + 1) % hull.size()];
                    const FT nx = b.y() - a.y(), ny = a.x() - b.x();
                    hp.push_back({approx(nx), approx(ny), approx(nx * a.x() + ny * a.y())});
                }
                const auto ray_exit_sq = [&](const double th) {
                    const double dx = std::cos(th), dy = std::sin(th);
                    bool any = false;
                    double best = 0.0;
                    for (const auto &[nx, ny, c] : hp) {
                        const double den = nx * dx + ny * dy;
                        if (den <= 0.0) continue;
                        const double t = (c - nx * ux - ny * uy) / den;
                        if (t >= 0.0 && (!any || t < best)) {
                            best = t;
                            any = true;
                        }
                    }
                    // Impossible for a bounded hull; the global radius, never 0,
                    // which would understate the bound.
                    return any ? best * best : glob;
                };
                for (const Point &v : hull) {
                    const double dx = approx(v.x() - u.x());
                    const double dy = approx(v.y() - u.y());
                    if (dx == 0.0 && dy == 0.0) continue;
                    const auto k = static_cast<std::size_t>(static_cast<long>(
                                       wrap_angle(std::atan2(dy, dx)) / TAU * nsec_))
                                   % sec_.size();
                    sec_[k] = std::max(sec_[k], dx * dx + dy * dy);
                }
                for (long k = 0; k < nsec_; ++k) {
                    const double lo = TAU * static_cast<double>(k) / static_cast<double>(nsec_);
                    const double hi = TAU * static_cast<double>(k + 1) / static_cast<double>(nsec_);
                    auto &s = sec_[static_cast<std::size_t>(k)];
                    s = std::max({s, ray_exit_sq(lo), ray_exit_sq(hi)});
                }
            }

            // Bound on r² for directions in [a1, a2] and in [a1 + π, a2 + π] (the
            // backward ray): the max over the sectors those ranges touch, walking
            // from the start sector to the end sector with wrap-around.
            [[nodiscard]] double over(const double a1, const double a2) const {
                const double span = a2 - a1;
                if (span >= PI_D) return *std::max_element(sec_.begin(), sec_.end());
                double best = 0.0;
                for (const double base : {a1, a1 + PI_D}) {
                    const long k0 = static_cast<long>(wrap_angle(base) / TAU * nsec_);
                    const long k1 = static_cast<long>(wrap_angle(base + span) / TAU * nsec_);
                    for (long k = k0;; ++k) {
                        best = std::max(best, sec_[static_cast<std::size_t>(k % nsec_)]);
                        if (k % nsec_ == k1 % nsec_) break;
                        if (k + 1 - k0 > nsec_) break;
                    }
                }
                return best;
            }

        private:
            long nsec_;
            std::vector<double> sec_;
        };

        struct PivotResult {
            FT bound, best;
        };

        // max over θ of area(P ∩ H+(u, θ)), bounded from above by branch and bound.
        //
        // The area f(θ) is Lipschitz with L = R_u²/2. On an interval [lo, hi] with
        // known end values, f ≤ f(lo) + L(θ - lo) and f ≤ f(hi) + L(hi - θ), and the
        // two meet at most at (f(lo) + f(hi) + L(hi - lo)) / 2: the "sawtooth" bound.
        // The interval with the largest bound is split at its midpoint until that
        // bound is within tol_abs of the best sample. Every interval's maximum is at
        // most its bound, so max(best sample, largest open bound) is a valid upper
        // bound at any point, which is what is returned.
        //
        // Also stops when the best sample exceeds `cutoff` (the best bound over the
        // pivots so far: U(u) is then at least that, so u cannot lower the minimum
        // and its exact value does not matter) or after max_evals samples.
        PivotResult u_pivot(const std::vector<Point> &pts, const Point &u, const FT &tol_abs,
                            const FT &cutoff, const std::size_t max_evals, const StopFn &stop) {
            const FT L = max_radius_sq(pts, u) / FT(2);

            // Start from the angles at which the line passes through a vertex (where
            // f changes form), both ways round, plus 32 uniform angles so no wide
            // gap is left.
            std::set<double> seeds;
            for (const Point &v : pts) {
                if (v == u) continue;
                const double a
                    = wrap_angle(std::atan2(approx(v.y() - u.y()), approx(v.x() - u.x())));
                seeds.insert(a);
                seeds.insert(wrap_angle(a + PI_D));
            }
            for (int k = 0; k < 32; ++k) seeds.insert(TAU * k / 32);
            std::vector<double> grid(seeds.begin(), seeds.end());
            grid.push_back(grid.front() + TAU);

            std::vector<FT> vals;
            for (std::size_t i = 0; i + 1 < grid.size(); ++i) {
                check_stop(stop);
                vals.push_back(halfplane_area(pts, u, grid[i]));
            }
            vals.push_back(vals.front());
            std::size_t evals = grid.size() - 1;
            FT best = *std::max_element(vals.begin(), vals.end());

            const auto sawtooth
                = [&](const FT &alo, const FT &ahi, const double lo, const double hi) {
                      return (alo + ahi + L * (FT(hi) - FT(lo) + ANG_EPS)) / FT(2);
                  };

            // The largest bound first; ties broken by (t1, t2, a1, a2).
            using Item = std::tuple<FT, double, double, FT, FT>;
            const auto later = [](const Item &x, const Item &y) {
                if (std::get<0>(x) != std::get<0>(y)) return std::get<0>(x) < std::get<0>(y);
                return std::tie(std::get<1>(x), std::get<2>(x), std::get<3>(x), std::get<4>(x))
                       > std::tie(std::get<1>(y), std::get<2>(y), std::get<3>(y), std::get<4>(y));
            };
            std::priority_queue<Item, std::vector<Item>, decltype(later)> heap(later);
            for (std::size_t i = 0; i + 1 < grid.size(); ++i)
                heap.emplace(sawtooth(vals[i], vals[i + 1], grid[i], grid[i + 1]), grid[i],
                             grid[i + 1], vals[i], vals[i + 1]);

            while (!heap.empty()) {
                check_stop(stop);
                const auto [bound, t1, t2, a1, a2] = heap.top();
                if (bound <= best + tol_abs) break;
                if (best > cutoff) break;
                if (evals >= max_evals) break;
                heap.pop();
                const double tm = (t1 + t2) / 2;
                const FT am = halfplane_area(pts, u, tm);
                ++evals;
                best = std::max(best, am);
                heap.emplace(sawtooth(a1, am, t1, tm), t1, tm, a1, am);
                heap.emplace(sawtooth(am, a2, tm, t2), tm, t2, am, a2);
            }
            return {heap.empty() ? best : std::max(best, std::get<0>(heap.top())), best};
        }

        // max over (a, b) of area(P ∩ H+(u, a) ∩ H+(v, b)): the pivot bound for two
        // pivots at once, by branch and bound on boxes [a1, a2] × [b1, b2].
        //
        // f is Lipschitz in a with constant Lu and in b with Lv. From the corner
        // (a1, b1), f ≤ f11 + Lu(a - a1) + Lv(b - b1); from the opposite corner,
        // f ≤ f22 + Lu(a2 - a) + Lv(b2 - b). Adding the two halves the slack:
        // f ≤ (f11 + f22 + Lu(a2 - a1) + Lv(b2 - b1)) / 2, and the same with the
        // other diagonal (f12, f21); the box bound is the smaller. Here fij is
        // f(a_i, b_j). Lu and Lv are per box: RadialBound over the box's range of
        // angles, never above the global R²/2.
        //
        // Stops, and returns, as u_pivot does; max_evals counts distinct samples.
        FT uv_pair(const std::vector<Point> &pts, const Point &u, const Point &v, const FT &tol_abs,
                   const FT &cutoff, const std::size_t max_evals, const StopFn &stop) {
            constexpr int INIT = 12;
            const RadialBound rbu(pts, u), rbv(pts, v);
            const FT lu_glob = max_radius_sq(pts, u) / FT(2);
            const FT lv_glob = max_radius_sq(pts, v) / FT(2);
            const auto lu = [&](const double a1, const double a2) {
                return std::min(lu_glob, FT(rbu.over(a1, a2) * (1 + 1e-9)) / FT(2) + ANG_EPS);
            };
            const auto lv = [&](const double b1, const double b2) {
                return std::min(lv_glob, FT(rbv.over(b1, b2) * (1 + 1e-9)) / FT(2) + ANG_EPS);
            };

            // Neighbouring boxes share corners, so samples are cached.
            std::map<std::pair<double, double>, FT> cache;
            const auto f = [&](const double a, const double b) -> FT {
                const auto it = cache.find({a, b});
                if (it != cache.end()) return it->second;
                return cache[{a, b}] = halfplane_area2(pts, u, a, v, b);
            };
            const auto box_bound = [&](const double a1, const double a2, const double b1,
                                       const double b2, const std::array<FT, 4> &fv) {
                const FT slack = lu(a1, a2) * (FT(a2) - FT(a1) + ANG_EPS)
                                 + lv(b1, b2) * (FT(b2) - FT(b1) + ANG_EPS);
                return std::min(fv[0] + fv[3], fv[1] + fv[2]) / FT(2) + slack / FT(2);
            };

            // The largest bound first; ties broken by (a1, a2, b1, b2, values).
            using Item = std::tuple<FT, double, double, double, double, std::array<FT, 4>>;
            const auto later = [](const Item &x, const Item &y) {
                if (std::get<0>(x) != std::get<0>(y)) return std::get<0>(x) < std::get<0>(y);
                return std::tie(std::get<1>(x), std::get<2>(x), std::get<3>(x), std::get<4>(x),
                                std::get<5>(x))
                       > std::tie(std::get<1>(y), std::get<2>(y), std::get<3>(y), std::get<4>(y),
                                  std::get<5>(y));
            };
            std::priority_queue<Item, std::vector<Item>, decltype(later)> heap(later);

            std::vector<double> grid(INIT + 1);
            for (int k = 0; k <= INIT; ++k) grid[k] = TAU * k / INIT;
            FT best(0);
            for (int i = 0; i < INIT; ++i) {
                check_stop(stop);
                for (int j = 0; j < INIT; ++j) {
                    const double a1 = grid[i], a2 = grid[i + 1], b1 = grid[j], b2 = grid[j + 1];
                    const std::array<FT, 4> fv{f(a1, b1), f(a1, b2), f(a2, b1), f(a2, b2)};
                    for (const FT &x : fv) best = std::max(best, x);
                    heap.emplace(box_bound(a1, a2, b1, b2, fv), a1, a2, b1, b2, fv);
                }
            }

            while (!heap.empty()) {
                check_stop(stop);
                const auto [bound, a1, a2, b1, b2, fv] = heap.top();
                if (bound <= best + tol_abs) break;
                if (best > cutoff) break;
                if (cache.size() >= max_evals) break;
                heap.pop();
                const auto &[f11, f12, f21, f22] = fv;
                // Split along the angle carrying more of the slack; two new samples.
                if (lu(a1, a2) * (FT(a2) - FT(a1)) >= lv(b1, b2) * (FT(b2) - FT(b1))) {
                    const double am = (a1 + a2) / 2;
                    const FT m1 = f(am, b1), m2 = f(am, b2);
                    best = std::max({best, m1, m2});
                    const std::array<FT, 4> lo{f11, f12, m1, m2}, hi{m1, m2, f21, f22};
                    heap.emplace(box_bound(a1, am, b1, b2, lo), a1, am, b1, b2, lo);
                    heap.emplace(box_bound(am, a2, b1, b2, hi), am, a2, b1, b2, hi);
                } else {
                    const double bm = (b1 + b2) / 2;
                    const FT m1 = f(a1, bm), m2 = f(a2, bm);
                    best = std::max({best, m1, m2});
                    const std::array<FT, 4> lo{f11, m1, f21, m2}, hi{m1, f12, m2, f22};
                    heap.emplace(box_bound(a1, a2, b1, bm, lo), a1, a2, b1, bm, lo);
                    heap.emplace(box_bound(a1, a2, bm, b2, hi), a1, a2, bm, b2, hi);
                }
            }
            return heap.empty() ? best : std::max(best, std::get<0>(heap.top()));
        }

        // The width bound for direction w = (p, q): Q*'s slice by any line
        // perpendicular to w is one interval inside P, hence inside P's longest
        // slice segment, so A* <= the integral of that longest length over the
        // levels (Cavalieri).
        //
        // Works in the frame t = x·w (the level) and s = x·v, v = w turned 90°, both
        // scaled by |w|, so everything stays rational; the integral is divided by
        // |w|² at the end. Between consecutive vertex levels the edges crossed and
        // their order are fixed, so each segment length is linear in t and the
        // longest, a max of linear functions, is convex: below its chord, so the
        // trapezoid over the cell bounds its integral exactly.
        FT width_bound_dir(const std::vector<Point> &pts, const FT &p, const FT &q) {
            const FT vx = -q, vy = p, w2 = p * p + q * q;
            const std::size_t n = pts.size();
            std::vector<FT> t;
            for (const Point &v : pts) t.push_back(v.x() * p + v.y() * q);
            std::vector<FT> events = t;
            std::sort(events.begin(), events.end());
            events.erase(std::unique(events.begin(), events.end()), events.end());
            if (events.size() < 2) return FT(0);

            FT total(0);
            for (std::size_t e = 0; e + 1 < events.size(); ++e) {
                const FT a = events[e], b = events[e + 1], tm = (a + b) / FT(2);
                std::vector<std::pair<FT, FT>>
                    lin;  // s along each crossed edge, as s = slope t + c
                for (std::size_t i = 0; i < n; ++i) {
                    const std::size_t j = (i + 1) % n;
                    if ((t[i] < tm) == (t[j] < tm)) continue;
                    const FT ex = pts[j].x() - pts[i].x(), ey = pts[j].y() - pts[i].y();
                    const FT slope = (ex * vx + ey * vy) / (ex * p + ey * q);
                    lin.emplace_back(slope, pts[i].x() * vx + pts[i].y() * vy - slope * t[i]);
                }
                // tm avoids vertices, so the count is even; skipping the cell
                // would under-count.
                if (lin.size() % 2) throw certification_error("upper_bound: odd crossing count");
                if (lin.empty()) continue;
                // In order along the slice, crossings alternate entering and leaving
                // P, so the segments inside P are (0,1), (2,3), ...
                std::stable_sort(lin.begin(), lin.end(), [&](const auto &x, const auto &y) {
                    return x.first * tm + x.second < y.first * tm + y.second;
                });
                FT fa, fb;  // the longest segment's length at the cell's two ends
                for (std::size_t k = 0; k + 1 < lin.size(); k += 2) {
                    const FT s = lin[k + 1].first - lin[k].first;
                    const FT c = lin[k + 1].second - lin[k].second;
                    if (k == 0 || s * a + c > fa) fa = s * a + c;
                    if (k == 0 || s * b + c > fb) fb = s * b + c;
                }
                total += (fa + fb) * (b - a) / FT(2);
            }
            return total / w2;
        }

        // Double twin of width_bound_dir, used only to rank directions: any
        // direction gives a valid bound, so ranking needs no exactness.
        double width_float(const std::vector<std::array<double, 2>> &pts, const double p,
                           const double q) {
            const double vx = -q, vy = p, w2 = p * p + q * q;
            const std::size_t n = pts.size();
            std::vector<double> t;
            for (const auto &[x, y] : pts) t.push_back(x * p + y * q);
            std::vector<double> events = t;
            std::sort(events.begin(), events.end());
            events.erase(std::unique(events.begin(), events.end()), events.end());
            if (events.size() < 2) return HUGE_VAL;
            double total = 0.0;
            for (std::size_t e = 0; e + 1 < events.size(); ++e) {
                const double a = events[e], b = events[e + 1], tm = (a + b) / 2;
                std::vector<std::pair<double, double>> lin;
                for (std::size_t i = 0; i < n; ++i) {
                    const std::size_t j = (i + 1) % n;
                    if ((t[i] < tm) == (t[j] < tm)) continue;
                    const double ex = pts[j][0] - pts[i][0], ey = pts[j][1] - pts[i][1];
                    const double ew = ex * p + ey * q;
                    if (ew == 0.0) continue;
                    const double slope = (ex * vx + ey * vy) / ew;
                    lin.emplace_back(slope, pts[i][0] * vx + pts[i][1] * vy - slope * t[i]);
                }
                if (lin.size() < 2 || lin.size() % 2) continue;
                std::stable_sort(lin.begin(), lin.end(), [&](const auto &x, const auto &y) {
                    return x.first * tm + x.second < y.first * tm + y.second;
                });
                double fa = 0.0, fb = 0.0;
                for (std::size_t k = 0; k + 1 < lin.size(); k += 2) {
                    const double s = lin[k + 1].first - lin[k].first;
                    const double c = lin[k + 1].second - lin[k].second;
                    if (k == 0 || s * a + c > fa) fa = s * a + c;
                    if (k == 0 || s * b + c > fb) fb = s * b + c;
                }
                total += (fa + fb) * (b - a) / 2;
            }
            return total / w2;
        }

        // min over directions of the width bound: P's edge directions and normals
        // plus n_uniform uniform directions, ranked in doubles, the best exact_top
        // evaluated exactly. The uniform directions are rounded to multiples of
        // 10^-7 so they are small rationals (cos and sin are irrational).
        FT width_bound(const std::vector<Point> &pts, const unsigned n_uniform, const StopFn &stop,
                       const std::size_t exact_top = 8) {
            std::vector<std::pair<FT, FT>> dirs;
            std::set<std::pair<FT, FT>> seen;
            const auto add = [&](const FT &x, const FT &y) {
                if ((x != FT(0) || y != FT(0)) && seen.insert({x, y}).second)
                    dirs.emplace_back(x, y);
            };
            const std::size_t n = pts.size();
            for (std::size_t i = 0; i < n; ++i) {
                const FT ex = pts[(i + 1) % n].x() - pts[i].x();
                const FT ey = pts[(i + 1) % n].y() - pts[i].y();
                add(ex, ey);
                add(-ey, ex);
            }
            const FT scale(10000000);
            for (unsigned k = 0; k < n_uniform; ++k) {
                const double th = PI_D * k / n_uniform;
                add(FT(std::nearbyint(std::cos(th) * 1e7)) / scale,
                    FT(std::nearbyint(std::sin(th) * 1e7)) / scale);
            }
            if (dirs.empty()) return CGAL::abs(shoelace(pts));

            std::vector<std::array<double, 2>> fpts;
            for (const Point &v : pts) fpts.push_back({approx(v.x()), approx(v.y())});
            std::vector<std::pair<double, std::size_t>> scored;
            for (std::size_t i = 0; i < dirs.size(); ++i) {
                check_stop(stop);
                scored.emplace_back(
                    width_float(fpts, approx(dirs[i].first), approx(dirs[i].second)), i);
            }
            std::stable_sort(scored.begin(), scored.end(),
                             [](const auto &x, const auto &y) { return x.first < y.first; });
            FT best;
            for (std::size_t r = 0; r < std::min(exact_top, scored.size()); ++r) {
                const FT val = width_bound_dir(pts, dirs[scored[r].second].first,
                                               dirs[scored[r].second].second);
                if (r == 0 || val < best) best = val;
            }
            return best;
        }
    }  // namespace

    Result upper_bound(const Polygon &p, const Options &options) {
        // 0 is allowed: the search then runs until the caps.
        if (!(options.rel_tol >= 0.0 && std::isfinite(options.rel_tol)))
            throw std::invalid_argument("upper_bound: rel_tol must be finite and >= 0");
        Polygon ccw = p;
        if (ccw.is_clockwise_oriented()) ccw.reverse_orientation();
        const std::vector<Point> pts(ccw.vertices_begin(), ccw.vertices_end());
        const std::size_t n = pts.size();
        const FT area_p = CGAL::abs(shoelace(pts));
        Result res{area_p, area_p, area_p};

        // Pivots: the reflex corners (a convex corner lies on P's hull, so its
        // half-planes can cut nothing off P), plus edge_samples points per edge.
        std::vector<Point> pivots;
        for (std::size_t i = 0; i < n; ++i)
            if (CGAL::orientation(pts[(i + n - 1) % n], pts[i], pts[(i + 1) % n])
                == CGAL::RIGHT_TURN)
                pivots.push_back(pts[i]);
        if (pivots.empty()) return res;  // convex: A* = area(P)
        for (std::size_t i = 0; i < n && options.edge_samples; ++i) {
            const Point &a = pts[i], &b = pts[(i + 1) % n];
            for (unsigned k = 1; k <= options.edge_samples; ++k) {
                const FT lam
                    = FT(static_cast<int>(k)) / FT(static_cast<int>(options.edge_samples + 1));
                pivots.emplace_back(a.x() + lam * (b.x() - a.x()), a.y() + lam * (b.y() - a.y()));
            }
        }

        const FT tol_abs = area_p * FT(options.rel_tol);
        FT best = area_p;
        std::vector<FT> samples;
        for (const Point &u : pivots) {
            const PivotResult r
                = u_pivot(pts, u, tol_abs, best, options.max_evals_1d, options.should_stop);
            samples.push_back(r.best);
            best = std::min(best, r.bound);
        }

        if (options.pairs && pivots.size() >= 2) {
            // Rank by best sample: with the cutoff, most pivots' bounds are loose.
            std::vector<std::size_t> order(pivots.size());
            for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(
                order.begin(), order.end(),
                [&](const std::size_t a, const std::size_t b) { return samples[a] < samples[b]; });
            // pairs = 1 still pairs the best two (one pivot makes no pair).
            const std::size_t m = std::min<std::size_t>(order.size(), std::max(2u, options.pairs));
            for (std::size_t x = 0; x < m; ++x)
                for (std::size_t y = x + 1; y < m; ++y)
                    best = std::min(best, uv_pair(pts, pivots[order[x]], pivots[order[y]], tol_abs,
                                                  best, options.max_evals_2d, options.should_stop));
        }
        res.pivot = best;

        res.width = width_bound(pts, options.width_dirs, options.should_stop);
        res.bound = std::min(res.pivot, res.width);
        return res;
    }

}  // namespace potato_peeling::internal::bounds
