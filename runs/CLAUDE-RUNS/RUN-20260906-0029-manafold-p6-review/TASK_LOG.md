# Task Log: RUN-20260906-0029 - [Describe objective here]

**Created:** 2026-09-06 00:29 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260906-0029-manafold-p6-review/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-06 00:29 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260906-0029
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

# Progress Timeline — by-eye REVIEW of Manafold pass 6

## 00:29 — lane set up
Own clones at `C:\programmieren\zencrifice\manafold-p6-review\{zhaozhou,Upheaval}`.
Did not touch `zhaozhou/`, `Upheaval/`, `manafold-p6-impl/`, `manafold-mana-lab/`,
`manafold-p6-architect/`, `manafold-p6-recon-*/`, `manafold-pass5-*/`, `manafold-p6-qa/`.

## 00:32 — FINDING BEFORE LOOKING AT A PIXEL: pass 6 is NOT PUSHED
`git ls-remote --heads origin` on both repos, after an explicit `git fetch`:

* zhaozhou `origin/main` = `460490c` "Mana lab: the fold reads when it is SEPARATED
  from the creature" (2026-09-05 23:07)
* untitled-game (Upheaval) `origin/main` = `1a02fd9` "Direction 6 RESULT ..." (23:20)

Neither repo has ANY branch carrying the pass-6 implementation. The site at
upheaval.pages.dev serves pass 6 (verified: card blurb says "MANAFOLD, pass 6").
So a build was deployed from a working tree that was never pushed.

Consequence for this review, stated up front: **the shipping source is not
reachable from this lane.** The implementer's tree is in `manafold-p6-impl/`,
which lane isolation forbids. Therefore this review judges **the shipped
artefacts** — the published webms and posters at native 384x240 — which is the
correct object for a by-eye review anyway, and notes every source-level claim as
INHERITED rather than verified.

## 00:45 — instrument proven honest BEFORE any colour call
`instrument_check.py` (committed). Posters are 1152x720 = exact 3x nearest
blowups of native (3x3 blocks constant: PASS). ffmpeg-extracted webm frames
match the decimated poster to mean|delta| 0.93-1.64 (VP9 loss), while the
channel-rotated control lands at 40.1-52.9 — 25-55x worse. So the reader is not
rotating channels, and the check demonstrably CAN fail. Four clips.
Consequence: posters are the lossless source for colour; webm frames carry ~1.5
LSB of codec noise, which is far below any judgement made here.

## 00:55 — first look: plate-01 (all 22 posters), plate-02, plate-03 (6x eyes)
RIGHT, and it should be protected: in `curious` and `hover` the eyes are two
SYMMETRIC POINTED LENSES in a clean Λ, deep violet, each carrying a 4-point
concave-edged CYAN star with a white star tracing it point for point. That is
the Front sheet. The teardrop is gone, the ring is gone, the hue is turquoise
and not navy. The rebuild landed.

WRONG, and it is the headline: in `rest` and `taunt` the SAME eye does not read.
- near eye: the star degenerates into a thin grey-white BAR lying across the
  lens's lower edge, half of it off the purple and on the pink. Reads as a
  scratch, not a star.
- far eye (`rest`): the star has ridden off the purple onto the silhouette,
  sits on/through the black ink line, and has gone YELLOW-GREEN. Reads as a
  sticker on the creature's edge — §5c's own words for the failure.

## 01:20 — MY OWN INSTRUMENT LIED, and it lied the documented way
`bodytrack.py` v1 located the creature as "saturated red-dominant pink". The sky
in these clips is a mauve-pink measuring r-g=69, sat=69 at row 100, against my
threshold of sat>70. The mask ate the sky and reported:
  hasty  body 8455px, centroid moves 38px, 100% of frames edge-clipped
