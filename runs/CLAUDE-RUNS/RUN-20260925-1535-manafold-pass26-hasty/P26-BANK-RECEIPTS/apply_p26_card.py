#!/usr/bin/env python3
"""Move Manafold's LIVE card to pass 26 -- Hasty's hurry.

Rewrites the creature blurb and the site note. Owner language throughout: the
card is for Fabian, not for a reviewer, so the numbers that appear are the ones
that change what he would do next.

! IT DOES NOT OVERCLAIM THE FRAME REVIEW. Every frame of all 22 clips was built
into contact sheets and the sheets were checked to hold their full counts; all
240 frames of the clip that changed were looked at, and the other 21 are
byte-identical to a generation that was already production-verified. The card
says exactly that and no more.

! IT DOES NOT SAY THE LOOP SEAM CLOSED. The implementation pass reported the
seam as 38.8 px -> 0.0 px and "effectively vanished". THE REVIEW FOUND THAT
CLAIM UNSUPPORTABLE and corrected it before it reached this card: the mask that
figure is measured with scores 82% background on a creature-removed frame, and
the two frames it samples are exactly the two the repair brings into agreement.
The hitch MOVED and SHRANK; it did not close. The card says the true thing,
which is also the better thing. See P26-REVIEW.md section 4.

The renderer MD5 and the bank manifest SHA-256 are READ FROM THE RECEIPTS rather
than typed, because a provenance line copied by hand is a provenance line that
can describe a different binary.

Idempotent: refuses to run if the blurb already says pass 26.
"""
import hashlib
import json
import re
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"
RECEIPTS = Path(__file__).resolve().parent

SOURCE = "bfb22a65"


def read_receipts():
    crcs = (RECEIPTS / "crcs-ship-p26.txt").read_text(encoding="utf-8")
    md5 = re.search(r"md5\s*:\s*([0-9a-f]{32})", crcs).group(1)
    bank = (RECEIPTS / "bank-manifest.txt").read_bytes()
    manifest_sha = hashlib.sha256(bank).hexdigest()
    rows = [l for l in bank.decode("utf-8").splitlines() if l and not l.startswith("#")]
    frames = sum(int(l.split("\t")[1]) for l in rows)
    return md5, manifest_sha, len(rows), frames


