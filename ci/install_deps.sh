#!/usr/bin/env bash
# Install the build dependencies for wheels into PREFIX (default /opt/deps):
# CGAL and Boost headers with their CMake package files, pinned by SHA-256, and
# GMP and MPFR. On macOS GMP and MPFR are built from source for
# MACOSX_DEPLOYMENT_TARGET (Homebrew's are built for the runner's macOS); on
# Linux they come from the manylinux image's package manager.
#
# A prefix already filled by this exact script, for this platform and deployment
# target, is left as it is (CI caches it on macOS).
#
#   ci/install_deps.sh [PREFIX]      then: CMAKE_PREFIX_PATH=PREFIX
set -euo pipefail

PREFIX="${1:-/opt/deps}"

sha256() {
    if command -v sha256sum >/dev/null; then sha256sum "$1" | cut -d' ' -f1
    else shasum -a 256 "$1" | cut -d' ' -f1; fi
}

STAMP="$(sha256 "$0") $(uname -s) $(uname -m) ${MACOSX_DEPLOYMENT_TARGET:-}"
if [ "$(cat "$PREFIX/.install_deps" 2>/dev/null)" = "$STAMP" ]; then
    echo "dependencies already installed in $PREFIX"
    exit 0
fi

WORK="$(mktemp -d)"
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
    curl -fsSL --retry 3 -o "$file" "$1"
    local sum
    sum=$(sha256 "$file")
    if [ "$sum" != "$2" ]; then echo "checksum mismatch for $1: $sum" >&2; exit 1; fi
    tar -xJf "$file" -C "$WORK"
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
elif command -v dnf >/dev/null; then
    dnf install -y -q gmp-devel mpfr-devel
elif command -v yum >/dev/null; then
    yum install -y -q gmp-devel mpfr-devel
fi

rm -rf "$WORK"
echo "$STAMP" >"$PREFIX/.install_deps"
echo "dependencies installed in $PREFIX"
