// zhao_measure_histogram.sv -- MEASURE.HISTOGRAM: the RAM-backed error-bucket
// histogram, with per-cycle same-bin aggregation, read-after-write forwarding,
// and double-banked epoch snapshots (phase 8, ZH-049).
//
// Law, in citation order:
//   design/contracts/MEASURE.HISTOGRAM.md -- the block contract. It is a
//       REFUSAL, not a specification: 186 lines arguing that the block should
//       not be built because four laws would have to be invented. Three of
//       those four are invented here, deliberately and in the open (see
//       "WHAT THIS FILE INVENTS"); the fourth is REFUSED, exactly as the
//       contract asked. Read the contract before changing anything here.
//   reports/MISSING-ORGAN-REGISTER-20260918.md, "OWNER REVOKES THE DEFERRALS"
//       -- "I don't want to defer any unfinished blocks now", Fabian,
//       2026-09-18. `deferred: true` is no longer a disposition, and this
//       block is listed "build now (was deferred)".
//   reports/Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt section 11.4
//       -- the architecture this file implements, quoted in full because
//       every sentence of it is a requirement:
//         "For a RAM-backed histogram, aggregate per-cycle same-bin events and
//          implement read-after-write forwarding or scheduled stalls. A
//          one-read/one-write port cannot accept arbitrary simultaneous
//          updates to many bins without extra hardware. Snapshots/clears need
//          epoch or double-bank semantics so a host read sees one complete
//          interval, not a mixture. Choose widths from the documented interval
//          and saturation/wrap policy, not arbitrary 32-bit counters
//          everywhere."
//       This file does BOTH halves of the first sentence (aggregation AND a
//       scheduled stall), BOTH halves of the second (epoch AND double bank),
//       and derives every width in "WIDTHS, DERIVED" below.
//   reports/OWNER-RULING-M10K-CEILINGS-20260918.md -- "Using some more M10K is
//       fine, we have enough, particularly if it saves ALMs." The bins are in
//       memory for that reason and the arithmetic is in "WHY RAM" below. Note
//       the ruling's own limit, honoured here: "Logical bits are still not
//       physical M10Ks" -- every memory figure in this header is SHAPE
//       ARITHMETIC, not a measurement. No fit has seen this block.
//   spec/counters.md section 4 -- "Internal registers may be narrower (u32
//       typical) but MUST saturate, never wrap, and a saturation is itself
//       visible". Every counter here saturates and the bin saturation is
//       visible on its own port.
//   ZHAOZHOU_CONSOLE_ENGINEERING_CHARTER.md section 9, "Practical
//       implementation path", Version 2 -- "FPGA builds a small histogram of
//       candidate error buckets; a cutoff bucket is selected; eligible
//       refinements above the cutoff are emitted." The FIRST clause is this
//       block. The second and third are NOT, and that is the whole of
//       "WHAT THIS FILE REFUSES".
//   fpga/rtl/measure/zhao_dc_sdp_ram.sv -- the ratified inferable memory
//       shape, instantiated rather than copied. It had NO instantiations in
//       the tree before this one.
//
// ---------------------------------------------------------------------------
// WHAT THIS FILE INVENTS, AND WHAT IT REFUSES
// ---------------------------------------------------------------------------
// The contract lists four inventions that building this block would require,
// and says a small block built on four invented laws is worse than no block.
// The owner has since revoked the deferral, so the block is built -- but the
// contract's argument is still correct about WHICH invention is dangerous, and
// the split below is the whole design decision of this file.
//
// INVENTED HERE (1) -- THE ERROR METRIC IS NOT NAMED, IT IS PARAMETERISED
// AWAY. The contract's first invention is "what number goes in a bucket", and
// it is right that no spec names one. So this block does not name one either.
// It takes an unsigned magnitude of EW bits and says nothing about what it
// measures. This is not evasion, it is the consequence of choice (2): with a
// logarithmic bucketing, a change of Q format is a CONSTANT SHIFT OF THE BIN
// INDEX and nothing else. Scaling every input by 2^k moves every event by
// k*2^SUB_BITS bins and changes the SHAPE of the histogram not at all. So the
// block is correct under any Q format the metric is eventually ratified with,
// and ratifying one does not reopen this file. The contract's "Q formats and
// rounding: UNDECIDED, and deliberately left so" survives intact.
//
// INVENTED HERE (2) -- THE BUCKET GEOMETRY IS LOG2 WITH SUB_BITS MANTISSA
// BITS. The contract's second invention is "how many buckets, linear or
// logarithmic, over what range". Chosen: logarithmic, because a screen-space
// error spans orders of magnitude and a linear histogram over the same range
// would put every useful observation in bin 0. The exact law is bin_of()
// below: exact bins for values under 2^SUB_BITS, then (exponent, mantissa)
// pairs. It is monotone non-decreasing in the input, which is the property
// that makes a cutoff SEARCH possible for whoever eventually writes one.
//   REJECTED: a linear histogram over a fixed range (needs a range, which is a
//   fifth invention, and quantises the interesting tail to nothing);
//   programmable bucket edges in a comparator chain (NBINS comparators of EW
//   bits each -- this is the block that is supposed to be REMOVING logic).
//
// INVENTED HERE (3) -- THE INTERVAL IS WHATEVER THE HOST SAYS IT IS.
// `snapshot_i` ends an interval and begins the next. The block does not know
// about frames. The plan says "choose widths from the DOCUMENTED interval";
// the interval documented here is one video frame, and it is documented by
// WIDTH rather than by a hard-wired frame tick, so a host that wants a
// per-half-frame or per-camera interval gets one for free.
//
// REFUSED -- THE CUTOFF RULE AND THE GOVERNOR PORT. The contract's inventions
// 3 and 4 ("a cutoff bucket is selected" -- selected how, against what budget,
// with what hysteresis; and how that cutoff reaches MEASURE.GOVERNOR). Its
// argument for refusing them is not weakened by the revocation and is quoted
// rather than paraphrased:
//
//     "The failure mode this project cares most about is two blocks
//      disagreeing about one policy ... Inventing a Version-2 cutoff in the
//      same increment that ratified a Version-1 governor would manufacture
//      that failure deliberately."
//
// So THIS BLOCK HAS NO GOVERNOR PORT and computes no cutoff. It is a
// measurement organ and the policy stays where charter section 9 Version 1
// already put it: "ARM predicts a pixel-error threshold per camera FROM PRIOR
// COUNTERS". This block IS the prior counters. That is a complete, useful,
// shippable job that invents no policy, and it is why the refusal costs
// nothing. `design/contracts/MEASURE.GOVERNOR.md` needs no amendment, and
// `blocks.yml`'s `downstream: [MEASURE.GOVERNOR]` edge is left UNBUILT and
// NAMED, which is exactly how MEASURE.GOVERNOR itself handled this same edge
// from the other side.
//
// ---------------------------------------------------------------------------
// THE DESIGN PROBLEM, WHICH IS THE PORT COUNT
// ---------------------------------------------------------------------------
// One read port and one write port. LANES events arrive per beat and each one
// wants a read-modify-write of a bin. The plan's sentence is the constraint:
// "A one-read/one-write port cannot accept arbitrary simultaneous updates to
// many bins without extra hardware." Three mechanisms, all present:
//
// H1. AGGREGATE SAME-BIN EVENTS. Every pending lane whose bin equals the
//     selected bin is folded into ONE read-modify-write carrying an increment
//     of up to LANES. Four events at one bin cost one memory update, not four.
//     This is the common case for a histogram -- neighbouring fragments have
//     similar error -- and it is directly visible: `updates_o` counts memory
//     updates while `events_o` counts events, and their ratio IS the
//     aggregation. A test that only checked the bin totals could not see it.
// H2. SCHEDULE A STALL FOR THE REST. Lanes whose bins differ retire on later
//     cycles, one group per cycle, with `ev_ready_o` held low until the last
//     group of the beat is issuing. So the beat costs (number of DISTINCT
//     bins) cycles: 1 in the best case, LANES in the worst. No event is ever
//     dropped and no update is ever lost -- backpressure absorbs it, which is
//     what `backpressure: ready_valid` in the ledger means.
// H3. FORWARD THE WRITE IN FLIGHT. The memory is read in cycle T and written
//     in cycle T+1. A group issued in cycle T+1 therefore reads the memory
//     BEFORE the write of the group ahead of it has landed, and back-to-back
//     updates of the same bin would lose one. `fwd_hit_acc_c` detects exactly
//     that address collision and substitutes the value being written.
//     The hazard is EXACTLY ONE CYCLE DEEP and that is a property of the
//     memory, not a hope: a read issued in cycle C sees every write committed
//     at the end of C-1 or earlier, so only the write issued in C itself can
//     be missed. Distance 2 needs nothing.
//     `fwd_hits_o` counts the substitutions, so the path is a MEASURED path
//     and not an argued one.
//
// ---------------------------------------------------------------------------
// SNAPSHOTS: DOUBLE BANK *AND* EPOCH, BECAUSE THEY SOLVE DIFFERENT HALVES
// ---------------------------------------------------------------------------
// The plan asks for "epoch or double-bank semantics so a host read sees one
// complete interval, not a mixture". Both are here because the "or" hides two
// distinct problems:
//
// S1. THE DOUBLE BANK is what lets the host read at all. Address bit AW-1 is
//     the bank. One bank accumulates, the other is frozen and is the only one
//     the host read port can address. Accumulation NEVER stops for a
//     snapshot, which is the point.
// S2. THE EPOCH is what makes the clear FREE. Each stored word is
//     {epoch, count}; a read whose stored epoch differs from the bank's
//     current epoch reads as ZERO. Swapping banks flips the newly-active
//     bank's epoch, which logically zeroes 2^BINW bins in ONE CYCLE.
//     REJECTED: scrubbing the newly-active bank after each swap. It is 2^BINW
//     cycles of held-off events at every interval boundary -- 64 cycles per
//     frame here -- for a bit that costs 2^AW bits of memory and two flops.
// S3. THE SWAP DRAINS FIRST. `snapshot_i` sets a request; the swap happens on
//     the first cycle with no accepted-but-unretired lanes, no update in
//     flight and no host read in flight, with `ev_ready_o` low in the
//     meantime. Without this an event accepted before the snapshot could land
//     in the bank the host is about to read -- "a mixture", precisely what
//     the plan forbids. The drain is at most 3 cycles.
// S4. A HOST THAT SNAPSHOTS WHILE STILL READING THE PREVIOUS INTERVAL LOSES
//     IT. The frozen bank becomes the active bank on the NEXT swap and its
//     epoch flips, so its contents read as zero from that moment. This is a
//     declared protocol limit, not a defect, and `snap_index_o` is how a host
//     detects that it happened: read the index, read the bins, read the index
//     again.
//     REJECTED: triple banking. It is +50% memory and the host still has to
//     sequence its own reads, so it buys ordering the host already owns.
//
// ---------------------------------------------------------------------------
// THE ONE-TIME SCRUB, AND WHY IT IS NOT OPTIONAL
// ---------------------------------------------------------------------------
// The epoch trick compares a STORED bit against a live one. After reset the
// stored bits are whatever the memory powers up holding, and an accidental
// match would admit an arbitrary count into a live bin. So the block walks all
// 2^AW addresses once after reset, writing {epoch 0, count 0}, with
// `ev_ready_o` and `rd_ready_o` low throughout. 2^AW cycles, once, ever.
// This is the ONLY clear that costs cycles; every interval clear afterwards is
// the epoch flip and costs none.
//
// ---------------------------------------------------------------------------
// WIDTHS, DERIVED -- the plan's fourth sentence
// ---------------------------------------------------------------------------
// "Choose widths from the documented interval and saturation/wrap policy, not
// arbitrary 32-bit counters everywhere."
//
//   THE INTERVAL is one video frame at 60 Hz.
//   THE EVENT SOURCE is RASTER.FRAGMENT, whose ledger row states "1 accepted
//   fast-path fragment per clock". So the interval cannot contain more events
//   than it contains gpu clocks.
//   AT 100 MHz that is 1,666,666 events per frame, which needs 21 bits.
//   CW = 24 is that bound rounded up to a byte boundary, which leaves a
//   factor of 10 -- ten frames of accumulation, or a 600 MHz clock, before a
//   bin can saturate. It is NOT 32 because 32 would be 8 bits per bin of
//   memory bought for nothing, across every bin, in both banks.
//   EVERY COUNTER ON THIS BLOCK IS ALSO CW WIDE, for the same reason: they all
//   count events, updates or cycles within one interval, and all three are
//   bounded by the same clocks-per-frame number. `snapshots_o` and
//   `snap_index_o` count intervals, at most 60 per second, so 24 bits is 194
//   days of continuous play.
//   SATURATION, NEVER WRAP, per spec/counters.md section 4 -- and a saturated
//   bin is visible on `bin_sat_o` rather than silently wrong. Note the
//   consequence, which is deliberate: after a saturation `snap_total_o` no
//   longer equals the sum of the bins, because the total counts events
//   ACCEPTED and the bins count events STORED. `bin_sat_o` non-zero is how a
//   host knows to stop trusting the sum.
//
// ---------------------------------------------------------------------------
// WHY RAM -- SHAPE ARITHMETIC, EXPLICITLY NOT A MEASUREMENT
// ---------------------------------------------------------------------------
// At the defaults the storage is 2^AW = 128 words of CW+1 = 25 bits:
// 3,200 logical bits. That is under one M10K (10,240 bits), so the shape
// arithmetic says ONE. Whether Quartus infers an M10K at this depth, or uses
// MLABs, or spreads it into logic, is NOT KNOWN AND NOT CLAIMED -- nothing has
// been fitted. The owner ruling's third limit says this in terms: "Logical
// bits are still not physical M10Ks ... only a fit reports what it cost."
//
// What the memory REPLACES is the number that matters, and it is also an
// estimate. The flip-flop form of this block is 2 banks x 2^BINW bins x CW
// bits = 3,072 registers, plus a 2^BINW-way read mux and a LANES-way update
// crossbar on top. At the Cyclone V packing limit of four registers per ALM
// that is 768 ALMs of pure storage before any of the muxing, so the realistic
// figure is four figures. Against a campaign that is ~15x over its ALM budget,
// that is the entire reason this block is shaped the way it is. BOTH numbers
// are arithmetic; the fit is the only thing that can settle either.
//
// ---------------------------------------------------------------------------
// COUNTERS, AND THE ONE THAT MUST READ ZERO
// ---------------------------------------------------------------------------
// Six counters are expected to move and each has a directed case that moves
// it as a delta. The seventh is different:
//
//   `frozen_write_o` counts memory updates aimed at the bank the host owns.
//   It is UNREACHABLE while S3's drain is correct -- no legal stimulus can
//   move it -- so it is the wq_overflow_o case from CLAUDE.md, and it gets a
//   COMMITTED MUTANT rather than an argument:
//   tests/mutants/zhao_measure_histogram_drain_mutant.sv deletes the drain and
//   the mutant driver PASSES WHEN THE COUNTER FIRES.
//
//   It is also built to survive CLAUDE.md's "a detector wired to two operands
//   that move together cannot fire". Its two operands are `b_bank_q`, loaded
//   by the ISSUE enable, and `active_q`, loaded by the SWAP enable. They are
//   different enables driven by different events, so the comparison can see a
//   timing fault and not merely a value fault. That is the property that the
//   metadata-bank generation check did not have.
//
// LEDGER DEVIATION -- `counters: [lod_representation_counts]`. This block does
// NOT drive that catalog entry, and the contract's own note is why: the entry
// already has two owners with different readings (TERRAIN.LOD counts subpatch
// levels, MEASURE.TOKENS counts charter ladder rungs), and nothing this block
// counts is honestly a REPRESENTATION count -- it counts error magnitudes.
// The contract offers exactly two options, "a reading of its own that is
// honestly a representation count, or ... record a ledger deviation instead of
// driving it". The second is taken, here, in writing.
//
// ---------------------------------------------------------------------------
// WHAT HAS NOT BEEN SHOWN
// ---------------------------------------------------------------------------
// This block has never been through quartus_map. Verilator lint-clean is one
// tool's opinion about syntax and width, and CLAUDE.md records two
// SystemVerilog forms that linted at 0 diagnostics and failed Quartus 17.0
// outright. The two forms named there are avoided on purpose -- the
// elaboration checks are inside `initial begin ... end`, and there is no
// implicit generate anywhere in the file -- but avoiding two known traps is
// not evidence of synthesizability. IT HAS NOT BEEN SHOWN TO BE
// SYNTHESIZABLE, and no area, Fmax or RAM-inference claim here is measured.
module zhao_measure_histogram #(
    // Width of the error magnitude. Unsigned. The block does not know or care
    // what it measures; see "WHAT THIS FILE INVENTS" (1).
    parameter int unsigned EW = 32,
    // Mantissa bits kept below the exponent. 0 = pure octaves; 1 = half
    // octaves (the default); 2 = quarter octaves. Bins double with each bit.
    parameter int unsigned SUB_BITS = 1,
    // Events offered per beat.
    parameter int unsigned LANES = 4,
    // Bits per bin AND per counter. Derived in "WIDTHS, DERIVED".
    parameter int unsigned CW = 24,
    // DERIVED. It is a parameter and not a localparam for one reason only:
    // `rd_bin_i` needs it in the port list, and a module-body localparam is not
    // visible there. DO NOT OVERRIDE IT -- the elaboration check below refuses
    // any value other than the one this expression produces.
    parameter int unsigned BINW = $clog2((EW - SUB_BITS + 1) << SUB_BITS)
) (
    input logic clk,
    input logic rst_n,

    // ---- event ingress: one beat of up to LANES events ---------------------
    input  logic               ev_valid_i,
    input  logic [  LANES-1:0] ev_lane_valid_i,
    input  logic [LANES*EW-1:0] ev_err_i,      // lane l occupies [l*EW +: EW]
    input  logic [       15:0] ev_src_id_i,    // `source_ids: true`
    output logic               ev_ready_o,

    // ---- interval control --------------------------------------------------
    // One-cycle pulse. Ends the current interval and begins the next. The swap
    // is DEFERRED until the pipeline drains (law S3); a second pulse arriving
    // before the first swap is absorbed into the pending request.
    input logic snapshot_i,

    // ---- host read of the FROZEN bank -------------------------------------
    // Two-cycle registered read. `rd_ready_o` is low only during the one-time
    // post-reset scrub. The read port has PRIORITY over accumulation, which is
    // what makes "an event hitting a bin that is being read back" a stall
    // rather than a race; `host_conflict_o` counts the stalls. A host that
    // holds `rd_valid_i` high forever starves the accumulator and blocks the
    // snapshot drain: that is the host's fault, not the block's, and
    // `host_conflict_o` is where it is visible.
    input  logic            rd_valid_i,
    input  logic [BINW-1:0] rd_bin_i,
    output logic            rd_ready_o,
    output logic            rd_data_valid_o,
    output logic [  CW-1:0] rd_count_o,

    // ---- frozen-interval summary ------------------------------------------
    output logic          snap_valid_o,   // an interval has been frozen
    output logic [CW-1:0] snap_total_o,   // events ACCEPTED into it
    output logic [  15:0] snap_src_id_o,  // src_id of its last accepted event
    output logic [CW-1:0] snap_index_o,   // interval sequence number

    // ---- counters (spec/counters.md section 4: saturate, never wrap) -------
    output logic [CW-1:0] events_o,         // events accepted
    output logic [CW-1:0] updates_o,        // memory read-modify-writes
    output logic [CW-1:0] stall_cycles_o,   // cycles offered and refused
    output logic [CW-1:0] bin_sat_o,        // updates that saturated a bin
    output logic [CW-1:0] fwd_hits_o,       // read-after-write substitutions
    output logic [CW-1:0] host_conflict_o,  // host reads that held off a group
    output logic [CW-1:0] snapshots_o,      // completed bank swaps
    // Structurally unreachable while law S3 holds. See "COUNTERS, AND THE ONE
    // THAT MUST READ ZERO".
    output logic [CW-1:0] frozen_write_o
);

  // ---------------------------------------------------------------------------
  // Geometry
  // ---------------------------------------------------------------------------
  // Highest bin the law below can produce is ((EW-SUB_BITS)<<SUB_BITS)|MASK,
  // one less than NBINS. BINW rounds that up to an address width; the bins
  // between NBINS and 2^BINW exist in memory, are never addressed by bin_of()
  // and are scrubbed like any other word.
  localparam int unsigned NBINS = (EW - SUB_BITS + 1) << SUB_BITS;
  localparam int unsigned AW = BINW + 1;          // {bank, bin}
  localparam int unsigned DW = CW + 1;            // {epoch, count}
  localparam int unsigned INCW = $clog2(LANES + 1);
  localparam int unsigned MANT_MASK = (1 << SUB_BITS) - 1;
  localparam logic [CW-1:0] CNT_MAX = {CW{1'b1}};

  // Quartus 17.0 will not accept a bare module-scope elaboration `if`; it has
  // to be inside `initial begin ... end` (CLAUDE.md, 2026-09-08). Note also
  // that `--lint-only` does not run this block, so a clean lint says nothing
  // whatever about these guards.
  initial begin
    if (EW < 2 || EW > 32)
      $fatal(1, "zhao_measure_histogram: EW must be 2..32 (bin_of is 32-bit); got %0d", EW);
    if (SUB_BITS >= EW)
      $fatal(1, "zhao_measure_histogram: SUB_BITS must be < EW; got %0d >= %0d", SUB_BITS, EW);
    if (LANES < 1)
      $fatal(1, "zhao_measure_histogram: LANES must be >= 1; got %0d", LANES);
    if (CW < 4 || CW > 31)
      $fatal(1, "zhao_measure_histogram: CW must be 4..31; got %0d", CW);
    if ((1 << CW) <= LANES)
      $fatal(1, "zhao_measure_histogram: CW too small to hold one beat (%0d lanes)", LANES);
    // The counter adders zero-extend an INCW-bit increment to CW bits, which
    // needs at least one pad bit; a zero-width replication is not legal.
    if (CW <= INCW)
      $fatal(1, "zhao_measure_histogram: CW (%0d) must exceed INCW (%0d)", CW, INCW);
    // BINW is derived, never chosen. See its declaration.
    if (BINW != $clog2(NBINS))
      $fatal(1, "zhao_measure_histogram: BINW is DERIVED, not a knob; expected %0d got %0d",
             $clog2(NBINS), BINW);
  end

  // ---------------------------------------------------------------------------
  // The bucket law (invention 2). Monotone non-decreasing in `v`.
  //
  //   v < 2^SUB_BITS            -> bin = v                       (exact)
  //   otherwise, e = MSB index  -> bin = ((e-SUB_BITS+1)<<SUB_BITS)
  //                                      + (mantissa below e)
  //
  // With SUB_BITS = 1: {0}->0, {1}->1, {2,3}->2,3, {4,5}->4, {6,7}->5,
  // {8..11}->6, {12..15}->7, ... Multiplying every input by 2^k adds
  // k*2^SUB_BITS to every bin and changes nothing else, which is the property
  // that lets the metric's Q format stay undecided.
  // ---------------------------------------------------------------------------
  function automatic logic [BINW-1:0] bin_of(input logic [EW-1:0] v);
    int unsigned vv;
    int unsigned e;
    logic [BINW-1:0] r;
    bit found;
    begin
      vv = 32'(v);
      e  = 0;
      found = 1'b0;
      for (int i = int'(EW) - 1; i >= 0; i--) begin
        if (!found && v[i]) begin
          e = unsigned'(i);
          found = 1'b1;
        end
      end
      if (!found) r = '0;
      else if (e < SUB_BITS) r = BINW'(vv);
      else r = BINW'(((e - SUB_BITS + 1) << SUB_BITS) + ((vv >> (e - SUB_BITS)) & MANT_MASK));
      bin_of = r;
    end
  endfunction

  function automatic logic [CW-1:0] sat_inc(input logic [CW-1:0] c, input logic [CW-1:0] d);
    logic [CW:0] s;
    begin
      s = {1'b0, c} + {1'b0, d};
      sat_inc = s[CW] ? CNT_MAX : s[CW-1:0];
    end
  endfunction

  // ---------------------------------------------------------------------------
  // Post-reset scrub (see "THE ONE-TIME SCRUB")
  // ---------------------------------------------------------------------------
  logic          init_q;
  logic [AW-1:0] init_addr_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      init_q      <= 1'b1;
      init_addr_q <= '0;
    end else if (init_q) begin
      init_addr_q <= init_addr_q + AW'(1);
      if (init_addr_q == {AW{1'b1}}) init_q <= 1'b0;
    end
  end

  // ---------------------------------------------------------------------------
  // Bank / epoch state
  // ---------------------------------------------------------------------------
  logic        active_q;   // bank currently accumulating
  logic [1:0]  epoch_q;    // per-bank live epoch bit
  logic        frozen_c;
  assign frozen_c = ~active_q;

  // ---------------------------------------------------------------------------
  // Ingress holding register and the group selector (laws H1, H2)
  // ---------------------------------------------------------------------------
  logic [LANES-1:0] pend_q;
  logic [ BINW-1:0] bin_q      [LANES];
  logic [     15:0] src_q;

  logic [BINW-1:0] in_bin_c[LANES];
  always_comb begin
    for (int l = 0; l < int'(LANES); l++) in_bin_c[l] = bin_of(ev_err_i[l*EW+:EW]);
  end

  logic             snap_req_q;
  logic             rd_accept_c;
  logic             issue_c;
  logic [LANES-1:0] grp_mask_c;
  logic [ BINW-1:0] sel_bin_c;
  logic [ INCW-1:0] grp_cnt_c;
  logic [LANES-1:0] pend_next_c;
  logic             accept_c;

  assign rd_accept_c = rd_valid_i && !init_q;
  assign rd_ready_o  = !init_q;

  always_comb begin
    sel_bin_c  = '0;
    grp_mask_c = '0;
    grp_cnt_c  = '0;
    issue_c    = (pend_q != '0) && !rd_accept_c && !init_q;
    if (issue_c) begin
      // Descending sweep leaves sel_bin_c holding the LOWEST pending lane's
      // bin, which makes group selection deterministic and order-independent.
      for (int l = int'(LANES) - 1; l >= 0; l--) begin
        if (pend_q[l]) sel_bin_c = bin_q[l];
      end
      for (int l = 0; l < int'(LANES); l++) begin
        if (pend_q[l] && (bin_q[l] == sel_bin_c)) begin
          grp_mask_c[l] = 1'b1;
          grp_cnt_c     = grp_cnt_c + INCW'(1);
        end
      end
    end
    pend_next_c = pend_q & ~grp_mask_c;
  end

  // Ready when the beat currently held will be empty at the next edge. That is
  // true immediately when nothing is held, and on the cycle the last group of
  // a beat issues -- so a beat whose events all share a bin costs one cycle.
  assign ev_ready_o = !init_q && !snap_req_q && (pend_next_c == '0);
  assign accept_c   = ev_valid_i && ev_ready_o;

  logic [INCW-1:0] accept_cnt_c;
  always_comb begin
    accept_cnt_c = '0;
    for (int l = 0; l < int'(LANES); l++) begin
      if (accept_c && ev_lane_valid_i[l]) accept_cnt_c = accept_cnt_c + INCW'(1);
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      pend_q <= '0;
      src_q  <= '0;
      for (int l = 0; l < int'(LANES); l++) bin_q[l] <= '0;
    end else if (accept_c) begin
      pend_q <= ev_lane_valid_i;
      src_q  <= ev_src_id_i;
      for (int l = 0; l < int'(LANES); l++) bin_q[l] <= in_bin_c[l];
    end else begin
      pend_q <= pend_next_c;
    end
  end

  // ---------------------------------------------------------------------------
  // Memory stage A -> B. `issue_c` and `rd_accept_c` are mutually exclusive by
  // construction -- `issue_c`'s own term contains `!rd_accept_c` -- so the
  // single read port never has two claimants. The host wins and the
  // accumulator stalls, which is what makes a read of a bin being updated a
  // scheduling question rather than a race.
  // ENFORCED-BY: tests/measure/measure_histogram_directed.cpp:test_read_during_update
  // ---------------------------------------------------------------------------
  logic            b_v_q;
  logic [BINW-1:0] b_bin_q;
  logic            b_bank_q;
  logic [INCW-1:0] b_inc_q;

  logic            h_v_q;
  logic [BINW-1:0] h_bin_q;
  logic            h_bank_q;

  logic [AW-1:0]   ram_rd_addr_c;
  assign ram_rd_addr_c = rd_accept_c ? {frozen_c, rd_bin_i} : {active_q, sel_bin_c};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      b_v_q   <= 1'b0;
      b_bin_q <= '0;
      b_bank_q<= 1'b0;
      b_inc_q <= '0;
      h_v_q   <= 1'b0;
      h_bin_q <= '0;
      h_bank_q<= 1'b0;
    end else begin
      b_v_q    <= issue_c;
      b_bin_q  <= sel_bin_c;
      b_bank_q <= active_q;
      b_inc_q  <= grp_cnt_c;
      h_v_q    <= rd_accept_c;
      h_bin_q  <= rd_bin_i;
      h_bank_q <= frozen_c;
    end
  end

  // ---------------------------------------------------------------------------
  // The memory. zhao_dc_sdp_ram is the ratified inferable shape (its own header
  // lists the five blocks that paid for it); both clocks are tied to `clk`.
  //
  // ITS PROTOCOL OBLIGATION, DISCHARGED: that module says a same-address
  // read-during-write is outside its protocol and the user "must make
  // same-address collision UNREACHABLE by ownership". Here the collision is
  // reachable -- it is precisely law H3's hazard -- and it is discharged the
  // other way: the colliding read's RESULT IS NEVER CONSUMED. `fwd_hit_acc_c`
  // is true on exactly those cycles and the forwarded write data is used
  // instead, so whatever the M10K presents on its read port is discarded. The
  // stored data is never at risk; only the read output is, and nothing reads
  // it.
  // ---------------------------------------------------------------------------
  logic          ram_wr_en_c;
  logic [AW-1:0] ram_wr_addr_c;
  logic [DW-1:0] ram_wr_data_c;
  logic [DW-1:0] ram_rd_data;

  zhao_dc_sdp_ram #(
      .DATA_W(DW),
      .ADDR_W(AW)
  ) u_bins (
      .wr_clk (clk),
      .wr_en  (ram_wr_en_c),
      .wr_addr(ram_wr_addr_c),
      .wr_data(ram_wr_data_c),
      .rd_clk (clk),
      .rd_en  (1'b1),
      .rd_addr(ram_rd_addr_c),
      .rd_data(ram_rd_data)
  );

  // ---------------------------------------------------------------------------
  // Stage B: epoch check, forwarding (law H3), saturating add
  // ---------------------------------------------------------------------------
  logic          w_v_q;
  logic [AW-1:0] w_addr_q;
  logic [CW-1:0] w_cnt_q;

  logic          raw_ep_c;
  logic [CW-1:0] raw_cnt_c;
  assign raw_ep_c  = ram_rd_data[CW];
  assign raw_cnt_c = ram_rd_data[CW-1:0];

  logic [AW-1:0] b_addr_c;
  logic [AW-1:0] h_addr_c;
  assign b_addr_c = {b_bank_q, b_bin_q};
  assign h_addr_c = {h_bank_q, h_bin_q};

  logic          fwd_hit_acc_c;
  logic          fwd_hit_host_c;
  assign fwd_hit_acc_c  = w_v_q && (w_addr_q == b_addr_c);
  assign fwd_hit_host_c = w_v_q && (w_addr_q == h_addr_c);

  logic [CW-1:0] acc_cur_c;
  logic [CW-1:0] host_cur_c;
  always_comb begin
    if (fwd_hit_acc_c) acc_cur_c = w_cnt_q;
    else if (raw_ep_c == epoch_q[b_bank_q]) acc_cur_c = raw_cnt_c;
    else acc_cur_c = '0;

    // The host reads the frozen bank and the accumulator writes the active
    // one, so this forward CANNOT hit today. It is written anyway because the
    // alternative is a correctness argument that depends on an invariant three
    // laws away, and because it costs one comparator.
    if (fwd_hit_host_c) host_cur_c = w_cnt_q;
    else if (raw_ep_c == epoch_q[h_bank_q]) host_cur_c = raw_cnt_c;
    else host_cur_c = '0;
  end

  logic [CW:0]   acc_sum_c;
  logic          acc_sat_c;
  logic [CW-1:0] acc_new_c;
  always_comb begin
    acc_sum_c = {1'b0, acc_cur_c} + {{(CW + 1 - INCW) {1'b0}}, b_inc_q};
    acc_sat_c = acc_sum_c[CW];
    acc_new_c = acc_sat_c ? CNT_MAX : acc_sum_c[CW-1:0];
  end

  assign ram_wr_en_c   = init_q ? 1'b1 : b_v_q;
  assign ram_wr_addr_c = init_q ? init_addr_q : b_addr_c;
  assign ram_wr_data_c = init_q ? {DW{1'b0}} : {epoch_q[b_bank_q], acc_new_c};

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      w_v_q    <= 1'b0;
      w_addr_q <= '0;
      w_cnt_q  <= '0;
    end else begin
      w_v_q    <= b_v_q;           // scrub writes are deliberately excluded:
      w_addr_q <= b_addr_c;        // nothing can forward from them, because
      w_cnt_q  <= acc_new_c;       // no group issues while init_q is high.
    end
  end

  // ---------------------------------------------------------------------------
  // Host read output (two-cycle registered read)
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rd_data_valid_o <= 1'b0;
      rd_count_o      <= '0;
    end else begin
      rd_data_valid_o <= h_v_q;
      rd_count_o      <= host_cur_c;
    end
  end

  // ---------------------------------------------------------------------------
  // Interval totals, the snapshot request and the drained swap (laws S1..S3)
  // ---------------------------------------------------------------------------
  logic [CW-1:0] tot_q[2];
  logic          drain_c;
  logic          swap_c;

  assign drain_c = (pend_q == '0) && !b_v_q && !h_v_q && !rd_accept_c;
  assign swap_c  = snap_req_q && drain_c && !init_q;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      active_q      <= 1'b0;
      epoch_q       <= 2'b00;
      snap_req_q    <= 1'b0;
      tot_q[0]      <= '0;
      tot_q[1]      <= '0;
      snap_valid_o  <= 1'b0;
      snap_total_o  <= '0;
      snap_src_id_o <= '0;
      snap_index_o  <= '0;
    end else begin
      if (snapshot_i) snap_req_q <= 1'b1;

      if (b_v_q) tot_q[b_bank_q] <= sat_inc(tot_q[b_bank_q], {{(CW - INCW) {1'b0}}, b_inc_q});

      if (swap_c) begin
        active_q            <= ~active_q;
        epoch_q[frozen_c]   <= ~epoch_q[frozen_c];  // the bank about to go live
        tot_q[frozen_c]     <= '0;
        snap_req_q          <= 1'b0;
        snap_valid_o        <= 1'b1;
        snap_total_o        <= tot_q[active_q];
        snap_src_id_o       <= src_q;
        snap_index_o        <= sat_inc(snap_index_o, CW'(1));
      end
    end
  end

  // ---------------------------------------------------------------------------
  // Counters. spec/counters.md section 4: saturate, never wrap.
  // ---------------------------------------------------------------------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      events_o        <= '0;
      updates_o       <= '0;
      stall_cycles_o  <= '0;
      bin_sat_o       <= '0;
      fwd_hits_o      <= '0;
      host_conflict_o <= '0;
      snapshots_o     <= '0;
      frozen_write_o  <= '0;
    end else begin
      if (accept_c) events_o <= sat_inc(events_o, {{(CW - INCW) {1'b0}}, accept_cnt_c});
      if (issue_c) updates_o <= sat_inc(updates_o, CW'(1));
      if (ev_valid_i && !ev_ready_o) stall_cycles_o <= sat_inc(stall_cycles_o, CW'(1));
      if (b_v_q && acc_sat_c) bin_sat_o <= sat_inc(bin_sat_o, CW'(1));
      if (b_v_q && fwd_hit_acc_c) fwd_hits_o <= sat_inc(fwd_hits_o, CW'(1));
      if (rd_accept_c && (pend_q != '0)) host_conflict_o <= sat_inc(host_conflict_o, CW'(1));
      if (swap_c) snapshots_o <= sat_inc(snapshots_o, CW'(1));
      // The guard. b_bank_q is loaded by the ISSUE enable and active_q by the
      // SWAP enable, so this difference is not blind to timing faults.
      if (b_v_q && (b_bank_q != active_q)) frozen_write_o <= sat_inc(frozen_write_o, CW'(1));
    end
  end

endmodule : zhao_measure_histogram
