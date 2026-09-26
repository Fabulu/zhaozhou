# FINDINGS — TERRAINTEX (`gz/terraintex`)

Branch `gz/terraintex`, based on `64f2c713`. Head at writing: **`1f53dfba`**.
Three commits: `847b223a` (the reader), `4aadc57c` (the texel + two false
absences), `1f53dfba` (the mutant-copy refresh the second one made owed).

Answering the brief's eight Deliverable points in order.

---

## 1. The register, measured BARE

**4 before, 4 after.** `python tools/budget/completion_register.py`, bare, at
`64f2c713` and again at `1f53dfba`:

```
MANDATORY GAPS REMAINING          : 4
  (3 tie-offs + 1 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable)
  I13  boundary   PROJ_SUBSYSTEM's TRIANGLE OUTPUT (`proj_out_*`) -- BOUNDARY.
  I34  boundary   TERRAIN.PATCH's FIELD-HEIGHT LANE (`terr_pt_fld_*`) ...
  I55  unclassified  GEOM.PARAMBUF's WALK REQUEST and DECODED OUTPUT
```

**`zhao_terrain_normalmap` did NOT leave the `BUILT BUT NOT CONNECTED` list.**
It was not composed. I13's first prohibition stands and nothing here needed it.

**A brief claim corrected:** the brief and I13's CELLCARRY paragraph both say
composing it "would move the register 5 -> 4". The register reads **4** at the
base commit, so composing it would move 4 -> 3. The count moved under the
entry's prose between passes; the shape of the claim is right and the number
in it is not.

## 2. The texel — the stimulus, the chain, the oracle

**A texel sampled. It is NOT a terrain texel, and that is the first thing to
say.** The console smoke's line is unchanged: `texture fragments=1216
samples=1190`, `combine_refused=0`. Terrain still declares `MATMODE_NONE` and
publishes `sample_count = 0`, so it asks the island for nothing.

What DOES sample is a fragment against a **TILESET binding row**, in the
composed island, and the tile it reads is the one TEXTURE.MOSAIC picked:

```
packet-b mosaic: pick=124 tx=13 ty=50 tileset_line=0047cc80 plain_line=00400c80
[texture_island_v3_packet_b_directed] PASS, outputs=70
```

* **stimulus** — one fragment, `sample_count = 1`, recipe PASSTHRU, class CLUT,
  binding selector 28 (the tileset row, base `0x00400000`), mosaic triple
  `{mat_a 0x31, mat_b 0x7c, weight 0x90}` on the island's existing
  `base_rgb`/`recipe_weight` carriage, `invw24 = 0x10000` and u/v such that the
  unfolded world texel index is (141, 77).
* **chain** — `zhao_texture_mosaic_v2` picks -> `zhao_texture_mosaic_hold`
  holds the pick under the owner's generation seal -> `zhao_texture_binding_
  resolver_v2` displaces the TILESET row's base by `tile * 4096` ->
  `zhao_texture_tmu_plan_v2` folds u/v with the row's own mirror wrap at
  log2w = log2h = 6 -> cache -> palette -> retired RGB.
* **oracle** — `zref::terrain::mosaic_pick` and `zref::terrain::mirror_texel`,
  the frozen 6.2 functions, which share no arithmetic with the RTL's CSD
  shift/add trees or `zhao_texture_mod255`. `0x0047cc80` is
  `0x400000 + 124*4096 + (50*64 + 13)` on its cache line.
* **negative control** — an ordinary CLUT8 64x64 mirror row at the SAME BASE
  that does not declare itself a tileset. The pick is computed for it too; its
  line is `0x00400c80`, undisplaced.
* **anti-vacuity** — the bench asserts the picked tile is NOT ZERO and that
  three picks are not all equal, because a zero displacement is exactly what an
  unwired reader produces. That is SHADELADDER's rung-centre lesson, one block
  over.

`texture_desc_expand_bind_v2_composed` proves the same at the planner, 60
checks (was 52): `tileset_samples=3`, `picks_held=9`, `stale_slot_holds=22`.

**Fired, not assumed.** `read_tileset_c` forced to `1'b0` in my own worktree:
2 of 60 composed checks FAIL — the base check and the tileset census, exactly
the two that describe the reader. Restored, verified by `.Contains` and a
forced timestamp (the `Copy-Item` trap), rebuilt, 60 pass.

