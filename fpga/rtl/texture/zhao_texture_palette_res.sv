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
    // EIGHT, not four. X6: "four-bit generation and the load protocol are not
    // final". A 4-bit generation wraps after sixteen reloads of a slot, and a
    // binding held longer than that comes back matching the WRONG palette --
    // worse than the stale answer the generation exists to give.
    parameter int unsigned GENW    = 8
) (
    input var logic clk,
    input var logic rst_n,

    // ---- load: the EXPLICIT protocol of ruling X6 ----------------------------
    //
    //   BEGIN(slot, new_generation, expected_crc)
    //       immediately marks the slot nonresident and blocks same-slot lookup
    //   WRITE(index, value)
    //       legal only while that slot is loading
    //   END(slot, generation, crc_ok)
    //       resident only if ALL entries arrived and the CRC matches
    //
    // The protocol this replaces was implicit: the first write began a load,
    // `last` ended it, and the slot became resident on `last` whatever had
    // actually arrived. Three things that could not be said in it: a load can
    // FAIL, a load can be INCOMPLETE, and a reload with the SAME generation is
    // illegal rather than a no-op.
    input  var logic                      ld_valid_i,
    output var logic                      ld_ready_o,
    input  var logic [1:0]                ld_op_i,      // see LD_* below
    input  var logic [$clog2(SLOTS)-1:0]  ld_slot_i,    // BEGIN and END
    input  var logic [GENW-1:0]           ld_gen_i,     // BEGIN and END
    input  var logic [$clog2(ENTRIES)-1:0] ld_idx_i,    // WRITE
    input  var logic [15:0]               ld_rgb565_i,  // WRITE
    input  var logic                      ld_crc_ok_i,  // END

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
    output var logic [31:0]               cold_o,
    // Every way the protocol can be misused, counted separately. One
    // `errors_o` would let a CRC failure and a stray write look alike, and
    // they are not alike: one is a bad cartridge, the other is a bad loader.
    output var logic [31:0]               err_write_outside_o,  // WRITE with no open load
    output var logic [31:0]               err_same_gen_o,       // BEGIN reusing a generation
    output var logic [31:0]               err_incomplete_o,     // END with entries missing
    output var logic [31:0]               err_crc_o,            // END with crc_ok low
    output var logic [31:0]               loads_ok_o
);

  // ---- the three operations ------------------------------------------------
  localparam logic [1:0] LD_BEGIN = 2'd0;
  localparam logic [1:0] LD_WRITE = 2'd1;
  localparam logic [1:0] LD_END   = 2'd2;

  // ---- storage -------------------------------------------------------------
  // One halfword RAM addressed {slot, index}, written by the load port and read
  // synchronously -- the shape an M10K infers from. Deliberately NOT reset: a
  // reset loop over the array is what stops M10K inference, and `res_r` gates
  // every read until a slot has been completely loaded.
  //
  // NOT RESETTING THE ARRAY WAS ONLY HALF THE RULE, and the half that was
  // written down here was the half that does not bite. An M10K has no reset
  // port, so an array touched by a process with an ASYNCHRONOUS RESET cannot
  // be one -- whatever that process does or does not do to the array itself.
  // Both ports lived in the module's `always_ff @(posedge clk or negedge
  // rst_n)`, so all 16,384 bits sat in flip-flops behind a comment claiming
  // the opposite. Corrected 2026-09-04; the same half-fix was made and caught
  // on zhao_texture_cache_pipe the day before.
  //
  // The ports are therefore in their own clock-only process below. Nothing
  // else may touch `mem_r`, and nothing else does.
  logic [15:0] mem_r [SLOTS * ENTRIES];

  // Per-slot generation and residency. These ARE reset: they are the guards.
  logic [GENW-1:0] gen_r [SLOTS];
  logic            res_r [SLOTS];

  // ---- the open load -------------------------------------------------------
  // ONE at a time, which is what lets WRITE carry only (index, value) as X6
  // specifies. `seen_r` is a presence bit per entry rather than a counter,
  // because a counter cannot tell 256 distinct writes from 255 plus a
  // duplicate -- and X6 asks for duplicate and missing entries to be tested.
  logic                        loading_r;
  logic [$clog2(SLOTS)-1:0]    ld_slot_r;
  logic [GENW-1:0]             ld_gen_r;
  logic [ENTRIES-1:0]          seen_r;

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

  // ---- the two memory ports ------------------------------------------------
  // One write, one read, one clock, NO reset. This is the whole reason the
  // array can be an M10K; putting either port in the reset process below would
  // put every bit of it back into flip-flops.
  //
  // `mem_wr_c` is a named signal rather than the condition spelled out twice,
  // so the write here and the `seen_r` flag there cannot drift apart -- they
  // are the same event and a palette whose coverage flag disagreed with its
  // contents would fail the completeness check for a reason nothing pointed at.
  logic mem_wr_c;
  assign mem_wr_c = ld_valid_i && (ld_op_i == LD_WRITE) && loading_r;

  always_ff @(posedge clk) begin
    if (mem_wr_c) mem_r[{ld_slot_r, ld_idx_i}] <= ld_rgb565_i;
    if (lu_valid_i) l1_data_q <= mem_r[{lu_slot_i, lu_idx_i}];
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      l1_v_q    <= 1'b0;
      lookups_o <= 32'd0;
      stale_o   <= 32'd0;
      cold_o    <= 32'd0;
      loading_r <= 1'b0;
      seen_r    <= '0;
      err_write_outside_o <= 32'd0;
      err_same_gen_o      <= 32'd0;
      err_incomplete_o    <= 32'd0;
      err_crc_o           <= 32'd0;
      loads_ok_o          <= 32'd0;
      for (int i = 0; i < SLOTS; i++) begin
        gen_r[i] <= '0;
        res_r[i] <= 1'b0;
      end
    end else begin
      // ---- load ---------------------------------------------------------
      if (ld_valid_i) begin
        unique case (ld_op_i)
          LD_BEGIN: begin
            // NEVER RELOAD A SLOT WITH THE SAME GENERATION (X6). A caller that
            // does has not moved the binding on, so every handle to the old
            // palette would still match the new one -- the generation would be
            // present and useless. Refused and counted, not quietly obeyed.
            if (ld_gen_i == gen_r[ld_slot_i]) begin
              err_same_gen_o <= err_same_gen_o + 32'd1;
            end else begin
              // Nonresident IMMEDIATELY, and the generation advances here
              // rather than at END: a lookup arriving mid-load must already
              // see the slot as moved on. Advancing at END would leave a
              // window where a half-written palette answers under the old
              // generation, which is the fault the whole mechanism closes.
              gen_r[ld_slot_i] <= ld_gen_i;
              res_r[ld_slot_i] <= 1'b0;
              loading_r        <= 1'b1;
              ld_slot_r        <= ld_slot_i;
              ld_gen_r         <= ld_gen_i;
              seen_r           <= '0;
            end
          end

          LD_WRITE: begin
            // Legal ONLY while that slot is loading. A write outside a load
            // would edit a resident palette under a live binding.
            if (!loading_r) begin
              err_write_outside_o <= err_write_outside_o + 32'd1;
            end else begin
              // The write itself is in the clock-only process below; only the
              // coverage flag is set here.
              seen_r[ld_idx_i] <= 1'b1;
            end
          end

          LD_END: begin
            if (!loading_r || ld_slot_i != ld_slot_r || ld_gen_i != ld_gen_r) begin
              err_write_outside_o <= err_write_outside_o + 32'd1;
            end else begin
              loading_r <= 1'b0;
              // RESIDENT ONLY IF ALL ENTRIES ARRIVED AND THE CRC MATCHES.
              // Either failure leaves the slot nonresident, which sends the
              // caller down the cold path -- slower, and correct. A palette
              // that is 255 entries of new data and one of old is the kind of
              // wrong that looks right on most pixels.
              if (!ld_crc_ok_i) begin
                err_crc_o <= err_crc_o + 32'd1;
              end else if (seen_r != {ENTRIES{1'b1}}) begin
                err_incomplete_o <= err_incomplete_o + 32'd1;
              end else begin
                res_r[ld_slot_r] <= 1'b1;
                loads_ok_o <= loads_ok_o + 32'd1;
              end
            end
          end

          default: err_write_outside_o <= err_write_outside_o + 32'd1;
        endcase
      end

      // ---- lookup -------------------------------------------------------
      // A LOOKUP ACCEPTED ON THE SAME CLOCK AS BEGIN MUST NOT ANSWER (X6),
      // and it must not do so by relying on read-during-write. `gen_r` and
      // `res_r` are registers, so on the clock BEGIN lands they still hold the
      // OLD generation and the OLD residency -- the lookup would match, read
      // resident, and return a colour from the palette being overwritten.
      //
      // So the BEGIN is forwarded explicitly. The directed suite caught this
      // as the one case of eighteen that failed, which is exactly the case the
      // ruling calls out by name.
      begin
        automatic logic begin_same_slot =
            ld_valid_i && (ld_op_i == LD_BEGIN)
            && (ld_gen_i != gen_r[ld_slot_i])      // i.e. the BEGIN is accepted
            && (ld_slot_i == lu_slot_i);

        l1_v_q <= lu_valid_i;
        if (lu_valid_i) begin
          automatic logic st = (gen_r[lu_slot_i] != lu_gen_i) || begin_same_slot;
          l1_stale_q <= st;
          l1_res_q   <= res_r[lu_slot_i] && !begin_same_slot;
          lookups_o  <= lookups_o + 32'd1;
          if (st)                      stale_o <= stale_o + 32'd1;
          else if (!res_r[lu_slot_i])  cold_o  <= cold_o  + 32'd1;
        end
      end
    end
  end

  assign lu_valid_o    = l1_v_q;
  assign lu_rgb565_o   = l1_data_q;
  assign lu_stale_o    = l1_stale_q;
  assign lu_resident_o = l1_res_q;

endmodule : zhao_texture_palette_res

`default_nettype wire
