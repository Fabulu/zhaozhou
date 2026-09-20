"""duplicate_functions -- the same arithmetic, written out in N separate modules.

`uncashed_cheques.py` check 3 finds duplication that is DECLARED: two blocks
naming the same `reference_model` in `design/blocks.yml`. That check found the
projector's two cores. It cannot see this one, because nobody declared anything:

    fpga/rtl/compositor/zhao_post_composite.sv        function unit_mul
    fpga/rtl/raster/zhao_raster_fragment.sv           function unit_mul
    fpga/rtl/texture/zhao_texture_combine.sv          function unit_mul
    fpga/rtl/compositor/zhao_post_composite.sv        function unit_lerp
    fpga/rtl/texture/zhao_texture_material_combine_v1.sv  function unit_lerp

`POST.COMPOSITE.md` and `TWOD.SPRITE.md` BOTH say they "reuse the frozen blend
law". There is no shared RTL function to reuse -- so each module wrote it again.
This is the exact shape CLAUDE.md's projector chapter describes, one level down:
the duplication is in SystemVerilog functions rather than in modules, so no
module-graph tool and no ledger tool looks at it.

Why it is an AREA question and not a tidiness one: plan section 14.4 ranks
"eliminate duplicated ownership/engines by construction" as area-preservation
priority #1. A function inlined into five parents is five copies of its logic
unless synthesis happens to share them, and whether it does is not something to
assume -- the combiner chapter in CLAUDE.md is about exactly that assumption
failing (fourteen multipliers where the architecture said two).

What this tool does NOT claim: that every duplicate is waste. A one-line helper
may genuinely belong in each module, and a shared function still costs a package
dependency. It reports WHERE THE SAME NAME IS DEFINED MORE THAN ONCE and leaves
the judgement where it belongs. It also reports whether the name is already in
`zhao_pkg.sv`, because a duplicate of something that IS shared is a different and
worse finding than a duplicate of something nobody centralised.

Reads LOW when broken -- a regex that matches no function definitions reports
"no duplication" -- so it refuses to run unless it first finds a known-good
example. See CLAUDE.md, "A broken instrument lies in ONE direction".
"""

from __future__ import annotations

import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
RTL = ROOT / "fpga" / "rtl"
PKG = RTL / "common" / "zhao_pkg.sv"

# A SystemVerilog function header. The return type is optional and may itself
# carry a packed dimension, which is why this is not simply \w+\s+\w+.
#
# IT USED TO ALLOW **ONE** WORD BEFORE THE RANGE, and that made this tool blind
# to `function automatic logic signed [31:0] f(...)` -- two words. Measured on
# 2026-09-20 (owner ruling R173): of **524** function headers under fpga/rtl it
# matched **380**, missing **125**, and **196** headers carry `signed`. So the
# tool that exists to find DUPLICATED FUNCTIONS could not see a quarter of the
# tree's functions -- specifically the arithmetic ones, which are exactly where
# duplication costs DSPs and ALMs.
#
# It is the flattering direction, as always: fewer functions seen is fewer
# duplicates reported.
#
# The repetition is BOUNDED at three words rather than written `*`, because an
# unbounded alternation here backtracks catastrophically on long headers -- the
# first attempt at this fix ran for over two minutes on the same corpus this
# version scans in seconds.
FUNC = re.compile(
    r"^\s*function\s+(?:automatic\s+)?"      # function [automatic]
    r"(?:[A-Za-z_]\w*\s+){0,3}"              # up to 3 type words: logic signed X
    r"(?:\[[^\]]*\]\s*)?"                    # optional packed range
    r"([A-Za-z_]\w*)\s*(?:\(|;)",            # the NAME
    re.MULTILINE,
)

# Names that MUST be found, or the pattern has rotted and every "no duplicates"
# below would be a false negative.
#
# `unit_mul` ALONE WAS NOT ENOUGH, and that is the whole lesson of R173. It is
# declared `function automatic logic [7:0] unit_mul(...)` -- **UNSIGNED**, one
# type word -- so it resolved happily under the broken pattern and the
# self-check passed over the blind spot it was supposed to guard. **A canary
# that cannot enter the failing state is not a canary.**
#
# `mw` is `function automatic logic signed [MATW-1:0] mw(...)`: two type words
# and a signed return. It is the case the old pattern could not see, so it is
# the one that has to be in here.
CANARY = "unit_mul"
SIGNED_CANARY = "mw"


def scan() -> dict[str, list[tuple[pathlib.Path, int]]]:
    found: dict[str, list[tuple[pathlib.Path, int]]] = collections.defaultdict(list)
    for p in sorted(RTL.rglob("*.sv")):
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for m in FUNC.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            found[m.group(1)].append((p, line))
    return found


def pkg_names() -> set[str]:
    if not PKG.exists():
        return set()
    text = PKG.read_text(encoding="utf-8", errors="replace")
    return {m.group(1) for m in FUNC.finditer(text)}


def main(argv: list[str]) -> int:
    gate = "--gate" in argv
    found = scan()

    if CANARY not in found:
        raise SystemExit(
            f"SELF-TEST FAILED: the function-header pattern did not find "
            f"'{CANARY}', which exists in at least three files. Every "
            f"'no duplication' this tool could print would be a false negative."
        )

    # THE SECOND CANARY, and it is the one that would have caught R173. The
    # unsigned canary above passed for the whole life of the broken pattern,
    # because it is a one-type-word declaration and the blind spot was two.
    # A self-check that cannot enter the failing state reports health it never
    # tested.
    if SIGNED_CANARY not in found:
        raise SystemExit(
            f"SELF-TEST FAILED: the function-header pattern did not find "
            f"'{SIGNED_CANARY}', a `logic signed [W-1:0]` function. The "
            f"pattern has lost SIGNED return types again -- that defect hid "
            f"125 of this tree's 524 functions, and they are the arithmetic "
            f"ones, which is where duplication actually costs DSPs."
        )

    shared = pkg_names()
    dupes = {n: v for n, v in found.items() if len({p for p, _ in v}) > 1}

    print(f"{len(found)} distinct function names across {len(list(RTL.rglob('*.sv')))} RTL files")
    print(f"self-test OK: '{CANARY}' resolves in {len(found[CANARY])} places, "
          f"and the SIGNED canary '{SIGNED_CANARY}' in {len(found[SIGNED_CANARY])}")
    print(f"zhao_pkg.sv defines {len(shared)} shared function(s)")
    print(f"\nDEFINED IN MORE THAN ONE FILE: {len(dupes)}\n")

    for name in sorted(dupes, key=lambda n: -len({p for p, _ in dupes[n]})):
        sites = dupes[name]
        files = sorted({p for p, _ in sites})
        flag = "  <-- ALSO IN zhao_pkg.sv" if name in shared else ""
        print(f"  {name}  ({len(files)} files){flag}")
        for p, line in sites:
            print(f"      {p.relative_to(ROOT).as_posix()}:{line}")
        print()

    if not dupes:
        print("  none -- which, given the canary resolved, is a real reading.")

    # Not a gate by default: some duplication is legitimate and this tool does
    # not know which. --gate is for a campaign that has decided on a list.
    return 1 if (gate and dupes) else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
