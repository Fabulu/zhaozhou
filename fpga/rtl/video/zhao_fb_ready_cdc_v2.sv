// zhao_fb_ready_cdc_v2.sv -- Packet-G generation-bearing READY/swap CDC.
//
// Two independent depth-four asynchronous FIFOs carry one frozen 84-bit tuple:
//   {writer, slot, generation[15:0], mode[1:0], base[31:0], span[31:0]}
// READY travels gpu->vid and the accepted swap echo travels vid->gpu. Either
// reset asynchronously clears both pointer domains; synchronized reset-done
// levels then hold both channels closed until both clocks have restarted.
//
// AUTHORITY: reports/SHELL-TEXTURE-V3-COMPOSITION-ARCHITECTURE-20260913.md
//            section 12.9 and Packet G.
`default_nettype none

`ifndef ZHAO_FB_CDC_FULL
`define ZHAO_FB_CDC_FULL(full) (full)
`endif
`ifndef ZHAO_FB_CDC_BARRIER
`define ZHAO_FB_CDC_BARRIER(done) (done)
`endif
`ifndef ZHAO_FB_CDC_READY_TUPLE
`define ZHAO_FB_CDC_READY_TUPLE(tuple) (tuple)
`endif
`ifndef ZHAO_FB_CDC_RD_NEXT
`define ZHAO_FB_CDC_RD_NEXT(next_value) (next_value)
`endif

module zhao_fb_ready_cdc_v2 (
    input  logic        gpu_clk,
    input  logic        gpu_rst_n,
    input  logic        vid_clk,
    input  logic        vid_rst_n,

    input  logic        gpu_ready_valid_i,
    output logic        gpu_ready_ready_o,
    input  logic [83:0] gpu_ready_tuple_i,
    output logic        vid_ready_valid_o,
    input  logic        vid_ready_ready_i,
    output logic [83:0] vid_ready_tuple_o,

    input  logic        vid_swap_valid_i,
    output logic        vid_swap_ready_o,
    input  logic [83:0] vid_swap_tuple_i,
    output logic        gpu_swap_valid_o,
    input  logic        gpu_swap_ready_i,
    output logic [83:0] gpu_swap_tuple_o,

    output logic        gpu_barrier_done_o,
    output logic        vid_barrier_done_o,
    output logic        gpu_protocol_fault_o,
    output logic        vid_protocol_fault_o,
    output logic [31:0] ready_enqueued_o,
    output logic [31:0] ready_dequeued_o,
    output logic [31:0] swap_enqueued_o,
    output logic [31:0] swap_dequeued_o,
    output logic [2:0]  ready_memory_level_o,
    output logic [2:0]  swap_memory_level_o,
    output logic        gpu_idle_o,
    output logic        vid_idle_o
);

  localparam int unsigned DATA_W = 84;
  localparam int unsigned ADDR_W = 2;
  localparam int unsigned PTR_W = ADDR_W + 1;

  initial begin : p_packet_g_cdc_selector_contract
`ifdef ZHAO_FB_CDC_MUTANT_COLLISION
    $fatal(1, "ZHAO_FB_READY_CDC_V2_MUTANT_SELECTOR_COLLISION");
`endif
  end
`ifdef ZHAO_FB_CDC_MUTANT_COLLISION
  ZHAO_FB_READY_CDC_V2_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE
      u_packet_g_cdc_selector_collision();
