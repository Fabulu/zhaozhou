// zhao_sdram_model.sv — behavioural SDR SDRAM (plan W2.5, D2).
//
// ###############################################################################
// #  TESTBENCH-ONLY. Non-synthesizable. NEVER in lint targets, files.qip     #
// #  or any synthesis file list (spec/memory_rules.md §1, plan D2). This     #
// #  file lives in sim/models/ precisely so it cannot wander into the RTL    #
// #  tree; the CTest lane registers lint for the synthesizable modules only  #
// #  and an explicit exclusion note in fpga/files.qip.                       #
// ###############################################################################
//
// A cycle-true behavioural SDR SDRAM honouring the frozen sim profile in
// fpga/rtl/memory/zhao_sdram_params_pkg.sv (CAS 3, burst 8, tRCD/tRP 3,
// tRC 9, refresh interval law). It decodes the ctrl's PHY pins, checks every
// timing law (sticky per-kind error outputs the C++ tests assert zero), and
// serves read data with exact CAS shaping and write beats with DQM masking.
// The law table it enforces is the one in zhao_sdram_ctrl.sv's header.
//
// Geometry: 4 banks x 8192 rows x 2048 cols x 16 bit = 128 MB. Read bursts
// wrap within the row (sequential, modulo 2048); the controller never issues
// a burst that crosses a row boundary, so the wrap is unreachable in lawful
// traffic and exists only so unlawful traffic is served, not hung.
//
// A peek port (peek_en/peek_waddr/peek_data) exposes memory contents to the
// C++ harness for shadow-memory compares without touching model internals.

module zhao_sdram_model
  import zhao_sdram_params_pkg::*;
