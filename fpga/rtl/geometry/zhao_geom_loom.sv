// zhao_geom_loom.sv -- GEOM.LOOM, the Transform Loom.
//
// Contract: design/contracts/GEOM.LOOM.md (ledger `design/blocks.yml`, ZH-041,
// phase 9). Reference: `zref::TransformLoom`
// (reference/include/zref/zref_loom.hpp).
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK IS, AFTER THE RULING THAT SHRANK IT
// ---------------------------------------------------------------------------
// Owner ruling 2026-08-31 section 6.4, quoted in the contract:
//
//   "The ARM/compiler supplies a parent-before-child topologically sorted
//    stream. Loom only composes transforms. It does not perform: recursion;
//    cycle detection; matrix inversion; gameplay event generation; autonomous
//    gait logic; autonomous formation logic. Gait and formation values come
//    from Form/Field programs. Keep-world reparenting is computed on the ARM
//    between frames."
//
// The contract calls that "not a clarification, it is a large deletion, and it
// is what makes this block buildable". WHAT SURVIVES IS A STREAMING MATRIX
// COMPOSER, and this file is exactly that and nothing else. It sorts nothing,
// inverts nothing, detects no cycles (a sorted stream cannot contain one) and
// simulates no gait. `parent_index < node_index` is a PRECONDITION it REFUSES
// on, not a property it establishes.
//
// ---------------------------------------------------------------------------
// WHAT WAS SEARCHED AND REUSED, BY NAME
// ---------------------------------------------------------------------------
// CLAUDE.md: "before building a block, read the contract of every block that
// consumes or produces the same quantity", and the three corollaries about
// building a second implementation of something already here. Searched
// `fpga/rtl/**` for every arithmetic this block needs:
//
//   * THE AFFINE COMPOSE -- `zhao_geom_mat3x4_mul` (fpga/rtl/geometry). Exactly
//     this product: 3x4 x 3x4, ready/valid both sides, a_m_i/b_m_i -> out_m_o,
//     with an in_tag_i/out_tag_o pair. IT OWNS THE ROUNDING LAW (one rescale
//     per output element, `rescale_sat16`, round-half-up then saturate), which
//     is the contract's "ONE ROUNDING PER OUTPUT ELEMENT, never per partial
//     product". This block adds NO second rounding and NO second multiplier.
//     It is instantiated once, at `u_mul`, and every node goes through it --
//     including ROOT, see the identity note below.
//
//   * THE SINE AND COSINE -- `zhao_field_sin` (fpga/rtl/field), latency 2,
//     initiation interval 1, angle16 in turns, s15.16 out. Its quarter-wave
//     table `zhao_field_sin_rom` is GENERATED from `zref_tables.hpp`.
//     ONE instance, walked twice (SIN then COS on consecutive cycles), which is
//     the house law `design/blocks.yml` states for the Field engine: "ONE
//     zhao_field_sin shared by OP_SIN/OP_COS and ROT". Two instances would be
//     two ROMs for a block that needs one number every forty-odd cycles.
//
//   * NOT USED, AND WHY -- `zhao_field_normalize` / `zhao_field_isqrt` exist and
//     are the one implementation of `zref::normalize3_approx`. AIM would need
//     them if AIM derived its basis here. It does not; see the kind table.
//
// ---------------------------------------------------------------------------
// THE THROUGHPUT TARGET IS MISSED, ON PURPOSE, BY THIS MUCH
// ---------------------------------------------------------------------------
// The ledger says `target_throughput: 1 transform per clock`. THIS BLOCK DOES
// NOT MEET IT and the arithmetic is written here rather than left to be
// discovered, because a silently missed declared target is the failure this
// repo has a chapter about.
//
// Sharing one multiplier is an explicitly allowed optimisation (the contract's
// latency section calls the flat-versus-sequenced choice "a Class-B
// area/throughput trade to be measured"). Here is the trade, in clocks per
// node. `tests/geometry/geom_loom_directed.cpp` section 9 MEASURES it off the
// RTL and fails if it moves; these figures are the measurement, not a
// prediction:
//
//   accept beat                                          1
//   decode window D0..D3 (one shared sin walked twice,
//     parent-store read hidden underneath it)            4
//   compose issue + zhao_geom_mat3x4_mul walk            1 + WALK
//   store write / counters                               1
//   emit walk (order RAM -> store RAM -> output reg)      3
//                                                      -----
//   MUL_LANES = 1 (WALK = 37,  3 DSP)                   47
//   MUL_LANES = 3 (WALK = 12,  9 DSP)                   22
//
// Against the contract's own content tier -- 256 creatures at ~28 bones plus
// attachments, ~9,000 nodes -- and a 1,333,333-clock frame:
//
//   one-per-clock (the declared target)   9,000 clocks     0.7 % of a frame
//   MUL_LANES = 1                       423,000 clocks    31.7 %
//   MUL_LANES = 3                       198,000 clocks    14.9 %
//
// THE DEFAULT IS MUL_LANES = 1, and the reason is the DSP budget, not laziness.
// `reports/BUDGET_HEATMAP.md` has the top-level at 185 DSP against a 112-DSP
// device; a later composed measurement puts it at 151 against 112. Either way
// the device is OVER-SUBSCRIBED, and owner ruling 2026-09-18
// (`reports/OWNER-RULING-M10K-CEILINGS-20260918.md`, after its own point 1 was
// STRUCK the same day) prices a freed DSP at roughly 135 ALM, because the
// multiplier that cannot get a DSP is built in logic. Six DSPs is therefore
// about 810 ALM against a block whose whole ALM ceiling is 1,500.
//
// 31.7 % is a third of a frame for ONE STAGE OF A PIPELINE, not a third of the
// frame time -- the geometry stages run concurrently with raster and texture,
// and nothing here is the slowest of them. If a `FIELD.SEQ.FORMATION` trace
// ever shows this block gating (the contract names that trace as the case that
// actually pressures it, and says it does not exist yet), the lever is ONE
// PARAMETER and the arithmetic above is already written down. That is the
// point of it being a parameter: CLAUDE.md's rule 6, "never remove the owner's
// control in the name of fidelity".
//
// ---------------------------------------------------------------------------
// TWO DEVIATIONS FROM THE CONTRACT'S PROSE, ARGUED RATHER THAN SLIPPED IN
// ---------------------------------------------------------------------------
// 1. THE PARENT STORE IS ON-CHIP. The contract says "NO M10K for the parent
//    store ... it lives in the local SDRAM hot region, with the working window
//    on-chip", reasoning that 1,024 x 48 B is 48 KiB and "the renderer needs its
//    block RAM".
//
//    A WINDOW DOES NOT WORK HERE, and that is the structural objection rather
//    than a budget preference. A topologically sorted stream constrains a
//    parent to be EARLIER, not NEARBY: node 1,023 may legally name node 0 as
//    its parent. There is no locality to window over, so an on-chip window
//    would be a cache with a worst case of a full SDRAM round trip per node --
//    on a block that already spends 47 clocks per node, and against an SDRAM
//    that has no behavioural model in this tree to measure it with.
//
//    COST, STATED: the store is MAX_NODES x 412 bits = 421,888 bits at the
//    default, about 42 M10K of the device's 553 (7.6 %), plus 1 M10K for the
//    emission-order list and 1 for the shared sine table. Owner ruling
//    2026-09-18: "Using some more M10K is fine, we have enough, particularly if
//    it saves ALMs. They're our only weapon against our massive ALM debt." The
//    alternative -- 421,888 flops -- is the budget disaster `design/blocks.yml`
//    already names elsewhere ("7,776 flops instead of one M10K").
//
//    MAX_NODES IS A PARAMETER. At MAX_NODES = 256 the store is ~11 M10K.
//
// 2. THE STREAM IS COMPOSED IN FULL BEFORE ANY OF IT IS EMITTED. The contract
//    demands "A refusal drops the whole stream", and the formal property "no
//    partial stream -- a stream emits all its nodes or none". Composing and
//    emitting in the same pass cannot satisfy that: a fault at node 900 would
//    already have shipped 899 transforms.
//
//    So composition writes into the parent store (which this block needs
//    anyway) and EMISSION IS A SECOND WALK, started only by a clean `last`.
//    All-or-nothing is then STRUCTURAL rather than asserted -- there is no
//    state in which a prefix can leave. It costs the 3 clocks per node in the
//    table above and NO extra memory beyond the IDXW-bit order list.
//
// ---------------------------------------------------------------------------
// RESET ABANDONS THE STREAM, AND THE SCRUB IS WHAT MAKES THAT REAL
// ---------------------------------------------------------------------------
// The contract: "Reset abandons the stream in flight and clears the
// parent-transform store. A stream must be restarted from its root, never
// resumed mid-stream: a child composed against a stale parent is silently wrong
// geometry rather than an error, which is the worst failure mode this block can
// have."
//
// A 421,888-bit array cannot be reset by a reset tree, and must not be -- an
// asynchronous clear on it destroys M10K inference and turns the store into
// flops. So CLEARING IS A STATE, NOT A RESET. Reset lands the FSM in S_SCRUB,
// which walks every row writing its valid bit to zero, and `in_ready_o` is LOW
// for the whole walk. Nothing can be accepted until the store is provably
// empty.
//
// That makes the law structural in the direction that matters: a stream resumed
// after reset presents a child whose parent's valid bit is clear, and the block
// REFUSES (reason PARENT_UNSET) instead of composing against whatever survived.
// `tests/geometry/geom_loom_directed.cpp` section 8 resets mid-stream and
// resumes, and asserts the refusal rather than the absence of output.
//
// The same scrub runs after every stream and after every refusal, so a stream
// never inherits a row from the one before it.
//
// ---------------------------------------------------------------------------
// THE NODE KINDS, AND WHY FIVE OF THEM DECODE THE SAME WAY
// ---------------------------------------------------------------------------
// Every kind resolves to a LOCAL 3x4 `L`, and then the SAME compose:
//
//     world[node] = world[parent] * L
//
// with ROOT composing against the identity rather than branching around the
// multiplier. That is exact, not an approximation of a bypass: for the identity
// A, `A*L` forms (1<<16)*L[i][j] whose low sixteen bits are zero, so the
// round-half-up rescale returns L[i][j] unchanged, and the translation column
// adds (0 << 16). Section 1 of the directed test asserts ROOT out == param in,
// element for element. Routing ROOT through the multiplier keeps the latency
// FIXED (the contract's word) and keeps one datapath instead of two.
//
//   kind             L
//   ---------------- ---------------------------------------------------------
//   0 ROOT           param[0..11]; parent_index and the store are not read
//   1 RIGID          param[0..11]
//   2 SCALE          diag(param[0], param[1], param[2]), translation zero
//   3 ORBIT          rotation by angle16 about axis 0=X/1=Y/2=Z,
//                    translation param[0..2] (the pivot, in the parent frame)
//   4 AIM            param[0..11]
//   5 BILLBOARD      cam_basis_i[0..8] as the 3x3, translation param[0..2]
//   6 OSCILLATOR     identity 3x3, translation param[0..2]
//   7 SPLINE         identity 3x3, translation param[0..2]
//   8 GAIT_OFFSET    param[0..11]
//   9 FORMATION_OFF  param[0..11]
//
// FIVE KINDS TAKE THEIR TWELVE ELEMENTS STRAIGHT FROM `param`, AND THAT IS THE
// RULING'S ARITHMETIC RATHER THAN A SHORTCUT. "Gait and formation values come
// from Form/Field programs"; the contract says GAIT_OFFSET and FORMATION_OFFSET
// "carry VALUES, they do not compute them", and that the Field/Form samples
// "arrive as ports". AIM is the one worth spelling out, because its absence of
// arithmetic looks like an omission:
//
//   AIM's basis is a look-at, i.e. normalize(target - origin) and two cross
//   products. `zref::normalize3_approx` is ALREADY IMPLEMENTED in this tree, as
//   `zhao_field_normalize`, and a second copy here is precisely the duplication
//   CLAUDE.md's "read the SIBLING contract" chapter was written after (two
//   projectors, `GEOM.PROJECT` and `TERRAIN.PROJECT`, declaring one reference
//   function). The Field program that already evaluates the creature's aim
//   target evaluates its basis, and hands it over as twelve numbers.
//
// A kind that decodes identically to another is STILL A DISTINCT KIND here: it
// has its own `node_kind_histogram` bucket, which is what the integration
// capture cases read, and it is the field a later revision widens if one of
// them grows arithmetic. Collapsing them in the encoding would delete that.
//
// BILLBOARD IS THE ONE PLACE THAT LOSES SOMETHING, AND IT IS RECORDED HERE
// RATHER THAN DISCOVERED LATER. `cam_basis_i` is the camera's 3x3 for the
// frame, a port like `GEOM.MESHFETCH`'s cull planes. Composed under a parent
// with a ROTATION, the result carries that rotation and is therefore NOT
// camera-facing. Making it camera-facing under an arbitrary parent needs the
// parent's INVERSE rotation, and matrix inversion is excluded by the ruling by
// name. So the compiler hangs billboards off a translation-only anchor, exactly
// as it supplies the topological order -- the hard half is the compiler's, which
// is the ruling's whole shape. A billboard under a rotating parent is a
// COMPILER bug this block cannot see, and nothing here pretends otherwise.
//
// ORBIT'S AXIS IS PART OF ITS OPCODE. `in_axis_i == 3` is refused as BAD_KIND,
// not silently folded onto Z. A rotation about an axis the block does not have
// is an unknown operation, which is what that reason means.
//
// ---------------------------------------------------------------------------
// THE SIX REFUSAL REASONS, AND WHICH ONE CANNOT FIRE AT THE DEFAULT
// ---------------------------------------------------------------------------
//   0 NOT_SORTED     `parent_index >= node_index` on a non-ROOT node, OR a
//                    `node_index` that did not strictly increase. Both say the
//                    same thing -- the stream is not topologically sorted -- and
//                    the second is what makes a duplicate index impossible, so
//                    no node can be written twice and the emission order is the
//                    stream order by construction.
//   1 PARENT_UNSET   the parent's row has no valid bit. This is the reason a
//                    resumed stream hits, and the reason a stream referencing a
//                    node that was never emitted hits. Checked after the store
//                    read, not at accept.
//   2 OVERFLOW       more than MAX_NODES nodes, or a `node_index` at or beyond
//                    MAX_NODES.
//   3 BAD_KIND       a kind above FORMATION_OFFSET, or ORBIT with axis 3.
//   4 SCALE_SHEAR    a non-uniform SCALE on a body-patch-bound node
//                    (spec/creature_rules.md section 6.5, "rigid + uniform
//                    scale, no shear, validator-enforced" -- enforced here
//                    rather than trusted, which is what the contract asks for).
//   5 FRAMING        a `first` beat arrived inside a stream (the previous stream
//                    ended without `last`), or a beat without `first` arrived
//                    while idle (a stream resumed mid-way).
//
// Faults are prioritised FRAMING > OVERFLOW > NOT_SORTED > BAD_KIND >
// SCALE_SHEAR, so a beat carrying two faults reports the outer one.
//
// REASON 2 CANNOT FIRE AT MAX_NODES = 1024 WITH IDXW = 10, and saying so is the
// point. `node_index` is ten bits, so it cannot reach 1024; and with strictly
// increasing indices drawn from 1,024 values a stream cannot hold 1,025 nodes.
// That is a guard unreachable by legal stimulus at the shipping parameters --
// CLAUDE.md's case. It does NOT need a committed mutant, because it is
// reachable by a LEGAL PARAMETERISATION: `MAX_NODES = 16, IDXW = 10` leaves
// 1,008 index values above the bound and admits a seventeenth node, and
// `test_geom_loom_small` is that build. Every other counter in this block is
// fired by ordinary stimulus in the directed test, by name, in section 10.
//
// ---------------------------------------------------------------------------
// ACCUMULATION OVER A DEEP CHAIN IS MEASURED, NOT ASSUMED
// ---------------------------------------------------------------------------
// The contract: "a 1,024-deep chain is legal and nobody has yet asked what the
// drift looks like. That measurement is a directed test below, not an
// assumption here." Section 7 of the directed test composes a maximal chain and
// reports the worst element error of the fx16 chain against exact rational
// arithmetic. It PRINTS A NUMBER for a question nobody has asked; it does not
// assert a bound invented by its author.
//
module zhao_geom_loom #(
    // Parent-store depth AND the node-count bound. A stream that would exceed
    // it is refused, never truncated.
    parameter int MAX_NODES = 1024,
    // Width of `node_index` / `parent_index` on the ports. Deliberately NOT
    // derived from MAX_NODES: holding them apart is what makes the OVERFLOW
    // guard reachable by a legal parameterisation (see the reason table above).
    parameter int IDXW      = 10,
    // Passed straight through to `zhao_geom_mat3x4_mul`. 1 = one shared 32x32
    // multiplier, 3 DSP, 37-cycle walk (the default, and the DSP budget is
    // why). 3 = one output element per cycle, 9 DSP, 12-cycle walk.
    parameter int MUL_LANES = 1
) (
    input  logic clk,
    input  logic rst_n,

    // ---- node stream in, one node per beat, parents first ------------------
    input  logic               in_valid_i,
    output logic               in_ready_o,
    input  logic [IDXW-1:0]    in_node_index_i,
    input  logic [IDXW-1:0]    in_parent_index_i,   // ignored when kind == ROOT
    input  logic [3:0]         in_kind_i,
    input  logic signed [31:0] in_param_i [12],
    input  logic [15:0]        in_angle_i,          // angle16 turns, ORBIT
    input  logic [1:0]         in_axis_i,           // 0=X 1=Y 2=Z, ORBIT
    input  logic               in_bodypatch_i,      // node drives a body_patch page
    input  logic [15:0]        in_src_id_i,
    input  logic               in_first_i,
    input  logic               in_last_i,

    // ---- per-frame camera basis, BILLBOARD ---------------------------------
    // Row-major 3x3. A port, like GEOM.MESHFETCH's cull planes -- this block
    // "Reads no other memory. Writes none."
    input  logic signed [31:0] cam_basis_i [9],

    // ---- transform stream out ----------------------------------------------
    output logic               out_valid_o,
    input  logic               out_ready_i,
    output logic [IDXW-1:0]    out_node_index_o,
    output logic signed [31:0] out_m_o [12],
    output logic [15:0]        out_src_id_o,
    output logic               out_last_o,

    // ---- refusal, one pulse per dropped stream -----------------------------
    // Each names the offending node and the src_id, as the contract requires.
    output logic               refuse_valid_o,
    output logic [2:0]         refuse_reason_o,
    output logic [IDXW-1:0]    refuse_node_index_o,
    output logic [15:0]        refuse_src_id_o,

    // ---- counters ----------------------------------------------------------
    // `vertices_transformed` in the ledger counts NODES here; the contract
    // records the discrepancy and asks for the ledger name to be corrected to
    // `nodes_transformed` when the block is built. It is that name here.
    output logic [31:0] nodes_transformed_o,
    output logic [31:0] streams_composed_o,
    output logic [31:0] streams_refused_o [6],
    output logic [15:0] nodes_per_stream_max_o,
    output logic [15:0] chain_depth_max_o,
    output logic [31:0] node_kind_hist_o [10],
    output logic [31:0] consumer_stall_cycles_o
);

  // Quartus 17 rejects a bare module-scope `if`; an elaboration check has to
  // sit inside `initial begin ... end`. And `--lint-only` does not run this
  // block at all, so a clean lint says nothing about it (CLAUDE.md).
  initial begin
    if (MAX_NODES < 2) begin
      $fatal(1, "zhao_geom_loom: MAX_NODES must be >= 2, got %0d", MAX_NODES);
    end
    if (IDXW < 2 || IDXW > 16) begin
      $fatal(1, "zhao_geom_loom: IDXW must be 2..16, got %0d", IDXW);
    end
    if (MAX_NODES > (1 << IDXW)) begin
      $fatal(1, "zhao_geom_loom: MAX_NODES %0d exceeds 2**IDXW %0d", MAX_NODES, (1 << IDXW));
    end
    if (MUL_LANES != 1 && MUL_LANES != 3) begin
      $fatal(1, "zhao_geom_loom: MUL_LANES must be 1 or 3, got %0d", MUL_LANES);
    end
  end

  // ---- node kinds ---------------------------------------------------------
  localparam logic [3:0] K_ROOT      = 4'd0;
  localparam logic [3:0] K_RIGID     = 4'd1;
  localparam logic [3:0] K_SCALE     = 4'd2;
  localparam logic [3:0] K_ORBIT     = 4'd3;
  localparam logic [3:0] K_AIM       = 4'd4;
  localparam logic [3:0] K_BILLBOARD = 4'd5;
  localparam logic [3:0] K_OSC       = 4'd6;
  localparam logic [3:0] K_SPLINE    = 4'd7;
  localparam logic [3:0] K_GAIT      = 4'd8;
  localparam logic [3:0] K_FORM      = 4'd9;

  // ---- refusal reasons ----------------------------------------------------
  localparam logic [2:0] R_NOT_SORTED   = 3'd0;
  localparam logic [2:0] R_PARENT_UNSET = 3'd1;
  localparam logic [2:0] R_OVERFLOW     = 3'd2;
  localparam logic [2:0] R_BAD_KIND     = 3'd3;
  localparam logic [2:0] R_SCALE_SHEAR  = 3'd4;
  localparam logic [2:0] R_FRAMING      = 3'd5;

  // ---- states -------------------------------------------------------------
  localparam logic [3:0] S_SCRUB = 4'd0;   // walking the store, clearing valid
  localparam logic [3:0] S_IDLE  = 4'd1;   // store empty, awaiting a `first`
  localparam logic [3:0] S_RUN   = 4'd2;   // mid-stream, awaiting the next node
  localparam logic [3:0] S_D0    = 4'd3;   // sin issued; parent read addressed
  localparam logic [3:0] S_D1    = 4'd4;   // cos issued; parent row landed
  localparam logic [3:0] S_D2    = 4'd5;   // sin result on the bus
  localparam logic [3:0] S_D3    = 4'd6;   // cos result on the bus
  localparam logic [3:0] S_MUL   = 4'd7;   // composing through u_mul
  localparam logic [3:0] S_WR    = 4'd8;   // world row written, counters moved
  localparam logic [3:0] S_EA    = 4'd9;   // emit: order RAM addressed
  localparam logic [3:0] S_EB    = 4'd10;  // emit: order row -> store address
  localparam logic [3:0] S_EC    = 4'd11;  // emit: store row -> output register
  localparam logic [3:0] S_EDONE = 4'd12;  // emit: awaiting the final handshake
  localparam logic [3:0] S_REF   = 4'd13;  // refusal pulse
  localparam logic [3:0] S_DRAIN = 4'd14;  // swallowing the rest of a dead stream

  // Store row: 12 x s32 world transform, src_id, depth, valid.
  localparam int SW  = 412;
  localparam int SSL = 384;   // src_id [399:384]
  localparam int SDL = 400;   // depth  [410:400]
  localparam int SV  = 411;   // valid

  // The scrub walk's final address. A localparam of the port width rather than
  // an inline (MAX_NODES - 1), which is a 32-bit constant and widens the
  // comparison.
  localparam logic [IDXW-1:0] SCRUB_LAST = IDXW'(MAX_NODES - 1);

  logic [3:0] state_q;

  // ---- the parent-transform store -----------------------------------------
  // ONE write port, ONE read port, no reset on the array: an asynchronous clear
  // over 421,888 bits destroys M10K inference and rebuilds the store in flops,
  // which `design/blocks.yml` names as a budget disaster in its own words.
  // S_SCRUB is the clear (see the header).
  logic [SW-1:0]   store_q [MAX_NODES];
  logic [SW-1:0]   store_rd_q;
  logic            store_we;
  logic [IDXW-1:0] store_wa;
  logic [SW-1:0]   store_wd;
  logic [IDXW-1:0] store_ra;

  // ---- the emission-order list --------------------------------------------
  // order[k] = the node_index of the k-th node accepted. Emission walks it, so
  // the output order is the ARRIVAL order exactly, with no scan over unused
  // index values and no dependence on the indices being dense.
  logic [IDXW-1:0] order_q [MAX_NODES];
  logic [IDXW-1:0] order_rd_q;
  logic            order_we;
  logic [IDXW-1:0] order_wa;
  logic [IDXW-1:0] order_wd;
  logic [IDXW-1:0] order_ra;

  always_ff @(posedge clk) begin
    if (store_we) store_q[store_wa] <= store_wd;
    store_rd_q <= store_q[store_ra];
    if (order_we) order_q[order_wa] <= order_wd;
    order_rd_q <= order_q[order_ra];
  end

  // ---- latched beat -------------------------------------------------------
  logic [IDXW-1:0]    node_q;
  logic [IDXW-1:0]    parent_q;
  logic [3:0]         kind_q;
  logic signed [31:0] param_q [12];
  logic [15:0]        angle_q;
  logic [1:0]         axis_q;
  logic [15:0]        src_q;
  logic               last_q;

  // ---- stream bookkeeping -------------------------------------------------
  logic [15:0]     count_q;      // nodes accepted in this stream
  logic [IDXW-1:0] prev_idx_q;   // last node_index accepted, for the sort check
  logic [IDXW-1:0] scrub_q;
  logic [15:0]     emit_k_q;
  logic [IDXW-1:0] emit_addr_q;  // the row emission is currently holding
  logic            ref_last_q;   // the refused beat carried `last`

  // ---- composition scratch ------------------------------------------------
  logic signed [31:0] pw_q [12];     // parent world row, held across the walk
  logic [10:0]        pdepth_q;
  logic signed [31:0] sin_q;
  logic signed [31:0] cos_q;
  logic               mul_issued_q;

  // =========================================================================
  // THE SHARED SINE, WALKED TWICE
  // =========================================================================
  // `is_cos` is a pure state decode: SIN is presented in S_D0, COS in S_D1, and
  // `zhao_field_sin`'s latency-2 puts them on the bus in S_D2 and S_D3. The
  // walk runs for EVERY node, not just ORBIT, which is what keeps the latency
  // fixed -- the contract's word -- for the price of four clocks in forty-seven.
  logic               sin_is_cos;
  logic signed [31:0] sin_result;
  assign sin_is_cos = (state_q == S_D1);

  zhao_field_sin u_sin (
      .clk     (clk),
      .angle_i (angle_q),
      .is_cos_i(sin_is_cos),
      .result_o(sin_result)
  );

  // =========================================================================
  // THE LOCAL MATRIX
  // =========================================================================
  // Row-major, m[row*4 + col]. Combinational from the latched beat, the two
  // trig results and the per-frame camera basis. This is the ONLY place a kind
  // is distinguished; after it, every node takes the same compose.
  localparam logic signed [31:0] FX_ONE = 32'sh0001_0000;

  logic signed [31:0] local_m [12];

  always_comb begin
    for (int e = 0; e < 12; e++) local_m[e] = 32'sd0;

    case (kind_q)
      K_SCALE: begin
        local_m[0]  = param_q[0];
        local_m[5]  = param_q[1];
        local_m[10] = param_q[2];
      end

      K_ORBIT: begin
        // Rotation by angle16 about a principal axis. The entries ARE sin and
        // cos -- there is no multiply in an axis-aligned rotation, which is why
        // ORBIT costs a table read and not a DSP.
        case (axis_q)
          2'd0: begin  // X
            local_m[0]  = FX_ONE;
            local_m[5]  = cos_q;   local_m[6]  = -sin_q;
            local_m[9]  = sin_q;   local_m[10] = cos_q;
          end
          2'd1: begin  // Y
            local_m[0]  = cos_q;   local_m[2]  = sin_q;
            local_m[5]  = FX_ONE;
            local_m[8]  = -sin_q;  local_m[10] = cos_q;
          end
          default: begin  // Z; axis 3 never reaches here, it is BAD_KIND
            local_m[0]  = cos_q;   local_m[1]  = -sin_q;
            local_m[4]  = sin_q;   local_m[5]  = cos_q;
            local_m[10] = FX_ONE;
          end
        endcase
        local_m[3]  = param_q[0];
        local_m[7]  = param_q[1];
        local_m[11] = param_q[2];
      end

      K_BILLBOARD: begin
        local_m[0]  = cam_basis_i[0];  local_m[1]  = cam_basis_i[1];
        local_m[2]  = cam_basis_i[2];  local_m[4]  = cam_basis_i[3];
        local_m[5]  = cam_basis_i[4];  local_m[6]  = cam_basis_i[5];
        local_m[8]  = cam_basis_i[6];  local_m[9]  = cam_basis_i[7];
        local_m[10] = cam_basis_i[8];
        local_m[3]  = param_q[0];
        local_m[7]  = param_q[1];
        local_m[11] = param_q[2];
      end

      K_OSC, K_SPLINE: begin
        local_m[0]  = FX_ONE;
        local_m[5]  = FX_ONE;
        local_m[10] = FX_ONE;
        local_m[3]  = param_q[0];
        local_m[7]  = param_q[1];
        local_m[11] = param_q[2];
      end

      // ROOT, RIGID, AIM, GAIT_OFFSET, FORMATION_OFFSET -- twelve elements
      // handed over. See the header for why AIM is one of them. They are listed
      // by name rather than left to default so that adding a kind cannot
      // silently inherit this arm.
      K_ROOT, K_RIGID, K_AIM, K_GAIT, K_FORM: begin
        for (int e = 0; e < 12; e++) local_m[e] = param_q[e];
      end

      // Kinds 10..15 are refused as BAD_KIND at accept and are never latched,
      // so this arm is unreachable. It exists because an incomplete case is a
      // latch in some tools and an X in others, and neither is a matrix.
      default: begin
        for (int e = 0; e < 12; e++) local_m[e] = param_q[e];
      end
    endcase
  end

  // =========================================================================
  // THE COMPOSE -- ONE zhao_geom_mat3x4_mul, AND IT OWNS THE ROUNDING
  // =========================================================================
  logic               mul_in_valid;
  logic               mul_in_ready;
  logic signed [31:0] mul_a [12];
  logic signed [31:0] mul_b [12];
  logic               mul_out_valid;
  logic               mul_out_ready;
  logic signed [31:0] mul_out [12];
  logic        [ 7:0] mul_out_tag;
  logic [31:0]        mul_products_done;

  // ROOT composes against the identity rather than branching around the
  // multiplier. Exact -- see the header.
  always_comb begin
    for (int e = 0; e < 12; e++) mul_a[e] = 32'sd0;
    if (kind_q == K_ROOT) begin
      mul_a[0]  = FX_ONE;
      mul_a[5]  = FX_ONE;
      mul_a[10] = FX_ONE;
    end else begin
      for (int e = 0; e < 12; e++) mul_a[e] = pw_q[e];
    end
    for (int e = 0; e < 12; e++) mul_b[e] = local_m[e];
  end

  assign mul_in_valid  = (state_q == S_MUL) && !mul_issued_q;
  assign mul_out_ready = (state_q == S_MUL);

  zhao_geom_mat3x4_mul #(
      .MUL_LANES(MUL_LANES)
  ) u_mul (
      .clk            (clk),
      .rst_n          (rst_n),
      .in_valid_i     (mul_in_valid),
      .in_ready_o     (mul_in_ready),
      .a_m_i          (mul_a),
      .b_m_i          (mul_b),
      .in_tag_i       (node_q[7:0]),
      .out_valid_o    (mul_out_valid),
      .out_ready_i    (mul_out_ready),
      .out_m_o        (mul_out),
      .out_tag_o      (mul_out_tag),
      .products_done_o(mul_products_done)
  );

  // The row this block is about to commit, assembled combinationally so the
  // write port takes ONE assignment and a reader can see the layout in one
  // place.
  logic [SW-1:0] node_word;
  always_comb begin
    node_word = '0;
    node_word[SV]        = 1'b1;
    node_word[SDL +: 11] = pdepth_q + 11'd1;
    node_word[SSL +: 16] = src_q;
    for (int e = 0; e < 12; e++) node_word[32*e +: 32] = mul_out[e];
  end

  // =========================================================================
  // FAULT DETECTION ON THE OFFERED BEAT
  // =========================================================================
  // Combinational on the input word, so a faulty beat is refused on the same
  // edge it would otherwise have been accepted -- nothing faulty is ever
  // latched as work. PARENT_UNSET is the exception and is checked in S_D1,
  // after the store read, because it is the one fault that is not a property of
  // the beat.
  logic        f_framing, f_overflow, f_sorted, f_kind, f_scale, f_any;
  logic [2:0]  f_reason;
  logic        accepting;
  logic [16:0] node_ext;

  assign accepting = (state_q == S_IDLE) || (state_q == S_RUN);
  assign node_ext  = {{(17 - IDXW){1'b0}}, in_node_index_i};

  always_comb begin
    f_framing  = (in_first_i && (state_q == S_RUN)) || (!in_first_i && (state_q == S_IDLE));

    f_overflow = (node_ext >= 17'(MAX_NODES))
              || (!in_first_i && (count_q >= 16'(MAX_NODES)));

    f_sorted   = (!in_first_i && (in_node_index_i <= prev_idx_q))
              || ((in_kind_i != K_ROOT) && (in_parent_index_i >= in_node_index_i));

    f_kind     = (in_kind_i > K_FORM) || ((in_kind_i == K_ORBIT) && (in_axis_i == 2'd3));

    f_scale    = (in_kind_i == K_SCALE) && in_bodypatch_i
              && !((in_param_i[0] == in_param_i[1]) && (in_param_i[1] == in_param_i[2]));

    f_any      = f_framing || f_overflow || f_sorted || f_kind || f_scale;

    // Outermost fault first: a beat that is framed wrong is not a beat whose
    // kind field means anything.
    if      (f_framing)  f_reason = R_FRAMING;
    else if (f_overflow) f_reason = R_OVERFLOW;
    else if (f_sorted)   f_reason = R_NOT_SORTED;
    else if (f_kind)     f_reason = R_BAD_KIND;
    else                 f_reason = R_SCALE_SHEAR;
  end

  // =========================================================================
  // HANDSHAKES AND MEMORY ADDRESSING
  // =========================================================================
  // `in_ready_o` is a pure state decode and never looks at `in_valid_i`;
  // `out_valid_o` is a register and never looks at `out_ready_i`. House
  // hygiene, and the contract names both.
  assign in_ready_o = accepting || (state_q == S_DRAIN);

  // The store read address. During a compose it holds the parent row for the
  // whole walk; during emission it holds the row the order list named, INCLUDING
  // while the consumer stalls -- letting it fall back to `parent_q` in S_EC
  // would reload `store_rd_q` with an unrelated row on the very next clock and
  // emit it.
  always_comb begin
    store_ra = parent_q;
    if (state_q == S_EB) begin
      store_ra = order_rd_q;
    end else if ((state_q == S_EC) || (state_q == S_EDONE)) begin
      store_ra = emit_addr_q;
    end
  end

  assign order_ra = emit_k_q[IDXW-1:0];

  // =========================================================================
  // THE SEQUENCER
  // =========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // Reset ABANDONS the stream. The store is not cleared here -- it cannot
      // be -- so the FSM lands in S_SCRUB and nothing is accepted until the
      // walk has cleared every valid bit.
      state_q      <= S_SCRUB;
      scrub_q      <= '0;
      node_q       <= '0;
      parent_q     <= '0;
      kind_q       <= '0;
      angle_q      <= '0;
      axis_q       <= '0;
      src_q        <= '0;
      last_q       <= 1'b0;
      count_q      <= '0;
      prev_idx_q   <= '0;
      emit_k_q     <= '0;
      emit_addr_q  <= '0;
      ref_last_q   <= 1'b0;
      pdepth_q     <= '0;
      sin_q        <= '0;
      cos_q        <= '0;
      mul_issued_q <= 1'b0;
      store_we     <= 1'b0;
      store_wa     <= '0;
      store_wd     <= '0;
      order_we     <= 1'b0;
      order_wa     <= '0;
      order_wd     <= '0;
      out_valid_o      <= 1'b0;
      out_node_index_o <= '0;
      out_src_id_o     <= '0;
      out_last_o       <= 1'b0;
      refuse_valid_o      <= 1'b0;
      refuse_reason_o     <= '0;
      refuse_node_index_o <= '0;
      refuse_src_id_o     <= '0;
      nodes_transformed_o     <= '0;
      streams_composed_o      <= '0;
      nodes_per_stream_max_o  <= '0;
      chain_depth_max_o       <= '0;
      consumer_stall_cycles_o <= '0;
      for (int i = 0; i < 12; i++) begin
        param_q[i] <= '0;
        pw_q[i]    <= '0;
        out_m_o[i] <= '0;
      end
      for (int i = 0; i < 6; i++)  streams_refused_o[i] <= '0;
      for (int i = 0; i < 10; i++) node_kind_hist_o[i]  <= '0;
    end else begin
      store_we       <= 1'b0;
      order_we       <= 1'b0;
      refuse_valid_o <= 1'b0;

      if (out_valid_o && out_ready_i) out_valid_o <= 1'b0;

      case (state_q)

        // ---- clearing the store ------------------------------------------
        S_SCRUB: begin
          store_we <= 1'b1;
          store_wa <= scrub_q;
          store_wd <= '0;                       // valid bit low; the rest is dead
          if (scrub_q == SCRUB_LAST) begin
            scrub_q <= '0;
            state_q <= S_IDLE;
          end else begin
            scrub_q <= scrub_q + 1'b1;
          end
        end

        // ---- awaiting a beat ---------------------------------------------
        S_IDLE, S_RUN: begin
          if (in_valid_i) begin
            if (f_any) begin
              refuse_reason_o     <= f_reason;
              refuse_node_index_o <= in_node_index_i;
              refuse_src_id_o     <= in_src_id_i;
              ref_last_q          <= in_last_i;
              state_q             <= S_REF;
            end else begin
              node_q   <= in_node_index_i;
              parent_q <= in_parent_index_i;
              kind_q   <= in_kind_i;
              angle_q  <= in_angle_i;
              axis_q   <= in_axis_i;
              src_q    <= in_src_id_i;
              last_q   <= in_last_i;
              for (int i = 0; i < 12; i++) param_q[i] <= in_param_i[i];
              count_q    <= in_first_i ? 16'd1 : (count_q + 16'd1);
              prev_idx_q <= in_node_index_i;
              state_q    <= S_D0;
            end
          end
        end

        // ---- decode window ------------------------------------------------
        // Four clocks: the shared sine is walked SIN then COS, and the parent
        // row lands underneath it at no extra cost.
        S_D0: state_q <= S_D1;

        S_D1: begin
          // The parent row addressed in S_D0 is on `store_rd_q` now. A ROOT
          // reads no parent, which is the contract's "the parent store is never
          // read before written" made structural rather than asserted.
          if ((kind_q != K_ROOT) && !store_rd_q[SV]) begin
            refuse_reason_o     <= R_PARENT_UNSET;
            refuse_node_index_o <= node_q;
            refuse_src_id_o     <= src_q;
            ref_last_q          <= last_q;
            state_q             <= S_REF;
          end else begin
            for (int i = 0; i < 12; i++) pw_q[i] <= $signed(store_rd_q[32*i +: 32]);
            pdepth_q <= (kind_q == K_ROOT) ? 11'd0 : store_rd_q[SDL +: 11];
            state_q  <= S_D2;
          end
        end

        S_D2: begin
          sin_q   <= sin_result;
          state_q <= S_D3;
        end

        S_D3: begin
          cos_q        <= sin_result;
          mul_issued_q <= 1'b0;
          state_q      <= S_MUL;
        end

        // ---- compose -------------------------------------------------------
        S_MUL: begin
          if (mul_in_valid && mul_in_ready) mul_issued_q <= 1'b1;
          if (mul_out_valid) begin
            store_we <= 1'b1;
            store_wa <= node_q;
            store_wd <= node_word;
            order_we <= 1'b1;
            order_wa <= count_q[IDXW-1:0] - 1'b1;
            order_wd <= node_q;
            state_q  <= S_WR;
          end
        end

        // ---- commit --------------------------------------------------------
        S_WR: begin
          nodes_transformed_o <= nodes_transformed_o + 32'd1;
          // kind_q > K_FORM is refused at accept and is never latched, so this
          // index is always in range; the guard keeps it in range STRUCTURALLY
          // rather than by that argument.
          if (kind_q <= K_FORM) begin
            node_kind_hist_o[kind_q] <= node_kind_hist_o[kind_q] + 32'd1;
          end
          if ({5'd0, (pdepth_q + 11'd1)} > chain_depth_max_o) begin
            chain_depth_max_o <= {5'd0, (pdepth_q + 11'd1)};
          end
          if (last_q) begin
            if (count_q > nodes_per_stream_max_o) nodes_per_stream_max_o <= count_q;
            emit_k_q <= '0;
            state_q  <= S_EA;
          end else begin
            state_q <= S_RUN;
          end
        end

        // ---- emission: the second walk, all-or-nothing by construction ----
        S_EA: state_q <= S_EB;   // order RAM addressed by `order_ra`

        S_EB: begin
          emit_addr_q <= order_rd_q;   // hold it for S_EC, stalls included
          state_q     <= S_EC;
        end

        S_EC: begin
          if (!out_valid_o || out_ready_i) begin
            out_valid_o      <= 1'b1;
            out_node_index_o <= emit_addr_q;
            out_src_id_o     <= store_rd_q[SSL +: 16];
            out_last_o       <= (emit_k_q == (count_q - 16'd1));
            for (int i = 0; i < 12; i++) out_m_o[i] <= $signed(store_rd_q[32*i +: 32]);
            if (emit_k_q == (count_q - 16'd1)) begin
              state_q <= S_EDONE;
            end else begin
              emit_k_q <= emit_k_q + 16'd1;
              state_q  <= S_EA;
            end
          end else begin
            consumer_stall_cycles_o <= consumer_stall_cycles_o + 32'd1;
          end
        end

        S_EDONE: begin
          if (!out_valid_o || out_ready_i) begin
            streams_composed_o <= streams_composed_o + 32'd1;
            scrub_q            <= '0;
            count_q            <= '0;
            prev_idx_q         <= '0;
            state_q            <= S_SCRUB;
          end else begin
            consumer_stall_cycles_o <= consumer_stall_cycles_o + 32'd1;
          end
        end

        // ---- refusal -------------------------------------------------------
        S_REF: begin
          refuse_valid_o <= 1'b1;
          if (refuse_reason_o < 3'd6) begin
            streams_refused_o[refuse_reason_o] <= streams_refused_o[refuse_reason_o] + 32'd1;
          end
          // A refused beat that already carried `last` ENDED the stream; there
          // is nothing left to drain, and waiting for a `last` that has gone by
          // would swallow the NEXT stream's beats instead.
          if (ref_last_q) begin
            scrub_q    <= '0;
            count_q    <= '0;
            prev_idx_q <= '0;
            state_q    <= S_SCRUB;
          end else begin
            state_q <= S_DRAIN;
          end
        end

        // ---- drain: the WHOLE stream is dropped, never a prefix -------------
        // Nothing composed so far is emitted, because emission has not started
        // and cannot start except from S_WR with `last`. Beats are swallowed
        // until the stream's own `last` goes by, then the store is scrubbed so
        // the next stream inherits nothing.
        S_DRAIN: begin
          if (in_valid_i && in_last_i) begin
            scrub_q    <= '0;
            count_q    <= '0;
            prev_idx_q <= '0;
            state_q    <= S_SCRUB;
          end
        end

        default: state_q <= S_SCRUB;
      endcase
    end
  end

  // =========================================================================
  // SIMULATION-ONLY CROSS-CHECKS
  // =========================================================================
  // `// synthesis translate_off` does NOT make Verilator skip a block -- proven
  // in this repo by planting a syntax error inside one -- so these are live in
  // simulation, which is the point.
  //
  // Both are genuine independent witnesses rather than decoration. CLAUDE.md's
  // "a detector wired to two operands that move together cannot fire" asks what
  // clocks the two sides of a comparison: here the left side is `u_mul`'s own
  // counter and its own held tag, loaded by ITS accept enable inside a
  // different module, and the right side is this block's commit counter and
  // `node_q`, loaded by the input handshake. No single enable drives both, so
  // the comparison can see a TIMING fault -- a result paired with the wrong
  // node -- and not only a value fault.
  // synthesis translate_off
  always_ff @(posedge clk) begin
    if (state_q == S_WR) begin
      if (mul_products_done != (nodes_transformed_o + 32'd1)) begin
        $fatal(1, "zhao_geom_loom: committed %0d nodes but u_mul finished %0d products",
               nodes_transformed_o + 32'd1, mul_products_done);
      end
      if (mul_out_tag != node_q[7:0]) begin
        $fatal(1, "zhao_geom_loom: composed row tagged %0d, node_q is %0d",
               mul_out_tag, node_q[7:0]);
      end
    end
  end
  // synthesis translate_on

endmodule : zhao_geom_loom
