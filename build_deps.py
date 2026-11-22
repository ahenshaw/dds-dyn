#!/usr/bin/env python3
"""Build the static C++ dependencies of fastdds_dyn into one install prefix.

    foonathan_memory_vendor v1.4.1, Fast-CDR v2.3.6, Fast DDS v3.6.2
    (the set pinned by Fast DDS v3.6.2's own fastdds.repos)

Everything is built static (and position-independent), so the Python extension
links Fast DDS in and the wheel needs no extra .so/.dll files. Fast DDS uses its
bundled Asio and TinyXML2; TLS, security, SQLite persistence and statistics are
off, so OpenSSL and SQLite are not needed either. Needs git, CMake >= 3.20 and a
C++17 compiler (GCC/Clang, or MSVC on Windows). Takes ~5-10 min.

patches/*.patch are applied to Fast DDS: fastdds-3.6.2-idl-string-bounds.patch fixes
the runtime IDL parser giving a string's bound to the wrong member (it kept one
shared "string_size" slot, consumed by whichever string member resolved first).

usage: python build_deps.py [--prefix DIR] [--work DIR] [--jobs N]
Then point the extension build at it: CMAKE_PREFIX_PATH=DIR pip install .
Safe to re-run: finished steps are skipped (delete DIR to force a rebuild).
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEPS = [
    # name, repo, tag, submodules to fetch, extra CMake definitions
    ("foonathan_memory_vendor", "https://github.com/eProsima/foonathan_memory_vendor.git", "v1.4.1",
     [], []),
    ("fastcdr", "https://github.com/eProsima/Fast-CDR.git", "v2.3.6",
     [], []),
    ("fastdds", "https://github.com/eProsima/Fast-DDS.git", "v3.6.2",
     ["thirdparty/asio", "thirdparty/tinyxml2"],
     ["-DTHIRDPARTY_Asio=FORCE", "-DTHIRDPARTY_TinyXML2=FORCE", "-DTHIRDPARTY_fastcdr=OFF",
      "-DNO_TLS=ON", "-DSECURITY=OFF", "-DSQLITE3_SUPPORT=OFF", "-DFASTDDS_STATISTICS=OFF",
      "-DCOMPILE_TOOLS=OFF", "-DCOMPILE_EXAMPLES=OFF", "-DBUILD_TESTING=OFF",
      "-DBUILD_DOCUMENTATION=OFF"]),
]


def run(cmd: list[str], cwd: Path | None = None) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd, cwd=cwd)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--prefix", type=Path, default=HERE / "deps" / "install")
    ap.add_argument("--work", type=Path, default=None,
                    help="source/build scratch dir (default: <prefix>/../work). Keep it short on "
                         "Windows, e.g. C:\\fd, to stay under MAX_PATH.")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    args = ap.parse_args()
    prefix = args.prefix.resolve()
    work = (args.work or prefix.parent / "work").resolve()
    work.mkdir(parents=True, exist_ok=True)

    for name, repo, tag, submodules, defs in DEPS:
        stamp = prefix / f".{name}-{tag}.done"
        if stamp.exists():
            print(f"== {name} {tag}: already built", flush=True)
            continue
        print(f"== {name} {tag}", flush=True)
        src, bld = work / "src" / name, work / "build" / name
        if not src.exists():
            run(["git", "-c", "advice.detachedHead=false", "clone", "--depth", "1", "-b", tag, repo, str(src)])
            if submodules:
                run(["git", "submodule", "update", "--init", "--depth", "1", *submodules], cwd=src)
        for patch in sorted((HERE / "patches").glob(f"{name}-*.patch")):
            applied = subprocess.call(["git", "apply", "--reverse", "--check", str(patch)], cwd=src,
                                      stderr=subprocess.DEVNULL) == 0
            if not applied:
                run(["git", "apply", str(patch)], cwd=src)
        run(["cmake", "-S", str(src), "-B", str(bld),
             f"-DCMAKE_INSTALL_PREFIX={prefix}", f"-DCMAKE_PREFIX_PATH={prefix}",
             "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_SHARED_LIBS=OFF",
             "-DCMAKE_POSITION_INDEPENDENT_CODE=ON", *defs])
        run(["cmake", "--build", str(bld), "--config", "Release", "--target", "install",
             "--parallel", str(args.jobs)])
        stamp.touch()

    print(f"deps installed in {prefix}\nnext: CMAKE_PREFIX_PATH={prefix} pip install {HERE}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
