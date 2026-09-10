// zhao_field_progdir.sv -- the OLD two-interface FIELD.PROGCACHE port list,
// served by the scanned directory (zhao_field_progdir_scan.sv).
//
// This is the adapter roadmap section 5 asks for: "The old two interfaces need
// a small adapter preserving their accepted transaction order and lookup
// priority. The adapter records the request kind until its response is
// accepted; it must not mistake a lookup miss for a rejected commit. Do not
// call this a fixed-latency drop-in."
//
// Port for port it is zhao_field_progcache. Transaction for transaction it is
// zhao_field_progcache (ENFORCED-BY: tests/field/field_progdir_differential.cpp,
// old and new elaborated side by side). Clock for clock it is NOT: a lookup or
// a valid commit is answered ENTRIES+4 clocks after acceptance instead of one
// (MEASURED 20 at ENTRIES=16, same for hit, miss and insert; a rejected commit
// 3), the no-stall accept-to-accept interval is the same 20, and while a
// transaction is in flight BOTH request ports are not-ready. The
// contract amendment of 2026-09-10 in design/contracts/FIELD.PROGCACHE.md
// records that and what it means for the caller (acquire ONCE per
// program/association and hold the slot; never per point).
//
// ---------------------------------------------------------------------------
// ARBITRATION -- the old block's law, preserved
// ---------------------------------------------------------------------------
// Old: `cm_ready_o = (!cm_resp_valid_o || cm_resp_ready_i) && !lu_fire`, so a
// lookup and a commit never fire on the same clock and the lookup wins.
//
// Here the scanner accepts one request at a time. A lookup is offered whenever
// its response channel is free; a commit is offered only when no lookup is
// being offered. So:
//   * the two never fire on one clock (one scanner request per clock);
//   * when both are offered on a clock the scanner is idle, the LOOKUP fires,
//     and the commit fires when the scanner is next idle -- the same accepted
//     order as the old block, later.
//
// ---------------------------------------------------------------------------
// RESPONSE ROUTING -- the kind travels WITH the transaction
// ---------------------------------------------------------------------------
// The scanner echoes the request kind on its response; that echo is the
// "recorded request kind" of section 5. A response with commit=0 goes to the
// lookup channel (hit/slot), commit=1 to the commit channel
// (inserted/evicted/slot). A lookup miss (hit=0) and a rejected commit
// (inserted=0) therefore cannot be confused: they leave on different ports
// because they arrived on different ports, and nothing in between looks at
// the result bits to decide.
//
// A response channel is always FREE when its response arrives: a request is
// accepted only when its own channel is free (empty, or being taken on that
// same edge), the scanner answers no earlier than the next clock, and nothing
// else fills that channel meanwhile. `s_resp_ready` is still wired to the
// channel-free condition rather than to 1'b1 -- if the invariant were ever
// broken the scanner would HOLD its answer instead of the channel overwriting
// an untaken one. Belt and braces, one AND gate.
module zhao_field_progdir #(
    parameter int ENTRIES = 16,
    parameter int LRUW    = 48
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
    input  logic        cm_ok_i,

    output logic                      cm_resp_valid_o,
    input  logic                      cm_resp_ready_i,
    output logic                      cm_inserted_o,
    output logic                      cm_evicted_o,
    output logic [$clog2(ENTRIES)-1:0] cm_slot_o,

    // ---- counters ----------------------------------------------------------
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

  // ---- the scanner -----------------------------------------------------------
  logic            s_req_valid, s_req_ready, s_req_commit, s_req_ok;
  logic [31:0]     s_req_hash;
  logic            s_resp_valid, s_resp_ready, s_resp_commit;
  logic            s_hit, s_inserted, s_evicted;
  logic [IDXW-1:0] s_slot;

  assign s_req_valid  = lu_offer || (cm_valid_i && cm_free);
  assign s_req_commit = !lu_offer;                 // lookup priority
  assign s_req_hash   = lu_offer ? lu_hash_i : cm_hash_i;
  assign s_req_ok     = cm_ok_i;

  // Neither ready depends on its own valid. cm_ready depends on lu_valid_i,
  // exactly as the old block's did.
  assign lu_ready_o = s_req_ready && lu_free;
  assign cm_ready_o = s_req_ready && cm_free && !lu_offer;

  assign s_resp_ready = s_resp_commit ? cm_free : lu_free;

  zhao_field_progdir_scan #(
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

  // ---- the two response channels ---------------------------------------------
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

`ifndef SYNTHESIS
  // The routing invariant, checked: a scanner response never finds its channel
  // occupied by an untaken answer.
  // (No rst_n term: s_resp_valid is low in reset, and cm_ready_o is low
  // whenever a lookup is offered, so neither can fire falsely there.)
  always_ff @(posedge clk) begin
    if (s_resp_valid && !s_resp_ready)
      $error("zhao_field_progdir: scanner response held because its channel was not free -- the acceptance rule was violated");
    if ((lu_valid_i && lu_ready_o) && (cm_valid_i && cm_ready_o))
      $error("zhao_field_progdir: a lookup and a commit fired on the same clock");
  end
`endif

endmodule : zhao_field_progdir
