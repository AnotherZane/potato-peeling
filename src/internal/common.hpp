#pragma once

// Types the internals share, on top of the public ones.

#include <CGAL/Aff_transformation_2.h>

#include "potato_peeling/errors.hpp"
#include "potato_peeling/types.hpp"

namespace potato_peeling::internal {
    using K = Kernel;
    using Vector = Kernel::Vector_2;
    using Line = Kernel::Line_2;
    using Direction = Kernel::Direction_2;
    using Aff_transformation = CGAL::Aff_transformation_2<Kernel>;
}  // namespace potato_peeling::internal
