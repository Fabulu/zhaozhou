// zhao_skid2.sv — a 2-deep skid buffer that BREAKS A COMBINATIONAL READY PATH.
//
// WHY THIS EXISTS, measured rather than assumed.
//
// The composed fit at a9aeb07 put 90 of the worst 100 gpu_clk setup paths on:
//
//   zhao_raster_tilestore | ram_block1a0~PORT_B_WRITE_ENABLE_REG
//     -> rd_ready_i
//     -> s0_to_s1     = !s1_hold && s0_v_r && rd_ready_i     [FRAGMENT]
//     -> frag_ready_o = !s0_v_r || s0_to_s1                  [FRAGMENT]
//     -> ez_cand_ready
//     -> out_free -> frag_acc -> hiz_qualify                 [EARLY-Z]
//     -> all 256 bits of acc_mask_next
//     -> acc_mask_r[*]
//
// A downstream READY, originating in a RAM write-enable register two blocks
// away, was reaching 256 register inputs inside one clock. Note what this is
// NOT: `&acc_mask_next`, the 256-input AND that reports/MHZArchitected names as
// the Early-Z offender, is on the OUTPUT side of those registers and appears
// nowhere on these paths. Rewriting the reduction would have cost the effort
// and moved nothing.
//
// A skid buffer is the textbook cut. Downstream ready stops propagating
// upstream combinationally: `up_ready_o` becomes a function of THIS block's own
// occupancy registers.
//
// WHY TWO DEEP, and not one. A single register slice must deassert
// `up_ready_o` whenever it holds a beat, so it accepts at best every other
// cycle -- it would halve the initiation rate. The second slot absorbs the beat
// already in flight when the consumer stalls, which is what lets ready stay
// high continuously. The architecture rule is exact about this:
//
//   "latency may grow; initiation rate and exact arithmetic may not regress"
//
// Latency grows by one cycle. Initiation rate stays at one beat per clock.
// The payload is carried opaquely and is bit-identical on the way out.
//
// HANDSHAKE HYGIENE, the same rule the rest of the tree follows: `up_ready_o`
// never depends on `up_valid_i`, and `dn_valid_o` never depends on
// `dn_ready_i`. Either dependence creates a combinational loop the moment two
// of these meet back to back.
//
// Conservative SystemVerilog subset only (charter §2).
// Lint: clean under `verilator_bin --lint-only -Wall`.
module zhao_skid2 #(
  parameter int unsigned W = 8   // payload width, carried opaquely
) (
  input  logic         clk,
  input  logic         rst_n,

  input  logic         up_valid_i,
  output logic         up_ready_o,
  input  logic [W-1:0] up_data_i,

  output logic         dn_valid_o,
  input  logic         dn_ready_i,
  output logic [W-1:0] dn_data_o,

  // Occupancy, for assertions and counters. 0, 1 or 2.
  output logic [1:0]   level_o
);

  // Two slots in a tiny ring. `count` is the authority on occupancy; the
  // pointers only choose which slot is touched.
  logic [W-1:0] mem [0:1];
  logic         wptr, rptr;
  logic [1:0]   count;

  logic push, pop;

  // Ready is a function of occupancy ONLY -- never of up_valid_i, and never
  // of dn_ready_i. This is the whole point of the block.
  assign up_ready_o = (count != 2'd2);

  // Valid is a function of occupancy ONLY -- never of dn_ready_i.
  assign dn_valid_o = (count != 2'd0);

  assign push = up_valid_i && up_ready_o;
  assign pop  = dn_valid_o && dn_ready_i;

  assign dn_data_o = mem[rptr];
  assign level_o   = count;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wptr    <= 1'b0;
      rptr    <= 1'b0;
      count   <= 2'd0;
      mem[0]  <= {W{1'b0}};
      mem[1]  <= {W{1'b0}};
    end else begin
      if (push) begin
        mem[wptr] <= up_data_i;
        wptr      <= ~wptr;
      end
      if (pop) begin
        rptr <= ~rptr;
      end
      // A simultaneous push and pop leaves occupancy unchanged, which is the
      // steady state at full rate -- and the case a naive `count + push - pop`
      // written as two separate ifs would get wrong.
      case ({push, pop})
        2'b10:   count <= count + 2'd1;
        2'b01:   count <= count - 2'd1;
        default: count <= count;
      endcase
    end
  end

endmodule
