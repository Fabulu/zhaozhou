// zhao_terrain_edgerecon.sv -- TERRAIN.EDGERECON: the NEIGHBOUR-EDGE LEVEL
// PRODUCER. Owner ruling 2026-09-22 item 6, ratified in
// `reports/OWNER-RATIFICATION-20260922-COMPLETION.md`:
//
//     "For the full-capability target, authorize implementing the real
//      neighbor-edge level producer ... The coordinator may introduce bounded
//      prepare/reconcile/emit sequencing over the admitted terrain set for the
//      frame ... This may buffer decisions and adjacency metadata; it does not
//      authorize a second full terrain engine or a duplicate world/geometry
//      payload store."
//
// Law (in citation order):
//   fpga/rtl/terrain/zhao_terrain_lod.sv -- the CONSUMER. Its `edge_nz_i`,
//       `edge_pz_i`, `edge_nx_i`, `edge_px_i` are four 2-bit lanes each,
//       indexed ALONG that edge, and they reach exactly one place:
//         nb_nz = (e_j != 0) ? lvl[{e_j-1, e_i}] : edge_lane(edge_nz_i, e_i);
//         nb_pz = (e_j != 3) ? lvl[{e_j+1, e_i}] : edge_lane(edge_pz_i, e_i);
//         nb_nx = (e_i != 0) ? lvl[{e_j, e_i-1}] : edge_lane(edge_nx_i, e_j);
//         nb_px = (e_i != 3) ? lvl[{e_j, e_i+1}] : edge_lane(edge_px_i, e_j);
//       -- the four BORDER lanes of the 4 x 4 subpatch grid, and nothing else.
//   fpga/rtl/terrain/zhao_terrain_tess.sv -- the arithmetic that makes this
//       block's law provable, and the reason it cannot be half-done:
//         lv_px = (job_lvl_px_i > job_level_i) ? job_lvl_px_i : job_level_i;
//       i.e. the shared edge is tessellated at MAX(neighbour, own) -- the
//       COARSER of the two. `max` is symmetric, so two patches agree on their
//       shared edge IF AND ONLY IF each is given the other's true level, or
//       BOTH are given the same wrong one.
//   design/contracts/TERRAIN.LOD.md -- "must be held stable across a patch
//       job", which is why the query result is REGISTERED and held.
//   design/contracts/TERRAIN.EDGERECON.md -- this block.
//   spec/terrain_rules.md 2.1 (a patch is 4 x 4 subpatches), 4.2 (the live /
//       visible set is 256 patches).
//
// ENFORCED-BY: tests/terrain/terrain_edgerecon_directed.cpp:main
//
// ===========================================================================
// THE STRUCTURAL FACT THIS BLOCK IS BUILT ON, AND IT IS WHY ONE PASS SUFFICES
// ===========================================================================
// `edge_*` reaches `out_lvl_nz/pz/nx/px` and NOTHING ELSE. `out_level_o` is
// `lvl[]`, decided by the projected-error ladder from `sp_*` and the governor
// alone. **A patch's own sixteen levels do not depend on its neighbours.**
//
// So the reconciliation has NO fixpoint to iterate. One pass decides every
// admitted patch's own levels; a second, pure lookup fills in the borders.
// That is the whole reason "bounded prepare/reconcile/emit" is bounded, and it
// was MEASURED in `zhao_terrain_lod.sv` rather than assumed -- the four
// `edge_*` ports appear on four lines of that file and nowhere else.
//
// ===========================================================================
// AND THE REASON A STREAMING, SERVE-ORDER PRODUCER IS NOT AN OPTION
// ===========================================================================
// The tempting cheap build is: file each patch's levels as it is served, and
// answer later patches from whatever has been filed. It is WRONG, and the
// tessellator's arithmetic above says why in one line.
//
// Take P at level 1 beside Q at level 2. Serve P first; Q is not yet filed, so
// P is answered with the conservative fallback 0 and tessellates its shared
// edge at max(0,1) = 1. Serve Q second; P IS filed, so Q is answered 1 and
// tessellates the same edge at max(1,2) = 2. **One side emits level-1 density
// and the other level-2 density on the same seam. That is a crack in the
// ground**, with every counter in the console balancing.
//
// Repairing it by making the second patch ALSO fall back (so the pair stays
// symmetric) makes every edge fall back, because the first-served patch of any
// pair never has its neighbour: the scheme degenerates to the constant it was
// meant to replace. **There is no partial answer.** The frame's admitted set
// must be DECIDED before any of it is EMITTED, which is exactly the sequencing
// the ruling authorises, and this block is the store and the law for it.
//
// ===========================================================================
// THE SYMMETRY LAW -- the theorem the crack-free property rests on
// ===========================================================================
// A query for patch P reads FIVE records: P's own, and its four neighbours'.
// Define
//
//     ok(X)  ==  bank[idx(X)].valid
//            &&  bank[idx(X)].tag == X
//            && !bank[idx(X)].poison
//            &&  bank[idx(X)].mask == 16'hFFFF      (all sixteen decided)
//
// and answer edge e between P and its neighbour N with N's true border row
// when `ok(P) && ok(N)`, and with the conservative 8'h00 otherwise.
//
// **The predicate is symmetric in P and N**, and the bank is FROZEN for the
// whole emit phase (a file offered outside PREPARE is refused and counted), so
// P's query and N's query evaluate the identical boolean from the identical
// bits. Either both sides get each other's true level -- and TESS's max()
// agrees on both sides -- or both sides get 8'h00 -- and TESS's max() reduces
// to each side's own level, which is the console's existing behaviour on that
// seam. **No arrangement of records can make the two sides disagree.**
//
// `ok(P)` is in the predicate for both of P's own edges AND for every query
// that names P as a neighbour, which is what closes the one asymmetry a
// direct-mapped bank can produce: if two live patches collide on one index the
// entry is POISONED, and a poisoned entry fails `ok` for its owner and for
// every querier alike. A collision therefore costs triangles on four seams and
// cannot cost a crack on any.
//
// ===========================================================================
// THE SWEEP IS THE "NO PREVIOUS-FRAME SHORTCUT" ENFORCEMENT
// ===========================================================================
// The ruling says: "Do not replace current-frame ownership with an unvalidated
// previous-frame shortcut." That is enforced structurally rather than
// promised: `frame_begin_i` walks every entry's valid bit to zero before the
// PREPARE phase opens, so **every record any query can read was filed in the
// frame that query belongs to.** A patch that left the admitted set is not
// stale data with an old tag, it is absent, and its neighbours fall back.
//
// The sweep is ENTRIES clocks -- 256 at the defaults, against a 1.67 M-clock
// frame. An epoch tag was considered instead and rejected: it is cheaper by
// 256 clocks a frame and it makes staleness a function of the tag width, which
// is a correctness property nobody can test at the width that matters.
//
// ===========================================================================
// WHY A DIRECT-MAPPED BANK AND NOT A DIRECTORY
// ===========================================================================
// `zhao_terrain_island_dir` does this arithmetic for the page store and its
// answer is worth repeating: an 8 km island is 125 x 125 patches, so a dense
// table over the grid is 16,384 entries for an occupancy of a few hundred --
// tens of M10Ks to hold mostly nothing.
//
// The residency solves that with a set-associative directory, and this block
// deliberately does NOT reuse it, because the reuse would be a second reader
// on a port the paging spine owns and a `(ix,iz) -> slot` lookup per neighbour
// per patch. Instead the bank is DIRECT-MAPPED on the low bits of the patch
// coordinate, `idx = {iz[IZW-1:0], ix[IXW-1:0]}`, with the FULL coordinate
// kept as a tag. At the default 4 + 4 that is a 16 x 16 patch window -- 1,024 m
// square at the canonical 2.0 m pitch -- which is larger than any view the
// visible-set radius produces, so two live patches collide only when the
// camera set spans more than sixteen patches in one axis. **A collision is not
// a fault and not a crack: it is the conservative fallback on four seams**, and
// it is counted so that the day a view outgrows the window the machine says so
// rather than a reviewer noticing.
//
// An off-island neighbour needs no special case: `ix - 1` at `ix == 0` wraps to
// 16'hFFFF, no patch can carry that coordinate (the island extent is 125), the
// tag compare fails, and the lane falls back. The wrap is the answer, not a
// hole in it.
//
// ===========================================================================
// WHAT IS NOT HERE, NAMED
// ===========================================================================
//   * NO LADDER. This block never decides a level. It stores the decisions
//     `zhao_terrain_lod` made and hands four of them back. A second ladder
//     would be the "second full terrain engine" the ruling refuses.
//   * NO HEIGHTS, NO LATTICE, NO PAGE BYTES. Thirty-two bits of decision per
//     patch. It is not a world store.
//   * NO ADMITTED-SET WALKER. Who presents the frame's patches in the PREPARE
//     phase is the caller's business; this block owns the store, the law and
//     the sequencing gate. See the contract's "what still has no producer".
//
// Conservative SystemVerilog subset only (charter section 2); explicit
// generate is not needed (no generate blocks); the $fatal guards live in
// `initial begin` (Quartus 17). `--lint-only` does not run them.
`default_nettype none

module zhao_terrain_edgerecon #(
    // The low bits of the patch coordinate that select a bank entry. The
    // window is (1<<IXW) x (1<<IZW) patches; two LIVE patches that differ by a
    // multiple of it collide and both fall back.
    parameter int unsigned IXW = 4,
    parameter int unsigned IZW = 4
) (
    input  var logic clk,
    input  var logic rst_n,

    // ---- the frame phase ---------------------------------------------------
    // PULSES, both of them. `frame_begin_i` sweeps the bank and opens the
    // PREPARE phase; `prepare_done_i` freezes the bank and opens EMIT. A file
    // beat offered outside PREPARE is REFUSED and counted -- the freeze is what
    // makes the symmetry law's "identical bits" true.
    input  var logic frame_begin_i,
    input  var logic prepare_done_i,
    output var logic [1:0] phase_o,        // 0 = sweep, 1 = prepare, 2 = emit
    output var logic [15:0] frame_count_o,

    // ---- the FILE port: `zhao_terrain_lod`'s emit stream, PREPARE phase ----
    // `f_ox_i`/`f_oz_i`/`f_level_i`/`f_surface_i` are TERRAIN.LOD's
    // `out_ox_o`/`out_oz_o`/`out_level_o`/`out_surface_o` field for field, so
    // the two wire with no adapter. `f_ix_i`/`f_iz_i` are the PATCH's grid
    // coordinate, held across the patch exactly as the governor's targets are.
    input  var logic        f_valid_i,
    output var logic        f_ready_o,
    input  var logic [15:0] f_ix_i,
    input  var logic [15:0] f_iz_i,
    // Only bits [4:3] of each are read: TERRAIN.LOD emits `ox = i*8` and
    // `oz = j*8` over a 4 x 4 grid, so [2:0] are structurally zero and [5] is
    // structurally unused. The full six bits are taken so the port matches
    // that block's field for field and no adapter can hide between them.
    /* verilator lint_off UNUSEDSIGNAL */
    input  var logic [ 5:0] f_ox_i,
    input  var logic [ 5:0] f_oz_i,
    /* verilator lint_on UNUSEDSIGNAL */
    input  var logic [ 1:0] f_level_i,
    input  var logic        f_surface_i,   // 1 = the underside replay; consumed, not filed

    // ---- the QUERY port: one patch, four edges, EMIT phase ------------------
    input  var logic        q_valid_i,
    output var logic        q_ready_o,
    input  var logic [15:0] q_ix_i,
    input  var logic [15:0] q_iz_i,
    output var logic        q_done_o,      // PULSE: the four words below are new

    // ---- the answer: `zhao_terrain_lod`'s `edge_*_i`, port for port ---------
    // HELD until the next query completes, which is TERRAIN.LOD.md's "must be
    // held stable across a patch job".
    output var logic [ 7:0] edge_nz_o,
    output var logic [ 7:0] edge_pz_o,
    output var logic [ 7:0] edge_nx_o,
    output var logic [ 7:0] edge_px_o,
    // Bit 0 = -z, 1 = +z, 2 = -x, 3 = +x. HIGH means the word above carries a
    // real neighbour decision; LOW means the conservative fallback.
    output var logic [ 3:0] edge_real_o,

    // ---- counters -----------------------------------------------------------
    output var logic [31:0] records_filed_o,      // patches whose sixteenth lane landed
    output var logic [31:0] lanes_filed_o,        // subpatch levels written
    output var logic [31:0] collisions_o,         // entries poisoned by two live patches
    output var logic [31:0] queries_o,            // queries answered
    output var logic [31:0] edges_real_o,         // lanes answered from a decision
    output var logic [31:0] edges_fallback_o,     // lanes answered 8'h00
    output var logic [31:0] query_own_missing_o,  // the QUERIED patch had no usable record
    output var logic [31:0] file_out_of_phase_o,  // a file beat outside PREPARE
    output var logic [31:0] query_out_of_phase_o, // a query outside EMIT
    output var logic        busy_o
);

  // --------------------------------------------------------------------------
  // Elaboration guards. Inside `initial begin ... end`: Quartus 17.0 rejects a
  // bare module-scope `if`, and a clean `--lint-only` says nothing about these.
  // --------------------------------------------------------------------------
  // verilator lint_off IGNOREDRETURN
  initial begin
    if (IXW < 1 || IXW > 8) begin
      $fatal(1, "zhao_terrain_edgerecon: IXW must be 1..8");
    end
    if (IZW < 1 || IZW > 8) begin
      $fatal(1, "zhao_terrain_edgerecon: IZW must be 1..8");
    end
  end
  // verilator lint_on IGNOREDRETURN

  localparam int unsigned IDXW    = IXW + IZW;    // w=8 at the defaults
  localparam int unsigned ENTRIES = 1 << IDXW;    // 256 entries at the defaults
  localparam int unsigned TAGW    = 32;           // w=32, the full {ix, iz}
  localparam int unsigned LVLW    = 32;           // w=32, sixteen 2-bit levels
  localparam int unsigned MASKW   = 16;           // w=16, one bit per subpatch
  // valid + poison beside the tag, the levels and the fill mask.
  localparam int unsigned BW      = TAGW + LVLW + MASKW + 2;  // 82

  // Field offsets inside a bank word. Written as offsets rather than a packed
  // struct because the conservative subset keeps the RAM a plain vector, which
  // is what Quartus infers an M10K from.
  localparam int unsigned OFF_TAG   = 0;                    // 0
  localparam int unsigned OFF_LVL   = TAGW;                 // 32
  localparam int unsigned OFF_MASK  = TAGW + LVLW;          // 64
  localparam int unsigned OFF_VALID = OFF_MASK + MASKW;     // 80
  localparam int unsigned OFF_POIS  = OFF_VALID + 1;        // 81

  localparam logic [1:0] PhSweep   = 2'd0;
  localparam logic [1:0] PhPrepare = 2'd1;
  localparam logic [1:0] PhEmit    = 2'd2;

  localparam logic [1:0] FsIdle  = 2'd0;
  localparam logic [1:0] FsWrite = 2'd1;

  localparam logic [2:0] QsIdle = 3'd0;
  localparam logic [2:0] QsOwn  = 3'd1;
  localparam logic [2:0] QsNz   = 3'd2;
  localparam logic [2:0] QsPz   = 3'd3;
  localparam logic [2:0] QsNx   = 3'd4;
  localparam logic [2:0] QsPx   = 3'd5;
  localparam logic [2:0] QsDone = 3'd6;

  // ==========================================================================
  // the bank -- one write port, one read port, registered read
  // ==========================================================================
  logic [BW-1:0] bank_q [ENTRIES];

  logic [IDXW-1:0] raddr_c;
  logic [IDXW-1:0] waddr_c;
  logic            we_c;
  logic [BW-1:0]   wdata_c;
  logic [BW-1:0]   rd_c;

  always_ff @(posedge clk) begin
    if (we_c) bank_q[waddr_c] <= wdata_c;
    rd_c <= bank_q[raddr_c];
  end

  // The one place a write and a read of the SAME address could meet is the
  // file path's read-modify-write, and it cannot: `f_ready_o` is low on the
  // write cycle, so the next presentation is a clock later and reads the
  // committed word. Stated because "read-old on a same-address write" is the
  // arena primitive's documented behaviour and a reader will look for it here.

  // ==========================================================================
  // decoding a bank word
  // ==========================================================================
  wire [TAGW-1:0]  rd_tag   = rd_c[OFF_TAG  +: TAGW];
  wire [LVLW-1:0]  rd_lvl   = rd_c[OFF_LVL  +: LVLW];
  wire [MASKW-1:0] rd_mask  = rd_c[OFF_MASK +: MASKW];
  wire             rd_valid = rd_c[OFF_VALID];
  wire             rd_pois  = rd_c[OFF_POIS];

  // ==========================================================================
  // phase, sweep and frame count
  // ==========================================================================
  logic [1:0]      phase_q;
  logic            sweep_to_prepare_q;   // where this sweep lands
  logic [IDXW-1:0] sweep_idx_q;
  logic [15:0]     frame_count_q;

  assign phase_o       = phase_q;
  assign frame_count_o = frame_count_q;

  // ==========================================================================
  // the FILE path
  // ==========================================================================
  // Two cycles per lane: present the address, then write the modified word.
  // `f_ready_o` is high only on the presenting cycle, so the port throttles
  // itself to one lane every other clock. TERRAIN.LOD emits one descriptor per
  // clock at best and holds `out_valid_o` until taken, so the cost is the
  // PREPARE phase running at half rate -- 64 clocks a patch against a ladder
  // that spends 784 on the same patch.
  logic [1:0]      fs_q;
  logic [15:0]     f_ix_q, f_iz_q;
  logic [ 3:0]     f_sub_q;
  logic [ 1:0]     f_lvl_q;
  logic [IDXW-1:0] f_idx_q;

  // The subpatch index n = {j, i} with ox = i*8 and oz = j*8, which is
  // TERRAIN.LOD's fixed emit order stated as arithmetic.
  wire [3:0]       f_sub_c = {f_oz_i[4:3], f_ox_i[4:3]};
  wire [IDXW-1:0]  f_idx_c = {f_iz_i[IZW-1:0], f_ix_i[IXW-1:0]};
  wire [TAGW-1:0]  f_tag_q = {f_ix_q, f_iz_q};

  // The underside replay carries the top's level (TERRAIN.LOD law 7) and would
  // file the identical bit into the identical lane. It is CONSUMED but not
  // filed, so `lanes_filed_o` counts decisions and not beats.
  wire f_file_c = (phase_q == PhPrepare) && (fs_q == FsIdle) && f_valid_i && !f_surface_i;

  assign f_ready_o = (phase_q == PhPrepare) && (fs_q == FsIdle);

  // The write decision, shared by the datapath and the counters so that the
  // count and the effect cannot disagree.
  wire f_hit_c   = rd_valid && (rd_tag == f_tag_q);
  wire f_clash_c = rd_valid && (rd_tag != f_tag_q);

  // ==========================================================================
  // the QUERY path
  // ==========================================================================
  logic [2:0]  qs_q;
  logic [15:0] q_ix_q, q_iz_q;
  logic        q_own_ok_q;
  logic [ 7:0] q_nz_q, q_pz_q, q_nx_q, q_px_q;
  logic [ 3:0] q_real_q;

  assign q_ready_o = (phase_q == PhEmit) && (qs_q == QsIdle);

  // THE COORDINATE THE CURRENT STATE IS CHECKING -- the record whose word is
  // on `rd_c` this cycle. One expression, so a state that reads one cell and
  // checks another cannot exist.
  logic [15:0] qr_ix_c, qr_iz_c;
  always_comb begin
    qr_ix_c = q_ix_q;
    qr_iz_c = q_iz_q;
    case (qs_q)
      QsNz: qr_iz_c = q_iz_q - 16'd1;
      QsPz: qr_iz_c = q_iz_q + 16'd1;
      QsNx: qr_ix_c = q_ix_q - 16'd1;
      QsPx: qr_ix_c = q_ix_q + 16'd1;
      default: begin
        qr_ix_c = q_ix_q;
        qr_iz_c = q_iz_q;
      end
    endcase
  end

  wire [TAGW-1:0] qr_tag_c = {qr_ix_c, qr_iz_c};

  // THE COORDINATE THE CURRENT STATE MUST FETCH -- the record the SUCCESSOR
  // state will check. The read is a clock deep, so the walk presents one
  // address ahead of the one it is testing; getting these two the same way
  // round is the whole content of the walk.
  // Only the low IXW/IZW bits reach the bank address; the full width is
  // carried so the +/- 1 wraps exactly as the tag compare expects.
  /* verilator lint_off UNUSEDSIGNAL */
  logic [15:0] qn_ix_c, qn_iz_c;
  /* verilator lint_on UNUSEDSIGNAL */
  always_comb begin
    qn_ix_c = q_ix_q;
    qn_iz_c = q_iz_q;
    case (qs_q)
      QsOwn: qn_iz_c = q_iz_q - 16'd1;     // the successor is QsNz
      QsNz:  qn_iz_c = q_iz_q + 16'd1;     // ... QsPz
      QsPz:  qn_ix_c = q_ix_q - 16'd1;     // ... QsNx
      QsNx:  qn_ix_c = q_ix_q + 16'd1;     // ... QsPx
      default: begin
        qn_ix_c = q_ix_q;
        qn_iz_c = q_iz_q;
      end
    endcase
  end

  // `ok(X)`, evaluated on the word that came back this cycle. ONE expression,
  // used for the queried patch and for every neighbour, which is what makes
  // the symmetry law a property of the code and not of a comment.
  wire rec_ok_c = rd_valid
                && !rd_pois
                && (rd_tag == qr_tag_c)
                && (rd_mask == {MASKW{1'b1}});

  // The border rows of the record just read, packed into the lane order
  // `zhao_terrain_lod`'s `edge_lane()` expects -- lane k occupies bits
  // [2k+1:2k]. Subpatch n = {j, i}.
  //   the -z NEIGHBOUR's shared row is its j = 3 row, lanes i = 0..3
  //   the +z NEIGHBOUR's shared row is its j = 0 row, lanes i = 0..3
  //   the -x NEIGHBOUR's shared column is its i = 3 column, lanes j = 0..3
  //   the +x NEIGHBOUR's shared column is its i = 0 column, lanes j = 0..3
  function automatic logic [1:0] lvl_of(input logic [LVLW-1:0] w, input logic [3:0] n);
    lvl_of = w[{28'd0, n} * 2 +: 2];
  endfunction

  wire [7:0] row_j3_c = {lvl_of(rd_lvl, 4'hF), lvl_of(rd_lvl, 4'hE),
                         lvl_of(rd_lvl, 4'hD), lvl_of(rd_lvl, 4'hC)};
  wire [7:0] row_j0_c = {lvl_of(rd_lvl, 4'h3), lvl_of(rd_lvl, 4'h2),
                         lvl_of(rd_lvl, 4'h1), lvl_of(rd_lvl, 4'h0)};
  wire [7:0] col_i3_c = {lvl_of(rd_lvl, 4'hF), lvl_of(rd_lvl, 4'hB),
                         lvl_of(rd_lvl, 4'h7), lvl_of(rd_lvl, 4'h3)};
  wire [7:0] col_i0_c = {lvl_of(rd_lvl, 4'hC), lvl_of(rd_lvl, 4'h8),
                         lvl_of(rd_lvl, 4'h4), lvl_of(rd_lvl, 4'h0)};

  // ==========================================================================
  // address arbitration -- the sweep owns the port in PhSweep, the file path in
  // PhPrepare, the query walk in PhEmit. They cannot overlap.
  // ==========================================================================
  always_comb begin
    raddr_c = '0;
    waddr_c = sweep_idx_q;
    we_c    = 1'b0;
    wdata_c = '0;

    if (phase_q == PhSweep) begin
      we_c    = 1'b1;
      waddr_c = sweep_idx_q;
      wdata_c = '0;                       // valid = 0, and every other field
    end else if (phase_q == PhPrepare) begin
      if (fs_q == FsIdle) begin
        raddr_c = f_idx_c;
      end else begin
        waddr_c = f_idx_q;
        we_c    = 1'b1;
        // Read-modify-write, decided from the word that came back:
        //   an invalid record is REPLACED;
        //   a valid record with THIS tag is EXTENDED;
        //   a valid record with ANOTHER tag is POISONED.
        wdata_c = '0;
        if (f_hit_c) begin
          wdata_c[OFF_TAG  +: TAGW]  = rd_tag;
          wdata_c[OFF_LVL  +: LVLW]  = rd_lvl;
          wdata_c[OFF_LVL + ({28'd0, f_sub_q} * 2) +: 2] = f_lvl_q;
          wdata_c[OFF_MASK +: MASKW] = rd_mask | (16'd1 << f_sub_q);
          wdata_c[OFF_VALID]         = 1'b1;
          wdata_c[OFF_POIS]          = rd_pois;
        end else if (f_clash_c) begin
          // POISONED. The record keeps the bits it held; the poison flag is
          // what every reader looks at, the OWNER of the entry included.
          wdata_c[OFF_TAG  +: TAGW]  = rd_tag;
          wdata_c[OFF_LVL  +: LVLW]  = rd_lvl;
          wdata_c[OFF_MASK +: MASKW] = rd_mask;
          wdata_c[OFF_VALID]         = 1'b1;
          wdata_c[OFF_POIS]          = 1'b1;
        end else begin
          wdata_c[OFF_TAG  +: TAGW]  = f_tag_q;
          wdata_c[OFF_LVL + ({28'd0, f_sub_q} * 2) +: 2] = f_lvl_q;
          wdata_c[OFF_MASK +: MASKW] = (16'd1 << f_sub_q);
          wdata_c[OFF_VALID]         = 1'b1;
          wdata_c[OFF_POIS]          = 1'b0;
        end
      end
    end else begin
      // EMIT: reads only. The bank is frozen, which is the freeze the symmetry
      // law needs.
      raddr_c = (qs_q == QsIdle) ? {q_iz_i[IZW-1:0], q_ix_i[IXW-1:0]}
                                 : {qn_iz_c[IZW-1:0], qn_ix_c[IXW-1:0]};
    end
  end

  assign busy_o = (phase_q == PhSweep) || (fs_q != FsIdle) || (qs_q != QsIdle);

  // ==========================================================================
  // sequencing
  // ==========================================================================
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      phase_q            <= PhSweep;
      sweep_to_prepare_q <= 1'b0;
      sweep_idx_q        <= '0;
      frame_count_q      <= '0;
      fs_q               <= FsIdle;
      f_ix_q             <= '0;
      f_iz_q             <= '0;
      f_sub_q            <= '0;
      f_lvl_q            <= '0;
      f_idx_q            <= '0;
      qs_q               <= QsIdle;
      q_ix_q             <= '0;
      q_iz_q             <= '0;
      q_own_ok_q         <= 1'b0;
      q_nz_q             <= 8'h00;
      q_pz_q             <= 8'h00;
      q_nx_q             <= 8'h00;
      q_px_q             <= 8'h00;
      q_real_q           <= 4'h0;
      q_done_o           <= 1'b0;

      records_filed_o      <= '0;
      lanes_filed_o        <= '0;
      collisions_o         <= '0;
      queries_o            <= '0;
      edges_real_o         <= '0;
      edges_fallback_o     <= '0;
      query_own_missing_o  <= '0;
      file_out_of_phase_o  <= '0;
      query_out_of_phase_o <= '0;
    end else begin
      q_done_o <= 1'b0;

      // ---- out-of-phase offers, refused and counted -----------------------
      if (f_valid_i && (phase_q != PhPrepare) && (file_out_of_phase_o != 32'hFFFF_FFFF)) begin
        file_out_of_phase_o <= file_out_of_phase_o + 32'd1;
      end
      if (q_valid_i && (phase_q != PhEmit) && (query_out_of_phase_o != 32'hFFFF_FFFF)) begin
        query_out_of_phase_o <= query_out_of_phase_o + 32'd1;
      end

      // ---- the frame phase ------------------------------------------------
      // A RAM has no reset, so the valid bits are walked to zero -- at reset,
      // and again at every frame. `frame_begin_i` during a sweep RESTARTS it
      // rather than queueing: two begins with no work between them is one
      // frame's worth of sweeping, and the later one wins.
      if (frame_begin_i) begin
        phase_q            <= PhSweep;
        sweep_to_prepare_q <= 1'b1;
        sweep_idx_q        <= '0;
        fs_q               <= FsIdle;
        qs_q               <= QsIdle;
        frame_count_q      <= frame_count_q + 16'd1;
      end else if (phase_q == PhSweep) begin
        if (sweep_idx_q == {IDXW{1'b1}}) begin
          sweep_idx_q <= '0;
          phase_q     <= sweep_to_prepare_q ? PhPrepare : PhEmit;
        end else begin
          sweep_idx_q <= sweep_idx_q + 1'b1;
        end
      end else if (prepare_done_i && (phase_q == PhPrepare)) begin
        phase_q <= PhEmit;
        fs_q    <= FsIdle;
      end

      // ---- FILE -----------------------------------------------------------
      if (!frame_begin_i) begin
        case (fs_q)
          FsIdle: begin
            if (f_file_c) begin
              f_ix_q  <= f_ix_i;
              f_iz_q  <= f_iz_i;
              f_sub_q <= f_sub_c;
              f_lvl_q <= f_level_i;
              f_idx_q <= f_idx_c;
              fs_q    <= FsWrite;
            end
          end

          default: begin  // FsWrite -- the word is on `rd_c` and is written this cycle
            fs_q <= FsIdle;
            if (lanes_filed_o != 32'hFFFF_FFFF) lanes_filed_o <= lanes_filed_o + 32'd1;
            if (f_clash_c) begin
              // Counted ONCE per entry: the first clashing lane sets the
              // poison, the remaining fifteen find it already set.
              if (!rd_pois && (collisions_o != 32'hFFFF_FFFF)) begin
                collisions_o <= collisions_o + 32'd1;
              end
            end else if (f_hit_c && !rd_pois) begin
              // The record becomes COMPLETE on the beat that sets its last lane.
              if (((rd_mask | (16'd1 << f_sub_q)) == {MASKW{1'b1}})
                  && (rd_mask != {MASKW{1'b1}})) begin
                if (records_filed_o != 32'hFFFF_FFFF) begin
                  records_filed_o <= records_filed_o + 32'd1;
                end
              end
            end
          end
        endcase
      end

      // ---- QUERY ----------------------------------------------------------
      // Each state checks the word its PREDECESSOR asked for and asks for the
      // next, so the walk costs one clock per record plus the accept and the
      // publish: seven clocks.
      if (!frame_begin_i) begin
        case (qs_q)
          QsIdle: begin
            if ((phase_q == PhEmit) && q_valid_i) begin
              q_ix_q   <= q_ix_i;
              q_iz_q   <= q_iz_i;
              q_real_q <= 4'h0;
              q_nz_q   <= 8'h00;
              q_pz_q   <= 8'h00;
              q_nx_q   <= 8'h00;
              q_px_q   <= 8'h00;
              qs_q     <= QsOwn;
            end
          end

          QsOwn: begin
            q_own_ok_q <= rec_ok_c;
            qs_q       <= QsNz;
          end

          QsNz: begin
            if (q_own_ok_q && rec_ok_c) begin
              q_nz_q      <= row_j3_c;
              q_real_q[0] <= 1'b1;
            end
            qs_q <= QsPz;
          end

          QsPz: begin
            if (q_own_ok_q && rec_ok_c) begin
              q_pz_q      <= row_j0_c;
              q_real_q[1] <= 1'b1;
            end
            qs_q <= QsNx;
          end

          QsNx: begin
            if (q_own_ok_q && rec_ok_c) begin
              q_nx_q      <= col_i3_c;
              q_real_q[2] <= 1'b1;
            end
            qs_q <= QsPx;
          end

          QsPx: begin
            if (q_own_ok_q && rec_ok_c) begin
              q_px_q      <= col_i0_c;
              q_real_q[3] <= 1'b1;
            end
            qs_q <= QsDone;
          end

          default: begin  // QsDone -- publish, and count what was answered
            q_done_o <= 1'b1;
            qs_q     <= QsIdle;
            if (queries_o != 32'hFFFF_FFFF) queries_o <= queries_o + 32'd1;
            if (!q_own_ok_q && (query_own_missing_o != 32'hFFFF_FFFF)) begin
              query_own_missing_o <= query_own_missing_o + 32'd1;
            end
            // `edges_real_o` and `edges_fallback_o` are incremented on ONE
            // event from COMPLEMENTARY predicates over `q_real_q`, whose four
            // bits were written by four different states from four different
            // bank words. `edges_real_o + edges_fallback_o == 4 * queries_o`
            // is therefore an invariant a test can assert, and a fault in the
            // predicate moves one sum and not the other -- which is what this
            // repository's metadata-swap chapter says a detector must be able
            // to do.
            if (edges_real_o <= (32'hFFFF_FFFF - 32'd4)) begin
              edges_real_o <= edges_real_o
                            + {31'd0, q_real_q[0]} + {31'd0, q_real_q[1]}
                            + {31'd0, q_real_q[2]} + {31'd0, q_real_q[3]};
            end
            if (edges_fallback_o <= (32'hFFFF_FFFF - 32'd4)) begin
              edges_fallback_o <= edges_fallback_o
                                + {31'd0, ~q_real_q[0]} + {31'd0, ~q_real_q[1]}
                                + {31'd0, ~q_real_q[2]} + {31'd0, ~q_real_q[3]};
            end
          end
        endcase
      end
    end
  end

  // The published answer is the walk's registers, held between queries.
  assign edge_nz_o   = q_nz_q;
  assign edge_pz_o   = q_pz_q;
  assign edge_nx_o   = q_nx_q;
  assign edge_px_o   = q_px_q;
  assign edge_real_o = q_real_q;

endmodule

`default_nettype wire
