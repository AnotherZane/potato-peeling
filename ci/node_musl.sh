#!/usr/bin/env bash
# Build, package and test the npm package for musl Linux, inside a musllinux_1_2
# container (Alpine), from the repository root: the musl counterpart of node.yml's build
# job, which runs in the container with docker run rather than as the job's container
# (GitHub's JavaScript actions do not run reliably in Alpine containers).
#
# nodejs.org has no musl builds; Node's unofficial builds do. The addon is built with
# Node 22, installed from its packed tarball with the local build removed, and tested on
# each Node in NODE_VERSIONS; every version is tested even if an earlier one fails.
#
#   ci/node_musl.sh      writes build/npm/linux-<arch>-musl/*.tgz
set -euo pipefail

NODE_VERSIONS=(20 22 24 26)
BUILD_NODE=22
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

case "$(uname -m)" in
    x86_64) ARCH=x64 ;;
    aarch64) ARCH=arm64 ;;
    *) echo "unsupported architecture $(uname -m)" >&2; exit 1 ;;
esac
UNOFFICIAL=https://unofficial-builds.nodejs.org/download/release

# Node's prebuilt musl binaries link libstdc++ dynamically.
apk add --no-cache libstdc++ curl xz >/dev/null
bash ci/install_deps.sh /opt/deps

# install_node MAJOR: the newest MAJOR.x musl build, in /opt/node-MAJOR; prints its bin.
install_node() {
    local version dir="/opt/node-$1"
    if [ ! -x "$dir/bin/node" ]; then
        version=$(curl -fsSL --retry 3 "$UNOFFICIAL/index.json" | python3 -c "
import json, sys
print(next(r['version'] for r in json.load(sys.stdin)
           if r['version'].startswith('v$1.') and 'linux-$ARCH-musl' in r['files']))")
        mkdir -p "$dir"
        curl -fsSL --retry 3 "$UNOFFICIAL/$version/node-$version-linux-$ARCH-musl.tar.xz" \
            | tar -xJ -C "$dir" --strip-components=1
    fi
    echo "$dir/bin"
}

PATH="$(install_node "$BUILD_NODE"):$PATH"
node --version
(cd js && npm ci --no-audit --no-fund && CMAKE_PREFIX_PATH=/opt/deps npm run build:native)
python3 ci/npm_package.py platform build/node/Release/potato_peeling.node build/npm
PACKAGE="build/npm/linux-$ARCH-musl"
(cd "$PACKAGE" && npm pack)

# The packaged addon, not the local build.
rm -rf build/node
(cd js && npm install --no-save "../$PACKAGE"/*.tgz \
    && node -p "require.resolve('@potato-peeling/linux-$ARCH-musl')")

failed=()
for major in "${NODE_VERSIONS[@]}"; do
    echo "::group::Test on Node $major"
    if ! (PATH="$(install_node "$major"):$PATH" && cd js && node --version && npm test); then
        failed+=("$major")
    fi
    echo "::endgroup::"
done
if [ ${#failed[@]} -gt 0 ]; then
    echo "tests failed on Node ${failed[*]}" >&2
    exit 1
fi
