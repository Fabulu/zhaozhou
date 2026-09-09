#!/usr/bin/env python3
"""Sum measured DSP (and ALM, M10K) over the manifest's intended production blocks.

WHY THIS IS A TOOL
------------------
The 154-DSP figure in reports/DSP-BUDGET-CENSUS-20260908.md was computed by hand,
and it has been re-quoted, adjusted and argued about ever since -- including by me,
twice in one day, once wrongly. The owner's live question is "we're over by like
180 to 112"; a number that gets re-derived by hand every time it is asked is a
number that drifts.

WHAT IT COUNTS, and the choices are the substance:

* `top:` entries only. `inside:` blocks are counted through their parent, and
  `excluded:` blocks are not part of the planned machine. Counting an `inside:`
  block as well would double it -- which is the mistake the manifest's own
  comments repeatedly warn about.
* LABELLED ledger rows are skipped. `@g4-nctx12`, `@map`, `@v3-before` are
  alternate MEASUREMENTS of one module, not additional hardware. Summing them
  would inflate the total by however many times a block has been measured.
* MapOnly rows count for DSP but NOT for ALM. A map row reports dspBlocks,
  blockMemoryBits and registers; `alms` and `ramBlocks` are fitter results and
  are absent. Treating a missing ALM as zero is how a rule silently cannot fire.
* Unmeasured blocks are REPORTED, not assumed zero. The total is a FLOOR, and the
  count of unfitted blocks is printed beside it, because the census's whole point
  is that 42 blocks have never been fitted and the figure can only rise.

Read `rtlCleanAtHead` before quoting any row: a row fitted from a dirty tree has
a digest that describes nothing. Dirty rows are counted (they are the best number
available) and listed separately so the uncertainty travels with the total.

Usage:
    python tools/budget/dsp_census.py
    python tools/budget/dsp_census.py --self-test
"""
import io
import json
import os
import sys

LEDGER = os.path.join("reports", "synthesis", "zhao_block_fit.json")
sys.path.insert(0, os.path.join("tools", "quartus"))

DEVICE = {"alm": 41910, "dsp": 112, "m10k": 553}


def load_rows():
    """Unlabelled rows, plus a separate map of modules ONLY ever measured labelled.

    "Measured only under a label" is not the same as "never measured", and lumping
    them together understates the floor. `zhao_raster_rcp24_v3` has five rows and
    every one carries a label, so a filter that drops all labelled rows reports it
    as unfitted and silently omits its 3 DSP.

    Kept out of the total on purpose -- a labelled row is a measurement of a
    PARAMETERISATION, and which one the machine will ship is a decision, not a
    reading. But it is reported separately, with its DSP, so the floor's own
    shortfall is visible instead of being folded into "45 unmeasured".
    """
    d = json.load(io.open(LEDGER, encoding="utf-8"))
    rows, labelled = {}, {}
    for r in d["blocks"]:
        name = r["module"]
        if "@" in name:
            base = name.split("@")[0]
            if r.get("dspBlocks") is not None:
                labelled.setdefault(base, []).append(r)
            continue
        rows[name] = r
    return rows, labelled


def census(tops, rows):
    dsp = alm = m10k = 0
    unmeasured, dirty, maponly = [], [], []
    for m in sorted(tops):
        r = rows.get(m)
        if r is None:
            unmeasured.append(m)
            continue
        if r.get("dspBlocks") is None:
            unmeasured.append(m)
            continue
        dsp += r["dspBlocks"]
        if r.get("alms") is not None:
            alm += r["alms"]
        else:
            maponly.append(m)
        if r.get("ramBlocks") is not None:
            m10k += r["ramBlocks"]
        if r.get("rtlCleanAtHead") is False:
            dirty.append(m)
    return dsp, alm, m10k, unmeasured, dirty, maponly


def self_test():
    """The sum must move when a row moves, and must IGNORE labelled variants."""
    rows = {
        "a": {"module": "a", "dspBlocks": 6, "alms": 100, "ramBlocks": 1,
              "rtlCleanAtHead": True},
        "b": {"module": "b", "dspBlocks": 3, "alms": None, "ramBlocks": None,
              "rtlCleanAtHead": True},
        "c": {"module": "c", "dspBlocks": None, "alms": None},
    }
    dsp, alm, m10k, un, dirty, mo = census(["a", "b", "c", "d"], rows)
    assert dsp == 9, "dsp sum %d, expected 9" % dsp
    assert alm == 100, "a map row with alms=None must not count as 0 ALM: %d" % alm
    assert mo == ["b"], "map-only row not reported: %s" % mo
    assert sorted(un) == ["c", "d"], "unmeasured wrong: %s" % un
    # and a labelled variant must be dropped before it can be summed
    d = {"blocks": [{"module": "a", "dspBlocks": 6},
                    {"module": "a@map", "dspBlocks": 6}]}
    tmp = {}
    for r in d["blocks"]:
        if "@" not in r["module"]:
            tmp[r["module"]] = r
    assert list(tmp) == ["a"], "labelled variant survived the filter: %s" % list(tmp)
    return True


