// zhao_texture_cache.sv — TEXTURE.CACHE: the M10K-backed texture / palette /
// material cache, with per-lane tag checks, a single fill port to VRAM and
// the hit/miss counters (phase 5, ZH-061).
//
// Law (in citation order):
//   design/contracts/TEXTURE.CACHE.md — the block contract.
//   design/blocks.yml — `inputs: [miss_addresses]`, `outputs: [cached_texels,
//       fill_requests]`, `upstream: []` (nothing feeds it: it is driven
//       entirely by its access port, which is what makes it exhaustively
//       verifiable), `downstream: [TEXTURE.TMU, MEM.GUARD]`, `backpressure:
//       ready_valid`, `latency: variable`, "1 cache access per clock",
//       counters `cache_hits` + `cache_misses`, and the note this file exists
//       to honour: "The per-frame star ramp pages (spec/stars_and_flares.md
//       §1) are hot palette entries: a palette-page invalidate per upload,
//       never a stale-frame paint."
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §7.3 — "texture tags and lines"
//       and "palette/material caches" are two of the named M10K/MLAB tenants.
//       (The ledger's notes say "M10K budget per §7.3"; spec/memory_rules.md
//       has seven sections and no §7.3 — the M10K tenancy list is CHARTER
//       §7.3, the same correction zhao_raster_tilestore.sv records.)
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md §26 "Never cut: … texture cache".
//   spec/stars_and_flares.md §1 — the 64-entry CLUT palette page is
//       "ARM-rebuilt every frame … Upload ≤512 B/frame + palette-page
//       invalidate". That sentence is why the `inv_*` port exists at all.
//
// ---------------------------------------------------------------------------
// WHAT NUMBER IS LAW HERE, AND WHAT NUMBER IS A CHOICE
// ---------------------------------------------------------------------------
// LAW: that this cache exists, that it is M10K-backed, that it checks tags,
// that it counts hits and misses, that it fetches misses through MEM.GUARD,
// and that a palette upload invalidates the page it replaces. Every one of
// those is written down in the ledger, the charter or stars_and_flares §1.
//
// NOT LAW, AND SAID SO PLAINLY: the CAPACITY. The ledger says "M10K budget
// per §7.3; capacity frozen post-Phase-0" — but charter §7.3 is a tenancy
// LIST with no numbers, spec/memory_rules.md carries the Phase-2 framebuffer
// region map and nothing about M10K at all, and no phase-0 record in this
// repository states a texture-cache size. So the three numbers below are
// CHOSEN, not cited, and they are parameters precisely so that the first
// board-fit measurement replaces them without touching a line of logic:
//
//   LANES = 4, LINES = 16, LINE_BYTES = 16
//     data  = 4 × 16 × 16 B = 1 KiB  (4 × 2,048 bits: one M10K per lane,
//             organised 128 × 16, which is how the halfword port below is
//             shaped)
//     tags  = 4 × 16 × (24 tag + 1 valid) = 1,600 flops
//
// One KiB is a deliberately modest working set: this machine renders into
// 16×16 tiles, so the texel footprint in flight is a tile's worth of one
// surface, and the ledger's own "1 cache access per clock" says the cache is
// a bandwidth filter in front of VRAM rather than a texture pool. Growing
// LINES is a parameter edit; growing it without a board-fit number to point
// at would be inventing a budget, which is the thing this file refuses to do.
//
// ---------------------------------------------------------------------------
// FOUR LANES, AND WHY THAT IS THE SHAPE
// ---------------------------------------------------------------------------
// A lane is a complete direct-mapped cache: its own tag array, its own valid
// bits, its own data RAM. The four lanes are INDEPENDENT — a fill in lane 2
// cannot evict a line in lane 0, and the same line may legally be resident in
// two lanes at once (duplication, never incoherence: this cache is read-only
// and its only writer is its own fill port).
//
// The reason there are four is the bilinear footprint. TEXTURE.TMU's bilinear
// sample reads texels (i,j), (i+1,j), (i,j+1), (i+1,j+1) and the ledger
// requires that to be ONE request ("1 sample per clock (bilinear = 1
// request)"). Four lanes make it one cache ACCESS as well: the TMU drives tap
// k on lane k, so a bilinear sample is a single beat on this port rather than
// four serialised lookups. Because a tap's position within its footprint is
// fixed, each lane sees a stable parity class of texel coordinates as the
// sample point walks across a triangle, so per-lane direct-mapped tags do not
// thrash against each other — which is why a quad-banked texture cache is the
// classical shape and not merely four caches bolted together.
//
// A nearest sample enables lane 0 only (`acc_en_i = 4'b0001`); lanes 1–3 are
// then not looked up, not counted, and not filled.
//
// ---------------------------------------------------------------------------
// THE ACCESS IS ACCEPTED ONLY WHEN IT CAN BE SERVED — the whole miss law
// ---------------------------------------------------------------------------
// There is no miss "response". An access is accepted iff every ENABLED lane
// already holds its line; otherwise `acc_ready_o` stays low and the block
// fills the missing lines, one at a time, through the fill port. The offered
// access — which ready/valid requires the master to hold stable — is
// re-checked combinationally after every fill, so it is accepted the moment
// the last hole closes. With at most four enabled lanes an access takes at
// most four fills, which is why `latency: variable` is the honest ledger
// entry, and why no outstanding-miss queue, no MSHR and no hit-under-miss
// machinery exists here: the master IS the queue, and it is one deep.
//
// The consequence worth stating because a reader will look for it: there is
// no fill bypass. A filled line is written into the RAM and read back out of
// it on the accepting cycle; fill data never shortcuts to `smp_data_o`. That
// costs one cycle per fill and removes an entire class of forwarding bug —
// the RAM is the single source of every texel this block ever emits.
// ENFORCED-BY: tests/texture/texture_cache_directed.cpp:test_miss_then_hit
//
// ---------------------------------------------------------------------------
// THE COUNTERS COUNT THE FIRST LOOK, NOT THE LAST
// ---------------------------------------------------------------------------
// Every access is eventually accepted with all its lanes hitting, so counting
// the tag check AT acceptance would make `cache_hits` equal the lookup count
// and the hit RATE identically 1 — a counter that cannot report the thing it
// is for. So both counters are decided at the FIRST look, PER LANE, ONCE PER
// TRANSACTION:
//
//   cache_hits   += the enabled lanes that were resident when the access was
//                   first offered;
//   cache_misses += the enabled lanes that were not, which is exactly the
//                   number of LINES this access goes on to fetch.
//
// `acct_r` marks the lanes already counted for the access in flight and is
// cleared ONLY at acceptance. It must not be conditioned on `acc_valid_i`:
// a master is entitled to drop that signal while it waits, and bookkeeping
// cleared by it forgets the first look and re-takes it AFTER the fills —
// which reported a hit rate near 1 under exactly that stimulus. Counting the
// misses here rather than at fill start is what makes the pair one instant's
// decision, and therefore timing-independent.
//
// hits + misses is therefore the enabled-lane lookup count, and
// hits / (hits + misses) is the hit rate an engineer means by that phrase.
// ENFORCED-BY: tests/texture/texture_cache_directed.cpp:test_counters
//
// ---------------------------------------------------------------------------
// THE INVALIDATE, AND WHY IT IS A LEDGER REQUIREMENT AND NOT A CONVENIENCE
// ---------------------------------------------------------------------------
// spec/stars_and_flares.md §1 rebuilds each near star's 64-entry CLUT page on
// the ARM EVERY FRAME and re-uploads it. Nothing in that upload path touches
// this block, so a resident copy of the old page would paint frame N+1 with
// frame N's palette — "never a stale-frame paint", and the ledger names the
// palette-ordering capture tests as the tripwire. `inv_valid_i` is therefore
// mandatory, not an extra:
//
//   inv_all_i = 1  — every valid bit in every lane, one cycle (the flush a
//                    resource-epoch change wants);
//   inv_all_i = 0  — the LINE containing `inv_addr_i`, in every lane (the
//                    per-page invalidate; a 512 B palette page is 32 of them).
//
// It is accepted unconditionally, exactly as RASTER.TILESTORE's `clear` is,
// and it is applied LAST in the cycle so that an invalidate racing a
// completing fill wins — the same ordering ruling RASTER.EARLYZ makes for
// `tile_begin`. Invalidating a line a fill is midway through ALSO cancels
// that fill's tag write, so a torn line can never become valid.
// ENFORCED-BY: tests/texture/texture_cache_directed.cpp:test_invalidate_beats_a_fill
//
// ---------------------------------------------------------------------------
// THE HALFWORD PORT
// ---------------------------------------------------------------------------
// A lane returns the 16-bit LITTLE-ENDIAN halfword at `addr & ~1` — never a
// whole line (4 × 128 bits of output for a consumer that wants at most
// 4 × 16), and never a single byte (every direct-colour format this machine
// has is 16 bits per texel: RGB565, ARGB1555, ARGB4444; charter §15). An
// 8-bpp or 4-bpp consumer takes the byte with `addr[0]` and the nibble with
// the texel index's LSB; that selection is TEXTURE.TMU's, because the FORMAT
// is TEXTURE.TMU's — this block stores bytes and has no opinion about them.
// Little-endian because every 16-bit pixel word in this machine already is
// (spec/capture_format.md: "RGB565 LE").
//
// WHAT THIS BLOCK IS NOT: no address generation, no format decode, no palette
// LOOKUP (it caches a palette page like any other bytes; the indexing is
// TEXTURE.TMU's second access), no filtering, no mip selection, no wrap, no
// VRAM protocol beyond a line request and its beats (MEM.GUARD owns the
// region check — this block emits an address and counts), no write path from
// anything but its own fill, no coherence protocol (there is one writer), and
// no eviction POLICY: direct-mapped has none, which is the point.
//
// Conservative SystemVerilog subset only (charter §2); no package deps.
// Lint: clean under `verilator_bin --lint-only -Wall` (lint_texture_cache).

