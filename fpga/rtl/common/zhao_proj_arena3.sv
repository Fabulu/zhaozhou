// zhao_proj_arena3.sv — the sealed projected-vertex arena with THREE read
// replicas: project each unique vertex ONCE, replay it by reference three
// corners at a time.
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS: THE SHARED PROJECTOR IS ONLY FAST ENOUGH IF VERTICES REUSE
// ---------------------------------------------------------------------------
// At fine terrain resolution one 33x33 patch is 1,089 unique lattice vertices
// feeding 32x32x2 = 2,048 triangles = 6,144 corner references — 5.64x
// redundant projection work if every corner is projected on demand. Checked
// against the rescue roadmap's own stress rows (2026-09-09):
//
//   naive two-view terrain:  2 x 256 x 6,144        = 3,145,728 projections
//   frame window (20% res.): 100 MHz / 60 Hz x 0.8  = 1,333,333 clocks
//
// 3,145,728 > 1,333,333: WITHOUT reuse, even a perfect one-per-clock shared
// core fails by 2.4x. With per-subpatch reuse (16 9x9 subpatches per patch,
// 81 unique vertices each):
//
//   two-view subpatch reuse: 2 x (256 x 16 x 81 + 120,000 geom) = 903,552
//
// 903,552 < 1,333,333 — the shared service carries both clients with ~32%
// headroom. So this arena is not an optimisation ON TOP of the shared
// projector (zhao_project_service.sv); it is the thing that makes a single
// projector POSSIBLE at the fine profile. The 19% edge duplication between
// adjacent subpatches (16x81 = 1,296 fills per patch against 1,089 unique) is
// already inside those numbers.
//
// Three read replicas exist because replay is the new bottleneck: 3,145,728
// corner READS per two-view frame through one port would fail exactly the way
// projection did. Three simultaneous corner reads make it one triangle per
// clock — 1,048,576 replay clocks for the two-view stress, inside the window.
//
// ---------------------------------------------------------------------------
// A SEALED DENSE ARENA, NOT A CACHE, AND NOT THE OLD SPARSE ARENA
// ---------------------------------------------------------------------------
// Like zhao_vertex_arena, identity is GIVEN (the tessellator knows each
// vertex's number), so a lookup IS an index — no tags, no LRU. Unlike it:
//
//   * FILL IS DENSE. There is no fill index port: the write address is the
//     group's own fill counter, so rows 0..DEPTH-1 are written exactly once
//     each, in order, per lifetime. That single restriction deletes the whole
//     sparse validity problem — no per-row valid bitmap (the old arena pays
//     GROUPS*DEPTH flops), no generation-in-RAM ambiguity, no payload reset.
//   * READABILITY IS PER GROUP. A group becomes readable at SEAL, and seal is
//     REFUSED unless exactly DEPTH rows were filled this lifetime. An
//     incompletely filled group can never be read, so whatever the RAM holds
//     after power-up or a previous lifetime is unreachable garbage, not a
//     hazard. THIS is why no bulk reset of payload RAM is needed — and why
//     none is performed: a reset loop over the array would also break M10K
//     inference (QUARTUS_GOTCHAS 10).
//   * ADDRESSING IS PADDED, NEVER MIXED. The old arena allocated ARENAS*DEPTH
//     words but addressed {arena, index} with a power-of-two stride — at
//     DEPTH=1089 arena 1 starts at 2048 in a 2178-word array; index 130 is
//     already out of bounds. Here every group owns a full STRIDE-row lane and
//     the address is {group, index} against a GROUPS*STRIDE allocation, with
//     an elaboration guard that STRIDE >= DEPTH. At the default geometry the
//     padding is FREE: 4x81 = 324 and 4x128 = 512 rows both need six 512x20
//     M10Ks per replica (width, not depth, is the binding constraint).
//
// ---------------------------------------------------------------------------
// THE RECORD, 106 BITS, AND WHY EVERY FIELD STAYS
// ---------------------------------------------------------------------------
// {behind[1], w[31], d[32], y[21], x[21]} — exactly the output register of
// zhao_project_core, nothing added, nothing dropped:
//
//   x, y    : S12.8 canvas coordinates, clamped +/-2048 px. What raster eats.
//   d       : Q16.16 legacy 1/w. Depth interpolation consumes this TODAY;
//             changing depth's number format is a product-law change and is
//             not this module's business.
//   w       : clip.w itself, fx16 raw, 31 bits. GEOM.DEPTHQUANT needs w, and
//             recovering it from the ROUNDED 1/w is forbidden — one rounding
//             happened already, a second would compound. INVARIANT.
//   behind  : clip.w <= 0. Behind the eye the core zeroes x/y/d/w, so a
//             consumer ignoring this flag would read a plausible on-screen
//             vertex at the origin. The flag is the only thing distinguishing
//             "behind" from "at (0,0), infinitely near". INVARIANT.
//
// VIEW IS NOT IN THE RECORD. Projection is view-dependent, so view is part of
// the GROUP's identity (its key), decided at open — a group holds one view's
// results and only that view's. Storing view per row would say the same thing
// 81 times.
//
// ---------------------------------------------------------------------------
// IDENTITY: THE KEY IS THE EXACT FINAL POSITION'S PROVENANCE, NOT AN INDEX
// ---------------------------------------------------------------------------
// A lattice index alone is NOT sufficient identity: the same index projects
// differently under a different deformation epoch, morph/stitch state,
// surface (top vs underside), view, or viewport/matrix epoch. The producer
// therefore opens each group with an opaque KEY_W-bit canonical key packing
// exactly those coordinates (the tessellator composes it; this block never
// interprets it), and every read reply returns the group's key so a consumer
// can audit what it is actually reading. The key is stored per GROUP in
// flops — GROUPS x KEY_W = 128 bits at defaults — because all DEPTH rows of a
// lifetime share one provenance by construction (dense fill under one seal).
// If two uses of the same lattice index have different final positions, they
// are different keys, hence different groups or different lifetimes. The
// per-group GENERATION then catches the temporal hazard the key cannot: a
// handle replayed from a PREVIOUS lifetime of the same group presents a stale
// generation and is refused deterministically.
//
// ---------------------------------------------------------------------------
// LIFETIME: OPEN -> FILL x DEPTH -> SEAL -> READ/REFERENCE -> RELEASE
// ---------------------------------------------------------------------------
//   OPEN     IDLE -> FILLING. Generation increments, key is captured, fill
//            counter zeroes. Opening a non-IDLE group is refused and counted.
//   FILL     one record per clock while FILLING and count < DEPTH. The
//            address is the counter. Fill to any other state, or an
//            out-of-range group, is refused (ready low) and counted.
//   SEAL     FILLING -> SEALED, accepted ONLY at count == DEPTH. A short
//            seal is refused and counted; the group stays FILLING and can
//            complete. Readability begins here and not one cycle earlier.
//   READ     three independent replica ports, one-cycle reply. Refused (with
//            the reply's payload forced to zero, valid still returned) when:
//            group out of range, group not SEALED, generation stale, or
//            index >= DEPTH. Refusal is a REPLY, not backpressure.
//   REFS     consumers holding triangle handles acquire/release per group.
//            The roadmap's starter held exactly one outstanding response;
//            a real integration has raster/attribute stages retaining
//            handles, so the count is a real counter (REF_W wide).
//   RELEASE  SEALED -> IDLE, accepted only with zero outstanding references.
//            A blocked release is refused and counted, and the caller
//            retries — visible, never a hidden deadlock.
//
// With GROUPS=4: per view, one group fills (81 clocks) while the previous
// sealed group replays (128 triangle clocks); two views double it. Fill and
// replay overlap on genuinely different ports (each replica is SDP: the fill
// write broadcasts to all three copies, each copy's read port serves one
// corner), so the overlap adds no port it does not own.
//
// ---------------------------------------------------------------------------
// RAM SHEET (mandatory, per the roadmap's rules)
// ---------------------------------------------------------------------------
//   owner        : projection domain (fed by zhao_project_service client B)
//   entry        : {behind, w[30:0], d[31:0], y[20:0], x[20:0]} = 106 bits
//   depth        : GROUPS x STRIDE = 512 rows (81 of each 128 used)
//   stride       : STRIDE = 128, padded power of two; {group, index} address
//   ports/phase  : per replica, 1 write (fill, broadcast) + 1 read (corner);
//                  legal write and read never touch the same GROUP, enforced
//                  by FILLING/SEALED being disjoint states
//   init         : none. Unreadable until sealed; NO reset loop over payload
//   bypass       : none needed — same-address read/write is protocol-illegal
//   epoch        : per-group generation, GEN_W bits, checked on every read
//   release      : explicit, gated on zero outstanding references
//   blocks       : ceil(106/20) = 6 M10K (512x20) per replica, x3 = 18 M10K
//                  (256x40 gives 2 deep x 3 wide = 6 per replica too; the
//                  bit floor 324x106/10,240 = 3.4 blocks is unreachable at
//                  legal aspects, so 18 is the honest count — it matches the
//                  roadmap's estimate, and it is a geometry calculation, not
//                  a fitted receipt)
//
// Every knob is a parameter. Nothing below is derived from a reference the
// owner cannot edit.

