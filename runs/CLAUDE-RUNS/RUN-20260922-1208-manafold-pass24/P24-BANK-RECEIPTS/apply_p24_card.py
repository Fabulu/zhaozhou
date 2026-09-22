#!/usr/bin/env python3
"""Move Manafold's LIVE card to pass 24.

Rewrites the creature blurb and the site note. Owner language throughout: the
card is for Fabian, not for a reviewer, so the numbers that appear are the ones
that change what he would do next.

! IT DOES NOT OVERCLAIM ITEM 1. The review measured Hover's rear-ambient change
at 178 changed pixels on its worst frame against the front's 11,758, and six
consecutive frames at 10x are indistinguishable. The card says so.

Idempotent: refuses to run if the blurb already says pass 24.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

RENDERER_MD5 = "e90af7c043cd9670054eeb256abd5e95"
MANIFEST_SHA = "f9128e0821004dc49b19a280651db864e4556947b20d9267fbb84d5b9e58703b"
SOURCE = "8e5a3ee3"

BLURB = """MANAFOLD, pass 24 — the lightning stops going through the antenna, and we counted exactly how often it did.

THE LIGHTNING THROUGH THE ANTENNA: WE MEASURED IT BEFORE CHANGING ANYTHING. You asked whether we had ideas, so the first thing built was a counter, not a fix. Across all 22 clips the bolts are drawn as 503,304 short straight segments. 54,595 of them — ABOUT ONE IN NINE — are not merely crossing the antenna on screen; they are physically inside the rod, by up to 99 millimetres. CRACKLE is the worst by a wide margin, exactly as you said it was: 5,685 segments, on 561 of its 600 frames. So this was never a drawing-order problem. The lightning sprites already test depth pixel by pixel, which means a bolt genuinely behind a rod is already hidden correctly and one genuinely in front is correctly on top. Both were right all along. It was geometry. One more thing the count settled, which nobody expected: EVERY SINGLE CROSSING BELONGS TO THE FOLDED FIGURE'S OUTLINE. The free-hanging strands never touch a rod, on any clip, on any frame.

THE COMPARISON YOU ASKED FOR — AND IT HAS A ONE-SIDED ANSWER. Two approaches, three clips, everything else left alone:

CRACKLE and HOVER carry BOLT AVOIDANCE — the bolt is pushed out of the rod in three dimensions. Both went from thousands of crossings to ZERO, on every frame, measured the same way the fault was.

INSPECT carries DEPTH SPLITTING — each bolt segment drawn as four shorter sprites, each with its own depth, so a segment can be partly hidden.

THE AVOIDANCE WORKS. THE SPLITTING CHANGED NOTHING AT ALL. Open INSPECT and the bolt still crosses the antenna exactly as it did before — and we can say precisely why, rather than guessing: the bolt was already being stamped every 26.6 millimetres, which is finer than the 46-millimetre rod it crosses, so there was no detail left to win. All 155 of the segments that straddle a rod's depth were already being drawn partly cut before the change. The only visible difference is that the white core reads as one smooth filament instead of showing faint beading. Now open CRACKLE or HOVER beside the pass-23 archive: the blue figure used to lie flat across the pink band, and now it sits clear of it. That is the whole comparison, and it says avoidance is the one to roll out.

THE AVOIDANCE CANNOT RESTYLE THE LIGHTNING, BY CONSTRUCTION. The code that moves a bolt only ever receives that bolt's path — its size, colour, brightness, jaggedness, density and identity are not things it can reach at all. Your "it is good now as it is" is kept by the shape of the code rather than by a promise, and the separate check on lightning identity and loop seams is clean.

ONE THING TO LOOK AT AND TELL US ABOUT. At HOVER's tightest loop closure the pocket gets small enough that the bolt is pushed to the OUTSIDE of the rods, where it wraps around the loop instead of filling it. Same lightning — same size, same shape, same colours, unbroken — but it is a placement you have not seen before. Our read is that it looks BETTER than the old one, where the bolt lay flat across the band and read as tangled behind it. If you disagree, it is one number to turn.

WORTH KNOWING FOR THE COMPARISON: CRACKLE, HOVER AND INSPECT ARE THE SAME ANIMATION, shown from three different cameras. That is what makes them compare cleanly with no pose difference in the way — and it is also why any change to that animation lands on all three cards at once.

HOVER'S FRONT BALL MOVES MORE, AND IT READS. Same curve, same timing, same loop seam — just more of it. The step above this one changed the whole attitude of the loop into a different performance, so this is the step below that.

