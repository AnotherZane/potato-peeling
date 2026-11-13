#include "internal/refinement/evaluator.hpp"

#include "internal/cabello/certify.hpp"

namespace potato_peeling::internal::refinement {

    std::optional<std::pair<FT, std::vector<Point>>>
    Evaluator::operator()(const std::vector<HalfPlane> &hp, const std::vector<Point> &box) const {
        std::vector<Point> pts = realise(hp, box);
        if (pts.size() < 3) return std::nullopt;
        const cabello::Certificate cert = cabello::certify(p_, pts);
        if (!cert.convex || !cert.contained) return std::nullopt;
        return std::make_pair(cert.area, std::move(pts));
    }

    std::optional<std::pair<FT, Polygon>> Evaluator::points(const std::vector<Point> &cand) const {
        const cabello::Certificate cert = cabello::certify(p_, cand);
        if (!cert.convex || !cert.contained) return std::nullopt;
        return std::make_pair(cert.area, cert.polygon);
    }

    Workers::Workers(const Polygon &p, const std::vector<Point> &box, const unsigned threads)
        : pool_(threads), poly_(pool_.size()), box_(pool_.size()) {
        for (std::size_t w = 0; w < pool_.size(); ++w) {
            for (const Point &v : p) poly_[w].push_back(local(v, w));
            box_[w] = local(box, w);
        }
        // Evaluators refer to poly_, which no longer changes.
        ev_.reserve(pool_.size());
        for (const Polygon &q : poly_) ev_.emplace_back(q);
    }

}  // namespace potato_peeling::internal::refinement
