#!/usr/bin/env python3
"""Move Manafold's LIVE card to pass 25 -- the creature's FINAL pass.

Rewrites the creature blurb and the site note. Owner language throughout: the
card is for Fabian, not for a reviewer, so the numbers that appear are the ones
that change what he would do next.

! IT DOES NOT OVERCLAIM THE FRAME REVIEW. Every frame of all 22 clips was built
into contact sheets and the sheets were checked to hold their full counts; the
close looking was done on frames chosen BY BADNESS (the measured nearest bolt
pass, the measured worst rear jerk, the measured biggest eye change), not by
even sampling. The card says exactly that.

! IT DOES NOT BURY THE TWO THINGS THE OWNER MIGHT DISAGREE WITH. The front
ball's own spin is slower, and Crackle still has the rear fault. Both are in the
card, in his words, with the size of each.

The renderer MD5 and the bank manifest SHA-256 are READ FROM THE RECEIPTS rather
than typed, because a provenance line copied by hand is a provenance line that
can describe a different binary.

Idempotent: refuses to run if the blurb already says pass 25.
"""
import hashlib
import json
import re
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"
RECEIPTS = Path(__file__).resolve().parent

SOURCE = "b3c5760e"


def read_receipts():
    renderer = (RECEIPTS / "renderer.txt").read_text(encoding="utf-8")
    md5 = re.search(r"^([0-9a-f]{32}) ", renderer, re.M).group(1)
    bank = (RECEIPTS / "bank-manifest.txt").read_bytes()
    manifest_sha = hashlib.sha256(bank).hexdigest()
    rows = [l for l in bank.decode("utf-8").splitlines() if l and not l.startswith("#")]
    frames = sum(int(l.split("\t")[1]) for l in rows)
    return md5, manifest_sha, len(rows), frames


