"""The four-part packet account the Decrufter brief requires of every packet.

    "Every packet must report: What disappeared from the source -> what
     disappeared from the synthesized circuit -> what replaced it -> what the
     real measurements say."

Nothing produced that before. Each packet reported a fit row and a prose claim,
and the two were joined by whoever was writing the paragraph -- which is how
`@pktC` came to be quoted as packet C's cost while being (a) a measurement of a
circuit with a live metadata-swap defect and (b) taken from a DIRTY TREE, so its
own `sourceCommit` does not describe what was fitted.

THE GATE THAT MATTERS
---------------------
Sections 2 and 4 are refused unless both receipts are anchored:

  * `rtlCleanAtHead` must be true. A dirty-tree row records a commit it did not
    measure. This is CLAUDE.md's "never compare a current file to an old
    measurement" in its worst form -- the receipt looks fully attributed, with a
    hash and a timestamp, and the hash is of something else.
  * each row's `sourceCommit` must be the ref it is being presented as.
  * the fit must have COMPLETED. `failed:structure` counts as completed -- see
    MEASURED_STATUSES below, and the comment beside it, which is the more
    interesting half of this file.

A refusal here is the tool working. The whole failure mode is a number that is
willing to be quoted.

Sections 1 and 3 come from git and are printed regardless, because they are
answerable without a fit at all.

WHAT IT DOES NOT DO
-------------------
It does not judge whether a deletion was a good idea, and it does not attribute
an Fmax delta to a change -- the owner brief is explicit that matching worst-path
families is "neither necessary nor sufficient for attribution". It lays the four
columns side by side and leaves the reading to a person.
"""

import io
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
FIT = os.path.join(ROOT, "reports", "synthesis", "zhao_block_fit.json")

# Constructs worth counting separately in a source diff. A packet that claims to
# have removed a scheduler should show `always_ff` blocks and instantiations
# going away, not merely a line count -- a line count is equally consistent with
# having reflowed the comments.
CONSTRUCTS = [
    ("always_ff", re.compile(r"\balways_ff\b")),
    ("always_comb", re.compile(r"\balways_comb\b")),
    ("generate", re.compile(r"\b(generate|endgenerate)\b")),
    ("instantiation", re.compile(r"^\s*(zhao_\w+)\s+(#\s*\(|\w+\s*\()")),
    ("array decl", re.compile(r"^\s*(logic|wire|reg)\b.*\[[^\]]+\]\s*\w+\s*\[")),
    ("counter bump", re.compile(r"\b\w+_o\s*<=\s*\w+_o\s*\+")),
]

RES = ["alms", "registers", "ramBlocks", "blockMemoryBits", "dspBlocks",
       "virtualPins"]

NL = chr(10)


def git(*args):
    # -c core.autocrlf=true. This helper takes its subcommand at RUNTIME, so it
    # could be any of them, and it must therefore be guarded unconditionally.
    # core.autocrlf lives only in Git-for-Windows' system config, so a bare
    # `git` that resolves to c:/devkitPro/msys2/usr/bin/git.exe reports 1,200
    # line-ending-only diffs on this clean tree. Measured 2026-09-09; see
    # reports/TWO-GITS-DISAGREE-ABOUT-CLEAN-20260909.md.
    return subprocess.check_output(
        ["git", "-c", "core.autocrlf=true"] + list(args), cwd=ROOT).decode(
        "utf-8", "replace")


def load_rows():
    d = json.load(io.open(FIT, encoding="utf-8"))
    return d["blocks"], d.get("tool"), d.get("device")


def find_row(all_rows, key):
    hit = [r for r in all_rows if r["module"] == key]
    return hit[-1] if hit else None


# `failed:structure` is NOT a failed measurement. It means the fit completed and
# the BUDGET RULES rejected the result -- ALM over 7500, DSP over 14, and so on.
# The ALM and Fmax in such a row are real numbers from a real fit.
#
# Getting this wrong in the first version of this file was instructive in the
# usual direction. The honest row -- 13133 ALM, clean tree, digest recorded,
# three rule violations written out -- is stamped `failed:structure`; the
# unanchored one, fitted from a DIRTY TREE, is stamped `ok`. A gate reading only
# `status` refuses the trustworthy row and waves the untrustworthy one through.
# The status field flatters exactly the row that deserves it least.
MEASURED_STATUSES = ("ok", "failed:structure")


def anchor_problems(row, key, ref_sha):
    """Every reason this row may not be quoted. Empty list means quotable."""
    bad = []
    if row is None:
        return ["no row named %r in the fit receipt" % key]
    if not row.get("rtlCleanAtHead"):
        bad.append(
            "rtlCleanAtHead is FALSE -- the tree was dirty when this was "
            "measured, so its sourceCommit %s names a commit it did not fit"
            % str(row.get("sourceCommit"))[:8])
    if row.get("status") not in MEASURED_STATUSES:
        bad.append("status is %r -- the fit did not complete, so there is no "
                   "measurement here to quote" % row.get("status"))
    if ref_sha and row.get("sourceCommit") != ref_sha:
        bad.append(
            "sourceCommit %s is not the ref it is presented as (%s)"
            % (str(row.get("sourceCommit"))[:8], ref_sha[:8]))
    return bad


def rule_caveats(row, key):
    """Warnings that accompany a quotable row rather than disqualifying it."""
    out = []
    if row is None:
        return out
    v = row.get("ruleViolations") or []
    for x in v:
        out.append("%s VIOLATES A BUDGET RULE: %s" % (key, x))
    if "@" in key and not v:
        # 26 labelled rows in this receipt carry zero ruleViolations while 12 of
        # 92 unlabelled ones carry some. Labelled rows are never rule-checked, so
        # an empty list here is silence, not compliance -- and reading it as
        # compliance is a mistake already made once in this repository.
        out.append("%s is a LABELLED row: labelled rows are never rule-checked, "
                   "so the absence of violations is silence, not compliance"
                   % key)
    return out


