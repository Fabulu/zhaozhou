#!/usr/bin/env python3
"""Pass-23 scope proof: the SHIPPING bank against the same renderer with the
press-depth ladder set back to the four values pass 22 shipped.

⚠ THE EXPECTATION IS THE OPPOSITE OF PASS 22's. Pass 22 changed an effect that
touches every clip, so 22 of 22 subjects had to move. Pass 23 changes FOUR
per-clip press depths, so exactly four subjects may move and the other
EIGHTEEN must be byte-identical. A scope proof that only counts "how many
changed" would call both of those a pass; this one names which.

Usage: p23_scope.py <shipping-root> <exactoff-root> <out.txt>
"""
import sys
from pathlib import Path

CHANGED = {"manafold-drift", "manafold-damage",
           "manafold-death-drop", "manafold-death-gutter"}


def main(ship, off, out):
    ship, off = Path(ship), Path(off)
    rows, errs = [], []
    subs = sorted(p.name for p in ship.iterdir() if p.is_dir())
    if len(subs) != 22:
        print(f"FAIL shipping root has {len(subs)} subjects, expected 22")
        return 1
    for s in subs:
        a, b = ship / s, off / s
        frames = sorted(p.name for p in a.glob("*.rgb"))
        changed, worst_px, worst_frame = 0, 0, ""
        for f in frames:
            x, y = (a / f).read_bytes(), (b / f).read_bytes()
            if x == y:
                continue
            changed += 1
            n = sum(1 for i in range(8, min(len(x), len(y)), 3)
                    if x[i:i + 3] != y[i:i + 3])
            if n > worst_px:
                worst_px, worst_frame = n, f[:-4]
        rows.append((s, len(frames), changed, worst_px, worst_frame))
        if s in CHANGED and changed == 0:
            errs.append(f"{s} is a RAISED clip and did not move")
        if s not in CHANGED and changed != 0:
            errs.append(f"{s} moved ({changed} frames) and no press depth touched it")
    hdr = ["# Pass 23 scope proof: SHIPPING bank vs the SAME renderer with the",
           "# press depths back to pass 22's (ZHAO_U02_KNEAD_DIP_CLIP_PM="
           "1:730,14:635,17:635,18:590).",
           "# EXPECTED: exactly the four raised clips move; the other 18 are",
           "# byte-identical. Named, not merely counted.",
           "# subject\tframes\tchanged\tworst_px\tworst_frame"]
    body = [f"{s}\t{n}\t{c}\t{w}\t{wf}" for s, n, c, w, wf in rows]
    moved = [s for s, _, c, _, _ in rows if c]
    tail = [f"# subjects={len(rows)} frames={sum(r[1] for r in rows)} "
            f"moved={len(moved)} expected_moved=4",
            "# moved: " + " ".join(sorted(moved))]
    Path(out).write_text("\n".join(hdr + body + tail) + "\n", encoding="utf-8",
                         newline="\n")
    for e in errs:
        print("FAIL " + e)
    print(f"scope: {len(rows)} subjects, {sum(r[1] for r in rows)} frames, "
          f"{len(moved)} moved ({' '.join(sorted(moved))})")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main(*sys.argv[1:4]))
