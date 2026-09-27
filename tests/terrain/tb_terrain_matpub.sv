// tb_terrain_matpub.sv -- the flat-port harness for TERRAIN.COMPOSED_MATERIAL's
// publisher.
//
// WHY A WRAPPER EXISTS AT ALL. `zhao_terrain_matpub` talks to MEM.GUARD through
// `zhao_guard_req_t` / `zhao_guard_rsp_t`, which are PACKED STRUCTS. Verilator
// flattens a 103-bit packed struct into a wide word, so a C++ driver would have
// to slice the fields by hand-counted bit offsets -- and a hand-counted offset
// that is wrong by one produces a test that drives a plausible address and
// checks nothing. The fields are broken out HERE, where the struct's own member
// names do the slicing and the compiler checks them.
//
// ===========================================================================
// WHAT THE BEHAVIOURAL GUARD MODELS, AND WHAT IT DELIBERATELY DOES NOT
// ===========================================================================
// It models the HANDSHAKE the real guard presents, and the one property of it
// that has cost this tree time: the answer takes TWO cycles -- `ready` is a
// LEVEL and `ok` is PULSED the cycle after the accept. A model that answered in
// one would let a DUT that tests both in the same arm pass here and fail in the
// console, which is the wrong way round for a bench to be wrong.
//
// It does NOT model arbitration, credits, refresh or bank conflicts. Those are
// MEM.VRAM.ARBITER's and they are measured where they live; a second, simpler
// copy of them here would be a rival implementation of a ratified law, which is
// the failure this repository keeps paying for.
//
// `g_deny_i` FORCES A VIOLATION on the next request. It is not a fault
// injection for its own sake: `guard_denied_o` is a counter the DUT exports and
// asserting it zero is a CLAIM, so the bench owes a way to make it move.
//
// ===========================================================================
// THE MEMORY WINDOW IS FOUR SLOTS, AND THAT IS A CHOICE
// ===========================================================================
// The region is 2 MiB and modelling all of it would be 262,144 words of
// simulation memory to check four. Four slots is enough for every question this
// bench asks -- "did it write the slot it was told to", "did it leave the
// neighbour alone", "did the stranger arm rewrite the right one" -- and a write
// outside the window is COUNTED (`oob_writes_o`) rather than silently dropped,
// so the bench cannot mistake a mis-addressed write for a missing one.

module tb_terrain_matpub
  import zhao_pkg::*;
