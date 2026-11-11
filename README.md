# potato-peeling

Algorithms for the [potato-peeling](https://en.wikipedia.org/wiki/Potato_peeling) problem: find the largest-area convex polygon contained in a simple polygon (its *convex skull*).

A C++ library with Python bindings; JavaScript bindings (a native Node.js addon and WebAssembly) are planned.

> [!WARNING]
> Not released yet: the API may still change before 0.1.

## Contents

- [Methods](#methods)
- [Python](#python)
- [C++](#c)
- [Building](#building)
- [Tests](#tests)
- [Background](#background)
- [Licence](#licence)

## Methods

The same functions, with the same names and guarantees, in every language.

| Function | Algorithm | Answer |
|---|---|---|
| `convex_skull` | `approximate_convex_skull`, then `refine` | the general-purpose method: certified, typically within 0.5% of optimal; may improve between versions |
| `exact_convex_skull` | [Chang & Yap (1986)](https://link.springer.com/article/10.1007/BF02187692) | the exact optimum; practical up to a few dozen vertices |
| `mesh_convex_skull` | [Aronov et al. (2011)](https://link.springer.com/article/10.1007/s00453-009-9346-8) | optimum among unions of triangles of P's constrained Delaunay triangulation, or of a triangulation you pass |
| `approximate_convex_skull` | [Cabello et al. (2017)](https://epubs.siam.org/doi/10.1137/16M1079695) | randomised approximation, deterministic for a given `seed` |
| `refine` | local ascent (from the thesis) | improves any convex polygon inside P; never returns less than it was given |
| `vertex_convex_skull` | Fischer; Bautista-Santiago et al. | the largest convex polygon whose vertices are vertices of P |
| `upper_bound` | pivot, two-pivot and width bounds | a certified upper bound on the optimum's area |

Utilities: `is_convex_subset`, `segment_in_polygon`, `triangulate`, `reflex_vertices`, `longest_segment`.

Every answer is a convex polygon inside P, checked exactly before it is returned. Input is a simple polygon without holes, in either orientation; repeated and collinear vertices are removed.

## Python

Python 3.9 or later.

### Quick start

```python
import potato_peeling as pp

p = [(0, 0), (6, 1), (8, 5), (4, 4), (2, 8)]

skull = pp.convex_skull(p)       # fast, certified, close to optimal
float(skull.area)                # 21.687
skull.coords                     # [(0.0, 0.0), (6.0, 1.0), (7.908..., 4.816...), ...]

best = pp.exact_convex_skull(p)  # the optimum
best.area                        # Fraction(109, 5), i.e. 21.8
best.vertices                    # ((8, 5), (4/5, 16/5), (0, 0), (6, 1)) as Fractions

float(pp.upper_bound(p))         # 21.826: no convex polygon in P is larger
```

### Inputs and results

Polygons may be given as sequences of (x, y) pairs (`int`, `float`, `Fraction` or `Decimal`), NumPy arrays of shape (n, 2), or objects with `__geo_interface__` such as shapely polygons (exterior ring only). Floats are read exactly: `0.1` is the double nearest 0.1.

Results are exact: a `Skull` has `vertices` and `area` as `Fraction`s, `coords` as floats, and `__geo_interface__`, so `shapely.geometry.shape(skull)` works. Options are keyword arguments, for example `pp.approximate_convex_skull(p, epsilon=0.05, seed=1)`.

### Long runs and errors

Every method takes `timeout=` in seconds: it raises `TimeoutError`, except `refine` and `convex_skull` while refining, which return the best answer so far. Ctrl-C interrupts any call. Calls release the GIL, so polygons can be solved in parallel with threads; `threads=` additionally parallelises Cabello's step (0, the default, uses every core).

`InvalidPolygonError` (a `ValueError`) is raised for input that is not a simple polygon; `CertificationError` if a result fails its own check, which would be a bug.

## C++

C++17, on CGAL's exact kernel.

### Quick start

```cpp
#include <iostream>
#include <potato_peeling/potato_peeling.hpp>

namespace pp = potato_peeling;

int main() {
    // A CGAL::Polygon_2 over CGAL's exact kernel (Epeck): a simple polygon,
    // either orientation, with exact coordinates.
    pp::Polygon p;
    for (auto [x, y] : {std::pair{0, 0}, {6, 1}, {8, 5}, {4, 4}, {2, 8}})
        p.push_back(pp::Point(x, y));

    const pp::Skull skull = pp::convex_skull(p);         // fast, certified
    const pp::Skull best = pp::exact_convex_skull(p);    // the optimum
    std::cout << CGAL::to_double(skull.area) << " " << best.area << " "
              << pp::upper_bound(p) << "\n";             // 21.6869 21.8 21.8256
}
```

A `Skull` holds the polygon (counter-clockwise, without collinear vertices) and its exact area. Each method also accepts a `CGAL::Polygon_2` over any other CGAL kernel, and takes an options struct (`ConvexSkullOptions`, `ApproximateOptions`, ...) as its last argument.

### Without CGAL

`potato_peeling/simple.hpp` offers the same methods on plain `double` coordinates and includes no CGAL headers; results are the exact answers rounded to nearest.

```cpp
#include <potato_peeling/simple.hpp>

namespace ps = potato_peeling::simple;

const ps::Ring p{{0, 0}, {6, 1}, {8, 5}, {4, 4}, {2, 8}};
const ps::SimpleSkull skull = ps::convex_skull(p);    // skull.polygon, skull.area
```

### Using it with CMake

The library builds as a shared library with a CMake package:

```cmake
find_package(potato_peeling CONFIG REQUIRED)
target_link_libraries(app PRIVATE potato_peeling::potato_peeling)
```

### Errors and cancellation

Methods throw `invalid_polygon` (a `std::invalid_argument`) for input that is not a simple polygon, and `certification_error` if a result fails its own check, which would be a bug. Each options struct has a `should_stop` callback, called between units of work; when it returns true the method throws `cancelled`, except `refine`, which returns its answer so far.

## Building

Requires a C++17 compiler, CMake 3.20 or later, and the CGAL (5.4 or later), Boost (1.74 or later), GMP and MPFR development packages. [CDT](https://github.com/artem-ogre/CDT) is used if installed, otherwise a pinned release is downloaded during the build.

Python package:

```
uv sync
```

C++ library and its tests:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix <prefix>
```

## Tests

```
uv run pytest                                 # Python, ~2 min; -m slow for the larger set
ctest --test-dir build -LE slow               # C++, ~30 s; -L slow for the brute-force sweeps
```

The Python tests check the exact methods against the exact optima published in the thesis, on 81 Natural Earth polygons ([`tests/data/optima.json`](https://github.com/AnotherZane/potato-peeling/blob/main/tests/data/optima.json)), and every method's guarantees on those and on hand-made polygons. The C++ tests check the vertex skull, longest segment and mesh skull against brute-force searches on random polygons.

## Background

The implementations were developed for the thesis *Peeling Potatoes Exactly and Almost Exactly: Computing the Convex Skull of a Polygon* (Zane Dmitri Pereira, The University of Melbourne, 2026). The code and results as used in the thesis are archived in [potatoes-research](https://github.com/AnotherZane/potatoes-research); this library has since been reworked (no GPL dependencies, a new API, bug fixes), so its approximate answers can differ from the thesis's, while its exact answers match.

## Licence

LGPL-3.0-or-later; see [`COPYING.LESSER`](https://github.com/AnotherZane/potato-peeling/blob/main/COPYING.LESSER) and [`COPYING`](https://github.com/AnotherZane/potato-peeling/blob/main/COPYING). Only the LGPL-licensed parts of CGAL are used; the build defines `CGAL_LICENSE_ERROR`, so including a GPL-licensed CGAL package fails to compile.

Third-party licences and notices are in [`THIRD-PARTY-NOTICES.txt`](https://github.com/AnotherZane/potato-peeling/blob/main/THIRD-PARTY-NOTICES.txt) and [`LICENSES/`](https://github.com/AnotherZane/potato-peeling/tree/main/LICENSES).

Built on [CGAL](https://www.cgal.org), the Computational Geometry Algorithms Library. If you use this library in published work, please also cite CGAL: The CGAL Project. *CGAL User and Reference Manual*. CGAL Editorial Board. https://doc.cgal.org ([how to cite](https://doc.cgal.org/latest/Manual/how_to_cite_cgal.html)).
