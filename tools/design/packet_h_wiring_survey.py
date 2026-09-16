#!/usr/bin/env python3
r"""packet_h_wiring_survey.py -- can zhao_shell_top_v2 actually be wired today?

WHY THIS EXISTS
---------------
Packet H owns `fpga/rtl/common/zhao_shell_top_v2.sv`, and that file does not
exist. Its 45 green `packet-h` ctests cover the four organs it composes, not the
composition -- the last of them is named `packet_h_shell_prereqs_registration_
static`, so the label was always honest and the roadmap prose reading it was not.

Before writing a 22-instance top, the question worth answering is the cheap one:
**does every port of the new organs have something that can drive or consume
it?** Packet H's gate says "every new port is connected", and discovering a port
with no counterpart halfway through writing the file is the expensive way to
learn it.

WHAT IT IS AND IS NOT
---------------------
It is a SURVEY, not a checker, and it is NOT registered as a ctest. It matches
by NAME, which is a heuristic: `frame_fault_o` on one block and
`frame_fault_i` on another are almost certainly the same wire, and two ports
called `busy_o` almost certainly are not. So its output is a work list for a
person, sorted so the interesting rows are at the top, and it never fails.

It reports three classes:

  PAIRED     -- an `_o` on one side and an `_i` of the same stem on the other.
                These are the wires the composition just connects.
  SHELL      -- the stem exists in the historical `zhao_shell_top`, so the
                sibling has an obvious source for it.
  UNMATCHED  -- nothing found. Either a genuinely new top-level port, or a wire
                whose two ends are spelled differently, or the thing Packet H
                has to invent. **This is the column to read.**

The heuristic's own blind spot, stated: a stem that appears on BOTH sides for
unrelated reasons (clk, rst_n, busy) is reported paired and means nothing. Those
are listed separately as TRIVIAL rather than padding the paired count.

**UNMATCHED IS A CEILING, NOT A COUNT**, and the difference is large. This tree
RENAMES ACROSS BOUNDARIES on purpose -- `zhao_shell_top` wires
`.tri_kx0_i(render_kx0_i)`, and the same practice hides most of
`zhao_engine1_raw_last_v2`'s eighteen "unmatched" ports behind different
spellings: `verdict_ok_i`, `raw16_valid_i` and `guard_accept_i` have no stem
match anywhere, yet `verdict`, `raw16` and `engine1` all appear in
`zhao_shell_top`, `zhao_mem_guard`, `zhao_vram_arbiter`,
`zhao_render_asset_mux` and `zhao_geom_mem_adapter`. The producers exist; only
the names differ.

So read UNMATCHED as "ports whose far end this tool could not NAME", which is an
upper bound on the wiring Packet H has to invent and a lower bound on nothing.
A row leaving that column is good news; a row in it is a question.
"""

from __future__ import annotations

import io
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RTL = os.path.join(REPO, "fpga", "rtl")

ORGANS = (
    "zhao_engine1_raw_last_v2",
    "zhao_renderer_lease_v2",
    "zhao_video_ready_bridge_v2",
    "zhao_video_terminal_adapter_v2",
)
# The V2 blocks the sibling swaps in for their historical counterparts.
SWAPS = {
    "zhao_geom_bin_pipe_v2": "zhao_geom_bin_pipe",
    "zhao_video_slotmgr_v2": "zhao_video_slotmgr",
}
SHELL = "zhao_shell_top"

PORT_RE = re.compile(
    r"^\s*(input|output|inout)\s+(?:wire\s+|logic\s+|reg\s+)?"
    r"(?:signed\s+)?(?:\[[^\]]*\]\s*)?(\w+)\s*[,)]?\s*(?://.*)?$", re.M)

TRIVIAL = {"clk", "rst_n", "reset", "rst", "clock", "en", "busy", "valid", "ready"}


def _stem_hit(stem_name: str, text: str) -> bool:
    return bool(re.search(r"\b%s(_[io])?\b" % re.escape(stem_name), text))


# THE POSITIVE CONTROL, at import, on the exact shape that broke it. Without the
# optional suffix group this assert fails, which is the whole point: the first
# version of this tool reported 285 of ~300 ports unwireable and printed it
# without hesitating.
assert _stem_hit("tri_kx0", ".tri_kx0_i(render_kx0_i),"), (
    "packet_h_wiring_survey: the stem matcher cannot see a port with an _i suffix")