def main():
    self_test()
    if "--self-test" in sys.argv[1:]:
        print("dsp_census self-test: sums, skips labelled variants, refuses to "
              "read a missing ALM as zero, and reports unmeasured blocks.")
        return 0

    from check_prod_manifest import read_manifest
    tops, excluded = read_manifest()
    rows, labelled = load_rows()
    dsp, alm, m10k, unmeasured, dirty, maponly = census(tops, rows)

    print("manifest: %d intended production blocks (top:), %d excluded"
          % (len(tops), len(excluded)))
    print()
    print("  %-8s %8s %8s   %s" % ("", "MEASURED", "DEVICE", "note"))
    for k, got in (("DSP", dsp), ("ALM", alm), ("M10K", m10k)):
        dev = DEVICE[k.lower()]
        over = got - dev
        note = "OVER by %d" % over if over > 0 else "%d spare" % -over
        print("  %-8s %8d %8d   %s" % (k, got, dev, note))
    print()
    # THE TOTAL IS BOUNDED ON BOTH SIDES AND THE CAVEAT MUST TRAVEL WITH IT.
    #
    # Printed here rather than left to the reader, because "ALM OVER by 2361" is
    # exactly the kind of line that gets quoted on its own. gen_prod_top.py's own
    # header says what this top is: one instance of each intended block, wired to
    # nothing.
    print("  BOUNDED ON BOTH SIDES -- neither figure is 'the machine':")
    print("    UPPER bound on the sum of parts: composition SHARES queues,")
    print("      control and arithmetic that a per-block sum counts twice, and")
    print("      leaf rows carry virtual pins the composed design does not.")
    print("      MEASURED precedent: the composed island came in 2.4% under the")
    print("      sum of its standalone fits.")
    print("    LOWER bound on the machine: integration glue is not here, and")
    print("      neither are the blocks nobody has built yet.")
    print()
    # SPLIT "not fitted yet" FROM "cannot be fitted at all".
    #
    # A bare list of unmeasured blocks reads as a queue of work waiting its turn.
    # It is not: a block with no `- top:` entry in design/fit_targets.yml has no
    # source list, so run_block_fit refuses it at preflight and it can never be
    # measured by anyone until someone writes the target. D22 recorded sixteen of
    # twenty-four geometry blocks in exactly that state.
    #
    # Those two populations need different work -- one needs toolchain time, the
    # other needs a target authored with its rules stated BEFORE the fit, because
    # a rule written afterwards reports a pass. Printing them as one list hides
    # which is which.
    try:
        y = io.open(os.path.join("design", "fit_targets.yml"),
                    encoding="utf-8", errors="replace").read()
    except OSError:
        y = ""
    no_target = [m for m in unmeasured if ("- top: %s\n" % m) not in y]
    queued = [m for m in unmeasured if m not in no_target]

    print("  THIS IS A FLOOR. %d of %d intended blocks have no measured DSP "
          "figure, and they split in two:" % (len(unmeasured), len(tops)))
    print()
    print("    %d have a fit target and simply have not been run:" % len(queued))
    for m in sorted(queued)[:8]:
        print("       %s" % m)
    if len(queued) > 8:
        print("       ... and %d more" % (len(queued) - 8))
    print()
    print("    %d have NO `- top:` entry in design/fit_targets.yml, so they "
          "CANNOT" % len(no_target))
    print("    be fitted by anyone until a target is authored -- with its rules")
    print("    stated BEFORE the fit, because a rule written afterwards reports")
    print("    a pass:")
    for m in sorted(no_target)[:12]:
        print("       %s" % m)
    if len(no_target) > 12:
        print("       ... and %d more" % (len(no_target) - 12))
    only_lab = [m for m in unmeasured if m in labelled]
    if only_lab:
        print()
        print("  of those, %d HAVE been measured but ONLY under a label, so they "
              "are excluded from the total above rather than unknown:" % len(only_lab))
        for m in sorted(only_lab):
            ds = sorted(set(r["dspBlocks"] for r in labelled[m]))
            print("     %-38s DSP %s across %d labelled row(s)"
                  % (m, ds, len(labelled[m])))
        print("     A labelled row measures a PARAMETERISATION; which one ships is")
        print("     a decision, not a reading. Counting one would pick it silently.")
    if maponly:
        print()
        print("  %d block(s) contribute DSP from a MapOnly row and therefore "
              "contribute NO ALM -- the ALM total above is short by their area:"
              % len(maponly))
        for m in maponly:
            print("     %s" % m)
    if dirty:
        print()
        print("  %d row(s) were fitted from a DIRTY TREE, so their digest "
              "describes nothing. Counted, because they are the best number "
              "available, and listed so the doubt travels with the total:"
              % len(dirty))
        for m in dirty:
            print("     %s" % m)
    return 0


if __name__ == "__main__":
    sys.exit(main())
