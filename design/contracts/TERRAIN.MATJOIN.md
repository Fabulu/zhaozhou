# TERRAIN.MATJOIN — Earth out-lane 2 composed onto authored layer E

**Module:** `fpga/rtl/terrain/zhao_terrain_matjoin.sv`
**Encoding:** `fpga/rtl/common/zhao_material_token_pkg.sv`, `spec/qformats.md` §14
**Reference law:** `zref::fieldir::compose_material`
**Tests:** `tests/terrain/terrain_matjoin_directed.cpp`,
`tests/terrain/composepub_acceptance.cpp` case 12
**Composed:** `zhao_console_core`, 2026-09-27 (MATERIALPATH), entry `I34` (P3)

---

## 1. WHY THIS BLOCK EXISTS

Before it, the two halves of a terrain cell reached TERRAIN.COMPCACHE by
different routes:

```
  heights  : TERRAIN.PAGESTREAM -> TERRAIN.PATCH (field lane composed in) -> COMPCACHE
  material : TERRAIN.PAGESTREAM ------------------------------------------> COMPCACHE
```

Height went **through** the composer. Material went **around** it.
`zhao_terrain_compcache_front`'s layer-E write face had exactly one driver — the
page stream, i.e. authored layer E — and `zhao_terrain_patch` accepts
`fld_height_i` and nothing else. A field material result therefore had **no port
to arrive on**, which is what entry `I34` (P3) records as material's blocker:
not a missing owner, not a missing destination, an **absent encoding and an
absent hop**.

This block is that hop. It is deliberately the same shape as `TERRAIN.VELJOIN`,
which does the same job for out-lane 1.

## 2. THE LAW — RATIFIED, NOT CHOSEN HERE

`zref::fieldir::compose_material`: **start from authored layer E; the LAST
ENABLED WRITER WINS, in accepted command order.** The reference header gives the
reason the order is the priority — *"hardware inventing an implicit material
hierarchy would be a game rule smuggled into silicon, and software can already
express any precedence it wants by choosing the order it submits."*

TERRAIN.PATCH's chosen law 1 puts one lane word per accepted §9.1 list entry per
vertex, **in list order**. Out-lane 2 rides that same word. So "last enabled
wins" is implemented by letting each enabled word overwrite the held override:
the list order *is* the command order. No priority encoder, no hierarchy, no
tie to break.

A lane word is **enabled** when all three hold, and the three are independent:

| term | source | meaning |
|---|---|---|
| `f_covers_i` | TERRAIN.PATCH `fld_covers_o` | the §9.1 footprint answer, decided ONCE by the block that owns the list and never re-decided here |
| `f_present_i` | the adapter's `ans_present_o[2]` | the Earth record's ordinal-2 presence. **An absent output is not a write of zero** |
| tag | `zmt_tag_ok(f_material_i)` | the word is a v1 material token |

## Scalar reference function

`zref::fieldir::compose_material`

HEADING ADDED 2026-09-29. The symbol was already named three times above -- as the
**Reference law** in the summary, in section 2, and in section 7 -- and matches the
ledger's `reference_model` exactly. What was missing was this canonical heading, which
is the one place rule V17(b) compares contract against ledger. That comparison exists
because drift between the two is how `zref::framePixelCrc` survived as a citation to
nothing, so the heading is load-bearing rather than decorative: naming the oracle in
prose leaves the two records uncomparable by any tool.

## 3. THE ARBITRATION IS AT THE COMPOSE POINT, NOT AT THE PAGE

This is the decision the block encodes and it is the one worth reading twice.
`zref_fieldir.hpp`'s sinks header:

> *"ALL THREE ARE LIVE COMPOSITION, NEVER PERSISTENT MUTATION. … a field
> evaluated every frame must never rewrite a VRAM page every frame, which is the
> failure this separation exists to prevent."*

So a field material result must **not** become a second writer of the authored
page. It composes **once per frame**, into TERRAIN.COMPCACHE — which is the
per-frame compose cache, and already where TERRAIN.PATCH deposits
field-composed *heights*. Material is therefore symmetric with height rather
than a mutation of anything, and the authored triple is never modified.

**Default is bit-for-bit the previous behaviour.** No covering field, no
ordinal-2 presence, or a non-v1 token ⇒ the authored bytes pass through
unchanged. That is the first case of the directed test and the reason composing
this block is safe.

## 4. THE TAG, AND WHY A REFUSAL IS POSSIBLE AT ALL

`spec/terrain_rules.md` §6.2 gives weight 0 and 255 meanings, so **every 24-bit
pattern is a legal material state**. A decoder without a tag cannot distinguish a
real result from a lane that was never written — and `32'd0`, which the adapter
parks on an absent lane, would decode as the perfectly legal triple `{0,0,0}`
and render.

So bits `[31:24]` carry `ZMT_TAG_V1 = 0xE1`. A token that is not v1 is
**refused**: the authored triple is kept and `token_refused_o` counts it.
Nothing is substituted.

