# Stage C's port inventory — what the new top must wire, and what it must NOT

Stage C composes twelve blocks into `zhao_texture_island_v3_top.sv`. This is the
mechanical half done before the RTL, for the same reason Stage B's contract was:
the expander went in clean on the first build because its behaviour was written
down first, and a twelve-block composition has far more places to be nearly right.

## The eleven instantiations the old top already has

From `zhao_texture_island_top.sv`, with the line each begins at:

| block | line | Stage C |
|---|---|---|
| `zhao_raster_rcp24_svc #(.NCTX(8), .TOKW(8))` | 537 | **carried over unchanged** |
| `zhao_raster_perspuv_svc #(.NTOK(16), .TAGW(16))` | 788 | carried over |
| `zhao_texture_mosaic` | 835 | carried over |
| `zhao_texture_fragrob #(...)` | 941 | **DELETED** |
| `zhao_texture_tmu_plan #(.SRCW(SRCW))` | 1111 | carried, `SRCW` 16 → 18 |
| `zhao_texture_cache_pipe #(...)` | 1229 | carried, **`.SRCW(18)`** — the parameter landed 2026-09-08 |
| `zhao_texture_rsp_dispatch #(...)` | 1280 | carried, `TOKW` 16 → 18 |
| `zhao_texture_bilerp_lane #(.TOKW(TOKW))` | 1413 | carried |
| `zhao_texture_palette_res #(...)` | 1470 | carried |
| `zhao_texture_aux_pipe #(.TOKW(AUX_TOKW))` | 1898 | carried |
| `zhao_texture_material_combine_v2 #(.NCTX(8), .TAGW(ROBTAGW))` | 2009 | carried |

Two blocks are NEW in the composition: `zhao_texture_frag_expand` (18 ports,
landed and fire-tested 2026-09-08) and `zhao_texture_v3own` (30 ports).

## The wiring budget, stated plainly

**48 ports across the two new blocks**, against eleven carried instantiations
whose connections are already written and known-good in the old top. That ratio
is the argument for a NEW top rather than an edit: the carried eleven are copied
verbatim, and every line that differs is a line about the ownership change.

## What must NOT be touched, and why each

* **`zhao_texture_island_top.sv` and its fit target.** It is the ORACLE for
  Stage C's paired run (brief §6.5: *"first keep the current island behavior as
  an end-to-end oracle"*). An edit there destroys the comparison.
* **`zhao_texture_v3own.sv`.** The 541-check adversarial suite reads internal
  probes — `c3t_we_q`, `cmt_q`, the `vgen_q` shadow — through `verilator public`
  markers. Every adapter belongs in the new top or the expander. If the
  integration needs v3own changed, that is a FINDING about the architecture, not
  a licence to edit.
* **The material arithmetic.** COMBINE V2's equations are V1's byte for byte
  (its own header, quoting the recovery brief), and
  `test_every_recipe_matches_the_oracle` holds all eight recipes against zref.

## The three gates, and the order they are checked in

Stage C's acceptance is ordered, and the order matters because the cheap gates
would otherwise be skipped once the expensive one passes:

1. `texture_v3own_adversarial` — 541 checks, on the UNMODIFIED file. Proves the
   integration changed nothing about the owner.
2. `island_v3_composed_directed` — the 119-check suite retargeted, same stimulus
   bytes, same zref oracle.
3. The PAIRED RUN — both tops, identical stimulus, retired streams byte-identical
   on rgb/a/tag/refused/ORDER. Counter identity is NOT required where semantics
   legitimately refine; colour and order identity IS, everywhere.

## The prediction that must be registered BEFORE the fit

Per M6 and this session's own retraction, and per the architecture's §2.3:

* **M10K:** 37 − deleted-table RAM + 17 (v3own) + 1 (AUX bank) ≈ **50-55**,
  against a gate of 64.
* **ALM: SIGN UNKNOWN.** §2.3 is explicit — *"anyone who requires P0-C to cut
  composed ALM on its own should not approve this plan."* A miss is a finding to
  publish, not a silent re-scope.
* **Gating family recorded on BOTH sides.** `worst_path_index.json` holds today's
  identities; Fmax is movement unless the family matches.

## Status

Inventory only; no RTL. The build is tractable because eleven of thirteen
instantiations are copies and the two new ones are both already measured as
leaves.
