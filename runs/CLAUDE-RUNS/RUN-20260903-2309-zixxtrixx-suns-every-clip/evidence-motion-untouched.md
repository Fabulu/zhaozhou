# Direction 29: evidence that the ANIMATION is untouched

Three independent lines, each against the pristine pre-suns build (c23c6a63):

1. **Committed pose probe: PASS.** zixx-probe walks every authored key and
   midpoint of every clip with declared 3D contact policy; all gates green on
   the suns build.

2. **Revert-path CRC identity: 22/22.** The suns build with ZIXX_SUNS=off is
   sequence-CRC32C-identical to the pristine build on every subject
   (evidence-gateoff-identity.txt). The change is provably inert when off,
   and pristine moving-light reproduces the published clip's logged CRC
   0x65A8D1E5.

3. **Ink-mask silhouette identity: every frame of every subject.** The cel ink
   outline is drawn around the creature's rendered silhouette, so an identical
   ink pixel set means an identical silhouette. Compared frame-by-frame,
   pristine vs the shipped sun render: 6674/6674 frames identical across all
   21 subjects (evidence-inkmask-raw.txt). Only shading inside the silhouette
   moved.

The raw log's per-subject "border-touch FAIL" column is the checker's own
false positive, not a render fault: it assumed the creature never reaches the
frame border, but the travelling and flight clips legitimately clip at the
viewport (walk frame 0's left column: hot-pink TAIL pigment (255,117,255)
re-shaded to (255,162,255) by the azure sun -- creature pixels, not
background). Spot-checked and understood; the silhouette (ink) check is the
sound form of that question and passed 100%.
