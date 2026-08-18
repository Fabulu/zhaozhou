// zhao_surface_stamp.sv — SURFACE.STAMP: the deterministic stamp engine
// (phase 6, ZH-052).
//
// Law, in citation order:
//   design/contracts/SURFACE.STAMP.md — the block contract.
//   design/blocks.yml — `inputs: [dispatch, stamp_field_results, sheet_pages]`,
//       `outputs: [stamp_results]`, `backpressure: ready_valid`,
//       `latency: variable`, "1 stamp texel per clock", counters
//       `surface_stamps` + `surface_texels_touched`, `source_ids: true`, and
//       the note "Capture-exact: identical inputs replay to identical sheets".
//   spec/commands.zidl SurfaceStamp 0x0210 — the FROZEN wire field set
//       {brush, patch, operation u8, tag u8, strength u16, transform2fx,
//        radius fx16, ring_width fx16}. ABI v3; the field set never changes.
//   reference/src/zrender/terrain.cpp `stamp_surface` — THE EXECUTED LAW. The
//       coverage test, the texel-centre placement, the `strength >> 8`
//       conversion, the operation branch and the unconditional tag write are
//       all quoted from it below, line for line.
//   reference/include/zref/zref_surface.hpp — the oracle, and the stated
//       input domain.
//   design/ops.yml FIELD.STAMP.{MAX,ADD,SUB,REPLACE,AGE} — the five stamp
//       blend modes the ledger's purpose line names.
//   spec/form/field-ir.md 7.1 — the stamp profile I/O record.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 12 — Scar Scribe.
//
// ---------------------------------------------------------------------------
// FOUND — the ratified path, reproduced and not reinterpreted
// ---------------------------------------------------------------------------
// `stamp_surface` is executed today by the software console and pinned by
// committed goldens (tests/render/render_golden.cpp stamps a crack ring into
// patch 44; tests/render/render_heightfield.cpp checks an annulus hole texel by
// texel). Every line of it is law here:
//
//   texel centre:  wx = ex0 + ((ex1 - ex0) * (2i + 1)) / 128       (j likewise)
//   radii:         r_outer2 = r*r
//                  r_inner  = rw > 0 ? max(r - rw, 0) : 0
//   coverage:      !(d2 > r_outer2 || d2 < r_inner2)
//   source byte:   src = strength >> 8
//   blend:         operation == 1 ? sat8((dst >> 1) + src) : max(dst, src)
//   tag:           written on every covered texel, outside the if/else
//   scan order:    j outer, i inner
//
// Three of those look like defects and are faithfully kept, because a capture
// must replay:
//   * the `/ 128` is C++ integer division = TRUNCATION TOWARD ZERO, which is
//     not an arithmetic shift when `ex1 - ex0` is negative. An inverted
//     envelope therefore differs from the shift form by one fx16 LSB, and this
//     block truncates.
//   * `r` is used SIGNED and squared, so a negative radius covers exactly like
//     its magnitude.
//   * `strength >> 8` TRUNCATES, where spec/qformats.md 2 would round a unit8
//     conversion half-up. The reference does not round and the goldens pin it.
//     Recorded as a deliberate divergence from the qformats family.
//
// THE STATED INPUT DOMAIN. The reference computes `dx*dx + dz*dz` in int64;
// for arbitrary int32 envelope/transform words |dx| reaches 2^33 and `dx*dx`
// overflows int64 — the reference has left its own arithmetic, so a
// differential out there compares two overflows. The domain is +-4,096 world
// metres (fx16 raw magnitude <= 2^28) on every envelope corner, on the
// transform translation and on radius/ring_width. Inside it |dx| < 2^30,
// d2 < 2^61 and r*r < 2^57, so the 64-bit datapath below is EXACT and matches
// the reference bit for bit. Outside it both wrap modulo 2^64 and neither is
// meaningful. (design/contracts/TERRAIN.NORMALS.md states its domain for the
// identical reason.)
//
// ---------------------------------------------------------------------------
// FOUND BUT NEVER IMPLEMENTED — the ops.yml blends
// ---------------------------------------------------------------------------
// design/ops.yml carries five `stamp_mode` ops whose
// `implementation_blocks:` list names this block. Their semantics are written
// (`dst = max(dst, src)`, `sat16(dst + src)`, `sat16(dst - src)`, `dst = src`,
// and AGE's "decays toward zero by a per-material age rate"), but the
// `zref::fieldir::stamp_*` functions they name DO NOT EXIST in this tree and
// neither does tests/differential/field_stamp_modes.cpp (checked 2026-08-19).
// This block is their first implementation. Two notes:
//   * ops.yml says `sat16` while layer F stores 8 bits per texel
//     (charter 12, terrain_rules 2). The saturation is at 8 bits here, because
//     that is the width the destination has; a 16-bit saturate on an 8-bit
//     store is not a choice, it is a typo in a line that was written before
//     the layer table.
//   * AGE's rate is CHOSEN: a right shift of 0..7. REJECTED ALTERNATIVE: the
//     qformats 2 `unit_mul` form `(dst*rate + 128) >> 8`, more expressive and
//     the right shape once a per-material table exists — but it costs a
//     multiplier and a rate of 255/256 never reaches zero, so an aged scar
//     lingers at strength 1 forever. A shift terminates.
//
// ---------------------------------------------------------------------------
// LAWS CHOSEN, NOT FOUND (each argued again in the contract)
// ---------------------------------------------------------------------------
// S1. THE BLEND SELECT IS TWO-LEVEL. `cmd_blend_en_i` = 0 is the RATIFIED
//     path: the blend is `cmd_operation_i == 1 ? DECAY_ACC : MAX`, the
//     reference's own branch including its `else` (so operation = 7 replays as
//     a max stamp, exactly as the software console replays it).
//     `cmd_blend_en_i` = 1 selects an ops.yml mode directly from
//     `cmd_blend_i`. REJECTED ALTERNATIVE: widening the ABI `operation` byte's
//     meaning to 0..4. That reinterprets a frozen wire field and would make
//     every committed capture ambiguous — capture_format.md 1.2's rule is that
//     opcodes and field SETS never change, and silently re-meaning a field's
//     VALUES is the same betrayal by another route.
//
// S2. THE FIELD BRUSH DELIVERS ONE RESULT PER VISITED TEXEL — all 4,096, in
//     the same j-outer/i-inner scan order — not one per COVERED texel. A
//     result whose texel is not covered is consumed and DISCARDED.
//     REJECTED ALTERNATIVE: results only for covered texels, which makes the
//     consumption rate depend on a coverage test that lives in THIS block, so
//     FIELD.SEQ.STAMP would have to reproduce this geometry bit-for-bit or the
//     pair deadlocks. (design/contracts/TERRAIN.PATCH.md chose the same
//     discipline for its field lanes, chosen law 2, for the same reason.)
//     `tag_op` unpacks as tag = [7:0], blend = [10:8], age_shift = [14:12];
//     no layout is written anywhere, so this one is chosen. The field's
//     `strength` is a u16 carrying the ABI's format so that ONE `>> 8`
//     conversion serves both paths — REJECTED ALTERNATIVE: a pre-reduced u8,
//     which puts a second, differently rounded conversion upstream and
//     guarantees the two paths drift. The `emissive` output lane of
//     field-ir 7.1 is DROPPED: layer F has two bytes and charter 12 spends
//     both, and adding a third would change the frozen 8,192 B layer size.
//
// S3. `stamp_results` CARRIES {texel, tag, strength_after, strength_before}.
//     TERRAIN.BAKE (phase 7) turns stamps into layer-B height16 scars and
//     needs the DELTA, not just the new value; sending `before` costs eight
//     wires and saves BAKE a second read port onto the sheet. REJECTED
//     ALTERNATIVE: emitting only the new value and letting BAKE re-read — a
//     second reader on a store whose whole rate budget is one texel per clock.
//
// S4. AN ACQUIRE THAT OVERFLOWS ABORTS THE WHOLE STAMP BEFORE ANY WRITE. The
//     ledger's SURFACE.SHEET note is "overflow rejects the stamp, never
//     partial-writes"; this block enforces it STRUCTURALLY — the texel loop
//     cannot start until the ACQUIRE has answered, so there is no ordering in
//     which a write escapes. REJECTED ALTERNATIVE: starting the loop
//     optimistically and relying on SURFACE.SHEET to drop the writes. It gives
//     the same sheet contents but a wrong `surface_texels_touched` and a wrong
//     `stamp_results` stream, and it burns 4,096 cycles to accomplish nothing.
//
// S5. `brush` IS NOT AN INPUT. commands.zidl carries `handle32[brush] brush`,
//     and charter 12 lists textured and noise brushes — but NOTHING in this
//     tree defines a brush page's format, and `stamp_surface` never reads the
//     field. A port that is wired to nothing is worse than an absent one: it
//     looks like a contract. When the brush page lands, the field path (S2) is
//     where it arrives. RECORDED, not hidden.
//
// NOT IN THIS BLOCK, deliberately: no height16 scar and no breach law
// (TERRAIN.BAKE owns layers B and D, phase 7 — see the seam note in the
// contract), no spline/brush primitive (the ABI opcode encodes a
// circle/annulus and inventing wire format for the rest would be inventing
// ABI), no draw-time sheet sampling, no VRAM port.
//
// Conservative SystemVerilog subset only (charter 2).

