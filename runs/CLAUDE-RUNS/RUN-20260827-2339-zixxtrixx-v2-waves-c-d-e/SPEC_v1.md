# SPEC v1 — Zixxtrixx V2: the rest of the ratified plan (Waves C, D, E + P2/W1)

Run: RUN-20260827-2339. Follows RUN-20260827-2140 (Waves 0/A/B + C1 done).
Authority: PRESENTATION-V2-PLAN.md (items T3-T7, C2, C4-C7, F1/F2, A1, W1, P2);
zhaozhou/reports/ZixxtrixxV2amendment WINS conflicts with ZixxtrixxV2.

## Order (the plan's own)
0. P2 first — multi-view diagnostic reel subjects (fixed front/side/tq,
   slow orbit, fullbright, wireframe, normal viz, texture-only). NOT in the
   site library. This is the acceptance gate everything later is judged with.
1. Wave C — T4 (128x256 RGB565 atlas, U=circumference, V=nose-to-tail, fins
   separate page), T5 (multi-scale crayon converter — #1 owner-visible gap:
   grain must survive mips and read at 240p at gameplay distance), T6
   (artistic mips + tooth fade + stable ordered dither), T7 (debug atlas
   BEFORE the real crown), T3 spec amendment.
   + OWNER ASK: orange eye surround + frontal eye presence (ruled YES).
     Judge beside Concept/Front.png on the front diagnostic camera.
   + OWNER RULING: idle -2 mm vs -3 mm sink stays. DO NOT FIX.
2. W1 — DIAGNOSE ONLY (fullbright, smooth-normal, bone/curvature overlay,
   loop-boundary diff at 60 Hz). Expected outcome: NO walk change.
3. Wave D (amendment order) — C2 (phase clips + seam hashes enforced by
   compile_creature) -> C4/C5 (AttackPlanner; ground dive golden, high
   aerial, long forward; then miss) -> C6/C7 (launch retiming + recovery
   rebuild; the ONLY sanctioned golden-clip changes, gated by plan 5.2,
   loudly, with before/after + re-pin) -> F1/F2 (falling: interim S
   authority, then offline deterministic fixed-point spring-chain baker —
   NO runtime physics) -> A1 LAST (baked 60 Hz companion when poses final).
4. Wave E — LOD distance sweep (crown survives, silhouette holds, no pop),
   60 fps canonical renders on disk, raw-vs-encode comparison, full
   multi-view gate. NO deploy.ps1 — owner publishes.

## Laws
- ART LAW: author by eye; render; look; compare vs concept; adjust.
  Measurement only on the comparison side. Named editable constants.
- Goldens are the contract: cmp clip-1..4.bin + pose-crcs after every wave.
  Only C6 (front porch) and C7 (tail extension) may change clip 3 — loudly.
- Never cmake --build. Direct g++. Struct change => rebuild every consumer.
- Determinism: fixed-point, no wall clock, bake offline.
- Gates at close: zixx-probe 0, zixx-choreo 0, reel --check "all sequence
  CRCs match" (REDIRECT to file), goldens cmp-clean (except gated C6/C7
  changes, re-pinned with provenance).
- House style: wobble not jitter; fewer and slower. Head is bone 0 = ROOT.
- DO NOT touch archive-2026-08-27-*; DO NOT run deploy.ps1.
