// zhao_terrain_lightlane.sv -- TERRAIN's LIT NORMALS: the world-vertex store
// beside the projector's arena, and the face-normal -> shade lane that reads it.
//
// Law, in citation order:
//   reports/OWNER-RULINGS-20260919-EVENING.md R21 -- "R5's GOAL stands (lit
//     terrain normals) and its MEANS changes: compute terrain face normals at
//     the REPLAY stage from the vertex store, with no third tessellation pass.
//     The sun direction gets its producer by ratifying SetEnvironment 0x0311.
//     SHADE's throughput must be proven against the replay-rate triangle
//     stream."
//   reports/OWNER-RULINGS-20260919-EVENING.md R11 / fpga/rtl/geometry/
//     zhao_geom_vattr.sv -- the pattern: a store keyed EXACTLY like the arena,
//     written at the same moment the arena position is written, read by the
//     replay's own per-triangle lookups. This is terrain's instance of it, and
//     what it stores is the WORLD vertex rather than an attribute.
//   fpga/rtl/terrain/zhao_terrain_normals.sv  -- the ratified face normal.
//   fpga/rtl/terrain/zhao_terrain_shade.sv    -- the ratified flat-shade law.
//
// ===========================================================================
// WHY THE WORLD VERTEX HAS TO BE STORED AT ALL
// ===========================================================================
// The face normal is a cross product of the triangle's WORLD corners, and by
// the time a terrain triangle exists as a triangle -- at the replay stage,
// where a reference names three arena rows -- the world positions are gone:
// `zhao_proj_subsystem`'s arena holds the PROJECTED vertex (x, y, d, w,
// behind), which is what replay needs and all it needs.
//
// The three candidates were: widen the arena (a change to `zhao_vertex_arena`,
// whose SymbiYosys proof and six refusals are the reason terrain reuses it);
// recompute the world position at replay (a second implementation of the
// tessellator's lattice law); or keep a store beside the arena, keyed by the
// same {arena, index}, written on the same beat. R11 ruled the third for
// geometry's attributes, for the same reason, and this is that ruling applied
// to the quantity terrain needs. Nothing joins two streams: the key is the
// producer's own, by construction.
//
// COST: ARENAS * DEPTH rows of {generation, vz, vy, vx} = 104 bits, which at
// the console's 4 x 81 is 33,696 bits -- M10K, and the owner prefers M10K over
// ALM. No DSP: the store is a memory and the arithmetic is the two blocks it
// feeds.
//
// ===========================================================================
// THE GENERATION IS CARRIED IN THE ROW, NOT BESIDE IT
// ===========================================================================
// `zhao_vertex_arena` refuses a stale read by comparing the reference's
// generation against the arena's. This store cannot see that comparison, so it
// makes its own out of the same fact: every row records the generation it was
// written in, and a reference whose generation does not match ALL THREE rows
// is refused here too, counted in `stale_reads_o`, and emits no light. That
// keeps this lane's refusals the same events as the replay's rather than a
// second opinion about them -- and it is a real guard with a real counter,
// fired by stimulus in the directed test.
//
// ===========================================================================
// THE RATE, AND WHY THIS LANE THROTTLES THE REFERENCE STREAM
// ===========================================================================
// `zhao_terrain_shade` is 147 clocks per triangle by its own header (0 DSP, 2
// M10K, one triangle in flight -- the shape the owner's memory-over-ALM rule
// asked for). The reference stream can offer a triangle every clock. So this
// lane presents `ref_ready_o` and the composer ANDs it with the replay shell's
// own ready: a terrain triangle is taken when BOTH its projected replay and
// its light can take it, and terrain replay runs at the light's rate.
//
// That is a schedule change, which is what R21 asks for, and it is proven in
// clocks rather than asserted: at the ruled 2,000 terrain triangles per frame
// (design/budgets/workloads.yml, the number both shade's and normals' headers
// derive) the lane costs 2,000 * 147 = 294,000 of the frame's 1,666,666
// clocks -- 17.6%, with the projector's own replay running inside that. The
// directed test measures the interval per triangle and fails if it grows.
//
// Conservative SystemVerilog subset only (charter 2). No generate blocks;
// elaboration guards live inside `initial begin` (Quartus 17 form law).
`default_nettype none

module zhao_terrain_lightlane #(
    parameter int unsigned ARENAS  = 4,
    parameter int unsigned DEPTH   = 81,
    parameter int unsigned GEN_W   = 8,
    parameter int unsigned SRCW    = 16,
    parameter int unsigned INDEX_W = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W = $clog2(ARENAS) + 1
) (
    input var logic clk,
    input var logic rst_n,

    // ---- the fill beat: the same one that writes the projector's arena ------
    input var logic                 fill_valid_i,
    input var logic                 fill_ready_i,
    input var logic [ARENA_W-1:0]   fill_arena_i,
    input var logic [INDEX_W-1:0]   fill_index_i,
    input var logic signed [31:0]   fill_vx_i,
    input var logic signed [31:0]   fill_vy_i,
    input var logic signed [31:0]   fill_vz_i,

    // ---- the arena lifetime: which generation the rows being written belong to
    input var logic                 open_i,
    /* verilator lint_off UNUSEDSIGNAL */
    // ARENA_W is clog2(ARENAS)+1 -- the shell's spare bit rides along and is
    // not an address here.
    input var logic [ARENA_W-1:0]   open_arena_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input var logic [GEN_W-1:0]     open_gen_i,

    // ---- the reference stream, at the replay's own rate ---------------------
    input  var logic                ref_valid_i,
    output var logic                ref_ready_o,
    input  var logic [ARENA_W-1:0]  ref_arena_i,
    input  var logic [GEN_W-1:0]    ref_gen_i,
    input  var logic [INDEX_W-1:0]  ref_ia_i,
    input  var logic [INDEX_W-1:0]  ref_ib_i,
    input  var logic [INDEX_W-1:0]  ref_ic_i,
    input  var logic [SRCW-1:0]     ref_src_id_i,

    // ---- the sun, from SetEnvironment through zhao_light_env (R21/R25) ------
    input var logic signed [31:0]   sun_x_i,
    input var logic signed [31:0]   sun_y_i,
    input var logic signed [31:0]   sun_z_i,

    // ---- the per-triangle base light ----------------------------------------
    output var logic                light_valid_o,
    input  var logic                light_ready_i,
    output var logic signed [31:0]  light_base_o,
    output var logic                light_degenerate_o,
    output var logic [SRCW-1:0]     light_src_id_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] refs_taken_o,
    output var logic [31:0] lights_emitted_o,
    output var logic [31:0] stale_reads_o,      // a corner from an older generation
    output var logic [31:0] normals_evaluated_o,
    output var logic [31:0] triangles_shaded_o,
    output var logic [31:0] degenerate_count_o,
    output var logic [31:0] base_sat_o,
    output var logic [31:0] degen_mismatch_o,
    output var logic        idle_o
);

  localparam int unsigned ROWS = ARENAS * DEPTH;
  localparam int unsigned RW   = (ROWS > 1) ? $clog2(ROWS) : 1;

  initial begin
    if (DEPTH == 0 || ARENAS == 0) begin
      $fatal(1, "zhao_terrain_lightlane: ARENAS and DEPTH must be non-zero");
    end
  end

  // ==========================================================================
  // THE STORE
  // ==========================================================================
  // {generation, vz, vy, vx}: one row per {arena, index}, written on the fill
  // beat and read three times per reference.
  localparam int unsigned ROW_W = 96 + GEN_W;
  logic [ROW_W-1:0] rd_q;
  logic [RW-1:0]    rd_addr_c;
  logic [ROW_W-1:0] mem [0:ROWS-1];

  logic [GEN_W-1:0] gen_q [0:ARENAS-1];
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int i = 0; i < int'(ARENAS); i++) gen_q[i] <= '0;
    end else if (open_i) begin
      // The arena index's top bit is the shell's spare (ARENA_W is clog2+1);
      // the store is indexed by the arenas that exist.
      gen_q[open_arena_i[$clog2(ARENAS)-1:0]] <= open_gen_i;
    end
  end

  function automatic logic [RW-1:0] row_of(input logic [ARENA_W-1:0] a,
                                           input logic [INDEX_W-1:0] i);
    row_of = RW'((32'(a) * 32'(DEPTH)) + 32'(i));
  endfunction

  wire fill_fire_c = fill_valid_i && fill_ready_i;

  always_ff @(posedge clk) begin
    if (fill_fire_c) begin
      mem[row_of(fill_arena_i, fill_index_i)] <=
          {gen_q[fill_arena_i[$clog2(ARENAS)-1:0]], fill_vz_i, fill_vy_i, fill_vx_i};
    end
    rd_q <= mem[rd_addr_c];
  end

  // ==========================================================================
  // THE REFERENCE WALK
  // ==========================================================================
  localparam logic [2:0] S_IDLE = 3'd0;
  localparam logic [2:0] S_RD_B = 3'd1;   // A's row is being read; ask for B
  localparam logic [2:0] S_CAP_B = 3'd3;
  localparam logic [2:0] S_CAP_C = 3'd4;
  localparam logic [2:0] S_OFFER = 3'd5;

  logic [2:0]          st_q;
  logic [ARENA_W-1:0]  r_arena_q;
  logic [GEN_W-1:0]    r_gen_q;
  logic [INDEX_W-1:0]  r_ib_q, r_ic_q;
  logic [SRCW-1:0]     r_src_q;
  logic signed [31:0]  ax_q, ay_q, az_q, bx_q, by_q, bz_q, cx_q, cy_q, cz_q;
  logic                stale_q;

  always_comb begin
    rd_addr_c = '0;
    unique case (st_q)
      S_IDLE:  rd_addr_c = row_of(ref_arena_i, ref_ia_i);
      S_RD_B:  rd_addr_c = row_of(r_arena_q, r_ib_q);
      S_CAP_B: rd_addr_c = row_of(r_arena_q, r_ic_q);
      default: rd_addr_c = row_of(r_arena_q, r_ic_q);
    endcase
  end

  // The normal producer's port, and its consumer.
  logic               nrm_valid, nrm_ready;
  logic signed [31:0] nx_w, ny_w, nz_w;
  logic               nrm_degen;
  logic [15:0]        nrm_src;

  logic tri_valid_c, tri_ready_w;
  assign tri_valid_c = (st_q == S_OFFER) && !stale_q;

  assign ref_ready_o = (st_q == S_IDLE);
  assign idle_o      = (st_q == S_IDLE) && !nrm_valid && !light_valid_o;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q      <= S_IDLE;
      r_arena_q <= '0;
      r_gen_q   <= '0;
      r_ib_q    <= '0;
      r_ic_q    <= '0;
      r_src_q   <= '0;
      ax_q      <= '0; ay_q <= '0; az_q <= '0;
      bx_q      <= '0; by_q <= '0; bz_q <= '0;
      cx_q      <= '0; cy_q <= '0; cz_q <= '0;
      stale_q   <= 1'b0;
      refs_taken_o  <= 32'd0;
      stale_reads_o <= 32'd0;
    end else begin
      unique case (st_q)
        S_IDLE: begin
          if (ref_valid_i) begin
            r_arena_q <= ref_arena_i;
            r_gen_q   <= ref_gen_i;
            r_ib_q    <= ref_ib_i;
            r_ic_q    <= ref_ic_i;
            r_src_q   <= ref_src_id_i;
            stale_q   <= 1'b0;
            if (refs_taken_o != 32'hFFFF_FFFF) refs_taken_o <= refs_taken_o + 32'd1;
            st_q      <= S_RD_B;
          end
        end

        // A's row lands this clock (the read was issued in S_IDLE).
        S_RD_B: begin
          ax_q <= signed'(rd_q[31:0]);
          ay_q <= signed'(rd_q[63:32]);
          az_q <= signed'(rd_q[95:64]);
          if (rd_q[96 +: GEN_W] != r_gen_q) stale_q <= 1'b1;
          st_q <= S_CAP_B;
        end

        S_CAP_B: begin
          bx_q <= signed'(rd_q[31:0]);
          by_q <= signed'(rd_q[63:32]);
          bz_q <= signed'(rd_q[95:64]);
          if (rd_q[96 +: GEN_W] != r_gen_q) stale_q <= 1'b1;
          st_q <= S_CAP_C;
        end

        S_CAP_C: begin
          cx_q <= signed'(rd_q[31:0]);
          cy_q <= signed'(rd_q[63:32]);
          cz_q <= signed'(rd_q[95:64]);
          if (rd_q[96 +: GEN_W] != r_gen_q) stale_q <= 1'b1;
          st_q <= S_OFFER;
        end

        S_OFFER: begin
          // A stale corner is refused here exactly as the arena refuses it
          // downstream: nothing is offered, and the refusal is counted.
          if (stale_q) begin
            if (stale_reads_o != 32'hFFFF_FFFF) stale_reads_o <= stale_reads_o + 32'd1;
            st_q <= S_IDLE;
          end else if (tri_ready_w) begin
            st_q <= S_IDLE;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  zhao_terrain_normals u_normals (
      .clk      (clk),
      .rst_n    (rst_n),
      .tri_valid_i(tri_valid_c),
      .tri_ready_o(tri_ready_w),
      .ax_i     (ax_q),
      .ay_i     (ay_q),
      .az_i     (az_q),
      .bx_i     (bx_q),
      .by_i     (by_q),
      .bz_i     (bz_q),
      .cx_i     (cx_q),
      .cy_i     (cy_q),
      .cz_i     (cz_q),
      .src_id_i (16'(r_src_q)),
      .nrm_valid_o(nrm_valid),
      .nrm_ready_i(nrm_ready),
      .nx_o     (nx_w),
      .ny_o     (ny_w),
      .nz_o     (nz_w),
      .degenerate_o(nrm_degen),
      .src_id_o (nrm_src),
      .terrain_samples_evaluated_o(normals_evaluated_o),
      /* verilator lint_off PINCONNECTEMPTY */
      .idle_o   ()
      /* verilator lint_on PINCONNECTEMPTY */
  );

  logic shade_tri_ready;
  assign nrm_ready = shade_tri_ready;

  logic [15:0] shade_src;
  assign light_src_id_o = SRCW'(shade_src);

  zhao_terrain_shade #(
      .CNTW (32)
  ) u_shade (
      .clk      (clk),
      .rst_n    (rst_n),
      .tri_valid_i(nrm_valid),
      .tri_ready_o(shade_tri_ready),
      .n_x_i    (nx_w),
      .n_y_i    (ny_w),
      .n_z_i    (nz_w),
      .degenerate_i(nrm_degen),
      .sun_x_i  (sun_x_i),
      .sun_y_i  (sun_y_i),
      .sun_z_i  (sun_z_i),
      .src_id_i (nrm_src),
      .base_valid_o(light_valid_o),
      .base_ready_i(light_ready_i),
      .base_o   (light_base_o),
      .degenerate_o(light_degenerate_o),
      .src_id_o (shade_src),
      .triangles_shaded_o(triangles_shaded_o),
      .degenerate_count_o(degenerate_count_o),
      .base_sat_o        (base_sat_o),
      .degen_mismatch_o  (degen_mismatch_o),
      /* verilator lint_off PINCONNECTEMPTY */
      .table_ready_o(),
      .idle_o       ()
      /* verilator lint_on PINCONNECTEMPTY */
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) lights_emitted_o <= 32'd0;
    else if (light_valid_o && light_ready_i && (lights_emitted_o != 32'hFFFF_FFFF))
      lights_emitted_o <= lights_emitted_o + 32'd1;
  end

endmodule

`default_nettype wire
