#!/usr/bin/env python3
"""build.py - compile the mesh code for the PC and run it.

    python mesh/build.py           build and run the tests
    python mesh/build.py --demo    build and run the 4-node demo

Needs a C++ compiler (g++). On Windows:  winget install BrechtSanders.WinLibs.POSIX.UCRT
"""
import glob
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "build")


def find_gpp():
    path = shutil.which("g++")
    if path:
        return path
    # winget installs WinLibs here but only adds it to PATH for new terminals.
    pattern = os.path.join(os.environ.get("LOCALAPPDATA", ""), "Microsoft", "WinGet", "Packages",
                           "BrechtSanders.WinLibs*", "mingw64", "bin", "g++.exe")
    found = glob.glob(pattern)
    if found:
        return found[0]
    sys.exit("Cannot find g++. Install it with: winget install BrechtSanders.WinLibs.POSIX.UCRT")


def run(cmd, **kw):
    result = subprocess.run(cmd, cwd=HERE, **kw)
    if result.returncode != 0:
        sys.exit(result.returncode)


def main():
    gpp = find_gpp()
    gcc = os.path.join(os.path.dirname(gpp), "gcc" + os.path.splitext(gpp)[1])
    env = dict(os.environ, PATH=os.path.dirname(gpp) + os.pathsep + os.environ["PATH"])
    os.makedirs(OUT, exist_ok=True)

    target = "demo" if "--demo" in sys.argv else "tests"
    exe = os.path.join(OUT, target + (".exe" if os.name == "nt" else ""))
    mono = os.path.join(OUT, "monocypher.o")

    # The crypto library is plain C; everything else is C++.
    run([gcc, "-c", "-O2", "-std=c99", "vendor/monocypher/monocypher.c", "-o", mono], env=env)
    run([gpp, "-std=c++17", "-O2", "-Wall", "-Wextra", "-static",
         "-Icore", "-Ivendor/monocypher",
         "core/packet.cpp", "core/crypto.cpp", "core/node.cpp", f"sim/{target}.cpp", mono,
         "-o", exe], env=env)
    run([exe], env=env)


if __name__ == "__main__":
    main()