`default_nettype none

module zhao_proj_arena3 #(
    parameter int unsigned GROUPS = 4,    // 2 views x 2 working generations
    parameter int unsigned DEPTH  = 81,   // 9x9 subpatch lattice
    parameter int unsigned STRIDE = 128,  // padded group lane, >= DEPTH, pow2
    parameter int unsigned XW     = 21,   // S12.8 canvas x (core's out_x_o)
    parameter int unsigned YW     = 21,   // S12.8 canvas y (core's out_y_o)
    parameter int unsigned DW     = 32,   // Q16.16 1/w      (core's out_d_o)
    parameter int unsigned WW     = 31,   // guarded clip w  (core's out_w_o)
    parameter int unsigned KEY_W  = 32,   // opaque canonical group key
    parameter int unsigned GEN_W  = 8,    // per-group lifetime generation
    parameter int unsigned REF_W  = 8,    // outstanding-reference counter
    // One bit wider than needed, deliberately, so an out-of-range group or
    // index is REPRESENTABLE and the refusal path exists in silicon, not just
    // in the contract (zhao_vertex_arena's lesson, kept).
    parameter int unsigned GRP_W  = $clog2(GROUPS) + 1,
    parameter int unsigned IDX_W  = $clog2(STRIDE) + 1
) (
    input  wire                    clk,
    input  wire                    rst_n,

    // ---- producer: open ----------------------------------------------------
    input  wire                    open_i,
    input  wire  [GRP_W-1:0]       open_group_i,
    input  wire  [KEY_W-1:0]       open_key_i,
    output logic [GEN_W-1:0]       open_gen_o,    // generation the open creates

    // ---- producer: dense fill (no index port: the counter IS the address) --
    input  wire                    fill_valid_i,
    output wire                    fill_ready_o,
    input  wire  [GRP_W-1:0]       fill_group_i,
    input  wire signed [XW-1:0]    fill_x_i,
    input  wire signed [YW-1:0]    fill_y_i,
    input  wire signed [DW-1:0]    fill_d_i,
    input  wire  [WW-1:0]          fill_w_i,
    input  wire                    fill_behind_i,

    // ---- producer: seal / release ------------------------------------------
    input  wire                    seal_i,
    input  wire  [GRP_W-1:0]       seal_group_i,
    input  wire                    rel_i,
    input  wire  [GRP_W-1:0]       rel_group_i,

    // ---- consumers: reference accounting -----------------------------------
    input  wire                    ref_acq_i,
    input  wire  [GRP_W-1:0]       ref_acq_group_i,
    input  wire                    ref_rel_i,
    input  wire  [GRP_W-1:0]       ref_rel_group_i,

    // ---- consumers: three corner read replicas, one-cycle reply ------------
    input  wire                    rd0_valid_i,
    input  wire  [GRP_W-1:0]       rd0_group_i,
    input  wire  [GEN_W-1:0]       rd0_gen_i,
    input  wire  [IDX_W-1:0]       rd0_index_i,
    output logic                   rd0_rep_valid_o,
    output logic                   rd0_refuse_o,
    output logic signed [XW-1:0]   rd0_x_o,
    output logic signed [YW-1:0]   rd0_y_o,
    output logic signed [DW-1:0]   rd0_d_o,
    output logic [WW-1:0]          rd0_w_o,
    output logic                   rd0_behind_o,
    output logic [KEY_W-1:0]       rd0_key_o,

    input  wire                    rd1_valid_i,
    input  wire  [GRP_W-1:0]       rd1_group_i,
    input  wire  [GEN_W-1:0]       rd1_gen_i,
    input  wire  [IDX_W-1:0]       rd1_index_i,
    output logic                   rd1_rep_valid_o,
    output logic                   rd1_refuse_o,
    output logic signed [XW-1:0]   rd1_x_o,
    output logic signed [YW-1:0]   rd1_y_o,
    output logic signed [DW-1:0]   rd1_d_o,
    output logic [WW-1:0]          rd1_w_o,
    output logic                   rd1_behind_o,
    output logic [KEY_W-1:0]       rd1_key_o,

    input  wire                    rd2_valid_i,
    input  wire  [GRP_W-1:0]       rd2_group_i,
    input  wire  [GEN_W-1:0]       rd2_gen_i,
    input  wire  [IDX_W-1:0]       rd2_index_i,
    output logic                   rd2_rep_valid_o,
    output logic                   rd2_refuse_o,
    output logic signed [XW-1:0]   rd2_x_o,
    output logic signed [YW-1:0]   rd2_y_o,
    output logic signed [DW-1:0]   rd2_d_o,
    output logic [WW-1:0]          rd2_w_o,
    output logic                   rd2_behind_o,
    output logic [KEY_W-1:0]       rd2_key_o,

    // ---- observation -------------------------------------------------------
    // Every refusal class has its own counter, because a refusal folded into
    // a neighbour's count is a refusal nobody investigates. All are reachable
    // with STIMULUS ALONE (an illegal request on a legal port), so each can
    // be fired in the bench before its silence is ever quoted.
    output logic [31:0]            fills_o,
    output logic [31:0]            seals_o,
    output logic [31:0]            releases_o,
    output logic [31:0]            open_illegal_o,    // open of non-IDLE/oob group
    output logic [31:0]            fill_illegal_o,    // fill attempt-cycles refused
    output logic [31:0]            seal_short_o,      // seal at count != DEPTH
    output logic [31:0]            read_refusals_o,   // all replicas, all causes
    output logic [31:0]            release_blocked_o, // release with refs != 0
    output logic [31:0]            ref_err_o          // acq on non-SEALED / rel at 0
);

  localparam int unsigned GA    = (GROUPS > 1) ? $clog2(GROUPS) : 1;
  localparam int unsigned SB    = $clog2(STRIDE);
  localparam int unsigned REC_W = XW + YW + DW + WW + 1;
  localparam int unsigned CNT_W = $clog2(DEPTH + 1);
  localparam int unsigned AW    = GA + SB;

  // Elaboration guards. Quartus 17 requires these inside `initial begin`.
  initial begin
    if (STRIDE < DEPTH)
      $fatal(1, "zhao_proj_arena3: STRIDE (%0d) must be >= DEPTH (%0d)",
             STRIDE, DEPTH);
    if ((STRIDE & (STRIDE - 1)) != 0)
      $fatal(1, "zhao_proj_arena3: STRIDE (%0d) must be a power of two",
             STRIDE);
    if (GROUPS < 2)
      $fatal(1, "zhao_proj_arena3: GROUPS (%0d) < 2 cannot overlap fill/replay",
             GROUPS);
  end

  // ---- per-group control state (flops; HOT CONTROL stays out of RAM) -------
  typedef enum logic [1:0] { S_IDLE = 2'd0, S_FILL = 2'd1, S_SEALED = 2'd2 }
      state_e;

  state_e             state_q [0:GROUPS-1];
  logic [GEN_W-1:0]   gen_q   [0:GROUPS-1];
  logic [KEY_W-1:0]   key_q   [0:GROUPS-1];
  logic [CNT_W-1:0]   cnt_q   [0:GROUPS-1];
  logic [REF_W-1:0]   refs_q  [0:GROUPS-1];

  // ---- request legality -----------------------------------------------------
  wire open_oob = (open_group_i    >= GRP_W'(GROUPS));
  wire fill_oob = (fill_group_i    >= GRP_W'(GROUPS));
  wire seal_oob = (seal_group_i    >= GRP_W'(GROUPS));
  wire rel_oob  = (rel_group_i     >= GRP_W'(GROUPS));
  wire racq_oob = (ref_acq_group_i >= GRP_W'(GROUPS));
  wire rrel_oob = (ref_rel_group_i >= GRP_W'(GROUPS));

  wire open_ok = open_i && !open_oob
               && (state_q[open_group_i[GA-1:0]] == S_IDLE);
  wire fill_ok = fill_valid_i && !fill_oob
               && (state_q[fill_group_i[GA-1:0]] == S_FILL)
               && (cnt_q[fill_group_i[GA-1:0]] < CNT_W'(DEPTH));
  wire seal_ok = seal_i && !seal_oob
               && (state_q[seal_group_i[GA-1:0]] == S_FILL)
               && (cnt_q[seal_group_i[GA-1:0]] == CNT_W'(DEPTH));
  wire rel_ok  = rel_i && !rel_oob
               && (state_q[rel_group_i[GA-1:0]] == S_SEALED)
               && (refs_q[rel_group_i[GA-1:0]] == '0);
  wire racq_ok = ref_acq_i && !racq_oob
               && (state_q[ref_acq_group_i[GA-1:0]] == S_SEALED)
               && (refs_q[ref_acq_group_i[GA-1:0]] != {REF_W{1'b1}});
  wire rrel_ok = ref_rel_i && !rrel_oob
               && (refs_q[ref_rel_group_i[GA-1:0]] != '0);

  assign fill_ready_o = fill_ok;
  assign open_gen_o   = open_oob ? '0
                      : (gen_q[open_group_i[GA-1:0]] + GEN_W'(1));

  wire [AW-1:0] fill_addr =
      {fill_group_i[GA-1:0], cnt_q[fill_group_i[GA-1:0]][SB-1:0]};
  wire [REC_W-1:0] fill_rec =
      {fill_behind_i, fill_w_i, fill_d_i, fill_y_i, fill_x_i};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      for (int g = 0; g < GROUPS; g = g + 1) begin
        state_q[g] <= S_IDLE;
        gen_q[g]   <= '0;
        key_q[g]   <= '0;
        cnt_q[g]   <= '0;
        refs_q[g]  <= '0;
      end
      fills_o           <= '0;
      seals_o           <= '0;
      releases_o        <= '0;
      open_illegal_o    <= '0;
      fill_illegal_o    <= '0;
      seal_short_o      <= '0;
      release_blocked_o <= '0;
      ref_err_o         <= '0;
    end else begin
      if (open_ok) begin
        state_q[open_group_i[GA-1:0]] <= S_FILL;
        gen_q[open_group_i[GA-1:0]]   <= gen_q[open_group_i[GA-1:0]] + GEN_W'(1);
        key_q[open_group_i[GA-1:0]]   <= open_key_i;
        cnt_q[open_group_i[GA-1:0]]   <= '0;
      end
      if (open_i && !open_ok) open_illegal_o <= open_illegal_o + 32'd1;

      if (fill_ok) begin
        cnt_q[fill_group_i[GA-1:0]] <= cnt_q[fill_group_i[GA-1:0]] + CNT_W'(1);
        fills_o <= fills_o + 32'd1;
      end
      if (fill_valid_i && !fill_ok) fill_illegal_o <= fill_illegal_o + 32'd1;

      if (seal_ok) begin
        state_q[seal_group_i[GA-1:0]] <= S_SEALED;
        seals_o <= seals_o + 32'd1;
      end
      if (seal_i && !seal_ok) seal_short_o <= seal_short_o + 32'd1;

      if (rel_ok) begin
        state_q[rel_group_i[GA-1:0]] <= S_IDLE;
        releases_o <= releases_o + 32'd1;
      end
      if (rel_i && !rel_ok) release_blocked_o <= release_blocked_o + 32'd1;

      // Reference accounting. A same-cycle acquire and release on the SAME
      // group must net to zero THROUGH ONE UPDATE — two separate writes to
      // one register in one cycle would lose the first — so that case is
      // resolved explicitly as "unchanged".
      if (racq_ok && rrel_ok
          && (ref_acq_group_i[GA-1:0] == ref_rel_group_i[GA-1:0])) begin
        // net zero: counter unchanged
      end else begin
        if (racq_ok)
          refs_q[ref_acq_group_i[GA-1:0]] <=
              refs_q[ref_acq_group_i[GA-1:0]] + REF_W'(1);
        if (rrel_ok)
          refs_q[ref_rel_group_i[GA-1:0]] <=
              refs_q[ref_rel_group_i[GA-1:0]] - REF_W'(1);
      end
      if (ref_acq_i && !racq_ok) ref_err_o <= ref_err_o + 32'd1;
      if (ref_rel_i && !rrel_ok) ref_err_o <= ref_err_o + 32'd1;
    end
  end

  // ---- the three replicas ---------------------------------------------------
  // Arrays assembled from the explicit port triples so one generate block
  // holds the single copy of the read law. Each replica's RAM is written by
  // the SAME broadcast fill enable with the SAME data — there is no per-copy
  // write path to drift, which is what makes a copy a copy.
  logic             rdv_c   [0:2];
  logic [GRP_W-1:0] rdg_c   [0:2];
  logic [GEN_W-1:0] rdgen_c [0:2];
  logic [IDX_W-1:0] rdi_c   [0:2];

  always_comb begin
    rdv_c[0] = rd0_valid_i;  rdg_c[0] = rd0_group_i;
    rdgen_c[0] = rd0_gen_i;  rdi_c[0] = rd0_index_i;
    rdv_c[1] = rd1_valid_i;  rdg_c[1] = rd1_group_i;
    rdgen_c[1] = rd1_gen_i;  rdi_c[1] = rd1_index_i;
    rdv_c[2] = rd2_valid_i;  rdg_c[2] = rd2_group_i;
    rdgen_c[2] = rd2_gen_i;  rdi_c[2] = rd2_index_i;
  end

  logic             rep_v_q   [0:2];
  logic             rep_ref_q [0:2];
  logic [REC_W-1:0] rec_q     [0:2];
  logic [KEY_W-1:0] rep_key_q [0:2];

  // Refusal count can be 0..3 per cycle; sum, then accumulate once.
  logic [1:0] refuse_now_c;
  always_comb begin
    refuse_now_c = 2'd0;
    for (int k = 0; k < 3; k = k + 1) begin
      if (rdv_c[k] && (
            (rdg_c[k] >= GRP_W'(GROUPS))
         || (state_q[rdg_c[k][GA-1:0]] != S_SEALED)
         || (rdgen_c[k] != gen_q[rdg_c[k][GA-1:0]])
         || (rdi_c[k] >= IDX_W'(DEPTH))))
        refuse_now_c = refuse_now_c + 2'd1;
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) read_refusals_o <= '0;
    else        read_refusals_o <= read_refusals_o + 32'(refuse_now_c);
  end

  genvar gk;
  generate
    for (gk = 0; gk < 3; gk = gk + 1) begin : g_replica
      // NO RESET TOUCHES THE STORE. Synchronous read, whole-word write —
      // the properties QUARTUS_GOTCHAS 10 measured as decisive for M10K
      // inference. Same-address read-during-write cannot occur legally
      // (fill needs S_FILL, read needs S_SEALED), so no bypass exists.
      logic [REC_W-1:0] mem [0:GROUPS*STRIDE-1];

      wire          k_oob   = (rdg_c[gk] >= GRP_W'(GROUPS));
      wire [GA-1:0] k_grp   = rdg_c[gk][GA-1:0];
      wire          k_refuse = k_oob
                             || (state_q[k_grp] != S_SEALED)
                             || (rdgen_c[gk] != gen_q[k_grp])
                             || (rdi_c[gk] >= IDX_W'(DEPTH));
      wire [AW-1:0] k_addr  = {k_grp, rdi_c[gk][SB-1:0]};

      always_ff @(posedge clk) begin
        if (fill_ok) mem[fill_addr] <= fill_rec;
        rec_q[gk] <= mem[k_addr];
      end

      always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
          rep_v_q[gk]   <= 1'b0;
          rep_ref_q[gk] <= 1'b0;
          rep_key_q[gk] <= '0;
        end else begin
          rep_v_q[gk]   <= rdv_c[gk];
          rep_ref_q[gk] <= k_refuse;
          rep_key_q[gk] <= k_oob ? '0 : key_q[k_grp];
        end
      end
    end
  endgenerate

  // A REFUSED reply carries a ZEROED payload, deterministically — the same
  // convention the core uses for behind-eye vertices. A consumer that
  // ignores the refuse flag reads zeros, never another group's vertex.
  wire [REC_W-1:0] rep0_rec = rep_ref_q[0] ? {REC_W{1'b0}} : rec_q[0];
  wire [REC_W-1:0] rep1_rec = rep_ref_q[1] ? {REC_W{1'b0}} : rec_q[1];
  wire [REC_W-1:0] rep2_rec = rep_ref_q[2] ? {REC_W{1'b0}} : rec_q[2];

  assign rd0_rep_valid_o = rep_v_q[0];
  assign rd0_refuse_o    = rep_v_q[0] && rep_ref_q[0];
  assign rd0_x_o         = rep0_rec[XW-1:0];
  assign rd0_y_o         = rep0_rec[XW +: YW];
  assign rd0_d_o         = rep0_rec[XW+YW +: DW];
  assign rd0_w_o         = rep0_rec[XW+YW+DW +: WW];
  assign rd0_behind_o    = rep0_rec[REC_W-1];
  assign rd0_key_o       = rep_ref_q[0] ? {KEY_W{1'b0}} : rep_key_q[0];

  assign rd1_rep_valid_o = rep_v_q[1];
  assign rd1_refuse_o    = rep_v_q[1] && rep_ref_q[1];
  assign rd1_x_o         = rep1_rec[XW-1:0];
  assign rd1_y_o         = rep1_rec[XW +: YW];
  assign rd1_d_o         = rep1_rec[XW+YW +: DW];
  assign rd1_w_o         = rep1_rec[XW+YW+DW +: WW];
  assign rd1_behind_o    = rep1_rec[REC_W-1];
  assign rd1_key_o       = rep_ref_q[1] ? {KEY_W{1'b0}} : rep_key_q[1];

  assign rd2_rep_valid_o = rep_v_q[2];
  assign rd2_refuse_o    = rep_v_q[2] && rep_ref_q[2];
  assign rd2_x_o         = rep2_rec[XW-1:0];
  assign rd2_y_o         = rep2_rec[XW +: YW];
  assign rd2_d_o         = rep2_rec[XW+YW +: DW];
  assign rd2_w_o         = rep2_rec[XW+YW+DW +: WW];
  assign rd2_behind_o    = rep2_rec[REC_W-1];
  assign rd2_key_o       = rep_ref_q[2] ? {KEY_W{1'b0}} : rep_key_q[2];

endmodule

`default_nettype wire
