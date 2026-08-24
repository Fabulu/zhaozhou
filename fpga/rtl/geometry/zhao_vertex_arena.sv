// zhao_vertex_arena.sv — the reusable direct-indexed vertex arena primitive.
//
// Owner ruling 2026-08-24: "Build a reusable parameterized arena primitive and a
// GEOM.WCACHE shell. Terrain may later instantiate the same primitive with its
// own depth/payload. This does not mean one physical cache shared between the
// two pipelines."
//
// A BOUNDED ARENA, NOT A CACHE. No tags, no comparators, no LRU, no eviction:
// a lookup IS an index. The producer already knows each vertex's number -- the
// tessellator holds vi/vj before it expands them into world coordinates -- so
// identity is GIVEN, and an associative structure would pay to rediscover
// something nobody lost.
//
// ---------------------------------------------------------------------------
// THE VALID MECHANISM, which is the one real design decision here
// ---------------------------------------------------------------------------
// A slot must be answerable as "not written in THIS use of the arena". Three
// ways to do that, and only one survives the constraints:
//
//   1. clear the payload memory on open  -- FORBIDDEN. A reset or clear that
//      touches the array prevents inference (QUARTUS_GOTCHAS 10), and this
//      block is a store: if it does not infer, it has failed.
//   2. store a generation tag per slot INSIDE the memory and compare on read --
//      attractive, and wrong here. After reset the memory contents are
//      UNDEFINED, so a slot never written could hold a tag equal to the current
//      generation by accident. The contract demands a DETERMINISTIC refusal,
//      and "almost certainly refuses" is not that.
//   3. a valid bitmap in FLOPS, cleared per arena on open. Costs ARENAS*DEPTH
//      registers and a wide clear, and is exactly deterministic.
//
// (3) is chosen. The cost is real and bounded: the shell's 2x1089 shape is
// 2,178 flops. For a much deeper instantiation the alternative is a clear WALK
// that holds `sealed` low until it completes -- deterministic for the same
// reason, at the cost of DEPTH cycles once per arena per frame, which is
// nothing against 1,666,667 clocks. Not built until something needs it.
//
// The generation is NOT redundant with the valid bitmap. The bitmap is
// per-slot; the mistake it cannot catch is per-arena -- a consumer replaying
// keys from a PREVIOUS use of a reopened arena. Every one of those keys is
// individually plausible; only the generation says they are stale.

