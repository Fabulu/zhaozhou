// zhao_raster_earlyz.sv — RASTER.EARLYZ: conservative early-Z rejection plus
// the coarse transparent-depth bins (phase 5, ZH-059).
//
// Law (in citation order):
//   design/contracts/RASTER.EARLYZ.md — the block contract.
//   design/blocks.yml — `inputs: [covered_fragments]`, `outputs:
//       [shaded_candidates, z_reject]`, `latency: fixed:1`, "1 reject
//       decision per clock", counters `early_z_rejects` + `covered_fragments`,
//       and the note this file exists to honour: **"Kept separate from
//       RASTER.FRAGMENT by architect ruling (1.D)."**
//   spec/qformats.md §8 — `invw24` depth, LARGER IS CLOSER, clear value 0,
//       and the exact late test `pass ⟺ d_new > d_old` (strict; ties fail).
//       Every rejection this block makes is a rejection that test would also
//       have made — see THE CONSERVATISM ARGUMENT below.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 "Pass order inside each tile"
//       — pass 6 is "coarse-depth-binned translucent geometry"; the bins
//       below are that binning's per-tile occupancy.
//   spec/sky_and_beams.md §1 — "Deterministic sub-order: sun (z=+2560) before
//       cloud (z=+1792) via coarse back-to-front binning".
//
// ---------------------------------------------------------------------------
// WHY THIS IS NOT PART OF RASTER.FRAGMENT (ruling 1.D, argued rather than
// merely cited)
// ---------------------------------------------------------------------------
// The two blocks answer different questions with different resources:
//
//   · RASTER.FRAGMENT's depth test is EXACT and PER-PIXEL. To make it, the
//     block must read the tile store — it owns `tile_read` (port A) and
//     performs a read-modify-write. Its answer is therefore only available
//     AFTER a memory access has been spent, and after the fragment's texel
//     has been fetched, because it is the same pipeline stage that blends.
//   · THIS block's decision is CONSERVATIVE and PER-TILE. It touches no
//     memory at all: `inputs: [covered_fragments]` is the entire input list,
//     there is no tile-store port anywhere in this file, and its state is a
//     few hundred flops. That is exactly what lets it answer in `fixed:1`
//     BEFORE the texture sample and before the tile-store read.
//
// Folding this into RASTER.FRAGMENT would mean the only depth rejection in
// the machine is the exact one, which happens after the TMU bandwidth and the
// tile-store read have already been paid — i.e. it would delete the entire
// saving early-Z exists to make. Keeping them separate also keeps the latency
// classes honest: `fixed:1` here (no memory), `variable` there (a memory
// round trip and a not-yet-built texture path). Two blocks, two contracts,
// two resource ownerships. The ruling is right and this file does not
// re-litigate it.
//
// ---------------------------------------------------------------------------
// THE CONSERVATISM ARGUMENT — why a rejection here is always safe
// ---------------------------------------------------------------------------
// `floor_r` is maintained as a LOWER BOUND on the stored depth of EVERY ONE of
// the tile's 256 pixels. Given that invariant, a fragment with
// `depth ≤ floor_r` fails `depth > stored` at every pixel of the tile, so
// rejecting it is exactly what the late test would have done — no pixel is
// lost, ever. The block is allowed to be pessimistic (keep a fragment the
// late test will kill); it is never allowed to be optimistic.
//
// The invariant is established and preserved by two facts and one accumulator:
//
//   1. At `tile_begin` every pixel holds the tile's clear depth, so
//      `floor_r := clear_depth` is exact.
//   2. A depth WRITE only ever raises a pixel's depth: the late test admits a
//      write only when `d_new > d_old`. So min-over-pixels is non-decreasing
//      and a bound established once stays a bound forever. (`z_force_far`
//      writes the far constant 0, which is ≤ every depth, so it is folded in
//      at its WRITTEN value, not at the fragment's interpolated one.)
//   3. RAISING the bound needs evidence that EVERY pixel was covered. The
//      accumulator collects it: `acc_mask_r` marks the pixels hit by
//      fragments that will certainly write depth, and `acc_min_r` is the
//      smallest depth among them. When `acc_mask_r` is all ones, every pixel
//      has taken at least one depth write of at least `acc_min_r`, so
//      `floor_r := max(floor_r, acc_min_r)` is sound; the accumulator then
//      restarts to collect the next round. This is a hierarchical-Z floor,
//      and the case it is built for is the common one: the charter's pass-1
//      prefill and any full-tile opaque surface raise the floor for
//      everything behind them in one sweep.
//
// "Will certainly write depth" is deliberately narrow — `blend == REPLACE`
// (opaque), depth writes enabled, no alpha test, stencil function ALWAYS, and
// the fragment survived this block's own reject. Anything that COULD be
// killed downstream (a masked star disc, a stencilled decal) contributes
// nothing to the accumulator. A fragment that is merely blended contributes
// nothing either: an additive beam leaves depth untouched by construction
// (`Z-write OFF`, spec/sky_and_beams.md §2), so it is no evidence about depth.
// ENFORCED-BY: tests/raster/raster_earlyz_directed.cpp:test_only_certain_writers_raise_the_floor
// (four disqualifiers, one full-tile sweep each, none of which may move the
// floor, plus a control sweep that must), and the invariant itself by
// tests/raster/raster_earlyz_directed.cpp:test_floor_rises_only_on_full_coverage
// (255 of 256 covered pixels move the floor not at all).
//
// ---------------------------------------------------------------------------
// THE COARSE TRANSPARENT-DEPTH BINS
// ---------------------------------------------------------------------------
// Eight bins, `bin = depth[23:21]` — the top three bits of the invw24 depth,
// so bin 7 is nearest and bin 0 farthest, the same sense as the depth itself.
// Each surviving fragment carries its bin out beside it (`cand_bin_o`), and
// `bin_mask_o` accumulates which bins the tile has seen since `tile_begin`.
// That is the per-tile occupancy the charter's pass-6 "coarse-depth-binned
// translucent geometry" and sky_and_beams §1's deterministic sun-before-cloud
// sub-order need in order to walk bins back-to-front without sorting anything.
//
// The bins are DERIVED, not stored per pixel: this block owns no per-pixel
// memory, so a bin is a classification of the fragment in flight and the mask
// is 8 flops. What this block does NOT do is the sorting or the scheduling —
// it publishes the occupancy; the tile scheduler decides the order.
//
// WHAT THIS BLOCK IS NOT: no exact depth test and no per-pixel depth state
// (RASTER.FRAGMENT, which owns the tile-store port), no tile memory of any
// kind, no stencil test, no alpha test, no shading, no blend, no coverage
// (RASTER.EDGEWALK), no bin ORDERING or tile scheduling, and no feedback path
// from RASTER.FRAGMENT — the floor is derived from what passes through here,
// never from what the tile store later contains.
//
// ---------------------------------------------------------------------------
// THE PAYLOAD IS OPAQUE
// ---------------------------------------------------------------------------
// This block decodes exactly three things — the address, the depth, and the
// six `state` bits its accumulator qualification needs. Everything else the
// fragment carries (colour, alpha, tag, texel, stencil reference) rides
// through `frag_payload_i` → `cand_payload_o` untouched and unexamined, the
// same way RASTER.TILESTORE stores 64 bits it never decodes. That keeps the
// shading packet's layout owned by exactly one block (RASTER.FRAGMENT) and
// keeps this contract to three fields.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_earlyz).

