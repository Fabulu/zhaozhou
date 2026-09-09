// zhao_probe_ram_infer -- WHY DOES e_tag INFER AND e_num_u NOT?
//
// ---------------------------------------------------------------------------
// THE QUESTION THIS EXISTS TO SETTLE
// ---------------------------------------------------------------------------
// In zhao_texture_island_v3_top@g2-prod, zhao_raster_perspuv_svc holds a
// 16-entry token table that is 85% of its 3,240 registers -- the island's
// largest single register consumer and 4.63x its S3.3 budget line. Exactly ONE
// of its arrays became an M10K:
//
//     e_tag    16 x 14, Simple Dual Port    INFERRED
//     e_num_u  16 x 32, one write, one read NOT inferred
//     e_mant_u 16 x 24                      NOT inferred (and 384 of its
//                                           registers were MERGED back into it
//                                           from e_mant_v, undoing a split)
//
// The 2026-09-06 per-axis split was built on the theory that READ-ADDRESS COUNT
// is what decides. `e_num_u` has one read address and one write address after
// that split and still did not infer, so that theory is refuted and the actual
// discriminator is unknown. `tools/quartus/check_ram_inference.py` flags every
// array in the block as async-reset-written, but it flags `e_tag` too -- and
// `e_tag` inferred -- so that signal cannot discriminate either. Its own header
// says the signal has measured false positives.
//
// Guessing was already tried today and cost two corrections. This probe MEASURES
// instead, and it is a probe rather than an edit to perspuv because production
// RTL must not be mutated to answer a question (CLAUDE.md, on committed
// mutants), and because a probe leaves the answer behind for the next reader.
//
// ---------------------------------------------------------------------------
// IT IS A MAP QUESTION, NOT A FIT QUESTION
// ---------------------------------------------------------------------------
// RAM inference is decided by quartus_map and reported in its RAM Summary.
// run_block_map.ps1 answers this in the seconds-to-minutes class instead of the
// 1.5-4 hours a fit costs, and already parses inferred depth/width. So the
// register-inference class behind the island's biggest breach never needed the
// scarce resource. What map CANNOT say is what any change costs in placed ALMs
// or Fmax -- that still needs a fit, afterwards, and only if this says a change
// is worth making.
//
// ---------------------------------------------------------------------------
// THE DESIGN: five variants, ONE geometry, one factor changed at a time
// ---------------------------------------------------------------------------
// All five are 16 deep with a single write address and a single read address,
// so the only differences are the three candidate factors:
//
//   variant  width  read style                 array cleared on reset?
//   -------  -----  -------------------------  -----------------------
//     A        14   continuous assign @ reg     YES   <- the e_tag shape
//     B        32   continuous assign @ reg     YES   <- A, but WIDE
//     C        32   inside always_ff @ comb     YES   <- the e_num_u shape
//     D        32   inside always_ff @ comb     NO    <- C, minus the reset
//     E        14   inside always_ff @ comb     YES   <- A's width, C's read
//
// Reading the RAM Summary then answers directly:
//
//   A infers, B does not          -> WIDTH decides
//   A infers, E does not          -> READ STYLE decides
//   C does not, D does            -> THE ARRAY RESET decides
//   all five infer                -> the blocker is something perspuv does that
//                                    this probe does not reproduce, and the
//                                    next step is to add its work queue
//
// A NEGATIVE CONTROL IS BUILT IN: variant A reproduces the one array that IS
// known to infer. If A does not appear in the RAM Summary, the probe does not
// reproduce the island's conditions and NONE of its other rows mean anything.
// That check comes first; without it a table of "did not infer" rows would look
// like five findings instead of one broken instrument.
//
// EVERY READ REACHES A PORT. An array whose output nothing observes is deleted
// before inference runs, which would report "did not infer" for a reason that
// has nothing to do with the question.

