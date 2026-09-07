// zhao_terrain_pagestream.sv -- the resident page slot, as a lattice.
//
// ===========================================================================
// THE HOLE THIS FILLS, MEASURED BY THE COMPOSED SUITE
// ===========================================================================
// `tests/terrain/world_composed_directed.cpp` phase C reports, and has reported
// since 2026-09-07, that the assembled world layer NEVER CALLS A LOADED PAGE
// RESIDENT: a claim sets `mips_stale`, so the loader's `fin` parks the entry in
// ST_MIPGEN, and only a SECOND completion moves it to RESIDENT_CLEAN -- the
// only state a lookup hits on. Eight pages were fetched, CRC-verified and
// byte-identical in their slots while `resident_o` stayed at ZERO.
//
// That finding's CAUSE has already moved once, which is worth recording here
// because it is why this block exists rather than a smaller change. The first
// description said `zhao_terrain_mipgen` had no slot, generation, epoch or
// completion port. True when written; false within hours, because those ports
// landed the same day. The hole is one step further back: MIPGEN consumes a
// 33x33 lattice through `fine_valid_i`, `TERRAIN.PATCH` consumes the same
// lattice through `base_i`/`scar_i`/`bottom_i`, and NOTHING in `fpga/rtl` turns
// a resident page slot into either stream.
//
// So: a block, not a wire, and this is it.
//
// ===========================================================================
// WHAT IT READS, AND WHY THE THREE LAYERS ARE READ TOGETHER
// ===========================================================================
// `spec/terrain_rules.md` section 2, Island Patch v1, page-relative offsets:
//
//     header  0      64 B
//     A  top base height    33x33 height16   2,178 B  at    64
//     B  top scar delta     33x33 height16   2,178 B  at 2,242
//     C  bottom height      33x33 height16   2,178 B  at 4,420
//
// The consumer wants ONE VERTEX AT A TIME with all three planes present --
// TERRAIN.PATCH's compose lane takes `base_i`, `scar_i` and `bottom_i` on the
// same beat -- and the three planes are 2,178 bytes apart in the page. Three
// cursors advance in lockstep, one per layer, each with its own 64-byte staging
// buffer, and a vertex is emitted when all three have their sample.
//
// The alternative -- read layer A whole, then B, then C -- needs 1,089 x 16
// bits of buffering per layer held across the other two passes. That is 17,424
// bits, and the whole point of doing it this way is that it needs 1,536.
//
// ===========================================================================
// A SAMPLE NEVER STRADDLES ANYTHING, AND THAT IS ARITHMETIC, NOT LUCK
// ===========================================================================
// TERRAIN.WRITEBACK had to re-lane layer F because F_OFF = 10,694 and
// 10,694 mod 8 = 6, so its 8,192-byte sheet begins six bytes into a 64-bit
// word. The same worry applies here and the answer is different, so it is
// written out rather than assumed:
//
//   * All three offsets are EVEN (64, 2242, 4420) and the element is 2 bytes,
//     so sample k of layer L sits at page byte `O_L + 2k`, which is even.
//   * An even byte address holds a 16-bit value entirely within one 64-bit
//     word: the byte lane is 0, 2, 4 or 6, and 6 + 2 = 8 fits exactly.
//   * The same argument at 64-byte granularity: lane is 0..62, and 62 + 2 = 64
//     fits the staging buffer exactly.
//
// So no sample crosses a word boundary and no sample crosses a burst boundary.
// The buffer's coverage is a plain address compare, not a re-laner. B and C
// still START mid-burst -- 2242 mod 64 = 2, 4420 mod 64 = 4 -- but that only
// means the first burst of those layers has 62 and 60 useful bytes, which the
// address compare handles without knowing it.
//
// It is checked at elaboration rather than trusted, because a layout change
// that made an offset odd would silently return the two halves of two
// neighbouring vertices.
//
// ===========================================================================
// WHAT IT DOES NOT DO, AND WHO OWNS EACH ONE
// ===========================================================================
// IT DOES NOT COMPOSE. `spec/terrain_rules.md` section 3.4 --
//
//     compose_top = max( fx(base) + fx(scar), fx(bottom) )
//
// -- is TERRAIN.PATCH's law and `zref::terrain::compose_vertex` is its oracle.
// This block hands over the three planes untouched. A second implementation of
// a saturating clamp is a second thing to keep in step with section 3.4, and
// its first divergence would be ground that is subtly in the wrong place with
// every counter agreeing.
//
// IT DOES NOT CHOOSE MIPGEN'S SURFACE, AND THAT IS AN OPEN RULING.
// Owner ruling T8 gives the mip law exactly -- `mip17[i,j] = fine33[2i, 2j]` --
// and says a page becomes RESIDENT only after "resident mips complete". It does
// NOT say which surface `fine33` is at load time. The candidates are not
// equivalent:
//
//   * layer A alone: a scarred patch would show unscarred ground at coarse LOD.
//   * `compose_top` (A + B clamped at C): correct-looking, but it is section
//     3.4 arithmetic, which this block deliberately does not own.
//
// So the choice is NOT made here. This block emits all three planes and the
// selection sits in whatever adapter feeds MIPGEN, where it is one visible
// wire instead of a decision buried in a datapath. Flagged in the contract as
// requiring an owner ruling; inventing it here is exactly the fault this tree
// keeps recording.
//
// IT DOES NOT DECIDE RESIDENCY, does not publish, does not verify the page CRC
// (TERRAIN.PAGELOADER did that before the page was ever called loaded), and
// does not place the lattice in the world -- `wx`/`wz` come from the island
// directory and the patch's own envelope, not from these three layers.
// ===========================================================================
`default_nettype none

module zhao_terrain_pagestream
  import zhao_pkg::*;
#(
    parameter int unsigned PAGE_BYTES = 21376,   // terrain_rules 2 / 7

    // The three plane offsets and the lattice edge. Parameters because the
    // layout is versioned, NOT because they are tuning knobs: change one and
    // the elaboration checks below have to keep agreeing.
    parameter int unsigned A_OFF = 64,
    parameter int unsigned B_OFF = 2242,
    parameter int unsigned C_OFF = 4420,
    parameter int unsigned EDGE  = 33,

    parameter logic [ZHAO_VRAM_ADDR_BITS-1:0] REGION_BASE  = 27'h400_0000,
    parameter int unsigned                    REGION_SLOTS = 1024,

    // ONE BIT WIDER THAN THE POOL NEEDS, the same way every other pool client
    // is: a computed slot of 1,024 must arrive as a REFUSAL rather than as slot
    // 0 with somebody else's page in it.
    parameter int unsigned SLOTW = $clog2(REGION_SLOTS) + 1,
    parameter int unsigned GENW  = 8,

    // The established read shape: one request at len 64 returns eight 64-bit
    // beats. zhao_scanout_fetch set it, zhao_geom_meshfetch copied it,
    // zhao_terrain_writeback copied it. A fourth spelling of the same thing is
    // a fourth thing that can be subtly different.
    parameter int unsigned BURST_BYTES = 64
) (
    input var logic clk,
    input var logic rst_n,

    // ---- configuration -------------------------------------------------------
    input var zhao_client_e cfg_vram_client_i,
    input var logic [31:0]  cfg_epoch_i,

    // ---- job in ---------------------------------------------------------------
    input  var logic             j_valid_i,
    output var logic             j_ready_o,
    input  var logic [SLOTW-1:0] j_slot_i,
    input  var logic [GENW-1:0]  j_gen_i,
    input  var logic [31:0]      j_epoch_i,
    input  var logic [31:0]      j_src_id_i,

    // ---- MEM.GUARD read client ------------------------------------------------
    output var zhao_guard_req_t guard_req_o,
    input  var zhao_guard_rsp_t guard_rsp_i,
    input  var logic            beat_valid_i,
    input  var logic [63:0]     beat_data_i,
    input  var logic            beat_last_i,

    // ---- the lattice out ------------------------------------------------------
    // Row-major, `vi` outer, `vj` inner. THE ORDER IS THE ADDRESS: `vi_o`/`vj_o`
    // are carried anyway because TERRAIN.PATCH's subpatch mask needs them, but
    // nothing downstream may derive position from anything except this stream's
    // order, for the same reason MIPGEN's fine port carries no coordinate.
    output var logic                v_valid_o,
    input  var logic                v_ready_i,
    output var logic signed [15:0]  v_base_o,     // layer A
    output var logic signed [15:0]  v_scar_o,     // layer B
    output var logic signed [15:0]  v_bottom_o,   // layer C
    output var logic [5:0]          v_vi_o,       // 0..EDGE-1
    output var logic [5:0]          v_vj_o,
    output var logic                v_first_o,    // vertex 0 of this lattice
    output var logic                v_last_o,     // vertex EDGE*EDGE-1
    output var logic [SLOTW-1:0]    v_slot_o,
    output var logic [GENW-1:0]     v_gen_o,
    output var logic [31:0]         v_epoch_o,
    output var logic [31:0]         v_src_id_o,

    // ---- completion -----------------------------------------------------------
    // ONE JOB, ONE COMPLETION, ALWAYS -- the rule TERRAIN.PAGELOADER's contract
    // states and for the same reason. A refusal that produced silence would
    // leave the directory's entry parked forever with nothing counted, which is
    // the exact failure this whole block exists to remove.
    output var logic             done_valid_o,
    input  var logic             done_ready_i,
    output var logic [SLOTW-1:0] done_slot_o,
    output var logic [GENW-1:0]  done_gen_o,
    output var logic [31:0]      done_epoch_o,
    output var logic             done_ok_o,
    output var logic [3:0]       done_verdict_o,
    output var logic [31:0]      done_src_id_o,

    // ---- counters -------------------------------------------------------------
    output var logic [31:0] lattices_streamed_o,
    output var logic [31:0] lattices_refused_o,
    output var logic [31:0] vertices_streamed_o,
    output var logic [31:0] bursts_read_o,
    output var logic [31:0] guard_denied_o,
    output var logic [31:0] incomplete_o,
    output var logic        idle_o
);

  localparam int unsigned VERTS      = EDGE * EDGE;             // 1,089
  localparam int unsigned PLANE_BYTES = VERTS * 2;              // 2,178
  localparam int unsigned BEATS      = BURST_BYTES / 8;         // 8
  localparam int unsigned LANEW      = $clog2(BURST_BYTES);     // 6
  localparam int unsigned VIDXW      = $clog2(VERTS + 1);       // 11

  // The three planes, indexed rather than named, because every one of them is
  // read the same way and three copies of the cursor logic is three places for
  // them to drift apart.
  localparam int unsigned NPLANE = 3;
  localparam int unsigned PLANE_OFF [NPLANE] = '{A_OFF, B_OFF, C_OFF};

  localparam logic [3:0] V_OK        = 4'd0;
  localparam logic [3:0] V_SLOT_OOR  = 4'd1;   // slot >= REGION_SLOTS
  localparam logic [3:0] V_EPOCH     = 4'd2;   // job epoch != cfg_epoch_i
  localparam logic [3:0] V_GUARD     = 4'd3;   // MEM.GUARD refused a read
  localparam logic [3:0] V_INCOMPLETE = 4'd4;  // a burst returned short

`ifndef SYNTHESIS
  // THE ALIGNMENT ARGUMENT, ENFORCED. See the header: the no-straddle property
  // is arithmetic about EVEN offsets, and a layout revision that made one odd
  // would return the two halves of two neighbouring vertices with every other
  // signal agreeing. Refused at elaboration, the same way TERRAIN.WRITEBACK
  // refuses a sheet that does not begin in its chunk's first beat.
  initial begin
    if ((A_OFF % 2) != 0 || (B_OFF % 2) != 0 || (C_OFF % 2) != 0)
      $fatal(1, "pagestream: a plane offset is odd (%0d, %0d, %0d) -- a 16-bit sample would straddle a word", A_OFF, B_OFF, C_OFF);
    if ((A_OFF + PLANE_BYTES) > B_OFF || (B_OFF + PLANE_BYTES) > C_OFF)
      $fatal(1, "pagestream: the planes overlap");
    if ((C_OFF + PLANE_BYTES) > PAGE_BYTES)
      $fatal(1, "pagestream: plane C runs past the page");
    if ((BURST_BYTES % 8) != 0)
      $fatal(1, "pagestream: BURST_BYTES must be whole 64-bit beats");
  end
