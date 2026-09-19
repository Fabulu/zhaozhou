#!/usr/bin/env python3
"""Which modules on the console's reset net use it SYNCHRONOUSLY, and which async.

WHY THIS EXISTS
---------------
Joining `zhao_console_board` to `zhao_console_core` on 2026-09-19 produced
exactly one lint diagnostic, and it was a real one:

    %Warning-SYNCASYNCNET: zhao_console_board.sv:442: Signal flopped as both
                           synchronous and async: 'rst_n_gpu_o'

It could not have appeared before. Linting `zhao_console_core` alone is SILENT
at RC 0 -- there `rst_n` is a top-level input and Verilator cannot see its
driver -- so the warning is a property of the JOIN, not of either file.

The board waives it, with a reason, and the reason quotes a number. CLAUDE.md:
*"a probe that does this was written once and thrown away, so its numbers are
unreproducible -- commit the probe."* This is that probe. Anyone can re-run it
and check the waiver still describes the tree.

WHAT IT MEASURES, AND WHAT IT DOES NOT
--------------------------------------
It reads the console's OWN fit closure from `design/fit_targets.yml` -- the
same list `run_console_core_smoke.ps1` and `run_console_board_lint.ps1` read,
so there is no second copy of a source list -- and classifies every `always_ff`
in it:

  ASYNC  the sensitivity list carries a reset edge (`negedge rst_n`)
  SYNC   it does not, and the body tests the reset as data (`if (!rst_n)`)

It is a TEXT census and it says so. It cannot tell which reset PORT a
submodule's `rst_n` is ultimately driven from -- for that, Verilator's
elaborated view is the authority and it is what raises the warning in the first
place. This tool answers the narrower question the waiver needs: how much of
the console is written in the synchronous idiom, so that "a 19-module edit" is
a measurement rather than an impression.

It is an INSTRUMENT, never a gate. It prints and exits 0 (exit 2 only if it
cannot read the closure, because a census over nothing that reports "0
synchronous users" is the broken-instrument failure -- precise at zero, and
flattering).
"""

from __future__ import annotations

import argparse
import io
import os
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve()
ROOT = HERE.parents[2]
TARGETS = ROOT / "design" / "fit_targets.yml"

ALWAYS_FF = re.compile(r"always_ff\s*@\s*\(([^)]*)\)")
SYNC_TEST = re.compile(r"if\s*\(\s*[!~]\s*(\w*rst\w*)\s*\)")
RESET_EDGE = re.compile(r"(neg|pos)edge\s+\w*rst\w*", re.I)


def closure(top: str = "zhao_console_core") -> list[str]:
    if not TARGETS.exists():
        return []
    out, in_target = [], False
    for line in io.open(TARGETS, encoding="utf-8", errors="replace"):
        m = re.match(r"\s*- top:\s*(\S+)", line)
        if m:
            in_target = m.group(1) == top
            continue
        m = re.match(r"\s*- (fpga/.*\.sv)\s*$", line)
        if in_target and m:
            out.append(m.group(1))
    return out


def strip_line_comments(text: str) -> str:
    """Line comments only, and the length is not preserved.

    Block comments are deliberately left: this tool only searches, never
    slices, so a `/* ... */` that quotes an `always_ff` would have to quote a
    reset test inside it as well to produce a false hit, and that has not
    happened in this tree. Said out loud rather than left as a silent
    approximation.
    """
    return re.sub(r"//[^\n]*", "", text)


def census(files: list[str], body_window: int = 2500) -> dict:
    sync: dict[str, int] = {}
    asyn: dict[str, int] = {}
    for rel in files:
        path = ROOT / rel
        if not path.exists():
            continue
        text = strip_line_comments(
            io.open(path, encoding="utf-8", errors="replace").read())
        for m in ALWAYS_FF.finditer(text):
            sens = m.group(1)
            if RESET_EDGE.search(sens):
                asyn[rel] = asyn.get(rel, 0) + 1
                continue
            if SYNC_TEST.search(text[m.end():m.end() + body_window]):
                sync[rel] = sync.get(rel, 0) + 1
    return {"sync": sync, "async": asyn}


def _self_test() -> None:
    """The classifier must be seen to put each idiom in the right bucket.

    A census whose pattern matches nothing reports zero synchronous users,
    which is the reassuring direction and therefore the one to prove against a
    hand-checkable example.
    """
    assert RESET_EDGE.search("posedge clk or negedge rst_n")
    assert not RESET_EDGE.search("posedge clk")
    assert SYNC_TEST.search("begin if (!rst_n) q <= '0; else q <= d; end")
    assert SYNC_TEST.search("if (~rst_n) q <= '0;")
    # A clock-enable test must NOT be read as a reset test, or every gated flop
    # in the tree joins the count and the number stops meaning anything.
    assert not SYNC_TEST.search("if (!valid_i) q <= '0;")
    assert not SYNC_TEST.search("if (en) q <= d;")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--top", default="zhao_console_core")
    ap.add_argument("--quiet", action="store_true", help="totals only")
    args = ap.parse_args(argv)

    _self_test()
    files = closure(args.top)
    if len(files) < 50:
        print("reset_discipline_census: %s's closure came back with %d sources. "
              "Refusing to report a census over a list that small -- a zero "
              "here would read as 'the console is uniformly asynchronous'."
              % (args.top, len(files)), file=sys.stderr)
        return 2

    c = census(files)
    nsync_mod, nsync_blk = len(c["sync"]), sum(c["sync"].values())
    nasy_mod, nasy_blk = len(c["async"]), sum(c["async"].values())

    print("reset discipline over %s's fit closure (%d sources)"
          % (args.top, len(files)))
    print("  SYNCHRONOUS reset users : %3d module(s), %4d always_ff block(s)"
          % (nsync_mod, nsync_blk))
    print("  ASYNCHRONOUS reset users: %3d module(s), %4d always_ff block(s)"
          % (nasy_mod, nasy_blk))
    if not args.quiet and c["sync"]:
        print()
        print("  the synchronous ones -- these are what a discipline change "
              "would have to edit:")
        for rel in sorted(c["sync"], key=lambda r: (-c["sync"][r], r)):
            print("    %-62s %3d block(s)" % (rel, c["sync"][rel]))
    print()
    print("Mixed discipline on one net is SAFE exactly when the release is "
          "synchronised per domain and held long enough for every synchronous "
          "user to see it. `zhao_sys_reset` is what makes that true here "
          "(SYNC_STAGES into each domain, RELEASE_SPAN reference cycles of "
          "stagger), and it is why zhao_console_board waives SYNCASYNCNET "
          "rather than repairing 19 modules from the outside.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
