// zhao_texture_cache_pipe_v2.sv — Packet-E typed-termination cache pipe.
//
// This versioned successor keeps Packet B's C0..C4 lookup, multicast,
// synchronous-RAM shape, response timing, and successful data path. Packet E
// makes the reserved fill refusal functional, carries a held response status,
// and makes the blocking miss's response reservation explicit so success and
// refusal can terminate without either losing or recreating that credit.
//
// The unversioned cache remains the executable old-island oracle.
// ENFORCED-BY: tests/texture/texture_cache_pipe_v2_directed.cpp
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

// Committed Packet-E mutants override these two expressions immediately before
// this exact source. Ordinary source lists see only the production defaults.
`ifndef ZHAO_PACKET_E_REFUSAL_NEXT_IP
  `define ZHAO_PACKET_E_REFUSAL_NEXT_IP(rp, one) ((rp) + (one))
`endif
`ifndef ZHAO_PACKET_E_ISSUE_OWNS_FRESH_RESV
  `define ZHAO_PACKET_E_ISSUE_OWNS_FRESH_RESV(prepaid) (!(prepaid))
`endif

module zhao_texture_cache_pipe_v2 #(
    parameter int unsigned LANES      = 4,
    parameter int unsigned LINES      = 16,
    parameter int unsigned LINE_BYTES = 16,
    // Local request FIFO. Must be a power of two: the occupancy is a pointer
    // subtraction with one spare bit, which only counts correctly if the
    // pointers wrap at a multiple of the depth. The old file hard-coded
    // `logic [1:0]` pointers and a `3'(REQN)` compare while calling REQN a
    // parameter -- X7's "REQN is nominal while pointer/count widths are
    // hard-coded for four entries". Every width below is derived.
    parameter int unsigned REQN       = 4,
    // ---- SOURCE-ID WIDTH, added for P0-C ------------------------------------
    // This width used to be a literal 16 in four places: both ports and both
    // queue arrays. P0-C makes the island's routing token 18 bits -- v3own's
    // 16-bit sample handle plus a 2-bit class -- because today's SRCW is
    // EXACTLY FULL at slot width 4 (the pad term in island_top's `plan_src_id`
    // is `SRCW-2-$clog2(DEPTH)-2-GENW` = 0), so a 6-bit slot has no slack to
    // grow into.
    //
    // V2 is new and has no 16-bit legacy instantiation: its identity is exactly
    // {class[1:0], sample_handle[15:0]} by default.
    parameter int unsigned SRCW       = 18
) (
    input var logic clk,
    input var logic rst_n,

    // ---- access --------------------------------------------------------------
    input  var logic                acc_valid_i,
    output var logic                acc_ready_o,
    input  var logic [LANES-1:0]    acc_en_i,
    input  var logic [LANES*32-1:0] acc_addr_i,
    input  var logic [SRCW-1:0]     acc_src_id_i,

    // ---- response ------------------------------------------------------------
    output var logic                smp_valid_o,
    input  var logic                smp_ready_i,
    output var logic [LANES*16-1:0] smp_data_o,
    output var logic [7:0]          smp_status_o,
    output var logic [SRCW-1:0]     smp_src_id_o,

    // ---- fill ----------------------------------------------------------------
    output var logic                fill_valid_o,
    input  var logic                fill_ready_i,
    output var logic [31:0]         fill_addr_o,
    input  var logic                fill_data_valid_i,
    input  var logic [15:0]         fill_data_i,
    input  var logic                fill_refused_i,
    input  var logic                frame_fault_clear_i,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]         cache_hits_o,
    output var logic [31:0]         cache_misses_o,
    output var logic [31:0]         fills_o,        // LINE allocations; unchanged
    output var logic [31:0]         multicast_o,    // lanes served by one fill
    output var logic [31:0]         replays_o,      // probes squashed by a miss
    output var logic                fill_protocol_fault_o,
    output var logic [31:0]         cache_jobs_accepted_o,
    output var logic [31:0]         cache_jobs_completed_o,
    output var logic [31:0]         fill_jobs_accepted_o,
    output var logic [31:0]         fill_jobs_completed_o,
    output var logic [31:0]         fill_jobs_refused_o,
    output var logic [31:0]         fill_data_beats_o,
    // Observation-only leaf evidence. E2 may sink these inside the island; they
    // are not V3 public ABI. owner_state bits are
    // {C2-prepaid,C1-prepaid,replay-prepaid,blocking-fill,C2-ordinary,
    //  C1-ordinary,queued-response}; work_state names every idle term except
    // the reservation count and the sticky protocol fault.
    output var logic [31:0]         reservation_count_o,
    output var logic [6:0]          reservation_owner_state_o,
    output var logic [8:0]          cache_work_state_o,
    // Accepted work only; raw external offers are not work until handshaken.
    output var logic                idle_o
);

  // Clamp derived widths only for rejected configurations so the explicit
  // runtime/elaboration guard can execute instead of the parser dying first on
  // a zero-width slice.  Every legal parameterization takes the original arm.
  localparam int unsigned OFF_W  = (LINE_BYTES < 2) ? 1 : $clog2(LINE_BYTES); // 4
  localparam int unsigned IDX_W  = (LINES < 2) ? 1 : $clog2(LINES);           // 4
  localparam int unsigned HW_PL  = (LINE_BYTES < 2) ? 1 : LINE_BYTES / 2;     // 8
  localparam int unsigned BEAT_W = (HW_PL < 2) ? 1 : $clog2(HW_PL);           // 3
  localparam int unsigned TAG_W  = 32 - OFF_W - IDX_W;   // 24
  localparam int unsigned DAW    = IDX_W + BEAT_W;       // data-array address
  localparam int unsigned RQW    = (REQN < 2) ? 1 : $clog2(REQN);
  // Lane popcounts are properties of LANES, never of the unrelated request
  // FIFO depth. In particular LANES=8, REQN=2 must represent the value eight.
  localparam int unsigned POP_W  = (LANES < 1) ? 1 : $clog2(LANES + 1);

  // Quartus 17 does not accept bare module-scope elaboration `if`; keep every
  // parameter detector in one explicit initial block.  The minima are not
  // arbitrary: RQW/IDX_W/BEAT_W appear in nonempty bit slices below, and a line
  // contains at least two 16-bit halfwords so BEAT_W is at least one.
  initial begin : p_parameter_contract