`endif

  // slot * PAGE_BYTES with no multiplier, the same shift-and-add
  // TERRAIN.WRITEBACK uses: 21,376 = 2^14 + 2^12 + 2^9 + 2^8 + 2^7, five shifts
  // and four adders, no DSP. Copied deliberately rather than shared, because a
  // shared helper would be a package dependency for eight lines of arithmetic;
  // if a third block wants it, THEN it becomes a function somewhere.
  function automatic logic [31:0] slot_scaled(input logic [SLOTW-1:0] s);
    logic [31:0] acc;
    begin
      acc = 32'd0;
      for (int unsigned b = 0; b < 32; b++) begin
        if (((PAGE_BYTES >> b) & 32'd1) != 32'd0) begin
          acc = acc + ({{(32 - SLOTW) {1'b0}}, s} << b);
        end
      end
      slot_scaled = acc;
    end
  endfunction

  // ---- the job, latched ----------------------------------------------------
  logic [SLOTW-1:0] job_slot_q;
  logic [GENW-1:0]  job_gen_q;
  logic [31:0]      job_epoch_q, job_src_q;
  logic [31:0]      page_base_q;   // byte address of the slot's page

  // ---- the three cursors ---------------------------------------------------
  // `buf_q[p]` holds one 64-byte burst of plane p; `cov_q[p]` is the
  // page-relative, BURST-ALIGNED byte address it covers; `covv_q[p]` says it
  // holds anything at all.
  logic [(BURST_BYTES*8)-1:0] buf_q [NPLANE];
  logic [31:0]                cov_q [NPLANE];
  logic                       covv_q [NPLANE];

  logic [VIDXW-1:0] vidx_q;        // 0 .. VERTS-1
  logic [5:0]       vi_q, vj_q;

  // What page byte plane p's sample for the current vertex lives at, and the
  // burst that contains it.
  function automatic logic [31:0] want_byte(input logic [1:0] p, input logic [VIDXW-1:0] k);
    want_byte = 32'(PLANE_OFF[p]) + (32'({{(32-VIDXW){1'b0}}, k}) << 1);
  endfunction

  logic [31:0] want_c [NPLANE];
  logic [31:0] wantal_c [NPLANE];
  logic        need_c [NPLANE];

  always_comb begin
    for (int unsigned p = 0; p < NPLANE; p++) begin
      want_c[p]   = want_byte(2'(p), vidx_q);
      // PARENTHESISED, AND THAT IS NOT STYLE. `~32'(BURST_BYTES - 1)` is
      // QUARTUS_GOTCHAS 4b: the parser reads the operator and the size as one
      // token and chokes on the quote --
      //     Error (10170): syntax error ... near text: "'"; expecting ";"
      // -- and the fit came back `incomplete:failed:quartus_map` in 30 seconds.
      // That gotcha was already written down, with nine instances listed in
      // four other files, and this file was written without reading it. The
      // meta-lesson at the bottom of that document is exactly this mistake:
      // "read this file before touching RTL that has never been through the
      // fitter -- the blocks that break are always the ones only Verilator has
      // ever seen."
      wantal_c[p] = want_c[p] & (~(32'(BURST_BYTES - 1)));
      need_c[p]   = !covv_q[p] || (cov_q[p] != wantal_c[p]);
    end
  end

  // The plane that must be refilled next, lowest index first.
  //
  // FIXED PRIORITY IS NOT A CLAIM THAT ONLY ONE PLANE IS EVER SHORT. The
  // machine returns to S_CHECK after EVERY refill and asks again, so any number
  // of cold planes -- one, or all three at vertex 0 -- is served one burst at a
  // time until none is short, and a vertex is emitted only when the answer is
  // none. That is what makes the order here a matter of which goes first rather
  // than of correctness, and it is why no round-robin is needed.
  // ENFORCED-BY: fpga/rtl/terrain/zhao_terrain_pagestream.sv:a_emit_has_all_planes
  logic [1:0] refill_c;
  logic       any_need_c;
  always_comb begin
    refill_c   = 2'd0;
    any_need_c = 1'b0;
    for (int unsigned p = 0; p < NPLANE; p++) begin
      if (!any_need_c && need_c[p]) begin
        refill_c   = 2'(p);
        any_need_c = 1'b1;
      end
    end
  end

  // ---- extraction ----------------------------------------------------------
  // The sample's byte lane inside the staging buffer. `want & (BURST_BYTES-1)`
  // is 0..62 and even, so `lane*8 +: 16` is always inside the buffer -- see the
  // header's arithmetic, and the elaboration check that enforces it.
  function automatic logic signed [15:0] sample_of(input logic [1:0] p);
    logic [LANEW-1:0] lane;
    begin
      lane = LANEW'(want_c[p]);
      sample_of = signed'(buf_q[p][({{(32-LANEW){1'b0}}, lane} * 8) +: 16]);
    end
  endfunction

  // ---- the machine ---------------------------------------------------------
  typedef enum logic [2:0] {
    S_IDLE,
    S_CHECK,
    S_REQ,
    S_VERD,
    S_WAIT,
    S_EMIT,
    S_DONE
  } state_e;

  state_e state_q;

  logic [1:0]           fill_p_q;      // which plane the in-flight burst is for
  logic [$clog2(BEATS+1)-1:0] beat_q;
  logic [31:0]          fill_addr_q;   // page-relative aligned address in flight
  logic [3:0]           verdict_q;

  assign j_ready_o = (state_q == S_IDLE);
  assign idle_o    = (state_q == S_IDLE);

  assign v_valid_o  = (state_q == S_EMIT);
  assign v_base_o   = sample_of(2'd0);
  assign v_scar_o   = sample_of(2'd1);
  assign v_bottom_o = sample_of(2'd2);
  assign v_vi_o     = vi_q;
  assign v_vj_o     = vj_q;
  assign v_first_o  = (vidx_q == VIDXW'(0));
  assign v_last_o   = (vidx_q == VIDXW'(VERTS - 1));
  assign v_slot_o   = job_slot_q;
  assign v_gen_o    = job_gen_q;
  assign v_epoch_o  = job_epoch_q;
  assign v_src_id_o = job_src_q;

  assign done_valid_o   = (state_q == S_DONE);
  assign done_slot_o    = job_slot_q;
  assign done_gen_o     = job_gen_q;
  assign done_epoch_o   = job_epoch_q;
  assign done_ok_o      = (verdict_q == V_OK);
  assign done_verdict_o = verdict_q;
  assign done_src_id_o  = job_src_q;

  // THE REFUSAL MUST NOT ISSUE THE READ IT IS REFUSING. Same shape as
  // TERRAIN.WRITEBACK's `ident_stop`: `guard_req_o.valid` is gated on the
  // state, so a job that failed its pre-checks never reaches the guard at all
  // and `guard_denied_o` stays a measurement of the GUARD rather than of this
  // block's own bookkeeping.
  always_comb begin
    guard_req_o        = '0;
    guard_req_o.valid  = (state_q == S_REQ);
    guard_req_o.write  = 1'b0;
    guard_req_o.client = cfg_vram_client_i;
    guard_req_o.addr   = ZHAO_VRAM_ADDR_BITS'(page_base_q + fill_addr_q);
    guard_req_o.len    = 7'(BURST_BYTES);
    guard_req_o.be     = '1;
  end

  logic pre_slot_bad_c, pre_epoch_bad_c;
  assign pre_slot_bad_c  = (32'({{(32-SLOTW){1'b0}}, j_slot_i}) >= 32'(REGION_SLOTS));
  assign pre_epoch_bad_c = (j_epoch_i != cfg_epoch_i);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state_q             <= S_IDLE;
      job_slot_q          <= '0;
      job_gen_q           <= '0;
      job_epoch_q         <= 32'd0;
      job_src_q           <= 32'd0;
      page_base_q         <= 32'd0;
      vidx_q              <= '0;
      vi_q                <= 6'd0;
      vj_q                <= 6'd0;
      fill_p_q            <= 2'd0;
      beat_q              <= '0;
      fill_addr_q         <= 32'd0;
      verdict_q           <= V_OK;
      lattices_streamed_o <= 32'd0;
      lattices_refused_o  <= 32'd0;
      vertices_streamed_o <= 32'd0;
      bursts_read_o       <= 32'd0;
      guard_denied_o      <= 32'd0;
      incomplete_o        <= 32'd0;
      for (int unsigned p = 0; p < NPLANE; p++) begin
        buf_q[p]  <= '0;
        cov_q[p]  <= 32'd0;
        covv_q[p] <= 1'b0;
      end
    end else begin
      unique case (state_q)

        S_IDLE: begin
          if (j_valid_i) begin
            job_slot_q  <= j_slot_i;
            job_gen_q   <= j_gen_i;
            job_epoch_q <= j_epoch_i;
            job_src_q   <= j_src_id_i;
            page_base_q <= 32'(REGION_BASE) + slot_scaled(j_slot_i);
            vidx_q      <= '0;
            vi_q        <= 6'd0;
            vj_q        <= 6'd0;
            // EVERY BUFFER IS INVALIDATED PER JOB, not carried. A second job
            // for the SAME slot could legitimately reuse them, and a second job
            // for a different slot absolutely could not -- and the difference
            // is one comparison this block would then have to get right on
            // every path. The cost of always refilling is three bursts.
            for (int unsigned p = 0; p < NPLANE; p++) covv_q[p] <= 1'b0;
            if (pre_slot_bad_c) begin
              verdict_q          <= V_SLOT_OOR;
              lattices_refused_o <= lattices_refused_o + 32'd1;
              state_q            <= S_DONE;
            end else if (pre_epoch_bad_c) begin
              verdict_q          <= V_EPOCH;
              lattices_refused_o <= lattices_refused_o + 32'd1;
              state_q            <= S_DONE;
            end else begin
              verdict_q <= V_OK;
              state_q   <= S_CHECK;
            end
          end
        end

        S_CHECK: begin
          if (any_need_c) begin
            fill_p_q    <= refill_c;
            fill_addr_q <= wantal_c[refill_c];
            beat_q      <= '0;
            state_q     <= S_REQ;
          end else begin
            state_q <= S_EMIT;
          end
        end

        S_REQ: begin
          // TWO CYCLES, LEVEL THEN PULSE -- MEM.GUARD's protocol, which
          // `zhao_mem_guard.sv` states outright: `rsp.ready` is a LEVEL
          // (`!fwd_active`) and `rsp.ok` a PULSE one cycle after the accept. A
          // block that waited for both on the same cycle waits forever, and a
          // block that treated the LEVEL as the verdict would start counting
          // beats for a request the guard was about to refuse.
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            verdict_q      <= V_GUARD;
            state_q        <= S_DONE;
          end else if (guard_rsp_i.ready) begin
            state_q <= S_VERD;
          end
        end

        S_VERD: begin
          // The pulse cycle. `ok` and `violation` are the two answers and
          // exactly one arrives; waiting here rather than in S_WAIT keeps the
          // beat counter from ever being armed for a refused read.
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            verdict_q      <= V_GUARD;
            state_q        <= S_DONE;
          end else if (guard_rsp_i.ok) begin
            state_q <= S_WAIT;
          end
        end

        S_WAIT: begin
          if (guard_rsp_i.violation) begin
            guard_denied_o <= guard_denied_o + 32'd1;
            verdict_q      <= V_GUARD;
            state_q        <= S_DONE;
          end else if (beat_valid_i) begin
            buf_q[fill_p_q][({{(32-$clog2(BEATS+1)){1'b0}}, beat_q} * 64) +: 64] <= beat_data_i;
            if (beat_last_i) begin
              // A BURST THAT ENDED EARLY IS NOT A BURST. `beat_last_i` before
              // the eighth beat means the fabric gave up mid-transfer, and the
              // buffer then holds a mixture of this page and the last one --
              // which reads as terrain, not as an error, which is why it is
              // counted and FAULTED rather than tolerated.
              if (beat_q != ($clog2(BEATS+1))'(BEATS - 1)) begin
                incomplete_o <= incomplete_o + 32'd1;
                verdict_q    <= V_INCOMPLETE;
                state_q      <= S_DONE;
              end else begin
                cov_q[fill_p_q]  <= fill_addr_q;
                covv_q[fill_p_q] <= 1'b1;
                bursts_read_o    <= bursts_read_o + 32'd1;
                state_q          <= S_CHECK;
              end
            end else begin
              beat_q <= beat_q + ($clog2(BEATS+1))'(1);
            end
          end
        end

        S_EMIT: begin
          if (v_ready_i) begin
            vertices_streamed_o <= vertices_streamed_o + 32'd1;
            if (vidx_q == VIDXW'(VERTS - 1)) begin
              lattices_streamed_o <= lattices_streamed_o + 32'd1;
              state_q             <= S_DONE;
            end else begin
              vidx_q <= vidx_q + VIDXW'(1);
              if (vj_q == 6'(EDGE - 1)) begin
                vj_q <= 6'd0;
                vi_q <= vi_q + 6'd1;
              end else begin
                vj_q <= vj_q + 6'd1;
              end
              state_q <= S_CHECK;
            end
          end
        end

        S_DONE: begin
          if (done_ready_i) state_q <= S_IDLE;
        end

        default: state_q <= S_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // ENFORCED-BY: tests/terrain/pagestream_rtl_directed.cpp
  logic armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) armed_q <= 1'b0;
    else armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (armed_q) begin
      a_emit_has_all_planes :
      assert (!(state_q == S_EMIT) || (covv_q[0] && covv_q[1] && covv_q[2]))
      else $error("pagestream: emitted a vertex with a cold plane buffer");

      a_vidx_bounded :
      assert (vidx_q < VIDXW'(VERTS))
      else $error("pagestream: vertex index %0d out of range", vidx_q);

      a_index_agrees :
      assert (!(state_q == S_EMIT)
              || ({{(32-VIDXW){1'b0}}, vidx_q} == (32'(vi_q) * 32'(EDGE) + 32'(vj_q))))
      else $error("pagestream: vi/vj disagree with the scan index");
    end
  end
`endif

endmodule
