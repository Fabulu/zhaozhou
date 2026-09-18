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
FUNC = re.compile(
    r"^\s*function\s+(?:automatic\s+)?"      # function [automatic]
    r"(?:[A-Za-z_]\w*\s*(?:\[[^\]]*\]\s*)?)?"  # optional return type + range
    r"([A-Za-z_]\w*)\s*(?:\(|;)",              # the NAME
    re.MULTILINE,
)

# A name that MUST be found, or the pattern has rotted and every "no duplicates"
# below would be a false negative.
CANARY = "unit_mul"


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

    shared = pkg_names()
    dupes = {n: v for n, v in found.items() if len({p for p, _ in v}) > 1}

    print(f"{len(found)} distinct function names across {len(list(RTL.rglob('*.sv')))} RTL files")
    print(f"self-test OK: '{CANARY}' resolves in {len(found[CANARY])} places")
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
