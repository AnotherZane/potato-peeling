"""Fail unless pyproject.toml's version and CMakeLists.txt's project(VERSION) agree.

The Python version may carry a pre-release or dev suffix (0.1.0a1, 0.1.0.dev0); its
release part must equal the CMake version, which has none.

  python ci/check_version.py [--tag vX.Y.Z...]   (--tag: the version must also match it)
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


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
    if len(sys.argv) == 3 and sys.argv[1] == "--tag":
        tag = sys.argv[2].removeprefix("refs/tags/").removeprefix("v")
        print(f"tag {sys.argv[2]}: {'ok' if tag == version else 'MISMATCH'}")
        ok = ok and tag == version
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
