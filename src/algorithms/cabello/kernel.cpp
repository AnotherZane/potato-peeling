#include "potato_peeling/algorithms/cabello/kernel.hpp"

namespace potato_peeling::cabello {

    Poly to_inexact(const Polygon &poly) {
        const ToInexact conv;
        Poly out;
        for (const auto &v : poly.container()) out.push_back(conv(v));
        return out;
    }

}  // namespace potato_peeling::cabello