module zhao_vertex_arena #(
    parameter int unsigned ARENAS    = 2,
    parameter int unsigned DEPTH     = 1089,   // 33x33 terrain lattice
    parameter int unsigned PAYLOAD_W = 64,
    parameter int unsigned GEN_W     = 8,
    // ONE BIT WIDER THAN THE ADDRESS, DELIBERATELY. The contract requires an
    // out-of-range index to be REFUSED deterministically, and a caller can only
    // present one if the port can carry it. Sized at exactly $clog2(DEPTH) the
    // check is vacuous for a power-of-two depth -- Verilator says so, in those
    // words: "Comparison is constant due to unsigned arithmetic". The refusal
    // path would then exist in the contract, in the oracle and in the tests,
    // and nowhere in the silicon.
    parameter int unsigned INDEX_W   = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W   = $clog2(ARENAS) + 1
) (
    input  logic clk,
    input  logic rst_n,

    // ---- producer: open / origin / fill / seal ------------------------------
    input  logic                       open_i,        // begin a new use
    input  logic [ARENA_W-1:0]         open_arena_i,
    output logic [GEN_W-1:0]           open_gen_o,    // the new generation

    input  logic                       org_we_i,
    input  logic [ARENA_W-1:0]         org_arena_i,
    input  logic signed [31:0]         org_x_i,
    input  logic signed [31:0]         org_y_i,
    input  logic signed [31:0]         org_z_i,

    input  logic                       fill_valid_i,
    output logic                       fill_ready_o,
    input  logic [ARENA_W-1:0]         fill_arena_i,
    input  logic [INDEX_W-1:0]         fill_index_i,
    input  logic [PAYLOAD_W-1:0]       fill_payload_i,

    input  logic                       seal_i,
    input  logic [ARENA_W-1:0]         seal_arena_i,

    // ---- consumer: lookup / replay -----------------------------------------
    input  logic                       look_valid_i,
    output logic                       look_ready_o,
    input  logic [ARENA_W-1:0]         look_arena_i,
    input  logic [GEN_W-1:0]           look_gen_i,
    input  logic [INDEX_W-1:0]         look_index_i,

    output logic                       rep_valid_o,
    output logic                       rep_hit_o,
    output logic                       rep_refuse_o,
    output logic [PAYLOAD_W-1:0]       rep_payload_o,
    output logic signed [31:0]         rep_org_x_o,
    output logic signed [31:0]         rep_org_y_o,
    output logic signed [31:0]         rep_org_z_o,

    // ---- counters -----------------------------------------------------------
    output logic [31:0]                arena_hits_o,
    output logic [31:0]                arena_misses_o,
    output logic [31:0]                arena_refusals_o,
    output logic                       arena_overflow_o
);

  localparam int unsigned AW = $clog2(ARENAS);
  localparam int unsigned IW = $clog2(DEPTH);

  // ---- the store. NO RESET TOUCHES IT. ------------------------------------
  // Synchronous read, whole-word write, no byte enables -- the three properties
  // QUARTUS_GOTCHAS 10 measured as decisive. A map reporting blockMemoryBits = 0
  // means this failed, however green the tests are.
  logic [PAYLOAD_W-1:0] mem [0:(ARENAS*DEPTH)-1];
  logic [PAYLOAD_W-1:0] mem_q;

  logic [AW+IW-1:0] wr_addr, rd_addr;

  // ---- metadata, in flops -------------------------------------------------
  logic [GEN_W-1:0]      gen_q   [0:ARENAS-1];
  logic                  sealed_q[0:ARENAS-1];
  logic                  valid_q [0:(ARENAS*DEPTH)-1];
  logic signed [31:0]    org_x_q [0:ARENAS-1];
  logic signed [31:0]    org_y_q [0:ARENAS-1];
  logic signed [31:0]    org_z_q [0:ARENAS-1];

  // Always accept: neither channel can stall, because the store answers in one
  // clock and the metadata is combinational. Stated as constants so a future
  // banking change has to change them deliberately rather than by accident.
  assign fill_ready_o = 1'b1;
  assign look_ready_o = 1'b1;

  // ---- fill decode --------------------------------------------------------
  wire fill_bad_index = (fill_index_i >= INDEX_W'(DEPTH));
  wire fill_bad_arena = (fill_arena_i >= ARENA_W'(ARENAS));
  wire fill_sealed    = !fill_bad_arena && sealed_q[fill_arena_i[AW-1:0]];
  // `rst_n` gates the ENABLE, not the array. Gating the enable is free and does
  // not touch inference -- what prevents inference is a reset that writes the
  // memory, which this still never does.
  //
  // FOUND BY THE PROOF. The memory process is clock-only (deliberately), so
  // without this a fill asserted DURING reset lands in the store while every
  // reset-gated observer -- valid bits, generation, and the proof's own shadow --
  // ignores it. The store then holds a value nothing recorded, which is exactly
  // the 'a lookup returns a payload nobody wrote' shape the first formal
  // property exists to forbid.
  wire fill_ok        = rst_n && fill_valid_i && !fill_bad_index && !fill_bad_arena && !fill_sealed;
  wire fill_drop      = fill_valid_i && (fill_bad_index || fill_bad_arena || fill_sealed);

  // Every arena-indexed channel is range-checked, not just the two that answer
  // a consumer. -Wall found these by noticing the top bit went unread.
  wire org_bad_arena  = (org_arena_i  >= ARENA_W'(ARENAS));
  wire seal_bad_arena = (seal_arena_i >= ARENA_W'(ARENAS));
  wire open_bad_arena = (open_arena_i >= ARENA_W'(ARENAS));

  assign wr_addr = {fill_arena_i[AW-1:0], fill_index_i[IW-1:0]};
  assign rd_addr = {look_arena_i[AW-1:0], look_index_i[IW-1:0]};

  // ---- lookup decode. The ORDER is contractual. ---------------------------
  // A caller with both an out-of-range index and a stale generation is told
  // about the index, so a diagnosis does not depend on which fault happens to
  // be tested first. Mirrors zref::geom::VertexArena::lookup exactly.
  wire look_bad_arena = (look_arena_i >= ARENA_W'(ARENAS));
  wire look_bad_index = (look_index_i >= INDEX_W'(DEPTH));
  wire look_unsealed  = look_bad_arena || !sealed_q[look_arena_i[AW-1:0]];
  wire look_stale     = look_bad_arena || (look_gen_i != gen_q[look_arena_i[AW-1:0]]);
  wire look_refuse    = look_valid_i &&
                        (look_bad_arena || look_bad_index || look_unsealed || look_stale);
  wire look_present   = look_valid_i && !look_refuse && valid_q[rd_addr];
  wire look_miss      = look_valid_i && !look_refuse && !valid_q[rd_addr];

  // ---- the memory process: CLOCK ONLY -------------------------------------
  always_ff @(posedge clk) begin
    if (fill_ok) mem[wr_addr] <= fill_payload_i;
    mem_q <= mem[rd_addr];
  end

  // ---- metadata process ---------------------------------------------------
  integer i;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (i = 0; i < ARENAS; i = i + 1) begin
        gen_q[i]    <= '0;
        sealed_q[i] <= 1'b0;
        org_x_q[i]  <= '0;
        org_y_q[i]  <= '0;
        org_z_q[i]  <= '0;
      end
      for (i = 0; i < ARENAS * DEPTH; i = i + 1) valid_q[i] <= 1'b0;
      arena_overflow_o <= 1'b0;
    end else begin
      // OPEN: bump the generation, unseal, drop this arena's valid bits.
      // The payload memory is untouched -- staleness is answered by metadata,
      // never by contents, which is what lets the store infer.
      if (open_i && !open_bad_arena) begin
        gen_q[open_arena_i[AW-1:0]]    <= gen_q[open_arena_i[AW-1:0]] + GEN_W'(1);
        sealed_q[open_arena_i[AW-1:0]] <= 1'b0;
        for (i = 0; i < DEPTH; i = i + 1) valid_q[open_arena_i * DEPTH + i] <= 1'b0;
      end

      // Range-checked like every other channel. An out-of-range origin write
      // would otherwise ALIAS onto a real arena's datum, which is the same
      // class of failure the lookup refusals exist to prevent -- one arena
      // silently answering for another.
      if (org_we_i && !org_bad_arena) begin
        org_x_q[org_arena_i[AW-1:0]] <= org_x_i;
        org_y_q[org_arena_i[AW-1:0]] <= org_y_i;
        org_z_q[org_arena_i[AW-1:0]] <= org_z_i;
      end

      if (fill_ok)   valid_q[wr_addr]        <= 1'b1;
      if (fill_drop) arena_overflow_o        <= 1'b1;   // sticky
      if (seal_i && !seal_bad_arena) sealed_q[seal_arena_i[AW-1:0]] <= 1'b1;
    end
  end

  // ---- the reply, registered ----------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rep_valid_o  <= 1'b0;
      rep_hit_o    <= 1'b0;
      rep_refuse_o <= 1'b0;
      rep_org_x_o  <= '0;
      rep_org_y_o  <= '0;
      rep_org_z_o  <= '0;
    end else begin
      rep_valid_o  <= look_valid_i;
      rep_hit_o    <= look_present;
      rep_refuse_o <= look_refuse;
      rep_org_x_o  <= org_x_q[look_arena_i[AW-1:0]];
      rep_org_y_o  <= org_y_q[look_arena_i[AW-1:0]];
      rep_org_z_o  <= org_z_q[look_arena_i[AW-1:0]];
    end
  end

  // The payload is the memory's own registered output. It is meaningful only
  // when rep_hit_o, and the consumer is required to check -- returning zero on
  // a miss would cost a mux on the widest path in the block for a value the
  // contract already says must not be read.
  assign rep_payload_o = mem_q;

  assign open_gen_o = gen_q[open_arena_i[AW-1:0]] + GEN_W'(1);

  // ---- counters -----------------------------------------------------------
  // Saturating, so a long run cannot silently wrap a diagnosis. Refusals are
  // counted apart from misses on purpose: a rising refusal count is a CALLER
  // BUG, a rising miss count is ordinary cold traffic.
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      arena_hits_o     <= '0;
      arena_misses_o   <= '0;
      arena_refusals_o <= '0;
    end else begin
      if (look_present && arena_hits_o     != 32'hFFFF_FFFF) arena_hits_o     <= arena_hits_o + 32'd1;
      if (look_miss    && arena_misses_o   != 32'hFFFF_FFFF) arena_misses_o   <= arena_misses_o + 32'd1;
      if (look_refuse  && arena_refusals_o != 32'hFFFF_FFFF) arena_refusals_o <= arena_refusals_o + 32'd1;
    end
  end


`ifdef FORMAL
  // -------------------------------------------------------------------------
  // THE ARENA PROPERTIES
  // -------------------------------------------------------------------------
  // Immediate assertions in clocked blocks, matching zhao_debug_frameblit:
  // read_slang, the formal frontend, rejects concurrent SVA outright
  // ("encountered unsupported SVA feature" on every assert property).
  //
  // THE WATCHED-SLOT TECHNIQUE. Property 1 -- a lookup never returns a payload
  // written under a different {arena, index} -- is a statement about EVERY
  // slot, which a bounded proof cannot enumerate. So the solver picks ONE
  // arbitrary slot at reset and holds it: `f_arena`/`f_index` are free and
  // constant, `f_shadow` mirrors only writes to that slot. If a hit on the
  // watched key can ever disagree with the shadow, the solver will find it, and
  // because the key was free the proof covers all of them.
  logic f_past_valid;
  initial f_past_valid = 1'b0;
  always_ff @(posedge clk) f_past_valid <= 1'b1;

  // THE MACHINE MUST HAVE BEEN RESET. Without this `rst_n` is a free input, so
  // the solver may start in an arbitrary state -- slots already valid, arenas
  // already sealed, generations already matching, memory holding anything -- and
  // then refute the shadow trivially at k = 2 without exhibiting any bug.
  //
  // That is exactly what happened on the first run of this proof, and it cost a
  // wrong diagnosis first: I read the counterexample as a read-during-write
  // ordering gap and modelled read-old before noticing the state was never
  // initialised. The read-old modelling was right on its own merits and stays;
  // it was simply not what property 0 was complaining about.
  //
  // Same shape as zhao_debug_frameblit_safety_harness.
  always_ff @(posedge clk) begin
    if (!f_past_valid)                assume (!rst_n);
    if (f_past_valid && $past(rst_n)) assume (rst_n);
  end

  (* anyconst *) logic [ARENA_W-1:0] f_arena;
  (* anyconst *) logic [INDEX_W-1:0] f_index;

  logic [PAYLOAD_W-1:0] f_shadow;
  logic                 f_shadow_valid;   // written since the last open
  logic [GEN_W-1:0]     f_shadow_gen;

  wire f_watched_fill = fill_ok &&
                        (fill_arena_i == f_arena) && (fill_index_i == f_index);
  wire f_watched_look = look_valid_i &&
                        (look_arena_i == f_arena) && (look_index_i == f_index);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      f_shadow       <= '0;
      f_shadow_valid <= 1'b0;
      f_shadow_gen   <= '0;
    end else begin
      if (open_i && !open_bad_arena && (open_arena_i == f_arena)) begin
        f_shadow_valid <= 1'b0;
        f_shadow_gen   <= gen_q[f_arena[AW-1:0]] + GEN_W'(1);
      end
      if (f_watched_fill) begin
        f_shadow       <= fill_payload_i;
        f_shadow_valid <= 1'b1;
      end
    end
  end

  // READ-OLD, and the proof found that this was never stated.
  //
  // A fill and a lookup to the SAME slot in the SAME cycle: the inferred
  // memory returns the value present BEFORE the write (read-old), because that
  // is what an M10K does without a bypass network. The first version of this
  // proof compared against a shadow that had already taken the new value, and
  // bmc refuted it at k = 2 -- correctly. The RTL was right and the SPEC was
  // silent.
  //
  // Read-old is kept rather than bypassed: a bypass costs a mux on the widest
  // path in the block to serve a case the producer/consumer split makes rare,
  // and SURFACE.SHEET already established that read-old survives here without
  // one. The contract now says so, and this capture models it -- f_expect takes
  // the shadow value as of the ACCEPT EDGE, before that edge's fill.
  logic f_look_was_watched;
  logic [PAYLOAD_W-1:0] f_expect;
  logic                 f_expect_valid;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      f_look_was_watched <= 1'b0;
      f_expect           <= '0;
      f_expect_valid     <= 1'b0;
    end else begin
      f_look_was_watched <= f_watched_look;
      f_expect           <= f_shadow;        // pre-edge value, like mem_q
      f_expect_valid     <= f_shadow_valid;
    end
  end

  always_ff @(posedge clk) begin
    if (f_past_valid && rst_n && $past(rst_n)) begin
      // 1. + 2. THE ONE THAT MATTERS. A hit on the watched key returns the
      //         value written to THAT key, never another slot's and never a
      //         previous generation's. This is "it must never wrap into
      //         another vertex", stated so a solver can refute it.
      if (f_look_was_watched && rep_hit_o) begin
        a_hit_is_the_watched_value: assert (rep_payload_o == f_expect);
        a_hit_implies_written:      assert (f_expect_valid);
      end

      // 3. a hit implies the slot was filled since the last open of its arena.
      //    The shadow only becomes valid on a fill and is cleared by open, so
      //    the assertion above already carries it; kept separate because it is
      //    the property a reader looks for.

      // 4. a refusal NEVER claims a hit, and never presents a payload as valid.
      if (rep_refuse_o) a_refuse_is_not_a_hit: assert (!rep_hit_o);

      // hit and refuse are mutually exclusive and only appear with a reply
      if (rep_hit_o || rep_refuse_o) a_reply_present: assert (rep_valid_o);

      // 5. the fault bit is sticky -- a dropped fill is not a transient
      if ($past(arena_overflow_o)) a_overflow_sticky: assert (arena_overflow_o);
    end
  end

  // COVERS. Every assertion above is guarded, and a machine that never looks
  // anything up satisfies all of them while proving nothing -- the vacuity
  // shape MEM.GUARD once shipped.
  always_ff @(posedge clk) begin
    if (rst_n) begin
      c_hit:        cover (rep_valid_o && rep_hit_o);
      c_miss:       cover (rep_valid_o && !rep_hit_o && !rep_refuse_o);
      c_refuse:     cover (rep_valid_o && rep_refuse_o);
      c_watched_hit:cover (f_look_was_watched && rep_hit_o);
      c_overflow:   cover (arena_overflow_o);
      c_regen:      cover (f_past_valid && open_i && !open_bad_arena);
    end
  end
`endif

endmodule
