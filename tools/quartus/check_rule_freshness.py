#!/usr/bin/env python3
r"""check_rule_freshness.py -- has each fit rule ever actually been EVALUATED?

WHY THIS EXISTS
---------------
Found 2026-09-04. Three blocks in `reports/synthesis/zhao_block_fit.json` carry
`status: ok` while measuring far past a declared ceiling:

    zhao_raster_texjoin_v2     7,151 registers   max_registers: 2500
    zhao_raster_perspuv_svc    3,293 registers   max_registers:  700
    zhao_raster_rcp24_svc      1,101 registers   max_registers:  600

(A first draft of this list also named `zhao_texture_cache_pipe`, whose 11,328
registers against a `max_registers: 2000` looked like the worst case of all. It
was wrong: that block's gates were deliberately REMOVED earlier the same day and
survive only as quoted text inside the comment explaining the removal. See the
hazard section below -- the file that documents a retraction is not a file
stating a rule.)

The evaluator is not broken. `zhao_texture_fragrob` has the same
`max_registers: 2500` and failed it correctly at 2,631. What happened is duller
and worse: **the rules were written AFTER those blocks were last fitted.**

    zhao_raster_perspuv_svc   row's sources 2026-09-03 11:47
                              its rule      2026-09-04 07:36   (+20 hours)
    zhao_raster_texjoin_v2    row's sources 2026-09-03 04:31
                              its rule      2026-09-03 17:29   (+13 hours)

A rule declared after the last measurement has never run. It sits in
`fit_targets.yml` looking like enforcement, and the row it governs says `ok`,
and there is nothing in either file to tell the two apart from a rule that was
evaluated and passed.

This is the same failure the repository has already recorded twice -- the Stop
hook that bypassed itself and fired exactly once (CLAUDE.md), and a port-coverage
self-check written with a `\\b` that became a literal backspace so it matched
nothing and printed "no silent drops". **A check that has never fired is worse
than no check, because it reassures.** The pattern is common enough to deserve
a tool rather than a third anecdote.

A HAZARD THIS TOOL AVOIDS, HAVING FIRST FALLEN INTO IT
-------------------------------------------------------
`fit_targets.yml` documents REMOVED rules in comments. `zhao_texture_cache_pipe`
carries a block reading *"NO RESOURCE GATES, AND THAT IS A CORRECTION"* which
then quotes the four rules it used to have and explains that each belonged to a
different block. A loose scrape for `max_registers:\s*(\d+)` reads those quoted
values as live rules and reports the block as breaching gates it does not have
-- which is exactly the wrong conclusion, since somebody had already done the
work of finding out they were misattributed.

So rules are matched only as a WHOLE LINE (`^\s+key: <digits>$`), which a
comment cannot satisfy. If a rule is ever written inline with a trailing comma
or a same-line comment it will be missed; that is the safer direction here,
because a missed rule under-reports staleness while a scraped comment invents
a violation.

WHAT IT DOES
------------
For every fit target with resource rules, it compares the rule's commit date
against the commit the recorded row was measured from, and reports:

    STALE     the rule is NEWER than the measurement -- never evaluated
    HIDDEN    `status` says ok but `lastAttemptStatus` says otherwise, so the
              displayed verdict is the last GOOD run rather than the last run
    BREACHED  the recorded numbers violate a rule while the row still says ok
              (which STALE explains, but is worth stating in resource terms)

It REPORTS. It does not gate, it does not edit `fit_targets.yml`, and it never
launches a fit -- the queue polls that file and a non-atomic rewrite of it has
already broken a run once.
"""
from __future__ import annotations

import io
import json
import re
import subprocess
import sys

FIT_TARGETS = "design/fit_targets.yml"
RESULTS = "reports/synthesis/zhao_block_fit.json"

RULE_KEYS = ("max_registers", "min_m10k", "max_alm", "max_alms", "max_dsp")


def read(p):
    return io.open(p, encoding="utf-8", errors="replace").read()


def commit_date(sha):
    """ISO date of a commit, or None if git cannot resolve it."""
    if not sha:
        return None
    r = subprocess.run(["git", "log", "-1", "--format=%cI", sha],
                       capture_output=True, text=True)
    out = r.stdout.strip()
    return out or None


def rule_commit_date(rule_key, value, target):
    """When did this rule's text last change in fit_targets.yml?

    `git log -S` on the exact `key: value` pair. It is a heuristic -- the same
    pair can appear on several targets -- so the answer is the most recent
    commit that touched that text at all, which is the CONSERVATIVE direction:
    it can only make a rule look newer, i.e. more likely to be reported stale.
    """
    needle = "%s: %s" % (rule_key, value)
    r = subprocess.run(["git", "log", "-1", "--format=%cI", "-S", needle,
                        "--", FIT_TARGETS], capture_output=True, text=True)
    out = r.stdout.strip()
    return out or None


