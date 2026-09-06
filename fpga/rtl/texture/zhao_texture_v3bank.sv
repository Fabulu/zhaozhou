// zhao_texture_v3bank.sv -- ONE synchronous, M10K-backed payload bank.
//
// reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt section 6 ("the
// transaction file") and point C of section 0: persistent payload lives in
// synchronous M10K-backed banks with an explicit port and collision law; only
// scoreboards, queue pointers and bounded pipeline registers stay in fabric.
//
// ---------------------------------------------------------------------------
// WHY THIS IS A MODULE AND NOT AN INLINE ARRAY
// ---------------------------------------------------------------------------
// Every persistent store in the V3 completion experiment has EXACTLY this
// shape: one writer, one reader, clock only, no reset, no logic between the
// array read and the register it lands in. Making that one module means the
// port inventory in the report is a list of instances, and the Quartus RAM
// summary can be read against it name by name instead of by argument.
//
// ---------------------------------------------------------------------------
// THE GOTCHA THIS FILE EXISTS TO NOT REINTRODUCE
// ---------------------------------------------------------------------------
// reports/QUARTUS_GOTCHAS.md section 14, measured 2026-09-04:
//
//   > An M10K has an output register, and inference works by absorbing the
//   > consumer's flop into it. That absorption needs the read to reach a
//   > register with nothing but wiring in between.
//
// `zhao_texture_tmu_pipe` put a decode function and a mux between the array
// and the flop and paid 65,536 FLIP-FLOPS for it. So `rd_data_o <= mem[...]`
// is the whole read path here, and the consumer's selection, comparison and
// arithmetic all happen on the NEXT cycle, out of the way. Section 6.3 calls
// that the RR -> RC boundary and the first production implementation is
// required to keep it explicit.
//
// ---------------------------------------------------------------------------
// WRITE-ENABLE CONTRACT -- the point of the whole experiment
// ---------------------------------------------------------------------------
// `wr_en_i` MUST be driven by a register output and nothing else. Section 0
// point D and section 8.7: the defect being replaced is FRAGROB's
// incoming-token -> owner-table -> predicate -> RAM write-enable cone
// (fpga/rtl/texture/zhao_texture_fragrob.sv:626, `if (tmu_ok_c)`). Every
// instantiation in zhao_texture_v3own connects a one-hot flop to this port.
//
// SystemVerilog cannot assert that a port argument is a register, so this was
// a comment and therefore unenforced. It is now MACHINE-DECIDABLE FROM THE
// SOURCE, by a two-part convention every instantiation obeys:
//
//   1. the `.wr_en_i()` argument is a BARE IDENTIFIER or a single bit-select
//      of one -- never an expression, a reduction, or a wire alias; and
//   2. a `// V3-WREN-REG: <signal>` marker above the instantiation names that
//      identifier.
//
// A gate can then check, without elaborating anything: the named signal is
// declared `logic`, every assignment to it appears inside an `always_ff`, and
// it appears in no `assign` and no `always_comb`. That is exactly the property
// "driven by a register output and nothing else", and it fails loudly the
// moment somebody reconnects a predicate to a memory enable.
//
// This is why the three ready queues in zhao_texture_v3own are instantiated by
// hand instead of in a generate loop: a loop needs `.wr_en_i(alias_c[i])`, the
// argument stops being a register name, and the check becomes undecidable at
// source level for the sake of six saved lines.
//
// ---------------------------------------------------------------------------
// READ-DURING-WRITE LAW (section 6.4) -- OLD DATA, and it is never used
// ---------------------------------------------------------------------------
// Both statements are nonblocking in one clocked process, so a same-address
// read on a write edge returns the OLD row in both simulation and in a
// Cyclone V simple-dual-port M10K. The callers do not depend on that: every
// bank in this experiment publishes its "readable" bit at least one edge AFTER
// the write edge, so a read that could collide is a read whose valid is false.
// Declared, not discovered.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_v3bank #(
    parameter int unsigned WIDTH = 40,
    parameter int unsigned DEPTH = 64,
    // Derived. Overriding this by hand is a way to make the ports disagree
    // with the array, so do not; it is a parameter only because a port width
    // cannot name a localparam.
    parameter int unsigned AW    = $clog2(DEPTH)
) (
    input  var logic             clk,
    input  var logic             wr_en_i,
    input  var logic [AW-1:0]    wr_addr_i,
    input  var logic [WIDTH-1:0] wr_data_i,
    input  var logic [AW-1:0]    rd_addr_i,
    output var logic [WIDTH-1:0] rd_data_o
);

  (* ramstyle = "M10K" *) logic [WIDTH-1:0] mem [DEPTH];

  // Clock only. No reset on the payload or on its output register -- section
  // 6.5: reset clears valid/ownership/pointers, not the payload planes. An
  // array touched by `always_ff @(posedge clk or negedge rst_n)` cannot become
  // an M10K at all.
  always_ff @(posedge clk) begin
    if (wr_en_i) mem[wr_addr_i] <= wr_data_i;
    rd_data_o <= mem[rd_addr_i];
  end

endmodule

`default_nettype wire
