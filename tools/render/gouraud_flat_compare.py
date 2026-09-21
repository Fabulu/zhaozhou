#!/usr/bin/env python3
"""gouraud_flat_compare -- LOOK at the Gouraud reading beside the FLAT stand-in.

Packet GOURAUDLOOK, owner ruling R230.

The console carries a lit per-vertex colour four blocks and drops it in
GEOM.ATTRPACK. Finishing that delivery costs ~1,420 ALM and +24 DSP against a
112-DSP budget; the alternative is a FLAT per-face stand-in the surviving
modulation lanes already deliver for nothing.

CLAUDE.md's first law says that is not a question a cost table may answer:

    Measurement never trumps actually looking at things.
    ... Look at the whole thing, in motion, against the concept.

So this tool builds NO verdict. It builds the PICTURE, and it builds it the way
CLAUDE.md's "Seeing the work properly" asks for:

  * BEFORE/AFTER PAIRS, never two separate galleries -- the eye cannot hold a
    240p shading difference across two browser tabs.
  * CONTACT SHEETS of EVERY frame, because "uniform sampling finds the typical
    frame and misses the broken one".
  * SAMPLED BY BADNESS, not by index: the frames where the two readings differ
    most are the ones that decide this, so they are ranked and shown first.
  * A FLIPBOOK, A/B on one spot at final resolution, which is the single most
    honest instrument for "can you actually see it".
  * A TRAJECTORY over time, where a flat line IS the finding.

Measurement appears here only on the COMPARISON side (CLAUDE.md rule 2): it
ranks and it reports magnitude. It never chooses.

Input is two reel output trees rendered from ONE binary, same camera, same
frames, same content -- `zhao-reel` unset and with ZIXX_SHADE=flat.

Usage:
  python tools/render/gouraud_flat_compare.py --a <gouraud-dir> --b <flat-dir>
                                              --out <png-dir> [--subjects ...]
                                              [--top 8] [--zoom 3]
                                              [--no-contact]
"""

from __future__ import annotations

import argparse
import csv
import pathlib
import struct
import sys

import numpy as np
from PIL import Image, ImageDraw

# --- named, editable constants (CLAUDE.md rule 6: every value stays a knob) ---

# A per-channel step of this size on a 240p creature is the threshold at which a
# shading difference stops being arithmetic and starts being something an eye
# can find. It is a JUDGEMENT, set here so it can be argued with and changed --
# not a derived quantity dressed up as one.
VISIBLE_STEP = 8

# The diff panel is unviewable at true scale, so it is amplified. This is a
# DIAGNOSTIC gain and the panel is labelled with it; the pair panels beside it
# are always the honest pixels.
DIFF_GAIN = 6

# Contact-sheet geometry.
CONTACT_COLS = 12
LABEL_H = 14
PAD = 4

BG = (24, 24, 28)
FG = (232, 232, 236)
DIM = (150, 150, 158)


def read_rgb(path: pathlib.Path) -> np.ndarray:
    """One reel frame: u32 w LE | u32 h LE | w*h*3 RGB888."""
    raw = path.read_bytes()
    w, h = struct.unpack_from("<II", raw, 0)
    px = np.frombuffer(raw, dtype=np.uint8, count=w * h * 3, offset=8)
    return px.reshape(h, w, 3)


def frames_of(d: pathlib.Path) -> list[pathlib.Path]:
    return sorted(d.glob("[0-9][0-9][0-9][0-9].rgb"))


def score(a: np.ndarray, b: np.ndarray) -> dict:
    """Magnitude of the disagreement between the two readings, one frame.

    Reported, never ruled on. `visible_px` is the headline: pixels whose worst
    channel moves by at least VISIBLE_STEP.
    """
    d = np.abs(a.astype(np.int16) - b.astype(np.int16))
    worst = d.max(axis=2)
    any_px = int((worst > 0).sum())
    vis_px = int((worst >= VISIBLE_STEP).sum())
    total = worst.size
    return {
        "any_px": any_px,
        "any_pct": 100.0 * any_px / total,
        "visible_px": vis_px,
        "visible_pct": 100.0 * vis_px / total,
        "max_delta": int(worst.max()),
        "mean_delta_over_changed": float(worst[worst > 0].mean()) if any_px else 0.0,
        "_worst": worst,
    }


