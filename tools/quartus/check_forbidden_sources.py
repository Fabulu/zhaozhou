"""No measurement fixture may appear in a PRODUCTION fit closure.

V3.1 §22.9, verbatim:

    The owner skeleton's fake COMBINE and tokenized fake services must not
    appear in the feature-live fit closure. List them explicitly in a
    forbidden-source check for production measurements.

WHY THIS IS WORTH A TOOL RATHER THAN CARE
-----------------------------------------
A fixture in a production closure does not fail. It fits, it reports ALMs and an
Fmax, and every one of those numbers is wrong in the direction that flatters:
a probe wrapper is smaller than the thing it probes, a `pair` harness ties off
ports the real design drives, and a tokenized fake service answers instantly
where the real one stalls. The result reads as a healthy measurement of a
design that was never built.

That is this repository's own law -- a broken instrument lies in ONE direction,
and nobody audits good news -- applied to the SOURCE LIST instead of to the
parser.

WHAT COUNTS AS A FIXTURE
------------------------
Everything under `fpga/rtl/synth/`. That directory exists to hold
characterisation wrappers: `zhao_probe_*` (a DUT plus a registered hash sink,
so the fitter cannot delete it) and `zhao_pair_*` (two or three blocks composed
to measure a seam). None of it ships.

The check is deliberately a DIRECTORY rule rather than a name list. A name list
has to be updated by the person adding the next probe, which is the person least
likely to remember; a directory rule covers files that do not exist yet.
"""

import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TARGETS = os.path.join(ROOT, "design", "fit_targets.yml")

# Fixture roots. A path is forbidden in a production closure if it sits under
# one of these.
FIXTURE_DIRS = ("fpga/rtl/synth/",)

# Targets that are THEMSELVES fixtures are allowed to name fixture sources --
# that is what they are for. Identified by the top module's own name, so a new
# probe needs no edit here either.
FIXTURE_TOP = re.compile(r"^zhao_(probe|pair)_")


def parse_targets(text):
    """[(top, [sources])] from fit_targets.yml, without a YAML dependency."""
    out = []
    top = None
    sources = []
    in_sources = False
    for raw in text.split("\n"):
        line = raw.rstrip()
        stripped = line.strip()
        m = re.match(r"^-?\s*top:\s*(\S+)", stripped)
        if m:
            if top is not None:
                out.append((top, sources))
            top = m.group(1)
            sources = []
            in_sources = False
            continue
        if stripped.startswith("sources:"):
            in_sources = True
            continue
        if in_sources:
            m2 = re.match(r"^-\s*(\S+)", stripped)
            if m2:
                sources.append(m2.group(1))
                continue
            # any other key ends the source list
            if stripped and not stripped.startswith("#"):
                in_sources = False
    if top is not None:
        out.append((top, sources))
    return out


def check_text(text):
    """(errors, n_targets, n_fixture_targets) for one fit_targets.yml body.

    Separated from main() so the rule can be exercised on a synthetic input.
    A checker whose only input is the real file can never be shown to FIRE
    without editing the real file -- and `design/fit_targets.yml` is read LIVE
    by a running fit at preflight, so editing it to test a tool is a way to
    corrupt a 90-minute measurement.
    """
    targets = parse_targets(text)
    errors = []
    fixture_targets = 0
    for top, sources in targets:
        if FIXTURE_TOP.match(top):
            fixture_targets += 1
            continue
        for src in sources:
            norm = src.replace("\\", "/")
            for d in FIXTURE_DIRS:
                if d in norm:
                    errors.append(
                        "PRODUCTION target '%s' names the measurement fixture "
                        "'%s' -- a fixture in a production closure produces a "
                        "clean number for a design that was never built"
                        % (top, src)
                    )
    return errors, targets, fixture_targets


# A KNOWN-BAD INPUT, checked on every run.
#
# This repository has three other tools that assert at import that their own
# pattern still matches a known-good example, because one self-check was written
# with an escape that a shell heredoc turned into a literal control byte -- so it
# matched nothing and printed reassurance for its whole life. This is that habit
# applied to a rule instead of a regex: the detector proves it can FIRE before it
# is allowed to report a pass.
#
# It also proves the rule DISCRIMINATES. The probe target below names the very
# same file and must NOT be flagged, so a lazy "does this path appear anywhere"
# rule would fail this test rather than pass it.
_FIRE = """
  - top: zhao_texture_island_top
    sources:
      - fpga/rtl/texture/zhao_texture_island_top.sv
      - fpga/rtl/synth/zhao_probe_v3rq_queue.sv

  - top: zhao_probe_v3rq_queue
    sources:
      - fpga/rtl/synth/zhao_probe_v3rq_queue.sv
      - fpga/rtl/texture/zhao_texture_v3rq.sv
"""


def self_fire_test():
    """True if the rule still catches a fixture in a production closure."""
    errors, targets, fixture = check_text(_FIRE)
    return len(errors) == 1 and fixture == 1 and "island_top" in errors[0]


def main(argv):
    text = io.open(TARGETS, encoding="utf-8").read()
    targets = parse_targets(text)

    if not self_fire_test():
        print("FORBIDDEN-SOURCE CHECK BROKEN: the rule no longer fires on a "
              "known-bad closure, or now flags a legitimate fixture target. "
              "Refusing to report a pass from a detector that cannot fail.")
        return 2

    # SELF-CHECK, because a parser that silently matches nothing reports a
    # clean audit. If this file stops yielding targets with sources, the check
    # below is vacuous and must say so rather than print a pass.
    with_sources = [t for t in targets if t[1]]
    if len(with_sources) < 5:
        print("FORBIDDEN-SOURCE CHECK BROKEN: parsed %d targets, %d with "
              "sources -- refusing to report a pass from a parser that found "
              "nothing" % (len(targets), len(with_sources)))
        return 2

    errors, targets, fixture_targets = check_text(text)

    print("forbidden-source check: %d targets, %d production, %d fixture"
          % (len(targets), len(targets) - fixture_targets, fixture_targets))
    if errors:
        print("FORBIDDEN-SOURCE CHECK FAILED -- %d error(s)" % len(errors))
        for e in errors:
            print("  - " + e)
        return 1
    print("forbidden-source check OK -- no production closure names a fixture")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
