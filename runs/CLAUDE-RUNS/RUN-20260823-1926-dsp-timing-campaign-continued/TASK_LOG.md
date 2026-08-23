# Task Log: RUN-20260823-1926 - The DSP and timing campaign, continued

**Created:** 2026-08-23 19:26 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260823-1926-dsp-timing-campaign-continued/

---

## Objective

Census **160 -> 85-90**, and close timing on the blocks that miss it. Continues
RUN-20260823-0934, which is archived; its SPEC (census reconciliation and
GEOM.SKIN) was complete and the work had outgrown it.

---

## Progress Timeline

### 2026-08-23 19:26 UTC+02:00 - Run opened

Entry state:

| | |
| --- | ---: |
| census | **160** DSPs against 112 |
| ceiling | 85-90 |
| blocks with a real Fmax | **4 of 47** |
| `ctest -L fast` | 269/270, sole failure the V16 `FIELD.SEQ.CORE` gate |

Measured Fmax so far — **three of four are comfortably fast, only Field is
genuinely slow**:

| block | Fmax |
| --- | ---: |
| `zhao_texture_tmu` (pre-rearchitecture) | **199.72 MHz** |
| `zhao_geom_skin` | 89.65 MHz |
| `zhao_surface_stamp` | 87.54 MHz |
| `zhao_field_seq` | **33.86 MHz** |

In flight: `zhao_texture_tmu`, 28 -> 6-9 DSPs against a demand of 850,000
samples/frame derived from Sacrifice. Its `@pre-rearch` baseline is already
recorded, which is the SURFACE.STAMP lesson applied on the first move.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| 17:36 | `acc49f0` | TEXTURE.TMU 28 -> 6-9 DSPs | Running | own run dir |

---

## Files Created

- this run directory

---

## Decisions Made

**Closed the previous run rather than letting it accrete a whole day.** Its
SPEC was census reconciliation and GEOM.SKIN; both were complete while the work
had moved on to three other blocks and an architectural ruling. A run whose log
no longer matches its SPEC stops being a record and becomes a diary.

---

## Next Steps

- [ ] `zhao_texture_tmu` result
- [ ] `zhao_terrain_normals` 18 -> 1-2 (derived demand 2,000 normals/frame)
- [ ] `zhao_terrain_project` 33, cache-then-sequence
- [ ] the Field rearchitecture, wave 0 first: preserve the 33.86 MHz netlist and
      group the top 200 setup paths by family
- [ ] derive per-frame demand for the five Field profiles
- [ ] re-measure blocks whose rows predate the SDC fix
- [ ] archive this run when the campaign closes
