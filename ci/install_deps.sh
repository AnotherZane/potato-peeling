#!/usr/bin/env bash
# Install the build dependencies for wheels into PREFIX (default /opt/deps):
# CGAL and Boost headers with their CMake package files, pinned by SHA-256, and
# GMP and MPFR. On macOS GMP and MPFR are built from source for
# MACOSX_DEPLOYMENT_TARGET (Homebrew's are built for the runner's macOS); on
# Linux they come from the manylinux image's package manager; on Windows (Git
# Bash) from vcpkg, built with MSVC as pinned in ci/vcpkg/vcpkg.json.
#
# A prefix already filled by this exact script, for this platform and deployment
# target, is left as it is (CI caches it on macOS).
#
#   ci/install_deps.sh [PREFIX]      then: CMAKE_PREFIX_PATH=PREFIX
#                                    (Windows: also PREFIX/vcpkg/x64-windows)
set -euo pipefail

PREFIX="${1:-/opt/deps}"
HERE="$(cd "$(dirname "$0")" && pwd)"

sha256() {
    if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

STAMP="$(sha256 "$0") $(sha256 "$HERE/vcpkg/vcpkg.json") $(uname -s) $(uname -m) ${MACOSX_DEPLOYMENT_TARGET:-}"
if [ "$(cat "$PREFIX/.install_deps" 2>/dev/null)" = "$STAMP" ]; then
    echo "dependencies already installed in $PREFIX"
    exit 0
fi

WORK="$(mktemp -d)"
# Windows: TEMP can be an 8.3 short path (RUNNER~1); CMake compares paths in
# their long form, so Boost's install rules failed to tell build from install paths.
if command -v cygpath >/dev/null; then WORK="$(cygpath -m -l "$WORK")"; fi
JOBS="$( (nproc || sysctl -n hw.ncpu) 2>/dev/null)"
mkdir -p "$PREFIX"

BOOST_URL=https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0-cmake.tar.xz
BOOST_SHA=aca59f889f0f32028ad88ba6764582b63c916ce5f77b31289ad19421a96c555f
CGAL_URL=https://github.com/CGAL/cgal/releases/download/v6.1.3/CGAL-6.1.3-library.tar.xz
CGAL_SHA=84c45a292103d5684568a6d889d09cf0f1d68659108049a26709e8176b0d2783
GMP_URL=https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz
GMP_SHA=a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898
MPFR_URL=https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.2.tar.xz
MPFR_SHA=b67ba0383ef7e8a8563734e2e889ef5ec3c3b898a01d00fa0a6869ad81c6ce01

# fetch URL SHA256: download into WORK, check the hash, unpack into WORK
fetch() {
    local file
    file="$WORK/$(basename "$1")"
    # timeouts, so a stalled connection fails (and --retry retries it) instead of hanging
    curl -fsSL --connect-timeout 30 --max-time 900 --retry 3 --retry-delay 5 -o "$file" "$1"
    local sum
    sum=$(sha256 "$file")
    if [ "$sum" != "$2" ]; then echo "checksum mismatch for $1: $sum" >&2; exit 1; fi
    # run in WORK with a bare name: GNU tar takes the C: of a Windows path for a host
    (cd "$WORK" && tar -xJf "$(basename "$file")")
}

command -v cmake >/dev/null || python3 -m pip install --quiet cmake

# Boost: the CMake release keeps each library's headers in libs/*/include. Install
# the boost_headers package files, then merge every library's headers.
fetch "$BOOST_URL" "$BOOST_SHA"
cmake -S "$WORK/boost-1.90.0" -B "$WORK/boost-build" -DBOOST_INCLUDE_LIBRARIES=headers \
    -DBUILD_TESTING=OFF >/dev/null
cmake --install "$WORK/boost-build" --prefix "$PREFIX" >/dev/null
mkdir -p "$PREFIX/include"
find "$WORK/boost-1.90.0/libs" -type d -path "*/include/boost" -prune \
    -exec cp -R {} "$PREFIX/include/" \;

# CGAL is header-only.
fetch "$CGAL_URL" "$CGAL_SHA"
cmake -S "$WORK/CGAL-6.1.3" -B "$WORK/cgal-build" -DCMAKE_INSTALL_PREFIX="$PREFIX" >/dev/null
cmake --install "$WORK/cgal-build" >/dev/null

if [ "$(uname)" = Darwin ]; then
    : "${MACOSX_DEPLOYMENT_TARGET:?set MACOSX_DEPLOYMENT_TARGET}"
    # A generic --build, so GMP does not tune for the build machine's CPU; on x86_64
    # --enable-fat adds code for every CPU, chosen at run time.
    DARWIN="apple-darwin$(uname -r | cut -d. -f1)"
    if [ "$(uname -m)" = x86_64 ]; then GMP_ARGS=(--build="x86_64-$DARWIN" --enable-fat)
    else GMP_ARGS=(--build="aarch64-$DARWIN"); fi
    fetch "$GMP_URL" "$GMP_SHA"
    # make check: GMP's own advice, as compilers do miscompile it.
    (cd "$WORK/gmp-6.3.0" && ./configure --prefix="$PREFIX" --disable-static "${GMP_ARGS[@]}" \
        >/dev/null && make -j"$JOBS" >/dev/null && make -j"$JOBS" check >/dev/null \
        && make install >/dev/null)
    fetch "$MPFR_URL" "$MPFR_SHA"
    (cd "$WORK/mpfr-4.2.2" && ./configure --prefix="$PREFIX" --disable-static \
        --with-gmp="$PREFIX" >/dev/null && make -j"$JOBS" >/dev/null && make install >/dev/null)
    # GMP 6.3.0's libtool links with -flat_namespace when it misreads the macOS
    # version; make sure it did not.
    for lib in "$PREFIX"/lib/libgmp.dylib "$PREFIX"/lib/libmpfr.dylib; do
        otool -hv "$lib" | grep -q TWOLEVEL || { echo "$lib: flat namespace" >&2; exit 1; }
    done
elif [[ "$(uname -s)" == MINGW* || "$(uname -s)" == MSYS* ]]; then
    # vcpkg itself at the pinned baseline commit, so its version database and tool
    # match the manifest whatever vcpkg a machine has installed (a newer baseline
    # than an installed checkout is not resolvable from that checkout).
    BASELINE=$(sed -n 's/.*"builtin-baseline": *"\([0-9a-f]*\)".*/\1/p' "$HERE/vcpkg/vcpkg.json")
    # A short path beside PREFIX: vcpkg's build trees nest deeply, and MSYS builds
    # can exceed Windows' 260-character path limit under a long temp directory.
    VCPKG_ROOT="${PREFIX%/}-vcpkg"
    rm -rf "$VCPKG_ROOT"
    git init --quiet "$VCPKG_ROOT"
    git -C "$VCPKG_ROOT" fetch --quiet --depth 1 https://github.com/microsoft/vcpkg "$BASELINE"
    git -C "$VCPKG_ROOT" checkout --quiet FETCH_HEAD
    cmd.exe //c "$(cygpath -w "$VCPKG_ROOT/bootstrap-vcpkg.bat")" -disableMetrics
    "$VCPKG_ROOT/vcpkg" install --triplet x64-windows \
        --x-manifest-root="$(cygpath -m "$HERE/vcpkg")" \
        --x-install-root="$(cygpath -m "$PREFIX/vcpkg")"
    rm -rf "$VCPKG_ROOT"
elif command -v dnf >/dev/null || command -v yum >/dev/null; then
    # A stalled mirror once hung this step silently for over 20 minutes, and another
    # served an empty index: show progress, fail over slow mirrors, give up on an
    # attempt after 10 minutes, and before retrying drop the cached index (so a
    # mirror is chosen afresh) and wait for transient faults to clear.
    pm=$(command -v dnf || command -v yum)
    for attempt in 1 2 3 4; do
        timeout 600 "$pm" install -y --setopt=timeout=60 --setopt=minrate=10k \
            gmp-devel mpfr-devel && break
        [ "$attempt" = 4 ] && { echo "installing GMP and MPFR failed" >&2; exit 1; }
        echo "installing GMP and MPFR failed (attempt $attempt), retrying" >&2
        "$pm" clean metadata || true
        sleep $((30 * attempt))
    done
fi

rm -rf "$WORK"
echo "$STAMP" >"$PREFIX/.install_deps"
echo "dependencies installed in $PREFIX"