HOVER'S BACK BALL: HONESTLY, ONLY A LITTLE, AND WE ARE NOT GOING TO PRETEND OTHERWISE. The setting is now per-clip instead of one number for the whole bank, and Hover's is lowered, which is what was asked for. But it barely shows. On the frame where it changes most it moves 178 pixels, inside a box twenty pixels by sixteen — against 11,758 pixels for the front change. Six consecutive frames put side by side and blown up ten times are essentially indistinguishable. The reason is that this particular setting is only about a tenth to a quarter of what the back ball is actually doing; the rest is the authored swallowing beat, the socket following the body as it breathes, and the rear rod changing aim as the loop closes.

AND WE FOUND A MUCH BIGGER LEVER, BY ACCIDENT. There has been a knob for the back nodule — the one the eye actually reads — since pass 20, but it was wired only to a measuring tool. No picture anyone has ever looked at was affected by it, which is why nobody noticed. It is connected properly now. Turned one step, it moves 9,352 pixels where the setting you pointed at moves 178 — roughly FIFTY TIMES as much, on exactly the part you are complaining about. Its value is unchanged in this pass, so nothing here moved because of it, and it has never been set by eye by anybody. That is the first thing to try next pass.

THE EYES DO A LITTLE ACTING NOW. Gaze drifts and the eyes change size slightly on the ordinary clips, running on three slow cycles of different lengths so the face never settles into one readable rhythm. The amount was chosen off a ladder by taking the step BELOW the first one that started to look deliberate. STARTLE, CURIOUS and TAUNT III are byte-for-byte untouched — they were already the good ones and no floor was slid under them. The diagnostics get none, and nothing drifts inside Trick's headstand.

PROVENANCE. Accepted source is zhaozhou """ + SOURCE + """. The exact one-binary renderer MD5 is """ + RENDERER_MD5 + """; bank manifest SHA-256 is """ + MANIFEST_SHA + """. All 22 live subjects — 7,992 frames — were rendered together in ONE invocation with no override of any kind, and every subject kept its exact pass-23 frame count. The renderer was rebuilt from scratch by an independent reviewer, who reproduced the implementation's whole 22-subject bank BYTE FOR BYTE, re-ran the 503,304-segment measurement to the digit, and fired every check's controls again. The whole bank was then rendered a SECOND time with every pass-24 setting put back to pass 23's and the two compared frame by frame: EXACTLY NINETEEN subjects differ, and the three that do not are Curious, Startle and Taunt III — which is the proof that the authored expression beats were left alone. With everything switched off the bank is byte-identical to pass 23 on all 22 subjects. Pass 23 itself is now in the archive, byte for byte, and the archive check was extended to lock it.

WHAT IS AND IS NOT CLAIMED FROM THE PICTURES. Complete every-frame contact sheets were built for all 22 clips — 7,992 frames, none omitted — and each sheet was checked to hold its full frame count. The frames that actually changed most were found by comparing the two banks rather than by sampling evenly, and those were opened close up: Crackle and Hover through the crossings, Hover at its tightest closure, Hover's front and back through their worst frames, and Rest and Taunt II on the eyes. Two things are deliberately NOT claimed: the remaining clips were seen at sheet scale rather than read frame by frame, and they rest on the exact frame-by-frame comparison above; and the bolt-crossing count is measured in three dimensions against the antenna's own posed geometry, while the separate on-screen crossing columns beside it are measured without perspective and are reported, not relied on.

A NOTE ON THE VIDEO FILES THEMSELVES. The video encoder is not byte-reproducible: encoding the same frames twice produces two different files of identical length. So every one of the 22 videos has new bytes this pass, including the three clips whose pictures did not change at all. The frames are the evidence, not the file hashes."""

SITE_NOTE = (
    "Every creature is a continuous skinned surface bound to at most 32 bones, "
    "compiled by the reference implementation that defines the console. Manafold "
    "pass 24 keeps the lightning out of the antenna rods on Crackle and Hover, "
    "after measuring that about one drawn bolt segment in nine was physically "
    "inside a rod; Inspect carries the rival approach, depth-splitting, which the "
    "same measurement and the picture both show changes nothing. Pass 23 is "
    "preserved in the archive."
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    if manafold["blurb"].startswith("MANAFOLD, pass 24"):
        print("FAIL the card already says pass 24; nothing done")
        return 1
    if not manafold["blurb"].startswith("MANAFOLD, pass 23"):
        print("FAIL the card does not say pass 23; not edited")
        return 1
    manafold["blurb"] = BLURB
    manifest["site"]["note"] = SITE_NOTE
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print("OK Manafold live card -> pass 24")
    print("OK site note -> pass 24")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
