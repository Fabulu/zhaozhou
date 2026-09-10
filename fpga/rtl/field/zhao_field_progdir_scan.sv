// zhao_field_progdir_scan.sv -- FIELD program directory as a SCANNED MEMORY.
//
// Candidate replacement for the residency half of zhao_field_progcache.sv
// (Zhaozhou_ALM_Liberation_Roadmap_2026-09-09 section 5). The old block is
// RETAINED as the transaction oracle; zhao_field_progdir.sv is the adapter
// that presents this scanner behind the old two-interface port list.
//
// ---------------------------------------------------------------------------
// WHAT CHANGES, AND WHAT DOES NOT
// ---------------------------------------------------------------------------
// The old directory keeps ENTRIES x {hash, stamp} in FLIP-FLOPS and searches
// them in parallel: sixteen 32-bit equality comparators, a sixteen-way 48-bit
// minimum tree and an 80-bit-wide 16:1 selection. Quartus 17.0.2 Analysis &
// Synthesis put that at 2,237 estimated ALMs / 1,490 registers (map-only,
// reports/synthesis/zhao_block_map.json, sourceCommit 7481711 -- the file is
// byte-identical today). Its own header argued a scan "would cost sixteen
// cycles to save almost nothing"; the map row is what "almost nothing" turned
// out to be.
//
// Here the payload rows live in ONE synchronous memory of ENTRIES x ROWW bits
// (16 x 80 at the defaults), read one row per clock, and the whole directory
// has ONE hash comparator and ONE stamp comparator. Only the ENTRIES valid
// bits, the LRU counter, the counters and the scan bookkeeping are flops.
//
// The transaction LAWS do not change. They are the retained oracle's, verbatim:
//
//   lookup hit      : restamp the hit, hits++.
//   lookup miss     : no miss count yet (a miss that becomes a rejection was
//                     never a miss).
//   valid commit    : first free row, else the oldest stamp, LOWEST index on a
//                     tie; misses++; evictions++ if a live row was displaced,
//                     else occupancy++.
//   rejected commit : NO cache mutation; saturating programs_rejected++ only.
//   counters        : hits/misses/evictions wrap at 32 bits; programs_rejected
//                     saturates; the LRUW-bit stamp counter wraps and the
//                     comparison stays a plain unsigned '<', so after a wrap the
//                     youngest row LOOKS oldest -- exactly as the old block.
//
// What DOES change is latency: a transaction is a scan, not a cycle.
// ENFORCED-BY: tests/field/field_progdir_differential.cpp (transaction-level
// differential against the retained zhao_field_progcache, both elaborated
// side by side in tests/field/tb_field_progdir_diff.sv; it also MEASURES the
// cycle counts quoted in reports/FIELD-PROGDIR-20260910.md rather than
// inheriting the roadmap's estimate).
//
// ---------------------------------------------------------------------------
// THE UNIFIED REQUEST
// ---------------------------------------------------------------------------
// One request port carries the kind (lookup / commit), the hash and, for a
// commit, the decode verdict. One response port echoes the KIND with the
// result, so the adapter routes by kind and can never mistake a lookup miss
// (hit=0) for a rejected commit (inserted=0) -- the hazard section 5 names.
// The scanner holds exactly one transaction at a time: accepted in S_IDLE,
// answered from S_RESP, and it does not accept again until the response has
// been taken.
//
// ---------------------------------------------------------------------------
// MEMORY SHEET (roadmap section 3.1 requires one beside every memory)
// ---------------------------------------------------------------------------
//   owner        this module, exclusively
//   entry        {stamp[LRUW-1:0], hash[HASHW-1:0]}  = ROWW bits (80 default)
//   depth        ENTRIES (16 default)
//   ports        1 read (every clock, address = scan index i), 1 write
//                (S_RESP's first cycle; address registered in S_DECIDE, data
//                {lru_ctr, q_hash} straight from the registers that hold it)
//   collision    NONE POSSIBLE: the write happens only in S_RESP, reads are
//                consumed only in S_SCAN, and S_RESP -> S_IDLE -> S_SCAN puts at
//                least two edges between them. Simulation asserts it below.
//                (Same-address read/write would return OLD data on an M10K;
//                the block does not rely on that because it never occurs.)
//   init         none. Payload is NEVER reset (no reset loop over the array);
//                the valid_q bits are reset and an invalid row is never
//                consulted as contents -- every use of rd_q is gated on
//                valid_q[j]. Uninitialised RAM can therefore not alias a hit.
//   partial write none: every write is a whole row.
//   expected     2 x M10K at 16 x 80 -- an M10K port is at most 40 bits wide
//                (256 x 40), so 80 bits needs two blocks side by side, each
//                using 16 of 256 rows (6.25 %). An MLAB layout (4 x 32x20)
//                is the one-attribute alternative; the fit gate F-PROGDIR1
//                in design/fit_targets.yml is what decides between them.
//                This is a STRUCTURAL PREDICTION until mapped.
//
// ---------------------------------------------------------------------------
// TIMELINE, derived (the differential measures it; see the report)
// ---------------------------------------------------------------------------
//   E0    accept in S_IDLE; i <= 0
//   c1    read row 0 issued (rd_addr = i)          -- first S_SCAN cycle
//   c2    evaluate row 0 (j = 0), read row 1 issued
//   ...
//   cN+1  evaluate row N-1 -> S_DECIDE
//   cN+2  S_DECIDE: victim / hit chosen, counters, write address, resp set
//   cN+3  S_RESP: resp_valid_o visible; the row write lands at this cycle's edge
//   cN+4  S_IDLE: can accept
// so, counting the accepting edge as edge 0, resp_valid_o is high after edge
// N+2 and can be TAKEN at edge N+3; the no-stall accept-to-accept interval is
// N+4 clocks. Behind the adapter (zhao_field_progdir), whose response channel
// is one more register, the old ports see the answer takeable at edge N+4 --
// MEASURED 20 clocks at ENTRIES=16 for lookup hit, lookup miss and valid
// commit alike, and 20 clocks accept-to-accept. A rejected commit skips the
// scan: measured 3 and 3. (tests/field/field_progdir_differential.cpp, the
// timing probe; the oracle measures 1 and 1 on the same probe.)
// The scan is one pipeline stage deep (issue row i, evaluate row j = i-1),
// which is the zhao_geom_pose_cache idiom, not the roadmap's two-clocks-per-
// row first candidate; its ~35 estimate described that other shape and is
// not quoted for this one.
//
// The scan always walks every row. An early exit on a hit would save clocks
// on the hit path and make the latency depend on the contents; the fixed walk
// is chosen so one transaction has one length. That is a decision, not a law.
module zhao_field_progdir_scan #(
    parameter int ENTRIES = 16,
    parameter int HASHW   = 32,
    parameter int LRUW    = 48
) (
    input  logic clk,
    input  logic rst_n,

    // ---- unified request -------------------------------------------------
    input  logic             req_valid_i,
    output logic             req_ready_o,
    input  logic             req_commit_i,   // 0 = lookup, 1 = commit
    input  logic [HASHW-1:0] req_hash_i,
    input  logic             req_ok_i,       // commit only: the decode verdict

    // ---- one response per request, kind echoed ---------------------------
    output logic                       resp_valid_o,
    input  logic                       resp_ready_i,
    output logic                       resp_commit_o,
    output logic                       resp_hit_o,       // lookup
    output logic                       resp_inserted_o,  // commit: 0 = rejected
    output logic                       resp_evicted_o,   // commit: a live row went
    output logic [$clog2(ENTRIES)-1:0] resp_slot_o,

    // ---- counters, the old block's set -----------------------------------
    output logic [31:0] hits_o,
    output logic [31:0] misses_o,
    output logic [31:0] programs_rejected_o,
    output logic [31:0] evictions_o,
    output logic [$clog2(ENTRIES):0] occupancy_o
);

  localparam int IDXW = $clog2(ENTRIES);
  localparam int ROWW = HASHW + LRUW;
  localparam int LRU_LO = HASHW;   // row layout: [HASHW-1:0] hash, [ROWW-1:HASHW] stamp

  // Quartus 17.0 needs an elaboration check inside `initial begin ... end`
  // (CLAUDE.md Build note); Verilator --lint-only does NOT run it.
  initial begin
    if (ENTRIES < 2) $fatal(1, "zhao_field_progdir_scan: ENTRIES must be >= 2 (got %0d)", ENTRIES);
    if (LRUW < 2)    $fatal(1, "zhao_field_progdir_scan: LRUW must be >= 2 (got %0d)", LRUW);
  end

  // ---- the directory memory ----------------------------------------------
  // Textbook simple-dual-port form (zhao_texture_v3bank / zhao_field_rf_ram):
  // clock only, no reset, no initialiser, registered read in the same process.
  (* ramstyle = "M10K" *) logic [ROWW-1:0] dir_mem [ENTRIES];

  logic [IDXW-1:0] rd_addr;
  logic [ROWW-1:0] rd_q;
  logic            wr_en;
  logic [IDXW-1:0] wr_addr;
  logic [ROWW-1:0] wr_data;
  logic            wr_pending;   // S_DECIDE chose a row to (re)stamp; S_RESP writes it

  always_ff @(posedge clk) begin
    if (wr_en) dir_mem[wr_addr] <= wr_data;
    rd_q <= dir_mem[rd_addr];
  end

  logic [HASHW-1:0] rd_hash;
  logic [LRUW-1:0]  rd_lru;
  assign rd_hash = rd_q[HASHW-1:0];
  assign rd_lru  = rd_q[ROWW-1:LRU_LO];

  // ---- state -------------------------------------------------------------
  typedef enum logic [1:0] {
    S_IDLE,
    S_SCAN,
    S_DECIDE,
    S_RESP
  } state_e;

  state_e state;

  logic [ENTRIES-1:0] valid_q;
  logic [LRUW-1:0]    lru_ctr;

  // The request, latched at acceptance.
  logic             q_commit;
  logic [HASHW-1:0] q_hash;
  logic             q_ok;

  // Scan bookkeeping: `i` is the row whose read is being issued, `j` the row
  // `rd_q` belongs to (one stage behind). `j_live` is false only on the first
  // scan cycle, when rd_q holds nothing yet.
  logic [IDXW-1:0] i;
  logic [IDXW-1:0] j;
  logic            j_live;
  logic            j_last;

  // The three things one pass finds. Each keeps the LOWEST index that
  // qualified, because the scan is ascending and each is set once (hit, free)
  // or only on a STRICT improvement (best), which is the old block's
  // descending-loop-with-<= tie rule expressed the other way round.
  logic            have_hit;
  logic [IDXW-1:0] hit_idx;
  logic            have_free;
  logic [IDXW-1:0] free_idx;
  logic            have_best;
  logic [IDXW-1:0] best_idx;
  logic [LRUW-1:0] best_lru;

  logic [IDXW-1:0] victim;
  assign victim = have_free ? free_idx : best_idx;

  assign rd_addr     = i;
  assign j_last      = j_live && (j == IDXW'(ENTRIES - 1));
  assign req_ready_o = (state == S_IDLE);

  logic req_fire;
  assign req_fire = req_valid_i && req_ready_o;

  // The write port. After the S_DECIDE edge `lru_ctr` already holds the stamp
  // the chosen row must carry (the old block stored `lru_ctr + 1` and
  // incremented on the same edge) and `q_hash` the hash, so the row is written
  // straight from those registers on S_RESP's first cycle -- no 80-bit copy of
  // the data. Whole row, one write, from registers stable for the whole state.
  assign wr_en   = (state == S_RESP) && wr_pending;
  assign wr_data = {lru_ctr, q_hash};

  // A stamp is unique per write until the counter wraps; the ONE comparator.
  logic row_older;
  assign row_older = !have_best || (rd_lru < best_lru);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= S_IDLE;
      valid_q <= '0;
      lru_ctr <= '0;
      q_commit <= 1'b0;
      q_hash <= '0;
      q_ok <= 1'b0;
      i <= '0;
      j <= '0;
      j_live <= 1'b0;
      have_hit <= 1'b0;
      hit_idx <= '0;
      have_free <= 1'b0;
      free_idx <= '0;
      have_best <= 1'b0;
      best_idx <= '0;
      best_lru <= '0;
      wr_pending <= 1'b0;
      wr_addr <= '0;
      resp_valid_o <= 1'b0;
      resp_commit_o <= 1'b0;
      resp_hit_o <= 1'b0;
      resp_inserted_o <= 1'b0;
      resp_evicted_o <= 1'b0;
      resp_slot_o <= '0;
      hits_o <= '0;
      misses_o <= '0;
      programs_rejected_o <= '0;
      evictions_o <= '0;
      occupancy_o <= '0;
    end else begin

      unique case (state)
        S_IDLE: begin
          if (req_fire) begin
            q_commit <= req_commit_i;
            q_hash <= req_hash_i;
            q_ok <= req_ok_i;
            i <= '0;
            j <= '0;
            j_live <= 1'b0;
            have_hit <= 1'b0;
            have_free <= 1'b0;
            have_best <= 1'b0;
            // A rejected commit touches nothing, so it has nothing to scan for.
            state <= (req_commit_i && !req_ok_i) ? S_DECIDE : S_SCAN;
          end
        end

        S_SCAN: begin
          // Issue side: the read of row i lands in rd_q at this edge.
          j <= i;
          j_live <= 1'b1;
          if (i != IDXW'(ENTRIES - 1)) i <= i + 1'b1;

          // Evaluate side: row j is in rd_q now. Every use is gated on valid_q[j].
          if (j_live) begin
            if (valid_q[j]) begin
              if (!have_hit && (rd_hash == q_hash)) begin
                have_hit <= 1'b1;
                hit_idx <= j;
              end
              if (row_older) begin
                have_best <= 1'b1;
                best_lru <= rd_lru;
                best_idx <= j;
              end
            end else if (!have_free) begin
              have_free <= 1'b1;
              free_idx <= j;
            end
          end

          if (j_last) state <= S_DECIDE;
        end

        S_DECIDE: begin
          resp_valid_o <= 1'b1;
          resp_commit_o <= q_commit;
          resp_hit_o <= 1'b0;
          resp_inserted_o <= 1'b0;
          resp_evicted_o <= 1'b0;
          resp_slot_o <= '0;
          if (!q_commit) begin
            // ---- lookup -------------------------------------------------
            resp_hit_o <= have_hit;
            if (have_hit) begin
              resp_slot_o <= hit_idx;
              hits_o <= hits_o + 32'd1;
              lru_ctr <= lru_ctr + 1'b1;
              // Restamp: whole-row write; the stored hash IS q_hash on a hit.
              wr_pending <= 1'b1;
              wr_addr <= hit_idx;
            end
            // A miss counts nothing and writes nothing.
          end else if (!q_ok) begin
            // ---- rejected commit: counted, and NOTHING else ---------------
            if (programs_rejected_o != 32'hFFFF_FFFF)
              programs_rejected_o <= programs_rejected_o + 32'd1;
          end else begin
            // ---- valid commit: first free, else oldest, lowest index ------
            resp_inserted_o <= 1'b1;
            resp_evicted_o <= !have_free;
            resp_slot_o <= victim;
            misses_o <= misses_o + 32'd1;
            if (!have_free) evictions_o <= evictions_o + 32'd1;
            else occupancy_o <= occupancy_o + 1'b1;
            valid_q[victim] <= 1'b1;
            lru_ctr <= lru_ctr + 1'b1;
            wr_pending <= 1'b1;
            wr_addr <= victim;
          end
          state <= S_RESP;
        end

        S_RESP: begin
          wr_pending <= 1'b0;   // the write lands on this state's first edge
          if (resp_ready_i) begin
            resp_valid_o <= 1'b0;
            state <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

`ifndef SYNTHESIS
  // The memory sheet's collision claim, checked rather than trusted: a write
  // never lands on a clock in which a read is being consumed. (Reads are only
  // consumed in S_SCAN; the write fires in the clock after S_DECIDE.)
  // (No rst_n term: in reset the state is S_IDLE and wr_en is low, so neither
  // condition can hold, and reading rst_n synchronously here would make lint
  // call it a mixed sync/async net.)
  always_ff @(posedge clk) begin
    if (wr_en && (state == S_SCAN))
      $error("zhao_field_progdir_scan: directory write during a scan (same-address read/write hazard)");
    // "Every live row has a distinct hash" is a CALLER law (commit follows
    // miss), not this block's, and is not asserted. A victim must exist when
    // the directory is full:
    if ((state == S_DECIDE) && q_commit && q_ok && !have_free && !have_best)
      $error("zhao_field_progdir_scan: full directory with no victim");
  end
`endif

endmodule : zhao_field_progdir_scan
