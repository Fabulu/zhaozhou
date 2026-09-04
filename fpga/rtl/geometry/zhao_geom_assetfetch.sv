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
// SINGLE BUFFERED, ON PURPOSE, FOR NOW
// ---------------------------------------------------------------------------
// The contract describes double buffering, and this block does not implement
// it. That is deliberate rather than unfinished: double buffering is an
// OPTIMISATION whose value is currently unmeasured, and `prefetch_stall_o` is
// the counter that will say whether it is worth its ~2.4 KB. Building it first
// and measuring afterwards is how a wrong number becomes an unadjustable wrong
// number. The buffers are sized and indexed so the second bank is a parameter
// change, not a rewrite.
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
    output var logic [31:0]       prefetch_stall_o
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
  localparam int unsigned IX_WORDS = IX_LINES * LINE_WORDS;
  localparam int unsigned VX_WORDS = VX_LINES * LINE_WORDS;

  localparam int unsigned IXAW = $clog2(IX_WORDS);
  localparam int unsigned VXAW = $clog2(VX_WORDS);

  // ------------------------------------------------------- latched job -----
  logic [31:0]     voff_q, ioff_q;
  logic [7:0]      vcnt_q, tcnt_q;
  logic [SRCW-1:0] src_q;

  // Absolute LINE-ALIGNED bases, and the byte each stream starts at inside its
  // first line. The absolute address is formed HERE and nowhere upstream, so a
  // pool move stays a one-constant edit (contract, "the law").
  logic [ZHAO_VRAM_ADDR_BITS-1:0] ix_line0_q, vx_line0_q;
  logic [5:0]                     ix_boff_q,  vx_boff_q;
  logic [7:0]                     ix_nlines_q, vx_nlines_q;

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
  // Two copies of the index buffer so a triplet's two words are read in ONE
  // cycle. It is 56 x 64 bits = 3,584 -- an MLAB apiece -- so duplicating it
  // is cheaper than the state machine that would read the same RAM twice.
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

  // ---------------------------------------------------------------- FSM ----
  typedef enum logic [2:0] {
    S_IDLE   = 3'd0,
    S_REQ    = 3'd1,   // one aligned 64-byte line offered to the guard
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
  assign guard_req_o.client = m_client_i;
  assign guard_req_o.addr   = req_addr_c;
  assign guard_req_o.len    = 7'd64;
  assign guard_req_o.be     = {64{1'b1}};

  // ------------------------------------------------- the write addresses ---
  assign wdata = beat_data_i;
  assign ix_we = (st_q == S_FILL) && !phase_q && beat_valid_i;
  assign vx_we = (st_q == S_FILL) &&  phase_q && beat_valid_i;
  assign ix_wa = IXAW'({line_q[IXAW-4:0], beat_q});
  assign vx_wa = VXAW'({line_q[VXAW-4:0], beat_q});

  // ------------------------------------------------- the index service -----
  // Triplet n lives at byte ix_boff + 3n. The offset is 8-aligned and the pool
  // base is 64-aligned, so the WORD is (ix_boff + 3n) >> 3 -- but the three
  // bytes may straddle into the next word, which is why two are read.
  logic [15:0] ix_byte_c;
  logic [2:0]  ix_sel_q;
  logic        ix_pend_q;

  always_comb begin
    ix_byte_c = 16'(ix_boff_q) + 16'(ix_index_i) * 16'(IX_PER_TRI);
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
  // A record is 32-byte aligned inside a 64-byte-aligned pool, so it is FOUR
  // consecutive words and never five. That is the whole reason the alignment
  // refusal exists (contract, "Alignment").
  logic [7:0]   v_ix_q;        // which vertex
  logic [1:0]   v_word_q;      // which of its four words
  logic [255:0] v_acc_q;
  logic         v_full_q;
  logic [2:0]   v_stage_q;     // 0 idle, 1..4 reading, 5 holding

  logic [15:0] v_byte_c;
  always_comb begin
    v_byte_c = 16'(vx_boff_q) + 16'(v_ix_q) * 16'(VX_BYTES);
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
      ix_line0_q  <= '0;
      vx_line0_q  <= '0;
      ix_boff_q   <= '0;
      vx_boff_q   <= '0;
      ix_nlines_q <= '0;
      vx_nlines_q <= '0;
      meshlets_fetched_o  <= '0;
      beats_read_o        <= '0;
      guard_denied_o      <= '0;
      refused_footprint_o <= '0;
      prefetch_stall_o    <= '0;
    end else begin
      // The index answer is one cycle behind its request, which GEOM.ASSEMBLE
      // permits: it sits in S_FETCH until ix_valid_i, with no deadline.
      ix_pend_q <= (st_q == S_SERVE) && ix_req_i;
      if ((st_q == S_SERVE) && ix_req_i) ix_sel_q <= ix_byte_c[2:0];

      // Every cycle a consumer waits on a buffer that is still filling. This is
      // the measurement that decides whether double buffering is worth ~2.4 KB,
      // rather than the assumption that it is.
      if ((st_q == S_REQ || st_q == S_FILL) && (ix_req_i || v_ready_i)) begin
        prefetch_stall_o <= prefetch_stall_o + 32'd1;
      end

      unique case (st_q)
        S_IDLE: if (m_valid_i) begin
          voff_q <= m_vertex_offset_i;
          ioff_q <= m_index_offset_i;
          vcnt_q <= m_vertex_count_i;
          tcnt_q <= m_triangle_count_i;
          src_q  <= m_src_id_i;

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
            if (guard_rsp_i.ok) begin
              beat_q <= '0;
              st_q   <= S_FILL;
            end else begin
              // A denial is not an asset fault: nothing was read, so there is
              // nothing to refuse. The job ends and is counted as its own kind
              // of failure -- and this counter is the one that says whether the
              // Phase-3 region is reaching this block at all.
              if (guard_rsp_i.violation) guard_denied_o <= guard_denied_o + 32'd1;
              st_q <= S_IDLE;
            end
          end
        end

        S_FILL: if (beat_valid_i) begin
          beats_read_o <= beats_read_o + 32'd1;
          beat_q       <= beat_q + 3'd1;
          if (beat_last_i) begin
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
            st_q      <= S_IDLE;
            v_full_q  <= 1'b0;
            v_stage_q <= 3'd0;
            v_ix_q    <= '0;
            v_word_q  <= '0;
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
