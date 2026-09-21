# P23 reviewer look notes

## Plate 02 (four clips, dip bottom, 4x) -- BEFORE/AFTER
- drift s1 730->900 f191: BEFORE a flat horizontal open loop, reads as a bright
  smear lying across the antenna. AFTER a clear diagonal wedge, loop tilted up,
  green pocket standing upright. Plainly turned. Still a loop, still lightning.
  KNEAD, not spasm. VERDICT: good.
- damage s14 635->780 f333: BEFORE broad upright near-symmetric double loop.
  AFTER tilted spiral, right wing flatter, inner curl tightened, green core now
  a distinct bright lozenge. Large but COHERENT -- whole figure moved together,
  identity carried. VERDICT: good.
- deathdrop s17 635->740 f2: BEFORE compact angular kite hugging the arm. AFTER
  a longer blade across the top with a sharp left hook, still anchored on the
  arm. VERDICT: good.
- gutter s18: ⚠ PANEL IS LABELLED "590->680", NOT THE SHIPPED 720. The AFTER
  here is only subtly lower/kinked vs BEFORE. MUST look at the shipped 720.

## Open at this point
- Plate 02's gutter panel does not show what shipped. Check plate 04 ladder and
  render 720 independently.

## Plate 04 (gutter ladder 590/680/720/760, 4x) + MY OWN RENDER at f111
Reviewer rendered manafold-death-gutter at 590/720/760 from a self-built
binary and cropped with the committed tools/reel/plates.py.
- 590: arm held high-left, lightning a broad flat open loop, airy gap to body.
- 720 SHIPPED: arm plainly pressed DOWN and angled into the body; loop turned
  and tightened into a narrower tilted band; teal pocket appears; strand kinks.
  Reads as PRESS-AND-ANSWER. A knead. NOT a spasm.
- 760: arm continues down and the antenna FOLDS OVER toward the body; the arm
  silhouette loses its shape. Reads as BUCKLING. Backing off was correct --
  reviewer's eye independently agrees with the implementer's call.
VERDICT gutter: good, 720 well chosen, 760 correctly rejected.

## Blown (slot 20) -- reviewer's own render, 815 SHIPPED vs 500
Badness-sampled 815 vs 500 over all 292 frames; worst frame f138 (1054 px
changed >32). At NATIVE the two are indistinguishable: Blown hurls the creature
small and tumbling, antenna tucked under the body, the whole strand ~25 px.
At 6x the difference is a slightly different quadrilateral and a small rotation.
⚠ FINDING: the implementer's "500 pm is the first setting that would READ" is an
over-claim taken from R7's 15.30 deg, not from looking. Looked at, 500 does not
read at native either. This STRENGTHENS the exemption: the prize for breaching
the owner-named B-lowest constraint would not be visible anyway.
Blown shipping crc32c=0x16532469 -- matches the report, and slot 20 is untouched
so Blown is byte-identical to pass 22.

## PUBLISH-PHASE FINDINGS (reviewer, 2026-09-22)

### F-A: pass 22's BANK MANIFEST CRC column does not describe what pass 22 shipped
My p23 bank gives hover sequence_crc32c=0xEFCCD8FA. Pass 22's
P22-BANK-RECEIPTS/bank-manifest.txt says hover=0x89EA99A9, and on that basis
ALL 22 subjects "changed" -- which is impossible, because hover is untouched.
Three independent records agree with ME and against that manifest:
  1. pass 22's OWN P22-RECEIPTS/identity-crcs.txt: shipping hover = 0xEFCCD8FA;
  2. my exact-off run reproduces 0xEFCCD8FA;
  3. ⚠ decisive: encoding my p23 hover frames produces a poster PNG
     byte-identical to pass 22's PUBLISHED manafold-hover.png (cf125a35...).
     A lossless poster identical => the FRAMES are identical.
Also confirmed subject set/order does NOT affect the CRC (hover rendered alone
gives the same 0xEFCCD8FA as hover inside the 22-subject bank), so that is not
the explanation. Pass 22's manifest_sha256 was published as PROVENANCE in
P22-PRODUCTION-VERIFY.md, so a number that describes nothing was shipped as a
receipt. Pass 23 must not compare against it.

### F-B: THE WEBM ENCODE IS NOT DETERMINISTIC
The SAME 600 hover frames encoded twice gave
  07d71ce06939ab360fbefeaf794cda033294759a2cb09fd05377d7a8110d8c09
  82c38a97b6a41c38f10e22598654c87063ae3ef8a33fcce2bdf3eead320a3483
-- different bitstreams, IDENTICAL length (3,852,570 B). So a webm SHA-256 can
never prove "this clip did not change"; only the FRAMES and the lossless poster
PNGs can. The scope proof must be done on frames. (The poster PNGs ARE stable
and byte-reproducible -- three of three matched pass 22.)

### F-C: a poster is NOT a positive control for a changed clip
manafold-drift IS a raised clip, yet its poster PNG is byte-identical to pass
22's. The poster is frame 160 and drift's press window is around f191, so the
frame simply sits outside the reaction -- which is correct behaviour (R7 asserts
the reaction is exactly 0 outside the window) but means a poster comparison
would have called a changed clip unchanged. Frames, not posters.

## BANK REVIEW (reviewer, own render, own frames)
- SCOPE: exactly 4 of 22 subjects moved -- damage, death-drop, death-gutter,
  drift. The other 18 are BYTE-IDENTICAL, 7,992 frames compared. RC 0.
- TRAJECTORIES: all four rise into the press and return to exactly zero.
  Drift's first AND last frames are unchanged -> the loop seam is exact (drift
  is the only looping clip of the four). Death-drop and death-gutter start
  nonzero at f0 -- both are one-shot death clips whose pose already sags at the
  start, and neither loops, so that is correct. Damage shows two press humps
  with a flat zero between them.
- THE DAMAGE f414 SPIKE, INDEPENDENTLY REPRODUCED: peak_any 10,886 px at f0414,
  exactly the implementer's number. The VISIBLE series does not spike there at
  all -- the orange line is flat through it. Looked at f0414 side by side at 4x:
  the two are essentially the same picture. Sub-perceptual dither shift on the
  body fill, NOT a pop in the mana. The implementer's diagnosis is right.
- WORST VISIBLE FRAME bank-wide is damage f0328 (4,055 px). Looked at 4x:
  the same single loop, same white core, same navy backing, rolled and curled
  into a tilted spiral with the green pocket now a distinct bright spot. A
  coherent MORPH, not a switch. No tearing, nothing detached, no artefact.
  The eight worst visible frames bank-wide are all damage f0325-f0335, i.e. one
  continuous stretch of one press -- not scattered, which is what a defect
  would look like.
NO FAULT FOUND. Bank accepted.