def source_account(before, after, paths):
    """(removed lines, added lines, per-construct deltas) between two refs."""
    diff = git("diff", "--unified=0", before, after, "--", *paths)
    removed = [l[1:] for l in diff.split(NL)
               if l.startswith("-") and not l.startswith("---")]
    added = [l[1:] for l in diff.split(NL)
             if l.startswith("+") and not l.startswith("+++")]
    cons = []
    for name, pat in CONSTRUCTS:
        r = sum(1 for l in removed if pat.search(l))
        a = sum(1 for l in added if pat.search(l))
        if r or a:
            cons.append((name, r, a))
    return removed, added, cons


def self_fire_test():
    """The anchor gate must REFUSE a dirty row and accept a clean one.

    A gate whose only input is the real receipt can never be shown to fire
    without corrupting the real receipt.
    """
    dirty = {"module": "m", "rtlCleanAtHead": False, "status": "ok",
             "sourceCommit": "a" * 40}
    clean = {"module": "m", "rtlCleanAtHead": True, "status": "ok",
             "sourceCommit": "a" * 40}
    wrong = {"module": "m", "rtlCleanAtHead": True, "status": "ok",
             "sourceCommit": "b" * 40}
    # A completed fit that broke a budget rule IS a measurement.
    overbudget = {"module": "m", "rtlCleanAtHead": True,
                  "status": "failed:structure", "sourceCommit": "a" * 40,
                  "ruleViolations": ["ALM 13133 > allowed 7500"]}
    # A fit that never finished is not.
    timedout = {"module": "m", "rtlCleanAtHead": True, "status": "timeout",
                "sourceCommit": "a" * 40}
    return (len(anchor_problems(dirty, "m", "a" * 40)) == 1
            and anchor_problems(clean, "m", "a" * 40) == []
            and len(anchor_problems(wrong, "m", "a" * 40)) == 1
            and anchor_problems(overbudget, "m", "a" * 40) == []
            and len(rule_caveats(overbudget, "m")) == 1
            and len(anchor_problems(timedout, "m", "a" * 40)) == 1
            and len(rule_caveats({"module": "m@x"}, "m@x")) == 1
            and len(anchor_problems(None, "m", None)) == 1)


def main(argv):
    if not self_fire_test():
        print("PACKET ACCOUNTING BROKEN: the anchor gate no longer refuses a "
              "dirty or mismatched row. Refusing to produce an account from a "
              "gate that cannot fail.")
        return 2

    if len(argv) < 5:
        print("usage: packet_accounting.py BEFORE_KEY AFTER_KEY BEFORE_REF "
              "AFTER_REF [path ...]")
        print("  KEYs are fit-receipt module names, e.g. "
              "zhao_texture_island_v3_top@pktC")
        return 2
    bkey, akey, bref, aref = argv[1:5]
    paths = argv[5:] or ["fpga/rtl/"]

    all_rows, tool, device = load_rows()
    brow, arow = find_row(all_rows, bkey), find_row(all_rows, akey)
    try:
        bsha = git("rev-parse", bref).strip()
        asha = git("rev-parse", aref).strip()
    except subprocess.CalledProcessError:
        print("cannot resolve refs %r / %r" % (bref, aref))
        return 2

    removed, added, cons = source_account(bref, aref, paths)

    print("PACKET ACCOUNT  %s -> %s" % (bkey, akey))
    print("  refs %s -> %s   tool %s %s   device %s"
          % (bsha[:8], asha[:8], (tool or {}).get("name"),
             (tool or {}).get("version"), device))
    print("")
    print("1. WHAT DISAPPEARED FROM THE SOURCE")
    print("   %d lines removed across %s" % (len(removed), ", ".join(paths)))
    for name, r, _a in cons:
        if r:
            print("     - %-14s %d removed" % (name, r))
    print("")
    print("3. WHAT REPLACED IT")
    print("   %d lines added" % len(added))
    for name, _r, a in cons:
        if a:
            print("     + %-14s %d added" % (name, a))
    print("")

    bbad = anchor_problems(brow, bkey, bsha)
    abad = anchor_problems(arow, akey, asha)
    print("2. WHAT DISAPPEARED FROM THE SYNTHESIZED CIRCUIT")
    print("4. WHAT THE REAL MEASUREMENTS SAY")
    if bbad or abad:
        print("   REFUSED. A receipt that cannot be anchored to the source it")
        print("   is presented as measuring is not evidence about that source.")
        for k, bad in ((bkey, bbad), (akey, abad)):
            for b in bad:
                print("     %s: %s" % (k, b))
        print("")
        print("   Sections 1 and 3 above stand -- they come from git, not from")
        print("   a receipt. Sections 2 and 4 need a re-fit from a clean tree.")
        return 1

    for c in rule_caveats(brow, bkey) + rule_caveats(arow, akey):
        print("   ! " + c)
    if rule_caveats(brow, bkey) or rule_caveats(arow, akey):
        print("")

    for f in RES:
        bv, av = brow.get(f), arow.get(f)
        if bv is None or av is None:
            continue
        try:
            d = int(av) - int(bv)
        except (TypeError, ValueError):
            continue
        print("   %-16s %8s -> %-8s  %+d" % (f, bv, av, d))
    print("")
    print("   Attribution of any Fmax delta is NOT asserted here: the owner")
    print("   brief holds that matching worst-path families is neither")
    print("   necessary nor sufficient for it.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
