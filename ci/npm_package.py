"""Stage the npm packages for `npm pack` / `npm publish`.

  python ci/npm_package.py platform ADDON OUT [--lib-dir DIR]
      @potato-peeling/<platform>-<arch> for the running Node's platform, in OUT/<name>:
      the built potato_peeling.node with GMP and MPFR beside it (relinked to load them
      from there) and the licences. --lib-dir: where GMP's and MPFR's DLLs are
      (Windows; Linux and macOS find them from the addon's own links).
  python ci/npm_package.py main OUT
      potato-peeling, in OUT/potato-peeling: js/dist, the README, the licences, and
      optionalDependencies on every platform package at the same version.

GMP and MPFR stay separate shared libraries (as in the wheels), so the LGPL's relinking
needs nothing more than replacing them.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
JS = ROOT / "js"
# The platform packages, as process.platform-process.arch; the loader (js/src/native.ts)
# asks for @potato-peeling/<this>.
PLATFORMS = ["linux-x64", "linux-arm64", "darwin-arm64", "darwin-x64", "win32-x64"]
LICENCES = ["COPYING", "COPYING.LESSER", "THIRD-PARTY-NOTICES.txt", "LICENSES"]
LIBS = ("gmp", "mpfr")


def run(*args: str) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def base_package() -> dict:
    return json.loads((JS / "package.json").read_text())


def copy_licences(out: Path) -> None:
    for name in LICENCES:
        src = ROOT / name
        if src.is_dir():
            shutil.copytree(src, out / name)
        else:
            shutil.copy2(src, out / name)


def is_bundled(path: str) -> bool:
    """GMP or MPFR (libgmp.so.10, libmpfr.6.dylib, gmp-10.dll)."""
    return re.match(rf"(lib)?({'|'.join(LIBS)})[.-]", Path(path).name) is not None


def bundle_linux(addon: Path) -> list[str]:
    # ldd lists each needed library with the file the loader resolved it to.
    found = re.findall(r"^\s*(\S+) => (\S+)", run("ldd", str(addon)), re.M)
    libs = []
    for soname, path in found:
        if is_bundled(soname):
            shutil.copy2(path, addon.parent / soname)
            libs.append(soname)
    for f in [addon.name, *libs]:
        run("patchelf", "--set-rpath", "$ORIGIN", str(addon.parent / f))
    return libs


def bundle_macos(addon: Path) -> list[str]:
    def links(f: Path) -> list[str]:
        return [line.split()[0] for line in run("otool", "-L", str(f)).splitlines()[1:]]

    libs = {}  # install name -> file name
    queue = [addon]
    while queue:
        f = queue.pop()
        for link in links(f):
            if is_bundled(link) and link not in libs and not link.startswith("@"):
                name = Path(link).name
                libs[link] = name
                dest = addon.parent / name
                shutil.copy2(link, dest)
                dest.chmod(0o755)
                queue.append(dest)
    for f in [addon, *(addon.parent / n for n in libs.values())]:
        if f != addon:
            run("install_name_tool", "-id", f"@loader_path/{f.name}", str(f))
        for link, name in libs.items():
            if link in links(f):
                run("install_name_tool", "-change", link, f"@loader_path/{name}", str(f))
        # Changing a binary invalidates its signature, which arm64 requires.
        run("codesign", "--force", "--sign", "-", str(f))
    return sorted(libs.values())


def bundle_windows(addon: Path, lib_dir: Path) -> list[str]:
    # Node loads an addon with LOAD_WITH_ALTERED_SEARCH_PATH, so DLLs beside it are found.
    libs = [p.name for p in lib_dir.glob("*.dll") if is_bundled(p.name)]
    if len(libs) != len(LIBS):
        raise SystemExit(f"expected {len(LIBS)} DLLs in {lib_dir}, found {libs}")
    for name in libs:
        shutil.copy2(lib_dir / name, addon.parent / name)
    return sorted(libs)


def platform(addon_src: Path, out: Path, lib_dir: Path | None) -> Path:
    name = run("node", "-p", "process.platform + '-' + process.arch").strip()
    if name not in PLATFORMS:
        raise SystemExit(f"no package for {name}")
    os_, cpu = name.split("-")
    base = base_package()
    pkg = out / name
    shutil.rmtree(pkg, ignore_errors=True)
    pkg.mkdir(parents=True)
    addon = pkg / "potato_peeling.node"
    shutil.copy2(addon_src, addon)
    if os_ == "linux":
        libs = bundle_linux(addon)
    elif os_ == "darwin":
        libs = bundle_macos(addon)
    else:
        if lib_dir is None:
            raise SystemExit("--lib-dir is needed on Windows")
        libs = bundle_windows(addon, lib_dir)
    if len(libs) != len(LIBS):
        raise SystemExit(f"bundled {libs}; expected GMP and MPFR")
    copy_licences(pkg)
    manifest = {
        "name": f"@potato-peeling/{name}",
        "version": base["version"],
        "description": f"potato-peeling's native addon for {name}",
        "license": base["license"],
        "author": base["author"],
        "homepage": base["homepage"],
        "repository": base["repository"],
        "os": [os_],
        "cpu": [cpu],
        **({"libc": ["glibc"]} if os_ == "linux" else {}),
        "main": "potato_peeling.node",
        "files": ["potato_peeling.node", *libs, *LICENCES],
    }
    (pkg / "package.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (pkg / "README.md").write_text(
        f"# {manifest['name']}\n\nThe native addon of [potato-peeling]"
        f"(https://www.npmjs.com/package/potato-peeling) for {name}, with GMP and MPFR. "
        "Install `potato-peeling`, which depends on this package.\n")
    return pkg


def main_package(out: Path) -> Path:
    base = base_package()
    if not (JS / "dist" / "index.js").exists():
        raise SystemExit("js/dist is missing: run npm run build in js/")
    pkg = out / "potato-peeling"
    shutil.rmtree(pkg, ignore_errors=True)
    pkg.mkdir(parents=True)
    shutil.copytree(JS / "dist", pkg / "dist")
    shutil.copy2(JS / "README.md", pkg / "README.md")
    copy_licences(pkg)
    manifest = {k: v for k, v in base.items() if k not in ("private", "scripts", "devDependencies")}
    manifest["files"] = ["dist", "README.md", *LICENCES]
    manifest["optionalDependencies"] = {f"@potato-peeling/{p}": base["version"] for p in PLATFORMS}
    (pkg / "package.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return pkg


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("platform")
    p.add_argument("addon", type=Path)
    p.add_argument("out", type=Path)
    p.add_argument("--lib-dir", type=Path)
    m = sub.add_parser("main")
    m.add_argument("out", type=Path)
    args = parser.parse_args()
    if args.command == "platform":
        print(platform(args.addon, args.out, args.lib_dir))
    else:
        print(main_package(args.out))


if __name__ == "__main__":
    sys.exit(main())
