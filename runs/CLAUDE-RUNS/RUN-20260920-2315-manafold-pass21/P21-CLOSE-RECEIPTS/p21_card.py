#!/usr/bin/env python3
"""Write the pass-21 live card, the site note and the affected clip notes.

Plain owner language. Every number in the PROVENANCE paragraph is a receipt this
pass produced and can hand over, and the frame review is described as exactly
what it was -- no more.

Idempotent-ish: refuses if the card already says pass 21.
"""
import json
from pathlib import Path

SITE = Path(r"C:\programmieren\zencrifice\manafold-p16\Upheaval\website")
MANIFEST = SITE / "creatures.json"

BLURB = """MANAFOLD, pass 21 \u2014 the antenna is ball-and-stick now: straight rods, and every bend at a ball.

THE ANTENNA HAS THE RIGHT NUMBER OF JOINTS. The owner's words were that it read as having too many joints, and that each run between the round carriers should read as one piece. It does now, because the rig was rebuilt rather than tuned. Every segment of the antenna is a STRAIGHT ROD that can only stretch and shorten \u2014 it cannot bend anywhere along its length \u2014 and all the bending happens at the balls. That is what removed the extra hinges: the old rig let a run curve in its own middle, and it put one to three extra corners just BEFORE each ball rather than on it, so the eye counted seven or eight joints where the creature has four. Put the two side by side and pass 20 has no structure to read at all in half the orbit; pass 21 reads as four straight runs meeting at four round knuckles, in every frame.

THE REAR END IS CALM. The owner also said the END part still flicked about. It was not the End: it was pass 20's own repair. The band that runs back into the body was made to BOW, and the bow swung its tangent around as the band went slack, which the last run then had to follow \u2014 rendered as a jerky polygon. Under the new rig that run is simply the straight line between its two balls and there is no bow to follow. The worst turn RATE on that run, over every frame of every clip, goes from about 18.8 degrees per sample to 0.02 \u2014 and the worst total bend across the whole rear from 113 degrees to 18. Plotted against each other, pass 20's trace is a field of erratic spikes and pass 21's is a clean, regular wave.

THE KNEADING BEAT IS KEPT. The pass-20 press is untouched and still reads: the ball at the middle of the top of the loop drives down into the body, the loop's window collapses to a wedge, the mana flattens and rides down with it, and the whole thing comes back up. It reaches its deepest press at the same moment in the animation as it did in pass 20.

THE BALLS ARE BIGGER, AND THAT WAS DECIDED BY EYE. At the size the rig first produced them, the joints did not read \u2014 the antenna looked like a bent wire with no visible knuckles, on a frame where every measurement passed. They were enlarged by 1.4 times against the concept sheet so that the ball, not the rod, carries the change of direction. One ball, the middle one, is a little smaller again because it is the one that touches the ground in the planted trick, and it was sized to the contact it makes rather than to the others. No gate was loosened for either decision.

EVERYTHING ELSE IS PASS 20. The kneading beat, the mana's answer to it, the repaired rear connection, the live mana, the eyes, the planted Trick turn, Flight's climb and Drift's framing all carry forward. Pass 20 itself is now in the archive, byte for byte.

PROVENANCE. Accepted source is zhaozhou bd29d4f9. The exact one-binary renderer MD5 is fe1bab84ae82b7aa6efac5432413abc4; bank manifest SHA-256 is 639c370cf32cb777e63896af064fc56a0d48f01edf354f2ee15d68e1ea395633. All 22 live subjects \u2014 7,992 frames \u2014 were rendered together in one invocation with no override of any kind, and passed the bank integrity check. All 22 changed against pass 20 and every one kept its exact frame count, which is what a rig change should look like. The same renderer with the new rig switched off reproduces the pass-20 bank EXACTLY on all 22 clips, so everything that changed comes from the rig and nothing else. Every frame of all 22 clips was looked at on complete contact sheets, and six subjects were looked at close up: the whole Inspect orbit, the three sharpest joints in the bank, the deepest knead press, and Trick's planted contact. The frames for those close looks were chosen by how BAD they were \u2014 the worst joint angle and the deepest press, computed from the rig's own per-frame output \u2014 not by picking evenly spaced ones. Sheet scale cannot show fine detail on the other sixteen clips; there, joint placement and line weight rest on that exact comparison and on the gate matrix, which is green in one run with every control fired and attributed. Two things are deliberately NOT claimed from the pictures: that the antenna reads smooth on clips only seen at thumbnail scale, and the ground penetration on the planted trick, which is a measurement walking the posed model against the terrain (25 mm, exactly as declared), not something counted off an image."""

SITE_NOTE = ("Every creature is a continuous skinned surface bound to at most 32 "
             "bones, compiled by the reference implementation that defines the "
             "console. Manafold pass 21 rebuilds the antenna as ball-and-stick: "
             "each run between the carriers is a straight rod that only stretches, "
             "and every bend happens at a ball, which is what removed the extra "
             "joints and calmed the rear end. The pass-20 kneading beat is kept; "
             "pass 20 is preserved in the archive.")

NOTES = {
    "Hover": ("Pass 21: the protected orbit is the clearest place to see the new rig \u2014 "
              "four straight rods meeting at four round knuckles, all the way round. "
              "The kneading press is still around frame 532 of this clip."),
    "Inspect": ("Pass 21: this is the clip to open. The runs are straight and every bend "
                "sits on a ball, through the whole orbit; the deepest kneading press in "
                "the bank is around frame 576, where the top of the loop is driven down "
                "into the body and the mana rides down with it. The showcase remains an "
                "honest lighting comparison rather than a duplicate rig path."),
    "Channel": ("Pass 21: the rear run into the body is now the straight line between its "
                "two balls, with no bow, so the End no longer flicks. The mana's answer "
                "to the knead still reads most clearly here."),
    "Trick": ("Pass 21: upside down, the planted support reads unmistakably as "
              "stick-ball-stick \u2014 two straight rods down to one round knuckle resting "
              "on the dirt, with dust. The contact is declared and measured at 25 mm "
              "into the surface, unchanged from pass 20."),
    "Nodule taunt": ("Pass 21: the crown shuffle is untouched, and this clip authors no "
                     "kneading beat, as before. Its End motion is one of the places where "
                     "an authored beat now arrives through a rigid rod instead of being "
                     "absorbed by a bow."),
}


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    manafold = next(c for c in manifest["creatures"] if c["id"] == "manafold")
    if manafold["blurb"].startswith("MANAFOLD, pass 21"):
        print("FAIL the card already reads pass 21; nothing done")
        return 1
    if not manafold["blurb"].startswith("MANAFOLD, pass 20"):
        print("FAIL the card does not read pass 20; not edited")
        return 1
    manafold["blurb"] = BLURB
    manifest["site"]["note"] = SITE_NOTE

    hit = 0
    for r in manafold["renders"]:
        if r.get("archive"):
            continue
        if r.get("label") in NOTES:
            r["note"] = NOTES[r["label"]]
            hit += 1
    if hit != len(NOTES):
        print(f"FAIL updated {hit} clip notes, expected {len(NOTES)}")
        return 1

    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8", newline="\r\n")
    print(f"OK card -> pass 21, site note updated, {hit} clip notes rewritten")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
