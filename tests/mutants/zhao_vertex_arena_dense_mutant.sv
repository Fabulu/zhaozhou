// zhao_vertex_arena_dense_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make `arena_misses_o` a detector in DENSE_SEAL mode instead
// of a hopeful zero.
//
// In VALID_MODE=1 the primitive PROVES a legal lookup can never miss
// (a_dense_no_miss, prove_dense): refusal already requires sealed, seal is
// refused below a full count, and the index bound is checked -- so the miss
// counter is unreachable by construction in a working block, and no stimulus
// can move it. "It can fire" would stay an argument forever. The only
// demonstration is to break the guard that makes it unreachable, which is
// what this copy does --
//
//     seal_gate_c: (32'(cnt_q[...]) == DEPTH)   ->   (32'(cnt_q[...]) <= DEPTH)
//
// admitting a seal of an INCOMPLETE arena. A lookup of an unwritten row of
// that wrongly-sealed arena then travels the whole legal path -- sealed,
// current generation, in-range index -- and lands on `slot_written_c` low,
// which is a MISS, and the counter moves. That is exactly the fault class
// the dense mechanism exists to make impossible, and the counter is the
// silicon-side witness that it stayed impossible.
//
// TWO DRIVERS, two polarities, deliberately:
//   * vertex_arena_dense_seal_control.cpp PASSES when arena_misses_o FIRES
//     (inverse polarity: evidence about the instrument, not the design);
//   * vertex_arena_dense_directed.cpp, compiled UNCHANGED against this
//     module, FAILS loudly -- the checker seen to fail on a deliberate
//     break, per the differential law.
//
// It is a separate FILE rather than a temporary edit for two reasons. A
// temporary edit to production RTL is a fit-corrupting live-tree hazard, and
// it leaves nothing behind: the next person has the same argument and no
// evidence. The module is RENAMED so it can never be elaborated in place of
// the real one by a source-list mistake, and it lives under tests/ where
// check_forbidden_sources.py will not find it in a production closure.
//
// REGENERATE IT if zhao_vertex_arena.sv changes shape: this is a copy, and a
// copy of an old version is a positive control for a block that no longer
// exists. (Copied from the 2026-09-09 VALID_MODE consolidation state.)


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
// A slot must be answerable as "not written in THIS use of the arena". Four
// ways to do that, and which one survives depends on the PRODUCER:
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
//   4. DENSE SEAL, found by the zhao_proj_arena3 design study (2026-09-09,
//      reports/ARENA-I-BUILT-A-SECOND-ONE-20260909.md): require the fill to be
//      DENSE -- each accepted fill's index must equal the arena's fill COUNT,
//      so rows 0..DEPTH-1 are written exactly once, in order -- and REFUSE the
//      seal unless the count is exactly DEPTH. "Was this row written this
//      lifetime" then stops being a per-row question and becomes a property of
//      the ARENA, held in ARENAS counters of $clog2(DEPTH+1) bits. No bitmap,
//      no clear, no walk, no tag-in-RAM: an unfilled arena can never seal, and
//      an unsealed arena refuses every lookup (that refusal already exists in
//      both modes), so whatever the RAM holds is unreachable, not hazardous.
//      Exactly as deterministic as (3).
//
// (3) and (4) are BOTH built, selected by the VALID_MODE parameter, because
// they serve different producers. (4) costs a real restriction -- fill in
// order, fill completely -- which a tessellator walking a lattice satisfies by
// construction and a fill-on-miss producer does not. GEOM.WCACHE's producer
// fills on lookup misses, in triangle order: it keeps (3). A terrain shell
// projecting a whole 9x9 subpatch before replay is dense by construction and
// takes (4), paying 4x7 = 28 count-register bits where (3) would pay 4x81 =
// 324 bitmap flops (registers, not ALM -- no fit row exists for any shape of
// this block, so ALM is unknown). For a much deeper BITMAP instantiation the
// remaining alternative is a clear WALK that holds `sealed` low until it
// completes -- deterministic for the same reason, at the cost of DEPTH cycles
// once per arena per frame. Not built until something needs it.
//
// The generation is NOT redundant with the valid bitmap. The bitmap is
// per-slot; the mistake it cannot catch is per-arena -- a consumer replaying
// keys from a PREVIOUS use of a reopened arena. Every one of those keys is
// individually plausible; only the generation says they are stale.