BLURB_TEMPLATE = """MANAFOLD, pass 25 — the last pass. The lightning is out of the antenna everywhere, the eyes act a little more, and we finally found out what the back ball actually is.

THIS IS THE FINAL PASS ON THIS CREATURE. Everything below is a rollout or a tuning of something you already approved. Nothing new was invented.

THE LIGHTNING IS NOW CLEAR OF THE ANTENNA ON ALL 22 CLIPS. Last pass it was an experiment on three. You said it still "gets very close now and looks like crossing", and you were right in a way the old check could not see: it was reporting ZERO crossings and you could still see the problem. Zero is not the same as clear. So the check now reports the NEAREST MISS as well, and that number located your complaint exactly — the tightest pass anywhere in the bank was FOUR AND A HALF MILLIMETRES. At 384 by 240, with a bolt a few pixels wide on one side and the black outline on the other, four millimetres closes straight back up into a touch. The clearance is raised from 46 to 96 millimetres and the tightest pass in the whole bank is now THIRTY-TWO millimetres — SEVEN TIMES the gap. Across all 22 clips there is not one bolt segment left inside a rod. It used to be about one segment in nine, and every single one of them belonged to the FOLDED FIGURE'S EDGE — the free-hanging strands never touched a rod on any clip, on any frame, and they still do not.

THE CLEARANCE KNOB IS NOT A DIAL YOU CAN JUST TURN UP, and that is worth knowing before anyone touches it. Raising it does NOT steadily reduce the crossings. 56, 66, 70, 76 and 106 millimetres all PUT CROSSINGS BACK — 70 is the worst, with eleven of them. 46, 86, 96, 116 and 130 are clean. The reason is that pushing a bolt off one rod can wedge it against the next one, and the push only gets a fixed number of tries. So 96 was not picked off a slider; it was picked from the values that were actually measured clean, and then looked at. The check goes red on a known-bad value, so nobody can set 70 by accident and have it look fine.

THE LIGHTNING ITSELF IS UNCHANGED, and that is guaranteed by the shape of the code rather than promised. The routine that moves a bolt is only ever handed that bolt's PATH — its size, colour, brightness, jaggedness, density and identity are not things it can reach at all. The separate check on lightning identity and loop seams is clean.

ONE THING YOU HAVE NOT SEEN BEFORE. At HOVER's tightest loop closure the pocket gets small enough that part of the figure is pushed OUTSIDE the rods, and more clearance makes that a little more pronounced. We looked hard at it: at 96 it still reads as energy running around the loop and coming back through it. At 116 it stops reading that way — it becomes a chain of beads drifting off into empty air — and that is exactly where we stopped. If you disagree, it is one number, downward, at the cost of the near passes coming back.

THE EYES ACT A LITTLE MORE, EVERYWHERE. The ambient gaze and eye-size drift goes from 600 to 800. It was chosen the same way as last time: a ladder, and the step BELOW the first one that starts to look deliberate. At 900 the star's arm sits on the rim of the lens; at 800 it comes near it and stays inside. STARTLE, CURIOUS and TAUNT III are byte-for-byte untouched — they are still plainly the loud ones, and we put them side by side with Rest at the new setting specifically to check that raising the quiet ones had not closed the gap. It has not: their stars are several times the size and swing across the whole lens, while the ambient layer is a sliver drifting inside a narrow one.

NOW THE BACK BALL — AND THE REAL ANSWER IS THAT WE HAD BEEN AIMING AT THE WRONG THING FOR FOUR PASSES.

Nobody had ever measured what actually moves it. So the first thing built this time was a measuring tool, not a fix: it freezes ONE part of the antenna at a time and measures what the visible surface does. Three things came out of it, and every one of them was a surprise.

FIRST: THE BALL AT THE VERY BACK BARELY MOVES AT ALL. It travels under a TENTH OF A PIXEL per frame — it is the calmest thing back there. Two thirds of even that is just the socket riding the body as it breathes. Every knob we have pointed at it in four passes was pointed at the one part of the rear that was already still.

SECOND: WHAT YOUR EYE IS ACTUALLY FOLLOWING is the ball one station further forward and the rod running back from it. Those move TWENTY times as much. They are what "too finicky" was about.

THIRD, AND THIS IS THE ONE THAT EXPLAINS THE WHOLE HISTORY: NO SINGLE PART OF THE ANTENNA OWNS THAT MOVEMENT. The grip travels up the antenna as a wave, on purpose, and the stations partly CANCEL EACH OTHER OUT. Freeze the middle one on its own and the last rod moves 146 PER CENT MORE. That is why every knob anyone tried either did nothing or made it worse — including the one from last pass that looked fifty times stronger on paper, and including the discovery that removing the kneading dip makes it travel FURTHER. Turning any one part down breaks a cancellation.

SO THE FIX IS NOT A KNOB AT ALL, IT IS A FILTER. "Too finicky" is a complaint about SPEED, not size, and Direction 20 said the antenna should stay "a bit wiggly" — so the answer is to take the fast jitter out and leave the slow swing in. Three stations of the antenna are now smoothed with a centred rolling average over about two thirds of a second, applied before the loop closes up, so the rear re-aims around the smoothed shape instead of being dragged after it. Centred means NO LAG: nothing arrives late.

WHAT IT LOOKS LIKE. Before, the rear rod's width and lean change from one frame to the next — the shape boils. After, it holds one shape and drifts, while the loop still visibly breathes. It travels 30 per cent less and turns 44 per cent slower. And here is the measure that says it is now right rather than merely quieter: the BACK of Hover's antenna used to turn MORE than the FRONT — 1.08 times as much, the only clip in the whole bank that did that, on the one long idle where the body is otherwise nearly still. It is now 0.99, inside the band every other clip already sits in. There are also two floors in the check, not just ceilings, precisely so a future change cannot satisfy it by killing the motion: the rear is REQUIRED to keep moving.

TWO THINGS YOU MIGHT DISAGREE WITH, SAID PLAINLY RATHER THAN BURIED.

ONE: HOVER AND INSPECT ARE ONE ANIMATION UNDER TWO CAMERAS, so both changed. There is no way to separate them without building a third copy of the idle, which is not a thing to do on a final pass. Inspect is the closer camera, so the fault was MORE visible there, not less.

TWO: THE FRONT BALL'S OWN SPIN IS SLOWER — about a third. Its POSITION is untouched, and so is everything you asked for last pass: the lift you approved lives on a different joint that this smoothing cannot reach at all, and the front ball still travels exactly as far as it did. What changed is only how fast the sphere itself rotates, on a ball that has no visible surface pattern at this size. We looked at it blown up eight times across consecutive frames and could not tell the two apart. If your eye disagrees with ours, that one is fixable and it is worth telling us.

AND ONE THING WE ARE OFFERING RATHER THAN DECIDING. CRACKLE STILL HAS THE REAR FAULT. It is the same animation as Hover on a separate copy, so the smoothing did not reach it, and it measures the same 1.08 that made Hover stand out. You named Hover, so Hover is what was changed — but Crackle is a long idle under a FIXED, CLOSE camera, which is the situation where a fidgety rear is most noticeable. Our honest read is that you would want it there too. It is ONE table entry and nothing else moves. Say the word.

WHAT IS AND IS NOT CLAIMED FROM THE PICTURES. Complete every-frame contact sheets were built for all 22 clips — {frames:,} frames, none omitted — and each sheet was checked to hold its full frame count. The close looking was done on frames chosen BY BADNESS rather than by even spacing: the measured nearest bolt pass on each clip, the measured worst rear jerk, and the frames a difference count named as the ones the eye setting moves most. Two things are deliberately NOT claimed: the remaining clips were seen at sheet scale rather than read frame by frame; and the bolt-crossing count is measured in three dimensions against the antenna's own posed geometry, while the on-screen crossing columns beside it are measured without perspective and are reported, not relied on.

PROVENANCE. Accepted source is zhaozhou {source}. The exact one-binary renderer MD5 is {md5}; bank manifest SHA-256 is {manifest_sha}. All {subjects} live subjects — {frames:,} frames — were rendered together in ONE invocation with no override of any kind, and every subject kept its exact frame count. The renderer that produced this bank was BUILT FROM SCRATCH BY THE INDEPENDENT REVIEWER, not by the author of the change, and it reproduces the implementation's own 22-subject bank BYTE FOR BYTE on all 22. The reviewer also re-ran the whole clearance sweep, the antenna decomposition and every check's controls on that binary, and got the same numbers to the digit. Switch the smoothing off and the bank is bit-for-bit identical to the rest of pass 25 on all 22 subjects; switch everything this pass added off and it is bit-for-bit identical to pass 24 on all 22. Exactly TWO subjects changed from the smoothing — Hover and Inspect — checked frame by frame, with Crackle's 600 frames byte-identical as the proof that it stayed contained. Pass 24 is now in the archive, byte for byte, and the archive check was extended to lock it.

A NOTE ON THE VIDEO FILES THEMSELVES. The video encoder is not byte-reproducible: encoding the same frames twice produces two different files of identical length. So every one of the 22 videos has new bytes this pass, including the clips whose pictures did not change at all. The frames are the evidence, not the file hashes."""


