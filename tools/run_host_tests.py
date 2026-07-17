#!/usr/bin/env python3
"""run_host_tests.py, compile and run every Gaitway host test with one command.

Compiles the assert-style C++ tests against the SHIPPED firmware sources and runs
the reference replay. Exits non-zero if anything fails. This is the Gate 1 + 2
host logic gate: it must pass before flashing. No em dashes.
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
INC = os.path.join(ROOT, "firmware", "hub", "include")
SRC = os.path.join(ROOT, "firmware", "hub", "src")
BUILD = os.path.join(HERE, "build")

# name -> (list of source files to compile together)
CPP_TESTS = {
    "test_rvc":            [os.path.join(HERE, "test_rvc.cpp"), os.path.join(SRC, "rvc_parser.cpp")],
    "test_hostlogic":      [os.path.join(HERE, "test_hostlogic.cpp")],
    "test_command_stream": [os.path.join(HERE, "test_command_stream.cpp"),
                            os.path.join(SRC, "command_stream.cpp")],
}


def find_gpp():
    g = shutil.which("g++")
    if g:
        return g
    guess = (r"C:\Users\akhil\AppData\Local\Microsoft\WinGet\Packages"
             r"\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe"
             r"\mingw64\bin\g++.exe")
    if os.path.exists(guess):
        return guess
    print("ERROR: g++ not found. Install a C++ compiler (WinLibs / MinGW).")
    sys.exit(2)


def main():
    os.makedirs(BUILD, exist_ok=True)
    gpp = find_gpp()
    ok = True

    for name, srcs in CPP_TESTS.items():
        exe = os.path.join(BUILD, name + (".exe" if os.name == "nt" else ""))
        cmd = [gpp, "-std=c++17", "-O2", "-Wall", "-I", INC, *srcs, "-o", exe]
        c = subprocess.run(cmd, capture_output=True, text=True)
        if c.returncode != 0:
            print(f"[{name}] COMPILE FAILED\n{c.stdout}{c.stderr}")
            ok = False
            continue
        r = subprocess.run([exe], capture_output=True, text=True)
        print(r.stdout.strip())
        if r.returncode != 0:
            ok = False

    # Reference replay drives the real detector end to end.
    print("[replay_reference]")
    r = subprocess.run([sys.executable, os.path.join(HERE, "replay_reference.py")],
                       capture_output=True, text=True)
    print(r.stdout.strip())
    if r.returncode != 0:
        print(r.stderr.strip())
        ok = False

    print("\n==== HOST TESTS:", "ALL PASS ====" if ok else "FAIL ====")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
