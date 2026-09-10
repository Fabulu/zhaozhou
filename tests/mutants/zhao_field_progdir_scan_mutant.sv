// zhao_field_progdir_scan_mutant.sv -- A DELIBERATELY BROKEN COPY. NOT SHIPPED.
//
// This exists to make the transaction differential in
// tests/field/field_progdir_differential.cpp a DEMONSTRATED instrument for the
// one law it is hardest to reach: the LRU TIE. Roadmap section 5 names it
// first in the gate ("including LRU ties, wrap, ..."), and it is only
// observable after the stamp counter has WRAPPED, because until then every
// stamp is unique. A directory with the tie rule inverted passes every
// non-wrapping test there is -- fill, hit, evict, reject, held responses,
// interleaved channels -- with every counter balancing. Only the victim's
// INDEX differs, and only when two live rows carry the same stamp.
//
// The one substantive change, in the scan's minimum-stamp comparator:
//
//     assign row_older = !have_best || (rd_lru <  best_lru);   // real: lowest index on a tie
//  -> assign row_older = !have_best || (rd_lru <= best_lru);   // mutant: HIGHEST index on a tie
//
// The retained oracle (zhao_field_progcache) walks its rows DESCENDING with
// `<=`, which lands on the lowest index; the real scanner walks ASCENDING with
// `<`, which lands on the lowest index too. The mutant's ascending `<=` lands
// on the highest. Same eviction count, same hit and miss totals, different slot.
//
// INVERTED POLARITY: driven by tests/field/field_progdir_mutant_control.cpp
// through tests/field/tb_field_progdir_diff.sv with -DPROGDIR_MUTANT, at
// ENTRIES=2, LRUW=2 so a tie is constructible in six transactions. The control
// PASSES when (a) the non-tie directed cases still AGREE with the oracle -- the
// weak-vector half, showing why a suite without wrap would wave this through --
// and (b) the tie case DISAGREES. Evidence about the instrument, not the design.
//
// The second module below is the ADAPTER copied verbatim with only its name and
// the scanner it instantiates changed: wiring, no second substantive change. It
// is here because a module name cannot be parameterised and the differential
// tests the adapter's port list, not the scanner's.
//
// Both modules are RENAMED so a source-list mistake can never elaborate them in
// place of the real ones, and they live under tests/ where
// check_forbidden_sources.py will not find them in a production closure.
//
// The real scanner's `ifndef SYNTHESIS` assertions are omitted here: they
// check the memory port discipline, which the mutation does not touch, and the
// differential -- not an assertion -- is the instrument this file exists for.
//
// REGENERATE IF fpga/rtl/field/zhao_field_progdir_scan.sv or
// zhao_field_progdir.sv change shape: this is a copy, and a copy of an old
// version is a positive control for a block that no longer exists.

