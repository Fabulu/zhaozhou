# sweep_project_core_preflight.py — LINT EVERY MUTANT BEFORE ANY IS SCORED.
#
# The sweep's guards DISCARD a mutant that does not link, which is correct but
# late: a malformed mutation then reads as "discarded" rather than as evidence,
# and before those guards existed it read as CAUGHT. Three of the LOD sweep's 23
# mutants were scored as caught by every run for three runs because two used
# `W'sd0` (a syntax error) and one wrote an always-false comparison that fails
# -Wall. The SURFACE.STAMP sweep's first preflight rejected five mutations that
# each orphaned a signal; every one was rewritten into a real defect that keeps
# the signal live, rather than the guard being relaxed.
#
# THREE FILES, THREE TOPS. A mutation in zhao_project_core.sv changes BOTH
# shells, so every mutant is linted as all three tops, not just as the one whose
# file it edits. RUN-20260824-0522 exists precisely because two blocks shared an
# implementation without sharing a file; a preflight that linted only the edited
# file would reproduce the same blind spot in the tooling.
#
# AND THE CORE IS LINTED AT BOTH PAYLOAD WIDTHS ITS CALLERS ACTUALLY USE — 16
# for GEOM's source id and 42 for TERRAIN's {corner, src, matA, matB, weight}.
# A parameter with one tested value is a constant with extra steps, and the
# whole point of the payload being a parameter is that the two callers differ.
#
# THE `$` IN THE TABLE IS BACKSLASH-ESCAPED, carried from
# sweep_texture_tmu_preflight.py: the entries live in a bash array under double
# quotes, so `$signed` would be expanded to nothing by the shell. This file
# reads the SOURCE, not bash's output, so it performs the same unescape —
# otherwise an anchor that matches perfectly in the sweep "does not match" here.
import io
import os
import re
import subprocess
import sys

SWEEP = "tools/sweep_project_core.sh"
CORE = "fpga/rtl/common/zhao_project_core.sv"
GEOM = "fpga/rtl/geometry/zhao_geom_project.sv"
TERR = "fpga/rtl/terrain/zhao_terrain_project.sv"
FILES = [CORE, GEOM, TERR]

# (top module, sources, extra verilator args)
TOPS = [
    ("zhao_geom_project", [GEOM, CORE], []),
    ("zhao_terrain_project", [TERR, CORE], []),
    ("zhao_project_core", [CORE], ["-GPAYLOAD_W=16"]),
    ("zhao_project_core", [CORE], ["-GPAYLOAD_W=42"]),
]

sh = io.open(SWEEP, encoding="utf-8", newline="").read()
gold = {f: io.open(f, encoding="utf-8", newline="").read() for f in FILES}
body = sh[sh.index("MUTS=("):sh.index(
    "\n# ---------------------------------------------------------------------------\n"
    "# ZHAO_SWEEP_ONLY")]
ents = re.findall(r'^"(.*?)"\r?$', body, re.M | re.S)

# A GUARD THAT LINTS NOTHING MUST FAIL, NOT PASS.
#
# Found 2026-08-23 the first time a sweep ran in a git worktree, as the owner
# ruling requires. A fresh checkout gave the sweep CRLF endings, `^"(.*?)"$`
# matched nothing because the character before each newline was \r rather than
# the closing quote, and the preflight printed "linted 0 mutants, 0 do not
# build" and exited 0. A clean pass over an empty set is the most flattering
# possible failure.
if len(ents) < 2:
    sys.exit("PREFLIGHT ABORT: parsed %d mutants out of %s. The mutant table did "
             "not parse -- check line endings. Scoring an empty sweep is worse "
             "than not running one." % (len(ents), SWEEP))

vr = os.environ["VERILATOR_ROOT"]
bad = []


def restore_all():
    for f in FILES:
        io.open(f, "w", encoding="utf-8", newline="").write(gold[f])


for e in ents:
    name, path, old, new = e.split("@@")
    path = path.strip()
    if path not in gold:
        bad.append((name.strip(), "names file %r, which the preflight does not track" % path))
        continue
    g = gold[path]
    NL = "\r\n" if "\r\n" in g else "\n"
    o = old.replace("\\$", "$").replace("\n", NL)
    nw = new.replace("\\$", "$").replace("\n", NL)
    if g.count(o) != 1:
        bad.append((name.strip(), "anchor x%d in %s" % (g.count(o), path.split("/")[-1])))
        continue
    if o == nw:
        bad.append((name.strip(), "mutant identical to base"))
        continue
    io.open(path, "w", encoding="utf-8", newline="").write(g.replace(o, nw, 1))
    for top, srcs, extra in TOPS:
        rc = subprocess.run(
            [vr + "/bin/verilator_bin.exe", "--lint-only", "-Wall", "-Wno-DECLFILENAME",
             "--top-module", top] + extra + srcs,
            capture_output=True, text=True)
        if rc.returncode != 0:
            first = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if "%Error" in l or "%Warning" in l]
            tag = top + (" " + extra[0] if extra else "")
            bad.append((name.strip(),
                        "%s: %s" % (tag, first[0][:70] if first else "rc=%d" % rc.returncode)))
            break
    restore_all()

restore_all()
print("linted %d mutants as %d tops, %d do not build" % (len(ents), len(TOPS), len(bad)))
for n, why in bad:
    print("   %-70s %s" % (n, why))
sys.exit(1 if bad else 0)