`default_nettype none

module zhao_probe_ram_infer #(
    parameter int unsigned DEPTH = 16,
    parameter int unsigned NARROW = 14,
    parameter int unsigned WIDE = 32,
    // AW is derived, but it must be a PARAMETER and not a localparam because
    // the port list below uses it. A localparam in the module body is declared
    // after the ports and cannot reach them.
    parameter int unsigned AW = $clog2(DEPTH)
) (
    input  wire                     clk,
    input  wire                     rst_n,

    // one write port, shared address, so every variant writes identically
    input  wire                     wr_en,
    input  wire [AW-1:0]            wr_addr,
    input  wire [WIDE-1:0]          wr_data,

    // registered read address, for the continuous-assign variants
    input  wire                     rd_adv,
    // combinational read address, for the always_ff variants
    input  wire [AW-1:0]            rd_addr_c,
    input  wire                     rd_en,

    output wire [NARROW-1:0]        a_o,
    output wire [WIDE-1:0]          b_o,
    output logic [WIDE-1:0]         c_o,
    output logic [WIDE-1:0]         d_o,
    output logic [NARROW-1:0]       e_o
);

  // The registered read pointer, mirroring perspuv's head_q.
  logic [AW-1:0] rptr_q;

  // ---- the five arrays ----------------------------------------------------
  logic [NARROW-1:0] arr_a [DEPTH];
  logic [WIDE-1:0]   arr_b [DEPTH];
  logic [WIDE-1:0]   arr_c [DEPTH];
  logic [WIDE-1:0]   arr_d [DEPTH];
  logic [NARROW-1:0] arr_e [DEPTH];

  // ---- A: narrow, continuous-assign read at a REGISTERED index ------------
  // This is e_tag's exact shape and it is the probe's positive control.
  assign a_o = arr_a[rptr_q];

  // ---- B: A, widened. Isolates WIDTH. -------------------------------------
  assign b_o = arr_b[rptr_q];

  // ---- C/D/E read inside always_ff at a COMBINATIONAL index ---------------
  // e_num_u's shape: the address arrives as logic, the result lands in a flop.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      c_o <= '0;
      d_o <= '0;
      e_o <= '0;
    end else if (rd_en) begin
      c_o <= arr_c[rd_addr_c];
      d_o <= arr_d[rd_addr_c];
      e_o <= arr_e[rd_addr_c];
    end
  end

  // ---- THE WRITES MUST DIFFER, OR THE VARIANTS MERGE INTO ONE ARRAY -------
  //
  // MEASURED BY v1 OF THIS PROBE, 2026-09-09. Written identically, four of the
  // five arrays were collapsed:
  //
  //     arr_b[i][b]  Merged with  arr_a[i][b]     224 registers
  //     arr_c[i][b]  Merged with  arr_a[i][b]     224 registers
  //     arr_e[i][b]  Merged with  arr_a[i][b]     224 registers
  //     arr_d                     not merged        0
  //
  // The merged array then carried the UNION of their read sites -- two
  // continuous assigns plus two always_ff reads, four read addresses -- so it
  // could not be a dual-port memory whatever its width, read style or reset.
  // Only arr_d stayed separate, and only because having no reset made it
  // provably different from the others. So v1 confounded "no reset" with "not
  // merged" and could not attribute its own result.
  //
  // THAT IS THE SAME MECHANISM AS perspuv's e_mant, reproduced minimally: 384
  // registers of a deliberate per-axis split were merged back because both
  // copies were written from one source on one clock. v1's flaw is independent
  // corroboration of that finding, which is why it is recorded here rather than
  // quietly fixed.
  //
  // The repair is to make each variant's stored value provably distinct. A
  // per-variant XOR constant is the cheapest thing that cannot be proved equal,
  // costs no ports, and leaves depth, width and addressing untouched.
  localparam logic [WIDE-1:0] SALT_A = 32'h0000_0001;
  localparam logic [WIDE-1:0] SALT_B = 32'h0000_0002;
  localparam logic [WIDE-1:0] SALT_C = 32'h0000_0004;
  localparam logic [WIDE-1:0] SALT_D = 32'h0000_0008;
  localparam logic [WIDE-1:0] SALT_E = 32'h0000_0010;

  // ---- the write side ----------------------------------------------------
  // A, B, C and E have their arrays CLEARED by the async reset, exactly as
  // perspuv's do. D deliberately does not -- that is the whole point of D, and
  // it is the only difference between D and C.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rptr_q <= '0;
      for (int unsigned i = 0; i < DEPTH; i++) begin
        arr_a[i] <= '0;
        arr_b[i] <= '0;
        arr_c[i] <= '0;
        arr_e[i] <= '0;
        // arr_d is NOT reset here. See above.
      end
    end else begin
      if (rd_adv) rptr_q <= rptr_q + AW'(1);
      if (wr_en) begin
        arr_a[wr_addr] <= wr_data[NARROW-1:0] ^ SALT_A[NARROW-1:0];
        arr_b[wr_addr] <=  wr_data ^ SALT_B;
        arr_c[wr_addr] <=  wr_data ^ SALT_C;
        arr_d[wr_addr] <=  wr_data ^ SALT_D;
        arr_e[wr_addr] <= wr_data[NARROW-1:0] ^ SALT_E[NARROW-1:0];
      end
    end
  end

endmodule

`default_nettype wire
