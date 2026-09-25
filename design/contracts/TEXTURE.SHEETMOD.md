# TEXTURE.SHEETMOD — the surface sheet's visible effect

**Module:** `fpga/rtl/texture/zhao_texture_sheetmod.sv`
**Enforced by:** `tests/texture/sheetmod_directed.cpp`, `tests/prod/terrainaux_acceptance.cpp`
**Composed in:** `zhao_texture_island_v3_top` (`u_sheetmod`), which is resident in
`zhao_console_core`'s closure through
core → `zhao_shell_top_v2` → `zhao_geom_bin_pipe_v2` → `zhao_raster_tile_pipe_v2`
→ `zhao_raster_texture_stage_v3` → `zhao_texture_island_v3_top`.
**Written:** 2026-09-25, packet TERRAINAUX, under the owner vacation directive of
2026-09-23 §6 ("Finish the aux-context, material-identity and color consumers").

---

## 1. DECISION RECORD

### Question

`zhao_texture_aux_pipe_v2` returns a typed AUX plane —
`{status8, tag8, strength8, 24'b0}` — for every fragment whose material declares
`aux_required`. **Nothing consumed the strength byte.** `TEXTURE.COMBINE.md` and
`TEXTURE.AUX.V2.md` both close their AUX section with the same sentence:

> "AUX status participates in final status only when AUX is required; **tag and
> strength are reserved for a later visible terrain-effect composition and
> Packet B does not claim that effect is connected.**"

So the console could read layer F and could not *see* it. Charter §12's sheet
tint — the scorch a player's spell leaves on the ground — had a producer
(`SURFACE.STAMP`), a store (`zhao_surface_sheet`), an address generator
(`zhao_texture_aux_pipe_v2`) and **no pixel**.

Where does the strength become a colour?

### Chosen option

**A separate combinational leaf, `zhao_texture_sheetmod`, on the island's final
return — between `u_combine`'s result and `zhao_texture_v3own`'s
`fin_result_i`.** The fragment's strength is read from an island-local per-slot
record keyed by the OWNER the combiner returns.

### Reason, and the alternatives that were rejected

* **Inside `zhao_texture_material_combine_v3`, as a post-recipe stage.**
  Rejected on cost and on blast radius, not on principle: that block has
  **thirteen committed mutant copies** in
  `tests/mutants/zhao_texture_material_combine_v3_mutants.sv`, and CLAUDE.md's
  own rule is that *editing a file stales its committed mutant copies even if
  you only touch comments* — `mutant_copy_drift` keys on commit time. Thirteen
  three-way merges to move an arithmetic stage that does not belong to the
  recipe engine is the wrong trade. The leaf is also independently testable,
  which the stage would not have been.
* **As a recipe operand (a ninth recipe, or AUX as sample 2).** Refused, and the
  refusal is re-affirmed rather than weakened — see §2.
* **In `zhao_raster_fragment`, beside `unit_mul(texel, vert_rgb)`.** That is
  where the oracle's *other* two factors meet, and it is the natural home — but
  the fragment has no aux port and the 48-bit `TEXTURE_RESULT` has only seven
  RESERVED-ZERO bits, so the strength would need a new field on every hop of the
  texture round trip. The island already holds both operands.
* **A wire read of `s_aux` at the point of use.** Refused at design time. The
  AUX return lands some clocks before the combiner produces the colour for the
  same fragment and the combiner interleaves `NCTX` contexts, so a wire read
  would apply fragment A's scar to fragment B's colour with every counter in the
  island balancing — CLAUDE.md's lockstep fault exactly. The record is keyed by
  the OWNER SLOT and read back with the owner the combiner returns, which is the
  record's own key.

### Constraints and cost

* **Arithmetic:** three 8×8 unsigned multiplies and one round-half-up each. No
  DSP is required (the same `unit_mul` shape `zhao_raster_fragment`,
  `zhao_post_composite` and `zhao_post_gather_tag` already hold); order 60–90
  ALM, **counted by hand, not fitted** — this leaf has never been through
  `quartus_map` and that is stated rather than glossed.
* **Storage:** `sheet_m[OWNERS]` in the island, `OWNERS = 64` rows of
  `{generation8, aux_required1, strength8}` = **576 flops**.
* **Latency:** ZERO. Purely combinational, in a cone that already terminates at
  `u_own`'s input registers.
* **No new port on any hierarchy level.** The evidence is the leaf's own directed
  test and the acceptance bench, not a counter threaded through six modules.

### Consequences — code, tests, compatibility

* `zhao_texture_island_v3_top` gains `u_sheetmod`, the `sheet_m` record and a
  simulation assertion. Its port list is **unchanged**.
* `design/fit_targets.yml`: the new source joins all five lists that already
  carry `zhao_texture_material_combine_v3.sv`.
* **Compatibility: a fragment that does not declare AUX is bit-identical.** The
  tint arm is taken on `aux_required` alone, and when it is low the RGB passes
  through untouched. Every existing capture, golden and CRC is unaffected,
  because nothing in the composed console declared AUX before this packet.

---

## 2. WHAT IS NOT SUPERSEDED, STATED BEFORE WHAT IS

Both amendments below are narrow. These three sentences of
`TEXTURE.AUX.V2.md` §"No AUX-as-sample-2 law" and `TEXTURE.COMBINE.md`
§"Required planes, AUX, status, and raw index" **stand unchanged**:

1. *"AUX never occupies, supplies, aliases, or substitutes for TMU sample 2."*
   `zhao_texture_material_combine_v3` is not edited by this packet; its
   `src_s2_i` and `src_aux_i` remain distinct and its thirteen mutants still
   describe it.
2. *"No material recipe 0 through 7 consumes AUX tag or strength as an RGB or
   alpha sample operand."* The modulation happens **after** the recipe, on the
   colour the recipe produced. That is the oracle's own shape:
   `span.mod_r/g/b` multiplies the texel the recipe made; it is not one of the
   texels.
3. *"The required-source mask remains
   `{aux_required, sample2_required, sample1_required, sample0_required}`"*, and
   final status is still the OR of committed required-plane statuses.

**The `tag` byte remains unconsumed by any effect.** `terrain_rules` §6.5 says
the aux lane exists "for tag/strength effects", plural, and the tag's family is
unauthored. Inventing one here would be the art decision a composer must not
make; `zref_aux.hpp`'s choice A1 argued for the tag being *readable*, not for it
being *interpreted*, and that is still the state.

## 3. WHAT IS SUPERSEDED

**`TEXTURE.AUX.V2.md`, "No AUX-as-sample-2 law", final clause** —
*"tag and strength are reserved for a later visible terrain-effect composition
and Packet B does not claim that effect is connected"* — is superseded **as to
`strength` only**, 2026-09-25. The composition exists, is named
`zhao_texture_sheetmod`, is instantiated in the island, and is claimed connected
with a pixel measured in `tests/prod/terrainaux_acceptance.cpp`. The clause
stands unchanged as to `tag`.

**`TEXTURE.COMBINE.md`, "Required planes, AUX, status, and raw index",
sentence "The combiner reads only AUX status"** — still TRUE OF THE COMBINER and
no longer true of the ISLAND. The island reads the strength on the return path.
Recorded here rather than by editing that sentence, because the sentence is a
correct statement about the block it governs.

---

## 4. THE LAW

`reference/src/zrender/terrain.cpp` states charter §12's tint twice, for its two
profiles, and this block implements both — because they are the same arithmetic:

```
line 45    header:            rgb = rgb * (255 - strength/2) / 256   (max ~50%)
line 638   sheet_factor:      sheet == nullptr ? 65536 : ((255 - (s >> 1)) << 8)
line 718   untextured tint:   tint = 255 - (sheet_at(i, j) >> 1)
line 756   its application:   (lit * tint + 128) >> 8
```

and

```
rescale( rgb * ((255 - (s>>1)) << 8), 16 )
  = (rgb * ((255-(s>>1))<<8) + 2^15) >> 16
  = (rgb * (255-(s>>1))      + 2^7 ) >> 8
  = unit_mul(rgb, 255 - (s>>1))
```

**Section 1 of the directed test proves that equality over all 65,536 operand
pairs before any RTL is touched**, so the RTL comparison that follows is evidence
about the RTL and not a transcription agreeing with itself. `zref` does not
export this law — it is inline in `draw_terrain` — so the suite carries two
independently transcribed oracles, which is `terrain_uvlane_directed.cpp`'s
discipline for the frozen UV law.

**THE IDENTITY IS THE ENABLE, NOT STRENGTH ZERO.** `255 - (0 >> 1) = 255`, and
255/256 is not 1: the oracle's own comment says *"even t = 255 would darken by
~0.4% ((v*255+128)>>8 < v for v >= 129), shifting every unstamped colour"*, and
`sheet_factor`'s `if (sheet == nullptr) return 65536` is a SEPARATE ARM. So
`en_i` low is bit-for-bit passthrough (section 3 of the test) and `en_i` high at
strength 0 still darkens (section 4). Both halves are asserted; neither alone
says anything.

`en_i` is the MATERIAL's `aux_required` declaration. It is not inferred from a
non-zero strength and not inferred from a non-zero aux context — owner directive
of 2026-09-23 §3: *"Profile selection is explicit, not inferred from whether a
port happens to be zero."*

## 5. THE THREE GUARDS ON THE ARM, AND WHY EACH IS SEPARATE

In `zhao_texture_island_v3_top`:

| guard | why |
|---|---|
| `sheet_row_c[8]` | the material's declaration; see above |
| `sheet_gen_ok_c` | the record's generation against the combiner's returned owner generation. Fail-**safe**: a mismatch leaves the colour untinted, never wrongly tinted |
| status == 0 | `TEXTURE.COMBINE.md` fixes a faulted result's terminal value at **exactly** `24'hFF00FF`. A tinted magenta is a different and quieter colour, and a fault must stay loud |

**No generation counter is added, deliberately** — a counter declined at design
time beats a counter explained at review time. `zhao_texture_v3own` is the sole
lifecycle owner and does not free a slot until its OUTPUT handshake, which is
downstream of `fin_result_i`, so the slot cannot be re-admitted while its own
result is in the combiner: the mismatch is unreachable by construction. The
generation is stored and compared anyway because the comparison is free, and the
disagreement trips a simulation assertion — `synthesis translate_off` does not
hide one from Verilator.

## 6. WHAT THIS BLOCK REFUSES

* No sampler, no address generation, no residency, no handshake. It receives a
  byte `SURFACE.SHEET` returned and `TEXTURE.AUX` typed.
* No ALPHA. §12's law is an RGB tint and the oracle applies it to three channels.
* No `tag`. See §2.
* No state. The record that holds the strength belongs to the island, which is
  where the owner lifetime is, not to this leaf.
