# FINDINGS — Manafold pass 6 ARCHITECT run

**Deliverable:** `Upheaval/creature/Manafold/PASS-6-ARCHITECTURE.md` — the ordered,
dependency-explicit work plan for the implementer. This file records only what THIS
run established by evidence; the plan itself lives beside the creature, per the
"run folders are the wrong home for anything durable" law.

## Settled by render (probe subjects; patch in evidence/, applied to lane clone only)

1. **The fog IS the smear plane.** Rest clip ablated: pale grey-white px at f200 —
   baseline 1167, smear-off 314, all-off 46 (~73% smear plane, ~23% mote halos, ~4%
   creature). Looked at on `fog-triptych-f200.png`: the gassy cloud is gone the
   moment the smear is off. Consequence: Direction 5 §3's thickened fog is
   re-founded as a creature-owned shell with its own knobs (stage D.2), delivered in
   the same unit as the smear-plateau fix (E.5), or the plateau fix thins the
   owner's promoted feature.
2. **Camera variant D (cam_k 240k -> 360k) adopted.** `channel-360-pair-*.png`:
   nothing crops, mana pocket becomes readable, eyes ~double. Aqua px 602->636,
   sat spread 117.6->129.9 at f210 (comparison side).
3. **`channel`'s mana SURVIVES the shipping moving rig** — first render ever of that
   combination (`channel-ml-pair-*.png`): strands stay strands, sat spread 107-127 vs
   baseline 118-119. The body swings toward hot red under the passing warm sources —
   the pink clip-fraction must be re-measured under the new rig before the stage-D
   light-gain fix.

## Settled by looking at the sheets

4. **The eye pop-out HAS sheet support** — `Concept/Description.png` inset
   ("abstehendes Auge schraeg von hinten betrachtet") shows the lens breaking the
   body silhouette. RECON-P6-ART §2.5 checked only Front/Side. "Slightly" remains
   the magnitude spec.

## Decisions recorded (architect's three)

- Camera: TAKE 360k as named `kU02CamK`; per-clip overrides stay; traverse clips
  never inherit it blindly.
- Trail/bob: CONFIRM 0-TER default option 1 (bob AND traverse, reframed), plus two
  hard rules: hasty's camera stays fixed (tracking kills the trail by construction),
  and the traverse magnitude is protected — reframe, never slow down.
- Order: instruments -> judging frame (rig+camera) -> structure (eyes, one-skin
  body) -> antenna motion -> light/pigment -> mana/smear/fog -> clips -> close.
  Full rationale in PASS-6-ARCHITECTURE.md.

## Hygiene

- No shipped constant changed; probe subjects reverted from the lane tree after the
  patch was captured. Nothing published. Hardware lane, pass-5 lanes and recon lanes
  untouched.
- Renders made under explicit `ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross`,
  BUILD_RC=0 read directly, frames read via `rgbframe.py` (selftest PASS).
