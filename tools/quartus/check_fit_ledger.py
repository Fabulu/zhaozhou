"""A fit row whose `status` disagrees with its own `ruleViolations`.

WHY THIS EXISTS
---------------
2026-09-08. `zhao_texture_material_combine_v1` carried:

    status          ok
    ruleViolations  ['ALM 1475 > allowed 800',
                     'registers 893 > allowed 500',
                     'fmax 36.28 < required 125']

One row in 114 -- and it was the row the M2 deletion trigger depended on. A
docket entry, a report and a recommendation all repeated "V1 is ok" because
`status` said so, while the same row listed three violations.

The row's own note says how it happened: the fitter and STA were run BY HAND
after the watchdog was killed so the fit could outlive its budget. The row was
assembled manually, so whatever normally derives `status` from the rules never
ran, and `ok` was left in place.

That is this repository's own law -- *a `status` field holding the last good run
reports `ok`* -- landing on the single row where it did the most damage. The
defect made the answer look BETTER, and nobody audits good news.

WHAT IT CHECKS
--------------
One invariant, deliberately: a row may not claim success while listing rule
violations. It does not re-derive the rules, re-run anything, or judge whether a
violation matters. It only refuses to let the ledger contradict itself.
"""

import io
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LEDGER = os.path.join(ROOT, "reports", "synthesis", "zhao_block_fit.json")


def rows_of(doc):
    if isinstance(doc, list):
        return doc
    for key in ("blocks", "results"):
        if isinstance(doc.get(key), list):
            return doc[key]
    return []


def inconsistent(rows):
    """[(module, status, n_violations)] for rows claiming ok while violating."""
    out = []
    for r in rows:
        status = str(r.get("status", ""))
        viols = r.get("ruleViolations") or []
        if viols and status.startswith("ok"):
            out.append((r.get("module", "?"), status, len(viols)))
    return out


# A KNOWN-BAD LEDGER, checked on every run. A detector that has not been shown
# to fire has not been tested, and this one's whole job is to report "no
# contradictions".
_FIRE = [
    {"module": "clean_pass", "status": "ok", "ruleViolations": []},
    {"module": "honest_fail", "status": "failed:structure",
     "ruleViolations": ["ALM 9 > allowed 8"]},
    {"module": "the_bad_one", "status": "ok",
     "ruleViolations": ["ALM 1475 > allowed 800"]},
]


def self_fire_test():
    bad = inconsistent(_FIRE)
    # Exactly the contradictory row, and NOT the honest failure beside it: a
    # rule that flagged both would fire while being useless.
    return len(bad) == 1 and bad[0][0] == "the_bad_one"


def main(argv):
    if not self_fire_test():
        print("FIT-LEDGER CHECK BROKEN: it no longer isolates a row that claims "
              "success while listing violations, or has started flagging honest "
              "failures. Refusing to report a pass.")
        return 2

    if not os.path.exists(LEDGER):
        print("no ledger at %s" % LEDGER)
        return 2

    rows = rows_of(json.load(io.open(LEDGER, encoding="utf-8")))
    if len(rows) < 10:
        print("FIT-LEDGER CHECK VACUOUS: parsed %d rows. A clean result from a "
              "parser that found almost nothing is not a clean result."
              % len(rows))
        return 2

    bad = inconsistent(rows)
    print("fit-ledger consistency: %d rows checked" % len(rows))
    if bad:
        print("ROWS CLAIMING SUCCESS WHILE LISTING VIOLATIONS: %d" % len(bad))
        for module, status, n in bad:
            print("  - %s: status=%s but %d ruleViolation(s)" % (module, status, n))
        print("A row like this has already been quoted as evidence in a docket "
              "entry and a recommendation. Fix the row or re-run the fit; do not "
              "read past it.")
        return 1
    print("no row claims success while listing rule violations")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