`endif

  wire pair_rst_n = gpu_rst_n && vid_rst_n;

  // Assert both domains immediately when either reset asserts. Deassertion is
  // synchronized independently so no data/pointer flop is released near its
  // own active clock edge. Verilator's vector-level SYNCASYNCNET warning sees
  // the synchronizer shift and its intentional async-reset fanout as mixed use.
  /* verilator lint_off SYNCASYNCNET */
  (* ASYNC_REG = "TRUE" *) logic [2:0] gpu_reset_release_q;
  (* ASYNC_REG = "TRUE" *) logic [2:0] vid_reset_release_q;
  wire gpu_local_rst_n = gpu_reset_release_q[2];
  wire vid_local_rst_n = vid_reset_release_q[2];

  always_ff @(posedge gpu_clk or negedge pair_rst_n) begin
    if (!pair_rst_n)
      gpu_reset_release_q <= 3'b000;
    else
      gpu_reset_release_q <= {gpu_reset_release_q[1:0], 1'b1};
  end

  always_ff @(posedge vid_clk or negedge pair_rst_n) begin
    if (!pair_rst_n)
      vid_reset_release_q <= 3'b000;
    else
      vid_reset_release_q <= {vid_reset_release_q[1:0], 1'b1};
  end
  /* verilator lint_on SYNCASYNCNET */

  function automatic logic [PTR_W-1:0] bin_to_gray(
      input logic [PTR_W-1:0] value);
    bin_to_gray = (value >> 1) ^ value;
  endfunction

  function automatic logic [PTR_W-1:0] gray_to_bin(
      input logic [PTR_W-1:0] value);
    logic [PTR_W-1:0] result;
    begin
      result[PTR_W-1] = value[PTR_W-1];
      for (int bit_index = PTR_W-2; bit_index >= 0; bit_index--)
        result[bit_index] = result[bit_index+1] ^ value[bit_index];
      gray_to_bin = result;
    end
  endfunction

  // ---------------------------------------------------------------- reset --
  logic [2:0] gpu_up_q, vid_up_q;
  logic vid_up_gpu_m1_q, vid_up_gpu_m2_q, vid_up_gpu_m3_q;
  logic gpu_up_vid_m1_q, gpu_up_vid_m2_q, gpu_up_vid_m3_q;

  always_ff @(posedge gpu_clk or negedge gpu_local_rst_n) begin
    if (!gpu_local_rst_n) begin
      gpu_up_q <= 3'b000;
      vid_up_gpu_m1_q <= 1'b0;
      vid_up_gpu_m2_q <= 1'b0;
      vid_up_gpu_m3_q <= 1'b0;
    end else begin
      gpu_up_q <= {gpu_up_q[1:0], 1'b1};
      vid_up_gpu_m1_q <= vid_up_q[2];
      vid_up_gpu_m2_q <= vid_up_gpu_m1_q;
      vid_up_gpu_m3_q <= vid_up_gpu_m2_q;
    end
  end

  always_ff @(posedge vid_clk or negedge vid_local_rst_n) begin
    if (!vid_local_rst_n) begin
      vid_up_q <= 3'b000;
      gpu_up_vid_m1_q <= 1'b0;
      gpu_up_vid_m2_q <= 1'b0;
      gpu_up_vid_m3_q <= 1'b0;
    end else begin
      vid_up_q <= {vid_up_q[1:0], 1'b1};
      gpu_up_vid_m1_q <= gpu_up_q[2];
      gpu_up_vid_m2_q <= gpu_up_vid_m1_q;
      gpu_up_vid_m3_q <= gpu_up_vid_m2_q;
    end
  end

  assign gpu_barrier_done_o = gpu_up_q[2] && vid_up_gpu_m3_q;
  assign vid_barrier_done_o = vid_up_q[2] && gpu_up_vid_m3_q;

  // ---------------------------------------------------------- READY gpu->vid --
  logic [PTR_W-1:0] ready_wr_bin_q, ready_wr_gray_q;
  logic [PTR_W-1:0] ready_rd_bin_q, ready_rd_gray_q;
  logic [PTR_W-1:0] ready_rd_gpu_m1_q, ready_rd_gpu_m2_q, ready_rd_gpu_m3_q;
  logic [PTR_W-1:0] ready_wr_vid_m1_q, ready_wr_vid_m2_q, ready_wr_vid_m3_q;
  logic [PTR_W-1:0] ready_wr_gray_increment_c;
  logic ready_full_c, ready_empty_c, ready_write_c, ready_read_issue_c;
  logic ready_read_pending_q;
  logic [DATA_W-1:0] ready_ram_data_w, ready_output_q;
  logic ready_output_valid_q;

  assign ready_wr_gray_increment_c = bin_to_gray(ready_wr_bin_q + PTR_W'(1));
  // One tuple may be held beyond RAM. Limiting RAM occupancy to three keeps
  // total channel ownership at the declared depth of four.
  assign ready_full_c = ready_wr_gray_increment_c ==
      {~ready_rd_gpu_m3_q[PTR_W-1:PTR_W-2],
        ready_rd_gpu_m3_q[PTR_W-3:0]};
  assign gpu_ready_ready_o = `ZHAO_FB_CDC_BARRIER(gpu_barrier_done_o) &&
      !`ZHAO_FB_CDC_FULL(ready_full_c);
  assign ready_write_c = gpu_ready_valid_i && gpu_ready_ready_o;

  always_ff @(posedge gpu_clk or negedge gpu_local_rst_n) begin
    if (!gpu_local_rst_n) begin
      ready_wr_bin_q <= '0;
      ready_wr_gray_q <= '0;
      ready_rd_gpu_m1_q <= '0;
      ready_rd_gpu_m2_q <= '0;
      ready_rd_gpu_m3_q <= '0;
      ready_enqueued_o <= 32'd0;
    end else begin
      ready_rd_gpu_m1_q <= ready_rd_gray_q;
      ready_rd_gpu_m2_q <= ready_rd_gpu_m1_q;
      ready_rd_gpu_m3_q <= ready_rd_gpu_m2_q;
      if (ready_write_c) begin
        ready_wr_bin_q <= ready_wr_bin_q + PTR_W'(1);
        ready_wr_gray_q <= ready_wr_gray_increment_c;
        ready_enqueued_o <= ready_enqueued_o + 32'd1;
      end
    end
  end

  assign ready_empty_c = ready_rd_gray_q == ready_wr_vid_m3_q;
  assign ready_read_issue_c = `ZHAO_FB_CDC_BARRIER(vid_barrier_done_o) &&
      !ready_empty_c && !ready_read_pending_q &&
      (!ready_output_valid_q || vid_ready_ready_i);
  assign vid_ready_valid_o = ready_output_valid_q;
  assign vid_ready_tuple_o = `ZHAO_FB_CDC_READY_TUPLE(ready_output_q);

  always_ff @(posedge vid_clk or negedge vid_local_rst_n) begin
    if (!vid_local_rst_n) begin
      ready_rd_bin_q <= '0;
      ready_rd_gray_q <= '0;
      ready_wr_vid_m1_q <= '0;
      ready_wr_vid_m2_q <= '0;
      ready_wr_vid_m3_q <= '0;
      ready_read_pending_q <= 1'b0;
      ready_output_valid_q <= 1'b0;
      ready_output_q <= '0;
      ready_dequeued_o <= 32'd0;
    end else begin
      ready_wr_vid_m1_q <= ready_wr_gray_q;
      ready_wr_vid_m2_q <= ready_wr_vid_m1_q;
      ready_wr_vid_m3_q <= ready_wr_vid_m2_q;
      if (ready_output_valid_q && vid_ready_ready_i) begin
        ready_output_valid_q <= 1'b0;
        ready_dequeued_o <= ready_dequeued_o + 32'd1;
      end
      if (ready_read_pending_q) begin
        ready_read_pending_q <= 1'b0;
        ready_output_valid_q <= 1'b1;
        ready_output_q <= ready_ram_data_w;
        ready_rd_bin_q <= `ZHAO_FB_CDC_RD_NEXT(
            ready_rd_bin_q + PTR_W'(1));
        ready_rd_gray_q <= bin_to_gray(`ZHAO_FB_CDC_RD_NEXT(
            ready_rd_bin_q + PTR_W'(1)));
      end
      if (ready_read_issue_c)
        ready_read_pending_q <= 1'b1;
    end
  end

  zhao_dc_sdp_ram #(.DATA_W(DATA_W), .ADDR_W(ADDR_W)) u_ready_ram (
      .wr_clk(gpu_clk), .wr_en(ready_write_c),
      .wr_addr(ready_wr_bin_q[ADDR_W-1:0]), .wr_data(gpu_ready_tuple_i),
      .rd_clk(vid_clk), .rd_en(ready_read_issue_c),
      .rd_addr(ready_rd_bin_q[ADDR_W-1:0]), .rd_data(ready_ram_data_w));

  // ------------------------------------------------------------ SWAP vid->gpu --
  logic [PTR_W-1:0] swap_wr_bin_q, swap_wr_gray_q;
  logic [PTR_W-1:0] swap_rd_bin_q, swap_rd_gray_q;
  logic [PTR_W-1:0] swap_rd_vid_m1_q, swap_rd_vid_m2_q, swap_rd_vid_m3_q;
  logic [PTR_W-1:0] swap_wr_gpu_m1_q, swap_wr_gpu_m2_q, swap_wr_gpu_m3_q;
  logic [PTR_W-1:0] swap_wr_gray_increment_c;
  logic swap_full_c, swap_empty_c, swap_write_c, swap_read_issue_c;
  logic swap_read_pending_q;
  logic [DATA_W-1:0] swap_ram_data_w, swap_output_q;
  logic swap_output_valid_q;

  assign swap_wr_gray_increment_c = bin_to_gray(swap_wr_bin_q + PTR_W'(1));
  assign swap_full_c = swap_wr_gray_increment_c ==
      {~swap_rd_vid_m3_q[PTR_W-1:PTR_W-2],
        swap_rd_vid_m3_q[PTR_W-3:0]};
  assign vid_swap_ready_o = `ZHAO_FB_CDC_BARRIER(vid_barrier_done_o) &&
      !`ZHAO_FB_CDC_FULL(swap_full_c);
  assign swap_write_c = vid_swap_valid_i && vid_swap_ready_o;

  always_ff @(posedge vid_clk or negedge vid_local_rst_n) begin
    if (!vid_local_rst_n) begin
      swap_wr_bin_q <= '0;
      swap_wr_gray_q <= '0;
      swap_rd_vid_m1_q <= '0;
      swap_rd_vid_m2_q <= '0;
      swap_rd_vid_m3_q <= '0;
      swap_enqueued_o <= 32'd0;
    end else begin
      swap_rd_vid_m1_q <= swap_rd_gray_q;
      swap_rd_vid_m2_q <= swap_rd_vid_m1_q;
      swap_rd_vid_m3_q <= swap_rd_vid_m2_q;
      if (swap_write_c) begin
        swap_wr_bin_q <= swap_wr_bin_q + PTR_W'(1);
        swap_wr_gray_q <= swap_wr_gray_increment_c;
        swap_enqueued_o <= swap_enqueued_o + 32'd1;
      end
    end
  end

  assign swap_empty_c = swap_rd_gray_q == swap_wr_gpu_m3_q;
  assign swap_read_issue_c = `ZHAO_FB_CDC_BARRIER(gpu_barrier_done_o) &&
      !swap_empty_c && !swap_read_pending_q &&
      (!swap_output_valid_q || gpu_swap_ready_i);
  assign gpu_swap_valid_o = swap_output_valid_q;
  assign gpu_swap_tuple_o = swap_output_q;

  always_ff @(posedge gpu_clk or negedge gpu_local_rst_n) begin
    if (!gpu_local_rst_n) begin
      swap_rd_bin_q <= '0;
      swap_rd_gray_q <= '0;
      swap_wr_gpu_m1_q <= '0;
      swap_wr_gpu_m2_q <= '0;
      swap_wr_gpu_m3_q <= '0;
      swap_read_pending_q <= 1'b0;
      swap_output_valid_q <= 1'b0;
      swap_output_q <= '0;
      swap_dequeued_o <= 32'd0;
    end else begin
      swap_wr_gpu_m1_q <= swap_wr_gray_q;
      swap_wr_gpu_m2_q <= swap_wr_gpu_m1_q;
      swap_wr_gpu_m3_q <= swap_wr_gpu_m2_q;
      if (swap_output_valid_q && gpu_swap_ready_i) begin
        swap_output_valid_q <= 1'b0;
        swap_dequeued_o <= swap_dequeued_o + 32'd1;
      end
      if (swap_read_pending_q) begin
        swap_read_pending_q <= 1'b0;
        swap_output_valid_q <= 1'b1;
        swap_output_q <= swap_ram_data_w;
        swap_rd_bin_q <= `ZHAO_FB_CDC_RD_NEXT(
            swap_rd_bin_q + PTR_W'(1));
        swap_rd_gray_q <= bin_to_gray(`ZHAO_FB_CDC_RD_NEXT(
            swap_rd_bin_q + PTR_W'(1)));
      end
      if (swap_read_issue_c)
        swap_read_pending_q <= 1'b1;
    end
  end

  zhao_dc_sdp_ram #(.DATA_W(DATA_W), .ADDR_W(ADDR_W)) u_swap_ram (
      .wr_clk(vid_clk), .wr_en(swap_write_c),
      .wr_addr(swap_wr_bin_q[ADDR_W-1:0]), .wr_data(vid_swap_tuple_i),
      .rd_clk(gpu_clk), .rd_en(swap_read_issue_c),
      .rd_addr(swap_rd_bin_q[ADDR_W-1:0]), .rd_data(swap_ram_data_w));

  // ----------------------------------------------------------- observations --
  assign ready_memory_level_o = ready_wr_bin_q - gray_to_bin(ready_rd_gpu_m3_q);
  assign swap_memory_level_o = swap_wr_bin_q - gray_to_bin(swap_rd_vid_m3_q);
  assign gpu_idle_o = gpu_barrier_done_o && !gpu_ready_valid_i &&
      (ready_memory_level_o == 3'd0) && swap_empty_c &&
      !swap_read_pending_q && !swap_output_valid_q;
  assign vid_idle_o = vid_barrier_done_o && !vid_swap_valid_i &&
      (swap_memory_level_o == 3'd0) && ready_empty_c &&
      !ready_read_pending_q && !ready_output_valid_q;

  logic gpu_stalled_q, vid_stalled_q;
  logic [83:0] gpu_stalled_tuple_q, vid_stalled_tuple_q;
  always_ff @(posedge gpu_clk or negedge gpu_local_rst_n) begin
    if (!gpu_local_rst_n) begin
      gpu_stalled_q <= 1'b0;
      gpu_stalled_tuple_q <= '0;
      gpu_protocol_fault_o <= 1'b0;
    end else begin
      if (gpu_stalled_q &&
          (!gpu_ready_valid_i || (gpu_ready_tuple_i != gpu_stalled_tuple_q)))
        gpu_protocol_fault_o <= 1'b1;
      gpu_stalled_q <= gpu_ready_valid_i && !gpu_ready_ready_o;
      if (gpu_ready_valid_i && !gpu_ready_ready_o && !gpu_stalled_q)
        gpu_stalled_tuple_q <= gpu_ready_tuple_i;
    end
  end

  always_ff @(posedge vid_clk or negedge vid_local_rst_n) begin
    if (!vid_local_rst_n) begin
      vid_stalled_q <= 1'b0;
      vid_stalled_tuple_q <= '0;
      vid_protocol_fault_o <= 1'b0;
    end else begin
      if (vid_stalled_q &&
          (!vid_swap_valid_i || (vid_swap_tuple_i != vid_stalled_tuple_q)))
        vid_protocol_fault_o <= 1'b1;
      vid_stalled_q <= vid_swap_valid_i && !vid_swap_ready_o;
      if (vid_swap_valid_i && !vid_swap_ready_o && !vid_stalled_q)
        vid_stalled_tuple_q <= vid_swap_tuple_i;
    end
  end

`ifndef SYNTHESIS
`ifndef QUARTUS_SYNTHESIS
  always_ff @(posedge gpu_clk) begin
    if (gpu_barrier_done_o) begin
      if ($past(gpu_barrier_done_o && gpu_swap_valid_o &&
                !gpu_swap_ready_i))
        assert (gpu_swap_valid_o && $stable(gpu_swap_tuple_o))
          else $error("fb_ready_cdc_v2: held GPU swap tuple changed");
      assert (ready_memory_level_o <= 3'd3)
        else $error("fb_ready_cdc_v2: READY FIFO RAM ownership exceeded three");
    end
  end
  always_ff @(posedge vid_clk) begin
    if (vid_barrier_done_o) begin
      if ($past(vid_barrier_done_o && vid_ready_valid_o &&
                !vid_ready_ready_i))
        assert (vid_ready_valid_o && $stable(vid_ready_tuple_o))
          else $error("fb_ready_cdc_v2: held VID ready tuple changed");
      assert (swap_memory_level_o <= 3'd3)
        else $error("fb_ready_cdc_v2: swap FIFO RAM ownership exceeded three");
    end
  end
`endif
`endif

endmodule : zhao_fb_ready_cdc_v2

`undef ZHAO_FB_CDC_FULL
`undef ZHAO_FB_CDC_BARRIER
`undef ZHAO_FB_CDC_READY_TUPLE
`undef ZHAO_FB_CDC_RD_NEXT
`ifdef ZHAO_FB_CDC_MUTANT_COLLISION
  `undef ZHAO_FB_CDC_MUTANT_COLLISION
`endif
`default_nettype wire
