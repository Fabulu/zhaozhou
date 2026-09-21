// zhao_geom_clipread.sv -- the kind-8 BODY and kind-9 CLIP FRAME page reader
// that fills `zhao_geom_bonesrc`. Entry I29 of `zhao_console_core.sv`.
//
// ENFORCED-BY: tests/geometry/geom_clipread_directed.cpp:main
// REFERENCE:   zref::creature_page::body  (reference/include/zref/zref_creature_page.hpp)
//              zref::clip_page            (reference/include/zref/zref_clip_page.hpp)
// GOLDENS:     tests/golden/creature_ladder/ladder_page_body_v1.bin
//              tests/golden/creature_clip/clip_page_v1.bin
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AND WHY IT IS NOT AN ABI DECISION
// ---------------------------------------------------------------------------
// `zhao_geom_pose_decode` needs, per bone, a parent, a rest translation, a
// quaternion and an inverse-rest matrix. `zhao_geom_bonesrc` is the STORE that
// answers those combinationally. Nothing FILLED it. This is the filler.
//
// Entry I29 named the remaining work as "the kind-8/kind-9 page reader, the
// sixth `u_geom_mem_adapter` requester, the form -> type resolution, and the
// instance walk", and packet POSECMD stopped before the reader on the grounds
// that section 4.2's RESIDENT-HANDLE LAYER had no ruling. IT HAS ONE, and the
// ruling is not in that architecture document at all:
//
//   `spec/memory_rules.md` 5f.1, ruled 2026-09-19, freezes the representation
//   for every resource published by MEM.UPLOAD under a handle32:
//
//       key   {index:24}
//       row   {slot:8, base:32, extent:32, kind:8}   + generation:16
//
//   and FOUR blocks in this tree already carry that shape. Stated exactly,
//   because "already implemented" is the kind of claim this tree keeps finding
//   half-true:
//
//     PRODUCER   `zhao_mem_upload`'s `publish_{valid,slot,generation,tag,
//                index,base,extent}` -- COMPOSED (`zhao_console_core`).
//     CONSUMER   `zhao_geom_drawjob`'s `dir_{we,entry,index,generation,base,
//                extent}` -- COMPOSED, and driven from exactly those ports.
//     CONSUMER   `zhao_material_resolve`'s `dir_{we,entry,set_index,
//                generation,base,count}` -- BUILT, not composed.
//     CONSUMER   `zhao_geom_ladderbank`'s `pub_{valid,tag,base,extent}` --
//                BUILT, not composed, and parked behind R133 besides.
//
//   So the rule has one composed producer and one composed consumer, and this
//   block is its fifth instance rather than a fourth opinion about resident
//   handles.
//
// **THE EPOCH IS NOT ON THIS PORT, AND THAT IS A MECHANISM RATHER THAN AN
// OMISSION.** Section 4.2 lists `generation` and `resource_epoch` on the
// conceptual handle, and a reader that carried an epoch input would be a
// SECOND owner of a check that has already been made: `zhao_mem_upload`
// refuses a stale-epoch upload outright -- `req_epoch_i != cfg_epoch_i` yields
// `V_EPOCH_STALE` -- and a refusal never reaches `publish_valid_o`. So a row
// carrying a dead epoch cannot exist to be read, which is why 5f.1's field
// list has no epoch in it.
//
// ---------------------------------------------------------------------------
// THE ONE LAW THIS BLOCK EXISTS TO OBEY: FILL WHOLE, THEN ASK
// ---------------------------------------------------------------------------
// `tests/mutants/zhao_geom_bonesrc_latefetch_mutant.sv` models "a page read
// that misses can easily exceed 115 cycles" against the decoder's MEASURED
// 115.4 cycles per bone. A reader that fetched per bone BEHIND the decode
// would turn `bone_prefetch_late_o` from a dead counter into a live fault --
// which is the one thing that mutant exists to say.
//
// So `src_req_o` rises only when BOTH stores hold whole, consistent data:
// the skeleton adopted from a kind-8 body, and every quaternion of the
// requested frame written from a kind-9 page. Nothing in this block issues a
// read after `src_req_o`. That is the same law `zhao_geom_ladderbank` obeys
// when it adopts a page before it answers a lookup, and entry I29 cites that
// block by name as the pattern.
//
// ---------------------------------------------------------------------------
// TWO PUBLICATIONS, TWO STORES, AND A DETECTOR ACROSS THEM
// ---------------------------------------------------------------------------
// The skeleton is per creature TYPE and the quaternions are per FRAME, so
// `zhao_geom_bonesrc` keeps them in two stores selected by `fill_sel`. They
// arrive here from two INDEPENDENT publications -- kind 8 and kind 9 -- and
// each declares its own bone count: byte 6 of the body header, byte 6 of the
// clip page header.
//
// `bone_mismatch_o` differences those two counts, and it is deliberately NOT
// the blind kind of checker `CLAUDE.md` opens with. The two operands are
// loaded by two different publications through two different states; no single
// register enable moves both, so a kind-9 page adopted against another
// creature's skeleton MOVES IT. A frame decoded against the wrong skeleton is
// a well-formed palette for the wrong animal with every handshake intact --
// this repository's named worst case -- so it is refused and counted, never
// decoded.
//
// ---------------------------------------------------------------------------
// WHAT IS DELIBERATELY NOT HERE: form -> CLIP BANK
// ---------------------------------------------------------------------------
// This block holds ONE resident creature: one skeleton, one clip directory.
// That is `zhao_geom_bonesrc`'s OWN tier -- that block stores exactly one
// skeleton and one frame -- so it is the store's tier restated, not a
// narrowing of anything.
//
// Raising the tier needs a fact the tree does not contain. `zhao_geom_drawjob`
// resolves `form` -> MESH_STREAM page through 5f.1's directory, and
// `zref::creature_page::Record` keys each kind-8 LADDER row by a
// `form_index`. **`zref::clip_page`'s header is
// {magic, version, bone_count, clip_count, dir_off, frames_off} and carries no
// form index, no creature-type key and no handle of any kind.** Nothing
// anywhere says which clip bank belongs to which form. Inventing that mapping
// here -- "the bank publishes under the form's index", say -- would be this
// block choosing an ABI the spec declines to define, which is the refusal
// entries I7, I33 and CMD.EXEC's draw arm already carry. It is filed as an
// owner decision instead.
//
// CORRECTED 2026-09-21 (FORMIDX). THE SENTENCE ABOVE USED TO END "-- so
// kind-8 -> form IS ruled", AND THAT IS TRUE OF THE LADDER TABLE AND FALSE OF
// THE BODY -- which is the half THIS BLOCK READS. It is exactly the shape
// POSEPAGE found one layer up ("true of a FRAME and false of a PAGE"),
// repeated inside the page it was found on, and it matters because the
// costing of the owner decision rested on it.
//
// MEASURED, not inferred, from the committed golden
// `tests/golden/creature_ladder/ladder_page_body_v1.bin`: 448 bytes,
// THREE ladder records -- form_index 256, 257 and 40983 -- and `body_off`
// 192 naming exactly ONE 6-bone body section. `tools/pack/mkcreatureladder.py`
// makes that the FORMAT rather than the fixture: `build(records, body_off=0,
// bones=None)` takes a LIST of records and a SINGLE bone list, and
// `build_body` packs "<IHBBII" -- magic, version, bone_count, flags,
// bones_off, reserved. **There is no form index anywhere in the body header.**
//
// So one kind-8 page is an N-form LADDER TABLE plus a 1-creature SKELETON, and
// the two halves have DIFFERENT CARDINALITY. `zhao_geom_ladderbank` reads the
// first half as a bank of ROWS=16 creature types keyed by `form_index`;
// `spec/creature_rules.md` 5 describes kind 8 as ONE creature ("compiled part
// table, bone hierarchy (parent-before-child, <=32), attachment points, hitbox
// class + per-bone 8-corner hitboxes"). Both readings are live in this tree and
// they disagree.
//
// THE CONSEQUENCE FOR THIS BLOCK: `res_body_index_o` and `res_clip_index_o`
// carry each resident page's PUBLICATION index, never a form index, and
// nothing relates either to the draw's form. The wrong-animal failure this
// section describes is therefore available on the SKELETON exactly as it is on
// the clip bank, and `bone_mismatch_o` is equally blind to it when the bone
// counts agree. A ruling that gives only the kind-9 header a form key closes
// HALF of this.
//
// Conservative SystemVerilog subset only (charter 2). No divider; the one
// multiply is the frame stride, which is a small unsigned product.
`default_nettype none

module zhao_geom_clipread
  import zhao_pkg::*;
#(
    // Clip directory rows held for the resident bank. NAMED AND EDITABLE
    // (CLAUDE.md rule 6). `zref::clip_page::kMaxClips` is 64 authored slots;
    // 16 is the working set a resident creature draws from, and a page
    // declaring more rows than this is REFUSED WHOLE (`overflow_o`), never
    // truncated -- a truncated directory answers the wrong frame offset for
    // every slot past the cut.
    parameter int unsigned CLIP_ROWS = 16,

    // `spec/creature_rules.md` 1.2's ceiling, and `zhao_geom_bonesrc`'s.
    parameter int unsigned MAX_BONES = 32,

    // `spec/cartridge.md` 4's page-kind registry, which is what `publish_tag_o`
    // carries (5f.1 names it "the `.zpak` resource kind").
    parameter logic [7:0] BODY_KIND = 8'd8,   // CREATURE_FORM
    parameter logic [7:0] CLIP_KIND = 8'd9,   // CLIP_BANK

    // The asset window's client, the one MEM.GUARD grants the pool to. The
    // same identity `zhao_geom_ladderbank` and `zhao_part_table_loader` read
    // under; production substitutes it at the shared requester anyway.
    parameter zhao_client_e CLIENT = ZHAO_CLIENT_ENGINE1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- MEM.UPLOAD's publication (spec/memory_rules.md 5f.1, whole) -------
    // `pub_index_i` and `pub_generation_i` are the resident handle's identity.
    // They are STORED with the page and reported on `res_*_o` so a consumer
    // can tell which resource answered; this block does not compare them,
    // because the publication IS the validation -- see the header.
    input  var logic        pub_valid_i,
    input  var logic [ 7:0] pub_tag_i,
    input  var logic [23:0] pub_index_i,
    input  var logic [15:0] pub_generation_i,
    input  var logic [31:0] pub_base_i,
    input  var logic [31:0] pub_extent_i,

    // ---- the pose request, from the instance walk --------------------------
    // `DrawPosedForm 0x0305`'s {clip_id, frame_no} (owner ruling R229). `sub`
    // is the pose cache's key discriminator and is not read here: this block
    // fetches an AUTHORED frame, and the 60 Hz midpoint is the decoder's
    // business.
    input  var logic        p_valid_i,
    output var logic        p_ready_o,
    // THE DRAW'S OWN FORM. `zhao_geom_drawjob.j_form_idx_o`, which is that
    // block's `form_idx_q` = `d_form_i[31:8]` -- the MESH_STREAM handle index
    // `spec/memory_rules.md` 5f.1 keys residency by and
    // `zref_creature_page.hpp` names as THE hardware per-creature-type key.
    // All 24 bits: the producer gates it by state (it is offered only while
    // the job is emitting) precisely because in its own S_IDLE the register
    // still holds the PREVIOUS draw.
    //
    // It is the third operand of this block's ownership comparison, and the
    // one that makes the comparison mean anything. `body_owner_q` and
    // `clip_owner_q` are loaded by two DIFFERENT publications in two
    // DIFFERENT states, and this arrives on a third path -- so no single
    // register enable moves two sides of the compare together, which is the
    // blind-detector shape this tree refuses.
    input  var logic [23:0] p_form_idx_i,
    input  var logic [15:0] p_clip_id_i,
    input  var logic [15:0] p_frame_no_i,

    // ---- the asset read window, through the shared requester ---------------
    output var zhao_guard_req_t g_req_o,
    // `ok` is not read: a violation is the verdict this block acts on, and a
    // request that is neither ready nor violating is still waiting. The law is
    // `tools/rtl/check_guard_verdict.py`'s -- `ready` and `ok` are NEVER high
    // together, so a client testing both in one cycle reads every pass as a
    // denial.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var zhao_guard_rsp_t g_rsp_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic            g_beat_valid_i,
    input  var logic [63:0]     g_beat_data_i,

    // ---- the fill into zhao_geom_bonesrc ------------------------------------
    output var logic        fill_we_o,
    output var logic        fill_sel_o,     // 0 = kind-8 body, 1 = kind-9 quats
    output var logic [ 4:0] fill_bone_o,
    output var logic [ 3:0] fill_word_o,
    output var logic [63:0] fill_data_o,

    // ---- the handover to zhao_geom_bonesrc ----------------------------------
    output var logic        src_req_o,
    output var logic [ 5:0] src_bone_count_o,
    input  var logic        src_ready_i,

    // ---- the frame's root displacement, into zhao_geom_pose_decode ---------
    // `creature_rules` 2.1's first twelve bytes of the frame, held from the
    // fill until the next frame replaces them.
    output var logic signed [31:0] root_dx_o,
    output var logic signed [31:0] root_dy_o,
    output var logic signed [31:0] root_dz_o,

    // ---- which resource answered (5f.1's identity, carried through) --------
    output var logic [23:0] res_body_index_o,
    output var logic [15:0] res_body_gen_o,
    output var logic [23:0] res_clip_index_o,
    output var logic [15:0] res_clip_gen_o,

    // ---- WHICH FORM each resident section says it belongs to ---------------
    // The 2026-09-21 ownership ruling's three identities, kept apart: these
    // are FORM identity (the 24-bit MESH_STREAM index the page NAMES), while
    // `res_*_index_o`/`res_*_gen_o` above are RESOURCE identity (which
    // publication answered). They are not the same number and are not derived
    // from one another -- a body and its clip bank are published under two
    // independent resource indices and still name ONE form.
    output var logic [23:0] res_body_owner_o,
    output var logic [23:0] res_clip_owner_o,

    // ---- evidence -----------------------------------------------------------
    output var logic [31:0] bodies_o,         // kind-8 skeletons adopted whole
    output var logic [31:0] clips_o,          // kind-9 directories adopted whole
    output var logic [31:0] frames_o,         // frames filled and handed over
    output var logic [31:0] pages_dropped_o,  // a publication while busy
    output var logic [31:0] bad_magic_o,      // magic or version wrong
    output var logic [31:0] truncated_o,      // a declared extent past the page
    output var logic [31:0] misaligned_o,     // an offset off the 64-byte grid
    output var logic [31:0] bad_bone_count_o, // 0, or past MAX_BONES
    output var logic [31:0] bone_mismatch_o,  // body and clip disagree; see header
    output var logic [31:0] overflow_o,       // more clips than CLIP_ROWS
    output var logic [31:0] not_rigid_o,      // a body claiming a rest rotation
    output var logic [31:0] reserved_nz_o,    // a reserved field is not zero
    output var logic [31:0] denied_o,         // MEM.GUARD refused a read
    output var logic [31:0] clip_miss_o,      // a request whose slot has no row
    output var logic [31:0] frame_oob_o,      // frame_no at or past frame_count
    output var logic [31:0] not_resident_o,   // a request before both stores hold
    // A request whose form is not the form the resident body and clip bank
    // NAME. REFUSED AND COUNTED, never treated as a clip miss: a miss is a
    // question this bank could have answered, and this is a question it must
    // not answer. Distinct from `bone_mismatch_o`, which is blind to exactly
    // this whenever the two creatures happen to have the same bone count.
    output var logic [31:0] owner_mismatch_o,
    output var logic        busy_o
);

  localparam int unsigned LINE_BYTES  = 64;
  localparam int unsigned BONE_BYTES  = 32;
  localparam int unsigned QUAT_BYTES  = 8;
  localparam int unsigned CLIP_BYTES  = 32;
  localparam int unsigned HDR_BYTES   = 64;

  // 'Z','C','F','M' little-endian -- the kind-8 page header.
  localparam logic [31:0] FORM_MAGIC = 32'h4D46435A;
  // 'T','C','B','8' little-endian -- the kind-8 BODY section (zref::…::body).
  localparam logic [31:0] BODY_MAGIC = 32'h38424354;
  // 'Z','C','L','P' little-endian -- the kind-9 page header.
  localparam logic [31:0] CLIP_MAGIC = 32'h504C435A;

  // THREE VERSIONS, CHECKED SEPARATELY. The 2026-09-21 ownership ruling keeps
  // the OUTER kind-8 header and the ladder format at 1 and introduces BODY 2
  // and CLIP_BANK 2, and says why this cannot be one constant: "merely
  // changing that single constant to 2 would reject the still-v1 outer
  // header." It WAS one constant here until this packet.
  localparam logic [15:0] FORM_VERSION = 16'd1;   // kind-8 outer page / ladder
  localparam logic [15:0] BODY_VERSION = 16'd2;   // kind-8 BODY section
  localparam logic [15:0] CLIP_VERSION = 16'd2;   // kind-9 CLIP_BANK page

  // The owner word stores a SEMANTIC u24 in one aligned little-endian u32.
  // Bits 31:24 MUST be zero -- a page setting them is refused, so the 24-bit
  // value can never be read out of 32 meaningful bits.
  localparam logic [7:0] OWNER_RSV_ZERO = 8'd0;

  localparam int unsigned ROWW = (CLIP_ROWS <= 1) ? 1 : $clog2(CLIP_ROWS);

  // `zref::creature_page::body::kFlagRigidRest`.
  localparam logic [7:0] FLAG_RIGID = 8'h01;

  // Quartus 17.0 rejects a bare module-scope `if`; the guard lives inside
  // `initial begin ... end`, and `--lint-only` does not run it, so a clean lint
  // is not evidence about it (CLAUDE.md, 2026-09-08). Both are fired by
  // parameter override in the directed test.
  initial begin
    if (CLIP_ROWS < 1 || CLIP_ROWS > 64)
      $fatal(1, "zhao_geom_clipread: CLIP_ROWS is %0d; zref::clip_page::kMaxClips is 64",
             CLIP_ROWS);
    if (MAX_BONES != 32)
      $fatal(1, "zhao_geom_clipread: MAX_BONES is %0d; bonesrc's bone index is 5 bits",
             MAX_BONES);
  end

  // ---- the line being assembled -------------------------------------------
  logic [511:0] line_q;
  logic [  2:0] beat_q;

  // ---- the resident BODY (kind 8) -----------------------------------------
  logic        body_v_q;        // a whole skeleton is in bonesrc's sel-0 store
  logic [ 5:0] body_bones_q;
  logic [23:0] body_owner_q;    // the FORM this skeleton says it belongs to

  // ---- the resident CLIP DIRECTORY (kind 9) -------------------------------
  logic        clip_v_q;
  logic [ 5:0] clip_bones_q;
  logic [23:0] clip_owner_q;    // the FORM this bank says it animates
  logic [15:0] clip_n_q;        // rows the page declared
  logic [31:0] cbase_q;
  logic [31:0] cextent_q;
  logic        row_v_q    [CLIP_ROWS];
  logic [15:0] row_slot_q [CLIP_ROWS];
  logic [15:0] row_fcnt_q [CLIP_ROWS];
  logic [31:0] row_foff_q [CLIP_ROWS];

  // ---- the walk -----------------------------------------------------------
  localparam logic [4:0] S_IDLE    = 5'd0;
  // kind-8: page header -> body header -> bone records
  localparam logic [4:0] S_PH_REQ  = 5'd1;
  localparam logic [4:0] S_PH_BEAT = 5'd2;
  localparam logic [4:0] S_PH_HDR  = 5'd3;
  localparam logic [4:0] S_BH_REQ  = 5'd4;
  localparam logic [4:0] S_BH_BEAT = 5'd5;
  localparam logic [4:0] S_BH_HDR  = 5'd6;
  localparam logic [4:0] S_BR_REQ  = 5'd7;
  localparam logic [4:0] S_BR_BEAT = 5'd8;
  localparam logic [4:0] S_BR_FILL = 5'd9;
  // kind-9: page header -> directory rows
  localparam logic [4:0] S_CH_REQ  = 5'd10;
  localparam logic [4:0] S_CH_BEAT = 5'd11;
  localparam logic [4:0] S_CH_HDR  = 5'd12;
  localparam logic [4:0] S_CD_REQ  = 5'd13;
  localparam logic [4:0] S_CD_BEAT = 5'd14;
  localparam logic [4:0] S_CD_ROW  = 5'd15;
  // a frame: frame header -> quaternion lines -> hand over
  localparam logic [4:0] S_FH_REQ  = 5'd16;
  localparam logic [4:0] S_FH_BEAT = 5'd17;
  localparam logic [4:0] S_FH_HDR  = 5'd18;
  localparam logic [4:0] S_FQ_REQ  = 5'd19;
  localparam logic [4:0] S_FQ_BEAT = 5'd20;
  localparam logic [4:0] S_FQ_FILL = 5'd21;
  localparam logic [4:0] S_HAND    = 5'd22;

  logic [ 4:0] st_q;
  logic [31:0] addr_q;          // the line being read
  logic [31:0] base_q;          // the page being adopted
  logic [31:0] extent_q;
  logic [23:0] pidx_q;          // its 5f.1 identity, held across the walk
  logic [15:0] pgen_q;
  logic [ 5:0] n_q;             // records/bones the header declared
  logic [ 5:0] done_q;          // bones written so far
  logic [15:0] rows_done_q;     // directory rows stored so far
  logic [ 2:0] fw_q;            // word within the line being filled

  // THE OWNER AS READ, BEFORE IT IS ADOPTED. It is deliberately NOT written
  // straight into `body_owner_q`/`clip_owner_q` at the header, and the reason
  // is the exact failure this tree writes down hardest: those two registers
  // are read by `owner_ok_c` while `body_v_q`/`clip_v_q` still describe the
  // PREVIOUS page. A header parsed and then a read DENIED (S_BR_REQ,
  // S_CD_REQ) would otherwise leave the old skeleton resident wearing the new
  // page's owner -- a live store and a fresh identity that belong to two
  // different creatures, with every counter green.
  //
  // One staging register serves both walks because only one walk runs at a
  // time (`p_ready_o` and the publication arms are gated on S_IDLE).
  logic [23:0] owner_stage_q;

  // ---- header field views, restated rather than derived --------------------
  // A change on either side must show up as a failure rather than track
  // silently -- `zhao_geom_ladderbank` states the same rule over the same page.
  wire [31:0] h_magic_c   = line_q[31:0];
  wire [15:0] h_version_c = line_q[47:32];

  // kind-8 page header: body_off at bytes 8..11.
  wire [31:0] ph_bodyoff_c = line_q[95:64];

  // kind-8 BODY header: bone_count at 6, flags at 7, bones_off at 8..11,
  // reserved u32 at 12..15.
  wire [ 7:0] bh_bones_c  = line_q[55:48];
  wire [ 7:0] bh_flags_c  = line_q[63:56];
  wire [31:0] bh_bonesoff_c = line_q[95:64];
  wire [31:0] bh_rsv_c    = line_q[127:96];
  // BODY v2's owner word at bytes 16..19 (`zref::…::body::kOffOwnerForm`).
  wire [31:0] bh_owner_c  = line_q[159:128];

  // kind-9 page header: bone_count at 6, rsv0 at 7, clip_count at 8..9,
  // rsv1 at 10..11, dir_off at 12..15, frames_off at 16..19.
  wire [ 7:0] ch_bones_c  = line_q[55:48];
  wire [ 7:0] ch_rsv0_c   = line_q[63:56];
  wire [15:0] ch_clips_c  = line_q[79:64];
  wire [15:0] ch_rsv1_c   = line_q[95:80];
  wire [31:0] ch_diroff_c = line_q[127:96];
  wire [31:0] ch_frmoff_c = line_q[159:128];
  // CLIP_BANK v2's owner word at bytes 20..23 (`zref::clip_page::kOffOwnerForm`).
  wire [31:0] ch_owner_c  = line_q[191:160];

  // ---- the two directory rows in a line ------------------------------------
  // `zref::clip_page`'s offsets: slot_id u16 @0, frame_count u16 @2,
  // event_count u16 @4, rsv u16 @6, frame_off u32 @8.
  /* verilator lint_off UNUSEDSIGNAL */
  wire [255:0] dr_c = (st_q == S_CD_ROW && rows_done_q[0]) ? line_q[511:256] : line_q[255:0];
  /* verilator lint_on UNUSEDSIGNAL */
  wire [15:0] dr_slot_c  = dr_c[15:0];
  wire [15:0] dr_fcnt_c  = dr_c[31:16];
  wire [15:0] dr_rsv_c   = dr_c[63:48];
  wire [31:0] dr_foff_c  = dr_c[95:64];

  // The line holds two 32-byte rows; which of the two `dr_c` selects is the low
  // bit of the row counter, which is why a line is RE-ENTERED for its second
  // row rather than re-read.

  // ---- the frame's geometry ------------------------------------------------
  // `zref::clip_page::frame_stride` -- a 64-byte frame header then
  // `bone_count` x 8 B of quat16, rounded up to the 64-byte read grid. The
  // rounding is a mask, not a divide, because the grid is a power of two.
  wire [15:0] quat_bytes_c   = 16'(clip_bones_q) * 16'(QUAT_BYTES);
  wire [15:0] quat_lines_c   = (quat_bytes_c + 16'(LINE_BYTES) - 16'd1) >> 6;
  wire [15:0] stride_bytes_c = 16'(LINE_BYTES) + (quat_lines_c << 6);

  // ---- the read request ---------------------------------------------------
  wire req_c = (st_q == S_PH_REQ) || (st_q == S_BH_REQ) || (st_q == S_BR_REQ)
            || (st_q == S_CH_REQ) || (st_q == S_CD_REQ)
            || (st_q == S_FH_REQ) || (st_q == S_FQ_REQ);

  always_comb begin
    g_req_o        = '0;
    g_req_o.valid  = req_c;
    g_req_o.write  = 1'b0;
    g_req_o.client = CLIENT;
    g_req_o.addr   = addr_q[ZHAO_VRAM_ADDR_BITS-1:0];
    g_req_o.len    = 7'(LINE_BYTES);
    // The shape rule: the mask must match the length. A whole line is all
    // sixty-four lanes.
    g_req_o.be     = {64{1'b1}};
  end

  wire denied_c = req_c && g_rsp_i.violation;
  wire accept_c = req_c && g_rsp_i.ready && !g_rsp_i.violation;

  wire pub_body_c = pub_valid_i && (pub_tag_i == BODY_KIND);
  wire pub_clip_c = pub_valid_i && (pub_tag_i == CLIP_KIND);
  wire pub_any_c  = pub_body_c || pub_clip_c;

  assign busy_o  = (st_q != S_IDLE);

  // A request is taken only when nothing is in flight. It is never QUEUED: a
  // pose request that arrives during a page adoption belongs to a frame whose
  // creature may be about to change, and holding it would decode it against
  // whichever skeleton won the race.
  assign p_ready_o = (st_q == S_IDLE) && !pub_any_c;

  // ---- residency, as the request sees it ----------------------------------
  // BOTH stores, and they must agree about the creature. The bone counts are
  // loaded in two different states from two different publications, so this
  // comparison is not blind to the fault it names.
  wire resident_c = body_v_q && clip_v_q && (body_bones_q == clip_bones_q);

  // ---- ownership, as the request sees it -----------------------------------
  // BOTH sections must name THE DRAW'S form. Not each other -- that would pass
  // for two pages of one foreign creature, which is exactly the "matching
  // resources must also match the draw" case the ruling names (its test D).
  //
  // Each side of each comparison has an independent clock enable: the request
  // arrives combinationally on `p_form_idx_i`, `body_owner_q` is written in
  // S_BR_FILL out of a kind-8 publication, `clip_owner_q` in S_CD_ROW out of a
  // kind-9 one. Three loads, three paths, so the compare is not structurally
  // blind to any fault a single enable participates in.
  wire owner_ok_c = (body_owner_q == p_form_idx_i) && (clip_owner_q == p_form_idx_i);

  // ---- the clip lookup -----------------------------------------------------
  // One level of compare across the resident rows. CLIP_ROWS is small by
  // construction, so this is a wide OR and not a search. The FIRST match wins,
  // which is why `zref::clip_page::decode` refuses a page with a duplicate
  // slot outright -- the page's meaning would otherwise depend on row order.
  logic            chit_c;
  logic [ROWW-1:0] crow_c;
  integer          r;
  always_comb begin
    chit_c = 1'b0;
    crow_c = '0;
    for (r = 0; r < int'(CLIP_ROWS); r = r + 1) begin
      if (!chit_c && row_v_q[r] && (row_slot_q[r] == p_clip_id_i)) begin
        chit_c = 1'b1;
        crow_c = ROWW'(r);
      end
    end
  end

  // ---- the fill ------------------------------------------------------------
  // One 64-bit word per cycle out of the assembled line. The body's 32-byte
  // record is four words and a line holds two records; a quat16 is one word
  // and a line holds eight. So the word counter is the same counter and only
  // the {bone, word} decode differs.
  wire [4:0] fill_bone_body_c = done_q[4:0] + {4'd0, fw_q[2]};
  wire [4:0] fill_bone_quat_c = done_q[4:0] + {2'd0, fw_q};
  wire       body_fill_c      = (st_q == S_BR_FILL);
  wire       quat_fill_c      = (st_q == S_FQ_FILL);
  wire [4:0] fill_bone_c      = body_fill_c ? fill_bone_body_c : fill_bone_quat_c;

  // A line always carries eight words; the LAST line of a short page carries
  // padding past the declared bone count, and writing it would leave a stale
  // bone looking freshly filled. So the write enable is gated on the bone
  // being one the header declared.
  wire fill_in_range_c = (6'({1'b0, fill_bone_c}) < n_q);

  assign fill_we_o   = (body_fill_c || quat_fill_c) && fill_in_range_c;
  assign fill_sel_o  = quat_fill_c;
  assign fill_bone_o = fill_bone_c;
  assign fill_word_o = body_fill_c ? {2'd0, fw_q[1:0]} : 4'd0;
  // fw_q * 64, as a concatenation rather than a multiply: the word index is
  // three bits and the line is eight words, so the shift IS the address.
  wire [8:0] fw_bit_c = {fw_q, 6'd0};
  assign fill_data_o = line_q[fw_bit_c +: 64];

  // ---- the body record's own legality, as the oracle judges it -------------
  // `zref::creature_page::body::decode_body`, restated. The parent rule is
  // PARENT-BEFORE-CHILD with a zero root, which is what makes
  // `zhao_geom_pose_decode`'s single forward pass correct; a page violating it
  // would compose a bone against a parent matrix that has not been written.
  wire [7:0] br_parent_c = fill_data_o[7:0];
  wire [7:0] br_flags_c  = fill_data_o[15:8];
  wire [15:0] br_rsv_c   = fill_data_o[31:16];
  wire [31:0] br_rsv3_c  = fill_data_o[63:32];
  wire br_is_w0_c = body_fill_c && fill_in_range_c && (fw_q[1:0] == 2'd0);
  wire br_is_w3_c = body_fill_c && fill_in_range_c && (fw_q[1:0] == 2'd3);
  wire br_not_rigid_c = br_is_w0_c && ((br_flags_c & FLAG_RIGID) == 8'd0);
  wire br_rsv_nz_c    = (br_is_w0_c && (((br_flags_c & ~FLAG_RIGID) != 8'd0) || (br_rsv_c != 16'd0)))
                     || (br_is_w3_c && (br_rsv3_c != 32'd0));
  wire br_bad_parent_c = br_is_w0_c
                      && ((fill_bone_c == 5'd0) ? (br_parent_c != 8'd0)
                                                : (br_parent_c > {3'd0, fill_bone_c}));

  // ---- the frame header's reserved tail ------------------------------------
  // `creature_rules` 2.1 gives twelve bytes of root displacement; the rest of
  // the 64-byte frame header is reserved and `zref::clip_page::decode` refuses
  // a page whose tail is not zero.
  wire fh_rsv_nz_c = (line_q[127:96] != 32'd0) || (line_q[511:128] != 384'd0);

  // ---- the walk ------------------------------------------------------------
  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q         <= S_IDLE;
      line_q       <= '0;
      beat_q       <= 3'd0;
      addr_q       <= 32'd0;
      base_q       <= 32'd0;
      extent_q     <= 32'd0;
      pidx_q       <= 24'd0;
      pgen_q       <= 16'd0;
      n_q          <= 6'd0;
      done_q       <= 6'd0;
      rows_done_q  <= 16'd0;
      fw_q         <= 3'd0;
      body_v_q     <= 1'b0;
      body_bones_q <= 6'd0;
      body_owner_q <= 24'd0;
      owner_stage_q <= 24'd0;
      clip_v_q     <= 1'b0;
      clip_bones_q <= 6'd0;
      clip_owner_q <= 24'd0;
      clip_n_q     <= 16'd0;
      cbase_q      <= 32'd0;
      cextent_q    <= 32'd0;
      src_req_o        <= 1'b0;
      src_bone_count_o <= 6'd0;
      root_dx_o    <= 32'sd0;
      root_dy_o    <= 32'sd0;
      root_dz_o    <= 32'sd0;
      res_body_index_o <= 24'd0;
      res_body_gen_o   <= 16'd0;
      res_clip_index_o <= 24'd0;
      res_clip_gen_o   <= 16'd0;
      res_body_owner_o <= 24'd0;
      res_clip_owner_o <= 24'd0;
      for (i = 0; i < int'(CLIP_ROWS); i = i + 1) begin
        row_v_q[i]    <= 1'b0;
        row_slot_q[i] <= 16'd0;
        row_fcnt_q[i] <= 16'd0;
        row_foff_q[i] <= 32'd0;
      end
      bodies_o         <= 32'd0;
      clips_o          <= 32'd0;
      frames_o         <= 32'd0;
      pages_dropped_o  <= 32'd0;
      bad_magic_o      <= 32'd0;
      truncated_o      <= 32'd0;
      misaligned_o     <= 32'd0;
      bad_bone_count_o <= 32'd0;
      bone_mismatch_o  <= 32'd0;
      overflow_o       <= 32'd0;
      not_rigid_o      <= 32'd0;
      reserved_nz_o    <= 32'd0;
      denied_o         <= 32'd0;
      clip_miss_o      <= 32'd0;
      frame_oob_o      <= 32'd0;
      not_resident_o   <= 32'd0;
      owner_mismatch_o <= 32'd0;
    end else begin
      src_req_o <= 1'b0;

      // A publication arriving while a walk runs is REFUSED AND COUNTED, never
      // queued -- `zhao_geom_ladderbank`'s rule, for its reason: hooking the
      // queue instead would adopt a page that is still landing.
      if (pub_any_c && (st_q != S_IDLE)) pages_dropped_o <= pages_dropped_o + 32'd1;

      case (st_q)
        // ------------------------------------------------------------------
        S_IDLE: begin
          if (pub_body_c) begin
            base_q   <= pub_base_i;
            extent_q <= pub_extent_i;
            addr_q   <= pub_base_i;
            pidx_q   <= pub_index_i;
            pgen_q   <= pub_generation_i;
            beat_q   <= 3'd0;
            done_q   <= 6'd0;
            // NOT invalidated here. `zhao_geom_ladderbank` keeps its live half
            // byte-identical through a refusal, and that is the right law; it
            // can only afford it because it pays ROWS*128 spare bits for a
            // second bank. `zhao_geom_bonesrc`'s store is SINGLE and this
            // block cannot give it a second one, so residency is dropped at
            // the moment the store is first WRITTEN instead -- which keeps
            // every HEADER-level refusal (magic, version, flags, alignment,
            // extent) harmless and drops residency only when a refusal has
            // genuinely torn the store. Stated rather than silently differing
            // from the block this entry cites as the pattern.
            if (pub_extent_i < 32'(HDR_BYTES)) truncated_o <= truncated_o + 32'd1;
            else                               st_q <= S_PH_REQ;
          end else if (pub_clip_c) begin
            base_q   <= pub_base_i;
            extent_q <= pub_extent_i;
            addr_q   <= pub_base_i;
            pidx_q   <= pub_index_i;
            pgen_q   <= pub_generation_i;
            beat_q   <= 3'd0;
            rows_done_q <= 16'd0;
            // Dropped at the first ROW STORE, not here -- see the kind-8 arm.
            if (pub_extent_i < 32'(HDR_BYTES)) truncated_o <= truncated_o + 32'd1;
            else                               st_q <= S_CH_REQ;
          end else if (p_valid_i) begin
            if (!resident_c) begin
              // The stores do not hold a consistent creature. Counted and
              // refused; the caller degrades to bind pose (ruling R229) rather
              // than losing the draw.
              not_resident_o <= not_resident_o + 32'd1;
              if (body_v_q && clip_v_q && (body_bones_q != clip_bones_q))
                bone_mismatch_o <= bone_mismatch_o + 32'd1;
            end else if (!owner_ok_c) begin
              // The stores hold a consistent creature -- and it is NOT this
              // draw's creature. Refused and counted. It is deliberately NOT
              // folded into `not_resident_o`: that counter says "nothing is
              // loaded", and this one says "something is loaded and it belongs
              // to somebody else", which is the failure the bone-count check
              // cannot see and the one that ships a well-formed palette for
              // the wrong animal.
              owner_mismatch_o <= owner_mismatch_o + 32'd1;
            end else if (!chit_c) begin
              clip_miss_o <= clip_miss_o + 32'd1;
            end else if (p_frame_no_i >= row_fcnt_q[crow_c]) begin
              frame_oob_o <= frame_oob_o + 32'd1;
            end else begin
              n_q       <= clip_bones_q;
              done_q    <= 6'd0;
              beat_q    <= 3'd0;
              // frame f begins at frame_off + stride * f. A multiply, not a
              // walk -- which is the whole reason `zref::clip_page` shapes
              // every offset to the 64-byte grid.
              addr_q    <= cbase_q + row_foff_q[crow_c]
                         + (32'(stride_bytes_c) * 32'({16'd0, p_frame_no_i}));
              st_q      <= S_FH_REQ;
            end
          end
        end

        // ---- kind-8: the page header, for `body_off` ----------------------
        S_PH_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_PH_BEAT; end
        end

        S_PH_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};   // LSB-first assembly
            beat_q <= beat_q + 3'd1;
            // The eighth beat lands on THIS edge, so the header is not
            // readable until the next one. S_PH_HDR is that cycle.
            if (beat_q == 3'd7) st_q <= S_PH_HDR;
          end
        end

        S_PH_HDR: begin
          if ((h_magic_c != FORM_MAGIC) || (h_version_c != FORM_VERSION)) begin
            bad_magic_o <= bad_magic_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (ph_bodyoff_c == 32'd0) begin
            // `body_off == 0` is the packer saying "no body yet" -- a legal
            // page with nothing this block can read. Not a fault, and not an
            // adoption either: the store stays invalid and says so.
            st_q <= S_IDLE;
          end else if (ph_bodyoff_c[5:0] != 6'd0) begin
            misaligned_o <= misaligned_o + 32'd1;
            st_q         <= S_IDLE;
          end else if ((ph_bodyoff_c + 32'(HDR_BYTES)) > extent_q) begin
            truncated_o <= truncated_o + 32'd1;
            st_q        <= S_IDLE;
          end else begin
            addr_q <= base_q + ph_bodyoff_c;
            // Held so the bone records can be addressed from the BODY's own
            // base rather than the page's.
            base_q <= base_q + ph_bodyoff_c;
            extent_q <= extent_q - ph_bodyoff_c;
            beat_q <= 3'd0;
            st_q   <= S_BH_REQ;
          end
        end

        // ---- kind-8: the body header --------------------------------------
        S_BH_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_BH_BEAT; end
        end

        S_BH_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) st_q <= S_BH_HDR;
          end
        end

        S_BH_HDR: begin
          n_q <= 6'(bh_bones_c[5:0]);
          if ((h_magic_c != BODY_MAGIC) || (h_version_c != BODY_VERSION)) begin
            bad_magic_o <= bad_magic_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (bh_flags_c != FLAG_RIGID) begin
            // The header's own declaration, judged before a record is read.
            // `zhao_geom_bonesrc` derives `inv_rest` from the rigid-rest
            // invariant, so a body claiming otherwise is refused, never
            // decoded wrongly (its `INV_REST_RIGID` is the extension path).
            not_rigid_o <= not_rigid_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (bh_rsv_c != 32'd0) begin
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else if (bh_owner_c[31:24] != OWNER_RSV_ZERO) begin
            // The owner is a u24 in a u32 slot. A page using the high byte is
            // refused rather than masked: masking would make two different
            // stored words mean one form.
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else if ((bh_bones_c == 8'd0) || (bh_bones_c > 8'(MAX_BONES))) begin
            bad_bone_count_o <= bad_bone_count_o + 32'd1;
            st_q             <= S_IDLE;
          end else if (bh_bonesoff_c[5:0] != 6'd0) begin
            misaligned_o <= misaligned_o + 32'd1;
            st_q         <= S_IDLE;
          end else if ((bh_bonesoff_c + (32'(bh_bones_c) * 32'(BONE_BYTES))) > extent_q) begin
            truncated_o <= truncated_o + 32'd1;
            st_q        <= S_IDLE;
          end else begin
            // STAGED across the record walk and committed with the body
            // below -- adopted as ONE logical state with the bones, never
            // before them.
            owner_stage_q <= bh_owner_c[23:0];
            addr_q <= base_q + bh_bonesoff_c;
            done_q <= 6'd0;
            beat_q <= 3'd0;
            st_q   <= S_BR_REQ;
          end
        end

        // ---- kind-8: the bone records --------------------------------------
        S_BR_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_BR_BEAT; end
        end

        S_BR_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) begin
              fw_q <= 3'd0;
              st_q <= S_BR_FILL;
              // THE STORE IS ABOUT TO BE OVERWRITTEN. From here a refusal
              // leaves a torn skeleton, so residency ends here and is re-earned
              // by the adoption below.
              body_v_q <= 1'b0;
            end
          end
        end

        S_BR_FILL: begin
          // The fill itself is combinational off `fw_q`; this arm judges the
          // record and sequences. A refusal abandons the page with `body_v_q`
          // still LOW, so nothing a request can see has changed.
          if (br_not_rigid_c) begin
            not_rigid_o <= not_rigid_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (br_rsv_nz_c) begin
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else if (br_bad_parent_c) begin
            // A parent at or after its child, or a non-zero root. Counted with
            // the reserved-field taxonomy rather than silently accepted: a
            // forward reference makes the decoder's single pass compose
            // against a matrix it has not written.
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else if (fw_q == 3'd7) begin
            // Two records landed, or one and the line's padding.
            if ((done_q + 6'd2) >= n_q) begin
              body_v_q     <= 1'b1;         // ADOPT, whole
              body_bones_q <= n_q;
              res_body_index_o <= pidx_q;
              res_body_gen_o   <= pgen_q;
              // Data, ownership and validity adopted together. `body_owner_q`
              // was captured at the header and only becomes VISIBLE here,
              // with `body_v_q`, so a torn or refused load cannot publish an
              // owner for bones that never landed.
              body_owner_q     <= owner_stage_q;
              res_body_owner_o <= owner_stage_q;
              bodies_o     <= bodies_o + 32'd1;
              st_q         <= S_IDLE;
            end else begin
              done_q <= done_q + 6'd2;
              addr_q <= addr_q + 32'(LINE_BYTES);
              beat_q <= 3'd0;
              st_q   <= S_BR_REQ;
            end
          end else begin
            fw_q <= fw_q + 3'd1;
          end
        end

        // ---- kind-9: the page header --------------------------------------
        S_CH_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_CH_BEAT; end
        end

        S_CH_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) st_q <= S_CH_HDR;
          end
        end

        S_CH_HDR: begin
          clip_n_q  <= ch_clips_c;
          cbase_q   <= base_q;
          cextent_q <= extent_q;
          if ((h_magic_c != CLIP_MAGIC) || (h_version_c != CLIP_VERSION)) begin
            bad_magic_o <= bad_magic_o + 32'd1;
            st_q        <= S_IDLE;
          end else if ((ch_rsv0_c != 8'd0) || (ch_rsv1_c != 16'd0)
                       || (ch_owner_c[31:24] != OWNER_RSV_ZERO)) begin
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else if ((ch_bones_c == 8'd0) || (ch_bones_c > 8'(MAX_BONES))) begin
            bad_bone_count_o <= bad_bone_count_o + 32'd1;
            st_q             <= S_IDLE;
          end else if (ch_clips_c == 16'd0) begin
            // `zref::clip_page::decode` refuses a zero-clip page; a bank with
            // no clips cannot answer any slot and is not a legal page.
            overflow_o <= overflow_o + 32'd1;
            st_q       <= S_IDLE;
          end else if (ch_clips_c > 16'(CLIP_ROWS)) begin
            overflow_o <= overflow_o + 32'd1;
            st_q       <= S_IDLE;
          end else if (ch_diroff_c != 32'(HDR_BYTES)) begin
            misaligned_o <= misaligned_o + 32'd1;
            st_q         <= S_IDLE;
          end else if (ch_frmoff_c[5:0] != 6'd0) begin
            misaligned_o <= misaligned_o + 32'd1;
            st_q         <= S_IDLE;
          end else if ((ch_diroff_c + (32'(ch_clips_c) * 32'(CLIP_BYTES))) > extent_q) begin
            truncated_o <= truncated_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (ch_frmoff_c < (ch_diroff_c + (32'(ch_clips_c) * 32'(CLIP_BYTES)))) begin
            misaligned_o <= misaligned_o + 32'd1;
            st_q         <= S_IDLE;
          end else begin
            clip_bones_q <= 6'(ch_bones_c[5:0]);
            owner_stage_q <= ch_owner_c[23:0];
            addr_q       <= base_q + ch_diroff_c;
            rows_done_q  <= 16'd0;
            beat_q       <= 3'd0;
            st_q         <= S_CD_REQ;
          end
        end

        // ---- kind-9: the directory ----------------------------------------
        S_CD_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_CD_BEAT; end
        end

        S_CD_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) st_q <= S_CD_ROW;
          end
        end

        S_CD_ROW: begin
          if (dr_rsv_c != 16'd0) begin
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else if (dr_fcnt_c == 16'd0) begin
            // A clip declaring zero frames -- `Verdict::kBadFrameCount`. It
            // can never answer a request, so the page is refused whole rather
            // than adopted with a row nothing may use.
            frame_oob_o <= frame_oob_o + 32'd1;
            st_q        <= S_IDLE;
          end else if (dr_foff_c[5:0] != 6'd0) begin
            misaligned_o <= misaligned_o + 32'd1;
            st_q         <= S_IDLE;
          end else if ((dr_foff_c + (32'(stride_bytes_c) * 32'({16'd0, dr_fcnt_c}))) > cextent_q) begin
            truncated_o <= truncated_o + 32'd1;
            st_q        <= S_IDLE;
          end else begin
            // THE ROWS ARE ABOUT TO BE OVERWRITTEN. Same law as the body's,
            // one store over: every header-level refusal above this line
            // leaves the resident directory whole, and only a row that is
            // being written ends its residency. The filling rows are cleared
            // with the first store so a SHORTER page cannot leave the previous
            // load's slots answering behind it.
            if (rows_done_q == 16'd0) begin
              clip_v_q <= 1'b0;
              for (i = 0; i < int'(CLIP_ROWS); i = i + 1) row_v_q[i] <= 1'b0;
            end
            row_v_q   [rows_done_q[ROWW-1:0]] <= 1'b1;
            row_slot_q[rows_done_q[ROWW-1:0]] <= dr_slot_c;
            row_fcnt_q[rows_done_q[ROWW-1:0]] <= dr_fcnt_c;
            row_foff_q[rows_done_q[ROWW-1:0]] <= dr_foff_c;
            if ((rows_done_q + 16'd1) >= clip_n_q) begin
              clip_v_q <= 1'b1;             // ADOPT, whole
              res_clip_index_o <= pidx_q;
              res_clip_gen_o   <= pgen_q;
              clip_owner_q     <= owner_stage_q;
              res_clip_owner_o <= owner_stage_q;
              clips_o  <= clips_o + 32'd1;
              st_q     <= S_IDLE;
            end else if (!rows_done_q[0]) begin
              // The line's second row is already in `line_q`; re-enter rather
              // than re-read it.
              rows_done_q <= rows_done_q + 16'd1;
            end else begin
              rows_done_q <= rows_done_q + 16'd1;
              addr_q      <= addr_q + 32'(LINE_BYTES);
              beat_q      <= 3'd0;
              st_q        <= S_CD_REQ;
            end
          end
        end

        // ---- a frame: its header, then its quaternions ---------------------
        S_FH_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_FH_BEAT; end
        end

        S_FH_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) st_q <= S_FH_HDR;
          end
        end

        S_FH_HDR: begin
          if (fh_rsv_nz_c) begin
            reserved_nz_o <= reserved_nz_o + 32'd1;
            st_q          <= S_IDLE;
          end else begin
            root_dx_o <= $signed(line_q[31:0]);
            root_dy_o <= $signed(line_q[63:32]);
            root_dz_o <= $signed(line_q[95:64]);
            addr_q <= addr_q + 32'(LINE_BYTES);
            done_q <= 6'd0;
            beat_q <= 3'd0;
            st_q   <= S_FQ_REQ;
          end
        end

        S_FQ_REQ: begin
          if (denied_c)      begin denied_o <= denied_o + 32'd1; st_q <= S_IDLE; end
          else if (accept_c) begin beat_q <= 3'd0; st_q <= S_FQ_BEAT; end
        end

        S_FQ_BEAT: begin
          if (g_beat_valid_i) begin
            line_q <= {g_beat_data_i, line_q[511:64]};
            beat_q <= beat_q + 3'd1;
            if (beat_q == 3'd7) begin
              fw_q <= 3'd0;
              st_q <= S_FQ_FILL;
            end
          end
        end

        S_FQ_FILL: begin
          if (fw_q == 3'd7) begin
            if ((done_q + 6'd8) >= n_q) begin
              st_q <= S_HAND;
            end else begin
              done_q <= done_q + 6'd8;
              addr_q <= addr_q + 32'(LINE_BYTES);
              beat_q <= 3'd0;
              st_q   <= S_FQ_REQ;
            end
          end else begin
            fw_q <= fw_q + 3'd1;
          end
        end

        // ---- the handover --------------------------------------------------
        // EVERY word of both stores is written before this state is reached,
        // and this block issues no read after it. That is the whole of the
        // late-fetch law the committed mutant exists to state.
        S_HAND: begin
          if (src_ready_i) begin
            src_req_o        <= 1'b1;
            src_bone_count_o <= n_q;
            frames_o         <= frames_o + 32'd1;
            st_q             <= S_IDLE;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

endmodule

`default_nettype wire
