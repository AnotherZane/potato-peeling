#include "potato_peeling/potato_peeling.hpp"

#include <string>
#include <utility>

#include "internal/aronov/aronov.hpp"
#include "internal/bounds/upper_bound.hpp"
#include "internal/cabello/cabello.hpp"
#include "internal/cabello/certify.hpp"
#include "internal/chang_and_yap/chang_and_yap.hpp"
#include "internal/geometry.hpp"
#include "internal/refinement/refinement.hpp"

namespace potato_peeling {
    namespace {
        // The result contract: CCW, no repeated or collinear vertices, convex and
        // contained in P (checked exactly), with its exact area. The exact values
        // are forced, which also frees the lazy history behind them.
        Skull make_skull(const char *method, const Polygon &p, const Polygon &q) {
            Polygon polygon = internal::remove_repeated_vertices(q);
            internal::remove_collinear_vertices(polygon);

            if (!internal::cabello::is_convex_subset(p, polygon))
                throw certification_error(std::string(method)
                                          + ": result is not a convex subset of P");
            if (polygon.is_clockwise_oriented()) polygon.reverse_orientation();

            for (const Point &v : polygon) v.exact();
            FT area = polygon.area();
            CGAL::exact(area);
            return {std::move(polygon), std::move(area)};
        }
    }  // namespace

    Skull exact_convex_skull(const Polygon &p) {
        const Polygon clean = internal::clean_polygon(p);
        return make_skull("exact_convex_skull", clean,
                          internal::chang_and_yap::convex_skull(clean));
    }

    Skull mesh_convex_skull(const Polygon &p) {
        const Polygon clean = internal::clean_polygon(p);
        return make_skull("mesh_convex_skull", clean, internal::aronov::convex_skull(clean));
    }

    Skull mesh_convex_skull(const Polygon &p, const Mesh &mesh) {
        const Polygon clean = internal::clean_polygon(p);
        return make_skull("mesh_convex_skull", clean,
                          internal::aronov::convex_skull(clean, mesh.points, mesh.triangles));
    }

    Skull approximate_convex_skull(const Polygon &p, const ApproximateOptions &options) {
        const Polygon clean = internal::clean_polygon(p);
        internal::cabello::Options opt;
        opt.epsilon = options.epsilon;
        opt.delta = options.delta;
        opt.seed = options.seed;
        opt.sample_scale = options.sample_scale;
        opt.s_scale = options.s_scale;
        opt.max_edges = options.max_edges;
        opt.threads = options.threads;
        const internal::cabello::Result res = internal::cabello::large_potato(clean, opt);
        if (!res.convex || !res.contained)
            throw certification_error("approximate_convex_skull: " + res.message);
        return make_skull("approximate_convex_skull", clean, res.polygon);
    }

    Skull refine(const Polygon &p, const Polygon &start, const RefineOptions &options) {
        const Polygon clean = internal::clean_polygon(p);
        // The search keeps P's collinear vertices: adjoin tries each vertex of P,
        // and those on P's edges can be good candidates.
        const Polygon searched = internal::remove_repeated_vertices(p);
        internal::refinement::Options opt;
        opt.max_sweeps = options.max_sweeps;
        opt.rot_steps = options.rot_steps;
        opt.rot_span = options.rot_span;
        opt.split_steps = options.split_steps;
        opt.adjoin_rounds = options.adjoin_rounds;
        return make_skull("refine", clean,
                          internal::refinement::refine(searched, start, opt).polygon);
    }

    FT upper_bound(const Polygon &p, const UpperBoundOptions &options) {
        internal::bounds::Options opt;
        opt.rel_tol = options.rel_tol;
        opt.pairs = options.pairs;
        opt.width_dirs = options.width_dirs;
        opt.edge_samples = options.edge_samples;
        opt.max_evals_1d = options.max_evals_1d;
        opt.max_evals_2d = options.max_evals_2d;
        FT bound = internal::bounds::upper_bound(internal::clean_polygon(p), opt).bound;
        CGAL::exact(bound);
        return bound;
    }

    Skull convex_skull(const Polygon &p, const ConvexSkullOptions &options) {
        ApproximateOptions approximate;
        approximate.seed = options.seed;
        approximate.threads = options.threads;
        RefineOptions refinement;
        refinement.adjoin_rounds = 8;
        return refine(p, approximate_convex_skull(p, approximate).polygon, refinement);
    }

}  // namespace potato_peeling
