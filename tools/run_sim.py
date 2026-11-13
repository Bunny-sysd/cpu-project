#!/usr/bin/env python3
"""run_sim.py - assemble the programs, compile the Verilog, run the testbench.

    python tools/run_sim.py            run the tests
    python tools/run_sim.py --trace    also print every instruction as it runs

Run it from anywhere; it always works relative to the project folder.
Afterwards open the waveform with:  gtkwave build/tb.vcd
"""
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICARUS_DIR = r"C:\iverilog\bin"   # where the Windows installer puts it


def find_tool(name):
    path = shutil.which(name) or shutil.which(name, path=ICARUS_DIR)
    if not path:
        sys.exit(f"Cannot find '{name}'. Install Icarus Verilog (winget install Icarus.Verilog).")
    return path


def run(cmd):
    print(">", " ".join(os.path.relpath(c, ROOT) if os.path.isabs(c) else c for c in cmd))
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
    output = result.stdout + result.stderr
    print(output, end="")
    if result.returncode != 0:
        sys.exit(f"'{os.path.basename(cmd[0])}' failed")
    return output


def main():
    os.makedirs(os.path.join(ROOT, "build"), exist_ok=True)

    for name in ("cipher", "count"):
        run([sys.executable, "tools/asm.py", f"programs/{name}.asm", f"build/{name}.hex"])

    sources = sorted(glob.glob(os.path.join(ROOT, "src", "*.v")))
    run([find_tool("iverilog"), "-g2005", "-Wall", "-Wno-timescale", "-o", "build/tb.vvp", "test/tb_local.v"] + sources)
    trace = ["+trace"] if "--trace" in sys.argv else []
    output = run([find_tool("vvp"), "build/tb.vvp"] + trace)

    if "ALL TESTS PASSED" not in output:
        sys.exit("Simulation did not pass.")
    print("Waveform written to build/tb.vcd  (open with: gtkwave build/tb.vcd)")


if __name__ == "__main__":
    main()