def targets_with_rules():
    s = read(FIT_TARGETS)
    out = {}
    for chunk in re.split(r"\n  - top: ", s)[1:]:
        mod = chunk.split("\n", 1)[0].strip()
        rules = {}
        for k in RULE_KEYS:
            m = re.search(r"^\s+%s:\s*(\d+)\s*$" % k, chunk, re.M)
            if m:
                rules[k] = int(m.group(1))
        if rules:
            out[mod] = rules
    return out


def rows():
    d = json.loads(read(RESULTS))
    rs = d if isinstance(d, list) else d.get("blocks", d)
    if isinstance(rs, dict):
        rs = list(rs.values())
    return {r.get("module"): r for r in rs if r.get("module")}


def breaches(rules, row):
    """Which declared rules do the RECORDED numbers violate?"""
    bad = []
    reg, ram = row.get("registers"), row.get("ramBlocks")
    alm, dsp = row.get("alms"), row.get("dspBlocks")
    if "max_registers" in rules and reg is not None and reg > rules["max_registers"]:
        bad.append("registers %d > %d" % (reg, rules["max_registers"]))
    if "min_m10k" in rules and ram is not None and ram < rules["min_m10k"]:
        bad.append("ramBlocks %d < %d" % (ram, rules["min_m10k"]))
    for k in ("max_alm", "max_alms"):
        if k in rules and alm is not None and alm > rules[k]:
            bad.append("alms %d > %d" % (alm, rules[k]))
    if "max_dsp" in rules and dsp is not None and dsp > rules["max_dsp"]:
        bad.append("dspBlocks %d > %d" % (dsp, rules["max_dsp"]))
    return bad


def main() -> int:
    tg, rw = targets_with_rules(), rows()
    stale, hidden, breached, unmeasured = [], [], [], []

    for mod, rules in sorted(tg.items()):
        row = rw.get(mod)
        if row is None:
            unmeasured.append((mod, rules))
            continue

        src = commit_date(row.get("sourceCommit"))
        for k, v in sorted(rules.items()):
            rd = rule_commit_date(k, v, mod)
            if src and rd and rd > src:
                stale.append((mod, "%s: %d" % (k, v), rd[:10], src[:10]))

        la, st = row.get("lastAttemptStatus"), row.get("status")
        if la and st and la != st:
            hidden.append((mod, st, la))

        b = breaches(rules, row)
        if b and st == "ok":
            breached.append((mod, b))

    print("rule freshness: %d target(s) carry resource rules; %d rule(s) are "
          "NEWER than the measurement that governs them, %d row(s) show a "
          "status that is not the latest attempt, %d row(s) breach a rule while "
          "reading ok, %d target(s) have rules but no recorded fit"
          % (len(tg), len(stale), len(hidden), len(breached), len(unmeasured)))

    if stale:
        print("\nNEVER EVALUATED -- the rule postdates the fit it governs, so it "
              "has not run once. It looks like enforcement and is not:")
        for mod, rule, rd, sd in stale:
            print("  %-30s %-22s rule %s > sources %s" % (mod, rule, rd, sd))

    if breached:
        print("\nBREACHED WHILE READING ok -- the recorded numbers violate a "
              "declared rule. Where the rule is also listed above, that is why:")
        for mod, b in breached:
            print("  %-30s %s" % (mod, "; ".join(b)))

    if hidden:
        print("\nSTATUS IS NOT THE LATEST ATTEMPT -- `status` holds the last "
              "GOOD run, so a block whose rules now fail still reads ok. Read "
              "`lastAttemptStatus` beside it, always:")
        for mod, st, la in hidden:
            print("  %-30s status=%-6s lastAttemptStatus=%s" % (mod, st, la))

    if unmeasured:
        print("\nRULES BUT NO RECORDED FIT (%d) -- nothing has measured these, "
              "so their rules are also unevaluated:" % len(unmeasured))
        for mod, rules in unmeasured:
            print("  %-30s %s" % (mod, rules))

    print("\nNOTE: rule dates come from `git log -S` on the exact `key: value` "
          "text, which several targets can share. That can only make a rule "
          "look NEWER than it is, so this over-reports staleness rather than "
          "under-reporting it. REPORTS; never gates, never edits "
          "fit_targets.yml, never launches a fit.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
