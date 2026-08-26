# FINDINGS R2 — art-direction pass (fable agent)

Input: Fabian's review of the remake ("a giant mess") plus two mid-pass owner
corrections (author by eye / authored ground contact; measured pigment is a
hue reference, not the shipped colour). Full narrative in
`Upheaval/creature/Zixxtrixx/WORKLOG.md` § "Art-direction pass (fable)".

## Root causes found by rendering + probing

1. **U-seam smear** (mangled face): ring closing face interpolated u 2xx->0
   backward across the whole tile. Fix: per-ring u=255 duplicate vertex for
   textured parts, `creature_core.cpp`. Untextured parts bit-identical;
   `reel --check` all CRCs unchanged.
2. **"Spinning disc"**: full-radius nose ring + flat cap fan (U sweeps a turn,
   apex holds one value). Fix: kNoseDome + flat-pigment cap sample rows.
3. **Eye footprint**: 26x26 texels = 40% of head circumference per eye.
   Fix: 12x26, transposed (slit stays vertical), high on the crown.
4. **Flat texture**: green grain box was 77% bare paper (median-flattened) and
   the ±16% clip was under the light rig's range. Fix: probed boxes (sd
   0.057->0.086), amplitude 2.1x.
5. **fx16 root-lane bug**: every clip root value was authored in mm but the
   lane is fx16 metres — all root motion (idle bob, attack leap/dive) was
   1/65536 of itself, i.e. nonexistent. Fix: fxm() everywhere; this single
   bug explains "doesn't bob" and much of the salto's failure.
6. **Blades gargantuan**: 1180mm -> 480mm; one pink, one green per sheets.
7. **Stance**: slope-table S with the apex ABOVE the skull; taper HAND-SET in
   KNOBS (profile header demoted to comparison, not compiled).

## Measured ground contact (pose probe, scratchpad zixx_check.cpp)

- idle  −17..−19 mm authored sink, belly excursion ~2 mm over the breath
- walk  −14..−17 mm sink, hump arches up only, ~zero skate (963 vs 880 mm/cycle)
- attack airborne keys 9..51, −292 mm authored bite keys 52..56
- fall  min +319 mm, airborne throughout

## Commits

- zhaozhou 0a4a587 seam fix; bceeaf4 page; db31848 model+clips+notes
- Upheaval b4bdb08 WORKLOG + PALETTE.md status correction

## Open

- Eye yellow slightly dim under key light at some angles (one knob away).
- Site videos stale; regenerate after sign-off.
