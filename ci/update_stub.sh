#!/usr/bin/env bash
# Regenerate python/potato_peeling/_core.pyi, the type stub of the compiled module,
# from the build in this project's uv environment (after `uv sync`). With --check,
# fail if the committed stub is out of date instead. The module is imported on its
# own (not through the package), so its classes are defined in the stub rather
# than re-exported from potato_peeling.
#
#   ci/update_stub.sh [--check]
set -euo pipefail
cd "$(dirname "$0")/.."

DIR=$(uv run python -c "import importlib.util, os; \
print(os.path.dirname(importlib.util.find_spec('potato_peeling._core').origin))")
STUB=python/potato_peeling/_core.pyi
OUT=$STUB
[ "${1:-}" = --check ] && OUT=$(mktemp)
uv run --with nanobind python -m nanobind.stubgen -q -i "$DIR" -m _core -o "$OUT"
if [ "${1:-}" = --check ]; then
    diff -u "$STUB" "$OUT" || { echo "$STUB is out of date: run ci/update_stub.sh" >&2; exit 1; }
    echo "$STUB is up to date"
fi
