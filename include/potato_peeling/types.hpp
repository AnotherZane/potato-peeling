#pragma once

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
}  // namespace potato_peeling