assert not _stem_hit("tri_kx0", ".tri_kx01_i(x),"), (
    "packet_h_wiring_survey: the stem matcher matches a LONGER name, so every "
    "report would be a false pairing")


def find(module: str) -> str | None:
    for root, _dirs, names in os.walk(RTL):
        if module + ".sv" in names:
            return os.path.join(root, module + ".sv")
    return None


def ports(path: str) -> list[tuple[str, str]]:
    """(direction, name) for the module header's ports, header only."""
    text = io.open(path, encoding="utf-8", errors="replace").read()
    start = text.find("module ")
    if start < 0:
        return []
    end = text.find(");", start)
    header = text[start:end] if end > 0 else text[start:]
    return [(m.group(1), m.group(2)) for m in PORT_RE.finditer(header)]


def stem(name: str) -> str:
    if name.endswith("_i") or name.endswith("_o"):
        return name[:-2]
    return name


def main() -> int:
    organ_ports: dict[str, list[tuple[str, str]]] = {}
    missing = []
    for m in list(ORGANS) + list(SWAPS):
        p = find(m)
        if p is None:
            missing.append(m)
            continue
        organ_ports[m] = ports(p)

    shell_path = find(SHELL)
    shell_stems = {stem(n) for _d, n in ports(shell_path)} if shell_path else set()
    # Everything the historical shell declares internally is fair game as a
    # source too -- it is the body the sibling is a variant of.
    shell_text = (io.open(shell_path, encoding="utf-8", errors="replace").read()
                  if shell_path else "")

    print("PACKET-H WIRING SURVEY -- a work list, not a gate")
    if missing:
        print("  MISSING MODULES (cannot survey): %s" % ", ".join(missing))
    print("  organs surveyed: %d" % len(organ_ports))

    # every stem offered by any organ, with its direction(s)
    offered: dict[str, set[str]] = {}
    for m, pl in organ_ports.items():
        for d, n in pl:
            offered.setdefault(stem(n), set()).add(d)

    rows = {"PAIRED": [], "SHELL": [], "TRIVIAL": [], "UNMATCHED": []}
    for m in sorted(organ_ports):
        for d, n in organ_ports[m]:
            s = stem(n)
            if s in TRIVIAL or n in TRIVIAL:
                rows["TRIVIAL"].append((m, d, n))
            elif len(offered.get(s, set())) > 1:
                rows["PAIRED"].append((m, d, n))
            # `\b%s\b` HERE MATCHED NOTHING, and reported 285 of ~300 ports
            # unwireable. The stem `tri_kx0` appears in the shell as
            # `.tri_kx0_i(render_kx0_i)`, and `_` IS A WORD CHARACTER, so the
            # trailing `\b` can never fire against the `_i` suffix every port in
            # this tree carries. CLAUDE.md files this exact class -- a
            # word-boundary assumption that makes a pattern match nothing while
            # the tool keeps printing confidently.
            #
            # The optional suffix group is the fix, and the assert below is the
            # positive control that keeps it honest.
            elif s in shell_stems or re.search(
                    r"\b%s(_[io])?\b" % re.escape(s), shell_text):
                rows["SHELL"].append((m, d, n))
            else:
                rows["UNMATCHED"].append((m, d, n))

    for key in ("UNMATCHED", "SHELL", "PAIRED", "TRIVIAL"):
        print("\n== %s (%d) ==" % (key, len(rows[key])))
        if key == "UNMATCHED":
            print("  Nothing in the other organs or in the historical shell offers")
            print("  this stem. Each is a new top-level port, a spelling mismatch,")
            print("  or something Packet H has to invent. READ THIS COLUMN.")
        for m, d, n in rows[key][:40 if key == "UNMATCHED" else 12]:
            print("  %-34s %-6s %s" % (m, d, n))
        if len(rows[key]) > (40 if key == "UNMATCHED" else 12):
            print("  ... and %d more" % (len(rows[key]) - (40 if key == "UNMATCHED" else 12)))

    print("\nNOTE: matching is by NAME and is a heuristic. A paired stem is a")
    print("likely wire, not a proved one, and an UNMATCHED row may simply be")
    print("spelled differently at its far end. REPORTS; never gates.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
