// zhao_forge_jobarb.sv -- TWO PRODUCERS, ONE zhao_forge_assemble, arbitrated at
// JOB granularity.
//
// WHY THIS BLOCK EXISTS (SHADOWRIDE, 2026-09-23)
// ---------------------------------------------------------------------------
// `zhao_forge_assemble` is single-job by construction -- `busy_o` holds its
// producer off for the whole primitive -- and its `v_*` port is SPECIFIED as a
// composer mux (`zhao_forge_assemble.sv:199-204`).  FORGE.SHADOW's hulls are a
// second producer on that mux.  Sharing the assembler is what removes a fifth
// client-A demand (the 2-bit owner field has exactly `2'd3` left and the
// instance centre needs it), a fourth `zhao_geom_clipdoor` client, a second
// `zhao_geom_depthquant_stream` and a second 520-slot vertex store.
//
// CLIENT A IS FORGE.PRIM AND IT WINS, ALWAYS.  This is not a shortcut, it is
// `design/contracts/FORGE.SHADOW.md`'s own backpressure rule in one line:
// "It is a background producer: a stalled shadow must never delay a creature."
// A shadow hull is at most 14 triangles, so the reverse wait -- FORGE.PRIM
// held behind a hull already in flight -- is bounded and short, and
// `wait_a_o` measures it rather than arguing it.
//
// THE FAULT THIS BLOCK IS SHAPED TO MAKE IMPOSSIBLE
// -------------------------------------------------
// A metadata swap: a job's VERTICES arriving under another job's MATERIAL.  The
// assembler moves `jset_q` into `mset_q` on the FIRST VERTEX TAKE
// (`zhao_forge_assemble.sv:723`), so whoever's sideband is parked there when a
// vertex lands owns that primitive.  Two producers pushing sidebands into one
// parking register is exactly the "detector wired to two operands that move
// together" shape -- every handshake would balance and the material would be
// wrong.  So:
//
//   1. THE GRANT IS DECIDED BY A VERTEX, NOT BY A SIDEBAND.  A client is
//      granted only when it has a descriptor AND is offering a vertex, so the
//      descriptor and the vertices provably belong to the same producer.
//   2. THE ARBITER PARKS THE SIDEBAND, NOT THE PRODUCERS.  Each client's
//      descriptor lands in its OWN register here and is copied into `jh_q` at
//      the grant.  Nothing downstream ever sees two producers' sidebands.
//   3. THE SIDEBAND GOES IN A CYCLE EARLY.  `A_SIDE` completes the assembler's
//      `j_*` handshake BEFORE `A_RUN` releases the first vertex, because a
//      sideband arriving on the same clock as the first vertex take is a cycle
//      late and the job would run under the PREVIOUS job's material.
//
// WHY EACH CLIENT'S DESCRIPTOR REGISTER IS ALWAYS REPLACEABLE
// -----------------------------------------------------------
// `c_j_ready_o` is constant high, which looks careless and is the opposite.
// `zhao_forge_pagebank` issues a draw's sideband together with its topology and
// position jobs, and an evaluator that REFUSES the job -- wrong family, wrong
// view mask (`zhao_forge_ring_eval.sv:567`, `skipped_view_o`), past a limit --
// takes it and emits NO VERTICES AT ALL.  A descriptor register that could not
// be overwritten would hold that dead job forever and wedge the bank.  The
// assembler already says this about its own parking register in as many words
// ("a VIEW-SKIPPED job produces no vertices at all, so its captured pair is
// never consumed", `:607-612`), and this block obeys the same rule for the same
// reason.  Nothing is lost by the overwrite because a descriptor that has not
// been granted has no vertices attached to it.
//
// AND THE OVERWRITE CANNOT RACE A LIVE JOB: the bank reaches its next issue
// only once its evaluator has emitted the previous job's last vertex, by which
// time the grant is long taken and `jh_q` holds a private copy.  That is the
// assembler's own argument (`:613-616`), inherited rather than re-derived.
`default_nettype none

module zhao_forge_jobarb #(
    parameter int unsigned IDW      = 16,
    parameter int unsigned CENSUS_W = 32
) (
    input var logic clk,
    input var logic rst_n,

    // =======================================================================
    // CLIENT A -- FORGE.PRIM.  Highest priority, unconditionally.
    // =======================================================================
    input  var logic               a_v_valid_i,
    output var logic               a_v_ready_o,
    input  var logic signed [31:0] a_v_x_i,
    input  var logic signed [31:0] a_v_y_i,
    input  var logic signed [31:0] a_v_z_i,
    input  var logic               a_v_last_i,

    input  var logic               a_t_valid_i,
    output var logic               a_t_ready_o,
    input  var logic [15:0]        a_t_i0_i,
    input  var logic [15:0]        a_t_i1_i,
    input  var logic [15:0]        a_t_i2_i,
    input  var logic [15:0]        a_t_material_i,
    input  var logic [IDW-1:0]     a_t_src_id_i,
    input  var logic               a_t_last_i,

    input  var logic               a_j_valid_i,
    output var logic               a_j_ready_o,
    input  var logic [31:0]        a_j_material_set_i,
    input  var logic [15:0]        a_j_material_id_i,
    input  var logic [ 1:0]        a_j_material_mode_i,
    input  var logic [ 7:0]        a_j_vertex_alpha_i,
    input  var logic signed [31:0] a_j_art_r_i,
    input  var logic signed [31:0] a_j_art_g_i,
    input  var logic signed [31:0] a_j_art_b_i,
    input  var logic signed [31:0] a_j_art_alpha_i,
    input  var logic [ 7:0]        a_j_quality_tier_i,
    input  var logic [ 1:0]        a_j_cull_mode_i,

    // =======================================================================
    // CLIENT B -- FORGE.SHADOW, through zhao_forge_fanindex.
    // =======================================================================
    input  var logic               b_v_valid_i,
    output var logic               b_v_ready_o,
    input  var logic signed [31:0] b_v_x_i,
    input  var logic signed [31:0] b_v_y_i,
    input  var logic signed [31:0] b_v_z_i,
    input  var logic               b_v_last_i,

    input  var logic               b_t_valid_i,
    output var logic               b_t_ready_o,
    input  var logic [15:0]        b_t_i0_i,
    input  var logic [15:0]        b_t_i1_i,
    input  var logic [15:0]        b_t_i2_i,
    input  var logic [15:0]        b_t_material_i,
    input  var logic [IDW-1:0]     b_t_src_id_i,
    input  var logic               b_t_last_i,

    input  var logic               b_j_valid_i,
    output var logic               b_j_ready_o,
    input  var logic [31:0]        b_j_material_set_i,
    input  var logic [15:0]        b_j_material_id_i,
    input  var logic [ 1:0]        b_j_material_mode_i,
    input  var logic [ 7:0]        b_j_vertex_alpha_i,
    input  var logic signed [31:0] b_j_art_r_i,
    input  var logic signed [31:0] b_j_art_g_i,
    input  var logic signed [31:0] b_j_art_b_i,
    input  var logic signed [31:0] b_j_art_alpha_i,
    input  var logic [ 7:0]        b_j_quality_tier_i,
    input  var logic [ 1:0]        b_j_cull_mode_i,

    // =======================================================================
    // THE SHARED zhao_forge_assemble
    // =======================================================================
    output var logic               v_valid_o,
    input  var logic               v_ready_i,
    output var logic signed [31:0] v_x_o,
    output var logic signed [31:0] v_y_o,
    output var logic signed [31:0] v_z_o,
    output var logic               v_last_o,

    output var logic               t_valid_o,
    input  var logic               t_ready_i,
    output var logic [15:0]        t_i0_o,
    output var logic [15:0]        t_i1_o,
    output var logic [15:0]        t_i2_o,
    output var logic [15:0]        t_material_o,
    output var logic [IDW-1:0]     t_src_id_o,
    output var logic               t_last_o,

    output var logic               j_valid_o,
    input  var logic               j_ready_i,
    output var logic [31:0]        j_material_set_o,
    output var logic [15:0]        j_material_id_o,

    // The job's HELD art and declaration.  These are read combinationally by
    // the assembler across the whole primitive (`pack_attr`, `o_untex_o`'s
    // neighbours), so they come from `jh_q` and not from a client port.
    output var logic [ 1:0]        j_material_mode_o,
    output var logic [ 7:0]        j_vertex_alpha_o,
    output var logic signed [31:0] art_r_o,
    output var logic signed [31:0] art_g_o,
    output var logic signed [31:0] art_b_o,
    output var logic signed [31:0] art_alpha_o,
    output var logic [ 7:0]        art_quality_tier_o,
    output var logic [ 1:0]        art_cull_mode_o,

    // =======================================================================
    // census
    // =======================================================================
    output var logic [CENSUS_W-1:0] grant_a_o,     // jobs granted to FORGE.PRIM
    output var logic [CENSUS_W-1:0] grant_b_o,     // ...and to FORGE.SHADOW
    output var logic [CENSUS_W-1:0] switches_o,    // grants whose owner changed
    output var logic [CENSUS_W-1:0] wait_a_o,      // clocks A waited on B's job
    output var logic [CENSUS_W-1:0] wait_b_o,      // ...and B on A's
    output var logic [CENSUS_W-1:0] no_desc_o,     // FAULT: a vertex, no descriptor
    output var logic                busy_o
);

  // The descriptor's packed width: {set 32, id 16, mode 2, alpha8 8,
  // r 32, g 32, b 32, art_alpha 32, tier 8, cull 2}.
  localparam int unsigned DESCW = 32 + 16 + 2 + 8 + 32 + 32 + 32 + 32 + 8 + 2;  // 196

  typedef enum logic [1:0] { A_IDLE, A_SIDE, A_RUN } state_e;
  state_e st_q;

  logic          own_q;      // 0 = client A, 1 = client B
  logic          prev_q;     // the previous grant's owner, for `switches_o`
  logic          prev_v_q;   // ... and whether there has been one
  logic [DESCW-1:0] jd_a_q, jd_b_q;
  logic          jv_a_q, jv_b_q;
  logic [DESCW-1:0] jh_q;

  function automatic logic [DESCW-1:0] pack_desc(
      input logic [31:0]        mset,
      input logic [15:0]        mid,
      input logic [ 1:0]        mode,
      input logic [ 7:0]        valpha,
      input logic signed [31:0] r,
      input logic signed [31:0] g,
      input logic signed [31:0] b,
      input logic signed [31:0] aalpha,
      input logic [ 7:0]        tier,
      input logic [ 1:0]        cull);
    pack_desc = {mset, mid, mode, valpha, r, g, b, aalpha, tier, cull};
  endfunction

  // ---- the descriptor registers -------------------------------------------
  // ALWAYS READY.  See the header: a client whose job is refused downstream
  // emits no vertices, and a register that could not be replaced would hold
  // that dead job forever.
  assign a_j_ready_o = 1'b1;
  assign b_j_ready_o = 1'b1;

  // ---- who may be granted --------------------------------------------------
  wire ask_a_c = jv_a_q && a_v_valid_i;
  wire ask_b_c = jv_b_q && b_v_valid_i;
  wire idle_c  = (st_q == A_IDLE);

  // A wins.  The contract's own rule -- a stalled shadow must never delay a
  // creature -- so there is no rotation here and none is wanted.
  wire pick_b_c = !ask_a_c && ask_b_c;
  wire grant_c  = idle_c && (ask_a_c || ask_b_c);

  // ---- routing --------------------------------------------------------------
  wire run_a_c = (st_q == A_RUN) && (own_q == 1'b0);
  wire run_b_c = (st_q == A_RUN) && (own_q == 1'b1);

  assign v_valid_o = run_a_c ? a_v_valid_i : (run_b_c ? b_v_valid_i : 1'b0);
  assign v_x_o     = run_b_c ? b_v_x_i    : a_v_x_i;
  assign v_y_o     = run_b_c ? b_v_y_i    : a_v_y_i;
  assign v_z_o     = run_b_c ? b_v_z_i    : a_v_z_i;
  assign v_last_o  = run_b_c ? b_v_last_i : a_v_last_i;
  assign a_v_ready_o = run_a_c && v_ready_i;
  assign b_v_ready_o = run_b_c && v_ready_i;

  assign t_valid_o    = run_a_c ? a_t_valid_i : (run_b_c ? b_t_valid_i : 1'b0);
  assign t_i0_o       = run_b_c ? b_t_i0_i       : a_t_i0_i;
  assign t_i1_o       = run_b_c ? b_t_i1_i       : a_t_i1_i;
  assign t_i2_o       = run_b_c ? b_t_i2_i       : a_t_i2_i;
  assign t_material_o = run_b_c ? b_t_material_i : a_t_material_i;
  assign t_src_id_o   = run_b_c ? b_t_src_id_i   : a_t_src_id_i;
  assign t_last_o     = run_b_c ? b_t_last_i     : a_t_last_i;
  assign a_t_ready_o  = run_a_c && t_ready_i;
  assign b_t_ready_o  = run_b_c && t_ready_i;

  assign j_valid_o = (st_q == A_SIDE);

  // The held descriptor, unpacked once.  Every reader below takes the same
  // slice of the same register, so no two of them can disagree about a field.
  assign art_cull_mode_o    = jh_q[1:0];
  assign art_quality_tier_o = jh_q[9:2];
  assign art_alpha_o        = $signed(jh_q[41:10]);
  assign art_b_o            = $signed(jh_q[73:42]);
  assign art_g_o            = $signed(jh_q[105:74]);
  assign art_r_o            = $signed(jh_q[137:106]);
  assign j_vertex_alpha_o   = jh_q[145:138];
  assign j_material_mode_o  = jh_q[147:146];
  assign j_material_id_o    = jh_q[163:148];
  assign j_material_set_o   = jh_q[195:164];

  assign busy_o = (st_q != A_IDLE);

  wire t_take_c = t_valid_o && t_ready_i;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_q       <= A_IDLE;
      own_q      <= 1'b0;
      prev_q     <= 1'b0;
      prev_v_q   <= 1'b0;
      jd_a_q     <= '0;
      jd_b_q     <= '0;
      jv_a_q     <= 1'b0;
      jv_b_q     <= 1'b0;
      jh_q       <= '0;
      grant_a_o  <= '0;
      grant_b_o  <= '0;
      switches_o <= '0;
      wait_a_o   <= '0;
      wait_b_o   <= '0;
      no_desc_o  <= '0;
    end else begin
      // ---- descriptor capture, one deep per client, always replaceable ----
      if (a_j_valid_i) begin
        jd_a_q <= pack_desc(a_j_material_set_i, a_j_material_id_i,
                            a_j_material_mode_i, a_j_vertex_alpha_i,
                            a_j_art_r_i, a_j_art_g_i, a_j_art_b_i,
                            a_j_art_alpha_i, a_j_quality_tier_i,
                            a_j_cull_mode_i);
        jv_a_q <= 1'b1;
      end
      if (b_j_valid_i) begin
        jd_b_q <= pack_desc(b_j_material_set_i, b_j_material_id_i,
                            b_j_material_mode_i, b_j_vertex_alpha_i,
                            b_j_art_r_i, b_j_art_g_i, b_j_art_b_i,
                            b_j_art_alpha_i, b_j_quality_tier_i,
                            b_j_cull_mode_i);
        jv_b_q <= 1'b1;
      end

      // ---- a vertex with no descriptor is a PRODUCER-ORDER FAULT ----------
      // It cannot be granted, so it would sit forever.  Counted every clock it
      // is offered, because the length of the stall is the evidence.
      if (idle_c && ((a_v_valid_i && !jv_a_q) || (b_v_valid_i && !jv_b_q))) begin
        if (no_desc_o != {CENSUS_W{1'b1}})
          no_desc_o <= no_desc_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
      end

      // ---- the CROSS-CLIENT wait, and nothing else -------------------------
      // Clocks this client offered a vertex while the OTHER ONE held the
      // arbiter.  Two things are deliberately NOT in it, because a counter
      // that added them could not answer the question it exists for -- does
      // the second producer wait on the FIRST one:
      //   * the two clocks of a client's OWN grant and sideband handshake,
      //     which every job pays and which are not contention;
      //   * the owner's own backpressure from the assembler, which is the
      //     assembler's cost and is already visible in its own census.
      if (a_v_valid_i && (st_q != A_IDLE) && (own_q == 1'b1)) begin
        if (wait_a_o != {CENSUS_W{1'b1}})
          wait_a_o <= wait_a_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
      end
      if (b_v_valid_i && (st_q != A_IDLE) && (own_q == 1'b0)) begin
        if (wait_b_o != {CENSUS_W{1'b1}})
          wait_b_o <= wait_b_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
      end

      unique case (st_q)
        A_IDLE: begin
          if (grant_c) begin
            own_q <= pick_b_c;
            // The PRIVATE COPY.  From here the job's material and art cannot
            // move, whatever the producer does with its port.
            jh_q  <= pick_b_c ? jd_b_q : jd_a_q;
            // A descriptor is spent by its grant.  A producer must present a
            // fresh one per job, so a second job cannot silently inherit the
            // first one's material.
            if (pick_b_c) jv_b_q <= 1'b0;
            else          jv_a_q <= 1'b0;
            prev_q   <= pick_b_c;
            prev_v_q <= 1'b1;
            if (prev_v_q && (prev_q != pick_b_c)) begin
              if (switches_o != {CENSUS_W{1'b1}})
                switches_o <= switches_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
            end
            if (pick_b_c) begin
              if (grant_b_o != {CENSUS_W{1'b1}})
                grant_b_o <= grant_b_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
            end else begin
              if (grant_a_o != {CENSUS_W{1'b1}})
                grant_a_o <= grant_a_o + {{(CENSUS_W-1){1'b0}}, 1'b1};
            end
            st_q <= A_SIDE;
          end
        end

        A_SIDE: begin
          if (j_ready_i) st_q <= A_RUN;
        end

        A_RUN: begin
          // The job ends when its LAST TRIPLE has been taken by the assembler.
          // That is the same event the assembler itself retires on, so the two
          // cannot disagree about where a job ends.
          if (t_take_c && t_last_o) st_q <= A_IDLE;
        end

        default: st_q <= A_IDLE;
      endcase
    end
  end

endmodule : zhao_forge_jobarb

`default_nettype wire