SITE_NOTE = (
    "Every creature is a continuous skinned surface bound to at most 32 bones, "
    "compiled by the reference implementation that defines the console. Manafold "
    "pass 25 is the creature's final pass: the lightning is clear of the antenna "
    "on all 22 clips with seven times the old margin, the eyes act a little more "
    "everywhere, and Hover's rear is smoothed after a measurement found that the "
    "ball the eye follows is not the one anybody had been adjusting. Pass 24 is "
    "preserved in the archive."
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    if manafold["blurb"].startswith("MANAFOLD, pass 25"):
        print("FAIL the card already says pass 25; nothing done")
        return 1
    if not manafold["blurb"].startswith("MANAFOLD, pass 24"):
        print("FAIL the card does not say pass 24; not edited")
        return 1

    md5, manifest_sha, subjects, frames = read_receipts()
    if subjects != 22:
        print(f"FAIL the bank manifest has {subjects} subjects, expected 22")
        return 1

    manafold["blurb"] = BLURB_TEMPLATE.format(
        source=SOURCE, md5=md5, manifest_sha=manifest_sha,
        subjects=subjects, frames=frames)
    manifest["site"]["note"] = SITE_NOTE
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK Manafold live card -> pass 25 (renderer {md5}, manifest {manifest_sha})")
    print(f"OK bank {subjects} subjects / {frames} frames")
    print("OK site note -> pass 25")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
