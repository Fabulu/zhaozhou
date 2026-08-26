// zhao_field_rf_ram -- one register-file memory: one write port, one read port.
//
// WHY THIS IS ITS OWN MODULE, AND WHY IT IS THIS SHAPE.
//
// The FIELD v2 register file was originally one array, `rf[LANES][0:511]`,
// read on four ports and written from four places inside the core's main
// always_ff. quartus_map measured that at 121,292 ALMs against a 41,910-ALM
// device and the fit errored 96 minutes into placement.
//
// Reducing it to four read replicas sharing one write port -- still declared
// as `rf_a[LANES][0:511]` inside the same always_ff -- halved the
// combinational logic (133,338 -> 66,292 ALUTs, the read muxes going away) and
// changed the storage NOT AT ALL: 75,835 registers, 4,369 block memory bits
// (the sine ROM, and nothing else), zero RAM-conversion warnings. Quartus
// still did not consider it storage.
//
// Two things were still wrong with the shape, and neither is about port count:
//
//   1. `[LANES][0:511]` is a TWO-DIMENSIONAL unpacked array. A memory is one
//      dimension. The lane index has to become separate instances, not an
//      outer dimension.
//   2. The reads and writes lived in the core's main always_ff alongside the
//      whole issue/retire machine. Inference wants the memory process to be
//      the memory and nothing else.
//
// So the storage is a module, instantiated once per (lane, reader), and its
// body is the textbook form and deliberately nothing more.
//
// READ-DURING-WRITE IS OLD DATA. `rdata_o` is assigned from `mem` with a
// non-blocking assignment in the same block as the write, so a read of the
// address being written returns the value from before the write. That is
// exactly what the array it replaces did, so no behaviour moves. It is also
// the read-during-write mode M10K implements natively -- asking for new data
// would cost bypass logic.
//
// THERE IS NO RESET. A reset over every cell is not expressible as a memory
// reset, and asking for one is how storage silently becomes flip-flops --
// zhao_vertex_arena.sv paid for that lesson in a formal proof. The file does
// not need one: a wavefront's registers are written by the host fill before
// the program that reads them is started.
//
// THERE IS NO READ ENABLE either. The core previously updated its read
// registers only while `rst_n` was high; here the read runs every clock. That
// is unobservable -- no wavefront is valid during reset, so nothing consumes a
// read taken then -- and an unconditional read is the most inferable form.
module zhao_field_rf_ram #(
    parameter int AW = 9,           // {wavefront, register}
    parameter int DW = 32
) (
    input  logic                   clk,

    input  logic                   we_i,
    input  logic [AW-1:0]          waddr_i,
    input  logic signed [DW-1:0]   wdata_i,

    input  logic [AW-1:0]          raddr_i,
    output logic signed [DW-1:0]   rdata_o
);

  logic signed [DW-1:0] mem [0:(1<<AW)-1];

  always_ff @(posedge clk) begin
    if (we_i) mem[waddr_i] <= wdata_i;
    rdata_o <= mem[raddr_i];
  end

endmodule
