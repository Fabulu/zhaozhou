# sweep_surface_stamp_preflight.py — LINT EVERY MUTANT BEFORE ANY IS SCORED.
#
# The sweep's guard 5 DISCARDS a mutant that does not link, which is correct but
# late: a malformed mutation then reads as "discarded" rather than as evidence,
# and before that guard existed it read as CAUGHT. Three of the LOD sweep's 23
# mutants were scored as caught by every run for three runs because two used
# `W'sd0` (a syntax error) and one wrote an always-false comparison that fails
# -Wall.
#
# TWO THINGS ARE DIFFERENT FROM sweep_geom_skin_preflight.py.
#
# (a) THE MUTANT TABLE SPANS THREE FILES. Each entry names its own, so the
#     anchor check has to open the right gold copy.
#
# (b) THE `$` IN THE TABLE IS BACKSLASH-ESCAPED. The entries live in a bash
#     array under double quotes, so `$signed` would be expanded to nothing by
#     the shell; the sweep writes `\$signed` and bash hands the mutation the
#     right text. This file reads the SOURCE, not bash's output, so it has to
#     perform the same unescape -- otherwise every `$signed` anchor "does not
#     match" here while matching perfectly in the sweep, and the preflight
#     rejects a table that is in fact correct.
#
# Every mutant is linted at ALL THREE SQ_RADIX settings, because they are three
# different elaborations of the same text: the b > 0 arms of `zhao_surface_sq`'s
# generate chain do not exist at all at SQ_RADIX = 1, so a mutation inside them
# can lint clean at the default and fail -Wall at 2 or 4.
import io
import os
import re
import subprocess
import sys

SWEEP = "tools/sweep_surface_stamp.sh"
FILES = [
    "fpga/rtl/surface/zhao_surface_blend.sv",
    "fpga/rtl/surface/zhao_surface_sq.sv",
    "fpga/rtl/surface/zhao_surface_stamp.sv",
]
RADIX = (1, 2, 4)

sh = io.open(SWEEP, encoding="utf-8", newline="").read()
gold = {f: io.open(f, encoding="utf-8", newline="").read() for f in FILES}
body = sh[sh.index("MUTS=("):sh.index("\nexpected=")]
ents = re.findall(r'^"(.*?)"\r?$', body, re.M | re.S)

# A GUARD THAT LINTS NOTHING MUST FAIL, NOT PASS.
#
# Found 2026-08-23 the first time a sweep here ran in a git worktree, as
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
    for r in RADIX:
        rc = subprocess.run(
            [vr + "/bin/verilator_bin.exe", "--lint-only", "-Wall", "-Wno-DECLFILENAME",
             "--top-module", "zhao_surface_stamp", "-GSQ_RADIX=%d" % r] + FILES,
            capture_output=True, text=True)
        if rc.returncode != 0:
            first = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if "%Error" in l or "%Warning" in l]
            bad.append((name.strip(),
                        "SQ_RADIX=%d: %s" % (r, first[0][:78] if first else "rc=%d" % rc.returncode)))
            break
    restore_all()

restore_all()
print("linted %d mutants at SQ_RADIX %s, %d do not build" % (len(ents), RADIX, len(bad)))
for n, why in bad:
    print("   %-62s %s" % (n, why))
sys.exit(1 if bad else 0)
