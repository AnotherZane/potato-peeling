#pragma once

#include <type_traits>

#include <CGAL/Cartesian_converter.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>

namespace potato_peeling {
    using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
    using FT = Kernel::FT;  // exact rational number
    using Point = Kernel::Point_2;
    using Segment = Kernel::Segment_2;
    using Polygon = CGAL::Polygon_2<Kernel>;

    // A convex polygon contained in P: counter-clockwise, without collinear
    // vertices, with its exact area.
    struct Skull {
        Polygon polygon;
        FT area;
    };

    // A polygon over any CGAL kernel, converted to this one. Exact for double
    // coordinates; other number types convert as CGAL's Cartesian_converter does.
    template <class K, class C> Polygon to_polygon(const CGAL::Polygon_2<K, C> &p) {
        if constexpr (std::is_same_v<CGAL::Polygon_2<K, C>, Polygon>) {
            return p;
        } else {
            const CGAL::Cartesian_converter<K, Kernel> convert;
            Polygon out;
            for (auto v = p.vertices_begin(); v != p.vertices_end(); ++v)
                out.push_back(convert(*v));
            return out;
        }
    }
}  // namespace potato_peeling
