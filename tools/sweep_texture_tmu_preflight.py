# sweep_texture_tmu_preflight.py — LINT EVERY MUTANT BEFORE ANY IS SCORED.
#
# The sweep's guard 5 DISCARDS a mutant that does not link, which is correct but
# late: a malformed mutation then reads as "discarded" rather than as evidence,
# and before that guard existed it read as CAUGHT. Three of the LOD sweep's 23
# mutants were scored as caught by every run for three runs because two used
# `W'sd0` (a syntax error) and one wrote an always-false comparison that fails
# -Wall. The SURFACE.STAMP sweep's first preflight run rejected five mutations
# that each orphaned a signal; every one was rewritten into a real defect that
# keeps the signal live, rather than the guard being relaxed.
#
# TWO THINGS TRAVEL FROM sweep_surface_stamp_preflight.py UNCHANGED.
#
# (a) THE MUTANT TABLE SPANS TWO FILES. Each entry names its own, so the anchor
#     check has to open the right gold copy.
#
# (b) THE `$` IN THE TABLE IS BACKSLASH-ESCAPED. The entries live in a bash
#     array under double quotes, so `$signed` would be expanded to nothing by
#     the shell; the sweep writes `\$signed` and bash hands the mutation the
#     right text. This file reads the SOURCE, not bash's output, so it has to
#     perform the same unescape -- otherwise every `$signed` anchor "does not
#     match" here while matching perfectly in the sweep, and the preflight
#     rejects a table that is in fact correct.
#
# EVERY MUTANT IS LINTED AT ALL THREE FILT_LANES SETTINGS, and here that is not
# a formality: `zhao_texture_tmu`'s `g_fin` generate takes a DIFFERENT branch at
# each setting (at FILT_LANES = 4 every channel is combinational and the
# `fres_r` branch is not elaborated at all), so a mutation inside one branch can
# lint clean at the default and fail -Wall at 2 or 1.
import io
import os
import re
import subprocess
import sys

SWEEP = "tools/sweep_texture_tmu.sh"
FILES = [
    "fpga/rtl/texture/zhao_texture_bilerp.sv",
    "fpga/rtl/texture/zhao_texture_tmu.sv",
]
LANES = (4, 2, 1)

sh = io.open(SWEEP, encoding="utf-8", newline="").read()
gold = {f: io.open(f, encoding="utf-8", newline="").read() for f in FILES}
body = sh[sh.index("MUTS=("):sh.index("\nexpected=")]
ents = re.findall(r'^"(.*?)"\r?$', body, re.M | re.S)

# A GUARD THAT LINTS NOTHING MUST FAIL, NOT PASS.
#
# Found 2026-08-23 the first time a sweep ran in a git worktree, as
# docs/OWNER_DOCKET.md requires. A fresh checkout gave the sweep CRLF endings,
# `^"(.*?)"$` matched nothing because the character before each newline was \r
# rather than the closing quote, and the preflight printed "linted 0 mutants,
# 0 do not build" and exited 0. A clean pass over an empty set is the most
# flattering possible failure, and it is the same shape as every entry in
# reports/QUARTUS_GOTCHAS.md: a directive accepted and silently ignored, with no
# symptom except a number that did not move.
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
    # (b) above: undo the bash double-quote escaping of `$`.
    o = old.replace("\\$", "$").replace("\n", NL)
    nw = new.replace("\\$", "$").replace("\n", NL)
    if g.count(o) != 1:
        bad.append((name.strip(), "anchor x%d in %s" % (g.count(o), path.split("/")[-1])))
        continue
    if o == nw:
        bad.append((name.strip(), "mutant identical to base"))
        continue
    io.open(path, "w", encoding="utf-8", newline="").write(g.replace(o, nw, 1))
    for n in LANES:
        rc = subprocess.run(
            [vr + "/bin/verilator_bin.exe", "--lint-only", "-Wall", "-Wno-DECLFILENAME",
             "--top-module", "zhao_texture_tmu", "-GFILT_LANES=%d" % n] + FILES,
            capture_output=True, text=True)
        if rc.returncode != 0:
            first = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if "%Error" in l or "%Warning" in l]
            bad.append((name.strip(),
                        "FILT_LANES=%d: %s" % (n, first[0][:78] if first else "rc=%d" % rc.returncode)))
            break
    restore_all()

restore_all()
print("linted %d mutants at FILT_LANES %s, %d do not build" % (len(ents), LANES, len(bad)))
for n, why in bad:
    print("   %-70s %s" % (n, why))
sys.exit(1 if bad else 0)
