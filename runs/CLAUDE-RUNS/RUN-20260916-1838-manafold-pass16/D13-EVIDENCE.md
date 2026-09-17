# Direction 13 evidence — folded lightning, independent particles

**Date:** 2026-09-17

## Accepted implementation

- The folded lightning keeps the existing `place()` transform and all depth-tested navy/shimmer/white drawing.
- Shape motes now ship in an independent ring-relative cloud/orbit frame. `kFoldMoteShapeFollowPm = 0` is the owner-facing shipping knob; `ZHAO_U02_MOTE_SHAPE_FOLLOW_PM=1000` reproduces the final3 coupled control from the same binary.
- Shape vocabulary expands from 9 to 12 with DIAMOND, INFINITY and HEART.
- Fold phrases are shorter but retain drift, gather, hold and knead.
- One independently hashed third of eligible holds makes one eased complete in-plane turn, clockwise or anticlockwise. Lasso retains only its separately authored throw spin.

## By-eye evidence

- `d13-particle-decouple-ab-v3.png`: same-binary control/candidate. The Lasso lightning loop and its world translation remain intact while its motes travel as an independent cloud rather than winding into the loop; Channel's motes stop inheriting the lightning line's orientation.
- `d13-channel-full-turn-v3.png`: frames 154–203 show the connected lightning CROSS make a complete anticlockwise turn. The surrounding particles keep their own local motion while the line turns.
- `d13-topology-morph-v3.png`: source-only edges fade out and destination-only edges fade in; CROSS changes continuously into INFINITY without the old wrong hold topology or a midpoint pop.
- `d13-channel-infinity-v3.png`: the shipping Channel schedule visibly lands on the new INFINITY figure.
- `d13-heart-full-turn-v3.png`: pinned diagnostic through the shipping path proves the new HEART remains connected and readable throughout a complete turn.
- `d13-shape-manafold-rest-v3.png`: the new DIAMOND appears in an ordinary shipping clip, not only a pin diagnostic.

The accepted swirl/connected-lightning read is preserved in every plate. No particle position feeds back into lightning geometry.

## Exact binaries and attribution

- Final reviewed candidate/control binary MD5: `1255A8F8DEE778F7E76DCC7759D678B0`.
- Candidate sequence CRCs: Lasso `0x88B892BD`, Channel `0x915E343F`, Hover `0x571C3D79`.
- Focused candidate renders: Lasso 336 frames, Channel 420, Hover 600; all exited 0.
- Same-binary `follow=1000` controls for the same three subjects all exited 0.
- `U02_FOLD_SHAPE=12` returns 2 before rendering; the `0..11` shape-pin guard was fired deliberately.

Shipping render environment:

```text
ZIXX_EXP=celmain
ZIXX_LIGHT=diagonal-cool-cross
```

## Independent review corrections

A targeted GPT/Codex review found three real issues before commit:

1. The legacy edge drawer selected destination topology even while the source shape was held. It now keeps shared edges full strength and continuously crossfades source-only/destination-only edges across the morph.
2. The first independent-particle cut left Lasso motes at the creature. The independent field now inherits only world clearance/translation, including the Lasso flight, while remaining free of scale, spin, skew and shape morph.
3. The diagnostic shape pin accepted out-of-range indices. Both environment input and the internal consumer now guard `0..11`; the invalid-12 positive control fires with RC 2.

`d13-topology-morph-v3.png` and `d13-particle-decouple-ab-v3.png` are from the corrected exact binary above.

## Regression gates

Fresh direct builds and executions all returned 0:

- `manafold-probe`
- `manafold-nodule`
- `manafold-spangate`
- `manafold-shellgate`
- `manafold-shellgate --selftest`
- `manafold-bandprobe`
- `manafold-meshcheck`
- `manafold-eyecam`

The full 28-subject bank remains required before publication; these focused plates accept the Direction 13 mechanism and art direction, not the final pass.