BLURB_TEMPLATE = """MANAFOLD, pass 26 — Hasty now actually hurries. You said it moved across the screen but was "not very hasty", and asked for it in the face and in the speed. Both are here. Nothing else on the page changed.

FIRST, WHY IT DID NOT LOOK HASTY, BECAUSE THE ANSWER IS THE WHOLE PASS. Hasty's travel had been DELETED fourteen passes ago — and the camera was never told. Pass 12 removed the creature's forward journey because it was only there to feed a motion smear that was also being removed. But the camera move that had been added to FOLLOW that journey stayed. So the camera went on sweeping across a creature that was standing still, which slides the creature AND THE GROUND UNDER IT across the frame together.

That is why you could see motion and still not see speed. Speed is not the picture moving — it is the ground going past the body. Measured, the creature crossed the screen at 0.68 pixels a frame and the ground crossed at 0.75, so the two were sliding as one piece: the ground-relative speed cue was SIX HUNDREDTHS OF A PIXEL A FRAME. Over the whole four seconds that adds up to about sixteen pixels. It was, precisely, a camera pan over a creature holding still. It is now 3.05 pixels a frame.

THE TRAVEL IS BACK, AND THIS TIME IT GOES THE RIGHT WAY. The old journey ran along the world's X axis, but this camera sits at a fixed three-quarter angle, so X is half sideways and half straight INTO THE LENS. Travelling along it, the creature grew and sank as it went, like something running diagonally at you, and no amount of camera following could ever hold it — the sideways half can be tracked and the depth half just changes its size. The travel now runs along the axis that is actually across the screen, so the creature holds its size and its height the whole way. It crosses 114 pixels of frame with 45 pixels of margin on the left and 93 on the right, and it never touches an edge on any of the 240 frames.

IT ALSO MOVES LIKE SOMETHING IN A HURRY, NOT JUST ACROSS A FRAME. The bob goes from 5 beats in the clip to 13 — about three a second instead of a bit over one. That was picked off a ladder: 9 ambles, 17 starts jittering the antenna frame to frame and tips over from hurried into panicked, 13 drives. And the lean is no longer a POSE. It had been one fixed tilt held for the entire clip, which reads as an attitude rather than as pushing against anything; there is now a surge on it, timed so the body digs in just BEFORE it rises. Turn the surge off and you can see the old version: only the antenna moves and the body just leans. Turn it up and it stops driving and starts lurching.

THE CAMERA CAME IN, AND THAT IS WHAT MADE THE FACE POSSIBLE. Hasty had inherited a camera pulled right back for that 8.4-metre journey it was no longer taking. The creature's outline was ONE OR TWO PIXELS a frame. There was simply no face there to act with — which is why the expression half of what you asked for could not have been done without moving the camera first. It is now nearly twice as close.

THE FACE IS HURRIED, AND IT IS ITS OWN THING RATHER THAN A BORROWED ONE. The eyes are held NARROW and driving — narrower than anything Startle or Curious ever reach — with the lids squinted into the wind and the brow tops drawn TOGETHER. Tops-together is the effort sign; Startle's payoff is the tops flying APART, which is alarm, and that is deliberately not what this does. Three times in the clip it CHECKS: the eyes snap wide and the gaze throws sideways for about a third of a second, then settles back to driving. Three short looks, unevenly spaced, so it does not become a metronome. Side by side with Startle and Curious it is plainly in the family and plainly not either of them — Curious is an ASYMMETRIC double-take, Startle is a single wide-eyed event, and this is a STATE with three events cut into it.

Two details worth knowing. Each check now ARRIVES and LEAVES instead of cutting: the first version stepped straight back to neutral on the key after the peak, which at two frames per key is a visible pop across all four face channels at once. And the quiet ambient eye drift you approved two passes ago was never reaching this clip at all — it only applies where a clip uses the eye-size channel, and Hasty never did. Authoring the eyes turns that on, so the ambient layer now sits underneath here too, at exactly the gain it already has everywhere else. Nothing about that layer was changed.

THE LOOP HITCH — YOU ACCEPTED IT, AND IT GOT BETTER ANYWAY, THOUGH NOT AS MUCH AS WE FIRST CLAIMED. You said "it moves across screen, it makes sense it hitches", so this was never the job. But the clip still has to get back to the start, so there is still exactly ONE jump per lap, same as before. It moved one frame earlier and it is about a THIRD SMALLER — 163 pixels down to 111. The figure that matters more is how much it stands out against the clip's own movement: before, the creature was barely moving, so the jump was 238 TIMES anything else on screen, which reads as a teleport; now the creature is visibly travelling and the same jump is 35 times, which reads as a stumble in a stride. The ground also shifts a few pixels at that moment, which on this bare desert you will not see.

SAID PLAINLY, BECAUSE WE GOT IT WRONG FIRST: the pass originally reported that seam as going from 38.8 pixels to ZERO — "effectively vanished". The review threw that out. The measurement it rests on uses a colour rule that, on a frame with the creature painted OUT, still scores 82 per cent of what it scored with the creature in it — so it was mostly measuring the sky. And it only ever compares two frames, which happen to be exactly the two the fix brings into line, while the jump simply moved to where it was not looking. The honest numbers are the ones above. The fix itself is real and worth having; the sentence about it was not.

EVERYTHING ELSE IS UNTOUCHED, AND THAT IS CHECKED RATHER THAN PROMISED. You closed two things last time — Crackle's rear is fine, Hover's front ball is fine — and both are byte-for-byte identical, 600 frames each, not one pixel different. Across the whole bank: 22 clips, 7,992 frames, and exactly 240 of them differ. All 240 are Hasty.

PROVENANCE. Accepted source is zhaozhou {source}. The exact one-binary renderer MD5 is {md5}; bank manifest SHA-256 is {manifest_sha}. All {subjects} live subjects — {frames} frames — were rendered together in ONE invocation with no override of any kind, and every subject kept its exact frame count. The renderer that produced this bank was BUILT FROM SCRATCH BY THE INDEPENDENT REVIEWER, not by the author of the change, and it reproduces the implementation's own 22-subject bank BYTE FOR BYTE on all 22. The reviewer also rebuilt pass 25 from its own commit in a separate tree and fired every control personally: 11 knobs that must change the picture all change it, 17 bad values are all refused rather than quietly clamped, and switching the hurry off reproduces pass 25's Hasty EXACTLY — 240 frames, zero different, matching hash. No bound anywhere was relaxed to make a value fit; the bob amplitude was laddered DOWN against the motion check instead, and a value that passed by three tenths of a millimetre was rejected for passing by that little.

FRAME REVIEW, STATED EXACTLY. Every frame of all 22 clips was built into contact sheets and each sheet was checked to hold its full count — 7,992 tiles, no sampling. All 240 frames of Hasty, the one clip that changed, were looked at, at native size and against the old version. An automatic sweep over all 7,992 frames found no black frame, no frozen frame and no frame missing the creature. The other 21 clips are byte-identical to pass 25, which was already production-verified, so they were not re-judged by eye and this card does not claim they were.

ONE MORE THING, FOUND BY ACCIDENT AND WORTH TELLING YOU. Two of the checks that prove older passes still reproduce had been RED since before pass 25 shipped, and nobody noticed — they passed through pass 25's close, its review and its production check while failing. The cause was harmless (a newer mechanism was never given an off-switch in the older ladders, so they were reproducing the wrong thing) and PASS 25'S ACTUAL PUBLISHED CLIPS WERE NEVER IN DOUBT — four separate rebuilds agree on them, including one made fresh during this review. But a check that reads green while broken is the thing most worth catching, so it was chased to the bottom, repaired, and given a control that fires. Pass 25 is now in the archive, byte for byte, and the archive check was extended to lock it.

A NOTE ON THE VIDEO FILES THEMSELVES. The video encoder is not byte-reproducible: encoding the same frames twice produces two different files of identical length. So every one of the 22 videos has new bytes even where the pixels are identical — which is why containment is proved on the RENDERED FRAMES, never on the video files. One useful cross-check fell out of it anyway: of the 22 encoded files, exactly ONE changed size, and it is Hasty."""

