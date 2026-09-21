// zhao_geom_mem_adapter.sv -- four logical ENGINE1 readers, one ENGINE1 client.
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
// (2026-09-19: requester C and `jobs_c_o` were ADDED for MATERIAL.RESOLVE;
// A's and B's ports and counters kept their names, and the directed test
// below runs against the widened adapter with C held idle.)
//
// PORTS, DEFAULTS AND BEHAVIOUR WERE UNCHANGED BY THE MOVE. `tests/geometry/
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

    // ---- requester C: MATERIAL.RESOLVE, one record (32 bytes) -------------
    // ADDED 2026-09-19 (cmdmem packet, ruling R20). `spec/memory_rules.md` 5f
    // already says it: "immutable texture-line reads [are serialized] with the
    // existing geometry adapter behind one local ENGINE1 mux, preserving the
    // same client, direction, bounds and global arbiter slot". A material
    // record is exactly such a read, so it joins HERE rather than getting a
    // second ENGINE1 path. The guard's ENGINE1 arm still admits
    // RENDER.ASSET_POOL reads and nothing else, so this requester can read
    // nothing the other two could not.
    input  var zhao_guard_req_t c_req_i,
    output var zhao_guard_rsp_t c_rsp_o,
    output var logic            c_beat_valid_o,
    output var logic [63:0]     c_beat_data_o,
    output var logic            c_beat_last_o,

    // ---- requester D: GEOM.DRAWJOB, one MESH_STREAM header (64 bytes) ------
    // ADDED 2026-09-20 (owner ruling R29). The page header is read from the
    // same pool, by the same client, in the same direction as the descriptors
    // requester A fetches -- it is the descriptor table's own front matter --
    // so it joins HERE for the reason 5f gives for C. One header per DRAW,
    // against A's one request per meshlet, so it adds the least traffic of the
    // four and cannot starve the others: the round robin is bounded at N-1.
    input  var zhao_guard_req_t d_req_i,
    output var zhao_guard_rsp_t d_rsp_o,
    output var logic            d_beat_valid_o,
    output var logic [63:0]     d_beat_data_o,
    output var logic            d_beat_last_o,

    // Requester E, PART.TABLE's species-page loader (owner ruling R42, core
    // entry I33). 64-byte lines, read ONCE per published SPECIES_TABLE page --
    // by far the rarest traffic on this share, so the round robin's bound
    // lengthens by at most one line and only while a page is loading.
    input  var zhao_guard_req_t e_req_i,
    output var zhao_guard_rsp_t e_rsp_o,
    output var logic            e_beat_valid_o,
    output var logic [63:0]     e_beat_data_o,
    output var logic            e_beat_last_o,

    // Requester F, FORGE.PRIM's program-page bank (owner ruling R234 D2, core
    // entry for the forge chain). 64-byte lines: ONE page header per
    // publication, then a bounded scan of record line 0 per PROCEDURAL DRAW.
    // That is the rarest traffic on this share after E -- a draw, not a meshlet
    // -- and it reads the asset pool by the same client in the same direction
    // as the five above, which is the condition spec/memory_rules.md 5f states
    // for joining here rather than taking a second ENGINE1 path.
    //
    // THE ROUND ROBIN'S BOUND IS N-1 TURNS AND IT MUST BE RE-PROVED AT SIX, NOT
    // ASSUMED -- `zhao_mem_share_n`'s own directed test carries the proof and
    // `geom_mem_adapter_directed` runs against the widened adapter.
    input  var zhao_guard_req_t f_req_i,
    output var zhao_guard_rsp_t f_rsp_o,
    output var logic            f_beat_valid_o,
    output var logic [63:0]     f_beat_data_o,
    output var logic            f_beat_last_o,

    // ---- the one permitted client, downstream to MEM.GUARD ----------------
    output var zhao_guard_req_t m_req_o,
    input  var zhao_guard_rsp_t m_rsp_i,
    input  var logic            m_beat_valid_i,
    input  var logic [63:0]     m_beat_data_i,
    input  var logic            m_beat_last_i,

    // ---- evidence ---------------------------------------------------------
    output var logic [31:0]     jobs_a_o,          // logical requests served, A
    output var logic [31:0]     jobs_b_o,          // ...and B
    output var logic [31:0]     jobs_c_o,          // ...and C
    output var logic [31:0]     jobs_d_o,          // ...and D
    output var logic [31:0]     jobs_e_o,          // ...and E
    output var logic [31:0]     jobs_f_o,          // ...and F
    output var logic [31:0]     denied_o,          // guard violations, any
    output var logic [31:0]     contention_o,
    output var logic [31:0]     err_short_o,
    output var logic [31:0]     err_long_o,
    output var logic [31:0]     err_unowned_o
);

  // THE TRUSTED FIXED IDENTITY (11.2), stated here and forced inside the
  // share: a leaf test's generic client input must never be able to reach the
  // production guard through this path.
  //
  // THE N-REQUESTER CORE (902949ea), at N=6 since requester F landed 2026-09-21. Its
  // round robin is bounded at N-1 turns, so C's addition lengthens A's and B's
  // worst-case wait by at most one 32-byte record -- and it is the SAME core the
  // two-port `zhao_mem_share2` instantiates, so nothing about the guard's
  // two-cycle verdict law is re-derived here.
  zhao_guard_req_t [5:0] s_req;
  zhao_guard_rsp_t [5:0] s_rsp;
  logic            [5:0] s_bv, s_bl;
  logic           [63:0] s_bd;
  logic      [5:0][31:0] s_jobs;

  assign s_req[0] = a_req_i;
  assign s_req[1] = b_req_i;
  assign s_req[2] = c_req_i;
  assign s_req[3] = d_req_i;
  assign s_req[4] = e_req_i;
  assign s_req[5] = f_req_i;
  assign a_rsp_o = s_rsp[0];
  assign b_rsp_o = s_rsp[1];
  assign c_rsp_o = s_rsp[2];
  assign d_rsp_o = s_rsp[3];
  assign e_rsp_o = s_rsp[4];
  assign f_rsp_o = s_rsp[5];
  assign a_beat_valid_o = s_bv[0];
  assign b_beat_valid_o = s_bv[1];
  assign c_beat_valid_o = s_bv[2];
  assign d_beat_valid_o = s_bv[3];
  assign e_beat_valid_o = s_bv[4];
  assign f_beat_valid_o = s_bv[5];
  assign a_beat_last_o  = s_bl[0];
  assign b_beat_last_o  = s_bl[1];
  assign c_beat_last_o  = s_bl[2];
  assign d_beat_last_o  = s_bl[3];
  assign e_beat_last_o  = s_bl[4];
  assign f_beat_last_o  = s_bl[5];
  assign a_beat_data_o  = s_bd;   // ONE bus; valid routes it
  assign b_beat_data_o  = s_bd;
  assign c_beat_data_o  = s_bd;
  assign d_beat_data_o  = s_bd;
  assign e_beat_data_o  = s_bd;
  assign f_beat_data_o  = s_bd;
  assign jobs_a_o = s_jobs[0];
  assign jobs_b_o = s_jobs[1];
  assign jobs_c_o = s_jobs[2];
  assign jobs_d_o = s_jobs[3];
  assign jobs_e_o = s_jobs[4];
  assign jobs_f_o = s_jobs[5];

  zhao_mem_share_n #(
    .N         (6),
    .CLIENT_ID (3),          // ZHAO_CLIENT_ENGINE1 -- see zhao_pkg
    .FORCE_READ(1'b1)        // the asset window is READ-ONLY by construction
  ) u_share (
    .clk  (clk),
    .rst_n(rst_n),

    .req_i       (s_req),
    .rsp_o       (s_rsp),
    .beat_valid_o(s_bv),
    .beat_data_o (s_bd),
    .beat_last_o (s_bl),

    .m_req_o       (m_req_o),
    .m_rsp_i       (m_rsp_i),
    .m_beat_valid_i(m_beat_valid_i),
    .m_beat_data_i (m_beat_data_i),
    .m_beat_last_i (m_beat_last_i),

    .jobs_o       (s_jobs),
    .denied_o     (denied_o),
    .contention_o (contention_o),
    .err_short_o  (err_short_o),
    .err_long_o   (err_long_o),
    .err_unowned_o(err_unowned_o)
  );
endmodule

`default_nettype wire
