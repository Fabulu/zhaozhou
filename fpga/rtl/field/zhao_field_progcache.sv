// zhao_field_progcache.sv — FIELD.PROGCACHE: the resident program directory.
//
// Contract: design/contracts/FIELD.PROGCACHE.md
// Reference: `zref::field::ProgCache` (reference/include/zref/zref_progcache.hpp).
//
// ---------------------------------------------------------------------------
// WHAT THIS BLOCK OWNS, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------------------------------
// It owns RESIDENCY: which validated programs are cached, which slot each one is
// in, which one to evict, and the four counters the ledger names.
//
// It does NOT own:
//
//   * VALIDATION. `zfield::decode` is the single implementation of the
//     spec/form/field-ir.md §4/§5 validation law, with thirteen named error
//     classes. Re-deriving any of them here would be a second implementation of
//     ratified law. The caller decodes and reports one bit: ok, or not.
//   * THE PROGRAM STORE. Microcode plus constant tables for sixteen programs is
//     a memory sizing decision with alternatives, and burying it inside this
//     module would settle it silently — the same reasoning the pose cache's
//     header gives. This block hands out a slot index; the caller owns the store.
//   * EXECUTION. Nothing here evaluates a program, which is what keeps the block
//     clear of field-ir.md §1's grep-audit law ("no RTL-side re-derivation ahead
//     of the profile engine").
//
// ---------------------------------------------------------------------------
// THE TRANSACTION IS TWO-PHASE, AND IT HAS TO BE
// ---------------------------------------------------------------------------
//   Phase A, LOOKUP: offered a declared hash. Answers HIT with a slot, or MISS.
//   Phase B, COMMIT: only after a miss. The caller has decoded by now and
//                    reports ok/not. Answers INSERTED with a slot, or REJECTED.
//
// One phase is not enough because the decode that a miss requires costs orders
// of magnitude more than the lookup, and speculatively decoding every offered
// program to find out whether it was already resident would make the cache cost
// more than it saves. Two phases is the shape the saving comes from.
//
// ---------------------------------------------------------------------------
// THE LAW, and where it differs from the pose cache
// ---------------------------------------------------------------------------
//   HIT      -> hits++, the entry's LRU is restamped. Nothing is decoded.
//   MISS     -> nothing is counted YET: a miss that turns out to be a rejection
//               is a rejection, not a miss.
//   COMMIT ok    -> misses++, insert into the first free slot, else evict the
//                   least-recently-used entry (evictions++).
//   COMMIT not ok-> programs_rejected++, and NOTHING is cached. A rejected
//                   program is not remembered either: it is re-validated the
//                   next time it is offered. The reference records why -- a
//                   negative cache would keep a program rejected after it became
//                   valid.
//
// **TAGS IN REGISTERS, COMPARED IN PARALLEL — the opposite of the pose cache,
// on purpose.** GEOM.POSE's directory is 128 entries, where 128 comparators
// means ~6,300 flip-flops of tag and a scan is the cheaper shape. This one is
// SIXTEEN entries: sixteen 32-bit comparators, and the whole directory is about
// 1,300 flip-flops. Scanning it would cost sixteen cycles to save almost
// nothing. The right answer differs because the size differs, and both headers
// say so rather than leaving a reader to wonder which is the house style.
//
// The LRU stamp is 48 bits. 32 would wrap in under a day of continuous play at
// a realistic acquire rate and silently invert the eviction order.
module zhao_field_progcache #(
    parameter int ENTRIES = 16
) (
    input logic clk,
    input logic rst_n,

    // ---- phase A: lookup ---------------------------------------------------
    input  logic        lu_valid_i,
    output logic        lu_ready_o,
    input  logic [31:0] lu_hash_i,

    output logic                      lu_resp_valid_o,
    input  logic                      lu_resp_ready_i,
    output logic                      lu_hit_o,
    output logic [$clog2(ENTRIES)-1:0] lu_slot_o,

    // ---- phase B: commit, after the caller has decoded ---------------------
    input  logic        cm_valid_i,
    output logic        cm_ready_o,
    input  logic [31:0] cm_hash_i,
    input  logic        cm_ok_i,      // the decode verdict, and nothing more

    output logic                      cm_resp_valid_o,
    input  logic                      cm_resp_ready_i,
    output logic                      cm_inserted_o,  // 0 = rejected
    output logic                      cm_evicted_o,   // an entry was displaced
    output logic [$clog2(ENTRIES)-1:0] cm_slot_o,

    // ---- counters ----------------------------------------------------------
    output logic [31:0] hits_o,
    output logic [31:0] misses_o,
    output logic [31:0] programs_rejected_o,
    output logic [31:0] evictions_o,
    output logic [$clog2(ENTRIES):0] occupancy_o
);

  localparam int IDXW = $clog2(ENTRIES);
  localparam int LRUW = 48;

  logic [ENTRIES-1:0]  ent_valid;
  logic [31:0]         ent_hash[0:ENTRIES-1];
  logic [LRUW-1:0]     ent_lru [0:ENTRIES-1];
  logic [LRUW-1:0]     lru_ctr;

  // ---- parallel lookup ----------------------------------------------------
  logic              hit_any;
  logic [IDXW-1:0]   hit_idx;
  logic              free_any;
  logic [IDXW-1:0]   free_idx;
  logic [IDXW-1:0]   lru_idx;

  integer i;
  logic [LRUW-1:0] best_lru;
  always_comb begin
    hit_any = 1'b0;
    hit_idx = '0;
    free_any = 1'b0;
    free_idx = '0;
    lru_idx = '0;
    best_lru = {LRUW{1'b1}};
    // Descending so the LOWEST matching index wins, which is the order the
    // reference's loops take.
    for (i = ENTRIES - 1; i >= 0; i = i - 1) begin
      if (ent_valid[i] && ent_hash[i] == lu_hash_i) begin
        hit_any = 1'b1;
        hit_idx = IDXW'(i);
      end
      if (!ent_valid[i]) begin
        free_any = 1'b1;
        free_idx = IDXW'(i);
      end
      if (ent_valid[i] && ent_lru[i] <= best_lru) begin
        best_lru = ent_lru[i];
        lru_idx = IDXW'(i);
      end
    end
  end

  logic [IDXW-1:0] victim;
  assign victim = free_any ? free_idx : lru_idx;

  assign lu_ready_o = !lu_resp_valid_o || lu_resp_ready_i;
  // A commit always FOLLOWS a miss, so the two phases are naturally sequential
  // for any one caller. Refusing a commit on a cycle a lookup fires makes that
  // explicit and removes the only interaction between them -- both restamp the
  // LRU counter, and letting them do so in the same cycle needs ordering logic
  // that buys nothing.
  assign cm_ready_o = (!cm_resp_valid_o || cm_resp_ready_i) && !lu_fire;

  logic lu_fire, cm_fire;
  assign lu_fire = lu_valid_i && lu_ready_o;
  assign cm_fire = cm_valid_i && cm_ready_o;

  integer k;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      ent_valid <= '0;
      for (k = 0; k < ENTRIES; k = k + 1) begin
        ent_hash[k] <= '0;
        ent_lru[k] <= '0;
      end
      lru_ctr <= '0;
      lu_resp_valid_o <= 1'b0;
      lu_hit_o <= 1'b0;
      lu_slot_o <= '0;
      cm_resp_valid_o <= 1'b0;
      cm_inserted_o <= 1'b0;
      cm_evicted_o <= 1'b0;
      cm_slot_o <= '0;
      hits_o <= '0;
      misses_o <= '0;
      programs_rejected_o <= '0;
      evictions_o <= '0;
      occupancy_o <= '0;
    end else begin
      if (lu_resp_valid_o && lu_resp_ready_i) lu_resp_valid_o <= 1'b0;
      if (cm_resp_valid_o && cm_resp_ready_i) cm_resp_valid_o <= 1'b0;

      // ---- phase A ---------------------------------------------------------
      if (lu_fire) begin
        lu_resp_valid_o <= 1'b1;
        lu_hit_o <= hit_any;
        lu_slot_o <= hit_idx;
        if (hit_any) begin
          // A hit restamps: touching a program has to make it young again, or
          // the cache throws away the one most likely to be wanted next.
          hits_o <= hits_o + 32'd1;
          lru_ctr <= lru_ctr + 1'b1;
          ent_lru[hit_idx] <= lru_ctr + 1'b1;
        end
        // A miss counts NOTHING here. Whether it was a miss or a rejection is
        // not known until the caller comes back with a decode verdict.
      end

      // ---- phase B ---------------------------------------------------------
      if (cm_fire) begin
        cm_resp_valid_o <= 1'b1;
        if (!cm_ok_i) begin
          // Rejected: not cached, not remembered, counted.
          cm_inserted_o <= 1'b0;
          cm_evicted_o <= 1'b0;
          cm_slot_o <= '0;
          if (programs_rejected_o != 32'hFFFF_FFFF) begin
            programs_rejected_o <= programs_rejected_o + 32'd1;
          end
        end else begin
          cm_inserted_o <= 1'b1;
          cm_slot_o <= victim;
          cm_evicted_o <= !free_any;
          misses_o <= misses_o + 32'd1;
          if (!free_any) evictions_o <= evictions_o + 32'd1;
          else occupancy_o <= occupancy_o + 1'b1;
          ent_valid[victim] <= 1'b1;
          ent_hash[victim] <= cm_hash_i;
          lru_ctr <= lru_ctr + 1'b1;
          ent_lru[victim] <= lru_ctr + 1'b1;
        end
      end
    end
  end

endmodule : zhao_field_progcache
