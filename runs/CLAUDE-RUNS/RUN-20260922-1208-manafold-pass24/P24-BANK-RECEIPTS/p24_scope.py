#!/usr/bin/env python3
"""Pass 24 scope proof: the SHIPPING bank against the SAME renderer with every
pass-24 knob at its pass-23 value, compared FRAME BY FRAME.

The claim this proves is "named, not merely counted": exactly which subjects
moved, and by how much on their worst frame. A sequence CRC already proves
byte-identity; this adds the size and the location of the change on the subjects
that did move, so "19 changed" is a statement somebody can check.

    python p24_scope.py SHIP_DIR BASE_DIR OUT.txt
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools" / "reel"))
from rgbframe import load  # noqa: E402

EXPECTED_UNCHANGED = {"manafold-curious", "manafold-startle", "manafold-taunt3"}


def main():
    ship, base, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    subjects = sorted(p.name for p in ship.iterdir() if p.is_dir())
    rows, bad = [], []
    for s in subjects:
        a = sorted((ship / s).glob("*.rgb"))
        b = sorted((base / s).glob("*.rgb"))
        if len(a) != len(b):
            bad.append(f"{s}: frame counts differ {len(a)} vs {len(b)}")
            continue
        changed, worst, worst_f = 0, 0, ""
        for fa, fb in zip(a, b):
            x, y = load(str(fa)).astype(np.int16), load(str(fb)).astype(np.int16)
            d = int(np.count_nonzero(np.any(x != y, axis=-1)))
            if d:
                changed += 1
                if d > worst:
                    worst, worst_f = d, fa.stem
        rows.append((s, len(a), changed, worst, worst_f))

    moved = {s for s, _, c, _, _ in rows if c}
    still = {s for s, _, c, _, _ in rows if not c}
    hdr = [
        "# Pass 24 scope proof: the SHIPPING bank vs the SAME renderer with every",
        "# pass-24 knob at its pass-23 value (BOLT_AVOID=off, BOLT_SPLIT_N=1,",
        "# REAR_AMBIENT_CLIP_PM=0:400,23:400, FRONT_FLEX_CLIP_PM=0:1000,23:1000,",
        "# EYE_AMBIENT_PM=0). Compared frame by frame, not by CRC alone.",
        "#",
        "# EXPECTED: exactly 19 subjects move. The three that must NOT are the",
        "# authored expression beats the owner called already right --",
        "# curious, startle and taunt III. Named, not merely counted.",
        f"# moved={len(moved)} unchanged={len(still)}",
        f"# unchanged: {' '.join(sorted(still))}",
        "# subject\tframes\tchanged\tworst_px\tworst_frame",
    ]
    body = [f"{s}\t{n}\t{c}\t{w}\t{wf}" for s, n, c, w, wf in rows]
    out.write_text("\n".join(hdr + body) + "\n", encoding="utf-8", newline="\n")

    if bad:
        for e in bad:
            print("FAIL", e)
        return 1
    if still != EXPECTED_UNCHANGED:
        print(f"FAIL unchanged set is {sorted(still)}, expected {sorted(EXPECTED_UNCHANGED)}")
        return 1
    print(f"OK scope proof: {len(moved)} subjects moved, {len(still)} byte-identical "
          f"({' '.join(sorted(still))}) -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
