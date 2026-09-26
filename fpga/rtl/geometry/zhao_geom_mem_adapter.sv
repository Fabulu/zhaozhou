// zhao_geom_mem_adapter.sv -- NINE logical ENGINE1 readers, one ENGINE1 client.
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
// below runs against the widened adapter with C held idle.  2026-09-26:
// requester I for TEXTURE.PALETTELOAD, by the same rule and with the same
// consequence -- every earlier requester keeps its port names and counters.)
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

    // Requester G, GEOM.POSE's kind-8 BODY and kind-9 CLIP FRAME page reader
    // (`zhao_geom_clipread`, core entry I29). 64-byte lines out of the SAME
    // asset pool under the SAME client in the SAME direction as the six above,
    // which is spec/memory_rules.md 5f's condition for joining here.
    //
    // ITS TRAFFIC IS PER PUBLICATION AND PER POSED DRAW, not per meshlet and
    // not per vertex: a kind-8 adoption is 2 + bone_count lines, a kind-9
    // adoption is 1 + ceil(clip_count/2) lines, and a FRAME is 1 + ceil(
    // bone_count/8) lines -- at 32 bones, six lines for the frame every posed
    // draw. That is heavier than E and F and lighter than A, and it is the
    // number any decision to give the pose path its own window has to beat.
    //
    // THE ROUND ROBIN'S BOUND IS N-1 TURNS AND IT IS RE-PROVED AT SEVEN RATHER
    // THAN ASSUMED, the same way it was re-proved at six: `zhao_mem_share_n`'s
    // own directed test carries the proof of the law and
    // `geom_mem_adapter_directed` runs against the widened adapter.
    //
    // `g_beat_last_o` IS NOT BROUGHT OUT. The reader counts its own eight
    // beats per 64-byte line and judges completeness from that count, exactly
    // as F's bank does, so a `last` it never reads would be a port nothing
    // drives a decision from. The share still produces it internally; what is
    // absent is the wire, not the signal.
    input  var zhao_guard_req_t g_req_i,
    output var zhao_guard_rsp_t g_rsp_o,
    output var logic            g_beat_valid_o,
    output var logic [63:0]     g_beat_data_o,

    // Requester H, GEOM.LADDERBANK's kind-8 CREATURE_FORM page reader
    // (`zhao_geom_ladderbank`, FORGE.SHADOW's ladder rows). 64-byte lines out
    // of the SAME asset pool under the SAME client in the SAME direction as the
    // seven above, which is spec/memory_rules.md 5f's condition for joining
    // here rather than taking a second ENGINE1 path.
    //
    // ITS TRAFFIC IS PER PUBLICATION AND NOTHING ELSE: a kind-8 CREATURE_FORM
    // publication is adopted whole, once, into ROWS registers, and every ladder
    // LOOKUP after that is a register read with no memory traffic at all. That
    // makes it the LIGHTEST requester on this share by a wide margin -- lighter
    // than E and F, which are per draw -- and it is the number any decision to
    // give the ladder its own window has to beat.
    //
    // THE ROUND ROBIN'S BOUND IS N-1 TURNS AND IT IS RE-PROVED AT EIGHT RATHER
    // THAN ASSUMED, the same way it was re-proved at six and at seven:
    // `zhao_mem_share_n`'s own directed test carries the proof of the law and
    // `geom_mem_adapter_directed` runs against the widened adapter.
    //
    // `h_beat_last_o` IS NOT BROUGHT OUT, for G's reason: the bank counts its
    // own eight beats per 64-byte line and judges completeness from that count,
    // so a `last` it never reads would be a port nothing drives a decision
    // from. The share still produces it internally; what is absent is the wire.
    input  var zhao_guard_req_t h_req_i,
    output var zhao_guard_rsp_t h_rsp_o,
    output var logic            h_beat_valid_o,
    output var logic [63:0]     h_beat_data_o,

    // Requester I, TEXTURE.PALETTELOAD's CLUT palette reader
    // (`zhao_texture_palette_load`).  The SAME asset pool, the SAME client, the
    // SAME read-only direction as the eight above, which is
    // spec/memory_rules.md 5f's condition for joining here rather than opening
    // a second ENGINE1 path.
    //
    // IT IS THE SHORTEST-BURST REQUESTER ON THIS SHARE and that is deliberate
    // rather than accidental: it asks for EIGHT BYTES at a time, sixty-four
    // times per palette, because its consumer -- the palette RAM's
    // BEGIN/WRITE/END port -- takes one 16-bit entry per cycle and
    // `mem_rsp_valid_i` cannot be stalled.  A 64-byte line would buy latency
    // nobody needs at the price of a 512-bit shadow register; that reasoning
    // is in `zhao_texture_palette_load.sv`'s header.  The consequence for
    // THIS block is the one that matters here: I's turns are short, so the
    // round robin's N-1 bound costs the other eight requesters one eight-byte
    // read each rather than one 64-byte line.
    //
    // ITS TRAFFIC IS PER PALETTE RESIDENCY CHANGE, not per draw and not per
    // fragment: a resident palette answers from this block's tag with no
    // memory traffic at all.
    //
    // `i_beat_last_o` IS NOT BROUGHT OUT, for G's and H's reason: the reader
    // takes exactly one beat per request and judges completeness from its own
    // entry count, so a `last` it never reads would be a port nothing drives a
    // decision from.
    input  var zhao_guard_req_t i_req_i,
    output var zhao_guard_rsp_t i_rsp_o,
    output var logic            i_beat_valid_o,
    output var logic [63:0]     i_beat_data_o,

    // Requester J, TERRAIN.NORMALMAP's DETAIL_NORMAL page loader (NORMALMAP,
    // 2026-09-26). ONE page per PUBLICATION and nothing per frame -- the
    // detail pyramid is resident in M10K once loaded -- so this is the
    // lightest traffic on the share after H, on the same pool, client and
    // direction as the nine above, which is 5f's condition for joining here
    // rather than opening a second ENGINE1 path. `j_beat_last_o` is not
    // brought out for G's, H's and I's reason: the loader counts its own
    // eight beats a line.
    input  var zhao_guard_req_t j_req_i,
    output var zhao_guard_rsp_t j_rsp_o,
    output var logic            j_beat_valid_o,
    output var logic [63:0]     j_beat_data_o,

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
    output var logic [31:0]     jobs_g_o,          // ...and G
    output var logic [31:0]     jobs_h_o,          // ...and H
    output var logic [31:0]     jobs_i_o,          // ...and I
    output var logic [31:0]     jobs_j_o,          // ...and J
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
  // THE N-REQUESTER CORE (902949ea), at N=9 since requester I landed 2026-09-26
  // for TEXTURE.PALETTELOAD's CLUT palette reader; at N=8 since requester H landed 2026-09-23
  // for GEOM.LADDERBANK's creature-form page reader; at N=7 since G landed 2026-09-22
  // for GEOM.POSE's page reader (F landed 2026-09-21, C on 2026-09-19). Its
  // round robin is bounded at N-1 turns, so C's addition lengthens A's and B's
  // worst-case wait by at most one 32-byte record -- and it is the SAME core the
  // two-port `zhao_mem_share2` instantiates, so nothing about the guard's
  // two-cycle verdict law is re-derived here.
  zhao_guard_req_t [9:0] s_req;
  zhao_guard_rsp_t [9:0] s_rsp;
  logic            [9:0] s_bv;
  // G, H and I take no `last`, so bits 6, 7 and 8 of this vector are driven by
  // the share and read by nothing. Named UNUSED here rather than left for the
  // linter to find.
  /* verilator lint_off UNUSEDSIGNAL */
  logic            [9:0] s_bl;
  /* verilator lint_on UNUSEDSIGNAL */
  logic           [63:0] s_bd;
  logic      [9:0][31:0] s_jobs;

  assign s_req[0] = a_req_i;
  assign s_req[1] = b_req_i;
  assign s_req[2] = c_req_i;
  assign s_req[3] = d_req_i;
  assign s_req[4] = e_req_i;
  assign s_req[5] = f_req_i;
  assign s_req[6] = g_req_i;
  assign s_req[7] = h_req_i;
  assign s_req[8] = i_req_i;
  assign s_req[9] = j_req_i;
  assign a_rsp_o = s_rsp[0];
  assign b_rsp_o = s_rsp[1];
  assign c_rsp_o = s_rsp[2];
  assign d_rsp_o = s_rsp[3];
  assign e_rsp_o = s_rsp[4];
  assign f_rsp_o = s_rsp[5];
  assign g_rsp_o = s_rsp[6];
  assign h_rsp_o = s_rsp[7];
  assign i_rsp_o = s_rsp[8];
  assign j_rsp_o = s_rsp[9];
  assign a_beat_valid_o = s_bv[0];
  assign b_beat_valid_o = s_bv[1];
  assign c_beat_valid_o = s_bv[2];
  assign d_beat_valid_o = s_bv[3];
  assign e_beat_valid_o = s_bv[4];
  assign f_beat_valid_o = s_bv[5];
  assign g_beat_valid_o = s_bv[6];
  assign h_beat_valid_o = s_bv[7];
  assign i_beat_valid_o = s_bv[8];
  assign j_beat_valid_o = s_bv[9];
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
  assign g_beat_data_o  = s_bd;
  assign h_beat_data_o  = s_bd;
  assign i_beat_data_o  = s_bd;
  assign j_beat_data_o  = s_bd;
  assign jobs_a_o = s_jobs[0];
  assign jobs_b_o = s_jobs[1];
  assign jobs_c_o = s_jobs[2];
  assign jobs_d_o = s_jobs[3];
  assign jobs_e_o = s_jobs[4];
  assign jobs_f_o = s_jobs[5];
  assign jobs_g_o = s_jobs[6];
  assign jobs_h_o = s_jobs[7];
  assign jobs_i_o = s_jobs[8];
  assign jobs_j_o = s_jobs[9];

  zhao_mem_share_n #(
    .N         (10),
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