(
  input logic clk,

  // PHY pins (driven by zhao_sdram_ctrl)
  input logic        phy_cs_n,
  input logic        phy_ras_n,
  input logic        phy_cas_n,
  input logic        phy_we_n,
  input logic [12:0] phy_a,
  input logic [1:0]  phy_ba,
  input logic [15:0] phy_dq_o,
  input logic        phy_dq_oe,
  input logic [1:0]  phy_dqm,
  output logic [15:0] phy_dq_i,

  // harness peek port (combinational; word address)
  input  logic        peek_en,
  input  logic [25:0] peek_waddr,
  output logic [15:0] peek_data,

  // harness POKE port. The peek side existed so a test could read what the
  // machine wrote; this is the other half, so a test can place asset bytes in
  // memory BEFORE the machine reads them. Without it a fetcher pointed at real
  // memory reads whatever the model was initialised to, and "the picture
  // changed" says nothing about whether the memory path works.
  //
  // Backdoor, not a bus master: it bypasses the SDRAM protocol entirely and is
  // for placing fixtures, never for modelling a write.
  input  logic        poke_en,
  input  logic [25:0] poke_waddr,
  input  logic [15:0] poke_data,

  // sticky error flags (asserted in the C++ tests as zero)
  output logic err_trcd,
  output logic err_trp,
  output logic err_trc,
  output logic err_refresh_interval,
  output logic err_protocol,     // READ/WRITE to a closed bank, ACT on open...
  output logic err_mrs,          // unsupported mode register value
  output logic model_error       // OR of all
);

  // ------------------------------------------------------------------ memory
  localparam int unsigned WORDS = 4 * 8192 * 2048;   // 64 Mi words = 128 MB
  logic [15:0] mem [0:WORDS-1];

  function automatic logic [15:0] word_at(input logic [1:0]  bank,
                                          input logic [12:0] row,
                                          input logic [10:0] col);
    word_at = mem[{bank, row, col}];
  endfunction

  // ------------------------------------------------------------ bank state --
  logic [12:0] open_row [0:3];
  logic [3:0]  open_valid;
  logic [31:0] last_act [0:3];
  logic [31:0] last_pre [0:3];
  logic [3:0]  has_pre;
  logic [31:0] last_ref;
  logic        has_ref;
  logic        mrs_done;
  logic [31:0] cycle;

  // command decode (sampled at posedge; pins are valid during the cycle)
  logic cmd_nop, cmd_act, cmd_read, cmd_write, cmd_pre, cmd_ref, cmd_mrs;
  assign cmd_nop    = !phy_cs_n && phy_ras_n && phy_cas_n && phy_we_n;
  assign cmd_act    = !phy_cs_n && !phy_ras_n &&  phy_cas_n &&  phy_we_n;
  assign cmd_read   = !phy_cs_n &&  phy_ras_n && !phy_cas_n &&  phy_we_n;
  assign cmd_write  = !phy_cs_n &&  phy_ras_n && !phy_cas_n && !phy_we_n;
  assign cmd_pre    = !phy_cs_n && !phy_ras_n &&  phy_cas_n && !phy_we_n;
  assign cmd_ref    = !phy_cs_n && !phy_ras_n && !phy_cas_n &&  phy_we_n;
  assign cmd_mrs    = !phy_cs_n && !phy_ras_n && !phy_cas_n && !phy_we_n;

  // ------------------------------------------------------- read pipeline ----
  // READ sampled at the edge ending cycle R => DQ driven during
  // [R+CAS, R+CAS+7] (law table): beat k is assigned at the edge entering
  // cycle R+CAS+k.
  logic        rd_active;
  logic [2:0]  rd_cas;      // cycles until the first beat (counts 3,2,1..)
  logic [3:0]  rd_beat;
  logic [1:0]  rd_bank;
  logic [12:0] rd_row;
  logic [10:0] rd_col;
  logic [15:0] dq_i_q;
  logic        dq_valid_q;

  assign phy_dq_i = dq_i_q;   // 2-state sim: idle cycles read 0 (the ctrl
                              // samples only inside its rdata window)

  // ------------------------------------------------------- write pipeline ---
  // WRITE sampled at the edge ending cycle R => beats sampled at the edges
  // ending [R+1, R+8]; dqm==2'b11 masks a beat's bytes.
  logic        wr_active;
  logic [3:0]  wr_beat;
  logic [1:0]  wr_bank;
  logic [12:0] wr_row;
  logic [10:0] wr_col;

  assign peek_data = peek_en ? mem[peek_waddr] : 16'd0;

  always_ff @(posedge clk) if (poke_en) mem[poke_waddr] <= poke_data;

  assign model_error = err_trcd | err_trp | err_trc
                     | err_refresh_interval | err_protocol | err_mrs;

  always_ff @(posedge clk) begin
    cycle <= cycle + 32'd1;

    // ---- read data shaping ------------------------------------------------
    dq_valid_q <= 1'b0;
    if (rd_active) begin
      if (rd_cas <= 3'd2) begin
        // driving phase: beat k visible during cycle R+CAS+k
        dq_i_q     <= word_at(rd_bank, rd_row, rd_col + 11'(rd_beat));
        dq_valid_q <= 1'b1;
        if (rd_beat == 4'd7) rd_active <= 1'b0;
        else                 rd_beat   <= rd_beat + 4'd1;
        rd_cas <= 3'd1;
      end else begin
        rd_cas <= rd_cas - 3'd1;   // 3 -> 2 at the first edge after READ
      end
    end

    // ---- write beat capture -----------------------------------------------
    if (wr_active) begin
      if (phy_dqm == 2'b00) begin
        mem[{wr_bank, wr_row, wr_col + 11'(wr_beat)}] <= phy_dq_o;
      end
      if (wr_beat == 4'd7) wr_active <= 1'b0;
      else                 wr_beat   <= wr_beat + 4'd1;
    end

    // ---- command sampling --------------------------------------------------
    if (cmd_mrs) begin
      // mode register: burst length 8 (A[2:0]=011), sequential (A3=0),
      // CAS 3 (A[6:4]=011), standard operating mode (A[8:7]=00)
      if (phy_a[2:0] != 3'b011 || phy_a[3] != 1'b0
          || phy_a[6:4] != 3'b011 || phy_a[8:7] != 2'b00) err_mrs <= 1'b1;
      mrs_done <= 1'b1;
    end else if (cmd_act) begin
      if (open_valid[phy_ba])                                   err_protocol <= 1'b1;
      if (has_pre[phy_ba] && (cycle - last_pre[phy_ba] < T_RP)) err_trp      <= 1'b1;
      open_valid[phy_ba] <= 1'b1;
      open_row[phy_ba]   <= phy_a[12:0];
      last_act[phy_ba]   <= cycle;
    end else if (cmd_pre) begin
      if (phy_a[10]) begin           // precharge all
        for (int b = 0; b < 4; b++) begin
          open_valid[b] <= 1'b0;
          last_pre[b]   <= cycle;
          has_pre[b]    <= 1'b1;
        end
      end else begin
        open_valid[phy_ba] <= 1'b0;
        last_pre[phy_ba]   <= cycle;
        has_pre[phy_ba]    <= 1'b1;
      end
    end else if (cmd_ref) begin
      if (open_valid != 4'b0) err_protocol <= 1'b1;
      if (has_ref && (cycle - last_ref < T_RC)) err_trc <= 1'b1;
      // refresh accounting law: no two AUTO_REFRESH commands may be more
      // than INTERVAL + deferral bound apart (memory_rules 1 + ctrl law)
      if (has_ref && (cycle - last_ref > 32'd865)) err_refresh_interval <= 1'b1;
      last_ref <= cycle;
      has_ref  <= 1'b1;
    end else if (cmd_read) begin
      if (!open_valid[phy_ba])                                  err_protocol <= 1'b1;
      if (cycle - last_act[phy_ba] < T_RCD)                     err_trcd     <= 1'b1;
      if (wr_active)                                            err_protocol <= 1'b1;
      rd_active <= 1'b1;
      rd_cas    <= CAS_LATENCY[2:0];
      rd_beat   <= 4'd0;
      rd_bank   <= phy_ba;
      rd_row    <= open_row[phy_ba];
      rd_col    <= phy_a[10:0];
    end else if (cmd_write) begin
      if (!open_valid[phy_ba])              err_protocol <= 1'b1;
      if (cycle - last_act[phy_ba] < T_RCD) err_trcd     <= 1'b1;
      if (rd_active)                        err_protocol <= 1'b1;
      wr_active <= 1'b1;
      wr_beat   <= 4'd0;
      wr_bank   <= phy_ba;
      wr_row    <= open_row[phy_ba];
      wr_col    <= phy_a[10:0];
    end
  end

  // power-on state: Verilator zero-initializes; the ctrl's init sequence
  // (PRECHARGE-ALL + 2x AUTO_REFRESH + MRS) establishes everything the
  // checks rely on before any client traffic (law table, zhao_sdram_ctrl).

endmodule
