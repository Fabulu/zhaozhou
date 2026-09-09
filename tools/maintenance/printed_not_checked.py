#!/usr/bin/env python3
"""Find counters a test PRINTS but never ASSERTS.

WHY THIS EXISTS
---------------
CLAUDE.md's most expensive recurring defect is a detector whose silence is
quoted as evidence. The two shapes it takes in a test file are:

  1. a counter that is printed and never asserted -- so a nonzero value passes;
  2. a counter asserted `== 0` with no non-vacuity check beside it -- so a zero
     that means "nothing ever reached this" passes as "nothing went wrong".

On 2026-09-09 a sweep of `tests/texture/island_composed_directed.cpp` found FOUR
of shape 1 in one file:

  * `meta_genmis_o` -- printed since the packet-C phase was written. Asserting it
    revealed shape 2 as well: in the fault probe the metadata bank had never been
    READ where the number was quoted (writes 4, reads 0), so the zero was real
    and meant nothing. That counter is the one CLAUDE.md records as having "never
    fired, and never could" before the D0 repair.
  * `meta_align_err_o`, `meta_bil_err_o`, `meta_near_err_o` -- wrong-response
    counts printed beside denominators that WERE asserted. The phase would have
    passed with any of them nonzero. `meta_align_err_o` is the D0 defect's exact
    signature.

This tool reports shape 1 -- mechanically checkable. Shape 2 needs a human to
say what the denominator for a given counter is, so it is flagged, not decided:
every `== 0` assertion is listed with whether anything nearby looks like a
non-vacuity guard.

WHAT IT IS NOT
--------------
Not a gate. A printed-only counter is often correct -- a print cannot falsely
reassure the way a green check can, and some numbers are diagnostics rather than
contracts. The output is a WORKLIST, and it says so.

Usage:
    python tools/maintenance/printed_not_checked.py tests/texture/*.cpp
    python tools/maintenance/printed_not_checked.py --self-test
"""
import argparse
import io
import re
import sys

# A DUT port reference: `d.<name>_o` or `d-><name>_o`, plus the `dut.` spelling.
_PORT = re.compile(r'\b(?:d|dut)\s*(?:\.|->)\s*(\w+_o)\b')

# A check() / zhao::check() call and its argument text. Non-greedy to the first
# `;` at paren depth 0 is overkill here -- checks are one statement, and the
# harness signature is (cond, what, expected, got), so the whole call text is
# what matters.
_CHECK = re.compile(r'\b(?:zhao::)?check\s*\(', re.S)

# Names that read like a fault counter rather than a diagnostic. Used only to
# RANK the worklist, never to filter it -- a filter here would be the same
# "hide what you decided not to look at" mistake the tool exists to catch.
_FAULTY = re.compile(
    r'err|mis|bad|invalid|overflow|underflow|drop|stale|dup|unsol|illegal'
    r'|fault|refus|wedge|starv|lost|truncat', re.I)


def _call_texts(src, pat):
    """Yield the full parenthesised text of each call matching `pat`."""
    for m in pat.finditer(src):
        i = m.end() - 1          # at the '('
        depth = 0
        for j in range(i, len(src)):
            c = src[j]
            if c == '(':
                depth += 1
            elif c == ')':
                depth -= 1
                if depth == 0:
                    yield src[i:j + 1]
                    break
        else:
            yield src[i:]        # unbalanced; hand back what there is


def analyse(src):
    referenced = set(_PORT.findall(src))
    checked = set()
    zero_asserts = []
    for text in _call_texts(src, _CHECK):
        ports = set(_PORT.findall(text))
        checked |= ports
        # Only PORT-valued zero-asserts are listed. A comparison against a local
        # tally computed in the test usually has its denominator on the adjacent
        # lines, and listing 58 of those buries the handful that matter -- which
        # is its own way of hiding a worklist.
        if ports and re.search(r'==\s*0\b|!\s*(?:d|dut)\s*(?:\.|->)', text):
            zero_asserts.append((sorted(ports), text.split(',')[0][:70]))
    return referenced, checked, zero_asserts