module zhao_texture_cache #(
  // One lane per bilinear tap — see FOUR LANES above. Not meant to vary.
  parameter int unsigned LANES      = 4,
  // Direct-mapped lines per lane, and the line size in bytes. CHOSEN, not
  // cited; see WHAT NUMBER IS LAW HERE.
  parameter int unsigned LINES      = 16,
  parameter int unsigned LINE_BYTES = 16
) (
  input  logic clk,
  input  logic rst_n,

  // ---- miss_addresses in: LANES parallel lookups in ONE access ----------
  // `acc_addr_i` is LANES × 32 bits, lane k at [32*k +: 32]. Byte addresses.
  input  logic                acc_valid_i,
  output logic                acc_ready_o,
  input  logic [LANES-1:0]    acc_en_i,
  input  logic [LANES*32-1:0] acc_addr_i,
  input  logic [15:0]         acc_src_id_i,

  // ---- cached_texels out: one halfword per lane, latency 1 from accept --
  output logic                smp_valid_o,
  input  logic                smp_ready_i,
  output logic [LANES*16-1:0] smp_data_o,
  output logic [15:0]         smp_src_id_o,

  // ---- fill_requests out: MEM.GUARD ------------------------------------
  // One line request, then LINE_BYTES/2 halfword beats in ascending address
  // order. One fill outstanding at a time (see THE ACCESS IS ACCEPTED ONLY
  // WHEN IT CAN BE SERVED), so the beats need no tag and no reordering, and
  // they are never refused — hence no ready on the data direction.
  output logic                fill_valid_o,
  input  logic                fill_ready_i,
  output logic [31:0]         fill_addr_o,
  output logic [15:0]         fill_src_id_o,
  input  logic                fill_data_valid_i,
  input  logic [15:0]         fill_data_i,

  // ---- invalidate: the stars §1 palette-page upload ---------------------
  input  logic                inv_valid_i,
  input  logic                inv_all_i,
  input  logic [31:0]         inv_addr_i,

  // ---- status ----------------------------------------------------------
  output logic                idle_o,

  // ---- counters --------------------------------------------------------
  output logic [31:0]         cache_hits_o,
  output logic [31:0]         cache_misses_o
);

  localparam logic [31:0] CNT_MAX = 32'hFFFF_FFFF;

  localparam int unsigned OFF_W  = $clog2(LINE_BYTES);   // 4
  localparam int unsigned IDX_W  = $clog2(LINES);        // 4
  localparam int unsigned HW_PL  = LINE_BYTES / 2;       // 8 halfwords a line
  localparam int unsigned BEAT_W = $clog2(HW_PL);        // 3
  localparam int unsigned TAG_W  = 32 - OFF_W - IDX_W;   // 24
  localparam int unsigned LANE_W = $clog2(LANES);        // 2

  // ---- the storage -------------------------------------------------------
  // Data: one halfword-wide RAM per lane, addressed {index, beat}. Written by
  // the fill port only, read SYNCHRONOUSLY on the accepting cycle — that is
  // the shape an M10K infers from, and it is why the fill streams halfwords
  // instead of dropping a whole line through eight write ports at once. It is
  // deliberately NOT reset: a reset loop over the data array is exactly what
  // stops M10K inference, and no read can reach it while `valid_r` is 0.
  logic [15:0]      mem_r   [LANES][LINES*HW_PL];
  // Tags and valids MUST be flops, not RAM: `inv_all_i` clears every valid
  // bit in one cycle, and all four lanes' tags are compared in the same cycle
  // the access is offered. Same reasoning as RASTER.TILESTORE's present bits.
  logic [TAG_W-1:0] tag_r   [LANES][LINES];
  logic             valid_r [LANES][LINES];

  // ---- the offered access, decoded --------------------------------------
  logic [TAG_W-1:0]  a_tag  [LANES];
  logic [IDX_W-1:0]  a_idx  [LANES];
  logic [BEAT_W-1:0] a_beat [LANES];
  logic [LANES-1:0]  hit_c;
  logic [LANES-1:0]  need_c;

  always_comb begin
    for (int unsigned k = 0; k < LANES; k++) begin
      a_tag[k]  = acc_addr_i[32*k + OFF_W + IDX_W +: TAG_W];
      a_idx[k]  = acc_addr_i[32*k + OFF_W +: IDX_W];
      a_beat[k] = acc_addr_i[32*k + 1 +: BEAT_W];
      hit_c[k]  = valid_r[k][a_idx[k]] && (tag_r[k][a_idx[k]] == a_tag[k]);
    end
    need_c = acc_en_i & ~hit_c;
  end

  // Bit 0 of every lane address is the BYTE-in-halfword select and belongs to
  // TEXTURE.TMU's format decode, not here; likewise the offset bits of the
  // invalidate address. Sunk explicitly rather than by a lint waiver, in the
  // style of zhao_raster_blend's own `unused_ok`.
  logic unused_ok;
  always_comb begin
    unused_ok = 1'b0;
    for (int unsigned k = 0; k < LANES; k++) unused_ok = unused_ok | acc_addr_i[32*k];
    for (int unsigned b = 0; b < OFF_W; b++) unused_ok = unused_ok | inv_addr_i[b];
    unused_ok = unused_ok & 1'b0;
  end

  // ---- the response stage (this is the latency-1 from acceptance) --------
  logic        s1_v_r;
  logic [15:0] s1_hw_r [LANES];
  logic [15:0] s1_src_r;

  always_comb begin
    for (int unsigned k = 0; k < LANES; k++) smp_data_o[16*k +: 16] = s1_hw_r[k];
  end
  assign smp_valid_o  = s1_v_r;
  assign smp_src_id_o = s1_src_r;

  // ---- the fill engine ---------------------------------------------------
  logic              fill_busy_r;  // a line is being fetched
  logic              fill_req_r;   // ...and its request has not been taken yet
  logic [LANE_W-1:0] fill_lane_r;
  logic [IDX_W-1:0]  fill_idx_r;
  logic [TAG_W-1:0]  fill_tag_r;
  logic [BEAT_W-1:0] fill_beat_r;
  logic              fill_kill_r;  // an invalidate hit this line mid-fill
  logic [15:0]       fill_src_r;

  assign fill_valid_o  = fill_req_r;
  assign fill_addr_o   = {fill_tag_r, fill_idx_r, {OFF_W{1'b0}}};
  assign fill_src_id_o = fill_src_r;

  // ---- flow control ------------------------------------------------------
  // Accept iff nothing is missing, no fill is in flight, and the response
  // stage is free. Hygiene: `acc_ready_o` depends on `smp_ready_i` — the
  // OTHER channel's ready, the permitted direction — and never on
  // `acc_valid_i`.
  logic acc_go;
  assign acc_ready_o = (need_c == {LANES{1'b0}}) && !fill_busy_r && (!s1_v_r || smp_ready_i);
  assign acc_go      = acc_valid_i && acc_ready_o;
  assign idle_o      = !s1_v_r && !fill_busy_r;

  // ---- the first-look accounting (see THE COUNTERS) ----------------------
  // PER LANE, ONCE PER TRANSACTION. `acct_r` marks the lanes already counted
  // for the access in flight; it is set on the first cycle the access is
  // offered and cleared ONLY at acceptance. That is what makes the counters
  // independent of timing: a master is entitled to drop `acc_valid_i` while it
  // waits, and any bookkeeping cleared by that signal forgets the first look
  // and re-takes it after the fills — which reported a hit rate near 1 under
  // exactly that stimulus, twice, before this form replaced it.
  //
  // A transaction runs from an access's first offer to its acceptance.
  // Withdrawing an offered access and substituting a DIFFERENT one before it
  // is accepted is outside what this accounting models — the substitute
  // inherits the withdrawn access's ledger. No master in this machine does
  // that, and ready/valid does not require it to be supported.
  logic [LANES-1:0] acct_r;
  logic [LANES-1:0] to_acct;
  assign to_acct = acc_valid_i ? (acc_en_i & ~acct_r) : {LANES{1'b0}};

  logic [2:0] hits_add, miss_add;
  always_comb begin
    hits_add = 3'd0;
    miss_add = 3'd0;
    for (int unsigned k = 0; k < LANES; k++) begin
      if (to_acct[k] && hit_c[k])  hits_add = hits_add + 3'd1;
      if (to_acct[k] && !hit_c[k]) miss_add = miss_add + 3'd1;
    end
  end

  // ---- which lane to fill next: the LOWEST still missing -----------------
  // Deterministic on purpose: a priority that depended on arrival order would
  // make the VRAM request sequence — which the capture tests compare — depend
  // on backpressure timing.
  logic [LANE_W-1:0] pick_lane;
  logic              pick_any;
  always_comb begin
    pick_lane = {LANE_W{1'b0}};
    pick_any  = 1'b0;
    for (int unsigned k = LANES; k > 0; k--) begin
      if (need_c[k-1]) begin
        pick_lane = LANE_W'(k - 1);
        pick_any  = 1'b1;
      end
    end
  end

  // ---- the invalidate, decoded ------------------------------------------
  logic [IDX_W-1:0] inv_idx;
  logic [TAG_W-1:0] inv_tag;
  assign inv_idx = inv_addr_i[OFF_W +: IDX_W];
  assign inv_tag = inv_addr_i[OFF_W+IDX_W +: TAG_W];

  // ---- sequential --------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1_v_r         <= 1'b0;
      s1_src_r       <= 16'd0;
      fill_busy_r    <= 1'b0;
      fill_req_r     <= 1'b0;
      fill_lane_r    <= {LANE_W{1'b0}};
      fill_idx_r     <= {IDX_W{1'b0}};
      fill_tag_r     <= {TAG_W{1'b0}};
      fill_beat_r    <= {BEAT_W{1'b0}};
      fill_kill_r    <= 1'b0;
      fill_src_r     <= 16'd0;
      acct_r         <= {LANES{1'b0}};
      cache_hits_o   <= 32'd0;
      cache_misses_o <= 32'd0;
      for (int unsigned k = 0; k < LANES; k++) begin
        for (int unsigned i = 0; i < LINES; i++) begin
          valid_r[k][i] <= 1'b0;
          tag_r[k][i]   <= {TAG_W{1'b0}};
        end
        s1_hw_r[k] <= 16'd0;
      end
    end else begin
      // ---- the response stage drains -----------------------------------
      if (s1_v_r && smp_ready_i) s1_v_r <= 1'b0;

      // ---- the first look at each enabled lane, counted once ------------
      if (to_acct != {LANES{1'b0}}) begin
        acct_r <= acct_r | acc_en_i;
        if (cache_hits_o <= (CNT_MAX - 32'(hits_add))) cache_hits_o <= cache_hits_o + 32'(hits_add);
        else                                          cache_hits_o <= CNT_MAX;
        if (cache_misses_o <= (CNT_MAX - 32'(miss_add))) cache_misses_o <= cache_misses_o + 32'(miss_add);
        else                                             cache_misses_o <= CNT_MAX;
      end

      // ---- an access is accepted: the synchronous RAM read --------------
      if (acc_go) begin
        s1_v_r   <= 1'b1;
        s1_src_r <= acc_src_id_i;
        for (int unsigned k = 0; k < LANES; k++) s1_hw_r[k] <= mem_r[k][{a_idx[k], a_beat[k]}];
        acct_r   <= {LANES{1'b0}};
      end

      // ---- a fill starts ------------------------------------------------
      if (!fill_busy_r && acc_valid_i && pick_any) begin
        fill_busy_r <= 1'b1;
        fill_req_r  <= 1'b1;
        fill_kill_r <= 1'b0;
        fill_lane_r <= pick_lane;
        fill_idx_r  <= a_idx[pick_lane];
        fill_tag_r  <= a_tag[pick_lane];
        fill_beat_r <= {BEAT_W{1'b0}};
        fill_src_r  <= acc_src_id_i;  // the request carries the asker's id
        // `cache_misses` is NOT incremented here: it is counted at the first
        // look, above, together with the hits. Counting them at one instant is
        // what makes the pair timing-independent, and it leaves the two equal:
        // a lane that missed at the first look gets exactly one fill.
        // ENFORCED-BY: tests/texture/texture_cache_directed.cpp:test_counters
        // (`misses == fills.size()` asserted directly), and every batch of
        // tests/texture/texture_cache_random.cpp, which diffs the whole
        // fill-request sequence AND both counters against zref::TextureCache.
      end

      // ---- the request is taken -----------------------------------------
      if (fill_req_r && fill_ready_i) fill_req_r <= 1'b0;

      // ---- the beats land -----------------------------------------------
      if (fill_busy_r && !fill_req_r && fill_data_valid_i) begin
        mem_r[fill_lane_r][{fill_idx_r, fill_beat_r}] <= fill_data_i;
        fill_beat_r <= fill_beat_r + {{(BEAT_W-1){1'b0}}, 1'b1};
        if (fill_beat_r == BEAT_W'(HW_PL - 1)) begin
          fill_busy_r <= 1'b0;
          // A line invalidated MID-fill never becomes valid: the bytes on the
          // way in are already stale, so publishing them would be exactly the
          // stale-frame paint stars §1 forbids.
          if (!fill_kill_r) begin
            valid_r[fill_lane_r][fill_idx_r] <= 1'b1;
            tag_r[fill_lane_r][fill_idx_r]   <= fill_tag_r;
          end
        end
      end

      // ---- the invalidate is LAST: it wins over this cycle's fill -------
      if (inv_valid_i) begin
        if (inv_all_i) begin
          for (int unsigned k = 0; k < LANES; k++)
            for (int unsigned i = 0; i < LINES; i++) valid_r[k][i] <= 1'b0;
          if (fill_busy_r) fill_kill_r <= 1'b1;
        end else begin
          for (int unsigned k = 0; k < LANES; k++) valid_r[k][inv_idx] <= 1'b0;
          if (fill_busy_r && (fill_idx_r == inv_idx) && (fill_tag_r == inv_tag))
            fill_kill_r <= 1'b1;
        end
      end
    end
  end

endmodule : zhao_texture_cache
