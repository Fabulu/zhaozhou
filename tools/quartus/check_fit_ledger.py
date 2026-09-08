"""ONE INVARIANT ONLY: a row may not claim success while LISTING violations.

MOST OF WHAT THIS FILE ONCE DID WAS ALREADY DONE, BETTER, BY
`tools/quartus/check_fit_rules.ps1`.

That script recomputes every verdict from the recorded numbers against
`design/fit_targets.yml`, so it does not consult the `status` field at all --
and it additionally marks rows whose source has changed since the fit. Its
output today: **10 pass, 17 FAIL, 3 unmeasured, 9 STALE**.

I wrote a numeric comparison here without looking for it, and produced a worse
version of a tool the repository already had. `fit_rules.ps1` names it in its
own header -- "check_fit_rules.ps1 applies the identical law to already-recorded
rows" -- which is one grep away from the file I was editing.

WHAT IS LEFT HERE, and why it is not covered there:

A row that carries a non-empty `ruleViolations` list while its `status` says
`ok`. `check_fit_rules.ps1` recomputes and would flag such a row on the numbers
-- but it would not notice that the ROW ITSELF is self-contradictory, which is a
sign the row was hand-assembled. `zhao_texture_material_combine_v1` was exactly
that: status `ok`, three violations listed, written by hand after a killed
watchdog. One row in 114.

Use `check_fit_rules.ps1` for verdicts. Use this only for that contradiction.
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


def rules_by_top(path):
    """{top: [rule names]} from fit_targets.yml, without a YAML dependency."""
    import re
    out, top, inr = {}, None, False
    try:
        text = io.open(path, encoding="utf-8").read()
    except OSError:
        return out
    for raw in text.split(chr(10)):
        st = raw.strip()
        m = re.match(r"^-?\s*top:\s*(\S+)", st)
        if m:
            top, inr = m.group(1), False
            continue
        if st.startswith("rules:"):
            inr = True
            continue
        if inr and top:
            m2 = re.match(r"^(max_\w+|min_\w+):\s*(\d+)", st)
            if m2:
                out.setdefault(top, {})[m2.group(1)] = int(m2.group(2))
            elif st and not st.startswith("#"):
                inr = False
    return out


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
