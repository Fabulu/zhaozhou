# SPEC v1 — Zixxtrixx round skull + Presentation V2 (smoothness priority)

Run: RUN-20260827-2140. Base commit: f8f6681a30d35ab2f0d527dfd23d2a5e0377281e

## Objective (owner-ordered priority)
0. WAVE 0 — snapshot goldens BEFORE any change (walk+attack clip bytes,
   per-key pose CRCs, full probe output, 60 Hz contact sheets, commit hash)
   into Upheaval/creature/Zixxtrixx/golden/.
1. HEAD (small, blocking): the cranium becomes a BALL — grow rz with rx at
   the eye stations, cut kEyeBulgeNum hard; ease the swell in/out so the
   skull grows out of the neck (no cliff at station 11). Acceptance =
   renders beside Concept/Front.png and Concept/Side.png.
2. DORSAL PINK (owner add-on): pink covers the WHOLE upper surface — what
   you'd see from straight above (~40-50% of circumference centred U=192),
   body AND head crown; judged from renders at the current ~15 deg camera.
3. V2 SMOOTHNESS (owner: "most important thing"):
   a. Direct RGB565 + bilinear + mips creature pages through the reel's
      sampler, bit-matching TEXTURE.TMU.md's frozen law. Zero silicon.
   b. Poly reduction with better distribution: ~1,400-1,800 tris, variable
      radial detail (zipper stitches unequal counts), fins as real topology.
   c. NO Gouraud in zref (charter §20: zref is the oracle; coordinator
      correction 2026-08-27). Instead: a clean costed proposal document.
4. As much further V2 (choreography etc.) as lands well.

## Constraints
- Walk, salto trajectory, idle body movement: FROZEN. Regression-pair method.
- Positive head pitch = nose DOWN; kHeadAttitude -12000; retune only by sweep.
- zixx_probe must exit 0; --check must end "all sequence CRCs match".
- Vertex-coincident head/body junction stays (no overlay shell).
- Build directly with g++, never cmake. Another lane owns
  tools/sweep_raster_resolve.sh + captures/failures — do not touch.