module zhao_vertex_arena_dense_mutant #(
    parameter int unsigned ARENAS    = 2,
    parameter int unsigned DEPTH     = 1089,   // 33x33 terrain lattice
    parameter int unsigned PAYLOAD_W = 64,
    parameter int unsigned GEN_W     = 8,
    // THE VALID MECHANISM (see the header essay). Named values, defined as
    // localparams just below the port list:
    //   0 = VALID_BITMAP     -- per-slot valid flops, cleared per arena on
    //                           open. Any fill order. Today's behaviour, and
    //                           the DEFAULT so nothing regresses.
    //   1 = VALID_DENSE_SEAL -- the fill must be dense and in order (an
    //                           accepted fill's index equals the arena's fill
    //                           count) and a seal is REFUSED unless the count
    //                           is exactly DEPTH (refusal sticky on
    //                           arena_seal_short_o). Deletes the bitmap.
    parameter int unsigned VALID_MODE = 0,
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
    output logic                       arena_overflow_o,
    // DENSE_SEAL only: a seal was refused because the arena was not completely
    // filled. Sticky, like arena_overflow_o, and its own bit rather than a
    // fold into overflow -- a refusal folded into a neighbour's count is a
    // refusal nobody investigates. Constant 0 in VALID_BITMAP mode, where a
    // seal is never refused for incompleteness; shells in that mode tie it to
    // an *_unused wire.
    output logic                       arena_seal_short_o