module zhao_raster_earlyz #(
  // Width of the opaque shading payload carried through. The default is the
  // 88 bits zhao_raster_fragment's packet needs (24 vertex RGB + 8 vertex
  // alpha + 8 tag + 8 stencil ref + 24 texel RGB + 8 texel alpha + 8 texel
  // index); it is a parameter so this block never has to be edited when that
  // packet grows.
  parameter int unsigned PAYLOAD_W = 88
) (
  input  logic clk,
  input  logic rst_n,

  // ---- tile_begin: the tile the following fragments belong to ------------
  // Accepted unconditionally, exactly like RASTER.TILESTORE's `clear`, and
  // like that clear it must not be issued with work in flight — see the
  // Backpressure section of the contract. It resets the floor to the tile's
  // clear depth and empties both the accumulator and the bin mask.
  input  logic        tile_begin_i,
  input  logic [23:0] tile_clear_depth_i,

  // ---- covered_fragments in (RASTER.EDGEWALK's coverage, expanded) -------
  input  logic                 frag_valid_i,
  output logic                 frag_ready_o,
  input  logic [7:0]           frag_addr_i,     // {row[3:0], col[3:0]}
  input  logic [23:0]          frag_depth_i,    // invw24, larger is closer
  input  logic [31:0]          frag_state_i,    // the fragment state word
  input  logic [15:0]          frag_src_id_i,
  input  logic [PAYLOAD_W-1:0] frag_payload_i,  // opaque, see above

  // ---- shaded_candidates out (survivors only) ---------------------------
  output logic                 cand_valid_o,
  input  logic                 cand_ready_i,
  output logic [7:0]           cand_addr_o,
  output logic [23:0]          cand_depth_o,
  output logic [31:0]          cand_state_o,
  output logic [15:0]          cand_src_id_o,
  output logic [PAYLOAD_W-1:0] cand_payload_o,
  output logic [2:0]           cand_bin_o,      // coarse depth bin, 7 = nearest

  // ---- z_reject out (an EVENT, not a channel) ---------------------------
  // A rejected fragment produces no downstream beat at all — that is the
  // point of the block — so `z_reject_o` is a one-cycle pulse rather than a
  // handshake, and `early_z_rejects_o` is its durable form.
  output logic                 z_reject_o,
  output logic [7:0]           z_reject_addr_o,

  // ---- per-tile observability -------------------------------------------
  output logic [7:0]           bin_mask_o,   // bins occupied since tile_begin
  output logic [23:0]          z_floor_o,    // the conservative depth floor

  // ---- counters ---------------------------------------------------------
  output logic [31:0] early_z_rejects_o,
  output logic [31:0] covered_fragments_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  // ---- the fragment state word (layout owned by zhao_raster_fragment) ----
  // Only the fields below are decoded here; the rest is carried.
  localparam logic [1:0] BL_REPLACE  = 2'd0;
  localparam logic [1:0] STEN_ALWAYS = 2'd0;

  logic       st_z_test_en, st_z_write_dis, st_z_force_far, st_atest_en;
  logic [1:0] st_blend, st_sten_func;
  always_comb begin
    st_z_test_en   = frag_state_i[0];    // ST_Z_TEST_EN
    st_z_write_dis = frag_state_i[1];    // ST_Z_WRITE_DIS
    st_z_force_far = frag_state_i[2];    // ST_Z_FORCE_FAR
    st_blend       = frag_state_i[4:3];  // ST_BLEND
    st_atest_en    = frag_state_i[7];    // ST_ATEST_EN
    st_sten_func   = frag_state_i[17:16];  // ST_STEN_FUNC
  end

  // ---- the conservative state -------------------------------------------
  logic [23:0]  floor_r;     // lower bound on the stored depth of EVERY pixel
  logic [255:0] acc_mask_r;  // pixels certain to have taken a depth write
  logic [7:0]   seen_count_r;  // UNIQUE pixels in acc_mask_r, 0..255
  logic [23:0]  acc_min_r;   // the smallest depth among those writes
  logic [7:0]   bin_mask_r;

  assign z_floor_o  = floor_r;
  assign bin_mask_o = bin_mask_r;

  // ---- the registered output stage (this is the `fixed:1`) ---------------
  logic                 out_v_r;
  logic [7:0]           out_addr_r;
  logic [23:0]          out_depth_r;
  logic [31:0]          out_state_r;
  logic [15:0]          out_src_r;
  logic [PAYLOAD_W-1:0] out_payload_r;
  logic [2:0]           out_bin_r;

  assign cand_valid_o   = out_v_r;
  assign cand_addr_o    = out_addr_r;
  assign cand_depth_o   = out_depth_r;
  assign cand_state_o   = out_state_r;
  assign cand_src_id_o  = out_src_r;
  assign cand_payload_o = out_payload_r;
  assign cand_bin_o     = out_bin_r;

  // ---- handshakes --------------------------------------------------------
  // Hygiene: `frag_ready_o` is a function of the OUTPUT channel's ready — a
  // different channel's, the permitted direction — and never of its own.
  // A rejected fragment leaves the output stage empty, so a stream of pure
  // rejects still retires one per clock ("1 reject decision per clock").
  logic out_free, frag_acc;
  assign out_free     = !out_v_r || cand_ready_i;
  assign frag_ready_o = out_free;
  assign frag_acc     = frag_valid_i && frag_ready_o;

  // ---- THE DECISION ------------------------------------------------------
  // Reject ⟺ the depth test is on AND the fragment cannot beat the floor.
  // `≤` and not `<`, because spec/qformats.md §8's late test is STRICT
  // (`d_new > d_old`, ties fail): a fragment exactly at the floor loses at
  // every pixel too. With the test off (spec/sky_and_beams.md §1.1's
  // `sky_backdrop`) nothing is ever rejected.
  logic reject_c;
  assign reject_c = st_z_test_en && (frag_depth_i <= floor_r);

  // ---- accumulator qualification (deliberately narrow; see the header) ---
  logic        hiz_qualify;
  logic [23:0] hiz_depth;
  always_comb begin
    hiz_qualify = frag_acc && !reject_c &&
                  (st_blend == BL_REPLACE) &&      // opaque: replaces, not blends
                  !st_z_write_dis &&               // it writes depth
                  !st_atest_en &&                  // it cannot be masked away
                  (st_sten_func == STEN_ALWAYS);   // it cannot be stencilled away
    hiz_depth   = st_z_force_far ? 24'd0 : frag_depth_i;
  end

  // ---- EXACT UNIQUE-COVERAGE COUNTING, not a 256-input reduction ---------
  // reports/MHZArchitected offender 3, and the last one on its list that any
  // fit has ever confirmed. What used to be here:
  //
  //     acc_mask_next = acc_mask_r;
  //     acc_mask_next[frag_addr_i] = 1'b1;
  //     acc_full = &acc_mask_next;          <-- 256-input reduction
  //
  // and `acc_full` then chose whether all 256 mask registers took zero or took
  // `acc_mask_next`. So every mask bit fed a 256-input reduction whose result
  // fanned back out to every mask bit's next-state mux, with a dynamic bit
  // update inside the same cone. The note's description is exact: "almost a
  // perfect machine for generating large total negative slack over hundreds of
  // endpoints."
  //
  // The fit at b248b8b agreed -- Early-Z owned ALL 100 worst paths,
  // floor_r -> acc_mask_r, once EDGEWALK left the list.
  //
  // The replacement is the note's, unchanged:
  //
  //     seen       = acc_mask[address]
  //     new_pixel  = qualify && !seen
  //     round_done = new_pixel && (seen_count == 255)
  //
  // A 256-input reduction becomes ONE SELECTED-BIT LOOKUP and an eight-bit
  // compare.
  //
  // SEMANTICALLY IDENTICAL, including the 256th pixel counting in the SAME
  // cycle it arrives: the old test was true exactly when every one of the 256
  // bits was set after this cycle's contribution, and 256 unique qualifying
  // pixels is the only way to reach that. `acc_min` still updates on every
  // qualifying fragment whether or not the pixel was new, which is what the
  // note requires and what keeps the promoted floor identical.
  logic         seen, new_pixel, round_done;
  logic [23:0]  acc_min_next;

  assign seen       = acc_mask_r[frag_addr_i];
  assign new_pixel  = hiz_qualify && !seen;
  assign round_done = new_pixel && (seen_count_r == 8'd255);

  always_comb begin
    acc_min_next = acc_min_r;
    if (hiz_qualify && (hiz_depth < acc_min_r)) acc_min_next = hiz_depth;
  end

  // ---- sequential --------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      floor_r             <= 24'd0;
      acc_mask_r          <= 256'd0;
      seen_count_r        <= 8'd0;
      acc_min_r           <= 24'hFFFFFF;
      bin_mask_r          <= 8'd0;
      out_v_r             <= 1'b0;
      out_addr_r          <= 8'd0;
      out_depth_r         <= 24'd0;
      out_state_r         <= 32'd0;
      out_src_r           <= 16'd0;
      out_payload_r       <= {PAYLOAD_W{1'b0}};
      out_bin_r           <= 3'd0;
      z_reject_o          <= 1'b0;
      z_reject_addr_o     <= 8'd0;
      early_z_rejects_o   <= 32'd0;
      covered_fragments_o <= 32'd0;
    end else begin
      z_reject_o <= 1'b0;

      // ---- the output stage drains -------------------------------------
      if (out_v_r && cand_ready_i) out_v_r <= 1'b0;

      // ---- one fragment, one decision, one cycle later ------------------
      if (frag_acc) begin
        if (covered_fragments_o != CNT_MAX) covered_fragments_o <= covered_fragments_o + 32'd1;

        if (reject_c) begin
          z_reject_o      <= 1'b1;
          z_reject_addr_o <= frag_addr_i;
          if (early_z_rejects_o != CNT_MAX) early_z_rejects_o <= early_z_rejects_o + 32'd1;
        end else begin
          out_v_r       <= 1'b1;
          out_addr_r    <= frag_addr_i;
          out_depth_r   <= frag_depth_i;
          out_state_r   <= frag_state_i;
          out_src_r     <= frag_src_id_i;
          out_payload_r <= frag_payload_i;
          out_bin_r     <= frag_depth_i[23:21];
          bin_mask_r[frag_depth_i[23:21]] <= 1'b1;
        end

        // ---- the hierarchical-Z floor ----------------------------------
        if (round_done) begin
          // Every pixel has taken a depth write of at least `acc_min_next`.
          // The floor never moves backwards: `max`, not assignment.
          if (acc_min_next > floor_r) floor_r <= acc_min_next;
          acc_mask_r   <= 256'd0;
          acc_min_r    <= 24'hFFFFFF;
          seen_count_r <= 8'd0;
        end else begin
          // Only the SELECTED bit is written, not all 256 from a reduction.
          if (hiz_qualify) acc_mask_r[frag_addr_i] <= 1'b1;
          acc_min_r <= acc_min_next;
          if (new_pixel) seen_count_r <= seen_count_r + 8'd1;
        end
      end

      // ---- tile_begin is LAST: it wins over this cycle's fragment -------
      // A new tile's clear invalidates every conclusion drawn from the old
      // one, so it resets the floor, the accumulator and the bin mask
      // outright. The output stage is NOT cleared: a candidate already
      // decided belongs to the tile it was decided in and must still be
      // delivered.
      if (tile_begin_i) begin
        floor_r    <= tile_clear_depth_i;
        acc_mask_r   <= 256'd0;
        seen_count_r <= 8'd0;
        acc_min_r  <= 24'hFFFFFF;
        bin_mask_r <= 8'd0;
      end
    end
  end

endmodule : zhao_raster_earlyz
