# FINDINGS -- TERRAINMAT (`gz/terrainmat`)

Branch `gz/terrainmat`, based on `86158a75`.
Answering the brief's nine Deliverable points in order.

---

## 1. The register, measured BARE

**4 before, 4 after.** `python tools/budget/completion_register.py`, bare, at
`86158a75` and again after the I13 edit:

```
MANDATORY GAPS REMAINING          : 4
  (3 tie-offs + 1 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable)
  I13  boundary       PROJ_SUBSYSTEM's TRIANGLE OUTPUT (`proj_out_*`) -- BOUNDARY.
  I34  boundary       TERRAIN.PATCH's FIELD-HEIGHT LANE (`terr_pt_fld_*`) ...
  I55  unclassified   GEOM.PARAMBUF's WALK REQUEST and DECODED OUTPUT
```

**It did not move and that is correct.** I13 closes when the terrain arm is
whole, not when two of its three named items are. `zhao_terrain_normalmap` is
still on the BUILT-BUT-NOT-CONNECTED list and was again not composed, and the
mosaic pick's reader is still not exercised by this console (§2, §7). Closing
I13 on (a) and (b) alone would be the register moving ahead of the picture,
which this entry has refused seven times.

## 2. The terrain texel -- stimulus, chain, oracle

**A TERRAIN FRAGMENT CARRIES A TEXEL. `texture samples` moves 1,190 ->
1,216.** Console smoke, PASS, `raster pixels=2816`, `frames_admitted=1`:

```
SMOKE: texture  fragments=1216 samples=1216 cache[hit/miss]=[1216 27]
                plan_accepted=1216 dispatch_accepted=1216 combine_refused=0
SMOKE: terrmat  backed=128 orphan=0  (SetEnvironment terrain_material {set=00abcd02 id=2})
SMOKE: terrcf   triangles=128 emitted=128 src_mismatch=0 uv_sat=0 degenerate=0
SMOKE: matwin   resolves=2 switches=2 no_record=0 sel_ovf=0 clut_unowned=0
SMOKE: material responses=2 misses=2 not_resident=0 fetch_denied=0
SMOKE: clip     submitted=144 clipped=69 culled=0 setup_submitted=75
SMOKE: raster   pixels=2816 bursts=176
```

**`resolves` 1 -> 2 with `switches` UNCHANGED at 2** is the line to read twice.
Terrain already formed its own span -- CARRIAGE measured that second switch as
"a lawful `MATMODE_NONE` that issues no resolve". What is new is that the span
now RESOLVES, and it resolves **its own record**.

* **stimulus** -- one `SetEnvironment 0x0311` in the frame packet carrying
  `terrain_material_set = 0x00ABCD02` and `terrain_material_id = 2`, plus a
  third MaterialRecord (record 2) in the same already-published 256-byte
  MATERIAL_SET. No new PublishResource, no new directory slot, no second
  binding row, no new upload region.
* **chain** -- the host's record -> FRAME_RING -> CMD.SCHEDULER -> CMD.DMA
  over the shell's real HPS bridge -> CMD.DECODER's verdict -> CMD.EXEC's R25
  arm -> latched in `zhao_console_core` on the environment's **commit beat** ->
  `zhao_terrain_clipfeed`, which **derives** the mode -> `u_geom_clipdoor`
  slice 3 -> `zhao_material_window` drains, switches and resolves ->
  MATERIAL.RESOLVE's directory and its ENGINE1 fetch through the real
  MEM.GUARD -> the published record's `sample_count` and `base_binding` -> the
  binding page -> the TMU -> a texel in every terrain fragment.
* **the gate is a LAW, not a pinned number.** The smoke now asserts
  `render_texture_samples_o == render_texture_fragments_o`. Every material this
  fixture uploads declares `control[1:0] = 1`, so one fragment is one sample;
  the equality survives a change in triangle counts, holds in
  `-TerrainFlatLattice` where both sides are the mesh's alone, and fails the
  moment any producer goes back to asking for nothing.