`ifdef FORMAL
    ,// FORMAL ONLY -- the symbolic watched key of the shadow proof below.
    // It is a PORT, not a local, because the frontend of this flow ties
    // attribute-carrying LOCALS to constants/x: an `(* anyconst *) logic`
    // declared inside the module is NOT free, and assumptions written about
    // it do not bind. video_linebuf_fv.sv records the same trap and uses the
    // same escape -- the symbolic constant enters through the port list and
    // is held constant by assumption. Proven here: with the key local, an
    // `assert` of the assumed bound FAILS at step 3; moved to a port, with
    // nothing else changed, the identical assert PASSES.
    input logic [ARENA_W-1:0] f_arena,
    input logic [INDEX_W-1:0] f_index
`endif
);

  localparam int unsigned AW = $clog2(ARENAS);
  localparam int unsigned IW = $clog2(DEPTH);

  // The named VALID_MODE values. Instantiators pass the literal (a localparam
  // is not visible from outside the module); these names are what the literal
  // MEANS, and the generate blocks below select on them.
  localparam int unsigned VALID_BITMAP     = 32'd0;
  localparam int unsigned VALID_DENSE_SEAL = 32'd1;
  // Dense mode's whole cost: one fill counter per arena, wide enough to hold
  // the value DEPTH itself (the "complete" mark), hence DEPTH+1.
  localparam int unsigned CNT_W = $clog2(DEPTH + 1);

  // Elaboration guard. Quartus 17 requires this inside `initial begin`, and
  // `--lint-only` does not run initial blocks -- a clean lint says nothing
  // about this check (CLAUDE.md, 2026-09-09).
  initial begin
    if (VALID_MODE > VALID_DENSE_SEAL)
      $fatal(1, "zhao_vertex_arena: VALID_MODE (%0d) is not a defined mode",
             VALID_MODE);
  end

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
  logic signed [31:0]    org_x_q [0:ARENAS-1];
  logic signed [31:0]    org_y_q [0:ARENAS-1];
  logic signed [31:0]    org_z_q [0:ARENAS-1];

  // ---- what each valid-mechanism mode supplies ----------------------------
  // Exactly four wires cross the mode boundary. Everything else -- the store,
  // the refusal order, the reply register, the counters -- is mode-blind.
  logic fill_gate_c;       // extra fill-acceptance term (bitmap: constant 1)
  logic seal_gate_c;       // seal-acceptance term       (bitmap: constant 1)
  logic seal_short_now_c;  // dense: this cycle's seal is refused as incomplete
  logic slot_written_c;    // "the looked-up slot was written THIS lifetime"

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
  // `fill_gate_c` is the mode's own acceptance term: constant 1 in bitmap
  // mode (so this line is bit-identical to what stood before VALID_MODE
  // existed), and the dense in-order check otherwise. A fill the mode refuses
  // is a DROP -- sticky on arena_overflow_o like every other dropped fill --
  // because an out-of-order fill in dense mode is a producer bug of exactly
  // the same class as a fill to a sealed arena.
  wire fill_ok        = rst_n && fill_valid_i && !fill_bad_index && !fill_bad_arena && !fill_sealed && fill_gate_c;
  wire fill_drop      = fill_valid_i && (fill_bad_index || fill_bad_arena || fill_sealed || !fill_gate_c);

  // Every arena-indexed channel is range-checked, not just the two that answer
  // a consumer. -Wall found these by noticing the top bit went unread.
  wire org_bad_arena  = (org_arena_i  >= ARENA_W'(ARENAS));
  wire seal_bad_arena = (seal_arena_i >= ARENA_W'(ARENAS));
  wire open_bad_arena = (open_arena_i >= ARENA_W'(ARENAS));

  // LINEAR ADDRESSING -- arena*DEPTH + index -- REPAIRED 2026-09-09.
  //
  // These were `{arena[AW-1:0], index[IW-1:0]}` concatenations: a PADDED
  // stride of 2^IW rows per arena, into an array allocated ARENAS*DEPTH rows
  // with no padding. At any non-power-of-two DEPTH those disagree: at the
  // shell's 2x1089 shape arena 1's base is 2048 in a 2,178-row array, so
  // index 130 is already past the end -- fills silently lost, lookups
  // deterministically missing, for 88% of arena 1. The open-clear loop below
  // was ALREADY linear (`valid_q[open_arena_i * DEPTH + i]`), so the block
  // disagreed with ITSELF about where a slot lives; so is the zref oracle
  // (`arena * depth_ + index`), which is why no differential caught it: the
  // directed suite runs DEPTH=16 and the formal shape DEPTH=4, both powers of
  // two, where concat and linear coincide bit-for-bit. Found by the
  // zhao_proj_arena3 design study ("ADDRESSING IS PADDED, NEVER MIXED"); the
  // repair chooses linear rather than padding because padding buys nothing
  // here (no M10K aspect is saved) and costs 2^(AW+IW)-ARENAS*DEPTH rows.
  // The multiply is by the constant DEPTH -- shift-add fabric, no DSP.
  assign wr_addr = (AW+IW)'(32'(fill_arena_i[AW-1:0]) * DEPTH + 32'(fill_index_i[IW-1:0]));
  assign rd_addr = (AW+IW)'(32'(look_arena_i[AW-1:0]) * DEPTH + 32'(look_index_i[IW-1:0]));

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
  // `slot_written_c` is the mode's answer to "written this lifetime": the
  // bitmap bit in bitmap mode, index < fill-count in dense mode. In dense
  // mode a non-refused lookup can in fact NEVER miss -- refusal already
  // requires sealed, sealed requires count == DEPTH, and the index bound is
  // checked -- but the miss path is kept in the silicon so the miss counter
  // stays a live detector (its positive control is the committed seal-guard
  // mutant, tests/mutants/zhao_vertex_arena_dense_mutant.sv).
  wire look_present   = look_valid_i && !look_refuse && slot_written_c;
  wire look_miss      = look_valid_i && !look_refuse && !slot_written_c;

  // ---- the memory process: CLOCK ONLY -------------------------------------
  always_ff @(posedge clk) begin
    if (fill_ok) mem[wr_addr] <= fill_payload_i;
    mem_q <= mem[rd_addr];
  end

  // ---- metadata process ---------------------------------------------------
  // Mode-blind: the valid mechanism's own state (the bitmap, or the fill
  // counters) lives in the generate blocks below, in ITS OWN always_ff over
  // its own registers -- same clock, same reset, so splitting the process
  // changes no cycle's outcome.
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
      arena_overflow_o   <= 1'b0;
      arena_seal_short_o <= 1'b0;
    end else begin
      // OPEN: bump the generation, unseal. The mode block drops its own
      // lifetime state (bitmap bits, or the fill counter) on the same edge.
      // The payload memory is untouched -- staleness is answered by metadata,
      // never by contents, which is what lets the store infer.
      if (open_i && !open_bad_arena) begin
        gen_q[open_arena_i[AW-1:0]]    <= gen_q[open_arena_i[AW-1:0]] + GEN_W'(1);
        sealed_q[open_arena_i[AW-1:0]] <= 1'b0;
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

      if (fill_drop) arena_overflow_o <= 1'b1;   // sticky
      // A seal the mode refuses (dense, incomplete) leaves the arena open and
      // fillable -- the producer can complete and seal again -- and is sticky
      // on its own bit. A bad-arena seal stays what it always was: a silent
      // no-op (a pre-existing observability gap, preserved verbatim so the
      // default mode is bit-identical; noted in the consolidation report).
      if (seal_i && !seal_bad_arena && seal_gate_c)
        sealed_q[seal_arena_i[AW-1:0]] <= 1'b1;
      if (seal_short_now_c) arena_seal_short_o <= 1'b1;   // sticky
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

  // f_arena / f_index arrive as FORMAL-only PORTS (see the port list for why).

  // THE WATCHED KEY MUST NAME A REAL SLOT.
  // Without this the solver may choose f_arena/f_index OUT OF RANGE -- the
  // ports are one bit wider than the address precisely so illegal keys are
  // expressible -- and then the shadow tracks a slot that does not exist while
  // the truncated address aliases onto one that does. That is a defect in the
  // QUESTION, not in the arena: 'a lookup never wraps into another vertex' is a
  // claim about real vertices.
  // A PORT is free every cycle, where an anyconst would have been constant by
  // construction, so the constancy the watched-slot technique needs is now
  // stated explicitly alongside the bound.
  // ENFORCED-BY: tests/formal/geom_wcache_arena_bounds.sby:a_probe_key_bounded
  // ENFORCED-BY: tests/formal/geom_wcache_arena_bounds.sby:a_probe_key_constant
  always_ff @(posedge clk) begin
    assume (f_arena < ARENA_W'(ARENAS));
    assume (f_index < INDEX_W'(DEPTH));
    if (f_past_valid) begin
      assume (f_arena == $past(f_arena));
      assume (f_index == $past(f_index));
    end
  end

  logic [PAYLOAD_W-1:0] f_shadow;
  logic                 f_shadow_valid;   // written since the last open
  logic [GEN_W-1:0]     f_shadow_gen;

  // THE SHADOW KEYS OFF THE ADDRESS, NOT THE KEY -- and the difference was a
  // real defect in this proof, found by c_fill_addr_alias being REACHABLE.
  //
  // The RTL writes and reads by the TRUNCATED address {arena[AW-1:0],
  // index[IW-1:0]}. The first version of this shadow compared the FULL-WIDTH
  // key instead, so a fill could land in the watched slot without the shadow
  // recording it, and bmc then refuted `a_hit_implies_written` by exhibiting a
  // hit the shadow believed had never been written. The arena was right; the
  // question was wrong.
  //
  // Four earlier hypotheses -- read-during-write, missing reset, fills during
  // reset, an out-of-range watched key -- were all refuted by experiment before
  // this one was found by asking the solver a DIRECT question instead of
  // proposing another theory.
  // Linear, exactly as the RTL addresses (see the wr_addr repair note).
  wire [AW+IW-1:0] f_addr = (AW+IW)'(32'(f_arena[AW-1:0]) * DEPTH + 32'(f_index[IW-1:0]));
  wire f_watched_fill = fill_ok && (wr_addr == f_addr);
  wire f_watched_look = look_valid_i && (rd_addr == f_addr);

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
      // 0. THE HARNESS ITSELF. Assert what the harness assumes, so that a
      //    binding failure is a LOUD failure rather than a proof that quietly
      //    asks nothing. This is FIRST on purpose: bmc names only the first
      //    failing assertion at a step, so if the watched key ever comes
      //    unbound again this is the name the log carries, instead of a
      //    property about lookups failing on a trace where nothing is looked
      //    up. Three days were spent on that reading once.
      a_probe_key_bounded: assert (f_arena < ARENA_W'(ARENAS) &&
                                   f_index < INDEX_W'(DEPTH));
      if (f_past_valid) begin
        a_probe_key_constant: assert (f_arena == $past(f_arena) &&
                                      f_index == $past(f_index));
      end

      // 1. + 2. THE ONE THAT MATTERS. A hit on the watched key returns the
      //         value written to THAT key, never another slot's and never a
      //         previous generation's. This is "it must never wrap into
      //         another vertex", stated so a solver can refute it.
      if (f_look_was_watched && rep_hit_o) begin
        a_hit_is_the_watched_value: assert (rep_payload_o == f_expect);
        a_hit_implies_written:      assert (f_expect_valid);
      end

      // THE CAUSE, LOCALISED -- and it is KEPT FAILING on purpose.
      //
      // `a_hit_implies_written` fails at k=4. This says the same thing one step
      // earlier and in terms of the ARRAY rather than the reply: if the store
      // believes the watched slot is valid, the shadow must believe it too. It
      // fails at **k=3**, so the divergence happens at the UPDATE, not in the
      // reply path -- the shadow misses a fill that the array records.
      //
      // Its companion direction, `!f_shadow_valid || valid_q[f_addr]`, returns
      // UNSAT: the shadow never claims a fill the array does not have. So the
      // gap is one-directional, which narrows it to `f_watched_fill`.
      //
      // THE VCD CANNOT SETTLE THIS AND SHOULD NOT BE TRUSTED HERE. Its trace
      // shows `fill_ok` high with `wr_addr` equal to the watched address while
      // `f_watched_fill` is low, which is impossible for a combinational AND of
      // those two terms -- and `f_arena`/`f_index` are `anyconst`, so the
      // watched address cannot have moved between the fill and the lookup. The
      // most likely reading is that yosys merged `wr_addr` and `rd_addr`, which
      // are structurally identical expressions, so the dumped column is not the
      // signal it is labelled. This file already recorded that same trap once.
      // The instrument for the rest of this is ASSERTIONS, not dumps.
      //
      // p_array_implies_shadow ITSELF now lives in g_bitmap above (2026-09-09,
      // the VALID_MODE consolidation): it differences the bitmap against the
      // shadow, and dense mode has no bitmap -- its analogue is g_dense's
      // p_cnt_implies_shadow. The war story below is kept HERE, unedited,
      // because it is about the harness and the flow, not about the bitmap.

      // a_shadow_tracks_valid: THE DIAGNOSIS ABOVE WAS RIGHT AND THE FIX WAS
      // ELSEWHERE. 2026-08-25.
      //
      // It said `f_shadow_valid == valid_q[f_addr]` and failed at k=3 on a
      // COMPLETELY IDLE trace -- no open, no fill, no seal, valid_q all zero,
      // f_shadow_valid zero. The note here concluded that the signal read was
      // not the signal dumped, because yosys models an unpacked array as a
      // MEMORY. That was correct.
      //
      // What it did not follow through: **a bulk asynchronous reset over every
      // cell is not expressible as a memory reset either.** The solver was
      // therefore free to start cells at 1, which is how bmc could exhibit a
      // HIT on a slot that was never filled.
      //
      // `valid_q` is now a PACKED vector (see its declaration), so it is a
      // register the solver must reset. Re-asking this question with the packed
      // vector returns UNSAT at k=3 in both directions -- shadow implies array
      // and array implies shadow -- so the invariant HOLDS and is no longer
      // unaskable. It is left out of the shipped property set because the
      // proof's remaining failure is not here, and an assertion that passes is
      // not evidence of the thing that does not.
      //
      // (That was true when written. 2026-08-27: the proof PASSES -- see the
      // resolution below -- and the CI lane is registered as
      // formal_geom_wcache_arena_bounds.)
      //
      // 2026-08-26, RE-RUN WITH A SOLVER THAT NAMES THE PROPERTY. The shipped
      // engine is `btor btormc`, which reports only FAIL and leaves the reader
      // to guess which assertion broke; the stored logfile names nothing. Run
      // under `smtbmc yices` instead, the answer is explicit:
      //
      //     failed assertion zhao_vertex_arena.p_array_implies_shadow
      //     at zhao_vertex_arena.sv:423 step 3
      //
      // TWO THINGS THAT CHANGES.
      //
      // 1. The note above says re-asking this with the packed vector "returns
      //    UNSAT at k=3 in BOTH directions -- so the invariant HOLDS". **The
      //    shipped assertion still fails at step 3.** Whatever was re-asked, it
      //    was not this. That claim should not be relied on.
      //
      // 2. The counterexample is again a COMPLETELY IDLE trace: rst_n released,
      //    no open, no fill, `valid_q` all zeros, `f_shadow_valid` zero. With
      //    `valid_q` all zero, `!valid_q[f_addr]` is true for every in-range
      //    f_addr and the assertion is trivially satisfied -- so the dump is not
      //    showing the signals the solver is reasoning about. That is the SAME
      //    trap this file records twice above, and it survived the packed-vector
      //    change, which means the packed vector was not the whole of it.
      //
      // 2026-08-26, TESTED, AND IT IS THE HARNESS: THE BOUNDING ASSUMES DO
      // NOT BIND.
      //
      // Asserting the EXACT expression the harness assumes --
      //
      //     assert (f_arena < ARENA_W'(ARENAS) && f_index < INDEX_W'(DEPTH));
      //
      // -- FAILS at step 3. The watched key is not constrained at the step the
      // property is checked, so the solver is free to pick a key naming no real
      // slot, which is the very defect the assume block's own comment says it
      // exists to prevent.
      //
      // HOW THIS HID FOR SO LONG, and it is worth knowing for every proof in
      // this tree: **BMC reports only the FIRST failing assertion at a step.**
      // With `p_array_implies_shadow` present, that is the one named, and any
      // probe added beside it looks like it passed. Silencing the compound
      // assertion one at a time is what made the harness defect visible:
      //
      //     probes with p_array_implies_shadow present -> only it is named
      //     p_array_implies_shadow silenced             -> a_probe_slot_clear FAILS
      //     that silenced too                           -> a_probe_no_watched_fill FAILS
      //     those silenced too                          -> a_probe_key_bounded FAILS
      //
      // So the earlier readings that the array bit was set and a watched fill
      // occurred are TRUE, and the reason both are reachable is that the key is
      // unconstrained underneath them.
      //
      // WHAT THIS DOES NOT SAY. It does not say the arena is correct. It says
      // the current counterexample is not evidence that it is wrong, because
      // the question was asked with an unbound key. The proof cannot be
      // believed in either direction until the assume binds -- and the fix
      // belongs in the HARNESS, not in the arena.
      //
      // 2026-08-26, MECHANISM ESTABLISHED: THE ASSUME IS INERT, IN EVERY FORM.
      //
      // Asserting the same expression the harness assumes fails under ALL THREE
      // ways of writing the assumption:
      //
      //     always_ff @(posedge clk) assume (...);      FAILS at step 3
      //     always_comb              assume (...);      FAILS at step 3
      //     assume property (@(posedge clk) ...);       FAILS at step 3
      //
      // And the assertion machinery itself is fine on the same signals in the
      // same block:
      //
      //     assert (f_arena <= 2'd3 && f_index <= 3'd7)   PASSES  (trivially true)
      //     assert (f_arena < ARENA_W'(ARENAS))           FAILS   (the assumed bound)
      //
      // So the solver reads `f_arena` correctly and picks 2 or 3 for it anyway,
      // in defiance of three assumptions that forbid exactly that.
      //
      // HOW FAR THIS REACHES -- NARROWED BY EXPERIMENT, because the first
      // version of this note over-reached.
      //
      // It said every proof bounding a variable with an assume was suspect.
      // `tests/formal/video_scanout_linebuf.sby` refutes that: fourteen assumes
      // on ORDINARY PORTS, and both its bmc and cover tasks PASS -- the covers
      // being what stops a vacuous pass from looking like a real one. So
      // assumptions DO bind in this flow on ordinary signals.
      //
      // What does not bind is the `(* anyconst *)` case, and this file is the
      // ONLY one in the tree that uses it. The audit is therefore one file, not
      // a sweep of every proof.
      //
      // The check itself is still the cheapest one available and worth keeping
      // in mind for any future free-variable proof: ASSERT WHAT YOU ASSUME, and
      // see whether it holds.
      //
      // 2026-08-26, RESOLVED. IT WAS THE LOCAL, AND THE RULE WAS ALREADY
      // WRITTEN DOWN IN THIS TREE.
      //
      // The question above -- read_slang, yosys, or this file -- has a fourth
      // answer: the declaration. The frontend of this flow ties
      // ATTRIBUTE-CARRYING LOCALS to constants/x, so an `(* anyconst *) logic`
      // declared inside a module is not a free variable at all, and every
      // assumption written about it constrains nothing. Move the same signal
      // into the PORT LIST and it becomes free, assumptions bind, and the
      // proof asks the question it was written to ask.
      //
      // The experiment, one variable at a time, same file, same engine:
      //
      //     f_arena/f_index as (* anyconst *) LOCALS
      //         assert (f_arena < ARENAS && f_index < DEPTH)   FAILS  step 3
      //     the attribute removed, held constant by assumption, still LOCAL
      //         same assert                                    FAILS  step 3
      //     the identical signals moved to the PORT LIST
      //         same assert                                    PASSES
      //
      // The middle row is the one that matters: dropping the attribute did not
      // help, so it is not `anyconst` that is broken. The note above saying
      // 'what does not bind is the (* anyconst *) case' is therefore WRONG in
      // the same way its predecessor was -- both narrowed to the last thing
      // that had changed instead of to the thing that was different. What does
      // not bind is a free variable declared as a LOCAL.
      //
      // AND THIS WAS NOT NEW. Four files in tests/formal already record it:
      //
      //     video_linebuf_fv.sv     'the anyconst idiom, but as a port'
      //     formal_mem_arbiter.sv   'must be PORTS, not (* anyseq *) locals'
      //     formal_mem_guard.sv     'deliberately NOT (* anyseq *) locals'
      //     formal_mem_refresh.sv   'carried on PORTS because locals do not'
      //
      // It is called the W2.5 ratification note there. This file is the only
      // one in the tree that broke the rule, and the cost of breaking it was
      // three separate wrong diagnoses -- read-during-write ordering, then the
      // unpacked-array reset, then anyconst itself -- each of which was a real
      // finding about something else and none of which was this.
      //
      // THE READING THAT WOULD HAVE SHORTENED THIS: the first counterexample
      // was an IDLE trace, and an idle trace cannot violate a property about
      // lookups. That is a harness signature, not a design signature, and it
      // was visible on day one. A counterexample that exercises nothing is
      // evidence about the question, not about the machine.
      //
      // 2026-08-27, PROVED -- AND BY A STRONGER QUESTION THAN THE ONE THAT
      // WOULD NOT FINISH.
      //
      // With the key bound, bmc ran CLEAN to k=20 and then spent over three
      // hours on k=21, with three more steps to go and each worse. It was not
      // failing; it could not finish, which is a different problem and not a
      // better one.
      //
      // The SCOPE-TOTAL note in the .sby had been arguing all along that these
      // assertions are step-local, so depth cannot matter. That argument is
      // INDUCTION, and bmc does not do induction -- it does bounded search. So
      // the file was asking an engine for something it structurally could not
      // give, and paying in hours.
      //
      // `prove` (abc pdr) returns PASS in FOUR SECONDS: an inductive
      // invariant, true at every depth rather than to 24. `cover` passes with
      // six traces, so none of this is vacuous.
      //
      // The lesson is not about this arena. It is that the SHAPE of the
      // question decides the cost, twice over in one file: an unbound key made
      // three days of counterexamples meaningless, and a bounded engine made
      // the answer unaffordable. Neither was a fact about the hardware.
      //
      // The cheapest check remains the one this file already names: ASSERT
      // WHAT YOU ASSUME. It located this in one run once it was finally asked
      // of the declaration rather than of the assumption.

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
      // c_miss moved into g_bitmap (2026-09-09): dense mode PROVES a miss
      // unreachable (a_dense_no_miss), and a cover that can never fire would
      // red the lane forever -- the c_fill_addr_alias reasoning again.
      c_refuse:     cover (rep_valid_o && rep_refuse_o);
      c_watched_hit:cover (f_look_was_watched && rep_hit_o);
      c_overflow:   cover (arena_overflow_o);
      c_regen:      cover (f_past_valid && open_i && !open_bad_arena);
      // c_fill_addr_alias lived here and did its job: it was REACHABLE, which
      // proved the shadow's full-width key compare could miss a fill the RTL
      // applied to the watched address. After keying the shadow off the address
      // it became UNREACHABLE -- so it is removed rather than left as a cover
      // that can never fire, which would red the lane forever for a condition
      // the fix deliberately eliminated.
    end
  end
`endif

  // ---- the valid mechanism, selected by VALID_MODE ------------------------
  // Explicit generate/endgenerate: Quartus 17 rejects the implicit form
  // ("syntax error near text: `if`; expecting `endmodule`", CLAUDE.md
  // 2026-09-08).
  generate
    if (VALID_MODE != VALID_BITMAP) begin : g_dense
      // (Any non-bitmap value lands here; values above VALID_DENSE_SEAL are
      // refused by the elaboration guard's $fatal.)
      // THE WHOLE MECHANISM: one fill counter per arena. An accepted fill's
      // index equals the count (in-order, exactly once), the count is the
      // write address's own provenance, and a seal is refused below DEPTH.
      // "Written this lifetime" is then index < count -- and after a seal,
      // count == DEPTH, so every in-range index qualifies.
      logic [CNT_W-1:0] cnt_q [0:ARENAS-1];

      // A fill racing an OPEN of the same arena on the same cycle is REFUSED,
      // not honoured: honouring it would leave the new lifetime's count
      // claiming rows the old lifetime wrote. (Bitmap mode resolves the same
      // race fill-wins, which is sound THERE because the bitmap bit and the
      // payload move together; here the count is the only witness, so the
      // race must lose. The formal shadow agrees: its open-clear and
      // fill-set would otherwise disagree with the count.)
      wire fill_open_race = open_i && !open_bad_arena &&
                            (open_arena_i[AW-1:0] == fill_arena_i[AW-1:0]);
      // A seal racing an OPEN of the same arena must lose too -- FOUND BY THE
      // PROOF (prove_dense counterexample, step 7, first run): with seal
      // winning the sealed bit (bitmap mode's behaviour, harmless there
      // because open also cleared the bits) while open zeroes the count, the
      // arena lands SEALED with cnt = 0, violating seal-implies-complete.
      // Refused instead, sticky on arena_seal_short_o like any other refused
      // seal. Bitmap mode keeps its historical seal-wins resolution verbatim.
      wire seal_open_race = open_i && !open_bad_arena &&
                            (open_arena_i[AW-1:0] == seal_arena_i[AW-1:0]);

      assign fill_gate_c = (32'(fill_index_i[IW-1:0]) ==
                            32'(cnt_q[fill_arena_i[AW-1:0]])) && !fill_open_race;
      // MUTANT: `==` weakened to `<=` -- a seal of an incomplete arena is
      // accepted, which is the one substantive change in this file.
      assign seal_gate_c = (32'(cnt_q[seal_arena_i[AW-1:0]]) <= DEPTH) &&
                           !seal_open_race;
      assign seal_short_now_c = seal_i && !seal_bad_arena && !seal_gate_c;
      assign slot_written_c = (32'(look_index_i[IW-1:0]) <
                               32'(cnt_q[look_arena_i[AW-1:0]]));

      integer d;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          for (d = 0; d < ARENAS; d = d + 1) cnt_q[d] <= '0;
        end else begin
          if (fill_ok)
            cnt_q[fill_arena_i[AW-1:0]] <= cnt_q[fill_arena_i[AW-1:0]] + CNT_W'(1);
          // Open wins the write port (the race is already refused at
          // fill_gate_c; this ordering is belt and braces, stated).
          if (open_i && !open_bad_arena) cnt_q[open_arena_i[AW-1:0]] <= '0;
        end
      end

`ifdef FORMAL
      // DENSE-MODE PROPERTIES. The shared block below carries everything
      // mode-blind; these are the invariants the bitmap mode does not have.
      always_ff @(posedge clk) begin
        if (f_past_valid && rst_n && $past(rst_n)) begin
          // The dense analogue of p_array_implies_shadow, and the induction
          // helper: if the count says the watched slot was written this
          // lifetime, the shadow must have seen that write.
          p_cnt_implies_shadow: assert (
              !(32'(f_index[IW-1:0]) < 32'(cnt_q[f_arena[AW-1:0]]))
              || f_shadow_valid);

          // Fill is in-order: an accepted fill's index IS the count. (True by
          // the wiring of fill_gate_c; asserted so the wiring itself is what
          // the proof certifies, not a reading of it.)
          a_dense_fill_in_order: assert (
              !fill_ok || (32'(fill_index_i[IW-1:0]) ==
                           32'(cnt_q[fill_arena_i[AW-1:0]])));

          // Per-arena state invariants, over EVERY arena (elaboration-bounded
          // loop): the count never exceeds DEPTH, and a sealed arena is a
          // COMPLETE arena -- the property that makes the RAM's stale
          // contents unreachable rather than hazardous.
          for (int a = 0; a < ARENAS; a++) begin
            assert (32'(cnt_q[a]) <= DEPTH);              // cnt bounded
            assert (!sealed_q[a] || (32'(cnt_q[a]) == DEPTH)); // seal => complete
          end

          // Read implies written: a reply in dense mode is a hit or a
          // refusal, NEVER a miss -- the miss path exists only as the
          // detector the committed mutant fires.
          a_dense_no_miss: assert (!rep_valid_o || rep_hit_o || rep_refuse_o);
        end
      end
      always_ff @(posedge clk) begin
        if (rst_n) begin
          c_dense_sealed:     cover (sealed_q[f_arena[AW-1:0]]);
          c_dense_seal_short: cover (arena_seal_short_o);
        end
      end
`endif
    end else begin : g_bitmap
      // PACKED, NOT UNPACKED, AND THE PROOF IS WHY.
      //
      // This was `logic valid_q [0:(ARENAS*DEPTH)-1]`, an unpacked array.
      // Yosys models an unpacked array as a MEMORY, and a bulk asynchronous
      // reset over every cell is not expressible as a memory reset -- so the
      // solver was free to start cells at 1. bmc then exhibited a HIT on a
      // slot that was never filled, which is `a_hit_implies_written` failing
      // at k = 4, and the same modelling gap is why an earlier
      // `a_shadow_tracks_valid` appeared to fail on a COMPLETELY IDLE trace
      // and was withdrawn as unaskable.
      //
      // A packed vector is a single wide register: the reset is one
      // assignment the solver must honour, and `valid_q[addr]` still
      // bit-selects exactly as before. It also makes the "in flops" claim
      // STRUCTURAL rather than a hope -- an unpacked array is what a
      // synthesiser converts to memory when it feels like it, and the storage
      // law (QUARTUS_GOTCHAS §10) only kept this one in logic because the
      // reset happens to touch every cell.
      logic [(ARENAS*DEPTH)-1:0] valid_q;

      assign fill_gate_c      = 1'b1;
      assign seal_gate_c      = 1'b1;
      assign seal_short_now_c = 1'b0;
      assign slot_written_c   = valid_q[rd_addr];

      integer vb;
      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          valid_q <= '0;   // one register, one reset -- see the declaration
        end else begin
          // OPEN drops this arena's valid bits; a fill accepted on the same
          // edge wins for its own bit (last assignment), which is the
          // behaviour this block has always had.
          if (open_i && !open_bad_arena)
            for (vb = 0; vb < DEPTH; vb = vb + 1)
              valid_q[open_arena_i * DEPTH + vb] <= 1'b0;
          if (fill_ok) valid_q[wr_addr] <= 1'b1;
        end
      end

`ifdef FORMAL
      // p_array_implies_shadow and c_miss live HERE because both are
      // statements about the bitmap: the first differences the bitmap
      // against the shadow, and the second covers a state (sealed, current
      // generation, slot never filled) that dense mode makes unreachable --
      // an unreachable cover would red the lane forever, the same reasoning
      // that removed c_fill_addr_alias. The three-day diagnostic saga behind
      // p_array_implies_shadow is preserved in the shared FORMAL block below.
      always_ff @(posedge clk) begin
        if (f_past_valid && rst_n && $past(rst_n)) begin
          p_array_implies_shadow: assert (!valid_q[f_addr] || f_shadow_valid);
        end
      end
      always_ff @(posedge clk) begin
        if (rst_n) begin
          c_miss: cover (rep_valid_o && !rep_hit_o && !rep_refuse_o);
        end
      end
`endif
    end
  endgenerate

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


// (The FORMAL harness and shared properties were HOISTED above the
// valid-mechanism generate blocks on 2026-09-09: slang requires declaration
// before use, and the per-mode formal code inside those blocks references the
// shared shadow. Nothing in the harness itself changed by the move.)


endmodule