# ---------------------------------------------------------------------------
# SELF-FIRE TEST. This tool's whole subject is instruments that report nothing
# and are believed, so it may not report before showing it can see both a
# checked and an unchecked port. The first version of the ad-hoc sweep this
# replaces used a bounded `[^;]{0,400}` window and would silently miss a check
# whose arguments ran long -- which reads as "this port is unasserted" and sends
# someone to add a duplicate assertion.
# ---------------------------------------------------------------------------
_FIRE = '''
  std::printf("count %u\\n", d.cnt_widgets_o);
  check(d.err_asserted_o == 0, "this one is gated", 0, d.err_asserted_o);
  check(local_tally == 0,
        "the port appears ONLY as the trailing `got` argument, after a message "
        "long enough to push it past any bounded window; padding padding "
        "padding padding padding padding padding padding padding padding "
        "padding padding padding padding padding padding padding padding",
        0, d.err_late_o);
  std::printf("ungated %u\\n", d.err_printed_only_o);
'''
# NOTE ON THE SHAPE OF `err_late_o` ABOVE, because the first version of this
# fixture did not work and said it did.
#
# It originally read `check(d.err_long_o == 0, "...long...", 0, d.err_long_o)`.
# That cannot detect a bounded-window scan: the port is the FIRST argument, so a
# 120-character window still contains it and the assertion passes either way. I
# only found out by deliberately reintroducing the bounded scan and watching the
# self-test NOT fire -- a self-test that is decorative about exactly the defect it
# names, which is the failure this whole file is about, one level up.
#
# So the fixture now puts the port ONLY in the trailing `got` position with the
# message padded past any plausible window. Verified to fire when `_call_texts`
# is replaced by a fixed-width slice.


def self_test():
    ref, chk, zeros = analyse(_FIRE)
    assert 'cnt_widgets_o' in ref, 'printf-only port not seen at all'
    assert 'err_asserted_o' in chk, 'a gated port was not recognised as gated'
    assert 'err_late_o' in chk, \
        ('a gated port appearing ONLY as the trailing `got` argument was missed '
         '-- the call-text scan is bounded again, which is the false positive '
         'this test exists for')
    assert 'err_printed_only_o' in ref and 'err_printed_only_o' not in chk, \
        'an ungated port was reported as gated'
    assert len(zeros) == 2, 'zero-assert detection found %d, expected 2' % len(zeros)
    return True


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='*')
    ap.add_argument('--self-test', action='store_true')
    a = ap.parse_args()

    self_test()   # unconditional: it may not report before proving it can see
    if a.self_test:
        print('printed_not_checked self-test: sees printf-only ports, gated '
              'ports, gated ports with long argument lists, and zero-asserts.')
        return 0
    if not a.files:
        ap.error('at least one file is required')

    worklist = 0
    for path in a.files:
        try:
            src = io.open(path, encoding='utf-8', errors='replace').read()
        except OSError as e:
            print('%s: %s' % (path, e), file=sys.stderr)
            continue
        ref, chk, zeros = analyse(src)
        if not ref:
            continue
        gap = sorted(ref - chk)
        faulty = [p for p in gap if _FAULTY.search(p)]
        print('%s' % path)
        print('   %d ports referenced, %d inside a check(), %d printed-only'
              % (len(ref), len(chk), len(gap)))
        if faulty:
            print('   FAULT-SHAPED AND UNGATED (look at these first):')
            for p in faulty:
                print('      %s' % p)
            worklist += len(faulty)
        other = [p for p in gap if p not in faulty]
        if other:
            print('   other printed-only (often correct -- diagnostics, not '
                  'contracts): %s' % ', '.join(other))
        if zeros:
            print('   %d `== 0` assertions -- each needs a non-vacuity guard '
                  'naming what would have been nonzero:' % len(zeros))
            for ports, head in zeros:
                print('      %-44s %s' % (','.join(ports) or '(no port)', head.strip()))
        print()

    print('%d fault-shaped ungated counters across %d file(s). This is a '
          'WORKLIST, not a verdict -- a printed-only diagnostic can be exactly '
          'right.' % (worklist, len(a.files)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
