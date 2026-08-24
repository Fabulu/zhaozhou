# sweep_surface_sheet_preflight.py — LINT EVERY MUTANT BEFORE ANY IS SCORED.
#
# The sweep's guard 5 DISCARDS a mutant that does not link, which is correct but
# late: a malformed mutation then reads as "discarded" rather than as evidence,
# and before that guard existed it read as CAUGHT. Three of the LOD sweep's 23
# mutants were scored as caught by every run for three runs because two used
# `W'sd0` (a syntax error) and one wrote an always-false comparison that fails
# -Wall. The SURFACE.STAMP sweep's first preflight rejected five mutations that
# each orphaned a signal; every one was rewritten into a real defect that keeps
# the signal live, rather than the guard being relaxed. The TEXTURE.TMU sweep's
# preflight rejected four more of the same shape.
#
# EVERY MUTANT IS LINTED AT MORE THAN ONE Slots, and here that is not a
# formality. `Slots` is the block's one elaboration parameter and it changes the
# elaborated design materially: `SlotBits` is `(Slots <= 1) ? 1 : $clog2(Slots)`,
# so the associative lookup, the free-slot search and the width of
# `res_occupancy_o` all move with it. A mutation that touches the directory can
# lint clean at the default and fail -Wall elsewhere. See SLOTS below for why
# the lower bound is excluded.
#
# THE `$` IN THE TABLE IS BACKSLASH-ESCAPED, carried from
# sweep_texture_tmu_preflight.py: the entries live in a bash array under double
# quotes, so `$clog2` would be expanded to nothing by the shell. This file reads
# the SOURCE, not bash's output, so it performs the same unescape — otherwise an
# anchor that matches perfectly in the sweep "does not match" here.
import io
import os
import re
import subprocess
import sys

SWEEP = "tools/sweep_surface_sheet.sh"
FILES = ["fpga/rtl/surface/zhao_surface_sheet.sv"]
# Slots = 1 is NOT linted, and that is a finding rather than a convenience.
# The PRISTINE block does not lint clean at Slots = 1 -- and neither does the
# pre-rearchitecture version at HEAD, so the split did not cause it:
#
#   %Warning-WIDTHTRUNC: Bit extraction of array[4095:0] requires 12 bit index,
#                        not 13 bits.
#
# `SlotBits` is `(Slots <= 1) ? 1 : $clog2(Slots)`, so it is 1 even when there
# is only one slot, and `AddrBits = SlotBits + 12` is therefore 13 against a
# 4,096-word array. The extra bit is always zero (there is no slot 1 to select),
# so the block is functionally unharmed and Verilator truncates it -- but the
# parameter does not elaborate cleanly at its own lower bound. Left alone
# deliberately: RUN-20260824-0317's scope is the storage SHAPE, and changing
# the address width at an untested `Slots` is a behaviour change wearing a
# lint fix. Docketed; see that run's SPEC.
SLOTS = (2, 4)

sh = io.open(SWEEP, encoding="utf-8", newline="").read()
gold = {f: io.open(f, encoding="utf-8", newline="").read() for f in FILES}
body = sh[sh.index("MUTS=("):sh.index("\n# ---------------------------------------------------------------------------\n# ZHAO_SWEEP_ONLY")]
ents = re.findall(r'^"(.*?)"\r?$', body, re.M | re.S)

# A GUARD THAT LINTS NOTHING MUST FAIL, NOT PASS.
#
# Found 2026-08-23 the first time a sweep ran in a git worktree, as the owner
# ruling requires. A fresh checkout gave the sweep CRLF endings, `^"(.*?)"$`
# matched nothing because the character before each newline was \r rather than
# the closing quote, and the preflight printed "linted 0 mutants, 0 do not
# build" and exited 0. A clean pass over an empty set is the most flattering
# possible failure, and it is the same shape as every entry in
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
    o = old.replace("\\$", "$").replace("\n", NL)
    nw = new.replace("\\$", "$").replace("\n", NL)
    if g.count(o) != 1:
        bad.append((name.strip(), "anchor x%d in %s" % (g.count(o), path.split("/")[-1])))
        continue
    if o == nw:
        bad.append((name.strip(), "mutant identical to base"))
        continue
    io.open(path, "w", encoding="utf-8", newline="").write(g.replace(o, nw, 1))
    for n in SLOTS:
        rc = subprocess.run(
            [vr + "/bin/verilator_bin.exe", "--lint-only", "-Wall", "-Wno-DECLFILENAME",
             "--top-module", "zhao_surface_sheet", "-GSlots=%d" % n] + FILES,
            capture_output=True, text=True)
        if rc.returncode != 0:
            first = [l for l in (rc.stdout + rc.stderr).splitlines()
                     if "%Error" in l or "%Warning" in l]
            bad.append((name.strip(),
                        "Slots=%d: %s" % (n, first[0][:78] if first else "rc=%d" % rc.returncode)))
            break
    restore_all()

restore_all()
print("linted %d mutants at Slots %s, %d do not build" % (len(ents), SLOTS, len(bad)))
for n, why in bad:
    print("   %-72s %s" % (n, why))
sys.exit(1 if bad else 0)
