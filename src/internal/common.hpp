#pragma once

// Types the internals share, on top of the public ones.

#include <variant>

#include <CGAL/Aff_transformation_2.h>
#include <CGAL/version.h>
#if CGAL_VERSION_NR < 1060000000
#include <boost/variant/get.hpp>
#endif

#include "potato_peeling/errors.hpp"
#include "potato_peeling/types.hpp"

namespace potato_peeling::internal {
    using K = Kernel;
    using Vector = Kernel::Vector_2;
    using Line = Kernel::Line_2;
    using Direction = Kernel::Direction_2;
    using Aff_transformation = CGAL::Aff_transformation_2<Kernel>;

    // The alternative T of an intersection result, or nullptr. CGAL 6 returns a
    // std::variant, CGAL 5 a boost::variant.
    template <class T, class... A> const T *get_if(const std::variant<A...> &v) {
        return std::get_if<T>(&v);
    }
#if CGAL_VERSION_NR < 1060000000
    template <class T, class... A> const T *get_if(const boost::variant<A...> &v) {
        return boost::get<T>(&v);
    }
#endif
}  // namespace potato_peeling::internal
