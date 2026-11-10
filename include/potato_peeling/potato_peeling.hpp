#pragma once

// The largest convex polygon inside a simple polygon P (P's convex skull).
//
// Input: a simple polygon without holes, either orientation; repeated and
// collinear vertices are removed first. Fewer than 3 distinct vertices, or a
// self-intersecting polygon, throws invalid_polygon. A convex P returns P.
// Every method also accepts a CGAL::Polygon_2 over any other kernel, converted
// to the exact kernel first (exact for double coordinates).
//
// Every result is certified exactly: convex and contained in P. A failed check
// throws certification_error. The same input and options give the same output
// on a given build.

#include <type_traits>

#include "potato_peeling/errors.hpp"
#include "potato_peeling/export.hpp"
#include "potato_peeling/options.hpp"
#include "potato_peeling/types.hpp"

namespace potato_peeling {

    // The best general-purpose method: certified, and close to optimal in
    // practice (see ConvexSkullOptions).
    POTATO_PEELING_EXPORT Skull convex_skull(const Polygon &p,
                                             const ConvexSkullOptions &options = {});

    // Exact optimum. Chang & Yap (1986). Practical up to a few dozen vertices.
    POTATO_PEELING_EXPORT Skull exact_convex_skull(const Polygon &p);

    // Optimum among unions of triangles of P's constrained Delaunay
    // triangulation. Aronov, van Kreveld, Löffler & Silveira (2011).
    POTATO_PEELING_EXPORT Skull mesh_convex_skull(const Polygon &p);

    // The same over the caller's triangulation of P. Throws invalid_polygon unless
    // the triangles tile P exactly (no gaps, overlaps or T-junctions). A finer mesh
    // can only give a larger answer when it refines the coarser one; time grows
    // steeply with the number of triangles.
    POTATO_PEELING_EXPORT Skull mesh_convex_skull(const Polygon &p, const Mesh &mesh);

    // Randomised approximation. Cabello, Cibulka, Kynčl, Saumell & Valtr (2017).
    POTATO_PEELING_EXPORT Skull approximate_convex_skull(const Polygon &p,
                                                         const ApproximateOptions &options = {});

    // Local ascent from `start`, a convex polygon inside P; never returns less
    // than start. A start that is not a convex subset of P throws invalid_polygon.
    POTATO_PEELING_EXPORT Skull refine(const Polygon &p, const Polygon &start,
                                       const RefineOptions &options = {});

    // The largest convex polygon in P whose vertices are vertices of P: a lower
    // bound on the skull, never below mesh_convex_skull(p). Fischer;
    // Bautista-Santiago et al. O(n^3 log n).
    POTATO_PEELING_EXPORT Skull vertex_convex_skull(const Polygon &p);

    // A certified upper bound on the area of P's convex skull: pivot, two-pivot
    // and width relaxations, computed exactly. A convex P returns area(P).
    POTATO_PEELING_EXPORT FT upper_bound(const Polygon &p, const UpperBoundOptions &options = {});

    // ── The same methods for polygons over another CGAL kernel ──────────────

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    Skull convex_skull(const CGAL::Polygon_2<K, C> &p, const ConvexSkullOptions &options = {}) {
        return convex_skull(to_polygon(p), options);
    }

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    Skull exact_convex_skull(const CGAL::Polygon_2<K, C> &p) {
        return exact_convex_skull(to_polygon(p));
    }

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    Skull mesh_convex_skull(const CGAL::Polygon_2<K, C> &p) {
        return mesh_convex_skull(to_polygon(p));
    }

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    Skull mesh_convex_skull(const CGAL::Polygon_2<K, C> &p, const Mesh &mesh) {
        return mesh_convex_skull(to_polygon(p), mesh);
    }

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    Skull approximate_convex_skull(const CGAL::Polygon_2<K, C> &p,
                                   const ApproximateOptions &options = {}) {
        return approximate_convex_skull(to_polygon(p), options);
    }

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    Skull vertex_convex_skull(const CGAL::Polygon_2<K, C> &p) {
        return vertex_convex_skull(to_polygon(p));
    }

    template <class K, class C,
              class = std::enable_if_t<!std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>>>
    FT upper_bound(const CGAL::Polygon_2<K, C> &p, const UpperBoundOptions &options = {}) {
        return upper_bound(to_polygon(p), options);
    }

    template <class K1, class C1, class K2, class C2,
              class = std::enable_if_t<!(std::is_same_v<CGAL::Polygon_2<K1, C1>, Polygon>
                                         && std::is_same_v<CGAL::Polygon_2<K2, C2>, Polygon>)>>
    Skull refine(const CGAL::Polygon_2<K1, C1> &p, const CGAL::Polygon_2<K2, C2> &start,
                 const RefineOptions &options = {}) {
        return refine(to_polygon(p), to_polygon(start), options);
    }

}  // namespace potato_peeling
