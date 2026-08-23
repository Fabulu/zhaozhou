# sweep_geom_skin_preflight.py — LINT EVERY MUTANT BEFORE ANY OF THEM IS SCORED.
#
# The same guard as tools/sweep_geom_cull_preflight.py, pointed at the skin
# sweep, and for the same reason: guard 5 in the sweep DISCARDS a mutant that
# does not link, which is correct but late. A malformed mutation then reads as
# "discarded" rather than as evidence, and before that guard existed it read as
# CAUGHT — three of the LOD sweep's 23 mutants were scored as caught by every
# run for three runs because two used `W'sd0` (a syntax error) and one wrote a
# comparison that is always false and fails -Wall.
#
# ONE THING IS DIFFERENT HERE. zhao_geom_skin is parameterised, and the three
# MUL_LANES settings are three different elaborations of the same text: a
# mutation can lint clean at 3 and fail -Wall at 1. Every mutant is therefore
# linted at ALL THREE points on the frontier, not just at the default.
import io
import os
import re
import subprocess
import sys

SWEEP = "tools/sweep_geom_skin.sh"
RTL = "fpga/rtl/geometry/zhao_geom_skin.sv"
LANES = (1, 3, 6)

sh = io.open(SWEEP, encoding="utf-8", newline="").read()
gold = io.open(RTL, encoding="utf-8", newline="").read()
body = sh[sh.index("MUTS=("):sh.index("\nexpected=")]
ents = re.findall(r'^"(.*?)"\r?$', body, re.M | re.S)

# A GUARD THAT LINTS NOTHING MUST FAIL, NOT PASS.
#
# Found 2026-08-23 the first time this sweep ran in a git worktree, as
# docs/OWNER_DOCKET.md requires. A fresh checkout gave the sweep CRLF endings,
# `^"(.*?)"$` matched nothing because the character before each newline was
# \r rather than the closing quote, and the preflight printed
# "linted 0 mutants, 0 do not build" and exited 0. A clean pass over an empty
# set is the most flattering possible failure, and it is the same shape as
# every other entry in reports/QUARTUS_GOTCHAS.md: a directive accepted and
# silently ignored, with no symptom except a number that did not move.
#
# The regex now tolerates CRLF and .gitattributes pins *.sh to LF, so this
# check should never fire. It exists because both of those are the kind of
# thing that gets edited.
if len(ents) < 2:
    sys.exit("PREFLIGHT ABORT: parsed %d mutants out of %s. The mutant table did "
             "not parse -- check line endings. Scoring an empty sweep is worse "
             "than not running one." % (len(ents), SWEEP))
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
    for lanes in LANES:
        rc = subprocess.run(
            [vr + "/bin/verilator_bin.exe", "--lint-only", "-Wall", "-Wno-DECLFILENAME",
             "--top-module", "zhao_geom_skin", "-GMUL_LANES=%d" % lanes, RTL],
            capture_output=True, text=True)
        if rc.returncode != 0:
            first = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if "%Error" in l or "%Warning" in l]
            bad.append((name.strip(),
                        "MUL_LANES=%d: %s" % (lanes, first[0][:70] if first else "rc=%d" % rc.returncode)))
            break
io.open(RTL, "w", encoding="utf-8", newline="").write(gold)
print("linted %d mutants at MUL_LANES %s, %d do not build" % (len(ents), LANES, len(bad)))
for n, why in bad:
    print("   %-56s %s" % (n, why))
sys.exit(1 if bad else 0)
