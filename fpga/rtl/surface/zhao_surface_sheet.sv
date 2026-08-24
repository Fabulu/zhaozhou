// zhao_surface_sheet.sv — SURFACE.SHEET: the resident surface-sheet store
// (phase 6, ZH-031).
//
// Law, in citation order:
//   design/contracts/SURFACE.SHEET.md — the block contract.
//   design/blocks.yml — `inputs: [sheet_requests]`,
//       `outputs: [sheet_pages, residency_status]`, `backpressure: ready_valid`,
//       `latency: variable`, "1 sheet texel per clock", counter
//       `surface_texels_touched`, `source_ids: true`, and the note
//       "Residency policy per 11; overflow rejects the stamp, never
//       partial-writes."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md 12 — the sheet IS 64x64 texels,
//       8-bit tag, 8-bit strength/age, per patch, PERSISTENT.
//   spec/terrain_rules.md 2 (layer F: 64x64 {tag u8, strength u8} = 8,192 B),
//       7 ("F written only by SURFACE.STAMP"), 8 (the 16 MB sheets pool),
//       11 (residency policy: EXPLICITLY NOT DECIDED).
//   reference/include/zref/zref_surface.hpp `zref::surface::SheetStore` — the
//       oracle, and the place every chosen rule below is argued a second time.
//   reference/src/zrender/render_frame.cpp `SoftwareRenderer::sheet_for` —
//       "creates on first use", value-initialised: a fresh sheet reads zero.
//
// ---------------------------------------------------------------------------
// FOUND vs CHOSEN — the honest split
// ---------------------------------------------------------------------------
// FOUND (ratified, and this block obeys it):
//   * the sheet's shape and size: 64x64 texels of {tag u8, strength u8};
//   * that it PERSISTS across frames (charter 12, and the reference renderer
//     keeps `sheets_` alive across `render_frame` calls);
//   * that a sheet which has never been stamped reads as ZERO everywhere;
//   * that overflow REJECTS the stamp and never partial-writes (ledger note).
//
// CHOSEN, because spec/terrain_rules.md 11 — the section the ledger cites for
// the residency policy — is the list of things that section explicitly does
// NOT decide. Each choice carries its rejected alternative:
//
//   C1. FULLY ASSOCIATIVE over `Slots` slots, keyed by the 32-bit patch
//       handle. REJECTED ALTERNATIVE: direct-mapped on the handle's low bits —
//       one comparator instead of `Slots`, and two patches whose handles
//       collide would evict each other every frame, destroying the one
//       property (persistence) the block exists for. At Slots = 2 the
//       associativity is two 32-bit comparators.
//
//   C2. NEVER EVICT. An ACQUIRE that finds no free slot answers OVERFLOW and
//       changes NOTHING — no slot is stolen, no handle is rewritten. The
//       requester must then write nothing at all; this block enforces the
//       second half structurally, because a WRITE names its handle and a
//       non-resident handle is dropped. REJECTED ALTERNATIVE: LRU eviction.
//       It needs recency state that a capture does not carry, so replay stops
//       being exact, and it can silently discard a scar the player can see.
//
//   C3. A FRESHLY ALLOCATED SLOT IS CLEARED BY A 4,096-CYCLE SWEEP, and the
//       ACQUIRE response is withheld until the sweep finishes — which is why
//       the ledger's latency is `variable`. RE-ACQUIRING A RESIDENT HANDLE
//       DOES NOT CLEAR: that is persistence.
//       REJECTED ALTERNATIVE: a per-texel `present` bit with a clear-word
//       bypass, which is what design/contracts/RASTER.TILESTORE.md chose for
//       its 256-word banks. Here it would cost 4,096 x Slots flops (8,192 at
//       the default) plus a 3-way output mux, to save 4,096 cycles once per
//       patch per frame — 4,096 cycles is 0.25 % of a 1.67 M-cycle frame, and
//       flops are the scarcer resource. The trade inverts at TILESTORE's
//       scale, which is why the two blocks answer differently.
//
//   C4. READS AND WRITES NAME THE HANDLE, NOT THE SLOT. The associative
//       lookup happens on every access. REJECTED ALTERNATIVE: returning a slot
//       index from ACQUIRE and indexing with it, which is one comparator
//       cheaper and lets a stale index from a previous frame write into
//       another patch's scars. The handle is the identity the ABI carries
//       (commands.zidl SurfaceStamp `handle32[patch] patch`); using anything
//       else re-derives identity that was already stated.
//
//   C5. THE READ PORT AND THE WRITE PORT ARE SEPARATE. One simple dual-port
//       M10K offers exactly one read and one write per clock
//       (design/contracts/RASTER.TILESTORE.md says so about the same device),
//       and the ledger asks for 1 sheet texel per clock while SURFACE.STAMP is
//       a read-modify-write engine. A single shared port would halve the rate
//       to one texel per two clocks and MISS the ledger target.
//       Read-during-write at the SAME address returns the OLD word (both
//       accesses live in one `always_ff`, so the read sees the pre-write
//       array). SURFACE.STAMP never does that — its cursor marches forward and
//       its write trails its read by two texels — but the semantics are stated
//       rather than left to the synthesiser.
//
// NOT IN THIS BLOCK, deliberately: no VRAM port and no page loader (MEM.GUARD
// owns the 21,376 B page and terrain_rules 7 makes the streaming path HPS ->
// VRAM, not fabric), no writeback of a dirty sheet, no blend arithmetic
// (SURFACE.STAMP owns the five ops.yml modes), no draw-time sampling
// (`zref::render::sample_sheet` and TEXTURE.MOSAIC own that), no compression.
//
// Conservative SystemVerilog subset only (charter 2).

