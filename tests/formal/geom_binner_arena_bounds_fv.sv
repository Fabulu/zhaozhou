// geom_binner_arena_bounds_fv.sv — formal harness for the GEOM.BINNER arena
// bound (ZH-058 / ZH-026; property geom_binner_arena_bounds.sby).
//
// WHAT IS PROVED, and why it is the right thing to prove.
//
// The DUT is zhao_geom_arena — the EXACT module zhao_geom_binner instantiates
// to hand out chunks. The ledger's note is "Safe overflow: excess triangles
// degrade to next-frame, NEVER SCRIBBLE", and "never scribble" reduces to one
// arithmetic fact: the allocator must never present a grant for an index at or
// past the end of the arena, no matter how many requests arrive, in any order,
// with any interleaving of frame releases. Everything else about the binner's
// overflow behaviour — which triangle is dropped, when the wall goes up — is
// policy, checked by the differential lanes; THIS is the safety property, and
// it is a bounded arithmetic core, so it is provable.
//
//   P1  a_bound       used <= CHUNKS, always. The occupancy counter can never
//                     run past the arena.
//   P2  a_in_range    a GRANT always names a chunk strictly inside the arena.
//                     This is the "never scribble" statement itself: a caller
//                     that respects alloc_ok_o cannot address arena[CHUNKS].
//   P3  a_wall        full => no grant. Once the arena is exhausted the
//                     allocator refuses every further request until a release,
//                     which is what makes the binner's frame wall total.
//   P4  a_distinct    outside a release, `used` never decreases, and a grant
//                     advances it by EXACTLY one. Two grants therefore never
//                     name the same chunk: the pointer is strictly increasing
//                     between releases, so no live tile list can be handed a
//                     chunk another list already owns. (The one-step form is
//                     what is proved; distinctness over all time follows from
//                     it together with P1, and is stated rather than restated
//                     as a temporal property the engine cannot carry.)
//   P5  a_release     a release empties the arena in one cycle — there is no
//                     residue for the next frame to trip over.
//
// The task is `prove` (temporal induction), not `bmc`. Every assertion above
// is ONE-STEP INDUCTIVE: each holds in the next state given only that it held
// in this one, so induction closes it for ALL time rather than for the first
// few cycles. That matters here in a way it did not for the fill rule: the
// interesting state — a FULL arena — is 256 grants away from reset, which no
// bounded run of a sane depth would ever reach.
//
// The cover task carries a SECOND instance, deliberately tiny (CHUNKS = 4), so
// that "the arena actually fills" and "the wall actually refuses" are shown to
// be REACHABLE inside a short cover trace. Without them P1 and P3 would also
// hold for an allocator that never granted anything at all, and this file
// would not be testing what its comments claim. The two instances are the same
// module with different parameters, and the inductive proof above is
// parameter-independent — it never mentions the value of CHUNKS, only the
// relation between `used`, CAP and the grant.

module geom_binner_arena_bounds_fv (
  input logic clk,
  input logic rst_n,
  input logic release_i,   // unconstrained
  input logic alloc_i      // unconstrained
);

  // ---- the SHIPPING instance: the parameters zhao_geom_binner uses --------
  localparam int unsigned CHUNKS = 256;
  localparam int unsigned PTR_W  = 8;

  logic             ok;
  logic [PTR_W-1:0] ptr;
  logic             full;
  logic [PTR_W:0]   used;

  zhao_geom_arena #(.CHUNKS(CHUNKS), .PTR_W(PTR_W)) u_dut (
    .clk        (clk),
    .rst_n      (rst_n),
    .release_i  (release_i),
    .alloc_i    (alloc_i),
    .alloc_ok_o (ok),
    .alloc_ptr_o(ptr),
    .full_o     (full),
    .used_o     (used)
  );

  // ---- the tiny instance, for the covers ---------------------------------
  localparam int unsigned SMALL       = 4;
  localparam int unsigned SMALL_PTR_W = 2;

  logic                   s_ok;
  logic [SMALL_PTR_W-1:0] s_ptr;
  logic                   s_full;
  logic [SMALL_PTR_W:0]   s_used;

  zhao_geom_arena #(.CHUNKS(SMALL), .PTR_W(SMALL_PTR_W)) u_small (
    .clk        (clk),
    .rst_n      (rst_n),
    .release_i  (release_i),
    .alloc_i    (alloc_i),
    .alloc_ok_o (s_ok),
    .alloc_ptr_o(s_ptr),
    .full_o     (s_full),
    .used_o     (s_used)
  );

  // ---- the initial state MUST be a reset ----------------------------------
  // `mode prove` starts from an UNCONSTRAINED initial state, and an ASYNC
  // reset only bites while rst_n is low — so without this the solver is free
  // to start with `used` already at some arbitrary value that no reachable
  // trace produces. That is not a bug in the allocator, it is a harness that
  // never turned the machine on: the first run failed the induction base case
  // at step 1 that way, and the cover task "reached" a FULL four-chunk arena in
  // one step, which is arithmetically impossible. Pinning step 0 to reset
  // fixes both, and makes the covers say something.
  logic first;
  initial first = 1'b1;
  always_ff @(posedge clk) first <= 1'b0;
  always_comb if (first) assume (!rst_n);

  // ---- history, for the one-step monotonicity property -------------------
  // RESET-INITIALISED, and gated by `past_valid`. Without both, the induction
  // BASE CASE fails at step 1 on a free initial value of `used_q` that no
  // reachable state ever produced — a false counterexample about the harness,
  // not about the allocator. (It did exactly that on the first run.)
  logic [PTR_W:0] used_q;
  logic           past_valid;
  logic           release_q;
  logic           ok_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      used_q     <= {(PTR_W+1){1'b0}};
      past_valid <= 1'b0;
      release_q  <= 1'b0;
      ok_q       <= 1'b0;
    end else begin
      used_q     <= used;
      past_valid <= 1'b1;
      release_q  <= release_i;
      ok_q       <= ok;
    end
  end

  // ---- the properties -----------------------------------------------------
  always_ff @(posedge clk) begin
    if (rst_n) begin
      // P1 — the occupancy never runs past the arena.
      a_bound: assert (used <= (PTR_W+1)'(CHUNKS));

      // P2 — NEVER SCRIBBLE: a grant always names a chunk inside the arena.
      if (ok) a_in_range: assert (used < (PTR_W+1)'(CHUNKS));

      // P3 — the wall: a full arena grants nothing, however hard it is asked.
      if (full) a_wall: assert (!ok);

      // P4 — between releases the pointer is strictly increasing, by exactly
      //      one per grant, so two grants never name the same chunk.
      if (past_valid && !release_q) begin
        a_monotone: assert (used >= used_q);
        a_step_one: assert (used == (ok_q ? used_q + (PTR_W+1)'(1) : used_q));
      end

      // P5 — a release empties the arena in one cycle; nothing survives a
      //      frame boundary for the next frame to trip over.
      if (past_valid && release_q) a_release: assert (used == (PTR_W+1)'(0));
    end
  end

  // ---- the covers: the interesting states are REACHABLE -------------------
  always_ff @(posedge clk) begin
    if (rst_n) begin
      c_grant:   cover (s_ok);
      c_fills:   cover (s_full);
      c_refuses: cover (s_full && alloc_i && !s_ok);
      c_reuse:   cover (s_full && release_i);
    end
  end

endmodule : geom_binner_arena_bounds_fv