## 3. Which of (a), (b), (c) I built, and what each needed

| item | built? | what it turned out to need |
|---|---|---|
| (a) sampling material | **no** | terrain to present `MATMODE_BACKED` with a real `{material_set, material_id}` — i.e. item (b). Not a machinery gap; see §5. |
| (b) binding key | **no** | ONE PRODUCER. Not the lookup: MATERIAL.RESOLVE is whole and live (§5). The carrier is measured: `SetEnvironment 0x0311`'s trailing `pad[12]`. |
| (c) reader for the mosaic pick | **YES** | (i) the answer goes on the ADDRESS, not the selector; (ii) a declaration with no carriage cost; (iii) a generation-sealed join. |

**(c)(i) — where the answer lands.** Every previous pass assumed the pick would
displace the BINDING SELECTOR, a tileset as 256 binding rows. The oracle says
otherwise: `zref::Tileset` is `uint8_t tiles[256][64*64]`, ONE memory object,
and `rast.cpp:370` reads `ts->tiles[tile][(ty << 6) + tx]`. A tile index is a
BYTE DISPLACEMENT inside one bound texture. The selector reading also collides
with `sample_index` above one sample. Reading the consumer settled it in a line.

**(c)(ii) — the declaration, and why it is free.** A per-FRAGMENT "this is a
mosaic material" bit HAS NO CARRIAGE, measured: `zhao_texture_v3_request_v2_t`
is 362 bits packed solid with no reserved field and the island's logical
descriptor is 287 bits packed solid, so the bit is the eleven-file `METAW` span
R234 D1 paid for Gouraud. The **binding row's `mode` word** has ELEVEN bits
`binding_row_legal` has always forced to zero (`row.mode[31:21] == 11'd0`), so
`mode[21]` is a mandatory-zero bit whose zero already means "not a tileset" in
every row this console has accepted. Zero keeps its meaning; nothing upstream
changes; the island gains no port.