#(
    parameter int unsigned WIN_SLOTS = 4
) (
    input  var logic        clk,
    input  var logic        rst_n,

    input  var logic        cfg_enable_i,
    input  var logic [2:0]  cfg_vram_client_i,

    // ---- the tap ----------------------------------------------------------
    input  var logic        c_we_i,
    input  var logic [4:0]  c_ci_i,
    input  var logic [4:0]  c_cj_i,
    input  var logic [7:0]  c_mat_a_i,
    input  var logic [7:0]  c_mat_b_i,
    input  var logic [7:0]  c_weight_i,

    // ---- the fill's boundaries and identity --------------------------------
    input  var logic        fill_start_i,
    input  var logic        commit_i,
    input  var logic [15:0] patch_id_i,
    input  var logic [7:0]  slot_i,

    // ---- the bench's guard controls ----------------------------------------
    input  var logic        g_deny_i,

    // ---- the memory probe ---------------------------------------------------
    input  var logic [31:0] probe_word_i,   // 64-bit word index into the window
    output var logic [63:0] probe_data_o,

    // ---- the DUT's censuses -------------------------------------------------
    output var logic [31:0] cells_captured_o,
    output var logic [31:0] commits_o,
    output var logic [31:0] patches_published_o,
    output var logic [31:0] skipped_clean_o,
    output var logic [31:0] stranger_pub_o,
    output var logic [31:0] bursts_written_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] cell_oob_o,
    output var logic [31:0] short_fill_o,
    output var logic [31:0] commit_busy_o,
    output var logic        busy_o,

    // ---- what the BENCH saw, which is the independent half -----------------
    output var logic [31:0] greqs_seen_o,
    output var logic [31:0] wbeats_seen_o,
    output var logic [31:0] oob_writes_o,
    output var logic [31:0] last_req_addr_o,
    output var logic [6:0]  last_req_len_o
);

  localparam logic [31:0] RBASE      = ZHAO_TERRAIN_COMPOSED_MATERIAL_BASE;
  localparam logic [31:0] RSPAN      = ZHAO_TERRAIN_COMPOSED_MATERIAL_SPAN;
  localparam int unsigned SLOT_BYTES = 8192;
  localparam int unsigned WIN_BYTES  = WIN_SLOTS * SLOT_BYTES;
  localparam int unsigned WIN_WORDS  = WIN_BYTES / 8;
  localparam int unsigned WW         = $clog2(WIN_WORDS);

  logic [63:0] win_mem [WIN_WORDS];

  // ------------------------------------------------------ the DUT ----------
  zhao_guard_req_t guard_req;
  zhao_guard_rsp_t guard_rsp;
  logic [63:0]     wdata;
  logic            wvalid;
  logic            wready;
  logic            wlast;

  zhao_terrain_matpub #(
      .REGION_BASE(RBASE),
      .REGION_SPAN(RSPAN),
      .SLOTS      (256),
      .SLOT_BYTES (SLOT_BYTES)
  ) u_dut (
      .clk  (clk),
      .rst_n(rst_n),

      .cfg_enable_i     (cfg_enable_i),
      .cfg_vram_client_i(zhao_client_e'(cfg_vram_client_i)),

      .c_we_i    (c_we_i),
      .c_ci_i    (c_ci_i),
      .c_cj_i    (c_cj_i),
      .c_mat_a_i (c_mat_a_i),
      .c_mat_b_i (c_mat_b_i),
      .c_weight_i(c_weight_i),

      .fill_start_i(fill_start_i),
      .commit_i    (commit_i),
      .patch_id_i  (patch_id_i),
      .slot_i      (slot_i),

      .guard_req_o   (guard_req),
      .guard_rsp_i   (guard_rsp),
      .guard_wdata_o (wdata),
      .guard_wvalid_o(wvalid),
      .guard_wready_i(wready),
      .guard_wlast_o (wlast),

      .cells_captured_o   (cells_captured_o),
      .commits_o          (commits_o),
      .patches_published_o(patches_published_o),
      .skipped_clean_o    (skipped_clean_o),
      .stranger_pub_o     (stranger_pub_o),
      .bursts_written_o   (bursts_written_o),
      .guard_denied_o     (guard_denied_o),
      .cell_oob_o         (cell_oob_o),
      .short_fill_o       (short_fill_o),
      .commit_busy_o      (commit_busy_o),
      .busy_o             (busy_o)
  );

  // ================================================================= guard ==
  // `g_busy_q` is the write's lifetime: the guard is not ready while it is
  // still taking beats for the request it already granted.
  logic        g_busy_q;      // a granted write is streaming
  logic        g_pend_q;      // a request was accepted; the verdict is due
  logic        g_ok_q;
  logic        g_viol_q;
  logic [31:0] g_addr_q;
  logic [3:0]  g_beat_q;

  wire [31:0] req_off_c   = guard_req.addr - RBASE[ZHAO_VRAM_ADDR_BITS-1:0];
  wire        req_inreg_c = ({5'd0, guard_req.addr} >= RBASE) &&
                            (req_off_c < RSPAN);

  assign guard_rsp.ready     = !g_busy_q && !g_pend_q;
  assign guard_rsp.ok        = g_ok_q;
  assign guard_rsp.violation = g_viol_q;
  assign wready              = g_busy_q;

  wire        wbeat_take_c = wvalid && wready;
  wire [31:0] w_off_c      = (g_addr_q - RBASE) + {28'd0, g_beat_q} * 32'd8;
  wire [31:0] w_word_c     = w_off_c >> 3;
  wire        w_inwin_c    = (w_word_c < 32'(WIN_WORDS));

  assign probe_data_o = win_mem[probe_word_i[WW-1:0]];

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      g_busy_q        <= 1'b0;
      g_pend_q        <= 1'b0;
      g_ok_q          <= 1'b0;
      g_viol_q        <= 1'b0;
      g_addr_q        <= 32'd0;
      g_beat_q        <= 4'd0;
      greqs_seen_o    <= 32'd0;
      wbeats_seen_o   <= 32'd0;
      oob_writes_o    <= 32'd0;
      last_req_addr_o <= 32'd0;
      last_req_len_o  <= 7'd0;
    end else begin
      // THE VERDICT IS A PULSE, ONE CYCLE AFTER THE ACCEPT.  This is the half
      // the real guard has and a one-cycle model would not.
      g_ok_q   <= 1'b0;
      g_viol_q <= 1'b0;

      if (guard_rsp.ready && guard_req.valid) begin
        greqs_seen_o    <= greqs_seen_o + 32'd1;
        last_req_addr_o <= {5'd0, guard_req.addr};
        last_req_len_o  <= guard_req.len;
        g_addr_q        <= {5'd0, guard_req.addr};
        g_pend_q        <= 1'b1;
        if (g_deny_i || !req_inreg_c) g_viol_q <= 1'b1;
        else                          g_ok_q   <= 1'b1;
      end

      if (g_pend_q) begin
        g_pend_q <= 1'b0;
        if (g_ok_q) begin
          g_busy_q <= 1'b1;
          g_beat_q <= 4'd0;
        end
      end

      if (wbeat_take_c) begin
        wbeats_seen_o <= wbeats_seen_o + 32'd1;
        if (w_inwin_c) win_mem[w_word_c[WW-1:0]] <= wdata;
        else           oob_writes_o <= oob_writes_o + 32'd1;
        g_beat_q <= g_beat_q + 4'd1;
        if (wlast) begin
          g_busy_q <= 1'b0;
          g_beat_q <= 4'd0;
        end
      end
    end
  end

endmodule
