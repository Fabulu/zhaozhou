# Manafold Pass 17 — held-punch A selection

**Date:** 2026-09-19
**Question:** remove Taunt III's 53 real F–A budget violations without weakening the held accusation
**Same-binary renderer:** `FC5A02949DE0BEE5D419BC24C3E9F67D`
**Held-aware mspan:** `CD4E313B43F55729879AF29D85DEA90E`

## Verdict

**Selected by eye: held A = 80 mm.**

The broad dismissal, Front/End `+90/-90`, B `-180`, C `+160`, front-held body `0/0`, lean `150`, eyes `1450/750`, crown shuffle and all smooth-motion timing remain unchanged. Only the held A term changed from the rejected `140 mm` pose.

## Structural filter before looking

`ZHAO_U02_TAUNT3_PUNCH_A_MM` is a strict same-binary control in both the renderer and `mspan`; malformed input returns RC 2. The fixed signed-span limits were not widened.

| Held A | mspan | F–A shipping maximum | f0324 named A mute | Disposition |
|---:|---:|---:|---:|---|
| 60 mm | RC 0 | +199 mm | 60.82 mm | legal, visually quieter |
| 80 mm | RC 0 | +199 mm | 81.10 mm | **selected** |
| 100 mm | RC 0 | +214 mm | 101.39 mm | legal but only 3 mm below the +217 mm ceiling |
| 140 mm | RC 1 | +253 mm | 140.99 mm | rejected: 53 F–A violations, f0300–f0352 |

All three legal candidates retain both signs on all four spans, zero reversed/pinched rings, the four crown tableaus and pair crossings, F/End attachment, closure and the five held-punch carrier reads. The `140` control fails only the real signed budget.

## By-eye decision

The complete 368-frame sheets and native held witnesses were viewed before selecting the source value. `60` remains coherent but gives A less visual authority in the accusation. `80` keeps the A lift clearly distinct beside the held B/C opposition and preserves the same readable face, open O and continuous lightning. `100` adds little useful read at native resolution over `80`, while sitting unnecessarily close to the fixed span ceiling. The picture chose `80`; the structural margin is comparison evidence supporting that choice, not its generator.

No candidate introduced a visible carrier snap, span buckle, socket slide, eye loss, lightning off/reappear, brightness seam or outline regression. The selected hold remains smooth through arrival f0300/f0304, witness f0324, late hold f0344 and release f0352.

## Evidence

- `PASS17-HELD-A-LADDER-A60-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-A80-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-A100-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-A140-ALLFRAMES.png`
- `PASS17-HELD-A-LADDER-WITNESSES-NATIVE.png`
- `PASS17-HELD-A-LADDER-4X.png`

Exact same-binary gate logs live in the authoring output as `mspan-a{60,80,100,140}-final.log`; the durable report records their relevant receipts without promoting the output directory itself.

## Source result

```cpp
kTaunt3PunchFrontMm = 90;
kTaunt3PunchAMm = 80;
kTaunt3PunchBMm = -180;
kTaunt3PunchCMm = 160;
kTaunt3PunchEndMm = -90;
```

The five values are named individually. Shipping uses `80`; the strict A override remains available for reproduction of all four rungs.

## Remaining integration

This ladder predates the checker owner's final frozen hash after held-punch metric/report bookkeeping. Art/clips/renderer source is frozen. One clean post-freeze build must rerun normal `mspan`/`msmooth` and every attributed mutant, focused geometry/contact/eye/outline gates, then reproduce the selected complete sheet before this evidence is final or committable.
