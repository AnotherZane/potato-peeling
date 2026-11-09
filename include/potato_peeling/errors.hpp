#pragma once

// Exceptions thrown by the library.

#include <stdexcept>

#include "potato_peeling/export.hpp"

namespace potato_peeling {

    // Bad input: not a simple polygon, fewer than 3 distinct vertices, a start
    // polygon that is not a convex subset of P, or an invalid mesh.
    class POTATO_PEELING_EXPORT invalid_polygon : public std::invalid_argument {
    public:
        using std::invalid_argument::invalid_argument;
    };

    // A result failed its own exact check. This is a bug in the library.
    class POTATO_PEELING_EXPORT certification_error : public std::logic_error {
    public:
        using std::logic_error::logic_error;
    };

}  // namespace potato_peeling
