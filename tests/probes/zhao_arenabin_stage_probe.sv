// zhao_arenabin_stage_probe -- the ARENAINFER packet's one-change-at-a-time
// instrument for `zhao_geom_arenabin`'s staging banks.
//
// WHY THIS FILE EXISTS
// --------------------
// ARENACOMPOSE put `zhao_geom_arenabin` through `quartus_map` on the shipping
// part 5CSEBA6U23I7 and measured **146,414 registers against 33,408 block
// memory bits**. Only the five MODULE-SCOPE directory arrays inferred; the
// fourteen 576 x 18 staging banks -- 145,152 bits, declared INSIDE A GENERATE
// -- appear in no RAM Summary at all and went entirely to flip-flops. Three
// rows (`@arenacompose`, `@ramstyle`, `@ramstyle-literal`) are BYTE-IDENTICAL,
// so `(* ramstyle *)` is inert here and reasoning about this block's inference
// without a row is worthless.
//
// The shape is the puzzle, because it passes every killer QUARTUS_GOTCHAS
// section 10 established on purpose with a 102-bench grid:
//
//   * the read is SYNCHRONOUS          (`if (stg_re) rd_q <= bank[stg_ra];`)
//   * NOTHING resets the array         (the process has no reset at all)
//   * there are NO byte enables        (the element is written whole)
//   * a SHARED read/write process with a read enable is measured innocent
//     (`calib_ram_8192x8_shared_re`, 65,536 bits, 23 ALM)
//
// and the five directory arrays that DO infer sit in an `always_ff @(posedge
// clk or negedge rst_n)` with a 40-line reset branch, which is the DIRTIER
// description of the two. So whatever stops the staging banks is a killer this
// repository has not yet written down, and a causal answer needs ONE VARIABLE
// MOVED PER MEASUREMENT.
//
// Mapping the real block costs ~1,000 s per row and drags a whole FSM, a
// `zhao_raster_fill` triple and 6 DSPs through every experiment. This probe is
// the staging banks and their ports and nothing else.
//
// This file is a PROBE, not production, and nothing composes it. It is
// committed for the reason the ground-contact rule gives: "a probe that does
// this was written once and thrown away, so its numbers are unreproducible --
// commit the probe."
//
// HOW TO USE IT
// -------------
//   tools\quartus\arenabin_stage_probe.ps1 -Label '@stage-v0' -TopParameters VARIANT=0
//
// READ VARIANT 0 FIRST, ALWAYS. It is the POSITIVE CONTROL: the production
// description copied verbatim, and it must reproduce the defect (0 block
// memory bits for the banks). A probe whose control arm infers M10K is not
// measuring the same machine as the real block and every number taken from its
// other arms is about something else.
//
// BANKS is a knob so the control can be established cheaply. STAGE_IDS=1 is
// one bank of 576 x 18 = 10,368 bits; STAGE_IDS=14 is the shipped shape. Run
// the control at BOTH before trusting a one-bank sweep: if one bank infers and
// fourteen do not, the replication is the finding and the arms below are about
// the wrong thing.
//
// THE ARMS. Each differs from ARM 0 in EXACTLY ONE PROPERTY.
//   0  production, verbatim: banks declared inside a generate-for, write and
//      read in ONE always_ff, write enable gated by a compare against the
//      genvar, and THE READ AND WRITE ADDRESSES ARE THE SAME NET (production
//      drives both `stg_wa` and `stg_ra` from `tile_r`).
//   1  the READ ADDRESS becomes an independent net. Everything else unchanged.
//      A memory whose read and write addresses are one net is a SINGLE-PORT
//      RAM whose described read-during-write returns the OLD word; Cyclone V
//      M10K single-port read-during-write is new-data, so this is the first
//      candidate for a killer the grid never covered -- every `sync` point in
//      it has separate `raddr_i` and `waddr_i` ports.
//   2  the bank is declared at MODULE SCOPE rather than inside the generate.
//      Defined only at STAGE_IDS=1, because a module-scope declaration cannot
//      be replicated by a genvar -- which is the whole point of the arm and is
//      also what makes the repair, should this be the cause, expensive.
//   3  the write enable drops the compare against the genvar (`if (stg_we)`).
//   4  the read moves into its OWN always_ff. Write form and addresses
//      unchanged.
//   5  `(* ramstyle = "no_rw_check" *)` on the declaration. ARENACOMPOSE tried
//      `"M10K"`, which is a PLACEMENT request and says nothing about
//      read-during-write; `no_rw_check` is the one that removes the bypass
//      obligation. Measured rather than assumed, and if it changes nothing it
//      is not shipped -- an inert pragma reads as a guarantee.
//   6  arms 1 and 5 together -- only meaningful once each is measured alone.
//   7  THE PROPOSED REPAIR: the bank becomes an instance of the committed
//      `zhao_dc_sdp_ram`, so the array sits at a MODULE's scope while the
//      INSTANCE stays inside the generate-for and STAGE_IDS stays a knob.
//      Its depth is `1 << ADDR_W`, so 576 becomes 1024 -- which is a second
//      variable and is stated rather than hidden. It costs nothing in blocks:
//      a Cyclone V M10K is 512 x 18 at this width, so 576 and 1024 both take
//      two.
//   8  the bank is declared inside a generate-IF with NO for-loop. Arms 0..6
//      are inside BOTH an if and a for; arm 2 is inside neither. This is the
//      arm that says which of the two scopes is the killer, and it changes
//      nothing else.
//
// WHAT THE FIRST SEVEN ROWS SAID, so the next reader does not re-run them
// (5CSEBA6U23I7, map_only, STAGE_IDS=1, one bank of 576 x 18 = 10,368 bits):
//
//   v0 production            10,386 reg        0 bits   <- the control, FIRED
//   v1 split read address    10,386 reg        0 bits
//   v2 MODULE SCOPE               0 reg   10,368 bits   <- ALTSYNCRAM SDP
//   v3 plain write enable    10,386 reg        0 bits
//   v4 split process         10,386 reg        0 bits
//   v5 no_rw_check           10,386 reg        0 bits
//   v6 v1 + v5               10,386 reg        0 bits
//
// So the read-during-write story, the shared process, the genvar compare and
// every attribute are ALL INNOCENT, and only the declaration's SCOPE moves the
// number. `no_rw_check` is as inert as `"M10K"` was, which is worth recording
// because it is the attribute that would have been tried next.
//
// Quartus 17.0 syntax law, both halves: explicit `generate`/`endgenerate` (an
// implicit generate is a syntax error there) and elaboration guards inside
// `initial begin ... end` (a module-scope `if` is a syntax error there, and
// `--lint-only` never runs the block, so a clean lint catches neither).
`default_nettype none

module zhao_arenabin_stage_probe #(
    // `zhao_geom_arenabin`'s own names and defaults.
    parameter int unsigned TILES     = 576,
    parameter int unsigned TIDX_W    = 10,
    parameter int unsigned STAGE_IDS = 14,
    parameter int unsigned ID_W      = 18,
    // Derived, but a parameter and not a localparam because the ports use it.
    parameter int unsigned STG_W     = $clog2(STAGE_IDS + 1),
    parameter int unsigned VARIANT   = 0
) (
    input  var logic                    clk,
    input  var logic                    rst_n,

    // the staging write port, A_PUSH's append
    input  var logic                    stg_we,
    input  var logic [STG_W-1:0]        stg_wsel,
    input  var logic [TIDX_W-1:0]       stg_wa,
    input  var logic [ID_W-1:0]         stg_wd,

    // the staging read port, A_EMITR's row read. PRODUCTION DRIVES THIS FROM
    // THE SAME REGISTER AS `stg_wa` (`tile_r`), so arms 0 and 2..5 read at
    // `stg_wa` and this port is used only by the arms that separate them.
    input  var logic                    stg_re,
    /* verilator lint_off UNUSEDSIGNAL */
    // Read only by arms 1 and 6; every other arm reads at `stg_wa`, which is
    // what production does. The port is unconditional so one elaboration of
    // this file covers every arm.
    input  var logic [TIDX_W-1:0]       stg_ra_i,
    /* verilator lint_on UNUSEDSIGNAL */

    // the chunk record's count, which selects real ids from ID_NULL
    input  var logic [STG_W-1:0]        cnt_i,

    output var logic [STAGE_IDS*32-1:0] ck_ids_o
);

  localparam logic [31:0] ID_NULL = 32'hFFFF_FFFF;

  // Quartus 17.0 needs an elaboration guard inside `initial begin ... end`.
  initial begin
    if (VARIANT > 8)
      $fatal(1, "zhao_arenabin_stage_probe: VARIANT is 0..8");
    if ((VARIANT == 2) && (STAGE_IDS != 1))
      $fatal(1, "zhao_arenabin_stage_probe: arm 2 is defined at STAGE_IDS=1 only");
    if ((VARIANT == 8) && (STAGE_IDS != 1))
      $fatal(1, "zhao_arenabin_stage_probe: arm 8 is defined at STAGE_IDS=1 only");
    if ((VARIANT == 7) && ((32'd1 << TIDX_W) < TILES))
      $fatal(1, "zhao_arenabin_stage_probe: arm 7's bank is 1<<TIDX_W deep and must cover TILES");
    if (ID_W > 32)
      $fatal(1, "zhao_arenabin_stage_probe: ID_W must fit a u32 chunk slot");
  end

  // ---------------------------------------------------------------- arm 2 --
  // THE MODULE-SCOPE BANK, and why it is declared unconditionally.
  //
  // The property under test IS the declaration's scope, so the declaration
  // cannot itself sit inside the `generate if (VARIANT == 2)` that selects the
  // arm -- that would make every arm a generate-scope arm and the experiment
  // would measure nothing. It is therefore declared at module scope always,
  // and its write enable carries the arm selection. `VARIANT` is a parameter,
  // so in every other arm `ms_we_c` folds to constant zero, the array is never
  // written, and Quartus removes it whole: the arm-0 row is not polluted by it
  // and its own summary says so (registers and memory bits both unchanged
  // against a build with this block deleted).
  wire ms_sel_c = (VARIANT == 2);
  wire ms_we_c  = stg_we && ms_sel_c && (stg_wsel == STG_W'(0));
  wire ms_re_c  = stg_re && ms_sel_c;

  logic [ID_W-1:0] bank_ms [0:TILES-1];
  /* verilator lint_off UNUSEDSIGNAL */
  // Read only by arm 2. In every other arm `ms_we_c` folds to zero and this
  // whole block is removed by synthesis; see the paragraph above.
  logic [ID_W-1:0] rd_ms_q;
  /* verilator lint_on UNUSEDSIGNAL */

  always_ff @(posedge clk) begin
    if (ms_we_c) bank_ms[stg_wa] <= stg_wd;
    if (ms_re_c) rd_ms_q <= bank_ms[stg_wa];
  end

  genvar gs;

  generate
    // ------------------------------------------------------------- arm 0 --
    // PRODUCTION, VERBATIM. zhao_geom_arenabin.sv:543-556, with `stg_ra`
    // replaced by `stg_wa` because production ties both to `tile_r`.
    if (VARIANT == 0) begin : g_v0_production
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        logic [ID_W-1:0] bank [0:TILES-1];
        logic [ID_W-1:0] rd_q;
        always_ff @(posedge clk) begin
          if (stg_we && (stg_wsel == STG_W'(gs))) bank[stg_wa] <= stg_wd;
          if (stg_re) rd_q <= bank[stg_wa];
        end
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end

    // ------------------------------------------------------------- arm 1 --
    // ONE CHANGE: the read address is its own net.
    end else if (VARIANT == 1) begin : g_v1_split_addr
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        logic [ID_W-1:0] bank [0:TILES-1];
        logic [ID_W-1:0] rd_q;
        always_ff @(posedge clk) begin
          if (stg_we && (stg_wsel == STG_W'(gs))) bank[stg_wa] <= stg_wd;
          if (stg_re) rd_q <= bank[stg_ra_i];
        end
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end

    // ------------------------------------------------------------- arm 2 --
    // ONE CHANGE: the bank is declared at module scope (above).
    end else if (VARIANT == 2) begin : g_v2_module_scope
      assign ck_ids_o[0 +: 32] = (STG_W'(0) < cnt_i)
                               ? {{(32-ID_W){1'b0}}, rd_ms_q}
                               : ID_NULL;

    // ------------------------------------------------------------- arm 3 --
    // ONE CHANGE: the write enable drops the genvar compare.
    end else if (VARIANT == 3) begin : g_v3_plain_we
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        logic [ID_W-1:0] bank [0:TILES-1];
        logic [ID_W-1:0] rd_q;
        always_ff @(posedge clk) begin
          if (stg_we) bank[stg_wa] <= stg_wd;
          if (stg_re) rd_q <= bank[stg_wa];
        end
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end

    // ------------------------------------------------------------- arm 4 --
    // ONE CHANGE: the read has its own always_ff.
    end else if (VARIANT == 4) begin : g_v4_split_proc
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        logic [ID_W-1:0] bank [0:TILES-1];
        logic [ID_W-1:0] rd_q;
        always_ff @(posedge clk) begin
          if (stg_we && (stg_wsel == STG_W'(gs))) bank[stg_wa] <= stg_wd;
        end
        always_ff @(posedge clk) begin
          if (stg_re) rd_q <= bank[stg_wa];
        end
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end

    // ------------------------------------------------------------- arm 5 --
    // ONE CHANGE: `no_rw_check` on the declaration.
    end else if (VARIANT == 5) begin : g_v5_norwcheck
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        (* ramstyle = "no_rw_check" *) logic [ID_W-1:0] bank [0:TILES-1];
        logic [ID_W-1:0] rd_q;
        always_ff @(posedge clk) begin
          if (stg_we && (stg_wsel == STG_W'(gs))) bank[stg_wa] <= stg_wd;
          if (stg_re) rd_q <= bank[stg_wa];
        end
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end

    // ------------------------------------------------------------- arm 7 --
    // THE PROPOSED REPAIR. The array moves to a MODULE's scope by becoming an
    // instance of the committed `zhao_dc_sdp_ram` -- the file that exists
    // precisely because this project has produced this defect five times --
    // while the INSTANCE stays inside the generate-for, so STAGE_IDS remains a
    // knob and `ck_ids_o`'s slot-per-bank structure is untouched.
    //
    // Both clocks are `clk`. Same-address read-during-write is outside that
    // module's protocol and is UNREACHABLE HERE BY OWNERSHIP: production
    // asserts `stg_we` only in A_PUSH and `stg_re` only in A_EMITR, which are
    // different states of one FSM, so the two enables cannot be high together.
    end else if (VARIANT == 7) begin : g_v7_sdp_submodule
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        logic [ID_W-1:0] rd_q;
        zhao_dc_sdp_ram #(.DATA_W(ID_W), .ADDR_W(TIDX_W)) u_bank (
          .wr_clk (clk),
          .wr_en  (stg_we && (stg_wsel == STG_W'(gs))),
          .wr_addr(stg_wa),
          .wr_data(stg_wd),
          .rd_clk (clk),
          .rd_en  (stg_re),
          .rd_addr(stg_wa),
          .rd_data(rd_q)
        );
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end

    // ------------------------------------------------------------- arm 8 --
    // ONE CHANGE from arm 0: the generate-FOR is gone. The declaration is
    // still inside a generate-IF -- this very arm -- so a difference between
    // this row and arm 0 is the for-loop, and a difference between this row
    // and arm 2 is the generate scope itself.
    end else if (VARIANT == 8) begin : g_v8_generate_if_only
      logic [ID_W-1:0] bank [0:TILES-1];
      logic [ID_W-1:0] rd_q;
      always_ff @(posedge clk) begin
        if (stg_we && (stg_wsel == STG_W'(0))) bank[stg_wa] <= stg_wd;
        if (stg_re) rd_q <= bank[stg_wa];
      end
      assign ck_ids_o[0 +: 32] = (STG_W'(0) < cnt_i)
                               ? {{(32-ID_W){1'b0}}, rd_q}
                               : ID_NULL;

    // ------------------------------------------------------------- arm 6 --
    // Arms 1 and 5 TOGETHER. Present so an interaction can be read rather
    // than assumed, and meaningless before each is measured alone.
    end else begin : g_v6_split_addr_norwcheck
      for (gs = 0; gs < int'(STAGE_IDS); gs = gs + 1) begin : g_stage
        (* ramstyle = "no_rw_check" *) logic [ID_W-1:0] bank [0:TILES-1];
        logic [ID_W-1:0] rd_q;
        always_ff @(posedge clk) begin
          if (stg_we && (stg_wsel == STG_W'(gs))) bank[stg_wa] <= stg_wd;
          if (stg_re) rd_q <= bank[stg_ra_i];
        end
        assign ck_ids_o[gs*32 +: 32] = (STG_W'(gs) < cnt_i)
                                     ? {{(32-ID_W){1'b0}}, rd_q}
                                     : ID_NULL;
      end
    end
  endgenerate

  // `rst_n` is a port so the probe's boundary matches the block's; nothing in
  // any arm resets an array, which is deliberate and is the point.
  wire unused_c = rst_n;

endmodule

`default_nettype wire
