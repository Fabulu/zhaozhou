# sweep_geom_cull_preflight.py — LINT EVERY MUTANT BEFORE ANY OF THEM IS SCORED.
#
# The same guard as tools/sweep_geom_lod_preflight.py, pointed at the cull
# sweep, and for the same reason: guard 5 in the sweep DISCARDS a mutant that
# does not link, which is correct but late. A malformed mutation then reads as
# "discarded" rather than as evidence, and before that guard existed it read as
# CAUGHT — three of the LOD sweep's 23 mutants were scored as caught by every
# run for three runs because two used `W'sd0` (a syntax error) and one wrote a
# comparison that is always false and fails -Wall.
#
# Linting up front turns that from a silent inflation into a refusal to start.
import io
import os
import re
import subprocess
import sys

SWEEP = "tools/sweep_geom_cull.sh"
RTL = "fpga/rtl/geometry/zhao_geom_cull.sv"

sh = io.open(SWEEP, encoding="utf-8", newline="").read()
gold = io.open(RTL, encoding="utf-8", newline="").read()
body = sh[sh.index("MUTS=("):sh.index("\nexpected=")]
ents = re.findall(r'^"(.*?)"$', body, re.M | re.S)
vr = os.environ["VERILATOR_ROOT"]
bad = []
for e in ents:
    name, old, new = e.split("@@")
    NL = "\r\n" if "\r\n" in gold else "\n"
    o = old.replace("\n", NL)
    nw = new.replace("\n", NL)
    if gold.count(o) != 1:
        bad.append((name.strip(), "anchor x%d" % gold.count(o)))
        continue
    if o == nw:
        bad.append((name.strip(), "mutant identical to base"))
        continue
    io.open(RTL, "w", encoding="utf-8", newline="").write(gold.replace(o, nw, 1))
    rc = subprocess.run(
        [vr + "/bin/verilator_bin.exe", "--lint-only", "-Wall", "-Wno-DECLFILENAME", RTL],
        capture_output=True, text=True)
    if rc.returncode != 0:
        first = [l for l in (rc.stdout + rc.stderr).splitlines()
                 if "%Error" in l or "%Warning" in l]
        bad.append((name.strip(), first[0][:90] if first else "rc=%d" % rc.returncode))
io.open(RTL, "w", encoding="utf-8", newline="").write(gold)
print("linted %d mutants, %d do not build" % (len(ents), len(bad)))
for n, why in bad:
    print("   %-46s %s" % (n, why))
sys.exit(1 if bad else 0)
