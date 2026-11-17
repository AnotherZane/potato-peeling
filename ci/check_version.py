"""Fail unless the versions in pyproject.toml, CMakeLists.txt's project(VERSION) and
js/package.json (with js/package-lock.json) agree.

The Python version may carry a pre-release or dev suffix (0.1.0a1, 0.1.0.dev0); its
release part must equal the CMake version, which has none. npm spells the same version
in semver: 0.1.0a1 is 0.1.0-alpha.1 (b: beta, rc: rc, .devN: -dev.N).

  python ci/check_version.py [--tag vX.Y.Z...]   (--tag: the version must also match it)
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def npm_version(version: str) -> str | None:
    """The semver spelling of a PEP 440 version, or None if it has none."""
    m = re.fullmatch(r"(\d+\.\d+\.\d+)(?:(a|b|rc)(\d+))?(?:\.dev(\d+))?", version)
    if not m:
        return None
    release, pre, n, dev = m.groups()
    out = release
    if pre:
        out += f"-{dict(a='alpha', b='beta', rc='rc')[pre]}.{n}"
    if dev is not None:
        out += f".dev.{dev}" if pre else f"-dev.{dev}"
    return out


def main() -> int:
    py = re.search(r'^version\s*=\s*"([^"]+)"', (ROOT / "pyproject.toml").read_text(), re.M)
    cm = re.search(r"project\(\s*potato_peeling\s+VERSION\s+([0-9.]+)",
                   (ROOT / "CMakeLists.txt").read_text())
    if not py or not cm:
        print("version not found in pyproject.toml or CMakeLists.txt")
        return 1
    version, cmake = py.group(1), cm.group(1)
    release = re.match(r"\d+(\.\d+)*", version).group(0)
    ok = release == cmake
    print(f"pyproject {version} (release {release}), CMake {cmake}: {'ok' if ok else 'MISMATCH'}")
    npm = npm_version(version)
    package = json.loads((ROOT / "js/package.json").read_text())["version"]
    lock = json.loads((ROOT / "js/package-lock.json").read_text())
    npm_ok = npm is not None and package == npm == lock["version"] == lock["packages"][""]["version"]
    print(f"js/package.json {package} (expected {npm}), lock {lock['version']}: "
          f"{'ok' if npm_ok else 'MISMATCH'}")
    ok = ok and npm_ok
    if len(sys.argv) == 3 and sys.argv[1] == "--tag":
        tag = sys.argv[2].removeprefix("refs/tags/").removeprefix("v")
        print(f"tag {sys.argv[2]}: {'ok' if tag == version else 'MISMATCH'}")
        ok = ok and tag == version
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