module zhao_surface_stamp (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // dispatch — one SurfaceStamp command
    // -----------------------------------------------------------------------
    input  logic               cmd_valid_i,
    output logic               cmd_ready_o,
    input  logic        [31:0] cmd_handle_i,      // handle32[patch] patch
    input  logic        [ 7:0] cmd_operation_i,   // the ABI operation byte
    input  logic        [ 7:0] cmd_tag_i,
    // The low byte is DISCARDED BY LAW: `stamp_surface` writes
    // `strength >> 8` and truncates. Saying so with a pragma rather than
    // hiding it behind a dummy reduction.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic        [15:0] cmd_strength_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  logic signed [31:0] cmd_tx_i,          // transform2fx translation
    input  logic signed [31:0] cmd_ty_i,
    input  logic signed [31:0] cmd_radius_i,      // fx16 world metres
    input  logic signed [31:0] cmd_ring_width_i,  // <= 0 = filled disc
    input  logic signed [31:0] cmd_env_x0_i,      // the patch envelope, fx16
    input  logic signed [31:0] cmd_env_z0_i,
    input  logic signed [31:0] cmd_env_x1_i,
    input  logic signed [31:0] cmd_env_z1_i,
    input  logic               cmd_blend_en_i,    // S1: 0 = ABI mapping
    input  logic        [ 2:0] cmd_blend_i,       // S1: ops.yml mode
    input  logic        [ 2:0] cmd_age_shift_i,   // AGE rate (chosen)
    input  logic               cmd_field_en_i,    // S2: field-driven brush
    input  logic        [15:0] cmd_src_id_i,

    // -----------------------------------------------------------------------
    // stamp_field_results — one record per VISITED texel (S2)
    // -----------------------------------------------------------------------
    input  logic        fld_valid_i,
    output logic        fld_ready_o,
    // tag_op bits [31:15] and [11] are RESERVED (S2's chosen layout packs tag
    // into [7:0], blend into [10:8], age_shift into [14:12]); fld_strength's
    // low byte meets the same `>> 8` truncation the ABI field does.
    /* verilator lint_off UNUSEDSIGNAL */
    input  logic [31:0] fld_tag_op_i,
    input  logic [15:0] fld_strength_i,
    /* verilator lint_on UNUSEDSIGNAL */

    // -----------------------------------------------------------------------
    // SURFACE.SHEET request port (ACQUIRE, then one READ per covered texel)
    // -----------------------------------------------------------------------
    output logic        req_valid_o,
    input  logic        req_ready_i,
    output logic [ 1:0] req_op_o,
    output logic [31:0] req_handle_o,
    output logic [11:0] req_texel_o,
    output logic [15:0] req_src_id_o,

    // -----------------------------------------------------------------------
    // sheet_pages — SURFACE.SHEET's responses
    // -----------------------------------------------------------------------
    input  logic       pg_valid_i,
    output logic       pg_ready_o,
    input  logic [1:0] pg_status_i,
    input  logic [7:0] pg_strength_i,

    // -----------------------------------------------------------------------
    // SURFACE.SHEET write port (terrain_rules 7: only this block writes F)
    // -----------------------------------------------------------------------
    output logic        wr_valid_o,
    input  logic        wr_ready_i,
    output logic [31:0] wr_handle_o,
    output logic [11:0] wr_texel_o,
    output logic [ 7:0] wr_tag_o,
    output logic [ 7:0] wr_strength_o,
    output logic        wr_we_tag_o,
    output logic        wr_we_strength_o,
    output logic [15:0] wr_src_id_o,

    // -----------------------------------------------------------------------
    // stamp_results -> TERRAIN.BAKE (S3)
    // -----------------------------------------------------------------------
    output logic        res_valid_o,
    input  logic        res_ready_i,
    output logic [11:0] res_texel_o,
    output logic [ 7:0] res_tag_o,
    output logic [ 7:0] res_strength_o,  // layer F after the blend
    output logic [ 7:0] res_before_o,    // layer F before the blend
    output logic [15:0] res_src_id_o,

    // -----------------------------------------------------------------------
    // status and counters
    // -----------------------------------------------------------------------
    output logic        stamp_done_o,      // 1-cycle pulse: stamp completed
    output logic        stamp_rejected_o,  // 1-cycle pulse: residency overflow
    output logic [31:0] surface_stamps_o,
    output logic [31:0] surface_texels_touched_o,
    output logic        idle_o
);

  // ---- SURFACE.SHEET protocol constants (kept in step with that block) -----
  localparam logic [1:0] OpAcquire = 2'd0;
  localparam logic [1:0] OpRead = 2'd1;
  localparam logic [1:0] StOverflow = 2'd2;

  // ---- the blend vocabulary (zref::surface::Blend) -------------------------
  localparam logic [2:0] BlStamp = 3'd0;  // ABI operation 0 and every value != 1
  localparam logic [2:0] BlDecayAcc = 3'd1;  // ABI operation 1
  localparam logic [2:0] BlAdd = 3'd3;  // ops.yml FIELD.STAMP.ADD
  localparam logic [2:0] BlSub = 3'd4;  // ops.yml FIELD.STAMP.SUB
  localparam logic [2:0] BlReplace = 3'd5;  // ops.yml FIELD.STAMP.REPLACE
  localparam logic [2:0] BlAge = 3'd6;  // ops.yml FIELD.STAMP.AGE

  localparam int unsigned Texels = 4096;  // 64 x 64, charter 12

  // ---- states --------------------------------------------------------------
  localparam logic [1:0] SIdle = 2'd0;
  localparam logic [1:0] SAcq = 2'd1;
  localparam logic [1:0] SRun = 2'd2;
  localparam logic [1:0] SDrain = 2'd3;

  logic [1:0] state;
  // The ACQUIRE is issued exactly once. Without this latch req_valid_o would
  // still be high in the cycle SURFACE.SHEET answers, and the sheet would
  // accept a SECOND acquire for the same handle.
  logic       acq_sent;

  // ---- the blend ------------------------------------------------------------
  // Codes 0/2 (BlStamp, ops.yml MAX) are the same arithmetic and share the
  // default arm; they are separate codes only so the two vocabularies stay
  // distinguishable in a trace.
  function automatic logic [7:0] blend_apply(input logic [2:0] b, input logic [7:0] dst,
                                             input logic [7:0] src, input logic [2:0] sh);
    logic [8:0] s;
    begin
      s = 9'd0;
      case (b)
        BlDecayAcc: begin
          s = {2'b0, dst[7:1]} + {1'b0, src};
          blend_apply = s[8] ? 8'hFF : s[7:0];
        end
        BlAdd: begin
          s = {1'b0, dst} + {1'b0, src};
          blend_apply = s[8] ? 8'hFF : s[7:0];
        end
        BlSub: begin
          s = {1'b0, dst} - {1'b0, src};
          blend_apply = s[8] ? 8'h00 : s[7:0];
        end
        BlReplace: blend_apply = src;
        BlAge: blend_apply = dst >> sh;
        default: blend_apply = (src > dst) ? src : dst;
      endcase
    end
  endfunction

  // ---- per-stamp registers (loaded once, at command accept) ----------------
  logic [31:0] st_handle;
  logic [15:0] st_src_id;
  logic [ 7:0] st_tag;
  logic [ 7:0] st_src;  // strength >> 8, the reference's truncation
  logic [ 2:0] st_blend;
  logic [ 2:0] st_age;
  logic        st_field;
  logic signed [31:0] st_ex0, st_ez0;
  logic signed [32:0] st_spanx, st_spanz;
  logic signed [31:0] st_tx, st_ty;
  logic signed [63:0] st_r_outer2, st_r_inner2;

  // ---- the per-stamp constants, computed combinationally at accept ---------
  // r_outer2 = r*r with r SIGNED (the reference squares the signed word).
  wire signed [63:0] rad_ext = 64'(cmd_radius_i);
  wire signed [63:0] r_outer2_c = rad_ext * rad_ext;
  // r_inner = rw > 0 ? max(r - rw, 0) : 0
  wire signed [32:0] r_minus_rw = 33'(cmd_radius_i) - 33'(cmd_ring_width_i);
  wire signed [32:0] r_inner_c =
      (cmd_ring_width_i > 32'sd0) ? ((r_minus_rw > 33'sd0) ? r_minus_rw : 33'sd0) : 33'sd0;
  wire signed [63:0] rin_ext = 64'(r_inner_c);
  wire signed [63:0] r_inner2_c = rin_ext * rin_ext;

  // ---- the texel cursor ----------------------------------------------------
  logic [12:0] cursor;  // 0..4096; 4096 = the loop is finished
  wire cursor_done = (cursor == 13'(Texels));
  wire [5:0] cur_i = cursor[5:0];
  wire [5:0] cur_j = cursor[11:6];

  // ---- texel centre, quoted from stamp_surface -----------------------------
  //   wx = ex0 + ((ex1 - ex0) * (2i + 1)) / 128
  // (2i+1) is 1..127 and always positive; {i, 1'b1} IS 2i+1.
  wire signed [7:0] two_i_1 = $signed({1'b0, cur_i, 1'b1});
  wire signed [7:0] two_j_1 = $signed({1'b0, cur_j, 1'b1});
  wire signed [40:0] numx = 41'(st_spanx) * 41'(two_i_1);
  wire signed [40:0] numz = 41'(st_spanz) * 41'(two_j_1);

  // TRUNCATION TOWARD ZERO, not an arithmetic shift: >>> 7 floors, and a
  // negative numerator with a non-zero remainder needs the +1 back. An
  // inverted envelope (ex1 < ex0) is the only way to get here, and this is the
  // one raw-LSB difference between the C++ `/ 128` and a shift.
  function automatic logic signed [33:0] trunc128(input logic signed [40:0] n);
    logic signed [33:0] q;
    begin
      q = n[40:7];
      if (n[40] && (n[6:0] != 7'd0)) q = q + 34'sd1;
      trunc128 = q;
    end
  endfunction

  wire signed [33:0] qx = trunc128(numx);
  wire signed [33:0] qz = trunc128(numz);
  wire signed [34:0] wx = 35'(st_ex0) + 35'(qx);
  wire signed [34:0] wz = 35'(st_ez0) + 35'(qz);
  wire signed [35:0] dx = 36'(wx) - 36'(st_tx);
  wire signed [35:0] dz = 36'(wz) - 36'(st_ty);

  // Exact inside the stated +-4,096 m domain: |dx| < 2^30 so dx*dx < 2^60 and
  // the sum is < 2^61, all comfortably inside 64 signed. EVERY comparison here
  // is signed on both sides — a Verilog compare goes unsigned if EITHER operand
  // is, and that trap already cost this tree 29 vanished tiles in GEOM.BINNER
  // (design/contracts/GEOM.CLIP.md).
  wire signed [63:0] dxw = 64'(dx);
  wire signed [63:0] dzw = 64'(dz);
  wire signed [63:0] d2 = dxw * dxw + dzw * dzw;

  //   covered = !(d2 > r_outer2 || d2 < r_inner2)   — BOTH radii inclusive.
  wire covered = !((d2 > st_r_outer2) || (d2 < st_r_inner2));

  // ---- the field record (S2) ------------------------------------------------
  wire [7:0] fld_tag = fld_tag_op_i[7:0];
  wire [2:0] fld_blend = fld_tag_op_i[10:8];
  wire [2:0] fld_age = fld_tag_op_i[14:12];
  wire [7:0] fld_src = fld_strength_i[15:8];

  wire [7:0] tex_tag = st_field ? fld_tag : st_tag;
  wire [7:0] tex_src = st_field ? fld_src : st_src;
  wire [2:0] tex_blend = st_field ? fld_blend : st_blend;
  wire [2:0] tex_age = st_field ? fld_age : st_age;

  // ---- stage 1: a read is in flight ----------------------------------------
  logic        s1_valid;
  logic [11:0] s1_texel;
  logic [ 7:0] s1_tag;
  logic [ 7:0] s1_src;
  logic [ 2:0] s1_blend;
  logic [ 2:0] s1_age;

  // ---- stage 2: blended, presenting the write and the result ---------------
  logic        s2_valid;
  logic [11:0] s2_texel;
  logic [ 7:0] s2_tag;
  logic [ 7:0] s2_after;
  logic [ 7:0] s2_before;
  logic        s2_wr_done;
  logic        s2_res_done;

  wire wr_fire = wr_valid_o && wr_ready_i;
  wire res_fire = res_valid_o && res_ready_i;
  wire s2_accept = s2_valid && (s2_wr_done || wr_fire) && (s2_res_done || res_fire);
  wire s2_free_next = !s2_valid || s2_accept;

  wire run_or_drain = (state == SRun) || (state == SDrain);
  wire s1_fire_out = run_or_drain && s1_valid && pg_valid_i && s2_free_next;
  wire s1_free_next = !s1_valid || s1_fire_out;

  // fld_ready_o must NOT depend on fld_valid_i (a ready that waits on its own
  // valid deadlocks against a producer that waits on ready). It depends on the
  // cursor's ability to move, and the read's valid depends on fld_valid_i.
  wire cursor_slot = (state == SRun) && !cursor_done;
  wire read_path_ok = covered ? (s1_free_next && req_ready_i) : 1'b1;
  assign fld_ready_o = cursor_slot && st_field && read_path_ok;
  wire fld_ok = !st_field || fld_valid_i;
  wire advance = cursor_slot && fld_ok && read_path_ok;

  // ---- SURFACE.SHEET request port ------------------------------------------
  wire acq_valid = (state == SAcq);
  assign req_valid_o  = (acq_valid && !acq_sent) || (cursor_slot && fld_ok && covered && s1_free_next);
  assign req_op_o     = acq_valid ? OpAcquire : OpRead;
  assign req_handle_o = st_handle;
  assign req_texel_o  = cursor[11:0];
  assign req_src_id_o = st_src_id;

  // Deliberately NOT a function of pg_valid_i: a ready that waits on its own
  // valid deadlocks against a producer that waits on ready.
  assign pg_ready_o   = acq_valid || (run_or_drain && s1_valid && s2_free_next);

  // ---- SURFACE.SHEET write port + stamp_results ----------------------------
  assign wr_valid_o = s2_valid && !s2_wr_done;
  assign wr_handle_o = st_handle;
  assign wr_texel_o = s2_texel;
  assign wr_tag_o = s2_tag;
  assign wr_strength_o = s2_after;
  // Both bytes always: `stamp_surface` writes the tag on every covered texel,
  // outside its if/else, even for a blend that leaves strength at 0.
  assign wr_we_tag_o = 1'b1;
  assign wr_we_strength_o = 1'b1;
  assign wr_src_id_o = st_src_id;

  assign res_valid_o = s2_valid && !s2_res_done;
  assign res_texel_o = s2_texel;
  assign res_tag_o = s2_tag;
  assign res_strength_o = s2_after;
  assign res_before_o = s2_before;
  assign res_src_id_o = st_src_id;

  assign cmd_ready_o = (state == SIdle);
  assign idle_o = (state == SIdle) && !s1_valid && !s2_valid;

  // ---- control -------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= SIdle;
      acq_sent <= 1'b0;
      cursor <= 13'd0;
      st_handle <= 32'd0;
      st_src_id <= 16'd0;
      st_tag <= 8'd0;
      st_src <= 8'd0;
      st_blend <= BlStamp;
      st_age <= 3'd0;
      st_field <= 1'b0;
      st_ex0 <= 32'sd0;
      st_ez0 <= 32'sd0;
      st_spanx <= 33'sd0;
      st_spanz <= 33'sd0;
      st_tx <= 32'sd0;
      st_ty <= 32'sd0;
      st_r_outer2 <= 64'sd0;
      st_r_inner2 <= 64'sd0;
      s1_valid <= 1'b0;
      s1_texel <= 12'd0;
      s1_tag <= 8'd0;
      s1_src <= 8'd0;
      s1_blend <= BlStamp;
      s1_age <= 3'd0;
      s2_valid <= 1'b0;
      s2_texel <= 12'd0;
      s2_tag <= 8'd0;
      s2_after <= 8'd0;
      s2_before <= 8'd0;
      s2_wr_done <= 1'b0;
      s2_res_done <= 1'b0;
      stamp_done_o <= 1'b0;
      stamp_rejected_o <= 1'b0;
      surface_stamps_o <= 32'd0;
      surface_texels_touched_o <= 32'd0;
    end else begin
      stamp_done_o <= 1'b0;
      stamp_rejected_o <= 1'b0;

      // --- stage 2 retire ---------------------------------------------------
      if (s2_valid) begin
        if (wr_fire) s2_wr_done <= 1'b1;
        if (res_fire) s2_res_done <= 1'b1;
        if (s2_accept) begin
          s2_valid <= 1'b0;
          s2_wr_done <= 1'b0;
          s2_res_done <= 1'b0;
          if (surface_texels_touched_o != 32'hFFFF_FFFF)
            surface_texels_touched_o <= surface_texels_touched_o + 32'd1;
        end
      end

      // --- stage 1 -> stage 2 (the blend happens here) ----------------------
      if (s1_fire_out) begin
        s1_valid <= 1'b0;
        s2_valid <= 1'b1;
        s2_texel <= s1_texel;
        s2_tag <= s1_tag;
        s2_before <= pg_strength_i;
        s2_after <= blend_apply(s1_blend, pg_strength_i, s1_src, s1_age);
        s2_wr_done <= 1'b0;
        s2_res_done <= 1'b0;
      end

      case (state)
        SIdle: begin
          if (cmd_valid_i) begin
            state <= SAcq;
            acq_sent <= 1'b0;
            cursor <= 13'd0;
            st_handle <= cmd_handle_i;
            st_src_id <= cmd_src_id_i;
            st_tag <= cmd_tag_i;
            // `strength >> 8`, the reference's TRUNCATION (not a round-half-up).
            st_src <= cmd_strength_i[15:8];
            // S1: the ratified ABI mapping, including the reference's `else`.
            st_blend <= cmd_blend_en_i ? cmd_blend_i :
                        ((cmd_operation_i == 8'd1) ? BlDecayAcc : BlStamp);
            st_age <= cmd_age_shift_i;
            st_field <= cmd_field_en_i;
            st_ex0 <= cmd_env_x0_i;
            st_ez0 <= cmd_env_z0_i;
            st_spanx <= 33'(cmd_env_x1_i) - 33'(cmd_env_x0_i);
            st_spanz <= 33'(cmd_env_z1_i) - 33'(cmd_env_z0_i);
            st_tx <= cmd_tx_i;
            st_ty <= cmd_ty_i;
            st_r_outer2 <= r_outer2_c;
            st_r_inner2 <= r_inner2_c;
          end
        end

        SAcq: begin
          if (req_valid_o && req_ready_i) acq_sent <= 1'b1;
          // S4: nothing may be written before residency is granted.
          if (pg_valid_i) begin
            if (pg_status_i == StOverflow) begin
              stamp_rejected_o <= 1'b1;
              state <= SIdle;
            end else begin
              state <= SRun;
            end
          end
        end

        SRun: begin
          if (advance) begin
            cursor <= cursor + 13'd1;
            if (covered) begin
              s1_valid <= 1'b1;
              s1_texel <= cursor[11:0];
              s1_tag   <= tex_tag;
              s1_src   <= tex_src;
              s1_blend <= tex_blend;
              s1_age   <= tex_age;
            end
          end
          if (cursor_done) state <= SDrain;
        end

        SDrain: begin
          if (!s1_valid && !s2_valid) begin
            state <= SIdle;
            stamp_done_o <= 1'b1;
            if (surface_stamps_o != 32'hFFFF_FFFF) surface_stamps_o <= surface_stamps_o + 32'd1;
          end
        end

        default: state <= SIdle;
      endcase
    end
  end

endmodule
