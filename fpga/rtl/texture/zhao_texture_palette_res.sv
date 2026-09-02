// zhao_texture_palette_res.sv — resident palette slots with generations.
//
// BESIDE the current lazy palette fetch. Nothing instantiates it yet.
//
// ---------------------------------------------------------------------------
// WHY
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md:
//
//   > Do not perform a 16-way palette-page search on the hot response path
//   > forever. The preferred production form is:
//   >     material binding -> resident palette slot + generation
//   > [...] translation to a resident slot happens once in T1 or at material
//   > binding -- not after every CLUT index returns.
//   >
//   > Keep the current lazy fallback only as a cold/error path behind a small
//   > fallback FIFO. It must not be one global pf_v that monopolizes issue and
//   > blocks resident work.
//
// The shipped pipe's `pf_v` is exactly that global: while a palette fetch is
// outstanding, `req_ready_o` is low and `issue_fire_c` is blocked, so ONE cold
// palette stalls every resident request behind it.
//
// ---------------------------------------------------------------------------
// WHAT THE GENERATION IS FOR, AND WHY IT IS NOT A VALID BIT
// ---------------------------------------------------------------------------
// A slot can be reloaded while requests that referenced its OLD contents are
// still in flight. A valid bit cannot express that: clearing it makes those
// requests miss (correct but slow), and leaving it set makes them read the NEW
// palette under the OLD binding (fast and WRONG -- a creature briefly wearing
// another creature's colours, which is precisely the kind of fault that is
// obvious in motion and invisible in a still).
//
// The generation resolves it exactly. A lookup carries the generation its
// binding was resolved against; if the slot has moved on, the lookup reports
// STALE and the caller takes the cold path. Nothing wrong is ever returned,
// and nothing correct is ever thrown away.
//
// That is the brief's "palette generation/invalidation under requests in
// flight" gate, and it is the one property here worth more than the speed.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_palette_res #(
    parameter int unsigned SLOTS   = 4,
    parameter int unsigned ENTRIES = 256,
    parameter int unsigned GENW    = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- load interface, exactly the brief's field list ----------------------
    //     slot / generation / entry index / RGB565 value / valid, last
    input  var logic                      ld_valid_i,
    output var logic                      ld_ready_o,
    input  var logic [$clog2(SLOTS)-1:0]  ld_slot_i,
    input  var logic [GENW-1:0]           ld_gen_i,
    input  var logic [$clog2(ENTRIES)-1:0] ld_idx_i,
    input  var logic [15:0]               ld_rgb565_i,
    input  var logic                      ld_last_i,

    // ---- lookup, one per clock, fully pipelined -----------------------------
    input  var logic                      lu_valid_i,
    input  var logic [$clog2(SLOTS)-1:0]  lu_slot_i,
    input  var logic [GENW-1:0]           lu_gen_i,     // the binding's generation
    input  var logic [$clog2(ENTRIES)-1:0] lu_idx_i,

    output var logic                      lu_valid_o,
    output var logic [15:0]               lu_rgb565_o,
    // STALE means "this slot has been reloaded since your binding resolved".
    // The caller must take the cold path; the colour returned is NOT usable.
    output var logic                      lu_stale_o,
    // RESIDENT is low when the slot has never been completely loaded.
    output var logic                      lu_resident_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]               lookups_o,
    output var logic [31:0]               stale_o,
    output var logic [31:0]               cold_o
);

  // ---- storage -------------------------------------------------------------
  // One halfword RAM addressed {slot, index}, written by the load port and read
  // synchronously -- the shape an M10K infers from. Deliberately NOT reset: a
  // reset loop over the array is what stops M10K inference, and `res_r` gates
  // every read until a slot has been completely loaded.
  logic [15:0] mem_r [SLOTS * ENTRIES];

  // Per-slot generation and residency. These ARE reset: they are the guards.
  logic [GENW-1:0] gen_r [SLOTS];
  logic            res_r [SLOTS];

  // The load port never refuses. It is fed by a bounded loader, and a ready it
  // never lowers would invite a caller to wait on it.
  assign ld_ready_o = 1'b1;

  // ---- lookup stage 1: register the request and its verdict ---------------
  // The generation is compared HERE, against the slot's state at the moment the
  // lookup is accepted -- not after the RAM read returns. Comparing later would
  // race a load that lands in between and reintroduce exactly the fault the
  // generation exists to prevent.
  logic            l1_v_q;
  logic            l1_stale_q;
  logic            l1_res_q;
  logic [15:0]     l1_data_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      l1_v_q    <= 1'b0;
      lookups_o <= 32'd0;
      stale_o   <= 32'd0;
      cold_o    <= 32'd0;
      for (int i = 0; i < SLOTS; i++) begin
        gen_r[i] <= '0;
        res_r[i] <= 1'b0;
      end
    end else begin
      // ---- load ---------------------------------------------------------
      if (ld_valid_i) begin
        mem_r[{ld_slot_i, ld_idx_i}] <= ld_rgb565_i;
        // The generation advances on the FIRST beat, not the last: a lookup
        // arriving mid-load must already see the slot as moved on. Advancing
        // on `last` would leave a window where a half-written palette answers
        // under the old generation, which is the fault this whole mechanism
        // exists to close.
        if (!ld_last_i && (gen_r[ld_slot_i] != ld_gen_i)) begin
          gen_r[ld_slot_i] <= ld_gen_i;
          res_r[ld_slot_i] <= 1'b0;      // not resident again until `last`
        end
        if (ld_last_i) begin
          gen_r[ld_slot_i] <= ld_gen_i;
          res_r[ld_slot_i] <= 1'b1;
        end
      end

      // ---- lookup -------------------------------------------------------
      l1_v_q <= lu_valid_i;
      if (lu_valid_i) begin
        l1_data_q  <= mem_r[{lu_slot_i, lu_idx_i}];
        l1_stale_q <= (gen_r[lu_slot_i] != lu_gen_i);
        l1_res_q   <= res_r[lu_slot_i];
        lookups_o  <= lookups_o + 32'd1;
        if (gen_r[lu_slot_i] != lu_gen_i) stale_o <= stale_o + 32'd1;
        else if (!res_r[lu_slot_i])       cold_o  <= cold_o  + 32'd1;
      end
    end
  end

  assign lu_valid_o    = l1_v_q;
  assign lu_rgb565_o   = l1_data_q;
  assign lu_stale_o    = l1_stale_q;
  assign lu_resident_o = l1_res_q;

endmodule : zhao_texture_palette_res

`default_nettype wire
