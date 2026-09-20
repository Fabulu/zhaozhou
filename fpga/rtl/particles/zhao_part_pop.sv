// zhao_part_pop.sv -- PART.POP: the population descriptor bank. The producer
// core entry I7 was waiting for.
//
// ENFORCED-BY: tests/particles/part_pop_directed.cpp:main
// REFERENCE:   zref::population (reference/include/zref/zref_population.hpp)
//
// ---------------------------------------------------------------------------
// WHAT THIS IS, AND WHY IT IS A BLOCK RATHER THAN A LATCH IN THE COMPOSER
// ---------------------------------------------------------------------------
// Owner ruling R41 (2026-09-19): "Ratify `SetPopulation` (origin, plane,
// active_count), lowered by CMD.EXEC. active_count also seeds PART.STATE's
// first generation, replacing I1's provisional HPS seed."
//
// Until now the origin and the plane were BOARD PINS on `zhao_console_core`
// (entry I7) -- live inputs of a composed engine with no ratified carrier --
// and the store's first generation came from a provisional HPS seed port
// (entry I1's `part_seed_*`, R46). SetPopulation 0x0303 carries all of it, and
// this block is what stands between the command stream and the engine:
//
//   * IT HOLDS THE DESCRIPTOR AS LEVELS. PART.COLLIDE's plane and
//     PART.TERRAIN_TAP's origin are read on every particle beat, so they are
//     registers here, not a handshake there.
//   * IT REFUSES WHAT IT CANNOT CARRY, AND COUNTS IT. Two wire fields are
//     wider than the engine's, and a silent narrowing of either is wrong in a
//     way that looks like physics rather than like a bug -- truncating a
//     +Y normal of 0x0800 to twelve bits gives -2048, a floor that has become
//     a ceiling. See the reference header.
//   * IT OWES THE STORE A SEED. `active_count` becomes `zhao_part_hps`'s seed
//     {buffer 0, count} through that block's own handshake, so the ratified
//     law -- "the first generation is staged in buffer 0" (spec/commands.zidl,
//     SetPopulation) -- is implemented in one place.
//
// A REFUSED RECORD IS REFUSED WHOLE. One record describes one population, so
// applying the fields that fit and dropping the one that did not would leave
// the engine holding a descriptor nobody wrote. The bank keeps what it had.
//
// ATOMICITY IS CMD.EXEC'S, NOT THIS BLOCK'S. `zhao_cmd_exec` offers a record
// only on a CLEAN verdict (ZH_ABI_OK and not poisoned), so an abandoned packet
// never reaches here -- the same rule SetEnvironment lands under.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_part_pop #(
    // The store's capacity, in records. An `active_count` above it is refused,
    // never wrapped.
    parameter int unsigned CAPACITY = 32768,
    parameter int unsigned CNT_W    = $clog2(CAPACITY) + 1,
    // PART.COLLIDE's normal format. Owner knobs there, mirrored here so the
    // refusal boundary and the consumer's width are ONE number.
    parameter int unsigned NRM_W    = 12
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one SetPopulation record, from CMD.EXEC ----------------------------
    // Offered on a clean verdict; taken in one cycle, so `ready` is constant.
    input  var logic          rec_valid_i,
    output var logic          rec_ready_o,
    input  var logic [31:0]   rec_population_i,
    input  var logic signed [31:0] rec_origin_x_i,   // 1/256-m grid
    input  var logic signed [31:0] rec_origin_y_i,
    input  var logic signed [31:0] rec_origin_z_i,
    input  var logic [31:0]   rec_active_count_i,
    input  var logic signed [31:0] rec_plane_c_i,    // Q10
    input  var logic signed [15:0] rec_plane_nx_i,   // Q1.10
    input  var logic signed [15:0] rec_plane_ny_i,
    input  var logic signed [15:0] rec_plane_nz_i,
    input  var logic [15:0]   rec_flags_i,           // b0 seed, b1 plane_enable

    // ---- the descriptor, as LEVELS, to the particle engine -------------------
    output var logic signed [31:0]      origin_x_o,
    output var logic signed [31:0]      origin_y_o,
    output var logic signed [31:0]      origin_z_o,
    output var logic                    plane_en_o,
    output var logic signed [NRM_W-1:0] plane_nx_o,
    output var logic signed [NRM_W-1:0] plane_ny_o,
    output var logic signed [NRM_W-1:0] plane_nz_o,
    output var logic signed [31:0]      plane_c_o,
    output var logic [31:0]             population_o,

    // ---- the store's seed (R41/R46): {buffer 0, active_count} ----------------
    output var logic             seed_valid_o,
    input  var logic             seed_ready_i,
    output var logic             seed_buf_o,
    output var logic [CNT_W-1:0] seed_count_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] taken_o,
    output var logic [31:0] refused_normal_o,   // a component outside NRM_W bits
    output var logic [31:0] refused_count_o,    // active_count above CAPACITY
    output var logic [31:0] refused_flags_o,    // a flag bit R41 does not define
    output var logic [31:0] seeds_issued_o
);

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`
  // (CLAUDE.md): a module-scope `if` is a syntax error there.
  initial begin
    if (NRM_W < 2 || NRM_W > 16)
      $fatal(1, "zhao_part_pop: NRM_W=%0d must be between 2 and 16", NRM_W);
    if (CNT_W < $clog2(CAPACITY) + 1)
      $fatal(1, "zhao_part_pop: CNT_W=%0d cannot hold CAPACITY=%0d", CNT_W, CAPACITY);
  end

  // ---- the ratified flag bits ---------------------------------------------
  localparam logic [15:0] FLAG_SEED     = 16'h0001;
  localparam logic [15:0] FLAG_PLANE_EN = 16'h0002;
  localparam logic [15:0] FLAGS_KNOWN   = FLAG_SEED | FLAG_PLANE_EN;

  // ---- the refusals, as named expressions ---------------------------------
  // The bounds are NRM_W's, computed rather than written, so a change to
  // PART.COLLIDE's normal width moves the refusal boundary with it.
  localparam logic signed [31:0] NRM_MIN = -(32'sd1 <<< (NRM_W - 1));
  localparam logic signed [31:0] NRM_MAX =  (32'sd1 <<< (NRM_W - 1)) - 32'sd1;

  function automatic logic normal_fits(input logic signed [15:0] v);
    logic signed [31:0] w;
    begin
      w = 32'(v);
      normal_fits = (w >= NRM_MIN) && (w <= NRM_MAX);
    end
  endfunction

  wire bad_flags_c  = (rec_flags_i & ~FLAGS_KNOWN) != 16'd0;
  wire bad_normal_c = !normal_fits(rec_plane_nx_i) ||
                      !normal_fits(rec_plane_ny_i) ||
                      !normal_fits(rec_plane_nz_i);
  wire bad_count_c  = rec_active_count_i > 32'(CAPACITY);
  // The ORDER is the reference's: flags, then the normal, then the count. A
  // record can breach more than one, and exactly one counter moves, or the
  // totals would not add up to the records refused.
  wire refuse_c     = bad_flags_c || bad_normal_c || bad_count_c;

  assign rec_ready_o = 1'b1;   // a record is consumed the cycle it is offered
  wire   rec_fire_c  = rec_valid_i && rec_ready_o;

  assign seed_buf_o = 1'b0;    // ratified: the first generation is in buffer 0

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      origin_x_o   <= 32'sd0;
      origin_y_o   <= 32'sd0;
      origin_z_o   <= 32'sd0;
      plane_en_o   <= 1'b0;
      plane_nx_o   <= '0;
      plane_ny_o   <= '0;
      plane_nz_o   <= '0;
      plane_c_o    <= 32'sd0;
      population_o <= 32'd0;
      seed_valid_o <= 1'b0;
      seed_count_o <= '0;
      taken_o          <= 32'd0;
      refused_normal_o <= 32'd0;
      refused_count_o  <= 32'd0;
      refused_flags_o  <= 32'd0;
      seeds_issued_o   <= 32'd0;
    end else begin
      if (seed_valid_o && seed_ready_i) begin
        seed_valid_o   <= 1'b0;
        seeds_issued_o <= seeds_issued_o + 32'd1;
      end

      if (rec_fire_c) begin
        if (bad_flags_c) begin
          refused_flags_o <= refused_flags_o + 32'd1;
        end else if (bad_normal_c) begin
          refused_normal_o <= refused_normal_o + 32'd1;
        end else if (bad_count_c) begin
          refused_count_o <= refused_count_o + 32'd1;
        end
        if (!refuse_c) begin
          origin_x_o   <= rec_origin_x_i;
          origin_y_o   <= rec_origin_y_i;
          origin_z_o   <= rec_origin_z_i;
          plane_en_o   <= (rec_flags_i & FLAG_PLANE_EN) != 16'd0;
          plane_nx_o   <= rec_plane_nx_i[NRM_W-1:0];
          plane_ny_o   <= rec_plane_ny_i[NRM_W-1:0];
          plane_nz_o   <= rec_plane_nz_i[NRM_W-1:0];
          plane_c_o    <= rec_plane_c_i;
          population_o <= rec_population_i;
          taken_o      <= taken_o + 32'd1;
          if ((rec_flags_i & FLAG_SEED) != 16'd0) begin
            // A newer seed replaces one the store has not taken yet: the
            // latest committed descriptor is the descriptor.
            seed_valid_o <= 1'b1;
            seed_count_o <= rec_active_count_i[CNT_W-1:0];
          end
        end
      end
    end
  end

endmodule : zhao_part_pop

`default_nettype wire