module zhao_surface_sheet #(
    // Resident sheets. 2 = one being stamped while one is being sampled, which
    // is the smallest set that does not serialise the two consumers. Each slot
    // costs 4,096 x 16 b = 65,536 bits (about seven M10K), so this is the
    // block's whole memory bill and the first fit report should retune it.
    parameter int unsigned Slots = 2
) (
    input logic clk,
    input logic rst_n,

    // -----------------------------------------------------------------------
    // sheet_requests — the control + read port
    // -----------------------------------------------------------------------
    input  logic        req_valid_i,
    output logic        req_ready_o,
    input  logic [ 1:0] req_op_i,      // OP_ACQUIRE / OP_READ / OP_RELEASE
    input  logic [31:0] req_handle_i,  // handle32 {index:24, generation:8}
    input  logic [11:0] req_texel_i,   // j*64 + i, scan order (OP_READ only)
    input  logic [15:0] req_src_id_i,

    // -----------------------------------------------------------------------
    // sheet_pages — the response stream
    // -----------------------------------------------------------------------
    output logic        pg_valid_o,
    input  logic        pg_ready_i,
    output logic [ 1:0] pg_op_o,        // echoes req_op_i
    output logic [ 1:0] pg_status_o,    // ST_HIT / ST_ALLOCATED / ST_OVERFLOW / ST_MISS
    output logic [ 7:0] pg_tag_o,       // OP_READ: layer F tag,  else 0
    output logic [ 7:0] pg_strength_o,  // OP_READ: layer F strength, else 0
    output logic [15:0] pg_src_id_o,

    // -----------------------------------------------------------------------
    // the write port (terrain_rules 7: layer F is written by SURFACE.STAMP
    // and by nothing else)
    // -----------------------------------------------------------------------
    input  logic        wr_valid_i,
    output logic        wr_ready_o,
    input  logic [31:0] wr_handle_i,
    input  logic [11:0] wr_texel_i,
    input  logic [ 7:0] wr_tag_i,
    input  logic [ 7:0] wr_strength_i,
    input  logic        wr_we_tag_i,       // byte enables, so a blend that only
    input  logic        wr_we_strength_i,  // moves strength leaves tag alone
    input  logic [15:0] wr_src_id_i,
    output logic        wr_miss_o,         // 1-cycle pulse: handle not resident
    output logic [15:0] wr_miss_src_id_o,

    // -----------------------------------------------------------------------
    // residency_status
    // -----------------------------------------------------------------------
    output logic [Slots-1:0] res_occupancy_o,  // bit s = slot s is live
    output logic             res_busy_o,       // a clear sweep is running
    output logic             res_overflow_o,   // 1-cycle pulse on a rejected ACQUIRE

    output logic [31:0] surface_texels_touched_o,
    output logic        idle_o
);

  // ---- request opcodes and response statuses -------------------------------
  localparam logic [1:0] OpAcquire = 2'd0;
  localparam logic [1:0] OpRead = 2'd1;
  localparam logic [1:0] OpRelease = 2'd2;

  localparam logic [1:0] StHit = 2'd0;        // resident (ACQUIRE) / read served
  localparam logic [1:0] StAllocated = 2'd1;  // fresh slot, cleared to zero
  localparam logic [1:0] StOverflow = 2'd2;   // ACQUIRE rejected, nothing changed
  localparam logic [1:0] StMiss = 2'd3;       // handle not resident (READ/RELEASE)

  localparam int unsigned Texels = 4096;  // 64 x 64, charter 12
  localparam int unsigned SlotBits = (Slots <= 1) ? 1 : $clog2(Slots);
  localparam int unsigned Words = Slots * Texels;
  localparam int unsigned AddrBits = SlotBits + 12;

  // ---- the store ----------------------------------------------------------
  // TWO BYTE PLANES, not one 16-bit array with byte enables. Deliberately NOT
  // reset — a reset loop over the array is exactly what stops M10K inference,
  // and C3's clear sweep makes the initial contents unobservable (no read can
  // be served for a handle that has not been ACQUIREd, and an allocating
  // ACQUIRE zeroes the slot before it answers).
  //
  // WHY TWO ARRAYS AND NOT ONE, measured (RUN-20260824-0317). This was
  //
  //     logic [15:0] mem[Words];
  //     if (mem_be[1]) mem[wr_addr][15:8] <= wr_word[15:8];
  //     if (mem_be[0]) mem[wr_addr][ 7:0] <= wr_word[ 7:0];
  //
  // and it cost 131,258 REGISTERS and an estimated 95,947 ALMs — 229 % of the
  // whole device — for 131,072 bits, because it inferred NO memory at all.
  // reports/QUARTUS_GOTCHAS.md §10 has three independent killers of storage
  // inference: an asynchronous read, a reset that touches the array, and BYTE
  // ENABLES. The first two were already absent here on purpose. The third was
  // present, and one is sufficient: Quartus 17.0.2 Lite does not infer M10K
  // byte-enable support from that template, and the penalty is superlinear in
  // the array, so it bit hardest exactly here.
  //
  // Splitting the word removes the byte enable rather than working around it:
  // each plane is written WHOLE, gated by its own enable. That is the same
  // behaviour — the two halves were never partially written *within* a half —
  // and it is also the shape the oracle has always had
  // (`zref::surface::Sheet` is `uint8_t tag[4096]; uint8_t strength[4096]`).
  // Preferred over instantiating `altsyncram`, which §10 offers for a
  // genuinely byte-enabled memory: no vendor primitive, nothing for the
  // simulation model to diverge on, and nothing to re-solve for the Steam and
  // silicon lanes.
  //
  // MEASURED, not assumed, because the calibration grid did not cover this
  // template: `calib_ram_8192x8_shared_re` in tools/budget/calibration.json is
  // one of these planes exactly — 8,192 x 8, synchronous read WITH a read
  // enable, read and write sharing ONE always_ff — and maps to 65,536 bits in
  // an ALTSYNCRAM AUTO Simple Dual Port at 23 ALM and ZERO registers. The
  // shared process is what preserves C5's read-during-write semantic, and it
  // costs nothing; the four-point `ram_rdw` family proves the split-process and
  // no-read-enable variants are indistinguishable from it.
  logic [7:0] mem_tag[Words];
  logic [7:0] mem_str[Words];

  // ---- the directory ------------------------------------------------------
  logic [31:0] dir_handle[Slots];
  logic [Slots-1:0] dir_live;

  // Associative lookup (C1/C4). Two 32-bit comparators at Slots = 2.
  function automatic logic lookup_hit(input logic [31:0] h);
    logic f;
    begin
      f = 1'b0;
      for (int unsigned s = 0; s < Slots; s++) if (dir_live[s] && dir_handle[s] == h) f = 1'b1;
      lookup_hit = f;
    end
  endfunction

  function automatic logic [SlotBits-1:0] lookup_slot(input logic [31:0] h);
    logic [SlotBits-1:0] r;
    begin
      r = '0;
      for (int unsigned s = 0; s < Slots; s++)
      if (dir_live[s] && dir_handle[s] == h) r = SlotBits'(s);
      lookup_slot = r;
    end
  endfunction

  // Lowest free slot. Lowest-index, not round-robin: allocation order must be a
  // pure function of the request sequence for replay to be exact.
  function automatic logic free_any();
    logic f;
    begin
      f = 1'b0;
      for (int unsigned s = 0; s < Slots; s++) if (!dir_live[s]) f = 1'b1;
      free_any = f;
    end
  endfunction

  function automatic logic [SlotBits-1:0] free_slot();
    logic [SlotBits-1:0] r;
    logic taken;
    begin
      r = '0;
      taken = 1'b0;
      for (int unsigned s = 0; s < Slots; s++)
      if (!dir_live[s] && !taken) begin
        r = SlotBits'(s);
        taken = 1'b1;
      end
      free_slot = r;
    end
  endfunction

  // ---- clear sweep (C3) ----------------------------------------------------
  logic                clr_active;
  logic [SlotBits-1:0] clr_slot;
  logic [        11:0] clr_addr;

  // ---- deferred ACQUIRE response (withheld until the sweep finishes) -------
  logic                pend_valid;
  logic [        15:0] pend_src_id;

  // ---- the response register ----------------------------------------------
  logic                pg_valid_q;
  logic [         1:0] pg_op_q;
  logic [         1:0] pg_status_q;
  logic [        15:0] pg_src_id_q;
  logic                pg_is_read_q;
  logic [         7:0] ram_tag_q;
  logic [         7:0] ram_str_q;

  assign pg_valid_o    = pg_valid_q;
  assign pg_op_o       = pg_op_q;
  assign pg_status_o   = pg_status_q;
  assign pg_src_id_o   = pg_src_id_q;
  // Only a served READ returns data. A missed READ answers zero, which is the
  // same value a resident-but-untouched texel answers; the STATUS is what
  // distinguishes them, and a consumer that ignores status gets the fail-safe
  // reading (nothing was stamped there) rather than another patch's scar.
  assign pg_tag_o      = (pg_is_read_q && pg_status_q == StHit) ? ram_tag_q : 8'd0;
  assign pg_strength_o = (pg_is_read_q && pg_status_q == StHit) ? ram_str_q : 8'd0;

  // A response slot is free when it is empty or being drained this cycle.
  wire pg_slot_free = !pg_valid_q || pg_ready_i;

  // Requests are refused while a clear sweep runs (the store is not coherent
  // mid-sweep) and while a deferred response is queued behind one.
  assign req_ready_o   = !clr_active && !pend_valid && pg_slot_free;
  assign wr_ready_o    = !clr_active;

  wire req_fire = req_valid_i && req_ready_o;
  wire wr_fire = wr_valid_i && wr_ready_o;

  // ---- decode of the offered request --------------------------------------
  wire req_hit = lookup_hit(req_handle_i);
  wire [SlotBits-1:0] req_hit_slot = lookup_slot(req_handle_i);
  wire req_free = free_any();
  wire [SlotBits-1:0] req_free_slot = free_slot();

  wire do_acquire_hit = req_fire && req_op_i == OpAcquire && req_hit;
  wire do_acquire_new = req_fire && req_op_i == OpAcquire && !req_hit && req_free;
  wire do_acquire_ovf = req_fire && req_op_i == OpAcquire && !req_hit && !req_free;
  wire do_read_hit = req_fire && req_op_i == OpRead && req_hit;
  wire do_release_hit = req_fire && req_op_i == OpRelease && req_hit;

  // ---- write decode --------------------------------------------------------
  wire wr_hit = lookup_hit(wr_handle_i);
  wire [SlotBits-1:0] wr_hit_slot = lookup_slot(wr_handle_i);
  wire do_write = wr_fire && wr_hit;

  // ---- the one memory process (C5: one read, one write, read-old) ---------
  // Both planes are read and written in ONE always_ff, as before. That is not
  // incidental: nonblocking assignment makes the read see the PRE-write array,
  // which is C5's stated read-during-write semantic, and keeping the shared
  // process is what buys it with NO bypass network. `calib_ram_8192x8_shared_re`
  // measures that this template still infers (see the store's declaration).
  wire [AddrBits-1:0] rd_addr = {req_hit_slot, req_texel_i};
  wire [AddrBits-1:0] wr_addr = clr_active ? {clr_slot, clr_addr} : {wr_hit_slot, wr_texel_i};
  wire [         7:0] wr_tag = clr_active ? 8'd0 : wr_tag_i;
  wire [         7:0] wr_str = clr_active ? 8'd0 : wr_strength_i;
  wire mem_we = clr_active || do_write;
  // Per-PLANE write enables, replacing the byte enables on one 16-bit word.
  // The clear sweep writes both planes; a stamp writes whichever halves it
  // named. Identical behaviour, no part-select, so no byte enable.
  wire mem_we_tag = mem_we && (clr_active || wr_we_tag_i);
  wire mem_we_str = mem_we && (clr_active || wr_we_strength_i);

  always_ff @(posedge clk) begin
    if (do_read_hit) begin
      ram_tag_q <= mem_tag[rd_addr];
      ram_str_q <= mem_str[rd_addr];
    end
    if (mem_we_tag) mem_tag[wr_addr] <= wr_tag;
    if (mem_we_str) mem_str[wr_addr] <= wr_str;
  end

  // ---- control -------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      dir_live <= '0;
      for (int unsigned s = 0; s < Slots; s++) dir_handle[s] <= 32'd0;
      clr_active <= 1'b0;
      clr_slot <= '0;
      clr_addr <= 12'd0;
      pend_valid <= 1'b0;
      pend_src_id <= 16'd0;
      pg_valid_q <= 1'b0;
      pg_op_q <= 2'd0;
      pg_status_q <= 2'd0;
      pg_src_id_q <= 16'd0;
      pg_is_read_q <= 1'b0;
      res_overflow_o <= 1'b0;
      wr_miss_o <= 1'b0;
      wr_miss_src_id_o <= 16'd0;
      surface_texels_touched_o <= 32'd0;
    end else begin
      res_overflow_o <= 1'b0;
      wr_miss_o <= 1'b0;

      if (pg_valid_q && pg_ready_i) pg_valid_q <= 1'b0;

      // --- the clear sweep --------------------------------------------------
      if (clr_active) begin
        clr_addr <= clr_addr + 12'd1;
        if (clr_addr == 12'((Texels - 1))) clr_active <= 1'b0;
      end

      // --- the deferred ACQUIRE answer, emitted once the sweep is done ------
      if (pend_valid && !clr_active && pg_slot_free) begin
        pend_valid   <= 1'b0;
        pg_valid_q   <= 1'b1;
        pg_op_q      <= OpAcquire;
        pg_status_q  <= StAllocated;
        pg_src_id_q  <= pend_src_id;
        pg_is_read_q <= 1'b0;
      end

      // --- request handling -------------------------------------------------
      if (req_fire) begin
        pg_valid_q   <= 1'b1;
        pg_op_q      <= req_op_i;
        pg_src_id_q  <= req_src_id_i;
        pg_is_read_q <= (req_op_i == OpRead);
        pg_status_q  <= StMiss;

        if (do_acquire_hit) begin
          // C3: a resident handle is NOT cleared. This line is persistence.
          pg_status_q <= StHit;
        end else if (do_acquire_new) begin
          dir_live[req_free_slot] <= 1'b1;
          dir_handle[req_free_slot] <= req_handle_i;
          clr_active <= 1'b1;
          clr_slot <= req_free_slot;
          clr_addr <= 12'd0;
          // The answer waits for the sweep; suppress the immediate response.
          pg_valid_q <= 1'b0;
          pend_valid <= 1'b1;
          pend_src_id <= req_src_id_i;
        end else if (do_acquire_ovf) begin
          // C2: nothing is evicted, nothing is written, the caller is told.
          pg_status_q <= StOverflow;
          res_overflow_o <= 1'b1;
        end else if (do_read_hit) begin
          pg_status_q <= StHit;
        end else if (do_release_hit) begin
          dir_live[req_hit_slot] <= 1'b0;
          pg_status_q <= StHit;
        end
      end

      // --- the write port ---------------------------------------------------
      if (do_write) begin
        // The ledger's counter. A texel is "touched" when layer F is written,
        // so a dropped (non-resident) write does NOT count — the counter has to
        // agree with what the sheet holds or it is not observability.
        if (surface_texels_touched_o != 32'hFFFF_FFFF)
          surface_texels_touched_o <= surface_texels_touched_o + 32'd1;
      end
      if (wr_fire && !wr_hit) begin
        wr_miss_o <= 1'b1;
        wr_miss_src_id_o <= wr_src_id_i;
      end
    end
  end

  assign res_occupancy_o = dir_live;
  assign res_busy_o      = clr_active;
  assign idle_o          = !clr_active && !pend_valid && !pg_valid_q;

endmodule