def diff_panel(worst: np.ndarray) -> Image.Image:
    """Amplified difference, black -> red -> yellow -> white. Labelled as x GAIN."""
    v = np.clip(worst.astype(np.int32) * DIFF_GAIN, 0, 255).astype(np.uint8)
    f = v.astype(np.float32) / 255.0
    r = np.clip(f * 3.0, 0, 1)
    g = np.clip(f * 3.0 - 1.0, 0, 1)
    b = np.clip(f * 3.0 - 2.0, 0, 1)
    rgb = (np.stack([r, g, b], axis=2) * 255).astype(np.uint8)
    return Image.fromarray(rgb)


def upscale(img: Image.Image, z: int) -> Image.Image:
    """NEAREST only. A smooth resample would INVENT the gradient that is the
    whole thing under test -- it would literally manufacture Gouraud. The zoom
    is a magnifying glass over final-resolution pixels, never a resample."""
    return img if z == 1 else img.resize((img.width * z, img.height * z), Image.NEAREST)


# Padding around the changed region, in final-resolution pixels. Enough
# unchanged surround that the crop is readable as a creature and not as an
# abstract patch -- the art law wants the whole thing looked at, in context.
CROP_PAD = 6


def union_bbox(worsts: list[np.ndarray], shape: tuple[int, int]) -> tuple[int, int, int, int]:
    """Where in the frame do the two readings EVER disagree?

    Sampling by badness applied to SPACE as well as to time. A 384x240 frame
    with a 40x90 creature in it is mostly sky, and a pair of full frames spends
    most of its area proving that the sky is identical -- which it is, and which
    nobody needed to see. Computed ONCE per subject and held across every frame,
    so the pair never shifts under the eye between panels.
    """
    h, w = shape
    acc = np.zeros((h, w), dtype=bool)
    for wo in worsts:
        acc |= wo > 0
    if not acc.any():
        return 0, 0, w, h
    ys, xs = np.where(acc)
    x0 = max(0, int(xs.min()) - CROP_PAD)
    y0 = max(0, int(ys.min()) - CROP_PAD)
    x1 = min(w, int(xs.max()) + 1 + CROP_PAD)
    y1 = min(h, int(ys.max()) + 1 + CROP_PAD)
    return x0, y0, x1, y1


def crop(a: np.ndarray, box: tuple[int, int, int, int]) -> np.ndarray:
    x0, y0, x1, y1 = box
    return a[y0:y1, x0:x1]


def label(draw: ImageDraw.ImageDraw, x: int, y: int, text: str, col=FG) -> None:
    draw.text((x, y), text, fill=col)


def build_triptych(a: np.ndarray, b: np.ndarray, s: dict, zoom: int, caption: str) -> Image.Image:
    """One row: GOURAUD | FLAT | amplified difference. The decisive panel."""
    pa = upscale(Image.fromarray(a), zoom)
    pb = upscale(Image.fromarray(b), zoom)
    pd = upscale(diff_panel(s["_worst"]), zoom)
    w, h = pa.width, pa.height
    sheet = Image.new("RGB", (w * 3 + PAD * 4, h + LABEL_H * 2 + PAD * 3), BG)
    d = ImageDraw.Draw(sheet)
    label(d, PAD, PAD, caption)
    y = PAD + LABEL_H
    for i, (panel, name) in enumerate(((pa, "GOURAUD (per-vertex)"), (pb, "FLAT (per-face stand-in)"),
                                       (pd, f"difference x{DIFF_GAIN}"))):
        x = PAD + i * (w + PAD)
        sheet.paste(panel, (x, y))
        label(d, x, y + h + 2, name, DIM)
    return sheet