A tileset row is CONSTRAINED, not merely flagged: CLUT8, unfiltered, mirror on
both axes, 64x64 (which is what makes the TMU's wrap equal `mirror_texel`), no
mip chain, whole 1 MiB extent inside 32 bits. Six single-field deviations are
checked for `CFG_BAD_ROW`.

**(c)(iii) — the join, which is CLAUDE.md's metadata-swap chapter.** The
expander offers a fragment's SAMPLE job and its MOSAIC job on the same beat on
two independent handshakes, and the mosaic answers TWO CYCLES LATER. The sample
is therefore ALWAYS offered before its own pick exists, and an ungated per-owner
table hands the resolver the PREVIOUS OCCUPANT of the slot.
`zhao_texture_mosaic_hold` seals the table with the owner's generation and HOLDS
the sample; the two sides of the comparison are loaded by different enables in
different blocks. No deadlock: the expander's `mosaic_valid_o` does not depend
on `sample_ready_i`, and the island drives the mosaic's `pick_ready_i` with a
constant one.

It is a NAMED LEAF rather than fifteen lines inside the island so the bench
proves the shipping block: `tb_texture_desc_expand_bind_v2_composed` now
instantiates `zhao_texture_mosaic_v2` and `zhao_texture_mosaic_hold`, both
production, and adds no join logic of its own.

## 4. Does a FIELD material write change the intended consumer?

**Not reached, and I will not imply otherwise.** That is the owner's bar for
I34's material half, and it requires (a) and (b) first: until terrain presents a
material identity there is no consumer for a Field material write to change.
I13's material half is likewise **not closed** — it is narrowed, and §5 says by
how much.

## 5. Claims I found FALSE

**In `zhao_material_resolve.sv`'s header, and they are the expensive ones.**
Its four "remaining seams" include *"1. `MEM.UPLOAD` is composed nowhere"* and
it concludes *"THE DIRECTORY WRITE PORT (`dir_*`) AND THE FETCH PORT (`mem_*`)
ARE BOUNDARIES, declared as real ports and driven by nobody in this
composition."* Both are FALSE at this tree:

* `zhao_mem_upload u_mem_upload` IS instantiated in `zhao_console_core.sv`, and
  `dir_we_i` is `upl_publish_valid_o && (upl_publish_tag_o ==
  MAT_KIND_MATERIAL_SET)`, with index, generation, base and a saturating count
  beside it;
* `mem_*` goes through `mr_guard_req`/`mr_guard_rsp` to the real MEM.GUARD as
  `ZHAO_CLIENT_ENGINE1`;
* and **the console smoke already exercises both end to end** — it uploads a
  MATERIAL_SET, `$fatal`s on `mat_not_resident_o != 0`, and reports
  `material responses=1 hits=0 misses=1 not_resident=0 fetch_denied=0`.

**This is why (a)/(b) has been costed as bigger than it is.** MATERIAL.RESOLVE
is whole; terrain lacks an IDENTITY, not a lookup. Corrected in that file, and
carried into its committed mutant copy, in the same commits.

**In I13's own prose:** the entry's "composing `zhao_terrain_normalmap` would
move the register 5 -> 4" is stale by one (it is 4 -> 3). Corrected.

**In the packet protocol's gate table:** the console-board lint row records
"262 / 262, same classes, same counts" measured at `f4b4a653` and
`gz/terrainvisible`. It is **263 at both** `64f2c713` and this branch today —
measured, class for class identical (1 DECLFILENAME, 34 PINCONNECTEMPTY, 209
UNUSEDPARAM, 17 UNUSEDSIGNAL, 2 WIDTHEXPAND). Still an inherited red, still not
any packet's regression; the number in the table has moved by one since it was
written, which is what a count pinned in prose does.

**In a test's own negative control, which had gone vacuous.**
`test_render_texture_packet_e.py`'s `validate_cmake` mutation list contains
`section.replace("list(INSERT ZHAO_PACKET_E_TOP_MUTANT_SOURCES 26", ...)`. When
the real line moved to 27 the "mutation" became a no-op, the mutated text
equalled the original, and the control passed by doing nothing. Repaired, not
baselined.

## 6. What I refused

* **The layer-E triple into `base_rgb`.** CARRIAGE's ground holds — `base_rgb`
  is the published texel RGB at `sample_count == 0`
  (`zhao_texture_material_combine_v3.sv:513`) and `recipe_weight` is the
  `R_LERP` blend weight (:719-722). The inviting rebuttal, that a terrain
  material would be PASSTHRU at count 1 so neither reader is live, is the
  comfortable explanation this campaign says to check hardest: it makes the
  overload safe only for materials somebody remembers to declare that way, and
  nothing refuses the pair. The island's OWN pre-existing overload
  (`wr_mosaic_material_a_i(frag_base_rgb_i[23:16])`) is untouched and was not
  mine to ratify.
* **(a) and (b), on SIZE AND MERGE RISK, not on anything being undecided.**
  Under the campaign's newest law this is a BUILD, and the route is named in
  I13 so nobody re-derives it: `SetEnvironment`'s `pad[12]` (twelve
  mandatory-zero bytes, the `MaterialRecord.fragment_state` pattern, zero =
  today's `MATMODE_NONE`, and the oracle's own per-draw granularity), plus an
  ABI regeneration, CMD.EXEC decode and two output ports (a PINMISSING in every
  bench instantiating it), three input ports on `zhao_terrain_clipfeed`, the
  composer wire, two generated tops — and THEN a fixture that uploads a terrain
  MaterialRecord, a tileset row and tile data. That last item is what moves
  `texture samples` off 1,190, and **it changes the rendered pixels, so whoever
  takes it takes the oracle with it.** Four of those files are the tree's most
  contended and one is the ABI. It is a packet, not a rider.
* No epsilon, clamp or stand-in anywhere. `GEOM_CLIP_ATTRS` stays 7. No flat
  colour stand-in. `zhao_terrain_normalmap` not composed. **No fit was run.**
