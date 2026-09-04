// zhao_raster_tilestore.sv — RASTER.TILESTORE: the ping-pong 16×16 tile RAM
// at 64 bpp with a single-cycle clear (phase 4, ZH-021).
//
// Law (in citation order):
//   design/contracts/RASTER.TILESTORE.md — the block contract (port layouts,
//       backpressure, the read-during-write law, the clear law).
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §8 "Active tile storage" — per
//       pixel: 24-bit RGB working colour, 8-bit effect tag/strength, 24-bit
//       inverse-W depth, 8-bit stencil. "Exactly 64 bits per active pixel,
//       or 2 KiB per tile. Two ping-pong tiles consume roughly 4 KiB."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §7.3 — "two active 16×16 tile
//       stores" is a named M10K/MLAB tenant. (The ledger cites "memory_rules
//       §7.3"; spec/memory_rules.md has seven sections and no §7.3 — the
//       M10K tenancy list is CHARTER §7.3. Recorded in the contract Notes.)
//   spec/stars_and_flares.md §1 — the frozen tile store is 24-bit working
//       colour + 8-bit tag; the tag is a field of this word, not a side
//       channel.
//
// WHAT THIS BLOCK IS NOT: no fragment maths (no Z test, no blend, no stencil
// op — RASTER.FRAGMENT owns all of those and this block never inspects the
// word it stores), no dither and no framebuffer write (RASTER.RESOLVE), no
// VRAM traffic of any kind, no tile scheduling, no addressing of tiles within
// the frame. It is a two-banked 256×64 memory with a clear and a swap.
//
// ---------------------------------------------------------------------------
// THE WORD (charter §8 fields; bit positions defined HERE)
// ---------------------------------------------------------------------------
//   [63:40]  24-bit RGB working colour, R [63:56] G [55:48] B [47:40]
//   [39:32]   8-bit effect tag/strength (stars_and_flares.md §1:
//             tag = (channel << 6) | strength)
//   [31: 8]  24-bit inverse-W depth
//   [ 7: 0]   8-bit stencil
// The charter fixes the four FIELDS and their widths; it does not fix their
// bit positions, so this module fixes them once, in charter order, MSB first.
// The block is field-agnostic — it stores 64 bits and never decodes them —
// but zref::TileStore, zref::TileResolve, zhao_raster_resolve and every test
// read the same packing, so the layout is load-bearing and lives in one place
// (repeated in design/contracts/RASTER.TILESTORE.md, nowhere else).
//
// ---------------------------------------------------------------------------
// THE CLEAR IS O(1), NOT 256 CYCLES
// ---------------------------------------------------------------------------
// A per-word PRESENT bit shadows each bank. `clear` resets all 256 present
// bits of the front bank and latches the clear word; a read of a word whose
// present bit is 0 returns the clear word instead of the RAM contents, and a
// write sets the present bit. So:
//
//   · clear costs ONE cycle and never blocks a read port — the charter's
//     per-tile pass order clears every tile at pass 1, 360 times a frame in
//     Z60, and 360 × 256 = 92k scrub cycles a frame is not a budget anyone
//     should pay; and
//   · no clear traffic ever competes with the fragment pipeline for the
//     write port.
//
// Cost: 2 × 256 present flops + 2 × 64 clear-word flops ≈ 640 flops against
// 2 × 2 KiB of M10K. The present bits MUST be flops, not RAM — 256 of them
// are cleared at once.
//
// This is exactly why writes are FULL 64-bit words with no byte enables. A
// partial write to a not-present word would set the present bit while the
// un-enabled bytes still held pre-clear RAM garbage, so byte enables would
// need either per-BYTE present bits (4,096 flops) or a read-modify-write port
// the M10K does not have. The fragment pipeline reads the pixel before it
// writes it (Z test, then blend), so it already holds the whole word.
//
// ---------------------------------------------------------------------------
// PING-PONG
// ---------------------------------------------------------------------------
// Two physical banks. One is the FRONT bank — the working tile: the clear,
// the write port and read port A address it. The other is the BACK bank —
// the finished tile: read port B (RASTER.RESOLVE's `tile_read`) addresses it.
// `swap` exchanges the roles in one cycle. That is the ledger's "ping-pong
// hides resolve latency": resolve streams the back bank at its own pace while
// the fragment pipeline already renders the next tile into the front.
//
// Each PHYSICAL bank therefore carries exactly one write port and one read
// port at any instant (port A reads it iff it is front, port B iff it is
// back — mutually exclusive), which is what a simple dual-port M10K offers.
// The banks are declared as two separate arrays with one read address each
// so that stays visible to inference; the role mux sits on the addresses in
// front of the RAM and on a registered bank tag behind it, never as a second
// read port.
//
// Ordering inside a cycle is fully defined:
//   1. `clear` (front bank only) happens first,
//   2. then `write` (front bank only) — a write is NOT accepted in a cycle
//      where a clear is accepted, so 1 and 2 never race,
//   3. reads on both ports observe the result of 1 and 2 — a read returns
//      NEW data for a same-cycle same-address write (write-first), because a
//      read-modify-write fragment pipeline must never see the pixel it just
//      wrote as stale,
//   4. `swap` is last: an access accepted in the same cycle as a swap targets
//      the roles as they were BEFORE the swap.
//
// Timing: read latency is FIXED at 1 cycle on both ports (ledger
// `latency: fixed:1`), and every port accepts one access per clock with no
// stall condition of its own — the ledger's "1 tile access per clock".
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_raster_tilestore).

