# SPEC v1 — Zixxtrixx colour-light repair (Owner Direction 27)

## Objective
The published moving-light subject shows four orbs but the owner cannot see
their light: no colours on the animal, no mixes, possibly only the warm lamp
doing anything. Phase 1: PROVE each source illuminates and that marker ==
light position, with committed evidence. Phase 2: make blue/orange/green
obvious at native 384x240 and make overlaps read as mixed colour.

## Constraints
- build-direct.sh only, one target at a time; never cmake --build.
- Render with explicit ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross.
- Only zixxtrixx-moving-light may change; CRC-prove the other 21 subjects.
- No animation edits. Four sources exactly (hardware budget). No fifth.
- Every colour/radius/gain/extent/period/phase stays a named constant.
- .rgb readers must verify the 8-byte w/h header (four prior diagnostics lied).
- Archive the current live bank as generation Seventeen before re-encode.

## Phase 1 evidence plan
1. Baseline: build at lane HEAD, render moving-light, reproduce published CRC.
2. Solo diagnostics: env-gated ZIXX_ML_SOLO (default off = byte-identical)
   isolates one source at a time; extreme-gain variants via ZIX_ML knob.
   Before/after difference images per source, from header-verified frames.
3. Marker/light unity: solo source at high gain, track pool centroid vs orb
   marker pixel across frames.
4. Trace the shade path (done statically): PointShade3 sums per channel ->
   creature_light points_active mix -> quant_shade clamps each channel at
   1.0 -> Gouraud corners -> per-fragment apply_toon_ramp (mean-thresholded
   3-band, ratio-preserving) -> texel multiply -> sat_u8. Hue survives in
   RATIO; clamps and band levels can erase it. Verify empirically.

## Phase 2 plan (after proof)
Rebalance warm lamp headroom; strengthen coloured gains/radii; re-author
paths for guaranteed dwell-time overlaps; judge by eye at native res on true
frames against pigment and terrain; iterate.

## Finish
Fresh explicit 22-subject render; stale-frame sweep; CRC proof (21 identical,
1 changed); encode; archive gen Seventeen (check creatures.json +
MAX_ARCHIVE_GENERATIONS + both CSS families); clip note; merge mains, no
force; probe; deploy once; verify; stop background jobs.
