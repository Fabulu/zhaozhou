// zhao_material_resolve.sv -- MATERIAL.RESOLVE: the lookup that turns
// {material_set, material_id, quality_tier} into a MaterialRecord.
//
// ENFORCED-BY: tests/texture/material_resolve_rtl_directed.cpp:main
// ORACLE:      reference/include/zref/zref_material_resolve.hpp
//              (`zref::material::Resolver`, `zref::material::record_legal`)
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS
// ---------------------------------------------------------------------------
// `BORING_3D_FUNDAMENTALS_AUDIT.md` R3 stated the gap as "all the nouns exist
// and the verb does not":
//
//     material_set + material_id  ->  ???  ->  sample_count, recipe, texture
//                                              bases, TMU modes, wrap/mips,
//                                              raster state, toon/ink flags
//
// Both nouns are LIVE IN THIS CONSOLE ALREADY, which is what made the absence
// of the verb expensive rather than theoretical:
//
//   * `material_set` -- `zhao_cmd_exec.sv:331` `draw_material_set_o`, a
//     handle32 lifted whole out of `DrawForm 0x0300`, composed and leaving
//     `zhao_console_core` as `cmd_draw_material_set_o`;
//   * `material_id`  -- `zhao_geom_meshfetch.sv:127` `r_material_id_o`, read
//     off the meshlet descriptor, composed as the core wire
//     `mf_r_material_id`.
//
// Nothing consumed either. `zhao_texture_combine.sv`'s own header lists
// "resolution (MATERIAL.RESOLVE's)" among the things it REFUSES to do, and
// `zhao_geom_attrpack.sv`'s says the flat material request "has no producer in
// this tree either". This block is that producer, for the part of the request
// a MATERIAL RECORD actually owns -- see THE PROJECTION below, which is
// narrower than it first looks and deliberately so.
//
// ---------------------------------------------------------------------------
// THE CARTRIDGE QUESTION (audit R4) IS RULED. IT IS NOT THE BLOCKER.
// ---------------------------------------------------------------------------
// `design/blocks.yml` carried, until this commit, the note "BLOCKED ON A
// CARTRIDGE DECISION (audit R4): .zpak has no generic texture-page or
// material-set kind". That refusal outlived its cause by sixteen days:
//
//   * owner ruling D-2, 2026-09-03, chose option A;
//   * `spec/cartridge.md` lines 65-67 and 102-104 ALLOCATE the kinds --
//     10 `TEXTURE_PAGE` (0x000E), 11 `MATERIAL_SET` (0x000F),
//     12 `MESH_STREAM` (0x0010) -- with §4a describing the nesting;
//   * `spec/commands.zidl:220-262` FROZE the record on 2026-09-05, and the ABI
//     generator emits it as `zhao_abi::ZhMaterialRecord`, 32 B exactly;
//   * `reference/include/zref/zref_material_resolve.hpp` (the oracle) and
//     `tests/texture/material_resolve_directed.cpp` (32 checks) were both
//     written the same day.
//
// So the record is frozen, the kind is allocated, the oracle exists. The
// ledger's `tests: PLANNED -- NOT WRITTEN` was stale on both lines.
//
// ---------------------------------------------------------------------------
// WHAT WAS MISSING (SLOT -> EXTENT) IS RULED; WHAT IS LEFT IS FOUR SEAMS
// ---------------------------------------------------------------------------
// A resolve must read record `material_id` of the table named by handle32
// `{index:24, generation:8}`. That needs the table's BASE ADDRESS and its
// RECORD COUNT.
//
// THE RULING THIS SECTION ASKED FOR WAS MADE, 2026-09-19. `spec/memory_rules.md`
// §5f.1: **a published slot is named by the handle index of the resource it
// holds**, so the residency directory is `{index:24}` keyed with row
// `{slot, base, extent, kind}`. `zhao_mem_upload` now publishes all five --
// `publish_index_o`, `publish_slot_o`, `publish_base_o`, `publish_extent_o` and
// the kind that always travelled as `publish_tag_o`. Base and extent were never
// missing VALUES: that block already took `req_vram_addr_i` and `req_len_i` and
// bounds-checked both against `cfg_region_*` before a byte moved, then dropped
// them on publication. The only genuinely absent field was the KEY, and §5f.1
// is what names it.
//
// SO `dir_*` NOW HAS A LAW AND STILL HAS NO PRODUCER IN THIS COMPOSITION, and
// the reason has changed from a missing decision to four missing SEAMS. Stated
// here so the next reader does not re-derive them or, worse, re-blame the
// ruling:
//
//   1. `MEM.UPLOAD` is composed nowhere. `zhao_hps_arbiter` carries exactly TWO
//      clients and both are taken in both instances -- CMD.DMA and
//      DEBUG.FRAMEBLIT in `zhao_shell_top_v2`, TERRAIN.CMD and
//      TERRAIN.PAGELOADER in `zhao_console_core`'s `u_terr_hps_arb`. A third is
//      an owner ruling, and core entry I27 already records it as one.
//      *** SEAM 1 IS CLOSED AND THE SENTENCE ABOVE IS FALSE. Corrected
//      2026-09-26 (gz/terraintex). `zhao_mem_upload u_mem_upload` IS
//      instantiated, in `zhao_console_core.sv`, and its publication drives
//      THIS BLOCK'S `dir_*` group: `dir_we_i` is `upl_publish_valid_o &&
//      (upl_publish_tag_o == MAT_KIND_MATERIAL_SET)`, with `dir_set_index_i`,
//      `dir_generation_i`, `dir_base_i` and a saturating `dir_count_i` beside
//      it. The clause is left standing so this correction has something to
//      point at. ***
//   2. The fetch port `mem_*` wants a third ENGINE1 requester;
//      `zhao_geom_mem_adapter` has exactly two, GEOM.MESHFETCH and
//      GEOM.ASSETFETCH.
//      *** SEAM 2 IS CLOSED AND THAT SENTENCE IS FALSE TOO, same date. The
//      `mem_*` group goes through `mr_guard_req`/`mr_guard_rsp` to the REAL
//      MEM.GUARD as `ZHAO_CLIENT_ENGINE1`, and the record read is one
//      32-byte burst with an explicit byte-enable shape. ***
//   3. The REQUEST has no honest producer. Both nouns are live in
//      `zhao_console_core` and they may NOT be joined: `cmd_draw_material_set_o`
//      is the DRAW's (entry I41, a boundary) and `mf_r_material_id` is the
//      fetcher's result register, which has moved on by the time the meshlet is
//      offered -- entry I39 refuses exactly that join, by name, because it
//      pairs meshlet N's triangles with meshlet M's material.
//   4. `tri_flat_request_i` wants the binding page's `palette_slot`,
//      `palette_generation` and `response_class` besides, which THE PROJECTION
//      below says plainly this block does not own.
//
// `spec/commands.zidl` still has no command that publishes a material set, and
// that is correct rather than missing: the route is the generic `.zpak`
// resource path, by owner ruling D-2's design.
//
// THEREFORE THE DIRECTORY WRITE PORT (`dir_*`) AND THE FETCH PORT (`mem_*`)
// ARE BOUNDARIES, declared as real ports and driven by nobody in this
// composition.
//
// *** THAT CONCLUSION IS FALSE AT THIS TREE, AND IT IS THE MOST EXPENSIVE
// SENTENCE IN THIS FILE -- it is the one the terrain lane kept reading as
// "a terrain material would need the lookup machinery built first".
// Corrected 2026-09-26 (gz/terraintex). BOTH PORTS ARE DRIVEN, by the two
// seams corrected above, and THE CONSOLE SMOKE EXERCISES THEM END TO END: it
// uploads a MATERIAL_SET through PublishResource, `$fatal`s if
// `mat_not_resident_o != 0` -- "MEM.UPLOAD's publication did not reach the
// directory" -- and its own comment records that the record fetched back "is
// the one the PublishResource uploaded, bit for bit", having crossed the HPS
// bridge, the slot-6 write queue and VRAM and come back as ENGINE1.
//
// So MATERIAL.RESOLVE IS WHOLE AND LIVE. What a new client owes is an
// IDENTITY to present at `zhao_material_window`'s door, not a lookup. For
// terrain that is `zhao_console_core` entry I13's (a) and (b), and the
// carrier measured for it there is `SetEnvironment 0x0311`'s trailing
// `pad[12]`. Seams 3 and 4 below are about the MESH path's request and are
// untouched by this correction. *** That is the honest shape, and it is the same standing
// `zhao_console_core` entry I39 gives GEOM.ASSEMBLE's descriptor fields: the
// block is whole, and the field with no owner is a port rather than a
// constant. Inventing a base here would be worse than leaving it open --
// a wrong address returns a well-formed record for the wrong surface, and
// there is no counter anywhere that can see that.
//
// ---------------------------------------------------------------------------
// D-3: THE CACHE TAG INCLUDES THE RESIDENCY GENERATION
// ---------------------------------------------------------------------------
// Owner ruling D-3, 2026-09-03, in the contract's words:
//
//     cache tag = physical line tag + residency generation
//
// So publishing a new material table makes every cached record from the old
// one STRUCTURALLY unable to match, and no flush is required for correctness.
// The tag here is `{set_index[23:0], set_generation[15:0], material_id[15:0]}`
// and the generation is the DIRECTORY's 16-bit residency generation, not the
// handle's low 8 bits -- MEM.UPLOAD's is 16-bit and the contract says "silent
// wrap is forbidden". Comparing only the 8 bits the handle carries would alias
// every 256th publication, which is precisely the class of fault D-3 exists to
// remove and would be invisible in every counter here.
//
// ---------------------------------------------------------------------------
// THE PROJECTION, AND THE THREE FIELDS THIS BLOCK DOES *NOT* OWN
// ---------------------------------------------------------------------------
// `zhao_console_core` entry I20 says of the 298-bit flat request "Every one of
// those fields is MATERIAL.RESOLVE's output". Read against the layout in
// `zhao_render_texture_pkg.sv`, that is too strong, and shipping it as written
// would have produced a resolver that fights the binding page. What the RECORD
// owns, and what it does not:
//
//   OWNED (emitted below, each straight off a named record offset):
//     sample_count          <- control[1:0]        request [297:296]
//     material_recipe       <- control[4:2]        request [279:277]
//     recipe_weight         <- recipe_weight       request [276:269]
//     base_binding_selector <- sample0.binding_slot request [295:288]
//     palette_base, raster_state, flags, and all three samples' modes
//
//   NOT OWNED, and emitted by nobody here:
//     palette_slot      request [9:8]    | these three are the BINDING PAGE's.
//     palette_generation request [7:0]   | `zhao_texture_binding_resolver_v2`
//     response_class    request [11:10]  | holds them in `binding_row_t`
//                                        | {palette_generation, palette_slot,
//                                        | mode, base} and takes the request's
//                                        | copies as WITNESSES
//                                        | (`req_sample0_*_witness_i`) which it
//                                        | CHECKS (`witness_mismatch_o`). A
//                                        | resolver that invented them would
//                                        | manufacture that mismatch.
//     lod_q4_4          request [287:280] the contract excludes LOD explicitly:
//                                        "It returns the mip policy; the
//                                        sampler picks the level."
//     base_rgb/base_alpha [43:20]/[19:12] vertex colour, not material.
//     aux_surface_ctx   [267:44]         224 bits of terrain world context
//                                        (world X/Z, sheet handle, envelope).
//                                        Zero here is the LEGAL and CORRECT
//                                        non-terrain profile, not a tie-off:
//                                        `zhao_raster_tile_pipe_v2` refuses on
//                                        `flat_request[268] ||
//                                        flat_request[267:44] != 0`.
//
// A BUG THIS PROJECTION FINDS, which is why it is here and not in a composer.
// `MaterialSample.binding_slot` is **u16**; the request's
// `base_binding_selector` is **u8**. The narrowing is real and the consumer
// already has a port for it -- `zhao_texture_binding_resolver_v2` takes
// `req_selector_overflow_i` and counts `selector_overflow_count_o`. So this
// block detects it, reports it on `rsp_selector_overflow_o`, and counts it,
// rather than truncating silently. Truncation would name binding 0 for slot
// 256 and sample the wrong page with every gate green.
//
// ---------------------------------------------------------------------------
// LEGALITY IS THE ORACLE'S, NOT A SECOND OPINION
// ---------------------------------------------------------------------------
// `record_ok_c` below is `zref::material::record_legal` field for field:
// reserved control bits, the recipe ceiling, reserved flag bits, both reserved
// words, and the wrap code of every sample the record CLAIMS. It deliberately
// does NOT implement `zref::material::count_legal` -- the recipe/count pairing
// is the COMBINER's law (`zhao_texture_material_combine_v3` refuses and counts
// it), and duplicating it here would be two implementations of one rule, the
// failure CLAUDE.md names for the projector and for TERRAIN.SHADE/GEOM.LIGHT.
// The pairing is still OBSERVED -- `recipe_count_mismatch_o` counts it without
// refusing -- because a record that resolves cleanly and is then refused
// downstream is exactly the thing a picture cannot show you.
//
// Conservative SystemVerilog subset only; Quartus 17.0 syntax rules apply
// (elaboration checks inside `initial begin`, explicit generate, no inline
// `for (genvar ...)`).
`default_nettype none

module zhao_material_resolve #(
    // The frozen record, `zhao_abi::ZhMaterialRecord`, 32 B. A parameter and
    // not a literal so a future record growth is one edit with an elaboration
    // check behind it -- CLAUDE.md rule 6.
    parameter int unsigned RECW = 256,
    // The fill beat. 64 bits is what `zhao_guard_req_t` traffic already moves,
    // so a record is four beats and not a width nobody else speaks.
    parameter int unsigned BEATW = 64,
    // Directory entries: how many material tables may be resident at once.
    parameter int unsigned SETS = 4,
    // Direct-mapped cache lines. "Materials are resolved per MESHLET, not per
    // fragment, which is what makes a small cache sufficient" -- the contract.
    parameter int unsigned LINES = 16,
    // The ratified combiner recipe ceiling, `zref::material::kRecipeCount`.
    // A PARAMETER because the historical value was 6 and the audit's R1 defect
    // was a record legal to the combiner and refused by the route into it; the
    // directed suite instantiates a second copy at 6 to fire that refusal with
    // legal stimulus rather than with a mutant.
    parameter int unsigned RECIPE_COUNT = 8,
    // The residency generation width. 16 bits, MEM.UPLOAD's own.
    parameter int unsigned GENW = 16,
    parameter int unsigned IDW = 16,
    parameter int unsigned CW = 32
) (
    input  logic clk,
    input  logic rst_n,

    // ---- the residency directory -------------------------------------------
    // BOUNDARY. The LAW now exists -- `spec/memory_rules.md` 5f.1 names this
    // row and `zhao_mem_upload` publishes it -- but MEM.UPLOAD is composed
    // nowhere in this console, so nothing here yet SAYS where a published
    // MATERIAL_SET lives. See the header for the four seams in the way.
    // `dir_entry_i` is a flat 8 bits rather than `$clog2(SETS)`: a width that
    // moves with a parameter is a port whose meaning changes silently when the
    // parameter does, and the smoke bench binds by name with `.*`. Entries at
    // or above SETS are ignored by the write guard below.
    input  logic                     dir_we_i,
    input  logic [7:0]               dir_entry_i,
    input  logic                     dir_valid_i,
    input  logic [23:0]              dir_set_index_i,
    input  logic [GENW-1:0]          dir_generation_i,
    input  logic [31:0]              dir_base_i,
    input  logic [IDW:0]             dir_count_i,

    // ---- the request --------------------------------------------------------
    input  logic                     req_valid_i,
    output logic                     req_ready_o,
    input  logic [31:0]              req_material_set_i,  // {index:24, gen:8}
    input  logic [IDW-1:0]           req_material_id_i,
    // The quality tier is CARRIED, not consulted. It selects a mip policy in a
    // later tier-aware table and there is no such table; reading it here would
    // be a policy invented in a lookup block. It is echoed on the response so a
    // consumer can prove the answer belongs to its question.
    input  logic [7:0]               req_quality_tier_i,

    // ---- the miss fetch -----------------------------------------------------
    // BOUNDARY. Address = base + (material_id << 5); the 32-byte stride is the
    // record's own frozen size, which is why `.zidl` chose a power of two:
    // "a material table is indexed by material_id so a power-of-two stride is
    // a shift".
    output logic                     mem_req_valid_o,
    input  logic                     mem_req_ready_i,
    output logic [31:0]              mem_req_addr_o,
    input  logic                     mem_rsp_valid_i,
    input  logic [BEATW-1:0]         mem_rsp_data_i,
    // THE FETCH WAS DENIED (R20, 2026-09-19). MEM.GUARD answers a refused read
    // with `violation` and NO beats -- `zhao_guard_rsp_t`: "request denied
    // (dropped; NOTHING was written)". Without this input the block sat in
    // S_FILL forever waiting for bytes that could not come. It now resolves to
    // `zref::material::Status::kFetchDenied`, counts it, and caches nothing.
    input  logic                     mem_rsp_denied_i,

    // ---- the resolved record ------------------------------------------------
    output logic                     rsp_valid_o,
    input  logic                     rsp_ready_i,
    output logic [2:0]               rsp_status_o,      // zref::material::Status
    output logic                     rsp_has_record_o,
    output logic [RECW-1:0]          rsp_record_o,
    output logic [7:0]               rsp_quality_tier_o,

    // ---- the projection: the flat request's MATERIAL-OWNED fields -----------
    output logic [1:0]               rsp_sample_count_o,
    output logic [2:0]               rsp_material_recipe_o,
    output logic [7:0]               rsp_recipe_weight_o,
    output logic [7:0]               rsp_base_binding_o,
    output logic                     rsp_selector_overflow_o,
    output logic [31:0]              rsp_palette_base_o,
    output logic [31:0]              rsp_raster_state_o,
    output logic [7:0]               rsp_flags_o,
    output logic [7:0]               rsp_sample0_modes_o,
    output logic [7:0]               rsp_sample1_modes_o,
    output logic [7:0]               rsp_sample2_modes_o,

    // ---- THE FRAGMENT PROFILE (FRAGSTATE, 2026-09-25) -----------------------
    // The material's own declaration for the fragment pipeline, projected beside
    // the rest of the record rather than left inside `rsp_record_o` for a
    // consumer to slice by number. Four ports and not one, because the state
    // word, the tag and the stencil reference travel to three different places
    // downstream and the DECLARED flag selects between this material and the
    // primitive producer's own door declaration.
    //
    // `rsp_frag_declared_o` is the authority selector, NOT a "is it non-zero"
    // test: the all-zero state word is the legal opaque profile, so a consumer
    // cannot infer the declaration from the payload and must read this bit.
    output logic                     rsp_frag_declared_o,
    output logic [31:0]              rsp_frag_state_o,
    output logic [7:0]               rsp_effect_tag_o,
    output logic [7:0]               rsp_stencil_ref_o,

    // ---- evidence -----------------------------------------------------------
    // The ledger's three catalog identities first, then the refusal classes
    // the oracle's `ResolveLedger` separates. `material_refused_o` is their
    // sum and is NOT a fourth independent tally -- a consumer that watches one
    // number gets the right one, and a consumer debugging gets the breakdown.
    output logic [CW-1:0]            material_hits_o,
    output logic [CW-1:0]            material_misses_o,
    output logic [CW-1:0]            material_refused_o,
    output logic [CW-1:0]            refused_id_o,
    output logic [CW-1:0]            refused_record_o,
    output logic [CW-1:0]            not_resident_o,
    output logic [CW-1:0]            selector_overflow_o,
    output logic [CW-1:0]            recipe_count_mismatch_o,
    output logic [CW-1:0]            fetch_denied_o
);

  // ---- derived widths ------------------------------------------------------
  localparam int unsigned BEATS   = RECW / BEATW;
  localparam int unsigned LINEW   = (LINES <= 1) ? 1 : $clog2(LINES);
  localparam int unsigned BEATIXW = (BEATS <= 1) ? 1 : $clog2(BEATS);

  // ---- the record's field offsets, in BITS, from the frozen 32-byte layout --
  // `spec/commands.zidl:242-262`. Byte k of the little-endian record is bits
  // [8k+7 : 8k]. Named constants rather than literals at the use sites: one
  // place to change, and a reader can check them against the .zidl by eye.
  //
  // FIVE OF THESE ARE NOT READ BY ANY EXPRESSION IN THIS FILE, and the waiver
  // says which and why rather than silencing the class. `OFF_S1_SLOT`,
  // `OFF_S1_GEN`, `OFF_S2_SLOT`, `OFF_S2_GEN` and `OFF_S0_GEN` name the
  // per-sample BINDINGS of samples 1 and 2 and sample 0's generation. The
  // 298-bit flat request carries exactly ONE binding --
  // `base_binding_selector`, `zhao_raster_texture_stage_v3.sv:388`
  // `.frag_binding_i(...)` -- so there is no port for the other two and
  // inventing one would be a field with no consumer. They are NOT dropped:
  // they leave whole inside `rsp_record_o`, which is the entire reason that
  // port exists beside the projection. The constants stay because they are the
  // file's map of the frozen layout and a reader checks them against the
  // `.zidl` by eye.
  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned OFF_CONTROL        = 0;    // byte 0
  localparam int unsigned OFF_RECIPE_WEIGHT  = 8;    // byte 1
  localparam int unsigned OFF_FLAGS          = 16;   // bytes 2-3, u16
  localparam int unsigned OFF_S0_SLOT        = 32;   // byte 4-5,  u16
  localparam int unsigned OFF_S0_GEN         = 48;   // byte 6
  localparam int unsigned OFF_S0_MODES       = 56;   // byte 7
  localparam int unsigned OFF_S1_SLOT        = 64;
  localparam int unsigned OFF_S1_GEN         = 80;
  localparam int unsigned OFF_S1_MODES       = 88;
  localparam int unsigned OFF_S2_SLOT        = 96;
  localparam int unsigned OFF_S2_GEN         = 112;
  localparam int unsigned OFF_S2_MODES       = 120;
  localparam int unsigned OFF_PALETTE_BASE   = 128;  // bytes 16-19
  localparam int unsigned OFF_RASTER_STATE   = 160;  // bytes 20-23
  // FRAGSTATE 2026-09-25: these two were OFF_RSV0 / OFF_RSV1. The OFFSETS are
  // unchanged -- that is the whole point of a same-bytes reinterpretation -- and
  // only the names and the legality rules moved. See `spec/commands.zidl`.
  localparam int unsigned OFF_FRAG_STATE     = 192;  // bytes 24-27
  localparam int unsigned OFF_FRAG_DECL      = 224;  // bytes 28-31
  /* verilator lint_on UNUSEDPARAM */

  // ---- `zref::material::Status`, value for value ---------------------------
  localparam logic [2:0] ST_HIT             = 3'd0;
  localparam logic [2:0] ST_MISS            = 3'd1;
  localparam logic [2:0] ST_REFUSED_ID      = 3'd2;
  localparam logic [2:0] ST_REFUSED_RECORD  = 3'd3;
  localparam logic [2:0] ST_NOT_RESIDENT    = 3'd4;
  localparam logic [2:0] ST_FETCH_DENIED    = 3'd5;   // R20

  localparam logic [1:0] S_IDLE  = 2'd0;
  localparam logic [1:0] S_ADDR  = 2'd1;
  localparam logic [1:0] S_FILL  = 2'd2;
  localparam logic [1:0] S_RSP   = 2'd3;

  // Quartus 17.0 rejects a bare module-scope `if`; elaboration checks live in
  // an `initial begin`. `--lint-only` does not RUN initial blocks, so a clean
  // lint says nothing whatever about these -- they are fired by parameterising
  // the block wrongly in the directed suite.
  // synthesis translate_off
  initial begin
    if (RECW != 256)
      $fatal(1, "zhao_material_resolve: RECW must be 256 -- ZhMaterialRecord is 32 B");
    if ((BEATW == 0) || ((RECW % BEATW) != 0))
      $fatal(1, "zhao_material_resolve: BEATW must divide RECW");
    if (SETS == 0)
      $fatal(1, "zhao_material_resolve: SETS must be at least one");
    if ((LINES == 0) || ((LINES & (LINES - 1)) != 0))
      $fatal(1, "zhao_material_resolve: LINES must be a power of two");
    if ((RECIPE_COUNT == 0) || (RECIPE_COUNT > 8))
      $fatal(1, "zhao_material_resolve: RECIPE_COUNT outside the 3-bit recipe field");
    if (GENW < 8)
      $fatal(1, "zhao_material_resolve: GENW must carry the handle's low byte");
  end
  // synthesis translate_on

  // --------------------------------------------------------------------------
  // THE RESIDENCY DIRECTORY
  // --------------------------------------------------------------------------
  logic              dir_v_q     [0:SETS-1];
  logic [23:0]       dir_index_q [0:SETS-1];
  logic [GENW-1:0]   dir_gen_q   [0:SETS-1];
  logic [31:0]       dir_base_q  [0:SETS-1];
  logic [IDW:0]      dir_count_q [0:SETS-1];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < SETS; i++) dir_v_q[i] <= 1'b0;
    end else if (dir_we_i && (int'(dir_entry_i) < SETS)) begin
      dir_v_q[int'(dir_entry_i)]     <= dir_valid_i;
      dir_index_q[int'(dir_entry_i)] <= dir_set_index_i;
      dir_gen_q[int'(dir_entry_i)]   <= dir_generation_i;
      dir_base_q[int'(dir_entry_i)]  <= dir_base_i;
      dir_count_q[int'(dir_entry_i)] <= dir_count_i;
    end
  end

  // The handle split. `zhao_abi_pkg.sv:711` and the oracle's `find()` agree:
  // handle32 is {index:24, generation:8} with the GENERATION IN THE LOW BYTE.
  logic [23:0] req_set_index_c;
  logic  [7:0] req_set_gen8_c;
  assign req_set_index_c = req_material_set_i[31:8];
  assign req_set_gen8_c  = req_material_set_i[7:0];

  logic              dir_hit_c;
  logic [31:0]       dir_hit_base_c;
  logic [IDW:0]      dir_hit_count_c;
  logic [GENW-1:0]   dir_hit_gen_c;

  always_comb begin
    dir_hit_c       = 1'b0;
    dir_hit_base_c  = 32'd0;
    dir_hit_count_c = '0;
    dir_hit_gen_c   = '0;
    for (int i = 0; i < SETS; i++) begin
      // The oracle matches the handle's LOW 8 GENERATION BITS against the
      // table's 16-bit generation truncated the same way, because that is all
      // a 32-bit handle can carry. The full 16 bits are what goes in the CACHE
      // TAG -- see D-3 in the header. The two are different comparisons on
      // purpose and conflating them reintroduces the aliasing D-3 removes.
      if (!dir_hit_c && dir_v_q[i] && (dir_index_q[i] == req_set_index_c) &&
          (dir_gen_q[i][7:0] == req_set_gen8_c)) begin
        dir_hit_c       = 1'b1;
        dir_hit_base_c  = dir_base_q[i];
        dir_hit_count_c = dir_count_q[i];
        dir_hit_gen_c   = dir_gen_q[i];
      end
    end
  end

  // --------------------------------------------------------------------------
  // THE D-3 TAGGED DIRECT-MAPPED CACHE
  //
  // Lines live in flops rather than an M10K because LINES is 16 and each tag
  // comparison is combinational in the accept cycle; a registered RAM read
  // would add a stage to the HIT path, which is the path the whole block exists
  // to make short. The RECORDS are the wide part and they are the ones that
  // want memory -- that trade is stated in the fit note rather than pre-judged
  // here, because LINES*RECW = 4,096 bits is one M10K and the inference depends
  // on the read port shape Quartus picks.
  // --------------------------------------------------------------------------
  logic              ln_v_q     [0:LINES-1];
  logic [23:0]       ln_index_q [0:LINES-1];
  logic [GENW-1:0]   ln_gen_q   [0:LINES-1];
  logic [IDW-1:0]    ln_id_q    [0:LINES-1];
  logic [RECW-1:0]   ln_rec_q   [0:LINES-1];

  logic [LINEW-1:0] line_sel_c;
  assign line_sel_c = req_material_id_i[LINEW-1:0];

  logic cache_hit_c;
  assign cache_hit_c = ln_v_q[line_sel_c] &&
                       (ln_index_q[line_sel_c] == req_set_index_c) &&
                       (ln_gen_q[line_sel_c]   == dir_hit_gen_c) &&
                       (ln_id_q[line_sel_c]    == req_material_id_i);

  // --------------------------------------------------------------------------
  // LEGALITY -- `zref::material::record_legal`, field for field
  // --------------------------------------------------------------------------
  //
  // BOTH FUNCTIONS TAKE THE WHOLE RECORD AND READ PART OF IT, which is the
  // point: legality is a property of the record, and passing only the fields
  // the current rules happen to inspect would mean editing the signature every
  // time a rule is added. The unread bits are the payload fields -- bindings,
  // palette base, raster state -- which legality has no opinion about, and
  // `flags[2:0]` is toon/ink/alpha_test, which are MEANINGFUL and therefore
  // deliberately not part of the reserved-must-be-zero test.
  /* verilator lint_off UNUSEDSIGNAL */
  function automatic logic record_ok_f(input logic [RECW-1:0] r);
    logic [7:0]  ctrl;
    logic [1:0]  count;
    logic [2:0]  recipe;
    logic [15:0] flags;
    logic        ok;
    logic  [7:0] modes [0:2];
    begin
      ctrl     = r[OFF_CONTROL +: 8];
      count    = ctrl[1:0];
      recipe   = ctrl[4:2];
      flags    = r[OFF_FLAGS +: 16];
      modes[0] = r[OFF_S0_MODES +: 8];
      modes[1] = r[OFF_S1_MODES +: 8];
      modes[2] = r[OFF_S2_MODES +: 8];
      ok       = 1'b1;
      // control bits 5-7 reserved and MUST be zero: a non-zero reserved field
      // is a record from a newer format being read by older logic.
      if (ctrl[7:5] != 3'd0) ok = 1'b0;
      // the combiner's own ceiling, carried as a parameter rather than a
      // literal so it cannot drift from `zref::material::kRecipeCount` the way
      // the oracle's `recipe >= 6` once did.
      if ({1'b0, recipe} >= RECIPE_COUNT[3:0]) ok = 1'b0;
      // flags bits 3-15 reserved 0.
      if (flags[15:3] != 13'd0) ok = 1'b0;
      // THE FRAGMENT PROFILE (FRAGSTATE, 2026-09-25). These two words were
      // `rsv0`/`rsv1`, both refused-if-nonzero; the owner vacation directive of
      // 2026-09-23 section 3 allocates them as the material's fragment-pipeline
      // declaration. `spec/commands.zidl`'s MaterialRecord carries the layout,
      // `zref::material::record_legal` is the mirror of these three lines, and
      // the two are differenced by `material_resolve_rtl_directed`.
      //
      // fragment_decl bits 1-7 and 24-31 stay reserved and MUST be zero.
      if (r[OFF_FRAG_DECL + 1 +: 7]  != 7'd0) ok = 1'b0;
      if (r[OFF_FRAG_DECL + 24 +: 8] != 8'd0) ok = 1'b0;
      // THE CONTRADICTORY DECLARATION, REFUSED WHOLE. Bit 0 is the EXPLICIT
      // selector: the all-zero state word is a legal, meaningful profile (the
      // plain opaque write), so "is the word zero?" cannot select and the flag
      // must. A record that fills the state and forgets the flag is refused
      // rather than masked, because masking would draw it opaque and say
      // nothing -- `SetPost.flags`'s rule, not `compose`'s silent mask.
      if (!r[OFF_FRAG_DECL] &&
          ((r[OFF_FRAG_STATE +: 32]      != 32'd0) ||
           (r[OFF_FRAG_DECL + 8 +: 16]   != 16'd0))) ok = 1'b0;
      // Every sample the record CLAIMS must have a legal wrap code; samples
      // beyond the count are NOT inspected, because they are not read. Wrap 3
      // is reserved by the frozen layout.
      for (int i = 0; i < 3; i++)
        if ((i < int'(count)) && (modes[i][5:4] == 2'd3)) ok = 1'b0;
      record_ok_f = ok;
    end
  endfunction

  // `count_legal`'s pairing, OBSERVED and never enforced -- see the header.
  function automatic logic count_paired_f(input logic [RECW-1:0] r);
    logic [7:0] ctrl;
    logic [1:0] count;
    logic [2:0] recipe;
    begin
      ctrl   = r[OFF_CONTROL +: 8];
      count  = ctrl[1:0];
      recipe = ctrl[4:2];
      if (recipe == 3'd0)                         count_paired_f = (count <= 2'd1);
      else if ((recipe == 3'd6) || (recipe == 3'd7)) count_paired_f = (count == 2'd3);
      else                                        count_paired_f = (count == 2'd2);
    end
  endfunction
  /* verilator lint_on UNUSEDSIGNAL */

  // --------------------------------------------------------------------------
  // THE FLOW
  // --------------------------------------------------------------------------
  logic [1:0]        state_q;
  logic [RECW-1:0]   rec_q;
  logic [2:0]        status_q;
  logic              has_rec_q;
  logic [7:0]        tier_q;
  logic [IDW-1:0]    id_q;
  logic [23:0]       set_index_q;
  logic [GENW-1:0]   set_gen_q;
  logic [31:0]       addr_q;
  logic [BEATIXW-1:0] beat_q;

  assign req_ready_o = (state_q == S_IDLE);
  assign rsp_valid_o = (state_q == S_RSP);

  assign rsp_status_o       = status_q;
  assign rsp_has_record_o   = has_rec_q;
  assign rsp_record_o       = rec_q;
  assign rsp_quality_tier_o = tier_q;

  assign mem_req_valid_o = (state_q == S_ADDR);
  assign mem_req_addr_o  = addr_q;

  // ---- THE PROJECTION ------------------------------------------------------
  // Gated on `has_rec_q` so a refusal presents zeros rather than the previous
  // record's fields. That matters more than it looks: a consumer that ignores
  // `rsp_status_o` and reads the projection would otherwise draw the LAST
  // material on a refusal, which is exactly the "guessed material draws the
  // wrong surface confidently" the contract's stall rule exists to forbid.
  logic [15:0] s0_slot_c;
  assign s0_slot_c = rec_q[OFF_S0_SLOT +: 16];

  assign rsp_sample_count_o    = has_rec_q ? rec_q[OFF_CONTROL + 0 +: 2] : 2'd0;
  assign rsp_material_recipe_o = has_rec_q ? rec_q[OFF_CONTROL + 2 +: 3] : 3'd0;
  assign rsp_recipe_weight_o   = has_rec_q ? rec_q[OFF_RECIPE_WEIGHT +: 8] : 8'd0;
  assign rsp_base_binding_o    = has_rec_q ? s0_slot_c[7:0] : 8'd0;
  assign rsp_selector_overflow_o = has_rec_q && (s0_slot_c[15:8] != 8'd0);
  assign rsp_palette_base_o    = has_rec_q ? rec_q[OFF_PALETTE_BASE +: 32] : 32'd0;
  assign rsp_raster_state_o    = has_rec_q ? rec_q[OFF_RASTER_STATE +: 32] : 32'd0;
  assign rsp_flags_o           = has_rec_q ? rec_q[OFF_FLAGS +: 8] : 8'd0;
  assign rsp_sample0_modes_o   = has_rec_q ? rec_q[OFF_S0_MODES +: 8] : 8'd0;
  assign rsp_sample1_modes_o   = has_rec_q ? rec_q[OFF_S1_MODES +: 8] : 8'd0;
  assign rsp_sample2_modes_o   = has_rec_q ? rec_q[OFF_S2_MODES +: 8] : 8'd0;

  // The fragment profile, on the SAME `has_rec_q` gate as every field above, so
  // a refusal presents "no declaration" rather than the previous material's
  // profile. A stale `declared` would be worse than a stale state word: the
  // consumer would take a dead material's blend mode as authoritative over the
  // live producer's own.
  assign rsp_frag_declared_o   = has_rec_q && rec_q[OFF_FRAG_DECL];
  assign rsp_frag_state_o      = has_rec_q ? rec_q[OFF_FRAG_STATE +: 32] : 32'd0;
  assign rsp_effect_tag_o      = has_rec_q ? rec_q[OFF_FRAG_DECL + 8  +: 8] : 8'd0;
  assign rsp_stencil_ref_o     = has_rec_q ? rec_q[OFF_FRAG_DECL + 16 +: 8] : 8'd0;

  // The assembled fill, one beat at a time, low beat first (little-endian, the
  // same order `spec/commands.zidl` lays the record out in).
  logic [RECW-1:0] fill_c;
  always_comb begin
    fill_c = rec_q;
    fill_c[beat_q * BEATW +: BEATW] = mem_rsp_data_i;
  end

  logic fill_last_c;
  assign fill_last_c = (int'(beat_q) == (BEATS - 1));

  logic [CW-1:0] hits_q, misses_q, refid_q, refrec_q, nores_q, selov_q, cntmm_q, denied_q;
  assign fetch_denied_o          = denied_q;
  assign material_hits_o         = hits_q;
  assign material_misses_o       = misses_q;
  assign refused_id_o            = refid_q;
  assign refused_record_o        = refrec_q;
  assign not_resident_o          = nores_q;
  assign selector_overflow_o     = selov_q;
  assign recipe_count_mismatch_o = cntmm_q;
  // The catalog identity: every refusal, one number. Combinational over the
  // three tallies rather than a fourth register, so it cannot drift from them.
  // A denied fetch is a refusal too: the resolve produced no record.
  assign material_refused_o      = refid_q + refrec_q + nores_q + denied_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q     <= S_IDLE;
      rec_q       <= '0;
      status_q    <= ST_NOT_RESIDENT;
      has_rec_q   <= 1'b0;
      tier_q      <= 8'd0;
      id_q        <= '0;
      set_index_q <= 24'd0;
      set_gen_q   <= '0;
      addr_q      <= 32'd0;
      beat_q      <= '0;
      hits_q      <= '0;
      misses_q    <= '0;
      refid_q     <= '0;
      refrec_q    <= '0;
      nores_q     <= '0;
      selov_q     <= '0;
      cntmm_q     <= '0;
      denied_q    <= '0;
      for (int i = 0; i < LINES; i++) ln_v_q[i] <= 1'b0;
    end else begin
      case (state_q)
        S_IDLE: begin
          if (req_valid_i) begin
            tier_q      <= req_quality_tier_i;
            id_q        <= req_material_id_i;
            set_index_q <= req_set_index_c;
            set_gen_q   <= dir_hit_gen_c;
            if (!dir_hit_c) begin
              // A residency fault, not a stall-forever. The contract puts it in
              // the frame-publication law's hands: the frame is not published
              // and the previous one repeats.
              status_q  <= ST_NOT_RESIDENT;
              has_rec_q <= 1'b0;
              rec_q     <= '0;
              if (nores_q  != {CW{1'b1}}) nores_q  <= nores_q + 1;
              state_q   <= S_RSP;
            end else if ({1'b0, req_material_id_i} >= dir_hit_count_c) begin
              // NOT clamped to zero: material 0 is a real material and drawing
              // with it hides the bug.
              status_q  <= ST_REFUSED_ID;
              has_rec_q <= 1'b0;
              rec_q     <= '0;
              if (refid_q  != {CW{1'b1}}) refid_q  <= refid_q + 1;
              state_q   <= S_RSP;
            end else if (cache_hit_c) begin
              status_q  <= ST_HIT;
              has_rec_q <= 1'b1;
              rec_q     <= ln_rec_q[line_sel_c];
              if (hits_q   != {CW{1'b1}}) hits_q   <= hits_q + 1;
              if (ln_rec_q[line_sel_c][OFF_S0_SLOT + 8 +: 8] != 8'd0)
                if (selov_q != {CW{1'b1}}) selov_q <= selov_q + 1;
              if (!count_paired_f(ln_rec_q[line_sel_c]))
                if (cntmm_q != {CW{1'b1}}) cntmm_q <= cntmm_q + 1;
              state_q   <= S_RSP;
            end else begin
              // base + (material_id << 5): the record's own 32-byte stride.
              addr_q  <= dir_hit_base_c + ({{(32-IDW){1'b0}}, req_material_id_i} << 5);
              beat_q  <= '0;
              state_q <= S_ADDR;
            end
          end
        end

        S_ADDR: begin
          if (mem_req_ready_i) state_q <= S_FILL;
        end

        S_FILL: begin
          // The guard's verdict arrives the cycle AFTER it accepted the
          // request, i.e. here. A denial ends the resolve: no record, not
          // cached, counted -- the defined fault R20 asks for.
          if (mem_rsp_denied_i) begin
            status_q  <= ST_FETCH_DENIED;
            has_rec_q <= 1'b0;
            if (denied_q != {CW{1'b1}}) denied_q <= denied_q + 1;
            state_q   <= S_RSP;
          end else if (mem_rsp_valid_i) begin
            rec_q <= fill_c;
            if (fill_last_c) begin
              if (record_ok_f(fill_c)) begin
                status_q  <= ST_MISS;
                has_rec_q <= 1'b1;
                if (misses_q != {CW{1'b1}}) misses_q <= misses_q + 1;
                if ((fill_c[OFF_S0_SLOT + 8 +: 8] != 8'd0) && (selov_q != {CW{1'b1}}))
                  selov_q <= selov_q + 1;
                if (!count_paired_f(fill_c) && (cntmm_q != {CW{1'b1}}))
                  cntmm_q <= cntmm_q + 1;
                // A malformed stored record is NEVER cached -- so a later hit
                // cannot serve what this resolve just refused.
                ln_v_q[id_q[LINEW-1:0]]     <= 1'b1;
                ln_index_q[id_q[LINEW-1:0]] <= set_index_q;
                ln_gen_q[id_q[LINEW-1:0]]   <= set_gen_q;
                ln_id_q[id_q[LINEW-1:0]]    <= id_q;
                ln_rec_q[id_q[LINEW-1:0]]   <= fill_c;
              end else begin
                status_q  <= ST_REFUSED_RECORD;
                has_rec_q <= 1'b0;
                if (refrec_q != {CW{1'b1}}) refrec_q <= refrec_q + 1;
              end
              state_q <= S_RSP;
            end else begin
              beat_q <= beat_q + 1;
            end
          end
        end

        S_RSP: begin
          // The record is NOT cleared on retire. `rsp_valid_o` gates the whole
          // response and `has_rec_q` gates the projection, so a held payload
          // behind a deasserted valid is invisible; clearing 256 flops every
          // response is real toggling that buys nothing. The next accepted
          // request overwrites both before it raises valid again.
          if (rsp_ready_i) state_q <= S_IDLE;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_material_resolve

`default_nettype wire