def build_contact(frames: list[tuple[np.ndarray, np.ndarray, dict, int]], zoom: int,
                  title: str) -> Image.Image:
    """EVERY frame, Gouraud stacked directly over Flat, so the pair is one cell.

    Two separate galleries do not work: the difference has to be a vertical
    saccade of a few pixels or the eye simply cannot hold it.
    """
    if not frames:
        return Image.new("RGB", (400, 40), BG)
    fh, fw = frames[0][0].shape[:2]
    cw, ch = fw * zoom, fh * zoom
    cell_h = ch * 2 + LABEL_H + PAD
    cols = CONTACT_COLS
    rows = (len(frames) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (cw + PAD) + PAD, rows * (cell_h + PAD) + PAD + LABEL_H * 2), BG)
    d = ImageDraw.Draw(sheet)
    label(d, PAD, PAD, title)
    label(d, PAD, PAD + LABEL_H, "each cell: TOP = Gouraud, BOTTOM = flat stand-in", DIM)
    y0 = PAD + LABEL_H * 2
    for i, (a, b, s, idx) in enumerate(frames):
        r, c = divmod(i, cols)
        x = PAD + c * (cw + PAD)
        y = y0 + r * (cell_h + PAD)
        sheet.paste(upscale(Image.fromarray(a), zoom), (x, y))
        sheet.paste(upscale(Image.fromarray(b), zoom), (x, y + ch))
        label(d, x, y + ch * 2 + 1, f"{idx:04d} {s['visible_pct']:.1f}%", DIM)
    return sheet


