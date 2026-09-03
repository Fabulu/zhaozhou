// zhao_texture_cache_pipe.sv — the texture cache probe, C0..C4, with the
// arrays read SYNCHRONOUSLY so they can be memory.
//
// BESIDE `zhao_texture_cache.sv`, which stays the golden implementation.
// Nothing instantiates this yet.
//
// ---------------------------------------------------------------------------
// WHAT WAS WRONG, MEASURED FROM TWO DIRECTIONS AT ONCE
// ---------------------------------------------------------------------------
// The previous version of this file called itself staged and read its tag,
// valid and data arrays COMBINATIONALLY, classifying from those reads inside
// one clock. Ruling X7:
//
//   > the source calls itself staged but reads tag/data arrays combinationally
//   > and classifies from those reads; there is no explicit M10K output
//   > capture stage before broad compare/select ... the expected M10K
//   > inference and timing seam are unproved.
//
// The fit proved it, twice over:
//
//   81.06 MHz     worst internal path  rq_rp[1] -> valid_r[1][2]   12.159 ns
//   5,634 ALM     10,812 REGISTERS     3 M10K
//
// Ten thousand registers is the data array. `data_r` is 4 lanes x 16 lines x
// 8 halfwords x 16 bits = 8,192 bits, and an asynchronously-read array cannot
// be a memory block, so every bit of it became a flip-flop with a 128-way
// read mux hanging off it. That is why the block is both large and slow, and
// it is one cause with two symptoms rather than two problems.
//
// X7 is explicit about what does NOT count as fixing it:
//
//   > Do not accept a cache fit as architectural closure if the RAMs become
//   > flops/MLABs or an M10K output launches a broad combinational path.
//
// ---------------------------------------------------------------------------
// THE STAGES, AS RULED
// ---------------------------------------------------------------------------
//   C0  local request FIFO                         `acc_ready_o` reads it alone
//   C1  register lane tag/index/beat, issue the synchronous RAM addresses
//   C2  capture the RAM outputs into fabric flops
//   C3  compare, classify, choose ONE miss identity and `fill_lane_mask`
//   C4  response FIFO / miss sequencer
//
// C2 exists solely so that nothing broad is computed from a memory output in
// the same clock it appears. It looks like a wasted stage and it is the whole
// point of the rebuild.
//
// ---------------------------------------------------------------------------
// A PIPELINE NEEDS A REPLAY, AND THAT IS THE REAL COST
// ---------------------------------------------------------------------------
// The old design re-evaluated the FIFO head every clock, so a miss simply kept
// looking until the fill landed. A pipeline cannot do that: by the time C3
// says "miss", C1 and C2 already hold the two requests behind it.
//
// So there are TWO pointers into the request FIFO. `rq_ip` issues; `rq_rp`
// retires. A miss at C3 rewinds `rq_ip` to `rq_rp` and squashes what is in
// flight, and the fill engine then runs. Nothing is lost because nothing was
// popped -- a request is only removed when it has fully hit.
//
// The all-hit path still accepts and retires ONE ACCESS PER CLOCK, which is
// what TEXTURE.TMU's II=2 sample rate rests on. A miss costs the pipeline
// depth on top of the fill, and misses were always the expensive case.
//
// ---------------------------------------------------------------------------
// KEPT, BECAUSE THE RULING SAYS TO KEEP THEM
// ---------------------------------------------------------------------------
//   > Keep multicast and one blocking miss.
//
// One line is fetched once and written into EVERY lane that wanted it. One
// miss is outstanding at a time.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_cache_pipe #(
    parameter int unsigned LANES      = 4,
    parameter int unsigned LINES      = 16,
    parameter int unsigned LINE_BYTES = 16,
    // Local request FIFO. Must be a power of two: the occupancy is a pointer
    // subtraction with one spare bit, which only counts correctly if the
    // pointers wrap at a multiple of the depth. The old file hard-coded
    // `logic [1:0]` pointers and a `3'(REQN)` compare while calling REQN a
    // parameter -- X7's "REQN is nominal while pointer/count widths are
    // hard-coded for four entries". Every width below is derived.
    parameter int unsigned REQN       = 4
) (
    input var logic clk,
    input var logic rst_n,

    // ---- access --------------------------------------------------------------
    input  var logic                acc_valid_i,
    output var logic                acc_ready_o,
    input  var logic [LANES-1:0]    acc_en_i,
    input  var logic [LANES*32-1:0] acc_addr_i,
    input  var logic [15:0]         acc_src_id_i,

    // ---- response ------------------------------------------------------------
    output var logic                smp_valid_o,
    input  var logic                smp_ready_i,
    output var logic [LANES*16-1:0] smp_data_o,
    output var logic [15:0]         smp_src_id_o,

    // ---- fill ----------------------------------------------------------------
    output var logic                fill_valid_o,
    input  var logic                fill_ready_i,
    output var logic [31:0]         fill_addr_o,
    input  var logic                fill_data_valid_i,
    input  var logic [15:0]         fill_data_i,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]         cache_hits_o,
    output var logic [31:0]         cache_misses_o,
    output var logic [31:0]         fills_o,        // LINE fetches, not lane misses
    output var logic [31:0]         multicast_o,    // lanes served by one fill
    output var logic [31:0]         replays_o       // probes squashed by a miss
);

  localparam int unsigned OFF_W  = $clog2(LINE_BYTES);   // 4
  localparam int unsigned IDX_W  = $clog2(LINES);        // 4
  localparam int unsigned HW_PL  = LINE_BYTES / 2;       // 8
  localparam int unsigned BEAT_W = $clog2(HW_PL);        // 3
  localparam int unsigned TAG_W  = 32 - OFF_W - IDX_W;   // 24
  localparam int unsigned DAW    = IDX_W + BEAT_W;       // data-array address
  localparam int unsigned RQW    = $clog2(REQN);

  // ==========================================================================
  // STORAGE
  // ==========================================================================
  // `data_r` is READ ONLY THROUGH A REGISTERED ADDRESS, below, and never in a
  // continuous assignment. That is HALF the difference between an M10K and
  // 8,192 flip-flops.
  //
  // THE OTHER HALF, and the one this file got wrong until 2026-09-03: the
  // WRITE must also live in a CLOCK-ONLY process. An M10K has no reset port,
  // so an array written from an `always_ff @(posedge clk or negedge rst_n)`
  // cannot be one -- whether or not it appears in the reset branch. This file
  // had correct synchronous reads and put both writes in the async-reset
  // process, and measured `blockMemoryBits: 128` against the 8,192 of the
  // block it replaced: 9,728 bits of array sitting in flip-flops.
  //
  // The block it replaced had already found this and written it down --
  // `zhao_texture_cache.sv:495-523` records the A/B, including that making the
  // lane index static changed NOTHING (5,402 -> 5,373 ALM, zero M10K both
  // times) because the async reset was the real blocker. That note was not
  // read. See reports/QUARTUS_GOTCHAS.md S10.
  //
  // `tag_r` is read the same way. `valid_r` stays in flops on purpose: it is
  // 4 x 16 = 64 bits, it needs a reset that a memory block cannot give it, and
  // putting it in memory would mean a line could read valid before the fill
  // engine had cleared it.
  // ONE FLAT ARRAY PER LANE, IN A GENERATE -- the OTHER half of the fix.
  //
  // Moving the writes to a clock-only process (2026-09-03) was necessary and
  // NOT sufficient. The fit came back with 2 M10K and 128 memory bits again,
  // and synthesis said why:
  //
  //     EDA Netlist Writer cannot regroup multidimensional array "data_r"
  //     Found 1 instances of uninferred RAM logic
  //
  // with no "Inferred RAM" line for data_r or tag_r at all.
  //
  // A `[LANES][N]` unpacked array is not a memory Quartus can map. It has to
  // build a mux across every lane, and the whole array falls into flip-flops
  // however the writes are written.
  //
  // THE BLOCK THIS REPLACES SAYS EXACTLY THIS, at zhao_texture_cache.sv:237,
  // with the measurement: "5,402 ALMs, 9,993 registers, ZERO M10K, zero memory
  // bits -- a cache made entirely of flops". I read that file's SECOND
  // explanation (the async reset, :495) and implemented only that one. The
  // first explanation was thirty lines earlier.
  //
  // So the lane index becomes STATIC: a genvar picks the array and the write
  // enable is decoded per lane. Same values, same cycle, same everything the
  // differential checks -- only the inference changes.
  genvar gl;
  generate
    for (gl = 0; gl < int'(LANES); gl++) begin : g_lane
      // Deliberately NOT reset: a reset loop over the data array is itself a
      // thing that stops M10K inference, and no read can reach it while
      // `valid_r` is 0.
      logic [15:0]      data_r [LINES * HW_PL];
      logic [TAG_W-1:0] tag_r  [LINES];
    end
  endgenerate

  // `valid_r` stays a flat flop array: 4 x 16 = 64 bits, it NEEDS the reset a
  // memory cannot give, and putting it in memory would let a line read valid
  // before the fill engine had cleared it.
  logic             valid_r [LANES][LINES];

  // ==========================================================================
  // C0 — the local request FIFO
  // ==========================================================================
  // `acc_ready_o` reads THIS and nothing else: not the consumer, not the fill
  // engine. That is the property that lets the TMU issue without knowing
  // anything about cache state.
  logic [LANES-1:0]    rq_en   [REQN];
  logic [LANES*32-1:0] rq_addr [REQN];
  logic [15:0]         rq_src  [REQN];
  logic [RQW:0]        rq_wp, rq_rp, rq_ip;   // write / retire / ISSUE

  logic [RQW:0] rq_n;
  assign rq_n = rq_wp - rq_rp;
  assign acc_ready_o = (rq_n != (RQW+1)'(REQN));

  logic rq_issuable;
  assign rq_issuable = (rq_wp != rq_ip);

  // ==========================================================================
  // C1 — decode the issued request, drive the RAM addresses
  // ==========================================================================
  logic [TAG_W-1:0]  i_tag  [LANES];
  logic [IDX_W-1:0]  i_idx  [LANES];
  logic [BEAT_W-1:0] i_beat [LANES];
  always_comb begin
    for (int unsigned k = 0; k < LANES; k++) begin
      i_tag[k]  = rq_addr[rq_ip[RQW-1:0]][32*k + OFF_W + IDX_W +: TAG_W];
      i_idx[k]  = rq_addr[rq_ip[RQW-1:0]][32*k + OFF_W +: IDX_W];
      i_beat[k] = rq_addr[rq_ip[RQW-1:0]][32*k + 1 +: BEAT_W];
    end
  end

  logic              c1_v;
  logic [LANES-1:0]  c1_en;
  logic [15:0]       c1_src;
  logic [TAG_W-1:0]  c1_tag  [LANES];
  logic [IDX_W-1:0]  c1_idx  [LANES];

  // ==========================================================================
  // C2 — the RAM outputs, captured
  // ==========================================================================
  // The memory's OWN output register. It updates on every clock, because a
  // synchronous RAM read cannot be conditional without becoming an enable that
  // some devices will not infer -- so its contents are only meaningful for the
  // address presented on the previous clock, and `c2_*` below is what holds
  // them still.
  logic [TAG_W-1:0]  ram_tag [LANES];
  logic              ram_val [LANES];
  logic [15:0]       ram_dat [LANES];

  logic              c2_v;
  logic [LANES-1:0]  c2_en;
  logic [15:0]       c2_src;
  logic [TAG_W-1:0]  c2_tag  [LANES];   // carried, to compare against
  logic [IDX_W-1:0]  c2_idx  [LANES];
  logic [TAG_W-1:0]  c2_rtag [LANES];   // captured FROM the tag array
  logic              c2_rval [LANES];
  logic [15:0]       c2_rdat [LANES];   // captured FROM the data array

  // ==========================================================================
  // C3 — compare and classify
  // ==========================================================================
  logic [LANES-1:0] c3_hit_c, c3_need_c;
  always_comb begin
    for (int unsigned k = 0; k < LANES; k++)
      c3_hit_c[k] = c2_rval[k] && (c2_rtag[k] == c2_tag[k]);
    c3_need_c = c2_en & ~c3_hit_c;
  end

  // ONE line, and every lane that wants it. The lowest-numbered needing lane
  // names the line; every other needing lane whose (tag, idx) matches joins
  // the mask and is written by the same beats.
  logic [TAG_W-1:0] m_tag_c;
  logic [IDX_W-1:0] m_idx_c;
  logic [LANES-1:0] m_mask_c;
  logic             m_any_c;
  always_comb begin
    m_any_c  = 1'b0;
    m_tag_c  = '0;
    m_idx_c  = '0;
    m_mask_c = '0;
    for (int unsigned k = 0; k < LANES; k++)
      if (!m_any_c && c3_need_c[k]) begin
        m_any_c = 1'b1;
        m_tag_c = c2_tag[k];
        m_idx_c = c2_idx[k];
      end
    if (m_any_c)
      for (int unsigned k = 0; k < LANES; k++)
        if (c3_need_c[k] && c2_tag[k] == m_tag_c && c2_idx[k] == m_idx_c)
          m_mask_c[k] = 1'b1;
  end

  logic [RQW+1:0] mask_pop_c, en_pop_c;
  always_comb begin
    mask_pop_c = '0;
    en_pop_c   = '0;
    for (int unsigned k = 0; k < LANES; k++) begin
      mask_pop_c = mask_pop_c + (RQW+2)'(m_mask_c[k]);
      en_pop_c   = en_pop_c   + (RQW+2)'(c2_en[k]);
    end
  end

  // ==========================================================================
  // C4 — response FIFO and miss sequencer
  // ==========================================================================
  logic [LANES*16-1:0] rs_data [REQN];
  logic [15:0]         rs_src  [REQN];
  logic [RQW:0]        rs_wp, rs_rp;
  logic [RQW:0]        rs_n;
  assign rs_n = rs_wp - rs_rp;

  assign smp_valid_o  = (rs_n != '0);
  assign smp_data_o   = rs_data[rs_rp[RQW-1:0]];
  assign smp_src_id_o = rs_src[rs_rp[RQW-1:0]];

  logic rs_room;
  assign rs_room = (rs_n != (RQW+1)'(REQN));

  logic              fb_busy_r, fb_req_r;
  logic [TAG_W-1:0]  fb_tag_r;
  logic [IDX_W-1:0]  fb_idx_r;
  logic [BEAT_W-1:0] fb_beat_r;
  logic [LANES-1:0]  fb_mask_r;

  assign fill_valid_o = fb_req_r;
  assign fill_addr_o  = {fb_tag_r, fb_idx_r, {OFF_W{1'b0}}};

  // C3 resolves in order, so the request it describes is always `rq_rp`.
  logic c3_all_hit, c3_retire, c3_miss;
  assign c3_all_hit = c2_v && (c3_need_c == '0);
  assign c3_retire  = c3_all_hit && rs_room;
  assign c3_miss    = c2_v && m_any_c && !fb_busy_r;

  // A probe may issue when there is something to issue, no miss is being
  // handled, and the response FIFO could take the result. Holding issue on
  // `rs_room` keeps the pipe from producing results it cannot place.
  logic c1_go;
  assign c1_go = rq_issuable && !fb_busy_r && !c3_miss && rs_room;

  // Read addresses. Registered, which is what makes the arrays memory.
  logic [IDX_W-1:0] rd_idx  [LANES];
  logic [DAW-1:0]   rd_daddr[LANES];
  always_comb begin
    for (int unsigned k = 0; k < LANES; k++) begin
      rd_idx[k]   = i_idx[k];
      rd_daddr[k] = {i_idx[k], i_beat[k]};
    end
  end

  // ==========================================================================
  // THE BANK PORTS LIVE INSIDE THE GENERATE, one per lane.
  //
  // `g_lane[k]` with `k` a loop variable is not a constant selection -- a
  // generate instance can only be picked by a genvar. That is not a syntax
  // inconvenience, it IS the point: a lane chosen by a register is exactly the
  // dynamic selection that forces the mux and kills inference. Putting the
  // port inside the generate makes the lane static by construction.
  //
  // No reset on any of it, deliberately: a memory block's output register
  // cannot be asynchronously reset, and asking for one is how an inferred RAM
  // quietly becomes flops. `c2_v` gates their use and IS reset.
  generate
    for (gl = 0; gl < int'(LANES); gl++) begin : g_lane_port
      always_ff @(posedge clk) begin
        // Read through a REGISTERED address -- the other half of the M10K
        // shape, and the half this file already had right.
        ram_tag[gl] <= g_lane[gl].tag_r[rd_idx[gl]];
        ram_dat[gl] <= g_lane[gl].data_r[rd_daddr[gl]];

        // Write, with the lane DECODED rather than indexed. Behaviour is
        // identical to the shared loop it replaces: still non-blocking, so a
        // read and a write to one address on one edge still returns the OLD
        // contents, which is the read-during-write mode an M10K provides.
        if (fb_busy_r && fill_data_valid_i && fb_mask_r[gl]) begin
          g_lane[gl].data_r[{fb_idx_r, fb_beat_r}] <= fill_data_i;
          if (fb_beat_r == BEAT_W'(HW_PL - 1)) begin
            g_lane[gl].tag_r[fb_idx_r] <= fb_tag_r;
          end
        end
      end
    end
  endgenerate

  // `valid_r` is a flat flop array and its read stays here: it is 64 bits, it
  // needs the reset a memory cannot give, and it is read on the SAME edge as
  // the tag. Reading it one stage later -- which the first version of this
  // file did -- samples the tag at clock T and the valid bit at T+1, and
  // during a fill those two disagree: the fill clears valid at the start and
  // sets it at the last beat, so a probe could see the OLD tag with the NEW
  // valid and call a miss a hit.
  always_ff @(posedge clk) begin
    for (int unsigned k = 0; k < LANES; k++) begin
      ram_val[k] <= valid_r[k][rd_idx[k]];
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rq_wp <= '0; rq_rp <= '0; rq_ip <= '0;
      rs_wp <= '0; rs_rp <= '0;
      c1_v <= 1'b0;
      c2_v <= 1'b0;
      fb_busy_r <= 1'b0;
      fb_req_r  <= 1'b0;
      cache_hits_o   <= 32'd0;
      cache_misses_o <= 32'd0;
      fills_o        <= 32'd0;
      multicast_o    <= 32'd0;
      replays_o      <= 32'd0;
      for (int unsigned k = 0; k < LANES; k++)
        for (int unsigned i = 0; i < LINES; i++) valid_r[k][i] <= 1'b0;
    end else begin
      // ---- C0: accept ------------------------------------------------------
      if (acc_valid_i && acc_ready_o) begin
        rq_en[rq_wp[RQW-1:0]]   <= acc_en_i;
        rq_addr[rq_wp[RQW-1:0]] <= acc_addr_i;
        rq_src[rq_wp[RQW-1:0]]  <= acc_src_id_i;
        rq_wp <= rq_wp + (RQW+1)'(1);
      end

      // ---- C1: issue -------------------------------------------------------
      c1_v <= c1_go;
      if (c1_go) begin
        c1_en  <= rq_en[rq_ip[RQW-1:0]];
        c1_src <= rq_src[rq_ip[RQW-1:0]];
        for (int unsigned k = 0; k < LANES; k++) begin
          c1_tag[k] <= i_tag[k];
          c1_idx[k] <= i_idx[k];
        end
        rq_ip <= rq_ip + (RQW+1)'(1);
      end

      // ---- C2: CAPTURE the memory outputs into fabric flops ----------------
      // This is the stage X7 asks for by name, and it looks like a wasted
      // clock until you notice what it prevents: without it the compare in C3
      // hangs directly off a memory output, which is the "M10K output launches
      // a broad combinational path" the ruling refuses.
      //
      // It is also a correctness requirement, not only a timing one. `ram_*`
      // is overwritten on EVERY clock by whatever address C1 is presenting
      // now; holding it still for one stage is the only reason C3 sees the
      // request it thinks it sees.
      c2_v <= c1_v;
      if (c1_v) begin
        c2_en  <= c1_en;
        c2_src <= c1_src;
        for (int unsigned k = 0; k < LANES; k++) begin
          c2_tag[k]  <= c1_tag[k];
          c2_idx[k]  <= c1_idx[k];
          c2_rtag[k] <= ram_tag[k];
          c2_rval[k] <= ram_val[k];
          c2_rdat[k] <= ram_dat[k];
        end
      end

      // ---- C3/C4: retire an all-hit probe ----------------------------------
      if (c3_retire) begin
        for (int unsigned k = 0; k < LANES; k++)
          rs_data[rs_wp[RQW-1:0]][16*k +: 16] <= c2_rdat[k];
        rs_src[rs_wp[RQW-1:0]] <= c2_src;
        rs_wp <= rs_wp + (RQW+1)'(1);
        rq_rp <= rq_rp + (RQW+1)'(1);
        // ONE add of the enabled-lane popcount, never one per lane in a loop:
        // four nonblocking increments all read the same old value and only the
        // last lands.
        cache_hits_o <= cache_hits_o + 32'(en_pop_c);
      end
      if (smp_valid_o && smp_ready_i) rs_rp <= rs_rp + (RQW+1)'(1);

      // ---- C3: a miss REWINDS the issue pointer and squashes the pipe ------
      // Nothing is lost: a request is only removed from the FIFO when it has
      // fully hit, so rewinding to `rq_rp` re-probes exactly the requests that
      // had not yet retired.
      if (c3_miss) begin
        fb_busy_r <= 1'b1;
        fb_req_r  <= 1'b1;
        fb_tag_r  <= m_tag_c;
        fb_idx_r  <= m_idx_c;
        fb_mask_r <= m_mask_c;
        fb_beat_r <= '0;
        fills_o   <= fills_o + 32'd1;
        cache_misses_o <= cache_misses_o + 32'(mask_pop_c);
        multicast_o    <= multicast_o + 32'(mask_pop_c) - 32'd1;
        for (int unsigned k = 0; k < LANES; k++)
          if (m_mask_c[k]) valid_r[k][m_idx_c] <= 1'b0;

        rq_ip <= rq_rp;
        c1_v  <= 1'b0;
        c2_v  <= 1'b0;
        replays_o <= replays_o + 32'd1;
      end

      // ---- the fill engine, unchanged in behaviour -------------------------
      if (fb_busy_r) begin
        if (fb_req_r && fill_ready_i) fb_req_r <= 1'b0;
        if (fill_data_valid_i) begin
          // `data_r` and `tag_r` are written by the CLOCK-ONLY process above,
          // under these same conditions. They are not written here because an
          // array touched by an asynchronously-reset process cannot infer as
          // an M10K. `valid_r` stays: it is 64 bits and it needs the reset.
          if (fb_beat_r == BEAT_W'(HW_PL - 1)) begin
            for (int unsigned k = 0; k < LANES; k++)
              if (fb_mask_r[k]) valid_r[k][fb_idx_r] <= 1'b1;
            fb_busy_r <= 1'b0;
          end
          fb_beat_r <= fb_beat_r + BEAT_W'(1);
        end
      end
    end
  end

endmodule : zhao_texture_cache_pipe

`default_nettype wire
