#pragma once

// nanobind casters: a Point arrives as an (x, y) pair of finite floats, a
// Polygon as a sequence of them. Input only; results are converted by hand.
//
// They must be visible in every translation unit that binds a function taking
// these types, since nanobind picks the caster where m.def is instantiated.

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include <nanobind/nanobind.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/vector.h>

#include "potato_peeling/types.hpp"

namespace nanobind::detail {
    template <> struct type_caster<potato_peeling::Point> {
        NB_TYPE_CASTER(potato_peeling::Point, const_name("tuple[float, float]"))

        using Pair = std::pair<double, double>;
        using PairCaster = make_caster<Pair>;

        bool from_python(handle src, std::uint32_t flags, cleanup_list *cleanup) noexcept {
            PairCaster caster;
            if (!caster.from_python(src, flags_for_local_caster<Pair>(flags), cleanup))
                return false;
            const Pair xy = caster.operator cast_t<Pair>();
            if (!std::isfinite(xy.first) || !std::isfinite(xy.second)) return false;
            value = potato_peeling::Point(xy.first, xy.second);
            return true;
        }

        // Not implemented yet: results are returned as exact strings by hand. The
        // empty handle makes nanobind raise TypeError if a binding returns this type.
        static handle from_cpp(const potato_peeling::Point &, rv_policy, cleanup_list *) noexcept {
            return {};
        }
    };

    template <> struct type_caster<potato_peeling::Polygon> {
        NB_TYPE_CASTER(potato_peeling::Polygon, const_name("Sequence[tuple[float, float]]"))

        using Points = std::vector<potato_peeling::Point>;
        using PointsCaster = make_caster<Points>;

        bool from_python(handle src, std::uint32_t flags, cleanup_list *cleanup) noexcept {
            PointsCaster caster;
            if (!caster.from_python(src, flags_for_local_caster<Points>(flags), cleanup))
                return false;
            const Points pts = caster.operator cast_t<Points>();
            value = potato_peeling::Polygon(pts.begin(), pts.end());
            return true;
        }

        // Not implemented yet: results are returned as exact strings by hand. The
        // empty handle makes nanobind raise TypeError if a binding returns this type.
        static handle from_cpp(const potato_peeling::Polygon &, rv_policy,
                               cleanup_list *) noexcept {
            return {};
        }
    };
}  // namespace nanobind::detail