**AND IT WAS SEEN TO FAIL.** `-NoTerrainMaterial` is a new committed control:
both ABI fields zero -- what every capture written before 2026-09-26 carries,
because those bytes were `pad` then -- and nothing else changes. It reproduces
the parent commit's numbers exactly:

```
-NoTerrainMaterial   terrmat backed=0    texture fragments=1216 samples=1190
                     matwin resolves=1 switches=2   raster pixels=2816
plain                terrmat backed=128  texture fragments=1216 samples=1216
                     matwin resolves=2 switches=2   raster pixels=2816
```

`raster pixels` is IDENTICAL in both, which is the proof that what moved is the
fragment's COLOUR and not its coverage.

**THE HONEST BOUND, and it belongs immediately after the number.** This bench
answers every fill line with one constant green and never reads
`fill_req_addr_o` -- its own paragraph says so (*"what is being proven here is
that a texel ARRIVES"*). So what moved is that terrain's fragments now take the
**same real path the mesh's do**: a real binding row, a real cache miss, a real
fill, a real published sample. **The texel's VALUE is the bench's constant for
both, and nothing here checks terrain's colour against the oracle.** No oracle
moved and none needed to.

**What the fragments sample is the DIRECT RGB565 row the fixture already
programmed, not a tileset.** The mosaic pick's reader TERRAINTEX built is still
exercised only at `texture_island_v3_packet_b_directed`. §7 says why, measured.

## 3. The identity, and where the declaration lives

`SetEnvironment 0x0311` ended in `pad[12]`. Two of those bytes are now

```
handle32[material_set] terrain_material_set;   // offset 36, 0 = MATMODE_NONE
u16 terrain_material_id;                       // offset 40
pad[6];                                        // still zero-checked
```

The record stays 48 bytes and **`abi version` does not move** -- no opcode,
field set or size does. That is `capture_format.md` 1.3's same-bytes
reinterpretation, the pattern `MaterialRecord.fragment_state` established, and
the directive's section 5 pre-authorises exactly this shape.

**The whole compatibility argument is that ZERO KEEPS ITS MEANING.** A zero set
is `MATMODE_NONE`, which is what terrain declared before today, so no committed
capture changes by a pixel. `npm run abi:check` clean, 38 outputs match.

**The MODE is derived in the terrain block, not chosen in the composer** --
which this entry's TRIMERGE paragraph requires in as many words.
`zhao_terrain_clipfeed` takes **two** ports, not the three the brief predicted,
and its header says why: a third `mode` port would let a caller present a
combination the block's own law refuses.

```
mat_set_i != 0                 -> MATMODE_BACKED, pair passed whole
mat_set_i == 0, mat_id_i == 0  -> MATMODE_NONE, the zero pair
mat_set_i == 0, mat_id_i != 0  -> ILLEGAL: zero pair declared, counted
```

**THE ORPHAN RULE is a real fault, not tidiness.** `mode_contra_c` REFUSES a
non-zero `{set, id}` under `MATMODE_NONE`, and a refused triangle does not
reach GEOM.CLIP -- so an environment naming an id with a zero set would drop
the frame's **entire terrain arm**. The block declares the zero pair and counts
the discarded id on `mat_id_orphan_o`. The directive requires an unresolved
identity be *"diagnosed and handled by the declared failure/fallback policy,
never silently truncated or made token 0"*: the counter is the diagnosis and
the header is the declaration. It is reachable with legal stimulus, so it owes
no committed mutant; it is fired by exact amount in
`terrain_clipfeed_mat_directed` section 3.

**The identity is LATCHED on the triangle's own accept beat**, by the same
enable as its corners and `src_id_q`. In this composer `mat_set_i` is a
frame-global register CMD.EXEC reloads on every committed SetEnvironment, so a
combinational path to a per-primitive port would repaint an already-accepted
triangle with a later frame's material -- **and nothing would count it**,
because no counter in the arm looks at the field that moved. CLAUDE.md's
metadata-swap chapter, refused by construction.

**AND THAT CHECK WAS FIRED, not argued.** Replacing `mset_q`/`mid_q` with the
live `mat_set_c`/`mat_id_c` in the three output assigns and rebuilding:
**4 of 38 checks FAIL, and they are exactly the four that describe the latch**
(`expected 0x11112222, got 0x44445555`). Restored, verified by CONTENT rather
than by the write succeeding, timestamp forced, rebuilt -- no "no work to do"
-- 38 pass. Note the direction: the live version produces a plausible,
self-consistent wrong answer that every other check in the file accepts.

## 4. Was the `{a,b,weight}` -> u32 resolver needed?

**No, and it was not built.** My identity does not pass through the layer-E
triple at all: it is a `{material_set, material_id}` pair travelling the
ratified material path, and the triple stays exactly where CARRIAGE left it --
dangling on an internal wire, not wired into `base_rgb` (§7).

The brief is right that the resolver is **commissioned and unbuilt** (PATCHV2's
withdrawal of the strike stands -- `ops.yml`'s named reference function takes a
triple and returns a triple, so it is a thing to build, not a thing to cite).
It stays unbuilt. It is owed by whoever carries a **field** material into the
mosaic, which is I34's half, and it was not on this packet's path.

## 5. Does a FIELD material write change the intended consumer?

**No, and I will not imply otherwise.** That is the owner's bar for I34's
material half and this packet does not reach it. What terrain now presents is
the **host's authored** material identity from SetEnvironment, not a value a
FIELD wrote. The chain from a field write to this consumer needs PATCHV2's item
(5) in full -- `zhao_terrain_patch` accepting a field material at all (it has
`fld_height_i` and nothing else), the rider's 24 bits, and the token map that
exists in no form anywhere.

What this packet does change is that **the consumer now exists.** Before today
a field material had nowhere to land even in principle, because terrain
resolved nothing. That is a precondition removed, not the bar met.

## 6. Claims I found FALSE

**In my brief AND in I13's TERRAINTEX section, and it is the one that chose my
carrier's SCOPE.** Both argue the frame-wide field is faithful because
*"`draw_terrain` takes ONE tileset for the whole call, so a per-frame
environment material is faithful"*.

**There is no `draw_terrain`.** The function is `draw_heightfield`
(`reference/src/zrender/terrain.cpp:355`) and it takes `const Tileset*` for
**one patch**; the SELECTION is per patch, at
`reference/src/zrender/render_frame.cpp:373-374`:

```cpp
if (patch->tileset_id != 0) {
  ts = res.tileset(patch->tileset_id);
```

-- the same per-page `tileset_id` PATCHV2's item (4) named as terrain's
material set. So a frame-scoped pair is **not** equivalent to the reference for
a frame whose patches carry different tileset ids.

It remains a strict capability **increase** (terrain could present zero
tilesets before), the limit is declared in `spec/commands.zidl` beside the
field rather than left to be discovered, and the refinement is named and
costed: the page header's `tileset_id` overriding `terrain_material_id` per
patch, whose cost is the carriage from `zhao_terrain_hdrread` to the clipfeed
**and nothing else** -- no ABI field moves and no port built here moves, only
what drives `mat_id_i`.

**A second, smaller one, in the brief:** it predicted "three input ports on
`zhao_terrain_clipfeed`". Two suffice, and the third would have been harmful --
see §3.

## 7. What I refused

* **The layer-E triple into `base_rgb`.** CARRIAGE's ground holds --
  `base_rgb` is the published texel RGB at `sample_count == 0`
  (`zhao_texture_material_combine_v3.sv:513`) and `recipe_weight` is the
  `R_LERP` blend weight (:719-722), both shut today only by coincidence.
  Nothing here touched it, and my identity does not need it.
* **The TILESET/MOSAIC route, on a MEASURED blocker rather than on size.**
  This was my first plan and I dropped it after reading four lines:
  1. a TILESET row is **CLUT8 by law** -- `tileset_shape_ok`
     (`zhao_texture_binding_resolver_v2.sv:304-308`) requires
     `fmt == FMT_CLUT8`, correctly, because `zref::Tileset` is
     `uint8_t tiles[256][64*64]`;
  2. a CLUT sample needs a **palette identity**, and this composer publishes it
     as CONSTANTS -- `MAT_PALETTE_SLOT_C = 2'd0`, `MAT_PALETTE_GEN_C = 8'd0` in
     `mat_flat_request_c`;
  3. `read_witness_bad_c` (`:673-678`) requires the fragment's
     `{class, palette_slot, palette_generation}` to **equal the ROW's**, so the
     row is forced to `{0, 0}` too;
  4. and generation **zero** is the one generation
     `zhao_texture_palette_res_v2` cannot be handed in a single pass:
     `generation_q[slot]` resets to 0 and `LD_BEGIN` refuses
     `ld_gen_i == generation_q[ld_slot_i]` onto `err_same_gen_o` (`:215-219`),
     so slot 0 reaches a RESIDENT generation 0 only by loading some other
     generation first and reloading at 0.

  Fact 2 is **not my discovery** -- `zhao_material_window`'s own header says
  *"for a CLUT format the pair is real and nothing in this console produces
  it"* and `clut_unowned_o` counts it. Facts 3 and 4 are what turn "no
  producer" into a specification: **whoever composes the mosaic owes a palette
  identity with a real producer**, and the ratified field that would carry it
  does not exist -- `MaterialRecord` has `palette_base`, an ADDRESS, and no
  slot or generation.
* **Repurposing record 0** of the uploaded MATERIAL_SET, which was the cheapest
  place to put terrain's material. It is a **deliberate negative
  discriminator** for the mesh's resolve -- its own comment says it differs
  from record 1 "in every field the flat request carries" -- and reusing it
  would have deleted a live control to save 32 bytes. Terrain got record 2.
* **Giving terrain the mesh's own `material_id = 1`**, which needed no new
  record at all. `match_c` is exactly `{mode, vertex_alpha, frag_state, set,
  id}`, so terrain and the mesh would have shared ONE span and the run would
  have proved that terrain can sample **somebody else's** material. A much
  weaker statement than that it resolved its own.
* `GEOM_CLIP_ATTRS` stays 7. No flat colour stand-in. `zhao_terrain_normalmap`
  not composed. `kMat`/`kVp` untouched. **No fit was run.**

## 8. What I got wrong and caught myself

* **I nearly reported a TRUE receipt as a stale one.** My draft said
  `FINDINGS-terraintex.md`'s "1586 core ports" no longer described this tree,
  because `gen_console_board.py` printed **1584** when I regenerated. The two
  numbers are DIFFERENT METRICS from the same tool: the generate path prints
  *"1584 core ports re-exported, 4 driven by the board"* and `--check` prints
  *"FRESH (1588 core ports)"* -- the 4 board-driven ones. Counted by hand on
  the file: **1599 port lines at `86158a75`, 1601 now, exactly my +2.**
  TERRAINTEX's 1586 was right for its tree and 1588 is right for mine. I had
  a false "false claim" written and ready to ship, which is this campaign's own
  disease pointed the other way.
* **The first plan was the tileset row, and it would have shipped a
  contortion.** The palette chain in §7 was found by reading
  `zhao_texture_palette_res_v2`'s load FSM, not by a failing run -- which
  matters, because **the fixture route around it exists** (load slot 0 at some
  generation, then reload at 0) and would have produced a passing bench whose
  green depended on a two-pass trick nobody would have re-derived. A CLUT
  material whose palette identity is a composer constant is not made correct by
  a bench that can reach it.
* **I nearly shipped the new smoke gate as a pinned number** (`samples ==
  1216`). That pins a measured number in an assertion, the shape this entry has
  been stopped by twice. `samples == fragments` is derivable and strictly
  stronger.
* **I wrote the `mat_backed != 0` gate unguarded and `-Mutant` caught it on the
  next run.** Under the slot-overflow mutant no page becomes resident and
  terrain emits NO TRIANGLE AT ALL, so a non-zero census there is asserting
  against a state the machine cannot reach -- the exact thing this bench had
  already written down twice at this seam. Guarded, with the reason beside it.
  The equality `mat_backed == emitted` is NOT guarded and holds there as 0 == 0.
* **The mutant WRAPPER needed the ports too.**
  `tests/mutants/zhao_console_core_slot_overflow_mutant.sv` re-exports every
  production port through `.*` and failed to elaborate the moment the core
  gained two outputs. CLAUDE.md's copy-drift chapter is about a copy going
  QUIETLY stale; this is a wrapper, cannot drift, and failed LOUDLY. "Wrappers
  cannot drift" must not be read as "wrappers need no attention".
* **`.*` does not create an implicit net, and I assumed it did.**
  `tb_zhao_twod_chain.sv` and `tb_procmat_acceptance.sv` both failed the cmake
  configure with *"Can't find definition of variable"*. Declared in both.
* **A heredoc into `bash -c` ate my first edit script** on a quote-heavy
  payload, exactly as PACKET-PROTOCOL.md's corrected trap says. Wrote the
  script to a file and ran it, as that paragraph instructs.

## 9. Branch and commits

**`gz/terrainmat`**, pushed. Nothing else pushed; no rebase; no `--force`.

| commit | what |
|---|---|
| `97a36b81` | the IDENTITY: the ABI field, CMD.EXEC's decode, the clipfeed's two ports and derived mode, the composer's commit-beat latch, two new core counters |
| `17e77647` | the TEXEL: the fixture's terrain material, the smoke's new laws, the `-NoTerrainMaterial` control, the directed test, the I13 entry section |
| (this)      | these findings |

---

## Gates at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` (bare) | **4**, unchanged from `86158a75` |
| `npm run abi:check` | clean, 38 outputs match |
| `check_console_inventory.py` | OK -- 406 declared, 291 fit sources |
| `check_prod_manifest.py` | OK |
| `check_quartus17_syntax.py` | RC 0, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | OK |
| `check_console_closure_lint.py` (gate 31) | OK, 291 sources, self-test 5/5 -- **no missing pin**, which is what clears the PINMISSING the new ports risk |
| `check_entry_claims.py` | RC 0, no NEW claim |
| `mutant_copy_drift.py` | run AFTER the commit (R121) |
| `gen_prod_top.py --check` | fresh, 86 instances |
| `gen_console_board.py --check` | FRESH, 1588 core ports (1586 + my two) |
| `gen_shell_paired_diff.py --check` | fresh, both halves |
| console smoke (plain) | **PASS, `raster pixels=2816`, `frames_admitted=1`, `texture fragments=1216 samples=1216`** |
| `-NoTerrainMaterial` (**NEW**) | PASS -- `backed=0`, `samples=1190` of 1216, `resolves=1` |
| `-TerrainFlatLattice` | PASS -- pixels 2560, `fragments=samples=1190` |
| `-BadVertex` | PASS -- pixels **512**, `backed=128` |
| `-NoEchoArm` | PASS -- pixels 2816, `samples=1216` |
| `-Mutant` | PASS -- `terr_pl_slot_overflow_o=1`, fired exactly once |
| `-BadTraceArm` | PASS |
| `terrain_clipfeed_mat_directed` (**NEW**) | **38 checks passed**; fire test fails exactly 4 |
| `test_cmd_exec_directed` (R60) | **977 checks passed** |
| `cmake --preset windows-native` | CFG_RC=0 |
| `verilator --lint-only -Wall` on `zhao_terrain_clipfeed` | 0 diagnostics |

`npm run abi:check` WAS run: `spec/commands.zidl` was touched.