`ifdef ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION
    $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PACKET_E_MUTANT_SELECTOR_COLLISION: define exactly one selector");
`endif
    if (REQN < 2)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[1]: REQN_MIN");
    if ((REQN & (REQN - 1)) != 0)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[2]: REQN_POWER_OF_TWO");
    if (LINES < 2)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[3]: LINES_MIN");
    if ((LINES & (LINES - 1)) != 0)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[4]: LINES_POWER_OF_TWO");
    if (LINE_BYTES < 4)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[5]: LINE_BYTES_MIN_HALFWORD_GEOMETRY");
    if ((LINE_BYTES & 1) != 0)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[6]: LINE_BYTES_HALFWORD_GEOMETRY");
    if ((LINE_BYTES & (LINE_BYTES - 1)) != 0)
      $fatal(1, "ZHAO_TEXTURE_CACHE_PIPE_V2_PARAM_FIRE[7]: LINE_BYTES_POWER_OF_TWO");
  end

`ifdef ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION
  // Elaboration sentinel: unlike an initial $fatal, this makes --lint-only/--cc
  // reject an ambiguous dual-selector build before any simulation can run. The
  // initial guard above preserves the exact diagnostic in tools that elaborate
  // unresolved cells differently.
  ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE_SELECTOR
      u_packet_e_selector_collision_compile_fail();
`endif

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
  logic [SRCW-1:0]     rq_src  [REQN];
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
  logic              c1_prepaid_r;
  logic [LANES-1:0]  c1_en;
  logic [SRCW-1:0]   c1_src;
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
  logic              c2_prepaid_r;
  logic [LANES-1:0]  c2_en;
  logic [SRCW-1:0]   c2_src;
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

  // COMPARE FIRST, SELECT SECOND -- 2026-09-18, and the arithmetic is unchanged.
  //
  // This block used to select the lowest needing lane, publish its tag as
  // `m_tag_c`, and only THEN compare all four lanes against it. That put four
  // 28-bit (TAG_W + IDX_W) equalities in series behind a LANES-deep priority
  // chain, and `@packet-h-satstage` measured the result as the machine's worst
  // path: `c2_tag[3][16]` -> `valid_r[2][11]`, 12.684 ns against 10.000. The
  // endpoint is what gives it away -- lane 3's TAG reaching lane 2's VALID can
  // only happen through the selected tag.
  //
  // Every operand of those comparisons is a register (`c2_tag`, `c2_idx`,
  // `c2_en`, `c2_rval`, `c2_rtag`), so the pairwise answers do not depend on
  // the selection at all and can be computed beside it. `same_c[j][k]` is the
  // full LANES x LANES table; the priority chain narrows to ONE BIT wide; and
  // the mask becomes a one-hot pick from a table that was already settled.
  //
  // This is a combinational restructuring inside a single cycle: the same
  // lane wins (lowest index), the same lanes join its mask, and `m_tag_c` /
  // `m_idx_c` carry the same values to the same consumers. A cycle-by-cycle
  // differential cannot see it, which is the point -- nothing downstream moves
  // by an edge and no latency constant changes.
  logic [LANES-1:0] same_c [LANES];
  always_comb
    for (int unsigned j = 0; j < LANES; j++)
      for (int unsigned k = 0; k < LANES; k++)
        same_c[j][k] = (c2_tag[j] == c2_tag[k]) && (c2_idx[j] == c2_idx[k]);

  logic [LANES-1:0] first_oh_c;   // the lowest needing lane, one-hot
  always_comb begin
    m_any_c    = 1'b0;
    first_oh_c = '0;
    for (int unsigned k = 0; k < LANES; k++)
      if (!m_any_c && c3_need_c[k]) begin
        m_any_c       = 1'b1;
        first_oh_c[k] = 1'b1;
      end
  end

  always_comb begin
    m_tag_c = '0;
    m_idx_c = '0;
    for (int unsigned k = 0; k < LANES; k++)
      if (first_oh_c[k]) begin
        m_tag_c = c2_tag[k];
        m_idx_c = c2_idx[k];
      end
  end

  // `same_c[j][j]` is trivially true, so the winning lane is always in its own
  // mask -- which is what the old form did too, by comparing a tag with itself.
  always_comb begin
    m_mask_c = '0;
    for (int unsigned k = 0; k < LANES; k++)
      for (int unsigned j = 0; j < LANES; j++)
        if (first_oh_c[j] && c3_need_c[k] && same_c[j][k])
          m_mask_c[k] = 1'b1;
  end

  logic [POP_W-1:0] mask_pop_c, en_pop_c;
  always_comb begin
    mask_pop_c = '0;
    en_pop_c   = '0;
    for (int unsigned k = 0; k < LANES; k++) begin
      mask_pop_c = mask_pop_c + POP_W'(m_mask_c[k]);
      en_pop_c   = en_pop_c   + POP_W'(c2_en[k]);
    end
  end

  // ==========================================================================
  // C4 — response FIFO and miss sequencer
  // ==========================================================================
  logic [LANES*16-1:0] rs_data   [REQN];
  logic [7:0]          rs_status [REQN];
  logic [SRCW-1:0]     rs_src    [REQN];
  logic [RQW:0]        rs_wp, rs_rp;
  logic [RQW:0]        rs_n;
  assign rs_n = rs_wp - rs_rp;

  assign smp_valid_o  = (rs_n != '0);
  assign smp_data_o   = rs_data[rs_rp[RQW-1:0]];
  assign smp_status_o = rs_status[rs_rp[RQW-1:0]];
  assign smp_src_id_o = rs_src[rs_rp[RQW-1:0]];

  // `rs_room` is TODAY's occupancy and NOTHING ELSE. It no longer gates
  // anything -- it survives as the subject of `a_retire_never_overflows` at the
  // bottom of this file, which is the property the reservation counter below
  // has to keep true. Read that counter's comment before reusing this signal.
  logic rs_room;
  assign rs_room = (rs_n != (RQW+1)'(REQN));

  // One name for "a response left the FIFO on this clock". It frees a
  // reservation and advances `rs_rp`, and those two must never disagree.
  logic rs_pop;
  assign rs_pop = smp_valid_o && smp_ready_i;

  logic              fb_busy_r, fb_req_r, fb_accepted_r;
  logic              fb_resv_r, replay_prepaid_r;
  logic [TAG_W-1:0]  fb_tag_r;
  logic [IDX_W-1:0]  fb_idx_r;
  logic [BEAT_W-1:0] fb_beat_r;
  logic [LANES-1:0]  fb_mask_r;
  logic [SRCW-1:0]   fb_src_r;

  assign fill_valid_o = fb_req_r;
  assign fill_addr_o  = {fb_tag_r, fb_idx_r, {OFF_W{1'b0}}};

  // FI owns the phase transition. Neither data nor refusal is accepted from an
  // offered request; the request must have been accepted on an earlier edge.
  logic fill_issue_accept_c, fill_refusal_legal_c, fill_data_accept_c;
  logic fill_success_c, fill_terminal_c, fill_protocol_fault_set_c;
  assign fill_issue_accept_c  = fill_valid_o && fill_ready_i;
  assign fill_refusal_legal_c = fill_refused_i && fb_busy_r && fb_accepted_r;
  assign fill_data_accept_c   = fill_data_valid_i && fb_busy_r
                              && fb_accepted_r && !fill_refused_i;
  assign fill_success_c       = fill_data_accept_c
                              && (fb_beat_r == BEAT_W'(HW_PL - 1));
  assign fill_terminal_c      = fill_success_c || fill_refusal_legal_c;

  // Every named malformed source has a reachable positive control in the
  // directed driver. Refusal still terminates when simultaneous or partial;
  // malformed data is excluded from the RAM write enable above and below.
  always_comb begin
    fill_protocol_fault_set_c = 1'b0;
    if (fill_data_valid_i && (!fb_busy_r || !fb_accepted_r))
      fill_protocol_fault_set_c = 1'b1; // before FI, ninth, or unsolicited
    if (fill_refused_i && (!fb_busy_r || !fb_accepted_r))
      fill_protocol_fault_set_c = 1'b1; // before FI, no miss, or duplicate
    if (fill_data_valid_i && fill_refused_i)
      fill_protocol_fault_set_c = 1'b1; // refusal wins this collision
    if (fill_refusal_legal_c && (fb_beat_r != '0))
      fill_protocol_fault_set_c = 1'b1; // partial-line refusal
  end

  // C3 resolves in order, so the request it describes is always `rq_rp`.
  logic c3_all_hit, c3_retire, c3_miss;
  assign c3_all_hit = c2_v && (c3_need_c == '0);
  assign c3_miss    = c2_v && m_any_c && !fb_busy_r;

  // ==========================================================================
  // THE RESPONSE SLOT IS RESERVED AT ISSUE, NOT CHECKED AT RETIREMENT
  // ==========================================================================
  // THE DEFECT THIS REPLACES. It was live, silent, and it returned
  // correct-looking data under the WRONG tag, which is worse than a hang.
  //
  // This block used to read:
  //
  //     assign c3_retire = c3_all_hit && rs_room;
  //     assign c1_go     = rq_issuable && !fb_busy_r && !c3_miss && rs_room;
  //
  // and claimed, in a comment sitting right here, that "holding issue on
  // `rs_room` keeps the pipe from producing results it cannot place". THAT
  // PROPERTY WAS FALSE. `rs_room` is a snapshot of the response FIFO's
  // occupancy TODAY; it accounts for nothing already in C1 or C2. And C1/C2 do
  // not stall: `c2_v <= c1_v` below is unconditional, outside any `if`, and the
  // payload capture under `if (c1_v)` replaces C2's contents whenever C1 is
  // occupied, with no reference to `c3_retire` or `rs_room`.
  //
  // So with the FIFO full and two hit results in flight, C2's result was
  // overwritten by C1's and then cleared, while `rq_ip` had ALREADY passed both
  // requests -- and every later retire blindly advances `rq_rp`, freeing the
  // vanished requests' slots while returning a YOUNGER request's `src_id`. A
  // consumer keyed on `smp_src_id_o` therefore receives somebody else's texels
  // for a fragment that will never be answered.
  //
  // REPRODUCED. Warm line, all lanes hit, `smp_ready_i` low, REQN = 4:
  //     accepted   0 1 2 3 4 5 6 7
  //     returned   0 1 2 3     6 7      <- 4 and 5 destroyed, silently
  // The miss path is not an accidental rescue: `c3_miss` and `c1_go` both
  // require `!fb_busy_r`, so the pipe is provably empty during a fill and there
  // is no replay that recreates the lost probe.
  // Evidence: reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt
  // section 2, and reports/TEXTURE-ISLAND-PREFIT-ADDENDUM-20260906.txt S4.
  //
  // THE INVARIANT THAT ACTUALLY HOLDS:
  //     rs_n + non-cancelled probe results in flight  <=  REQN
  //
  // `rs_resv` counts exactly that sum. A probe may only issue while the count
  // is below capacity, so by the time it reaches C3 its slot has been paid for
  // and a hit can ALWAYS retire -- which is why `c3_retire` loses its `rs_room`
  // term. Retirement TRANSFERS the reservation from the pipeline into the FIFO;
  // it does NOT free it, so `rs_wp++` must not decrement `rs_resv`. Only the
  // consumer's pop frees a slot. Getting that backwards re-opens the exact hole
  // this counter closes, because it would let a fresh probe issue against room
  // a queued response is still occupying.
  //
  // WHY NOT MAKE C1/C2 STALLABLE INSTEAD -- the obvious alternative, and the
  // expensive wrong road in THIS file. `rd_idx`/`rd_daddr` are combinational
  // off `rq_ip`, and `ram_tag`/`ram_dat` reload on EVERY clock with NO read
  // enable, deliberately: see the C2 comment above and the M10K notes at the
  // top of this file. Holding C2 means adding that enable and holding the RAM
  // outputs against it, which is precisely the inference the rebuild fought to
  // win back (5,634 ALM / 3 M10K was the cost of losing it). This counter is
  // three bits.
  logic [RQW:0] rs_resv;
  logic         resv_room;
  logic [RQW:0] squash_c;
  logic         c1_issue_fresh_c;
  logic         c1_ordinary_live_c, c2_ordinary_live_c;
  logic [RQW:0] resv_expected_c;
  assign resv_room = (rs_resv != (RQW+1)'(REQN));
  assign c1_issue_fresh_c = c1_go
                         && `ZHAO_PACKET_E_ISSUE_OWNS_FRESH_RESV(replay_prepaid_r);
  assign c1_ordinary_live_c = c1_v && !c1_prepaid_r;
  assign c2_ordinary_live_c = c2_v && !c2_prepaid_r;

  // Exact reservation ownership, not a bound. The five terms are clocked by
  // independent enables: response pushes/pops, each pipeline stage, fill
  // allocation/termination, and replay issue. A shared bad enable therefore
  // cannot move both sides of an intended timing comparison in lockstep.
  always_comb begin
    resv_expected_c = rs_n
                    + (RQW+1)'(c1_ordinary_live_c)
                    + (RQW+1)'(c2_ordinary_live_c)
                    + (RQW+1)'(fb_resv_r)
                    + (RQW+1)'(replay_prepaid_r)
                    + (RQW+1)'(c1_v && c1_prepaid_r)
                    + (RQW+1)'(c2_v && c2_prepaid_r);
  end

  assign reservation_count_o = 32'(rs_resv);
  assign reservation_owner_state_o = {
      c2_v && c2_prepaid_r,
      c1_v && c1_prepaid_r,
      replay_prepaid_r,
      fb_resv_r,
      c2_ordinary_live_c,
      c1_ordinary_live_c,
      (rs_n != '0)
  };
  assign cache_work_state_o = {
      replay_prepaid_r,
      fb_resv_r,
      fb_accepted_r,
      fb_req_r,
      fb_busy_r,
      (rs_n != '0),
      c2_v,
      c1_v,
      (rq_n != '0)
  };

  // Payload RAM is deliberately unreset and is not busy state. Every accepted
  // work owner, including FI phase and reservation-transfer state, is explicit.
  assign idle_o = (rq_n == '0)
               && !c1_v
               && !c2_v
               && (rs_n == '0)
               && (rs_resv == '0)
               && !fb_busy_r
               && !fb_req_r
               && !fb_accepted_r
               && !fb_resv_r
               && !replay_prepaid_r;

  // A miss rewinds both probes and counts both as replayed work, but only the
  // younger C1 reservation is refunded. C2 is the miss identity; its reservation
  // changes owner to fb_resv_r and later to either the prepaid replay or refusal
  // response. If the C2 probe itself was prepaid, the same single credit simply
  // begins another blocking fill for a second missing line of that request.
  assign squash_c = (RQW+1)'(c2_v) + (RQW+1)'(c1_v);

  // Room was reserved two stages ago. If this ever needs `rs_room` back, the
  // counter is broken -- fix the counter, do not re-add the term.
  assign c3_retire = c3_all_hit;

  // A fresh probe needs room; the first replay after a successful fill spends
  // the reservation already carried by replay_prepaid_r and must not reserve a
  // second slot.
  logic c1_go;
  assign c1_go = rq_issuable && !fb_busy_r && !c3_miss
              && (replay_prepaid_r || resv_room);

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
  // ENFORCED-BY: tools/quartus/check_ram_inference.py
  // That checker gained this exact rule on 2026-09-03, after it called the
  // half-fixed version of this file CLEAN and an 88-minute fit came back with
  // 2 M10K anyway. It is validated by flagging the version the fitter rejected
  // while staying quiet on this one.
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
        if (fill_data_accept_c && fb_mask_r[gl]) begin
          g_lane[gl].data_r[{fb_idx_r, fb_beat_r}] <= fill_data_i;
          if (fill_success_c) begin
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
      rs_wp <= '0; rs_rp <= '0; rs_resv <= '0;
      c1_v <= 1'b0;
      c1_prepaid_r <= 1'b0;
      c2_v <= 1'b0;
      c2_prepaid_r <= 1'b0;
      fb_busy_r    <= 1'b0;
      fb_req_r     <= 1'b0;
      fb_accepted_r <= 1'b0;
      fb_resv_r    <= 1'b0;
      replay_prepaid_r <= 1'b0;
      fill_protocol_fault_o <= 1'b0;
      cache_hits_o   <= 32'd0;
      cache_misses_o <= 32'd0;
      fills_o        <= 32'd0;
      multicast_o    <= 32'd0;
      replays_o      <= 32'd0;
      cache_jobs_accepted_o  <= 32'd0;
      cache_jobs_completed_o <= 32'd0;
      fill_jobs_accepted_o   <= 32'd0;
      fill_jobs_completed_o  <= 32'd0;
      fill_jobs_refused_o    <= 32'd0;
      fill_data_beats_o      <= 32'd0;
      for (int unsigned k = 0; k < LANES; k++)
        for (int unsigned i = 0; i < LINES; i++) valid_r[k][i] <= 1'b0;
    end else begin
      // Malformed observation dominates clear on the same edge. A frame clear
      // is effective only after every accepted owner and credit has drained.
      if (fill_protocol_fault_set_c)
        fill_protocol_fault_o <= 1'b1;
      else if (frame_fault_clear_i && idle_o)
        fill_protocol_fault_o <= 1'b0;

      // ---- C0: accept ------------------------------------------------------
      if (acc_valid_i && acc_ready_o) begin
        rq_en[rq_wp[RQW-1:0]]   <= acc_en_i;
        rq_addr[rq_wp[RQW-1:0]] <= acc_addr_i;
        rq_src[rq_wp[RQW-1:0]]  <= acc_src_id_i;
        rq_wp <= rq_wp + (RQW+1)'(1);
        cache_jobs_accepted_o <= cache_jobs_accepted_o + 32'd1;
      end

      // ---- C1: issue -------------------------------------------------------
      c1_v <= c1_go;
      if (c1_go) begin
        c1_prepaid_r <= replay_prepaid_r;
        c1_en  <= rq_en[rq_ip[RQW-1:0]];
        c1_src <= rq_src[rq_ip[RQW-1:0]];
        for (int unsigned k = 0; k < LANES; k++) begin
          c1_tag[k] <= i_tag[k];
          c1_idx[k] <= i_idx[k];
        end
        rq_ip <= rq_ip + (RQW+1)'(1);
        if (replay_prepaid_r)
          replay_prepaid_r <= 1'b0;
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
        c2_prepaid_r <= c1_prepaid_r;
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
        rs_status[rs_wp[RQW-1:0]] <= 8'h00;
        rs_src[rs_wp[RQW-1:0]] <= c2_src;
        rs_wp <= rs_wp + (RQW+1)'(1);
        rq_rp <= rq_rp + (RQW+1)'(1);
        // ONE add of the enabled-lane popcount, never one per lane in a loop:
        // four nonblocking increments all read the same old value and only the
        // last lands.
        cache_hits_o <= cache_hits_o + 32'(en_pop_c);
      end
      if (rs_pop) begin
        rs_rp <= rs_rp + (RQW+1)'(1);
        cache_jobs_completed_o <= cache_jobs_completed_o + 32'd1;
      end

      // ---- C3: a miss REWINDS the issue pointer and squashes the pipe ------
      // Nothing is lost: a request is only removed from the FIFO when it has
      // fully hit, so rewinding to `rq_rp` re-probes exactly the requests that
      // had not yet retired.
      if (c3_miss) begin
        fb_busy_r    <= 1'b1;
        fb_req_r     <= 1'b1;
        fb_accepted_r <= 1'b0;
        fb_resv_r    <= 1'b1;
        fb_tag_r  <= m_tag_c;
        fb_idx_r  <= m_idx_c;
        fb_mask_r <= m_mask_c;
        fb_src_r  <= c2_src;
        fb_beat_r <= '0;
        fills_o   <= fills_o + 32'd1;
        cache_misses_o <= cache_misses_o + 32'(mask_pop_c);
        multicast_o    <= multicast_o + 32'(mask_pop_c) - 32'd1;
        for (int unsigned k = 0; k < LANES; k++)
          if (m_mask_c[k]) valid_r[k][m_idx_c] <= 1'b0;

        rq_ip <= rq_rp;
        c1_v  <= 1'b0;
        c2_v  <= 1'b0;
        // `replays_o` counts PROBES squashed, not miss events.  C3's missing
        // probe and the younger C1 probe each already own a reservation and both
        // are rewound; incrementing by one hid the second unit of repeated work.
        replays_o <= replays_o + 32'(squash_c);
      end

      // ---- C4: exact reservation next-state --------------------------------
      // A fresh issue creates one reservation, a response pop destroys one,
      // and a miss destroys only a younger ordinary C1 reservation. C2's credit
      // is transferred to fb_resv_r; response enqueue, successful replay and
      // refusal retirement are ownership moves and therefore absent here.
      rs_resv <= rs_resv
               + (RQW+1)'(c1_issue_fresh_c)
               - (RQW+1)'(c3_miss && c1_ordinary_live_c)
               - (RQW+1)'(rs_pop);

      // ---- C4: typed fill termination --------------------------------------
      if (fill_issue_accept_c) begin
        fb_req_r      <= 1'b0;
        fb_accepted_r <= 1'b1;
        fill_jobs_accepted_o <= fill_jobs_accepted_o + 32'd1;
      end
      if (fill_terminal_c)
        fill_jobs_completed_o <= fill_jobs_completed_o + 32'd1;

      if (fill_data_accept_c) begin
        fill_data_beats_o <= fill_data_beats_o + 32'd1;
        if (fill_success_c) begin
          // RAM/tag writes occur in the clock-only bank process from this same
          // legal-data predicate. Publish valid only after beat eight and move
          // the blocking credit to the first replay of this exact head.
          for (int unsigned k = 0; k < LANES; k++)
            if (fb_mask_r[k]) valid_r[k][fb_idx_r] <= 1'b1;
          fb_busy_r       <= 1'b0;
          fb_accepted_r   <= 1'b0;
          fb_resv_r       <= 1'b0;
          replay_prepaid_r <= 1'b1;
        end else begin
          fb_beat_r <= fb_beat_r + BEAT_W'(1);
        end
      end

      if (fill_refusal_legal_c) begin
        // Refusal wins over same-cycle data, so fill_data_accept_c is false.
        // Partial storage is made unreachable again. The C2-origin reservation
        // moves unchanged into this one typed terminal response.
        for (int unsigned k = 0; k < LANES; k++)
          if (fb_mask_r[k]) valid_r[k][fb_idx_r] <= 1'b0;
        rs_data[rs_wp[RQW-1:0]]   <= '0;
        rs_status[rs_wp[RQW-1:0]] <= 8'h01;
        rs_src[rs_wp[RQW-1:0]]    <= fb_src_r;
        rs_wp <= rs_wp + (RQW+1)'(1);
        rq_rp <= rq_rp + (RQW+1)'(1);
        rq_ip <= `ZHAO_PACKET_E_REFUSAL_NEXT_IP(rq_rp, (RQW+1)'(1));
        c1_v <= 1'b0;
        c2_v <= 1'b0;
        fb_busy_r       <= 1'b0;
        fb_req_r        <= 1'b0;
        fb_accepted_r   <= 1'b0;
        fb_resv_r       <= 1'b0;
        replay_prepaid_r <= 1'b0;
        fill_jobs_refused_o   <= fill_jobs_refused_o + 32'd1;
      end
    end
  end

`ifndef SYNTHESIS
  // ==========================================================================
  // THE RESPONSE-RESERVATION PROPERTIES
  // ==========================================================================
  // Simulation-only immediate assertions in a clocked block, matching
  // zhao_geom_assetfetch.sv:632 -- they run under Verilator in the directed
  // test rather than only under a formal frontend, because the defect they
  // guard was found by a cycle model and has to stay caught by an ordinary run.
  //
  // ENFORCED-BY: tests/texture/texture_cache_pipe_v2_directed.cpp
  //
  // `rst_n` is NOT read synchronously here. A net that is an asynchronous reset
  // in one process and a synchronous condition in another is Verilator's
  // SYNCASYNCNET, and it is a real caution rather than a style note. The idiom
  // is zhao_raster_tile_pipe.sv:417's: a plain flag that says "reset has
  // released", which is all the assertions actually want.
  logic assert_armed_q;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) assert_armed_q <= 1'b0;
    else        assert_armed_q <= 1'b1;
  end

  always_ff @(posedge clk) begin
    if (assert_armed_q) begin
      // 1. Exact ownership is the contract. A capacity-only assertion cannot
      //    detect refunded blocking credit or a double-reserved replay.
      a_resv_exact_ownership :
        assert (rs_resv == resv_expected_c)
        else $error("cache_pipe: reservation identity failed rs_resv=%0d expected=%0d (rs=%0d c1o=%0d c2o=%0d fill=%0d prepaid=%0d/%0d/%0d)",
                    rs_resv, resv_expected_c, rs_n,
                    c1_ordinary_live_c, c2_ordinary_live_c, fb_resv_r,
                    replay_prepaid_r, c1_v && c1_prepaid_r,
                    c2_v && c2_prepaid_r);

      a_prepaid_has_one_owner :
        assert (((RQW+1)'(replay_prepaid_r)
               + (RQW+1)'(c1_v && c1_prepaid_r)
               + (RQW+1)'(c2_v && c2_prepaid_r)) <= (RQW+1)'(1))
        else $error("cache_pipe: one fill reservation appeared in multiple prepaid owners");

      a_idle_evidence_is_complete :
        assert (idle_o == ((reservation_count_o == 32'd0)
                        && (cache_work_state_o == 9'd0)))
        else $error("cache_pipe: observation-only evidence omitted an idle operand");

      // 2. Never promise more slots than exist. If this trips, an issue path
      //    is missing `resv_room` and results will start overwriting one
      //    another in C2 again.
      a_resv_never_exceeds_capacity :
        assert ((rs_resv <= (RQW+1)'(REQN)))
        else $error("cache_pipe: rs_resv=%0d exceeds REQN=%0d -- a probe issued without a reserved response slot",
                    rs_resv, REQN);

      // 2. Every entry already queued is still covered by a live reservation.
      //    This is the half that breaks the moment `rs_wp++` is made to
      //    decrement `rs_resv`: the FIFO would then hold responses nobody had
      //    reserved room for, and a fresh probe could issue against an
      //    occupied slot.
      a_queued_entries_stay_reserved :
        assert ((rs_n <= rs_resv))
        else $error("cache_pipe: rs_n=%0d exceeds rs_resv=%0d -- a retirement freed a reservation instead of transferring it",
                    rs_n, rs_resv);

      // 3. The property the ORIGINAL comment claimed and did not have.
      //    `c3_retire` no longer tests `rs_room`, so this is the statement that
      //    dropping the term was safe: a hit never retires into a full FIFO.
      //    Before the counter this condition was reachable, and the fallout was
      //    silent -- C2 was overwritten, `rq_ip` had already passed the
      //    request, and a later retire returned a younger `src_id` in the older
      //    request's place.
      a_retire_never_overflows :
        assert (!(c3_retire && !rs_room))
        else $error("cache_pipe: a hit retired into a FULL response FIFO -- the lost-result defect is back");
    end
  end
`endif

endmodule : zhao_texture_cache_pipe_v2

`undef ZHAO_PACKET_E_REFUSAL_NEXT_IP
`undef ZHAO_PACKET_E_ISSUE_OWNS_FRESH_RESV
`ifdef ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION
  `undef ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION
`endif
`default_nettype wire
