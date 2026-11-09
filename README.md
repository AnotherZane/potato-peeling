# potato-peeling

Algorithms for the [potato-peeling](https://en.wikipedia.org/wiki/Potato_peeling) problem: find the largest-area convex polygon contained in a simple polygon.

> [!WARNING]
> Not released yet: the API will change before the first release.

## Implementations

| Module | Algorithm | Answer |
|---|---|---|
| `chang_and_yap` | [Chang & Yap (1986)](https://link.springer.com/article/10.1007/BF02187692) | exact optimum, in exact rational arithmetic; practical up to a few dozen vertices |
| `aronov` | [Aronov et al. (2011)](https://link.springer.com/article/10.1007/s00453-009-9346-8) | optimum among unions of triangles of the polygon's constrained Delaunay triangulation |
| `cabello` | [Cabello et al. (2017)](https://epubs.siam.org/doi/10.1137/16M1079695) | randomised approximation; certified convex and contained |
| `refinement` | proposed local ascent | improves any convex polygon inside P; certified |

The implementations were developed for the thesis *Peeling Potatoes Exactly and Almost Exactly: Computing the Convex Skull of a Polygon* (Zane Dmitri Pereira, The University of Melbourne, 2026). The code and results as used in the thesis are archived in [potatoes-research](https://github.com/AnotherZane/potatoes-research).

## Building

Requires a C++17 compiler, CMake 3.20 or later, and the CGAL (5.4 or later), Boost, GMP and MPFR development packages. [CDT](https://github.com/artem-ogre/CDT) is used if installed, otherwise a pinned release is downloaded during the build.

```
uv sync
```

builds the `potato_peeling` Python package (Python 3.9 or later).

## Licence

LGPL-3.0-or-later; see `COPYING.LESSER` and `COPYING`. Only the LGPL-licensed parts of CGAL are used; the build defines `CGAL_LICENSE_ERROR`, so including a GPL-licensed CGAL package fails to compile.

Third-party licences and notices are in `THIRD-PARTY-NOTICES.txt` and `LICENSES/`.

Built on [CGAL](https://www.cgal.org), the Computational Geometry Algorithms Library: The CGAL Project. *CGAL User and Reference Manual*. CGAL Editorial Board. https://doc.cgal.org
