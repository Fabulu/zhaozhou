# The comparison-side checks

`npm run design:report` runs all five. They **report and never gate**, and that
is deliberate in both halves.

    check_port_coverage.py    output ports no test names, and fault reporters first
    check_counters.py         does the ledger's `counters:` list name real ports (D19c)
    check_array_storage.py    declared array bits that did not become memory (D19m)
    check_seam_widths.py      do the two ends of a declared seam agree on width
    check_rule_freshness.py   fit rules never evaluated; rows older than their RTL

## Why they report and do not gate

CLAUDE.md's rule is that measurement belongs on the comparison side: *"a tool
that says 'you are 20% off' is good; a tool that decides a radius is not."*
Every one of these is a heuristic with named false positives, listed in its own
docstring. A heuristic that blocks a commit trains people to silence it.

## Why they are NOT in CI, stated rather than left to be discovered

CI has no Python step (`.github/workflows/ci.yml` sets up node and the C++
toolchain, not `actions/setup-python`). Adding one is a real change and belongs
in its own pass, not smuggled in beside a report script.

**What must NOT happen is a CI step that skips when Python is absent.** This
repository has already paid for that: a gate written to SKIP-if-absent hid weeks
of drift while reporting success. An honest local-only script beats a
dishonest green tick.

## What they cost, measured 2026-09-04

`check_rule_freshness.py` shells out to `git log` twice per census row, so it
takes tens of seconds on 85 rows. The other four are seconds.

## Read the docstrings before believing a number

Every one of these tools was wrong at least once on the day it was written, and
each failure is recorded at the top of the file it happened in:

* the port scanner could not see width brackets, scoped types, unpacked arrays,
  a final port with no trailing comma, or `.sby` files — five separate blind
  spots, and **every one made the answer look better than it was**;
* its self-check was written with a `\b` that a shell heredoc turned into a
  literal backspace, so it matched nothing and printed *"no silent drops"* while
  17 ports were being dropped;
* the array scanner used `^` with no `re.MULTILINE` and reported a confident
  zero on a tree containing the 65,536-bit array it was written to find;
* the rule checker scraped a *comment that retracted* four rules and reported
  the block as breaching gates it does not have.

Three of the five now assert at import that their own pattern still matches a
known-good example. **A detector that has not been shown to fire has not been
tested.**