module zhao_field_progdir_scan_mutant #(
    parameter int ENTRIES = 16,
    parameter int HASHW   = 32,
    parameter int LRUW    = 48
) (
    input  logic clk,
    input  logic rst_n,
    input  logic             req_valid_i,
    output logic             req_ready_o,
    input  logic             req_commit_i,
    input  logic [HASHW-1:0] req_hash_i,
    input  logic             req_ok_i,
    output logic                       resp_valid_o,
    input  logic                       resp_ready_i,
    output logic                       resp_commit_o,
    output logic                       resp_hit_o,
    output logic                       resp_inserted_o,
    output logic                       resp_evicted_o,
    output logic [$clog2(ENTRIES)-1:0] resp_slot_o,
    output logic [31:0] hits_o,
    output logic [31:0] misses_o,
    output logic [31:0] programs_rejected_o,
    output logic [31:0] evictions_o,
    output logic [$clog2(ENTRIES):0] occupancy_o
);

  localparam int IDXW = $clog2(ENTRIES);
  localparam int ROWW = HASHW + LRUW;
  localparam int LRU_LO = HASHW;

  initial begin
    if (ENTRIES < 2) $fatal(1, "zhao_field_progdir_scan_mutant: ENTRIES must be >= 2 (got %0d)", ENTRIES);
    if (LRUW < 2)    $fatal(1, "zhao_field_progdir_scan_mutant: LRUW must be >= 2 (got %0d)", LRUW);
  end

  (* ramstyle = "M10K" *) logic [ROWW-1:0] dir_mem [ENTRIES];

  logic [IDXW-1:0] rd_addr;
  logic [ROWW-1:0] rd_q;
  logic            wr_en;
  logic [IDXW-1:0] wr_addr;
  logic [ROWW-1:0] wr_data;
  logic            wr_pending;

  always_ff @(posedge clk) begin
    if (wr_en) dir_mem[wr_addr] <= wr_data;
    rd_q <= dir_mem[rd_addr];
  end

  logic [HASHW-1:0] rd_hash;
  logic [LRUW-1:0]  rd_lru;
  assign rd_hash = rd_q[HASHW-1:0];
  assign rd_lru  = rd_q[ROWW-1:LRU_LO];

  typedef enum logic [1:0] {
    S_IDLE,
    S_SCAN,
    S_DECIDE,
    S_RESP
  } state_e;

  state_e state;

  logic [ENTRIES-1:0] valid_q;
  logic [LRUW-1:0]    lru_ctr;
  logic             q_commit;
  logic [HASHW-1:0] q_hash;
  logic             q_ok;
  logic [IDXW-1:0] i;
  logic [IDXW-1:0] j;
  logic            j_live;
  logic            j_last;
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

  assign wr_en   = (state == S_RESP) && wr_pending;
  assign wr_data = {lru_ctr, q_hash};

  // THE MUTATION: `<` became `<=`. A later row with an EQUAL stamp now
  // displaces the earlier one, so the HIGHEST index wins a tie.
  logic row_older;
  assign row_older = !have_best || (rd_lru <= best_lru);

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
            state <= (req_commit_i && !req_ok_i) ? S_DECIDE : S_SCAN;
          end
        end

        S_SCAN: begin
          j <= i;
          j_live <= 1'b1;
          if (i != IDXW'(ENTRIES - 1)) i <= i + 1'b1;

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
            resp_hit_o <= have_hit;
            if (have_hit) begin
              resp_slot_o <= hit_idx;
              hits_o <= hits_o + 32'd1;
              lru_ctr <= lru_ctr + 1'b1;
              wr_pending <= 1'b1;
              wr_addr <= hit_idx;
            end
          end else if (!q_ok) begin
            if (programs_rejected_o != 32'hFFFF_FFFF)
              programs_rejected_o <= programs_rejected_o + 32'd1;
          end else begin
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
          wr_pending <= 1'b0;
          if (resp_ready_i) begin
            resp_valid_o <= 1'b0;
            state <= S_IDLE;
          end
        end

        default: state <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_field_progdir_scan_mutant


// The adapter, copied with its name and its scanner changed. WIRING ONLY.
// (Second module in one file, deliberately: the mutant is ONE artefact.)
/* verilator lint_off DECLFILENAME */
module zhao_field_progdir_mutant #(
    parameter int ENTRIES = 16,
    parameter int LRUW    = 48
) (
    input logic clk,
    input logic rst_n,
    input  logic        lu_valid_i,
    output logic        lu_ready_o,
    input  logic [31:0] lu_hash_i,
    output logic                      lu_resp_valid_o,
    input  logic                      lu_resp_ready_i,
    output logic                      lu_hit_o,
    output logic [$clog2(ENTRIES)-1:0] lu_slot_o,
    input  logic        cm_valid_i,
    output logic        cm_ready_o,
    input  logic [31:0] cm_hash_i,
    input  logic        cm_ok_i,
    output logic                      cm_resp_valid_o,
    input  logic                      cm_resp_ready_i,
    output logic                      cm_inserted_o,
    output logic                      cm_evicted_o,
    output logic [$clog2(ENTRIES)-1:0] cm_slot_o,
    output logic [31:0] hits_o,
    output logic [31:0] misses_o,
    output logic [31:0] programs_rejected_o,
    output logic [31:0] evictions_o,
    output logic [$clog2(ENTRIES):0] occupancy_o
);

  localparam int IDXW = $clog2(ENTRIES);

  logic lu_free, cm_free;
  assign lu_free = !lu_resp_valid_o || lu_resp_ready_i;
  assign cm_free = !cm_resp_valid_o || cm_resp_ready_i;

  logic lu_offer;
  assign lu_offer = lu_valid_i && lu_free;

  logic            s_req_valid, s_req_ready, s_req_commit, s_req_ok;
  logic [31:0]     s_req_hash;
  logic            s_resp_valid, s_resp_ready, s_resp_commit;
  logic            s_hit, s_inserted, s_evicted;
  logic [IDXW-1:0] s_slot;

  assign s_req_valid  = lu_offer || (cm_valid_i && cm_free);
  assign s_req_commit = !lu_offer;
  assign s_req_hash   = lu_offer ? lu_hash_i : cm_hash_i;
  assign s_req_ok     = cm_ok_i;

  assign lu_ready_o = s_req_ready && lu_free;
  assign cm_ready_o = s_req_ready && cm_free && !lu_offer;

  assign s_resp_ready = s_resp_commit ? cm_free : lu_free;

  zhao_field_progdir_scan_mutant #(
      .ENTRIES(ENTRIES),
      .HASHW  (32),
      .LRUW   (LRUW)
  ) u_scan (
      .clk                (clk),
      .rst_n              (rst_n),
      .req_valid_i        (s_req_valid),
      .req_ready_o        (s_req_ready),
      .req_commit_i       (s_req_commit),
      .req_hash_i         (s_req_hash),
      .req_ok_i           (s_req_ok),
      .resp_valid_o       (s_resp_valid),
      .resp_ready_i       (s_resp_ready),
      .resp_commit_o      (s_resp_commit),
      .resp_hit_o         (s_hit),
      .resp_inserted_o    (s_inserted),
      .resp_evicted_o     (s_evicted),
      .resp_slot_o        (s_slot),
      .hits_o             (hits_o),
      .misses_o           (misses_o),
      .programs_rejected_o(programs_rejected_o),
      .evictions_o        (evictions_o),
      .occupancy_o        (occupancy_o)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      lu_resp_valid_o <= 1'b0;
      lu_hit_o <= 1'b0;
      lu_slot_o <= '0;
      cm_resp_valid_o <= 1'b0;
      cm_inserted_o <= 1'b0;
      cm_evicted_o <= 1'b0;
      cm_slot_o <= '0;
    end else begin
      if (lu_resp_valid_o && lu_resp_ready_i) lu_resp_valid_o <= 1'b0;
      if (cm_resp_valid_o && cm_resp_ready_i) cm_resp_valid_o <= 1'b0;

      if (s_resp_valid && s_resp_ready) begin
        if (!s_resp_commit) begin
          lu_resp_valid_o <= 1'b1;
          lu_hit_o <= s_hit;
          lu_slot_o <= s_slot;
        end else begin
          cm_resp_valid_o <= 1'b1;
          cm_inserted_o <= s_inserted;
          cm_evicted_o <= s_evicted;
          cm_slot_o <= s_slot;
        end
      end
    end
  end

endmodule : zhao_field_progdir_mutant
/* verilator lint_on DECLFILENAME */
