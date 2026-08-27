# FINDINGS — RUN-20260827-2140 zixxtrixx-round-skull-and-v2

## Delivered
1. **Goldens** (Wave 0, before anything): Upheaval/creature/Zixxtrixx/golden/
   — clip bytes, per-key pose CRCs, probe output, 60 Hz contact sheets,
   source commit. Dumper committed (tools/reel/zixx_golden.cpp).
   Clip payloads verified BIT-IDENTICAL after every subsequent wave.
2. **The round skull** (kBallNum every-axis swell, envelope to zero at the
   junction — no cliff; eye bulge 85 -> 22). Attitude re-swept: -12000 keeps.
   Allowances re-authored on worst-key renders; probe exit 0.
3. **Whole-top dorsal pink** (body half 4.5 -> 13 texels; head crown to
   match; judged at the current cameras).
4. **Eyes forward on the ball** (EYE_ROW 19 -> 12), **pupil vertical**
   (EYE_ROT -30 off a painted-tile fan + reel confirm), colour judged AFTER
   the format change (stronger for free — the shared palette was the cause).
5. **Gouraud in the oracle**: SkinVertex s8x3 normal lane (flat fallback at
   (0,0,0)), compiler-generated seam-safe normals (micro recomputed),
   per-bone-Lambert-blend law, 0.8/0.2 smooth/face knob, three colour lanes
   on the per-row barycentric model (kept deliberately — RTL row walker owns
   the division). creature_rules amendment written; silicon costs stated.
6. **Direct RGB565 + bilinear + mips** through Tmu::sample itself
   (DirectPageSet; repeat U / clamp V; per-triangle req_lod upstream).
7. **Poly diet 3,680 -> 2,076** (skull 22 / neck 18 / trunk 16 / tail 10 /
   blades 6x6). Trunk 16 for the belly-line vertex — ground contact kept.
8. **ChoreoRoot (C1)** + committed proof (zixx_choreo.exe): palette sharing
   pointer-identical; spin-migration recomposition matches golden to 2 mm
   worst over 220 keys. Shipped clips untouched.
9. Site renders re-encoded (canonical names, 60 fps); archive copies intact.

## Gates at close
- zixx-probe: exit 0; walk [-13..+10] EXACT, attack -426 @56 EXACT, fall
  min 584 EXACT, idle [-8..-2] (1 mm shallow-end chord effect, declared).
- zhao-reel --check: all sequence CRCs match (two creature subjects
  re-pinned once, for Gouraud, with provenance).
- zixx-choreo: exit 0.
- Pose CRCs + all four clip payloads: bit-identical to golden/.

## Not done (honest)
- T5 multi-scale crayon converter (bilinear softens grain at distance).
- T4 128x256 atlas; C2/C4-C7, F1/F2, A1, W1; P1/P2 extensions beyond the
  overlap probe.
- Open owner calls: front-on orange eye surround; idle shallow-end -2 vs -3.