`token_refused_o` differences the incoming word against a **constant**, not
against a second register loaded by the same enable — so it is not the
structurally-blind detector shape `CLAUDE.md` records.

## 5. SEQUENCING, STATED SO IT CAN BE CHECKED

`zhao_terrain_patch` holds ONE vertex at a time (`vtx_ready_o = !busy &&
out_free`, `fld_ready_o = busy`), so:

```
  accept(V)  ->  lane beats for V  ->  state publish for V  ->  accept(V+1)
```

and the accept and lane faces are mutually exclusive by construction — on the
accept cycle `busy` is still low.

**What is NOT true, and the first draft of this contract asserted it:**
accept(V+1) is *not* strictly after the state publish of V. The patch's
`out_free = !r_valid || st_ready_i` (`zhao_terrain_patch.sv:285`) lets
`vtx_ready_o` rise on the **same cycle** the held record retires, so an accept
and a publish coincide routinely. That error cost nothing in data — the emit is
lossless either way — but it made `held_overrun_o` fire **992 times per patch on
a correct walk**, and it was found by `composepub_acceptance` case 12 against the
real patch rather than by re-reading the RTL. The guard now carries `!st_fire_i`.

The block latches at **accept**, accumulates across V's **lane beats**, and
emits at V's **state publish**. Emitting at the state publish rather than at the
next accept pins the material write to the same record as its height, so it can
never be outstanding when `fill_done_o` — which counts *height* records —
declares the parity full.

**The assumption is guarded, not trusted.** A second accept while a cell is
still held increments `held_overrun_o` *and* emits the held cell, so the guard
is lossless as well as loud. It is unreachable in the composed console, which is
why it is driven at the block's own ports in the directed test and seen to fire
there.

## 6. PORTS

| dir | port | note |
|---|---|---|
| in | `a_we_i`, `a_ci_i[4:0]`, `a_cj_i[4:0]`, `a_mat_a_i`, `a_mat_b_i`, `a_weight_i` | the authored layer-E face, verbatim as the page stream presents it |
| in | `f_fire_i`, `f_covers_i`, `f_present_i`, `f_material_i[31:0]` | the Earth answer lane, forked where TERRAIN.VELJOIN forks it |
| in | `st_fire_i` | TERRAIN.PATCH's state publish — the emit beat |
| out | `o_we_o`, `o_ci_o`, `o_cj_o`, `o_mat_a_o`, `o_mat_b_o`, `o_weight_o` | port-for-port TERRAIN.COMPCACHE's layer-E write face |
| out | `cells_written_o` | census |
| out | `field_composed_o` | cells where a field token was accepted and applied |
| out | `token_refused_o` | non-v1 tokens; authored kept |
| out | `lane_no_cell_o` | **CENSUS, NOT A FAULT** — the 65 lattice vertices per patch that own no cell still receive lane words. Expected non-zero whenever a field is active |
| out | `held_overrun_o` | the sequencing guard |
| out | `idle_o` | no cell held |

Three reach `zhao_console_core`'s edge: `field_composed`, `token_refused`,
`held_overrun`. The two censuses and `idle` do not — `cells_written_o` restates
`terr_cc_mat_cells_o` one hop up and `lane_no_cell_o` is arithmetic, and a port
for a number nobody reads is the uncashed cheque this tree keeps finding.

## 7. WHAT THE EVIDENCE DOES AND DOES NOT COVER

**Covered.** `terrain_matjoin_directed` — 50 checks, 0 failures: authored
pass-through bit-exact; a field write changing the composed triple with an
asymmetric value; all four non-enabled forms; both command orders; both guards
fired against controls; and 600 pseudo-random vertices differenced against
`zref::fieldir::compose_material` with 247 enabled and 51 refused words, so the
sweep is shown to have *reached* both paths rather than passing vacuously.

**Not covered here.** The console smoke issues no TerrainField, so
`field_composed` is expected to read **zero** there; that zero is evidence about
the authored path being undisturbed, and nothing else. The composed-path
evidence is `composepub_acceptance` case 12, which drives out-lane 2 through the
real `zhao_field_earth_adapter`.

**Downstream is NOT closed by this block, and must not be reported as such.**
The composed triple reaches TERRAIN.COMPCACHE and is served to TERRAIN.TESS,
which forwards it per triangle to `tcf_tri_mat_{a,b,weight}_w` in
`zhao_console_core` — where it has **no reader**. Carrying it to the mosaic
needs a per-triangle source for the flat request's `base_rgb`/`recipe_weight`,
and that is `I13`'s seam, not this one. See `FINDINGS-MATERIALPATH.md` for the
measured alignment obstacle.

## 8. COST

Hand-counted, not fitted (R236), in the unflattering direction: one held-valid
bit, two 5-bit addresses, six 8-bit byte holds, one override-valid bit, five
32-bit censuses. **Order 230 ALM, 0 DSP, 0 M10K.**
