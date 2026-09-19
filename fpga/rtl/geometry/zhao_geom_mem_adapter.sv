// zhao_geom_mem_adapter.sv -- two logical geometry requesters, one ENGINE1 client.
//
// Law: reports/COMBINE-ASSETFETCH-RECOVERY-20260906.txt 12
//      spec/memory_rules.md 5f (the asset pool window, ENGINE1, read-only)
//
// ===========================================================================
// THIS IS NOW A WRAPPER, AND THE BODY MOVED RATHER THAN BEING COPIED
// ===========================================================================
// The arbitration, the ownership record, the three acceptance boundaries and
// the two burst scales all live in `fpga/rtl/memory/zhao_mem_share2.sv`. Read
// that file for the argument; this one is the GEOMETRY BINDING of it, and the
// binding is two values:
//
//     CLIENT_ID  = ZHAO_CLIENT_ENGINE1   -- the one identity the guard grants
//                                           the asset pool to
//     FORCE_READ = 1                     -- the asset window is READ-ONLY by
//                                           construction
//
//   MESHFETCH descriptors --+
//                           +--> zhao_mem_share2 --> ENGINE1 guard --> slot 3
//   ASSETFETCH payloads ----+
//
// WHY IT MOVED. Core entry I26 records the terrain compose path needing the
// same thing for the same reason -- one guard socket, two readers -- and the
// choice was a second copy of ~200 lines of arbitration or one parameter. Two
// copies of a state machine is two places for the guard's two-cycle verdict law
// to be got wrong, and this tree has a committed tool
// (`tools/rtl/check_guard_verdict.py`) that exists precisely because it WAS got
// wrong, twice, in two clients that had each written it out separately.
//
// PORTS, DEFAULTS AND BEHAVIOUR ARE UNCHANGED. `tests/geometry/
// geom_mem_adapter_directed.cpp` is untouched and is the evidence for that:
// the module name, every port name and width, and every counter are the same,
// so a passing run of that test after the move is a statement about the move.
//
// NOT A COPY, SO IT CANNOT DRIFT. `tools/budget/mutant_copy_drift.py` names
// this exact distinction -- "a wrapper that instantiates the production module
// cannot drift, and must not be counted as a copy". There is no second body
// here to go stale.
//
// Conservative SystemVerilog subset only (charter 2).
`default_nettype none

module zhao_geom_mem_adapter
  import zhao_pkg::*;
(
    input  var logic clk,
    input  var logic rst_n,

    // ---- requester A: GEOM.MESHFETCH, descriptors (32 bytes) ---------------
    input  var zhao_guard_req_t a_req_i,
    output var zhao_guard_rsp_t a_rsp_o,
    output var logic            a_beat_valid_o,
    output var logic [63:0]     a_beat_data_o,
    output var logic            a_beat_last_o,

    // ---- requester B: GEOM.ASSETFETCH, payload lines (64 bytes) ------------
    input  var zhao_guard_req_t b_req_i,
    output var zhao_guard_rsp_t b_rsp_o,
    output var logic            b_beat_valid_o,
    output var logic [63:0]     b_beat_data_o,
    output var logic            b_beat_last_o,

    // ---- the one permitted client, downstream to MEM.GUARD ----------------
    output var zhao_guard_req_t m_req_o,
    input  var zhao_guard_rsp_t m_rsp_i,
    input  var logic            m_beat_valid_i,
    input  var logic [63:0]     m_beat_data_i,
    input  var logic            m_beat_last_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0]     jobs_a_o,          // logical requests served, A
    output var logic [31:0]     jobs_b_o,          // ...and B
    output var logic [31:0]     denied_o,          // guard violations, either
    output var logic [31:0]     contention_o,
    output var logic [31:0]     err_short_o,
    output var logic [31:0]     err_long_o,
    output var logic [31:0]     err_unowned_o
);

  // THE TRUSTED FIXED IDENTITY (11.2), stated here and forced inside the
  // share: a leaf test's generic client input must never be able to reach the
  // production guard through this path.
  zhao_mem_share2 #(
    .CLIENT_ID (3),          // ZHAO_CLIENT_ENGINE1 -- see zhao_pkg
    .FORCE_READ(1'b1)        // the asset window is READ-ONLY by construction
  ) u_share (
    .clk  (clk),
    .rst_n(rst_n),

    .a_req_i       (a_req_i),
    .a_rsp_o       (a_rsp_o),
    .a_beat_valid_o(a_beat_valid_o),
    .a_beat_data_o (a_beat_data_o),
    .a_beat_last_o (a_beat_last_o),

    .b_req_i       (b_req_i),
    .b_rsp_o       (b_rsp_o),
    .b_beat_valid_o(b_beat_valid_o),
    .b_beat_data_o (b_beat_data_o),
    .b_beat_last_o (b_beat_last_o),

    .m_req_o       (m_req_o),
    .m_rsp_i       (m_rsp_i),
    .m_beat_valid_i(m_beat_valid_i),
    .m_beat_data_i (m_beat_data_i),
    .m_beat_last_i (m_beat_last_i),

    .jobs_a_o     (jobs_a_o),
    .jobs_b_o     (jobs_b_o),
    .denied_o     (denied_o),
    .contention_o (contention_o),
    .err_short_o  (err_short_o),
    .err_long_o   (err_long_o),
    .err_unowned_o(err_unowned_o)
  );

endmodule

`default_nettype wire
