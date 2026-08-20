// zhao_dc_sdp_ram.sv — dual-clock simple dual-port RAM, written to INFER.
//
// WHY THIS EXISTS. This project has now produced the same defect five times:
// an array that was meant to become block RAM, described in a way that
// prevents inference, and nobody noticed because SIMULATION CANNOT SEE
// INFERENCE. TEXTURE.CACHE (5,402 ALMs of flip-flops), CMD.DMA (1.97 Mbit,
// which made the composed shell unsynthesizable), FORGE.CLIFF, this line
// buffer, and an ambiguous read-during-write in AUDIO.FIFO.
//
// So the shape lives in ONE place, written once, correctly:
//
//   - the array is NEVER initialised in synthesizable RTL
//   - the array is NEVER touched in an asynchronous-reset branch
//     (an M10K has no reset port; a reset on the data is what forces flops)
//   - the read happens ONLY inside the read-clock process, so the read is
//     REGISTERED (an asynchronous array read is the single most common cause
//     of a failed inference, and is what Quartus named for both CMD.DMA and
//     the scanout line buffer)
//   - `rd_data` is meaningful only for a cycle following `rd_en`
//
// READ LATENCY IS EXACTLY ONE `rd_clk` EDGE. That is not an implementation
// detail to be hidden; it is the contract. A consumer absorbs it by issuing
// the address one cycle ahead, never by adding a pipeline stage to whatever
// it feeds.
//
// READ-DURING-WRITE AT THE SAME ADDRESS IS OUTSIDE THE PROTOCOL. Across two
// clocks the result is genuinely undefined in silicon and will not match
// simulation; Quartus warns about exactly this for AUDIO.FIFO. A user of this
// module must make same-address collision UNREACHABLE by ownership, and say
// how in its contract. This module does not arbitrate.
module zhao_dc_sdp_ram #(
    parameter int unsigned DATA_W = 64,
    parameter int unsigned ADDR_W = 8
) (
    // write port, write clock
    input  logic              wr_clk,
    input  logic              wr_en,
    input  logic [ADDR_W-1:0] wr_addr,
    input  logic [DATA_W-1:0] wr_data,

    // read port, read clock. One-cycle registered read.
    input  logic              rd_clk,
    input  logic              rd_en,
    input  logic [ADDR_W-1:0] rd_addr,
    output logic [DATA_W-1:0] rd_data
);

  // No reset, no initial value, no procedural init outside a formal harness.
  // The owner's validity state decides whether a location's contents matter.
  logic [DATA_W-1:0] mem [0:(1 << ADDR_W)-1];

  always_ff @(posedge wr_clk) begin
    if (wr_en) mem[wr_addr] <= wr_data;
  end

  always_ff @(posedge rd_clk) begin
    if (rd_en) rd_data <= mem[rd_addr];
  end

endmodule : zhao_dc_sdp_ram