module zhao_raster_tilestore (
  input  logic clk,
  input  logic rst_n,

  // ---- clear: reset the FRONT bank to `clear_data_i` in one cycle --------
  input  logic        clear_valid_i,
  output logic        clear_ready_o,
  input  logic [63:0] clear_data_i,

  // ---- write port: FRONT bank, full 64-bit words ------------------------
  input  logic        wr_valid_i,
  output logic        wr_ready_o,
  input  logic [7:0]  wr_addr_i,      // {row[3:0], col[3:0]}, row 0 = top
  input  logic [63:0] wr_data_i,

  // ---- read port A: FRONT bank (the fragment working view), latency 1 ---
  input  logic        rd_valid_i,
  output logic        rd_ready_o,
  input  logic [7:0]  rd_addr_i,
  input  logic [15:0] rd_src_id_i,    // source_id passthrough
  output logic        rd_valid_o,
  output logic [63:0] rd_data_o,
  output logic [15:0] rd_src_id_o,

  // ---- read port B: BACK bank (RASTER.RESOLVE's tile_read), latency 1 ---
  input  logic        res_valid_i,
  output logic        res_ready_o,
  input  logic [7:0]  res_addr_i,
  output logic        res_valid_o,
  output logic [63:0] res_data_o,

  // ---- ping-pong --------------------------------------------------------
  input  logic        swap_valid_i,
  output logic        swap_ready_o,
  output logic        front_bank_o,   // which PHYSICAL bank is the front one

  // ---- counters ---------------------------------------------------------
  output logic [31:0] tile_references_o
);

  localparam int unsigned WORDS   = 256;
  localparam logic [31:0] REF_MAX = 32'hFFFF_FFFF;

  // ------------------------------------------------------------ handshake --
  // No port has a stall condition of its own; the ONLY cross-port rule is
  // that a clear locks the write port for that cycle (ordering rule 2).
  // Nothing here reads a downstream ready, so there is no valid←ready path.
  assign clear_ready_o = 1'b1;
  assign wr_ready_o    = !clear_valid_i;
  assign rd_ready_o    = 1'b1;
  assign res_ready_o   = 1'b1;
  assign swap_ready_o  = 1'b1;

  logic clear_acc, wr_acc, rd_acc, res_acc, swap_acc;
  assign clear_acc = clear_valid_i && clear_ready_o;
  assign wr_acc    = wr_valid_i    && wr_ready_o;
  assign rd_acc    = rd_valid_i    && rd_ready_o;
  assign res_acc   = res_valid_i   && res_ready_o;
  assign swap_acc  = swap_valid_i  && swap_ready_o;

  // ------------------------------------------------------------- storage ---
  // Two banks of 256 × 64 bits (2 KiB each, charter §8), one read address and
  // one write enable apiece.
  logic [63:0]      ram0 [0:WORDS-1];
  logic [63:0]      ram1 [0:WORDS-1];
  logic [WORDS-1:0] present0, present1;
  logic [63:0]      clear0, clear1;

  logic front_r;    // physical index of the FRONT (working) bank
  logic back_r;
  assign back_r       = !front_r;
  assign front_bank_o = front_r;

  // Per-bank port routing. Bank 0 is addressed by port A iff it is the front
  // bank, otherwise by port B — never by both.
  logic [7:0] b0_raddr, b1_raddr;
  logic       b0_we,    b1_we;
  always_comb begin
    b0_raddr = (front_r == 1'b0) ? rd_addr_i : res_addr_i;
    b1_raddr = (front_r == 1'b0) ? res_addr_i : rd_addr_i;
    b0_we    = wr_acc && (front_r == 1'b0);
    b1_we    = wr_acc && (front_r == 1'b1);
  end

  // Effective present/clear state AFTER this cycle's clear, per ROLE. Only
  // the front bank can be cleared, so the back bank is stable for the whole
  // resolve pass by construction — `clear_acc` reaches neither res_pres_eff
  // nor res_clr_eff below.
  // ENFORCED-BY: tests/raster/raster_tilestore_directed.cpp:test_pingpong_isolation
  logic        rd_pres_eff,  res_pres_eff;
  logic [63:0] rd_clr_eff,   res_clr_eff;
  always_comb begin
    rd_pres_eff  = clear_acc ? 1'b0
                             : ((front_r == 1'b0) ? present0[b0_raddr] : present1[b1_raddr]);
    rd_clr_eff   = clear_acc ? clear_data_i
                             : ((front_r == 1'b0) ? clear0 : clear1);
    res_pres_eff = (front_r == 1'b0) ? present1[b1_raddr] : present0[b0_raddr];
    res_clr_eff  = (front_r == 1'b0) ? clear1 : clear0;
  end

  // ------------------------------------------------------- read pipelines --
  // Each port registers, at the accepting edge: the RAM word, the present
  // bit, the clear word in force, and a same-address write bypass. Next cycle
  //
  //     data = byp_q ? byp_data_q : (present_q ? ram_q : clear_q)
  //
  // The bypass carries the WHOLE written word (writes are full words), so it
  // needs no merge with the base value — precisely the simplification the
  // no-byte-enable decision above buys.
  logic [63:0] ram0_q, ram1_q;
  logic        rd_v_q,  res_v_q;
  logic        rd_bank_q, res_bank_q;   // which physical bank each port read
  logic        rd_pres_q, res_pres_q;
  logic [63:0] rd_clr_q,  res_clr_q;
  logic        rd_byp_q,  res_byp_q;
  logic [63:0] rd_byp_data_q, res_byp_data_q;
  logic [15:0] rd_src_q;

  logic [63:0] rd_base, res_base;
  always_comb begin
    rd_base  = rd_bank_q  ? ram1_q : ram0_q;
    res_base = res_bank_q ? ram1_q : ram0_q;
  end

  assign rd_valid_o  = rd_v_q;
  assign rd_src_id_o = rd_src_q;
  assign rd_data_o   = rd_byp_q  ? rd_byp_data_q
                                 : (rd_pres_q  ? rd_base  : rd_clr_q);
  assign res_valid_o = res_v_q;
  assign res_data_o  = res_byp_q ? res_byp_data_q
                                 : (res_pres_q ? res_base : res_clr_q);

  // --------------------------------------------------------- the counter ---
  // Accepted DATA accesses (writes + both read ports). `clear` and `swap` are
  // commands, not accesses, and are not counted. Saturating per
  // spec/counters.md §4 — a counter never wraps.
  logic [31:0] refs_r;
  logic [2:0]  refs_add;
  assign tile_references_o = refs_r;
  always_comb begin
    refs_add = {2'd0, wr_acc} + {2'd0, rd_acc} + {2'd0, res_acc};
  end

  // ------------------------------------------------------------ the RAMs ---
  // Both banks, both ports, one clock, NO reset. 32,768 bits turn on this.
  //
  // These four lines used to sit at the top of the reset process below. An
  // M10K has no reset port, so an array TOUCHED BY an asynchronously-reset
  // process cannot be one -- and that is true of the read as much as the
  // write, which is the part that is easy to miss: `ram0_q <= ram0[addr]` puts
  // the array inside that process just as surely as a write does.
  //
  // `ram0_q` and `ram1_q` left the reset list with them, for the same reason
  // one level down: a read register with a reset cannot be the M10K's own
  // output register, so keeping it would have cost the block a pipeline stage
  // of registers to save nothing. They power up to zero on the device, and the
  // simulator zeroes them too, which is what the reset was asking for anyway.
  //
  // WHAT MAKES THAT SOUND is `present`, and it is worth stating because the
  // next person to add a reset back will be looking for the reason it was safe
  // to remove. `rd_data_o` selects `rd_base` -- the only consumer of these two
  // registers -- ONLY when `rd_pres_q`, and `present0`/`present1` ARE reset to
  // zero. So no word is readable until it has been written, and the contents
  // of an uninitialised RAM word are never observable at the output. That is
  // the same invariant that let the ARRAYS go unreset in the first place; the
  // read registers simply inherit it.
  //
  // If `present` ever stops gating the read, these registers need their reset
  // back and the block gives up its M10K output registers with them.
  always_ff @(posedge clk) begin
    ram0_q <= ram0[b0_raddr];
    ram1_q <= ram1[b1_raddr];
    if (b0_we) ram0[wr_addr_i] <= wr_data_i;
    if (b1_we) ram1[wr_addr_i] <= wr_data_i;
  end

  // ---------------------------------------------------------- sequential ---
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      front_r        <= 1'b0;
      present0       <= {WORDS{1'b0}};
      present1       <= {WORDS{1'b0}};
      clear0         <= 64'd0;
      clear1         <= 64'd0;
      rd_v_q         <= 1'b0;
      res_v_q        <= 1'b0;
      rd_bank_q      <= 1'b0;
      res_bank_q     <= 1'b1;
      rd_pres_q      <= 1'b0;
      res_pres_q     <= 1'b0;
      rd_clr_q       <= 64'd0;
      res_clr_q      <= 64'd0;
      rd_byp_q       <= 1'b0;
      res_byp_q      <= 1'b0;
      rd_byp_data_q  <= 64'd0;
      res_byp_data_q <= 64'd0;
      rd_src_q       <= 16'd0;
      refs_r         <= 32'd0;
    end else begin
      // ---- 1. clear (front bank only) ------------------------------------
      if (clear_acc) begin
        if (front_r == 1'b0) begin
          present0 <= {WORDS{1'b0}};
          clear0   <= clear_data_i;
        end else begin
          present1 <= {WORDS{1'b0}};
          clear1   <= clear_data_i;
        end
      end

      // ---- 2. write (front bank only; never with a clear) -----------------
      if (wr_acc) begin
        if (front_r == 1'b0) present0[wr_addr_i] <= 1'b1;
        else                 present1[wr_addr_i] <= 1'b1;
      end

      // ---- 3. reads --------------------------------------------------------
      rd_v_q <= rd_acc;
      if (rd_acc) begin
        rd_bank_q     <= front_r;
        rd_pres_q     <= rd_pres_eff;
        rd_clr_q      <= rd_clr_eff;
        rd_byp_q      <= wr_acc && (wr_addr_i == rd_addr_i);
        rd_byp_data_q <= wr_data_i;
        rd_src_q      <= rd_src_id_i;
      end

      res_v_q <= res_acc;
      if (res_acc) begin
        res_bank_q     <= back_r;
        res_pres_q     <= res_pres_eff;
        res_clr_q      <= res_clr_eff;
        // The write port targets the FRONT bank, so it can never alias a back
        // bank read: this bypass is dead BY CONSTRUCTION. It is wired (as a
        // constant) rather than omitted so the two read paths stay visibly
        // identical, and the deadness is asserted rather than assumed.
        // ENFORCED-BY: tests/raster/raster_tilestore_directed.cpp:test_pingpong_isolation
        res_byp_q      <= 1'b0;
        res_byp_data_q <= 64'd0;
      end

      // ---- 4. swap — LAST: this cycle's accesses used the pre-swap roles ---
      if (swap_acc) front_r <= !front_r;

      // ---- counters (saturating, spec/counters.md §4) ----------------------
      if (refs_add != 3'd0) begin
        if (refs_r > (REF_MAX - {29'd0, refs_add})) refs_r <= REF_MAX;
        else                                        refs_r <= refs_r + {29'd0, refs_add};
      end
    end
  end

endmodule : zhao_raster_tilestore