* **A counter I declined to export, said out loud.** `tileset_samples_o` and the
  hold's two counters are leaf outputs the island does NOT re-export. Exporting
  them means an island port plus a shell/tile-pipe/binner lane for a number that
  is ZERO in the composed console and will stay zero until terrain presents a
  sampling material — an uncashed cheque, not observability. They are fired by
  stimulus and asserted by exact amount at their own benches, and the decision
  is written beside the wire rather than left for
  `v3_closure_inherited.vlt`'s blanket `UNUSEDSIGNAL` waiver to swallow. That
  waiver is how the mosaic pick went unread for weeks; this packet will not add
  to its pile silently.

## 7. What I got wrong and caught myself

* **The first design put the pick on the SELECTOR** (`binding_selector +
  sample_index + pick_tile`). Arithmetically tidy and wrong twice: it needs a
  per-fragment declaration bit that has no carriage, and it collides with
  `sample_index` on any material above one sample. Caught by reading
  `rast.cpp:370` instead of the RTL's shape.
* **The first island edit inlined the per-owner table**, which would have meant
  the composed bench proving a COPY of the join rather than the shipping one.
  Factored into `zhao_texture_mosaic_hold` before anything was committed.
* **I appended to I13 and truncated the register.** A single blank line between
  entries ends `completion_register.py`'s walk, downward, which is the
  direction nobody questions — its own error message says so, and it fired
  immediately. Fixed; the number is measured bare after the fix.
* **The duplicate-name fingerprint moved and I nearly re-pinned it on faith.**
  105 -> 106. Every previous re-pin in that file could say "the count stayed at
  105"; this one cannot, so it owes the one new marker by name. It is
  `member_name='mosaic_tile'`, the field `sample_job_t` gained, and nothing was
  removed — obtained by replaying the committed manifest's own
  `elaboration.argv` in a shadow tree, **with the probe checked against the BASE
  COMMIT first**, where it reproduced 105 / `a95970fe...` exactly. A
  differencing tool that had quietly stopped working could not have produced
  that answer.

## 8. Branch and commits

**`gz/terraintex`**, pushed. Nothing else pushed; no rebase; no `--force`.

| commit | what |
|---|---|
| `847b223a` | the reader: `zhao_texture_mosaic_hold`, the TILESET row, the composed seam bench |
| `4aadc57c` | the texel in the island bench, the I13 section, the two false absences, all manifest/hash refreshes |
| `1f53dfba` | the gen8tag mutant copy refreshed onto the corrected header |

---

## Gates at `1f53dfba`

| gate | result |
|---|---|
| `completion_register.py` (bare) | **4**, unchanged from `64f2c713` |
| `check_console_inventory.py` | OK (406 declared, 291 fit sources) |
| `check_prod_manifest.py` | OK |
| `gen_prod_top.py --check` | fresh (86 instances) |
| `gen_console_board.py --check` | FRESH (1586 core ports) |
| `gen_shell_paired_diff.py --check` | fresh, both halves |
| `check_quartus17_syntax.py` | RC 0, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | OK |
| `check_console_closure_lint.py` (gate 31) | OK, self-test 5/5 |
| `check_entry_claims.py` | no NEW claim |
| `mutant_copy_drift.py` (**after** the commit, R121) | OK, 77 copies |
| `check_texture_v3_interface_manifest.py` | OK — 16 parameters, **120 ports**, 28 ordered sources |
| console smoke | **PASS, `raster pixels=2816`, `frames_admitted=1`** |
| `-Mutant` | PASS — `terr_pl_slot_overflow_o=1`, pixels 2560 |
| `-BadVertex` | PASS — pixels **512**, terrain's two tiles with the mesh refused |
| `-TerrainFlatLattice` | PASS — pixels 2560, `texture fragments=1190` |
| `-NoEchoArm` | PASS |
| `-BadTraceArm` | PASS |
| console-board lint | RC 1, **263 warnings — IDENTICAL to `64f2c713`**, class for class. Inherited. |
| `texture_desc_expand_bind_v2_composed` | **60 checks** (was 52) |
| `texture_desc_expand_bind_v2_pad_control` | PASS |
| `texture_island_v3_packet_b_directed` / `_production_profile` | PASS, outputs 70 |
| packet A/C/D/E + interface-manifest static suites | **no new failures** — each differenced against the base commit, not eyeballed |

`npm run abi:check` was not run: `spec/commands.zidl` was not touched.