SITE_NOTE = (
    "Every creature is a continuous skinned surface bound to at most 32 bones, "
    "compiled by the reference implementation that defines the console. Manafold "
    "pass 26 gives Hasty its hurry: its travel had been deleted while the camera "
    "went on compensating for it, so the creature and the ground slid across the "
    "frame together and nothing read as speed. The travel is back on the axis "
    "that actually crosses the screen, the cadence is nearly three times faster, "
    "and the camera came in far enough for the face to act — narrowed driving "
    "eyes that snap wide on three checks. Every other clip is byte-identical. "
    "Pass 25 is preserved in the archive."
)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    if manafold["blurb"].startswith("MANAFOLD, pass 26"):
        print("FAIL the card already says pass 26; nothing done")
        return 1
    if not manafold["blurb"].startswith("MANAFOLD, pass 25"):
        print("FAIL the card does not say pass 25; not edited")
        return 1

    md5, manifest_sha, subjects, frames = read_receipts()
    if subjects != 22:
        print(f"FAIL the bank manifest has {subjects} subjects, expected 22")
        return 1

    manafold["blurb"] = BLURB_TEMPLATE.format(
        source=SOURCE, md5=md5, manifest_sha=manifest_sha,
        subjects=subjects, frames=f"{frames:,}")
    manifest["site"]["note"] = SITE_NOTE
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK Manafold live card -> pass 26 (renderer {md5}, manifest {manifest_sha})")
    print(f"OK bank {subjects} subjects / {frames} frames")
    print("OK site note -> pass 26")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