def build_trajectory(scores: list[dict], title: str, w: int = 900, h: int = 260) -> Image.Image:
    """Disagreement over time. A FLAT LINE IS THE FINDING -- CLAUDE.md says so."""
    img = Image.new("RGB", (w, h), BG)
    d = ImageDraw.Draw(img)
    label(d, PAD, PAD, title)
    label(d, PAD, PAD + LABEL_H, f"% of frame pixels moving >= {VISIBLE_STEP}/255 (solid) "
                                f"and > 0 (faint)", DIM)
    x0, y0, x1, y1 = 46, 44, w - 12, h - 24
    d.rectangle([x0, y0, x1, y1], outline=(70, 70, 80))
    n = len(scores)
    if n == 0:
        return img
    top = max(1e-9, max(s["any_pct"] for s in scores)) * 1.12
    for tick in range(5):
        yy = y1 - (y1 - y0) * tick / 4
        d.line([x0, yy, x1, yy], fill=(48, 48, 56))
        label(d, 4, yy - 6, f"{top * tick / 4:5.1f}%", DIM)

    def poly(key, col, width):
        pts = []
        for i, s in enumerate(scores):
            xx = x0 + (x1 - x0) * (i / max(1, n - 1))
            yy = y1 - (y1 - y0) * (s[key] / top)
            pts.append((xx, yy))
        if len(pts) == 1:
            d.ellipse([pts[0][0] - 3, pts[0][1] - 3, pts[0][0] + 3, pts[0][1] + 3], fill=col)
        else:
            d.line(pts, fill=col, width=width)

    poly("any_pct", (90, 90, 150), 1)
    poly("visible_pct", (255, 140, 60), 2)
    label(d, x0, y1 + 6, "frame 0", DIM)
    label(d, x1 - 60, y1 + 6, f"frame {n - 1}", DIM)
    return img


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", required=True, help="Gouraud reel output root")
    ap.add_argument("--b", required=True, help="flat stand-in reel output root")
    ap.add_argument("--out", required=True, help="PNG directory to write")
    ap.add_argument("--subjects", nargs="*", default=None)
    ap.add_argument("--top", type=int, default=8, help="worst-N frames to show as pairs")
    ap.add_argument("--zoom", type=int, default=3, help="nearest-neighbour zoom for pair panels")
    ap.add_argument("--contact-zoom", type=int, default=1)
    ap.add_argument("--no-contact", action="store_true")
    ap.add_argument("--extra", nargs="*", default=[], metavar="LABEL=DIR",
                    help="further readings to add as BOARD columns, e.g. "
                         "flat-pv-a=gzout/flat-pv-a. The per-frame analysis stays A-vs-B; "
                         "these widen the picture the owner rules from.")
    args = ap.parse_args()
    extras = []
    for spec in args.extra:
        lab, _, dd = spec.partition("=")
        extras.append((lab, pathlib.Path(dd)))

    A, B = pathlib.Path(args.a), pathlib.Path(args.b)
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    subjects = args.subjects or sorted(
        p.name for p in A.iterdir() if p.is_dir() and (B / p.name).is_dir())

    summary, board_true, board_crop = [], [], []
    for name in subjects:
        fa, fb = frames_of(A / name), frames_of(B / name)
        if len(fa) != len(fb) or not fa:
            print(f"{name}: SKIP ({len(fa)} vs {len(fb)} frames)")
            continue
        sdir = out / name
        sdir.mkdir(exist_ok=True)

        scores, cells = [], []
        for i, (pa, pb) in enumerate(zip(fa, fb)):
            a, b = read_rgb(pa), read_rgb(pb)
            s = score(a, b)
            s["frame"] = i
            scores.append(s)
            cells.append((a, b, s, i))

        # --- per-frame CSV: the measurement, on the comparison side only ---
        with open(sdir / "frames.csv", "w", newline="") as fh:
            wcsv = csv.writer(fh)
            wcsv.writerow(["frame", "any_px", "any_pct", "visible_px", "visible_pct",
                           "max_delta", "mean_delta_over_changed"])
            for s in scores:
                wcsv.writerow([s["frame"], s["any_px"], f"{s['any_pct']:.4f}",
                               s["visible_px"], f"{s['visible_pct']:.4f}", s["max_delta"],
                               f"{s['mean_delta_over_changed']:.3f}"])

        # --- where does it ever disagree? crop there, once, for the subject ---
        box = union_bbox([s["_worst"] for s in scores], scores[0]["_worst"].shape)
        cw, chh = box[2] - box[0], box[3] - box[1]

        # --- SAMPLE BY BADNESS, not by index ---
        ranked = sorted(scores, key=lambda s: (s["visible_px"], s["any_px"]), reverse=True)
        worst = ranked[: args.top]
        for rank, s in enumerate(worst):
            i = s["frame"]
            a, b = cells[i][0], cells[i][1]
            cap = (f"{name}  frame {i:04d}  rank {rank + 1}/{len(scores)} by badness   "
                   f"changed {s['any_pct']:.2f}%  visible(>={VISIBLE_STEP}) {s['visible_pct']:.2f}%  "
                   f"max delta {s['max_delta']}")
            # the honest frame, whole, at true size -- the art law's "at final
            # resolution, against what it sits on"
            build_triptych(a, b, s, 1, cap + "   [WHOLE FRAME, TRUE 384x240]").save(
                sdir / f"pair-rank{rank + 1:02d}-f{i:04d}-frame-x1.png")
            # and the same final-resolution pixels under a magnifier, cropped
            # to THIS frame's own disagreement -- a union box over a 160-frame
            # walk spans the whole path and leaves the creature a sliver
            fbox = union_bbox([s["_worst"]], s["_worst"].shape)
            fw_, fh_ = fbox[2] - fbox[0], fbox[3] - fbox[1]
            ca, cb = crop(a, fbox), crop(b, fbox)
            cs = dict(s)
            cs["_worst"] = crop(s["_worst"], fbox)
            build_triptych(ca, cb, cs, args.zoom, cap +
                           f"   [crop {fw_}x{fh_} of 384x240, NEAREST x{args.zoom}]").save(
                sdir / f"pair-rank{rank + 1:02d}-f{i:04d}-crop-x{args.zoom}.png")

        # --- FLIPBOOK: A/B on one spot. The single most honest instrument for
        # "can you actually see it" -- the eye cannot compare across a gap, but
        # it is very good at noticing something change under it. ---
        wf = ranked[0]["frame"]
        wbox = union_bbox([ranked[0]["_worst"]], ranked[0]["_worst"].shape)
        fa_img, fb_img = Image.fromarray(cells[wf][0]), Image.fromarray(cells[wf][1])
        ca_img = Image.fromarray(crop(cells[wf][0], wbox))
        cb_img = Image.fromarray(crop(cells[wf][1], wbox))
        upscale(fa_img, 1).save(sdir / "flip-frame-x1.gif", save_all=True,
                                append_images=[upscale(fb_img, 1)], duration=700, loop=0)
        upscale(ca_img, args.zoom).save(sdir / f"flip-crop-x{args.zoom}.gif", save_all=True,
                                        append_images=[upscale(cb_img, args.zoom)],
                                        duration=700, loop=0)

        # --- CONTACT SHEET of EVERY frame, cropped to where it matters ---
        if not args.no_contact:
            ccells = [(crop(a, box), crop(b, box), s, i) for (a, b, s, i) in cells]
            build_contact(ccells, args.contact_zoom,
                          f"{name} -- EVERY frame, {len(ccells)} pairs, {cw}x{chh} crop of "
                          f"384x240 native, NEAREST x{args.contact_zoom}").save(
                sdir / "contact-all-frames.png")

        build_trajectory(scores, f"{name} -- Gouraud vs flat stand-in over time").save(
            sdir / "trajectory.png")

        bcap = (f"{name}  frame {wf}  (worst of {len(scores)})   "
                f"{ranked[0]['visible_pct']:.2f}% of the frame moves >= {VISIBLE_STEP}/255, "
                f"peak {ranked[0]['max_delta']}/255")
        cols_true = [("GOURAUD", fa_img.copy()), ("FLAT face-Lambert", fb_img.copy())]
        bz = max(1, min(6, 260 // max(1, ca_img.height)))
        cols_crop = [("GOURAUD", upscale(ca_img, bz)),
                     ("FLAT face-Lambert", upscale(cb_img, bz))]
        for lab, ed in extras:
            ef = frames_of(ed / name)
            if len(ef) != len(fa):
                continue
            ea = read_rgb(ef[wf])
            cols_true.append((lab, Image.fromarray(ea)))
            cols_crop.append((lab, upscale(Image.fromarray(crop(ea, wbox)), bz)))
        board_true.append((cols_true, bcap))
        board_crop.append((cols_crop, bcap + f"   [NEAREST x{bz}]"))

        tot_any = sum(s["any_px"] for s in scores)
        tot_vis = sum(s["visible_px"] for s in scores)
        tot_px = sum(s["_worst"].size for s in scores)
        summary.append({
            "subject": name, "frames": len(scores),
            "any_pct": 100.0 * tot_any / tot_px,
            "visible_pct": 100.0 * tot_vis / tot_px,
            "max_delta": max(s["max_delta"] for s in scores),
            "worst_frame": wf,
            "worst_frame_visible_pct": ranked[0]["visible_pct"],
        })
        print(f"{name}: {len(scores)} frames  changed {summary[-1]['any_pct']:.3f}%  "
              f"visible {summary[-1]['visible_pct']:.3f}%  max delta {summary[-1]['max_delta']}  "
              f"worst frame {wf} ({ranked[0]['visible_pct']:.2f}%)")

    # --- THE BOARD: every subject's worst frame, one image, so the ruling can
    # be made from ONE file instead of a directory. Two of them: the honest
    # 384x240 the console actually outputs, and the same pixels magnified. ---
    for tag, panels in (("true-384x240", board_true), ("magnified", board_crop)):
        if not panels:
            continue
        gap, head = PAD * 2, LABEL_H * 4
        wmax = max(sum(c[1].width + gap for c in cols) for cols, _ in panels)
        htot = sum(cols[0][1].height + LABEL_H * 2 + gap for cols, _ in panels) + head
        sheet = Image.new("RGB", (wmax + gap * 2, htot + gap), BG)
        dd = ImageDraw.Draw(sheet)
        label(dd, PAD, PAD, "R230 -- GOURAUD vs THE FLAT STAND-INS")
        label(dd, PAD, PAD + LABEL_H,
              "worst frame of each subject, ranked by badness; "
              + ("TRUE console output size, 384x240 -- JUDGE HERE" if tag == "true-384x240"
                 else "NEAREST magnification of those same final-resolution pixels"), DIM)
        label(dd, PAD, PAD + LABEL_H * 2,
              "nothing outside the creature moves: terrain, sky and fog are bit-identical "
              "in every column", DIM)
        label(dd, PAD, PAD + LABEL_H * 3,
              "face-Lambert needs a per-face normal; pv-a holds corner A's light and is what "
              "the console's per-triangle vertex_rgb can actually carry", DIM)
        y = head + gap
        for cols, cap in panels:
            label(dd, gap, y, cap, DIM)
            x = gap
            for cname, img in cols:
                sheet.paste(img, (x, y + LABEL_H))
                label(dd, x, y + LABEL_H + img.height + 1, cname, FG)
                x += img.width + gap
            y += cols[0][1].height + LABEL_H * 2 + gap
        sheet.save(out / f"BOARD-{tag}.png")

    with open(out / "summary.csv", "w", newline="") as fh:
        wcsv = csv.DictWriter(fh, fieldnames=list(summary[0].keys()) if summary else ["subject"])
        wcsv.writeheader()
        for row in summary:
            wcsv.writerow(row)
    print(f"\nwrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
