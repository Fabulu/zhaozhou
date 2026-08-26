# Task Log: RUN-20260826-0617 - Zixxtrixx, first creature

**Created:** 2026-08-26 06:17 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260826-0617-zixxtrixx-first-creature-model/

---

## Objective

Take the first two pieces of concept art (Zixxtrixx, by S. Hofer) and turn them
into a creature that compiles through `zref::creature` and renders at 384x240,
applying the owner's three deviations from the drawing: bigger eyes, blockier
three-prong tail, head more distinct and exaggerated.

---

## Progress Timeline

### 2026-08-26 06:17 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260826-0617
- Initial context: first concept art received; no creature has been built before.

### 2026-08-26 06:20 UTC+02:00 - Reconnaissance complete (no subagents)

Read both concept sheets, all six creature guides, `zref_creature.hpp`,
`compile_creature`, and the reel tool. Located both external references.

**Concept read (Front.png, Side.png):**
- Rounded head, wider than the body, with a pink dorsal cap over a blue face.
- Two large yellow eyes with orange rims; side view shows a vertical orange slit
  pupil. A short straight mouth slit.
- Body: pink dorsal stripe running the whole length, green flanks/belly, blue
  wedge under the chin.
- Tail forks. Side view shows two long prongs plus a short middle nub = three.
- Side sheet carries two small ink thumbnails of the S-curve pose (top right).

**Found:**
- Sacrifice, Steam app 38440, installed at
  `C:\Program Files (x86)\Steam\steamapps\common\Sacrifice`. The SAXS model data
  is right there: `data\saxs*.wad` (14 files, plus `saxshero.wad`, `saxs_wiz.wad`).
- sacengine cloned at `C:\Programmieren\sacengine`, HEAD `10334b8 Volcano shape.`
  It ships `source/saxs2obj.d` -- a model-and-skeleton OBJ exporter, which is the
  tool for taking donor measurements.

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| - | - | none spawned; recon done inline | - | - |

---

## Files Created

- `SPEC_v1.md`
- `TASK_LOG.md`

---

## Decisions Made

*Nothing about the creature itself is decided yet -- awaiting Fabian on the
questions in SPEC_v1 "Open Questions". Findings that constrain those choices:*

1. **The render path already exists end to end.** `tools/reel/zhao_reel.cpp`
   builds a "watchdog" quadruped inline from `zc::RingPart` + `zc::Skeleton` +
   `zc::ClipBank`, compiles it with `zc::compile_creature`, and composites it
   through the renderer's pre-resolve hook. `build/tools/zhao-reel.exe` is
   already built. Zixxtrixx is authored the same way: it is a new subject
   alongside `creature-wave-walk` and `creature-bulk-pop`.

2. **Several parts MAY share one bone.** `compile_creature` (creature_core.cpp
   L441-456) validates only `p.bone < sk.bone_count` -- there is no uniqueness
   check. "One part = one bone" bounds how a part BENDS, not how many parts a
   bone may carry. This is what makes eyes, prongs and colour bands affordable:
   they cost parts and meshlets, but not bones, and bones are the scarce
   resource for a serpent.

3. **Colour is per-part and flat, today.** `SkinVertex` carries u/v but has
   "NO colour lane"; `Meshlet` carries r/g/b as the CLUT8 page stand-in. So the
   concept's longitudinal stripes cannot be painted -- they run AROUND the ring,
   not along the body, and U is not authorable anyway (U = ring angle high byte).
   Either the stripe is built as geometry riding the body on the same bones, or
   the creature reads as banded-along-its-length, which is the wrong banding.

4. **The owner's three deviations are all cheap under (2).** Bigger eyes and
   blockier prongs are extra rigid parts on existing bones. Exaggerating the head
   is ring radii, which are free.

---

## Next Steps

1. Fabian to answer the three Open Questions in SPEC_v1 (scale, slither cycle,
   whether to install a D toolchain for donor measurements).
2. Decide the dorsal-stripe question from Decision 3.
3. Author the part/bone decomposition; size it with the triangle anchor BEFORE
   modelling.
4. Slither + stance clips; reel subject; GIF to the Upheaval creature site.