against a contact sheet in which the creature is ~30x40px and crosses the entire
frame. That is EXACTLY PASS-6-INPUTS §9's recorded fault from the last pass
("a creature mask that matched the orange horizon band, claiming 15 of 16 clips
were edge-clipped in every frame"). I reproduced it, having read the warning.
Caught by disbelieving the number against the plate, not by the tool.
Rewritten to locate on the CEL INK OUTLINE (ink lum-sum 65-140; ground 187; sky
327-390, all measured not assumed) with a leak assertion that reports SUSPECT
when the mask exceeds 25% of frame. The v1 numbers are retracted and appear
nowhere in FINDINGS.

## 01:25 — hover vs inspect: the duplicate IS honest, with one nit
All 600 webm frames are PIXEL-IDENTICAL (max|delta| = 0 over the whole clip), so
the caption's "the two clips render byte-for-byte the same" is TRUE, and the
caption states the collapse openly rather than shipping it twice in silence.
NIT: the two POSTERS differ (243,657 of 829,440 px) because they are different
frame picks of the identical clip. Harmless, but it contradicts the caption's
own word "byte-for-byte" for the still.
LOSS TO CHECK: `inspect` was the only subject raising the shipping rig. Nothing
lost its lighting — every clip now has it — but the reel lost a SUBJECT, not
just a duplicate: there is no longer any clip in the bank that renders a
different presentation, so the bank is 22 clips of one rig with two identical.

## 01:30 — the mana lab's headline claim: CONFIRMED, independently, at native
`manalab-edge-snap-held` f281 at TRUE NATIVE 384x240, no zoom: a closed aqua
ring hangs in mid-air, clearly separated from the creature, drawn as a bright
cyan outline with a soft glow. It reads as a RING without being told. The
control `manalab-control-channel` at the same frame has only a sparkle cluster
inside the loop window — no findable shape. So "the first nameable mana shape
this creature has produced" is TRUE and it is the most important result here.
⚠ BUT: the lab reel renders the PASS-5 creature, not pass 6 — its eyes are the
old purple teardrop with a circular white RING around a cyan blob, and its
antenna still has countable balls at the top. The lab predates the pass-6 commit.
So the lab plates are evidence about the MANA MECHANISM only, and must not be
read as evidence about the pass-6 body, eyes or antenna.

## 01:35 — the antenna (plate-09, 6x)
RIGHT: no countable spheres. The band is ONE continuous surface in all four
clips — §2b's own acceptance sentence is met, and the beads are genuinely gone.
The loop is closed around a real window, and the band re-enters the body at the
lower right, so §1's free-floating dongle is resolved structurally.
WRONG: the band is ANGULAR — hard corners, straight faceted runs, a sharp V at
the lower left of `rest` and a kinked zigzag on `curious`. The Side sheet draws
gentle undulations in a flowing band. And I cannot find four knuckles: the swell
has been flattened out along with the beads, which §2b explicitly warned against
("Do not flatten the antenna to a uniform band. That would be as wrong as the
current beads, in the other direction"). The pass landed on the flattened side.
Band is also much thicker relative to the loop window than the sheet draws.

## 01:40 — eyes against the sheet, matched pair width (plate-11)
The construction is right and the proportions inside it are not:
* the star spans ~35-40% of its lens's length; on the sheet it spans ~60-65%
  and nearly the lens's width. `kStarScalePm=950` was meant to keep 95% of the
  drawn size — the drawn PROPORTION is not what shipped.
* the white is a 1px hairline where the sheet draws a bold band roughly as thick
  as the star's own limbs, and on the right eye it has gone cream/tan, not white.
* the star's silhouette is a narrow DART elongated along the lens axis, not the
  sheet's fat 4-point concave star with limbs of roughly equal length.
Together these are why the eye reads as "a purple lens with a glint" instead of
"a star eye". Λ tilt, lens symmetry, purple, cyan hue: all correct.
⚠ `hover` is not a true front view, so lens proportion is not claimed here —
only the star-to-its-own-lens ratio, which survives moderate foreshortening
along the lens's long axis.

## 01:45 — the fog (plate-10, 14x on clean silhouette against open sky)
It does not read. The edge is body pink -> a 3-4px hard BLACK ink band -> clean
flat sky, with no graded translucent band on either side. Direction 5 §3 asked
for it "thickened by a lot ... very visible"; PASS-6-ARCHITECTURE D.2 specified
a 2-3px band at ~double the opacity. I cannot see a band at all at native or at
14x. CONFIRMS the implementer's own declared gap — it is the largest single
unmet owner instruction in the pass.

## 01:50 — motion, corrected tracker (ink locator)
hasty : centroid x span 304px of 384 -> real traverse, and the trail separates.
        0 OFF-FRAME frames (was 29). 4 frames (2%) touch the frame edge at the
        wrap, f234-237. y span 85px with 92 reversals: it bobs AND sinks.
drift : x span 273px, 0% edge-touching. The pass-5 "edge-clipped for 70 frames"
        is FIXED.
fall  : y span 102px, tumbles, stays in frame.
hover : y span 42px, 301 reversals in 600 frames.

## 02:10 — per-frame results in, FINDINGS written
eyescan over every frame of six clips: the star is degenerate (>4:1) on 96.1% of
`taunt`, 78.3% of `taunt2`, 73.8% of `rest`, 51.7% of `hover`, 51.2% of `trick`,
26.7% of `curious`. The good frames score 2.3-2.7. So the bar is the NORMAL
state, not a bad frame. plate-13 samples the worst frame of each clip by badness
and every one shows the same defect.
clipscan over every frame: mana is 55-75% hue-neutral on the shipping clips,
`channel` worst at 74.8%. Pink clipping is now 3.5-5.9% median on fixed-camera
clips — §4 largely delivered — with `channel` the outlier at 14.7% clipped /
27.4% dark median.
Fog settled with a 1D pixel profile (a check that CAN fail, unlike my fog
fraction): sky dither is bit-identical to the last pixel before the ink, ONE
transition pixel after it, then full body value. Absent, not thin.

## 02:15 — closing
FINDINGS.md written: 12 protected items, 14 faults ranked by damage to the read,
verified vs inherited separated, §2a recorded as NOT ASSESSED with its reason.
No creature constant changed. Nothing published.
