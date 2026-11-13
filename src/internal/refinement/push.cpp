#include "internal/refinement/push.hpp"

#include <algorithm>
#include <optional>
#include <utility>

namespace potato_peeling::internal::refinement {

    PushResult push(const Evaluator &ev, std::vector<HalfPlane> hp, const std::size_t i,
                    const std::vector<Point> &box, double lo, double hi, const unsigned rounds) {
        PushResult best;
        hp[i].c = FT(hi);
        if (auto v = ev(hp, box)) {  // unconstrained in this direction
            best = {true, FT(hi), v->first, std::move(v->second)};
            return best;
        }
        for (unsigned r = 0; r < rounds; ++r) {
            const double mid = (lo + hi) / 2.0;
            hp[i].c = FT(mid);
            if (auto v = ev(hp, box)) {
                lo = mid;
                best = {true, FT(mid), v->first, std::move(v->second)};
            } else {
                hi = mid;
            }
        }
        return best;
    }

    PushResult push(Workers &workers, const std::vector<HalfPlane> &hp, const std::size_t i,
                    double lo, double hi, const unsigned rounds) {
        PushResult best;
        {
            std::vector<HalfPlane> h = hp;
            h[i].c = FT(hi);
            if (auto v = workers.ev(0)(h, workers.box(0)))  // unconstrained in this direction
                return {true, FT(hi), v->first, std::move(v->second)};
        }
        unsigned depth = 1;
        while (depth < 16 && (2u << depth) - 1 <= workers.size()) ++depth;
        for (unsigned r = 0; r < rounds;) {
            const unsigned d = std::min(depth, rounds - r);
            // Node k (from 1) bisects [lo_k, hi_k]; its children 2k and 2k+1 are the
            // halves taken when its midpoint is infeasible or feasible.
            const std::size_t nodes = (std::size_t{1} << d) - 1;
            std::vector<double> nlo(nodes + 1), nhi(nodes + 1), mid(nodes + 1);
            nlo[1] = lo;
            nhi[1] = hi;
            for (std::size_t k = 1; k <= nodes; ++k) {
                mid[k] = (nlo[k] + nhi[k]) / 2.0;
                if (2 * k + 1 <= nodes) {
                    nlo[2 * k] = nlo[k];
                    nhi[2 * k] = mid[k];
                    nlo[2 * k + 1] = mid[k];
                    nhi[2 * k + 1] = nhi[k];
                }
            }
            std::vector<std::optional<std::pair<FT, std::vector<Point>>>> got(nodes + 1);
            workers.run(nodes, [&](const std::size_t t, const std::size_t w) {
                std::vector<HalfPlane> h = Workers::local(hp, w);
                h[i].c = FT(mid[t + 1]);
                got[t + 1] = workers.ev(w)(h, workers.box(w));
            });
            std::size_t k = 1;
            for (unsigned level = 0; level < d; ++level) {
                if (got[k]) {
                    lo = mid[k];
                    best = {true, FT(mid[k]), got[k]->first, std::move(got[k]->second)};
                    k = 2 * k + 1;
                } else {
                    hi = mid[k];
                    k = 2 * k;
                }
            }
            r += d;
        }
        return best;
    }

}  // namespace potato_peeling::internal::refinement
