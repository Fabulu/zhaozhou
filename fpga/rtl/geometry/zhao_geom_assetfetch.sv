// zhao_geom_assetfetch.sv — GEOM.ASSETFETCH.
//
// Contract: design/contracts/GEOM.ASSETFETCH.md
// Oracle:   zref::assetfetch::plan (reference/include/zref/zref_assetfetch.hpp)
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS
// ---------------------------------------------------------------------------
// It reads one meshlet's WHOLE asset footprint out of MEM.GUARD's Phase-3 pool
// (spec/memory_rules.md §5f) as a run of aligned 64-byte lines, then serves the
// two consumers that cannot fetch for themselves:
//
//   GEOM.ASSEMBLE  asks for triplet n and gets three u8 indices
//   GEOM.VDECODE   is streamed each 32-byte vertex record in order
//
// IT BUFFERS RATHER THAN CACHES, AND THE REASON IS THE CONSUMER, NOT SPEED.
// `zhao_geom_assemble.sv`'s index port has `ix_req_o` and `ix_valid_i` and NO
// ready — the port has no way to say "wait", and that block's own comment says
// it deliberately does not buffer ("378 bytes of buffer to save a stream port
// is the wrong trade at this depth"). A cache behind a port that cannot stall
// is not an optimisation, it is a protocol violation waiting for a miss.
//
// The second reason is the one W2.7 paid for: banks come from address bits
// [26:25] and ~82 of 192 Duo lines were starved by two streams sharing one.
// A whole-footprint prefetch is ONE sequential run of lines, so a row stays
// open; a demand-miss stream re-opens rows in an order nobody chose.
//
// ---------------------------------------------------------------------------
// COMPACT SINGLE-BANK CHECKPOINT, ON PURPOSE
// ---------------------------------------------------------------------------
// The owner recovery brief's WP6 orders this migration: prove compact
// single-bank bytes first, then add two banks/lookahead/readers/release. This
// checkpoint keeps the old reader timing and one-fill-engine law while removing
// fetched-line padding from payload RAM. The next checkpoint may add bank
// identity; it must not hide a compaction error inside simultaneous overlap.
`default_nettype none

module zhao_geom_assetfetch
  import zhao_pkg::*;
#(
    // Ruling limits (GEOM.MESHFETCH.md). KNOBS: raising one makes this block
    // bigger, and it will never silently truncate a footprint that does not fit.
    parameter int unsigned MAX_VERTICES  = 64,
    parameter int unsigned MAX_TRIANGLES = 126,
    parameter int unsigned SRCW          = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- one meshlet in, from GEOM.MESHFETCH's result record ---------------
    input  var logic              m_valid_i,
    output var logic              m_ready_o,
    input  var logic [31:0]       m_vertex_offset_i,   // POOL-RELATIVE bytes
    input  var logic [31:0]       m_index_offset_i,    // POOL-RELATIVE bytes
    input  var logic [7:0]        m_vertex_count_i,
    input  var logic [7:0]        m_triangle_count_i,
    input  var logic [SRCW-1:0]   m_src_id_i,
    // The memory-client identity, an input for the same reason MESHFETCH takes
    // one: no block invents which client it is.
    input  var zhao_client_e      m_client_i,

    // ---- MEM.GUARD master --------------------------------------------------
    output var zhao_guard_req_t   guard_req_o,
    input  var zhao_guard_rsp_t   guard_rsp_i,
    input  var logic              beat_valid_i,
    input  var logic [63:0]       beat_data_i,
    input  var logic              beat_last_i,

    // ---- the meshlet is buffered and servable ------------------------------
    output var logic              s_valid_o,
    input  var logic              s_ready_i,
    output var logic [7:0]        s_vertex_count_o,
    output var logic [7:0]        s_triangle_count_o,
    output var logic [SRCW-1:0]   s_src_id_o,
    // The consumer pulses this when it has finished walking the meshlet. It is
    // EXPLICIT rather than inferred from "all vertices streamed and the last
    // triplet asked for", because two consumers finish independently and a
    // buffer released on a guess is a buffer overwritten under a reader.
    input  var logic              release_i,

    // ---- GEOM.ASSEMBLE's index service (request/valid, no ready) -----------
    input  var logic              ix_req_i,
    input  var logic [8:0]        ix_index_i,
    output var logic              ix_valid_o,
    output var logic [7:0]        ix_a_o,
    output var logic [7:0]        ix_b_o,
    output var logic [7:0]        ix_c_o,

    // ---- GEOM.VDECODE's vertex stream --------------------------------------
    output var logic              v_valid_o,
    input  var logic              v_ready_i,
    output var logic [255:0]      v_bytes_o,
    output var logic [SRCW-1:0]   v_src_id_o,

    // ---- counters (design/blocks.yml counter_catalog) ----------------------
    output var logic [31:0]       meshlets_fetched_o,
    output var logic [31:0]       beats_read_o,
    output var logic [31:0]       guard_denied_o,
    output var logic [31:0]       refused_footprint_o,
    output var logic [31:0]       prefetch_stall_o,

    // ---- BEAT PROTOCOL FAULTS (owner recovery brief 11.4, 2026-09-06) -------
    // "Do not make beat_last alone authority for arbitrary received length.
    //  Early last is truncation; late/extra data is a protocol fault. Old RAM
    //  contents cannot supply missing words of a supposedly complete vertex."
    //
    // A 64-byte asset line is EXACTLY eight packed 64-bit words. Until now this
    // block counted beats and believed `beat_last_i` about where the line
    // ended, so a short line produced a meshlet whose missing words were
    // whatever the RAM last held -- a complete-looking vertex record built
    // partly from the previous meshlet. That is a silent wrong picture, not a
    // stall, which is why it needs counters rather than a stall metric.
    //
    // Three DISTINCT counters, not one "errors" total: they have different
    // causes and different fixes. Truncation is the memory or the arbiter
    // ending a burst early; overrun is a burst that did not stop; unowned is a
    // response arriving with no request outstanding, which after the two-bank
    // rework would mean a beat landing in the wrong bank.
    output var logic [31:0]       err_beat_truncated_o,
    output var logic [31:0]       err_beat_overrun_o,
    output var logic [31:0]       err_beat_unowned_o
);

  // -------------------------------------------------------------- sizes ----
  // Mirrors of zref::assetfetch's constants. Named, never inlined: the oracle
  // and the RTL must move together or the differential is comparing two laws.
  localparam int unsigned IX_PER_TRI  = 3;
  localparam int unsigned VX_BYTES    = 32;
  localparam int unsigned LINE_BYTES  = 64;
  localparam int unsigned LINE_WORDS  = LINE_BYTES / 8;    // 8 beats per line
  localparam int unsigned VX_ALIGN    = 32;
  localparam int unsigned IX_ALIGN    = 8;

  // Worst-case spans, DERIVED so they cannot drift from the limits.
  //   index : offset is 8-aligned, so it starts at byte 0..56 of a line;
  //           56 + 378 = 434 bytes -> 7 lines -> 56 words.
  //   vertex: offset is 32-aligned, so it starts at byte 0 or 32;
  //           32 + 2048 = 2080 bytes -> 33 lines -> 264 words.
  localparam int unsigned IX_MAX_BYTES = MAX_TRIANGLES * IX_PER_TRI;
  localparam int unsigned VX_MAX_BYTES = MAX_VERTICES * VX_BYTES;
  // The worst START offset inside a line is LINE_BYTES - ALIGN, not zero: an
  // 8-aligned index stream can begin at byte 56 and still need its whole span.
  localparam int unsigned IX_MAX_BOFF = LINE_BYTES - IX_ALIGN;   // 56
  localparam int unsigned VX_MAX_BOFF = LINE_BYTES - VX_ALIGN;   // 32
  localparam int unsigned IX_LINES = (IX_MAX_BOFF + IX_MAX_BYTES + LINE_BYTES - 1) / LINE_BYTES;
  localparam int unsigned VX_LINES = (VX_MAX_BOFF + VX_MAX_BYTES + LINE_BYTES - 1) / LINE_BYTES;

  // The fetch still covers whole aligned lines, but the RAM stores only useful
  // words from local offset zero (owner recovery brief section 10.2). Keep 64
  // index words even though the default format needs 48: the spare adjacent word
  // makes every two-word triplet read bounded and gives the later banked address
  // a clean six-bit local field. Raising a format limit still grows the array.
  localparam int unsigned IX_COMPACT_WORDS = (IX_MAX_BYTES + 7) / 8;
  localparam int unsigned VX_COMPACT_WORDS = (VX_MAX_BYTES + 7) / 8;
  localparam int unsigned IX_WORDS = (IX_COMPACT_WORDS < 64) ? 64 : IX_COMPACT_WORDS;
  localparam int unsigned VX_WORDS = VX_COMPACT_WORDS;

  localparam int unsigned IXAW = $clog2(IX_WORDS);
  localparam int unsigned VXAW = $clog2(VX_WORDS);

  // ------------------------------------------------------- latched job -----
  logic [31:0]     voff_q, ioff_q;
  logic [7:0]      vcnt_q, tcnt_q;
  // ---- INGRESS CAPTURE (owner recovery brief, COMBINE/ASSETFETCH 2026-09-06)
  // The brief's second prerequisite, verified in source before being touched:
  //
  //   "ASSETFETCH still reads its client identity live after acceptance.
  //    MESHFETCH reads its descriptor address and client live after
  //    acceptance. These must be captured before prefetching makes overlapping
  //    requests ordinary."
  //
  // This is the SAME rule the composed texture island was repaired against --
  // owner recovery v2 Appendix B, "no later computation reads unrelated live
  // ingress data" -- and the same failure mode: the pin is sampled when the
  // request is FORMED, many cycles after the job was accepted, so it carries
  // whatever the caller happens to be presenting then. Today one job is in
  // flight at a time and the caller holds its inputs steady, which is why
  // nothing has broken. Two banks and descriptor lookahead make a second job
  // ordinary, and then it is not steady at all.
  //
  // Captured at acceptance, beside the fields that already were.
  logic [SRCW-1:0] src_q;
  zhao_client_e    client_q;

  // Absolute LINE-ALIGNED bases, and the byte each stream starts at inside its
  // first line. The absolute address is formed HERE and nowhere upstream, so a
  // pool move stays a one-constant edit (contract, "the law").
  logic [ZHAO_VRAM_ADDR_BITS-1:0] ix_line0_q, vx_line0_q;
  logic [5:0]                     ix_boff_q,  vx_boff_q;
  logic [7:0]                     ix_nlines_q, vx_nlines_q;
  // Exact useful length survives line rounding. Index bytes can end partway
  // through the last stored word; transferred suffix bytes never become indices.
  logic [9:0]                     ix_nbytes_q;
  logic [9:0]                     vx_nwords_q;

  // --------------------------------------------------- admission (comb) ----
  // Exactly zref::assetfetch::plan, in the same ORDER, because the taxonomy's
  // order is part of the law and a differing order silently reclassifies
  // counters.
  logic [31:0] ix_bytes_c, vx_bytes_c;
  logic [32:0] ix_abs_c, vx_abs_c;      // 33 bits: the wrap must be VISIBLE
  logic [33:0] ix_end_c, vx_end_c;
  logic        bad_count_c, bad_align_c, bad_pool_c, refuse_c;

  localparam logic [32:0] POOL_BASE = {1'b0, ZHAO_GEOM_ASSET_BASE[31:0]};
  localparam logic [33:0] POOL_END  = {2'b0, ZHAO_GEOM_ASSET_BASE} + {2'b0, ZHAO_GEOM_ASSET_SPAN};

  always_comb begin
    ix_bytes_c = 32'(m_triangle_count_i) * IX_PER_TRI;
    vx_bytes_c = 32'(m_vertex_count_i) * VX_BYTES;

    // WIDE on purpose. The guard's own blit-wrap defect is the standing
    // reminder that `base + span` in exactly 32 bits is where a bounds check
    // stops being one, so these carry the carry out.
    ix_abs_c = {1'b0, ZHAO_GEOM_ASSET_BASE} + {1'b0, m_index_offset_i};
    vx_abs_c = {1'b0, ZHAO_GEOM_ASSET_BASE} + {1'b0, m_vertex_offset_i};
    ix_end_c = {1'b0, ix_abs_c} + {2'b0, ix_bytes_c};
    vx_end_c = {1'b0, vx_abs_c} + {2'b0, vx_bytes_c};

    bad_count_c = (m_vertex_count_i   > 8'(MAX_VERTICES))
               || (m_triangle_count_i > 8'(MAX_TRIANGLES));

    // Refused even when the stream is never read: a meshlet with no triangles
    // still carries an index_offset, and letting a malformed one through
    // because it happens to be unused is a technicality, not a rule.
    bad_align_c = (m_vertex_offset_i[$clog2(VX_ALIGN)-1:0] != '0)
               || (m_index_offset_i[$clog2(IX_ALIGN)-1:0]  != '0);

    bad_pool_c = (ix_abs_c < POOL_BASE) || (ix_end_c > POOL_END)
              || (vx_abs_c < POOL_BASE) || (vx_end_c > POOL_END);

    refuse_c = bad_count_c || bad_align_c || bad_pool_c;
  end

  // How many 64-byte lines cover [addr, addr+bytes)? Zero bytes is zero lines,
  // which is NOT the same as one -- an empty meshlet is legal and must read
  // nothing rather than one stray line.
  function automatic logic [7:0] lines_of(input logic [5:0] boff, input logic [31:0] bytes);
    logic [39:0] span;
    if (bytes == 32'd0) begin
      lines_of = 8'd0;
    end else begin
      span     = {34'd0, boff} + {8'd0, bytes};
      lines_of = 8'((span + 40'(LINE_BYTES) - 40'd1) / 40'(LINE_BYTES));
    end
  endfunction

  // ------------------------------------------------------------ storage ----
  // Two copies of the compact index buffer so a triplet's two words are read in
  // ONE cycle. The default format uses 48 useful words and allocates 64 per copy
  // for bounded adjacent reads and the later bank/address concatenation.
  // Vertices occupy exactly 256 useful words. Whole fetched-line prefixes and
  // suffixes never consume payload RAM.
  logic [63:0] ix_ram_a [0:IX_WORDS-1];
  logic [63:0] ix_ram_b [0:IX_WORDS-1];
  logic [63:0] vx_ram   [0:VX_WORDS-1];

  logic [63:0]      ix_qa, ix_qb, vx_q;
  logic [IXAW-1:0]  ix_ra, ix_rb;
  logic [VXAW-1:0]  vx_ra;
  logic             ix_we, vx_we;
  logic [IXAW-1:0]  ix_wa;
  logic [VXAW-1:0]  vx_wa;
  logic [63:0]      wdata;

  always_ff @(posedge clk) begin
    if (ix_we) begin
      ix_ram_a[ix_wa] <= wdata;
      ix_ram_b[ix_wa] <= wdata;
    end
    if (vx_we) vx_ram[vx_wa] <= wdata;
    ix_qa <= ix_ram_a[ix_ra];
    ix_qb <= ix_ram_b[ix_rb];
    vx_q  <= vx_ram[vx_ra];
  end

  // ------------------------------------------------------------------------
  // THE GUARD'S VERDICT ARRIVES ONE CYCLE AFTER THE ACCEPT (found by D22
  // tread 10, 2026-09-06)
  //
  // This block used to require `guard_rsp_i.ready && guard_rsp_i.ok` IN ONE
  // CYCLE. Against `zhao_mem_guard` that condition can never be true:
  //
  //     rsp.ready = !fwd_active;      // a LEVEL: "the forwarding stage is free"
  //     rsp_ok_q  <= 1'b1;            // a PULSE, the cycle AFTER the accept
  //
  // and the accept is what sets `fwd_active`. So at the accepting cycle ready
  // is 1 and ok is 0; at the verdict cycle ok is 1 and ready is 0. A passing
  // request therefore looked like a DENIAL with no violation flag -- silently,
  // with `guard_denied_o` staying at zero, because the guard only raises
  // `violation` when it actually refuses.
  //
  // It went unseen because every bench PLAYED the guard, and a played
  // responder answers ready and ok together. That is precisely the class of
  // fault D22's staircase exists to find: the harness modelled a protocol the
  // real block does not implement, and everything measured against it agreed.
  //
  // S_VERD is the accepting cycle's successor. The request drops when it is
  // entered -- the guard has already latched it -- and exactly one of ok and
  // violation pulses there.
  //
  // THIS IS A DIVERGENCE, NOT AN AMBIGUITY. Two other guard clients already
  // had it right and had it NAMED: `zhao_raster_fbwrite` waits in `W_VERD`
  // and `zhao_debug_frameblit` in `B_GUARD_VERDICT`, and fbwrite's header
  // even quotes the guard line -- "zhao_mem_guard.sv:186  rsp.ok = rsp_ok_q;
  // verdict 1 cycle after accept". Four clients, two protocols, and the two
  // that were wrong are exactly the two whose memory was played.
  // ---------------------------------------------------------------- FSM ----
  typedef enum logic [2:0] {
    S_IDLE   = 3'd0,
    S_REQ    = 3'd1,   // one aligned 64-byte line offered to the guard
    S_VERD   = 3'd5,   // the guard's verdict, one cycle after it accepted
    S_FILL   = 3'd2,   // its eight beats
    S_HAND   = 3'd3,   // present the buffered meshlet downstream
    S_SERVE  = 3'd4    // answer triplets, stream vertices, await release
  } state_e;

  state_e     st_q;
  logic       phase_q;          // 0 = index stream, 1 = vertex stream
  logic [7:0] line_q;           // line within the current stream
  logic [2:0] beat_q;           // beat within the current line

  // ------------------------------------------------------- the guard port --
  logic [ZHAO_VRAM_ADDR_BITS-1:0] req_addr_c;
  always_comb begin
    req_addr_c = phase_q
        ? (vx_line0_q + ZHAO_VRAM_ADDR_BITS'({line_q, 6'd0}))
        : (ix_line0_q + ZHAO_VRAM_ADDR_BITS'({line_q, 6'd0}));
  end

  // Whole aligned lines only. `be` must equal mask_of(len) or the guard's shape
  // check refuses it, so a full line is the only shape worth asking for -- and
  // it is also the shape that keeps an SDRAM row open.
  // A phase with ZERO lines must not offer a request. An empty index stream is
  // legal (a meshlet with no triangles), and S_REQ is still entered for it to
  // advance the phase -- so the valid is gated on there being a line to ask
  // for, not merely on the state.
  logic req_live_c;
  assign req_live_c = phase_q ? (vx_nlines_q != 8'd0) : (ix_nlines_q != 8'd0);
  assign guard_req_o.valid  = (st_q == S_REQ) && req_live_c;
  assign guard_req_o.write  = 1'b0;             // READ ONLY. The pool admits
                                                // nothing else, by proof.
  assign guard_req_o.client = client_q;   // CAPTURED, not live
  assign guard_req_o.addr   = req_addr_c;
  assign guard_req_o.len    = 7'd64;
  assign guard_req_o.be     = {64{1'b1}};

  // ------------------------------------------------- compact write address --
  // `fill_word_c` is the word within the fetched whole-line run. Both permitted
  // stream alignments are word aligned, so compaction is subtraction, not a byte
  // funnel: discard prefix/suffix words and write useful words from RAM offset 0.
  logic [10:0] fill_word_c, fill_first_c, fill_count_c, fill_rel_c;
  logic [9:0]  ix_nwords_c;
  logic        fill_keep_c;

  always_comb begin
    fill_word_c = {line_q, beat_q};
    ix_nwords_c = (ix_nbytes_q + 10'd7) >> 3;
    if (phase_q) begin
      fill_first_c = {8'd0, vx_boff_q[5:3]};
      fill_count_c = 11'(vx_nwords_q);
    end else begin
      fill_first_c = {8'd0, ix_boff_q[5:3]};
      fill_count_c = 11'(ix_nwords_c);
    end
    fill_rel_c  = fill_word_c - fill_first_c;
    fill_keep_c = (fill_word_c >= fill_first_c)
               && (fill_word_c < fill_first_c + fill_count_c);
  end

  assign wdata = beat_data_i;
  assign ix_we = (st_q == S_FILL) && !phase_q && beat_valid_i && fill_keep_c;
  assign vx_we = (st_q == S_FILL) &&  phase_q && beat_valid_i && fill_keep_c;
  assign ix_wa = IXAW'(fill_rel_c);
  assign vx_wa = VXAW'(fill_rel_c);

  // ------------------------------------------------- the index service -----
  // Triplet n lives at compact byte 3n. Whole-line prefixes were discarded
  // during fill, so every admitted index stream starts at RAM word zero. The
  // three bytes may straddle into the next word, which is why two are read.
  logic [15:0] ix_byte_c;
  logic [8:0]  ix_index_q;
  logic [2:0]  ix_sel_q;
  logic        ix_episode_q;
  logic        ix_read_q;
  logic        ix_pend_q;

  // ASSEMBLE holds ix_req_i throughout S_FETCH. Treat that level as ONE request
  // episode: capture its triplet once, issue one synchronous RAM read and return
  // one pulse. Re-reading it every held cycle happens to be harmless with the
  // current one-cycle consumer, but becomes a queue-corrupting duplicate as soon
  // as banked readers add latency (owner recovery brief section 14.1).
  always_comb begin
    ix_byte_c = 16'(ix_index_q) * 16'(IX_PER_TRI);
    ix_ra     = IXAW'(ix_byte_c[15:3]);
    ix_rb     = IXAW'(ix_byte_c[15:3] + 13'd1);
  end

  // Sixteen bytes, then three of them. A 16:1 byte select is one LUT level per
  // output byte and the straddle disappears into it.
  logic [127:0] ix_pair_c;
  assign ix_pair_c = {ix_qb, ix_qa};

  always_comb begin
    ix_a_o = ix_pair_c[7'({ix_sel_q, 3'd0})          +: 8];
    ix_b_o = ix_pair_c[7'({ix_sel_q, 3'd0}) + 7'd8   +: 8];
    ix_c_o = ix_pair_c[7'({ix_sel_q, 3'd0}) + 7'd16  +: 8];
  end
  assign ix_valid_o = ix_pend_q;

  // ------------------------------------------------ the vertex stream ------
  // A record is 32 bytes = four compact words. Its whole-line prefix was
  // discarded during fill, so vertex zero begins at RAM word zero.
  logic [7:0]   v_ix_q;        // which vertex
  logic [1:0]   v_word_q;      // which of its four words
  logic [255:0] v_acc_q;
  logic         v_full_q;
  logic [2:0]   v_stage_q;     // 0 idle, 1..4 reading, 5 holding

  logic [15:0] v_byte_c;
  always_comb begin
    v_byte_c = 16'(v_ix_q) * 16'(VX_BYTES);
    vx_ra    = VXAW'(v_byte_c[15:3] + 13'(v_word_q));
  end

  assign v_valid_o = v_full_q;
  assign v_bytes_o = v_acc_q;
  assign v_src_id_o = src_q;

  // ------------------------------------------------------- the handshake ---
  assign m_ready_o          = (st_q == S_IDLE);
  assign s_valid_o          = (st_q == S_HAND);
  assign s_vertex_count_o   = vcnt_q;
  assign s_triangle_count_o = tcnt_q;
  assign s_src_id_o         = src_q;

  // ------------------------------------------------------------- control ---
  always_ff @(posedge clk) begin
    if (!rst_n) begin
      st_q      <= S_IDLE;
      phase_q   <= 1'b0;
      line_q    <= '0;
      beat_q    <= '0;
      ix_pend_q <= 1'b0;
      ix_read_q <= 1'b0;
      ix_episode_q <= 1'b0;
      ix_index_q <= '0;
      ix_sel_q  <= '0;
      v_ix_q    <= '0;
      v_word_q  <= '0;
      v_full_q  <= 1'b0;
      v_stage_q <= '0;
      v_acc_q   <= '0;
      voff_q    <= '0;
      ioff_q    <= '0;
      vcnt_q    <= '0;
      tcnt_q    <= '0;
      src_q     <= '0;
      client_q  <= zhao_client_e'(0);
      ix_line0_q  <= '0;
      vx_line0_q  <= '0;
      ix_boff_q   <= '0;
      vx_boff_q   <= '0;
      ix_nlines_q <= '0;
      vx_nlines_q <= '0;
      ix_nbytes_q <= '0;
      vx_nwords_q <= '0;
      meshlets_fetched_o  <= '0;
      beats_read_o        <= '0;
      guard_denied_o      <= '0;
      err_beat_truncated_o <= 32'd0;
      err_beat_overrun_o   <= 32'd0;
      err_beat_unowned_o   <= 32'd0;
      refused_footprint_o <= '0;
      prefetch_stall_o    <= '0;
    end else begin
      // One held legacy request level is one episode, not one request per
      // cycle. Capture first, then issue the synchronous read from that captured
      // index; the caller may change ix_index_i immediately after acceptance.
      ix_pend_q <= ix_read_q;
      ix_read_q <= 1'b0;
      if ((st_q != S_SERVE) || !ix_req_i) ix_episode_q <= 1'b0;
      if ((st_q == S_SERVE) && ix_req_i && !ix_episode_q && !release_i) begin
        ix_index_q   <= ix_index_i;
        ix_episode_q <= 1'b1;
        ix_read_q    <= 1'b1;
      end
      if (ix_read_q) ix_sel_q <= ix_byte_c[2:0];

      // UNOWNED: a beat with no request outstanding. Today this cannot happen
      // -- one logical request is in flight and only S_FILL consumes beats --
      // and it is counted anyway, because the two-bank rework makes "which
      // bank does this word belong to" a real question and a beat arriving
      // between requests would land in whichever bank was last reserved.
      // A counter that reads zero for a whole architecture is the cheapest
      // possible evidence that the next one is still zero.
      if (beat_valid_i && (st_q != S_FILL)) begin
        err_beat_unowned_o <= err_beat_unowned_o + 32'd1;
      end

      // Every cycle a consumer waits on a buffer that is still filling. This is
      // the measurement that decides whether double buffering is worth ~2.4 KB,
      // rather than the assumption that it is.
      if ((st_q == S_REQ || st_q == S_VERD || st_q == S_FILL) &&
          (ix_req_i || v_ready_i)) begin
        prefetch_stall_o <= prefetch_stall_o + 32'd1;
      end

      unique case (st_q)
        S_IDLE: if (m_valid_i) begin
          voff_q <= m_vertex_offset_i;
          ioff_q <= m_index_offset_i;
          vcnt_q <= m_vertex_count_i;
          tcnt_q <= m_triangle_count_i;
          src_q  <= m_src_id_i;
          client_q <= m_client_i;

          if (refuse_c) begin
            // A refused meshlet emits NOTHING -- not the part that fits.
            // Partial geometry reads as a modelling error rather than a fault.
            refused_footprint_o <= refused_footprint_o + 32'd1;
          end else begin
            ix_line0_q  <= ZHAO_VRAM_ADDR_BITS'({ix_abs_c[26:6], 6'd0});
            vx_line0_q  <= ZHAO_VRAM_ADDR_BITS'({vx_abs_c[26:6], 6'd0});
            ix_boff_q   <= ix_abs_c[5:0];
            vx_boff_q   <= vx_abs_c[5:0];
            ix_nlines_q <= lines_of(ix_abs_c[5:0], ix_bytes_c);
            vx_nlines_q <= lines_of(vx_abs_c[5:0], vx_bytes_c);
            ix_nbytes_q <= 10'(ix_bytes_c);
            vx_nwords_q <= 10'(vx_bytes_c >> 3);
            phase_q     <= 1'b0;
            line_q      <= '0;
            beat_q      <= '0;
            // An empty index stream skips straight to the vertices, and a
            // wholly empty meshlet skips straight to being servable.
            st_q <= S_REQ;
          end
        end

        S_REQ: begin
          // Zero lines in this phase: nothing to ask for.
          if ((!phase_q && ix_nlines_q == 8'd0) ||
              ( phase_q && vx_nlines_q == 8'd0)) begin
            if (!phase_q) begin
              phase_q <= 1'b1;
              line_q  <= '0;
            end else begin
              st_q <= S_HAND;
            end
          end else if (guard_rsp_i.ready) begin
            st_q <= S_VERD;   // accepted; the verdict is the NEXT cycle
          end
        end

        S_VERD: begin
          if (guard_rsp_i.ok) begin
            beat_q <= '0;
            st_q   <= S_FILL;
          end else if (guard_rsp_i.violation) begin
            // A denial is not an asset fault: nothing was read, so there is
            // nothing to refuse. The job ends and is counted as its own kind
            // of failure -- and this counter is the one that says whether the
            // Phase-3 region is reaching this block at all.
            guard_denied_o <= guard_denied_o + 32'd1;
            st_q           <= S_IDLE;
          end
          // Neither yet: the guard has not answered. WAIT rather than assume a
          // denial -- reading silence as refusal is the bug this state fixes.
        end

        S_FILL: if (beat_valid_i) begin
          beats_read_o <= beats_read_o + 32'd1;
          beat_q       <= beat_q + 3'd1;
          // TRUNCATION: `last` before the eighth word of the line. The job is
          // abandoned rather than handed on -- a partial meshlet reads as a
          // modelling error, exactly as a refused footprint does, and the
          // block's own rule is that a refused meshlet emits NOTHING rather
          // than the part that fits.
          if (beat_last_i && (beat_q != 3'd7)) begin
            err_beat_truncated_o <= err_beat_truncated_o + 32'd1;
            st_q <= S_IDLE;
          end else if (!beat_last_i && (beat_q == 3'd7)) begin
            // OVERRUN: an eighth word that did not say it was the last, so a
            // ninth is coming. Also abandoned: the line's destination range is
            // full and the next word would land outside it.
            err_beat_overrun_o <= err_beat_overrun_o + 32'd1;
            st_q <= S_IDLE;
          end else if (beat_last_i) begin
            if (!phase_q) begin
              if (line_q + 8'd1 == ix_nlines_q) begin
                phase_q <= 1'b1;
                line_q  <= '0;
              end else begin
                line_q <= line_q + 8'd1;
              end
              st_q <= S_REQ;
            end else begin
              if (line_q + 8'd1 == vx_nlines_q) begin
                st_q <= S_HAND;
              end else begin
                line_q <= line_q + 8'd1;
                st_q   <= S_REQ;
              end
            end
          end
        end

        S_HAND: if (s_ready_i) begin
          meshlets_fetched_o <= meshlets_fetched_o + 32'd1;
          v_ix_q    <= '0;
          v_word_q  <= '0;
          v_full_q  <= 1'b0;
          v_stage_q <= (vcnt_q == 8'd0) ? 3'd0 : 3'd1;
          st_q      <= S_SERVE;
        end

        S_SERVE: begin
          // The vertex stream: four registered reads, then one 256-bit record.
          // Multi-cycle is free here -- GEOM.VDECODE is ready/valid and the
          // records are consumed far slower than they can be assembled.
          if (v_stage_q != 3'd0 && !v_full_q) begin
            if (v_stage_q <= 3'd4) begin
              // vx_q holds the word requested LAST cycle, so stage k stores
              // word k-2. Stage 1 only issues the first address.
              if (v_stage_q >= 3'd2) begin
                v_acc_q[8'({v_stage_q - 3'd2, 6'd0}) +: 64] <= vx_q;
              end
              v_word_q  <= v_word_q + 2'd1;
              v_stage_q <= v_stage_q + 3'd1;
            end else begin
              v_acc_q[192 +: 64] <= vx_q;
              v_full_q  <= 1'b1;
              v_stage_q <= 3'd0;
            end
          end

          if (v_full_q && v_ready_i) begin
            v_full_q <= 1'b0;
            if (v_ix_q + 8'd1 == vcnt_q) begin
              v_ix_q <= v_ix_q;          // stream exhausted; hold
            end else begin
              v_ix_q    <= v_ix_q + 8'd1;
              v_word_q  <= '0;
              v_stage_q <= 3'd1;
            end
          end

          // The consumer says when it is done. Not inferred: two consumers
          // finish independently, and a buffer released on a guess is a buffer
          // overwritten under a reader.
          //
          // THE STREAM STATE IS TORN DOWN WITH IT, and this assignment is LAST
          // on purpose so it overrides the vertex handshake above. A release
          // that arrives before the stream is drained -- which is legal, since
          // ASSEMBLE may refuse every triplet and want nothing more -- would
          // otherwise leave `v_full_q` set, and `v_valid_o` would stay asserted
          // over the next meshlet's buffer with the previous meshlet's record
          // still on the wires. Stale valid is the worst kind of wrong: the
          // consumer has no way to tell it from a fresh one.
          if (release_i) begin
            st_q         <= S_IDLE;
            ix_pend_q    <= 1'b0;
            ix_read_q    <= 1'b0;
            ix_episode_q <= 1'b0;
            v_full_q     <= 1'b0;
            v_stage_q    <= 3'd0;
            v_ix_q       <= '0;
            v_word_q     <= '0;
          end
        end

        default: st_q <= S_IDLE;
      endcase
    end
  end

  // ---------------------------------------------------------- unused -------
  // `beat_last_i` and the response fields are all consumed above; this keeps
  // the linter honest about the ones that are deliberately not.
  // `voff_q`/`ioff_q` are latched for waveform legibility only -- the address
  // arithmetic runs off the incoming offsets, so nothing reads them back.
  logic unused_ok;
  // `v_byte_c[2:0]` is PROVABLY ZERO -- a vertex record is 32-byte aligned
  // inside a 64-byte-aligned pool, which the kMisaligned refusal enforces. It
  // is listed here rather than masked away, and asserted below, because "these
  // bits are always zero" is exactly the kind of claim that stops being true
  // when a limit moves and nobody re-reads the comment.
  assign unused_ok = &{1'b0, voff_q, ioff_q, beat_last_i, v_byte_c[2:0], 1'b0};

`ifndef SYNTHESIS
  // ENFORCED-BY: tests/geometry/assetfetch_rtl_directed.cpp
  always_ff @(posedge clk) begin
    if (rst_n && st_q == S_SERVE) begin
      a_vertex_word_aligned: assert (v_byte_c[2:0] == 3'd0)
        else $error("a vertex record landed unaligned: the kMisaligned refusal did not hold");
    end
  end
`endif

endmodule

`default_nettype wire
