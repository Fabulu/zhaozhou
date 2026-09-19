// zhao_geom_group_seq_mutant.sv -- COMMITTED MUTANT. NOT PRODUCTION RTL.
//
// It exists to prove that seal_early_o in
// fpga/rtl/geometry/zhao_geom_group_seq.sv can FIRE. That counter watches for a
// seal issued before every landing has arrived, and that state is UNREACHABLE
// while the drain rule is correct -- so no legal stimulus can move it, and "it
// can fire" would stay an argument forever. CLAUDE.md, 2026-09-09: a guard you
// cannot reach with legal stimulus needs a committed mutant.
//
// THE ONLY SUBSTANTIVE CHANGE is in StSealWait: if (drain_done_c) becomes
// if (1'b1), i.e. the machine seals on the ACCEPT count instead of the
// LANDING count -- exactly the fault the counter names. Everything else is a
// byte-for-byte copy.
//
// Driven by tests/geometry/geom_group_seq_directed.cpp compiled with
// ZHAO_EXPECT_SEAL_EARLY_MUTANT, whose polarity is INVERTED: it passes when
// the counter fires.
//
// THIS IS A COPY, AND A COPY GOES STALE IN THE FLATTERING DIRECTION.
// REGENERATE IT whenever zhao_geom_group_seq.sv changes shape: a control cut
// from an old body goes on passing while measuring a machine that no longer
// exists. tools/budget/mutant_copy_drift.py watches for exactly that; the
// module name is deliberately left unchanged so that a source list holding
// both files fails loudly with a duplicate module rather than quietly picking
// one.
//
// ---------------------------------------------------------------------------
// Below this line is the copy.
// ---------------------------------------------------------------------------
// zhao_geom_group_seq.sv -- THE GEOMETRY CLIENT-A PRODUCER: one meshlet job in,
// one arena group per view out -- allocated, opened, filled through the shared
// projector, sealed, handed over, released.
//
//     zhao_geom_skin (skinned vertices, VIEW-INDEPENDENT)
//         | v_valid_i / v_ready_o
//         v
//     zhao_geom_group_seq  --- a_* --->  zhao_proj_subsystem CLIENT A
//         ^                                      | a_valid_o ...
//         | open / seal / rider                  v
//         +---------------------------  zhao_geom_proj_lane -> zhao_geom_wcache
//               fill_landed_i / fill_arena_i
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS -- AND WHY IT IS NOT GEOM.WARP
// ---------------------------------------------------------------------------
// `design/prod_manifest.yml` (zhao_geom_proj_lane, 2026-09-18) states the gap
// this file closes, in its own words:
//
//   "Selecting one shared service instead of two wrappers (6,598 ALM / 33 DSP
//    against ~12,400 / 66) needs this composed AND a producer driving it; the
//    producer is still absent, so this is not yet adoptable on its own"
//
// `zhao_proj_subsystem`'s header names the same absence: "client A raw -- the
// geometry producer ... is not composed anywhere in this tree". The LANE was
// built on 2026-09-18 and computes nothing; what was missing is the thing that
// ALLOCATES an arena, walks a meshlet's vertices into client A with the
// {arena, index} rider attached, waits for the LANDINGS and seals. That is this
// block.
//
// It is deliberately NOT `GEOM.WARP`. GEOM.WARP is DEFERRED by owner ruling
// 2026-08-31 section 6.3 (cut-order 5), its contract is deliberately unwritten,
// `design/V1-RELEASE-DEFINITION.md` line 136 excludes it from v1, and
// `design/contracts/GEOM.PROJECT.md` line 67 puts it in the ALTERNATIVE
// position -- "the vertices arrive from GEOM.WCACHE **or** GEOM.WARP" -- so it
// is not on the fill path into the arena at all and cannot be what blocks the
// projector's adoption. Building it would have added area to a design already
// far over its ALM budget, for a function v1 refuses.
//
// ---------------------------------------------------------------------------
// LAW, IN CITATION ORDER
// ---------------------------------------------------------------------------
//   fpga/rtl/geometry/zhao_geom_proj_lane.sv -- THE RIDER LAYOUT LIVES THERE,
//       NOT HERE. This block hands the lane {arena, index} on
//       rider_arena_o/rider_index_o and takes the packed word back on
//       rider_payload_i; it never packs the field itself. One file owns the
//       layout, and duplicating the pack here is exactly the duplicate
//       arithmetic this repository has shipped three times. The lane's own
//       `initial` guard asserts the rider FITS PAYLOAD_A_W; it is not repeated
//       here for the same reason.
//   fpga/rtl/common/zhao_project_service.sv -- client A: `a_ready_o` is the
//       arbiter's grant, combinational from `a_valid_i`; the result lands many
//       cycles later with the rider intact, and the result port has NO
//       backpressure.
//   fpga/rtl/geometry/zhao_geom_wcache.sv -- instantiates `zhao_vertex_arena`
//       with `VALID_MODE(0)`, the BITMAP mode. So a geometry group may be
//       SHORTER than DEPTH and still seal; that is the difference from the
//       terrain side, whose 9x9 window `zhao_terrain_group_seq` always fills
//       completely -- an ASSUMPTION on that block, upheld there, not here.
//       `job_count_i` is therefore a real per-meshlet value and not a constant.
//       ENFORCED-BY: tests/geometry/geom_group_seq_directed.cpp -- CASE 2
//       offers job_count_i = 5 against the bench's DEPTH = 8 and requires the
//       group to SEAL with its handle carrying count == 5; a constant-DEPTH
//       reading would still be waiting for eight landings. CASE 1 refuses
//       count == 0 and count > DEPTH, so the bound is read as well.
//   fpga/rtl/terrain/zhao_terrain_group_seq.sv -- the proven sibling. Its
//       lowest-free-arena search, its slot fan-out and its "seal on LANDINGS,
//       never on ACCEPTS" rule are reproduced here because they are the same
//       problem; its ModeRef replay phase is NOT, because the geometry replay
//       customer is downstream of the handle this block emits.
//   design/contracts/GEOM.WCACHE.md -- the arena's law, including that
//       identity is GIVEN by the producer and never inferred. `job_count_i`
//       and the running index are that given identity.
//
// ---------------------------------------------------------------------------
// SEAL ON LANDINGS, NEVER ON ACCEPTS -- the one rule that is easy to get wrong
// ---------------------------------------------------------------------------
// The projector core is deep. A seal issued when the last vertex was ACCEPTED
// would seal an arena whose last vertices have not yet been WRITTEN, and in
// bitmap mode that does not even fail loudly: the seal succeeds and the
// missing slots answer MISS forever, which reads as a cold cache rather than
// as a bug. So `landed_q` counts the result port's own valid, per arena, and
// the seal waits for `landed == count` on every slot.
//
// `seal_early_o` is the detector for that fault and it is STRUCTURALLY
// UNREACHABLE with legal stimulus, because the state machine leaves StSealWait
// only when the drain condition holds. It is therefore evidenced by a
// committed mutant rather than by a claim -- see
// tests/mutants/zhao_geom_group_seq_mutant.sv, whose only substantive change
// is that StSealWait advances on the ACCEPT count instead of the landing
// count, and whose driver passes when the counter FIRES.
//
// ---------------------------------------------------------------------------
// A REFUSED RECORD IS A HOLE, AND A HOLE POISONS THE BATCH (owner ruling R31)
// ---------------------------------------------------------------------------
// GEOM.VDECODE refuses a malformed record INSTEAD of emitting it (its header;
// `design/contracts/GEOM.VDECODE.md`: "unknown format_id: refused, and NO
// VERTEX EMITTED"). Until 2026-09-19 this block waited for `job_count_i`
// skinned vertices regardless, so ONE refused record left it in StFill for
// ever: no seal, no handle, GEOM.REPLAY waiting on the handle, GEOM.ASSETFETCH
// waiting on REPLAY's release -- the whole geometry path deadlocked, and no
// counter anywhere moved, because every counter watches something that ARRIVES.
//
// `hole_i` is VDECODE's refusal pulse, one per refused record. Only one meshlet
// is ever between GEOM.ASSETFETCH and this block (the dispatcher fork takes a
// meshlet only when GEOM.REPLAY is idle, and REPLAY releases it only after this
// block's handles), so a hole that arrives while a job is held belongs to that
// job by construction. The fill ends when VERTICES + HOLES == count, and the
// drain waits for LANDINGS == VERTICES SENT (not count: a hole never lands).
//
// A HOLE WITH NO JOB HELD IS CARRIED, NOT ORPHANED (review of 3dae8f10). Once a
// job's fill has ended, EVERY record of that job is accounted for -- arrived or
// refused -- so any later hole can only stand for a record of the NEXT job. It
// is held in `early_q` and charged to the next job the clock that job is
// accepted; a hole on the accept clock itself is charged to the job accepted.
// The first version counted such a hole as an orphan and charged nothing, and
// the next job's VERTICES + HOLES == count could then never be reached: the
// same hang, moved one state earlier.
//
// In the composed console an early hole is ALSO structurally excluded, and the
// handshake that excludes it is named rather than assumed: GEOM.ASSETFETCH
// streams a meshlet's vertex records only in S_SERVE, which it enters on the
// SAME `s_valid && s_ready` edge that hands this block the job (the core's
// dispatcher fork ANDs `gs_job_ready` = StIdle into `af_s_ready`), and VDECODE
// can refuse only a record it has been given. So the earliest refusal of job
// N+1 reaches `hole_i` at least one clock after this block has left StIdle for
// it. `holes_early_o` counts the carried case anyway: it reads 0 in
// composition, it is fired by stimulus in the directed test, and if it ever
// moves in composition the handshake above has stopped being true. What it
// can NOT make right is a hole belonging to a job this block REFUSED
// (`jobs_refused_o`): that job's records have no batch to fall into, and its
// vertices stall the stream as they always did -- unchanged by R31.
//
// A group with a hole is SEALED AND HANDED OVER WITH `grp_poison_o` HIGH, never
// withheld. The contract's law is "a refusal drops the BATCH, not the frame":
// the indices after a hole are shifted, so no triangle of the meshlet may be
// drawn from this arena -- but the arena must still reach GEOM.REPLAY, because
// REPLAY is the block that proves both readers are done and releases the
// buffer. Withholding the handle would be the old deadlock with extra steps.
//
// `holes_o` counts every hole (every refusal VDECODE reports);
// `groups_poisoned_o` counts the handles marked; `holes_early_o` counts the
// holes that arrived with no job held and were carried to the next. All three
// are fired by stimulus in tests/geometry/geom_group_seq_directed.cpp.
//
// ---------------------------------------------------------------------------
// THE MEMORY TRADE, WITH NUMBERS (standing owner direction: ALMs are the
// binding constraint, M10K is the slack, prefer a lookup over computation)
// ---------------------------------------------------------------------------
// THE LEVER DOES NOT APPLY HERE, and the numbers say why rather than a
// sentence asserting it:
//
//   * This block contains NO MULTIPLIER and NO TABLE. It is a state machine,
//     a running index and per-arena bookkeeping. 0 DSP is therefore an
//     ASSUMPTION, not a measurement, and it is upheld by the author of this
//     file: there is no `*` on a non-constant operand and no ROM anywhere in
//     it. Only a quartus_map report can close it and none has been run on this
//     block, so no enforcer is named here -- the assumption is stated with its
//     upholder instead. Under it, the "do not spend memory to remove DSPs"
//     half of the direction is moot.
//   * Its whole state at the defaults (ARENAS=4 -> ARENA_W=3, DEPTH=1089 ->
//     INDEX_W=12, CNT_W=11) is: held 8 bits + landed 8x11 = 88 bits + two slot
//     records (arena 3 + gen 8 + view 1) = 24 bits + job registers (count 12 +
//     src_id 16 + shape 4) = 32 bits + index/slot/state ~= 20 bits. About 172
//     flops.
//   * One M10K is 10,240 bits. Moving ~172 bits into one would spend 1 of the
//     device's 553 M10K blocks to save ~172 ALM flops -- but it would ADD
//     logic, not remove it: `held_q` must be searched COMBINATIONALLY in the
//     same cycle the open is issued, and `landed_q` must be incremented from
//     the result port while the drain comparison reads it. A synchronous-read
//     memory needs an address pipeline and a write-to-read bypass for both,
//     which costs more ALMs than the flops it removes.
//
// So the honest entry is: no M10K, 0 DSP, and the memory lever is declined
// with a reason rather than not considered.
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS NOT
// ---------------------------------------------------------------------------
// It does not project (that is `zhao_project_core`, once, shared). It does not
// store (that is `zhao_vertex_arena` through the lane). It does not skin, and
// it does not decide which meshlet to draw. It holds no payload at any point:
// the vertex it forwards is the vertex it was handed, in the same cycle.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_geom_group_seq #(
    parameter int unsigned ARENAS      = 4,
    parameter int unsigned DEPTH       = 1089,
    parameter int unsigned NVIEWS      = 2,
    parameter int unsigned GEN_W       = 8,
    parameter int unsigned PAYLOAD_A_W = 16,
    parameter int unsigned INDEX_W     = $clog2(DEPTH) + 1,
    parameter int unsigned ARENA_W     = $clog2(ARENAS) + 1
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the job: one meshlet, one or both views ---------------------------
    input  wire                     job_valid_i,
    output wire                     job_ready_o,
    input  wire [INDEX_W-1:0]       job_count_i,     // vertices, 1..DEPTH
    input  wire [NVIEWS-1:0]        job_view_mask_i, // which views to project
    input  wire [15:0]              job_src_id_i,    // opaque passthrough

    // ---- skinned vertices in, view-independent, LOCAL (rebased) coords -----
    input  wire                     v_valid_i,
    output wire                     v_ready_o,
    input  wire signed [31:0]       v_x_i,
    input  wire signed [31:0]       v_y_i,
    input  wire signed [31:0]       v_z_i,
    // One pulse per record GEOM.VDECODE REFUSED: a vertex that will never
    // arrive on `v_*`. See "A REFUSED RECORD IS A HOLE" in the header.
    input  wire                     hole_i,

    // ---- client A of the shared projection service -------------------------
    output wire                     a_valid_o,
    input  wire                     a_ready_i,
    output wire signed [31:0]       a_vx_o,
    output wire signed [31:0]       a_vy_o,
    output wire signed [31:0]       a_vz_o,
    output wire                     a_view_o,
    output wire [PAYLOAD_A_W-1:0]   a_payload_o,

    // ---- the lane's arena control; the lane owns the rider LAYOUT ----------
    output wire                     open_o,
    output wire [ARENA_W-1:0]       open_arena_o,
    input  wire [GEN_W-1:0]         open_gen_i,
    output wire [ARENA_W-1:0]       rider_arena_o,
    output wire [INDEX_W-1:0]       rider_index_o,
    input  wire [PAYLOAD_A_W-1:0]   rider_payload_i,
    output wire                     seal_o,
    output wire [ARENA_W-1:0]       seal_arena_o,

    // ---- landings, from the lane's result port -----------------------------
    input  wire                     fill_landed_i,
    input  wire [ARENA_W-1:0]       fill_arena_i,

    // ---- the sealed group handle, to the replay customer -------------------
    output wire                     grp_valid_o,
    input  wire                     grp_ready_i,
    output wire [ARENA_W-1:0]       grp_arena_o,
    output wire [GEN_W-1:0]         grp_gen_o,
    output wire [INDEX_W-1:0]       grp_count_o,
    output wire                     grp_view_o,
    output wire [15:0]              grp_src_id_o,
    // The batch lost a record: nothing in this arena may be drawn. REPLAY
    // still takes the handle, drops the meshlet's triangles and releases it.
    output wire                     grp_poison_o,

    // ---- release, driven by the replay customer when it is done ------------
    input  wire                     rel_valid_i,
    input  wire [ARENA_W-1:0]       rel_arena_i,

    // ---- counters ----------------------------------------------------------
    output logic [31:0]             groups_opened_o,
    output logic [31:0]             groups_sealed_o,
    // Client-A ACCEPTS, one per vertex PER VIEW: a dual-view job moves this by
    // twice its vertex count (the fan-out sends each vertex once per slot).
    output logic [31:0]             view_vertices_sent_o,
    output logic [31:0]             landings_o,
    output logic [31:0]             jobs_refused_o,
    output logic [31:0]             alloc_stall_cycles_o,
    output logic [31:0]             rel_unheld_o,
    output logic [31:0]             holes_o,
    output logic [31:0]             groups_poisoned_o,
    output logic [31:0]             holes_early_o,
    // FAULT, and structurally unreachable while the drain rule is correct --
    // see the header and the committed mutant.
    output logic                    seal_early_o
);

  localparam int unsigned CNT_W = $clog2(DEPTH + 1);
  // Per-arena tables are sized to the ENCODING, not to ARENAS. ARENA_W is
  // $clog2(ARENAS)+1, so a handle can name an arena that does not exist; giving
  // the tables every representable index means a stray handle writes a counter
  // nobody reads instead of aliasing a live arena or reading out of range.
  localparam int unsigned NA = 1 << ARENA_W;

  // Quartus 17 requires these inside `initial begin`; `--lint-only` does not
  // run them at all (CLAUDE.md, 2026-09-09), so a clean lint says nothing here.
  initial begin
    if (NVIEWS != 2)
      $fatal(1, "zhao_geom_group_seq: NVIEWS (%0d) must be 2 -- the slot fan-out is written for the camera pair",
             NVIEWS);
    if (ARENAS < NVIEWS)
      $fatal(1, "zhao_geom_group_seq: ARENAS (%0d) must be >= NVIEWS (%0d) -- a dual-view job holds one arena per view at once",
             ARENAS, NVIEWS);
    if (DEPTH < 1)
      $fatal(1, "zhao_geom_group_seq: DEPTH (%0d) must be positive", DEPTH);
  end

  // ---- state ---------------------------------------------------------------
  localparam logic [2:0] StIdle     = 3'd0;
  localparam logic [2:0] StAlloc    = 3'd1;
  localparam logic [2:0] StFill     = 3'd2;
  localparam logic [2:0] StSealWait = 3'd3;
  localparam logic [2:0] StSeal     = 3'd4;
  localparam logic [2:0] StHand     = 3'd5;

  logic [2:0] st;

  // the held job
  logic [INDEX_W-1:0] j_count;
  logic [15:0]        j_src;

  // the slots: slot 0 is the lowest view in the mask, slot 1 the other
  logic               two_slots_q;
  logic               slot_view_q [0:1];
  logic [ARENA_W-1:0] slot_arena_q[0:1];
  logic [GEN_W-1:0]   slot_gen_q  [0:1];

  // per arena
  logic [NA-1:0]      held_q;
  logic [CNT_W-1:0]   landed_q[0:NA-1];

  logic               alloc_slot_q;  // the slot being allocated
  logic               seal_slot_q;   // ... sealed
  logic               hand_slot_q;   // ... handed over
  logic               vs_q;          // the fan-out slot of the current vertex
  logic [INDEX_W-1:0] vi_q;          // the vertex index within the group
  logic [INDEX_W-1:0] holes_q;       // records of this job VDECODE refused
  logic [INDEX_W-1:0] early_q;       // holes that arrived with no job held,
                                     // carried to the next job (see header)

  // ---- the free arena, lowest index first ----------------------------------
  logic               any_free_c;
  logic [ARENA_W-1:0] free_c;
  integer fi;
  always_comb begin
    any_free_c = 1'b0;
    free_c     = '0;
    for (fi = ARENAS - 1; fi >= 0; fi = fi - 1) begin
      if (!held_q[fi]) begin
        any_free_c = 1'b1;
        free_c     = ARENA_W'(fi);
      end
    end
  end

  // ---- the drain condition: LANDINGS, per active slot ----------------------
  // Against the vertices actually SENT (`vi_q` once the fill has ended), not
  // against `j_count`: a hole never lands, so a batch with one would otherwise
  // wait in StSealWait for ever -- the R31 deadlock moved one state later.
  logic drain_done_c;
  always_comb begin
    drain_done_c = (landed_q[slot_arena_q[0]] == CNT_W'(vi_q));
    if (two_slots_q && (landed_q[slot_arena_q[1]] != CNT_W'(vi_q)))
      drain_done_c = 1'b0;
  end

  // ---- the job port --------------------------------------------------------
  //
  // `job_ready_o` is independent of `job_valid_i` (house hygiene). A MALFORMED
  // job is CONSUMED and counted, never held: refusing by stalling would wedge
  // the stream behind a job that can never become legal.
  assign job_ready_o = (st == StIdle);

  wire job_legal_c = (job_count_i != '0)
                  && (job_count_i <= INDEX_W'(DEPTH))
                  && (job_view_mask_i != '0);
  wire job_take_c  = job_valid_i && (st == StIdle);

  // ---- allocation ----------------------------------------------------------
  assign open_o       = (st == StAlloc) && any_free_c;
  assign open_arena_o = free_c;

  // ---- the vertex fan-out --------------------------------------------------
  wire last_slot_c = (vs_q == two_slots_q);

  assign a_valid_o = (st == StFill) && v_valid_i;
  assign a_vx_o    = v_x_i;
  assign a_vy_o    = v_y_i;
  assign a_vz_o    = v_z_i;
  assign a_view_o  = slot_view_q[vs_q];

  // The rider is built by the LANE and consumed here. This block states
  // {arena, index}; it never states the packing.
  assign rider_arena_o = slot_arena_q[vs_q];
  assign rider_index_o = vi_q;
  assign a_payload_o   = rider_payload_i;

  wire a_take_c = a_valid_o && a_ready_i;

  // ---- holes, and the end of the fill ----------------------------------------
  // A hole is charged to the held job while its records can still be arriving
  // (StAlloc: a record can be refused before both arenas are open; StFill).
  // Anywhere else the held job's records are all accounted for, so the hole
  // stands for a record of the NEXT job: it is CARRIED in `early_q` and charged
  // when that job is accepted -- including a hole on the accept clock itself.
  wire job_accept_c  = job_take_c && job_legal_c;
  wire hole_in_job_c = hole_i && ((st == StAlloc) || (st == StFill));
  wire hole_early_c  = hole_i && !hole_in_job_c && !job_accept_c;
  wire vtx_done_c    = a_take_c && last_slot_c;
  wire [INDEX_W-1:0] vi_next_c    = vi_q + (vtx_done_c ? INDEX_W'(1) : INDEX_W'(0));
  wire [INDEX_W-1:0] holes_next_c = holes_q + (hole_in_job_c ? INDEX_W'(1) : INDEX_W'(0));
  // Every record of the batch is accounted for: arrived, or refused upstream.
  wire fill_end_c = ((vi_next_c + holes_next_c) == j_count);

  // A vertex is consumed only when its LAST slot has been accepted: the stream
  // is view-independent and must not be skinned twice, so one vertex is held
  // across the fan-out rather than buffered.
  assign v_ready_o = (st == StFill) && last_slot_c && a_ready_i;

  // ---- seal ----------------------------------------------------------------
  assign seal_o       = (st == StSeal);
  assign seal_arena_o = slot_arena_q[seal_slot_q];

  // ---- the handle ----------------------------------------------------------
  assign grp_valid_o  = (st == StHand);
  assign grp_arena_o  = slot_arena_q[hand_slot_q];
  assign grp_gen_o    = slot_gen_q[hand_slot_q];
  assign grp_count_o  = j_count;
  assign grp_view_o   = slot_view_q[hand_slot_q];
  assign grp_src_id_o = j_src;
  assign grp_poison_o = (holes_q != '0);

  wire grp_take_c = grp_valid_o && grp_ready_i;

  // ---- release -------------------------------------------------------------
  wire rel_held_c = rel_valid_i && held_q[rel_arena_i];

  integer ai;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st                   <= StIdle;
      j_count              <= '0;
      j_src                <= '0;
      two_slots_q          <= 1'b0;
      slot_view_q[0]       <= 1'b0;
      slot_view_q[1]       <= 1'b0;
      slot_arena_q[0]      <= '0;
      slot_arena_q[1]      <= '0;
      slot_gen_q[0]        <= '0;
      slot_gen_q[1]        <= '0;
      held_q               <= '0;
      alloc_slot_q         <= 1'b0;
      seal_slot_q          <= 1'b0;
      hand_slot_q          <= 1'b0;
      vs_q                 <= 1'b0;
      vi_q                 <= '0;
      holes_q              <= '0;
      early_q              <= '0;
      holes_o              <= '0;
      groups_poisoned_o    <= '0;
      holes_early_o        <= '0;
      groups_opened_o      <= '0;
      groups_sealed_o      <= '0;
      view_vertices_sent_o <= '0;
      landings_o           <= '0;
      jobs_refused_o       <= '0;
      alloc_stall_cycles_o <= '0;
      rel_unheld_o         <= '0;
      seal_early_o         <= 1'b0;
      for (ai = 0; ai < NA; ai = ai + 1) landed_q[ai] <= '0;
    end else begin
      // --- landings are counted wherever the machine is -------------------
      // The result port has no backpressure, so this is unconditional on
      // state; it is gated only by the port's own valid.
      if (fill_landed_i) begin
        landed_q[fill_arena_i] <= landed_q[fill_arena_i] + CNT_W'(1);
        if (landings_o != 32'hFFFF_FFFF) landings_o <= landings_o + 32'd1;
      end

      // --- release is independent of the job machine -----------------------
      if (rel_valid_i) begin
        if (rel_held_c) begin
          held_q[rel_arena_i] <= 1'b0;
        end else if (rel_unheld_o != 32'hFFFF_FFFF) begin
          rel_unheld_o <= rel_unheld_o + 32'd1;
        end
      end

      // --- holes: charged to the held job, or carried to the next -----------
      // (a hole on the accept clock itself is charged in StIdle below)
      if (hole_i && (holes_o != 32'hFFFF_FFFF)) holes_o <= holes_o + 32'd1;
      if (hole_in_job_c) holes_q <= holes_next_c;
      if (hole_early_c) begin
        if (early_q != '1) early_q <= early_q + INDEX_W'(1);
        if (holes_early_o != 32'hFFFF_FFFF) holes_early_o <= holes_early_o + 32'd1;
      end

      case (st)
        StIdle: begin
          if (job_take_c) begin
            if (!job_legal_c) begin
              if (jobs_refused_o != 32'hFFFF_FFFF)
                jobs_refused_o <= jobs_refused_o + 32'd1;
            end else begin
              j_count        <= job_count_i;
              j_src          <= job_src_id_i;
              // the carried holes, and this clock's, belong to THIS job
              holes_q        <= early_q + (hole_i ? INDEX_W'(1) : INDEX_W'(0));
              early_q        <= '0;
              two_slots_q    <= (&job_view_mask_i);
              slot_view_q[0] <= !job_view_mask_i[0];  // lowest set view
              slot_view_q[1] <= 1'b1;                 // only used when two
              alloc_slot_q   <= 1'b0;
              st             <= StAlloc;
            end
          end
        end

        StAlloc: begin
          if (any_free_c) begin
            slot_arena_q[alloc_slot_q] <= free_c;
            // SAMPLED COMBINATIONALLY IN THE OPEN CYCLE. The arena drives
            // `open_gen_o = gen_q[arena] + 1`, the generation this open is
            // about to install; reading it after the edge returns gen+1 again
            // and every later lookup misses with a perfect payload.
            slot_gen_q[alloc_slot_q]   <= open_gen_i;
            held_q[free_c]             <= 1'b1;
            landed_q[free_c]           <= '0;
            if (groups_opened_o != 32'hFFFF_FFFF)
              groups_opened_o <= groups_opened_o + 32'd1;

            if (alloc_slot_q == two_slots_q) begin
              vi_q <= '0;
              vs_q <= 1'b0;
              st   <= StFill;
            end else begin
              alloc_slot_q <= 1'b1;
            end
          end else if (alloc_stall_cycles_o != 32'hFFFF_FFFF) begin
            alloc_stall_cycles_o <= alloc_stall_cycles_o + 32'd1;
          end
        end

        StFill: begin
          if (a_take_c) begin
            if (view_vertices_sent_o != 32'hFFFF_FFFF)
              view_vertices_sent_o <= view_vertices_sent_o + 32'd1;
            if (last_slot_c) begin
              vs_q <= 1'b0;
              vi_q <= vi_next_c;
            end else begin
              vs_q <= 1'b1;
            end
          end
          // Evaluated every clock, not only on a vertex: the record that
          // completes the batch may be a HOLE, and a batch whose every record
          // was refused has no vertex at all to trigger the check.
          if (fill_end_c) st <= StSealWait;
        end

        StSealWait: begin
          // THE RULE: landings, never accepts. See the header.
          if (1'b1) begin
            seal_slot_q <= 1'b0;
            st          <= StSeal;
          end
        end

        StSeal: begin
          if (!drain_done_c) seal_early_o <= 1'b1;  // sticky fault
          if (groups_sealed_o != 32'hFFFF_FFFF)
            groups_sealed_o <= groups_sealed_o + 32'd1;
          if (seal_slot_q == two_slots_q) begin
            hand_slot_q <= 1'b0;
            st          <= StHand;
          end else begin
            seal_slot_q <= 1'b1;
          end
        end

        StHand: begin
          if (grp_take_c) begin
            if (grp_poison_o && (groups_poisoned_o != 32'hFFFF_FFFF))
              groups_poisoned_o <= groups_poisoned_o + 32'd1;
            if (hand_slot_q == two_slots_q) begin
              st <= StIdle;
            end else begin
              hand_slot_q <= 1'b1;
            end
          end
        end

        default: st <= StIdle;
      endcase
    end
  end

endmodule : zhao_geom_group_seq

`default_nettype wire
