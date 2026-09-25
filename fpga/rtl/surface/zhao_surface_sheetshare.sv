// zhao_surface_sheetshare.sv -- SURFACE.SHEET'S THREE-CLIENT REQUEST SHARE.
//
// WIDENED 2 -> 3 ON 2026-09-25 (TERRAINAUX). The third client is the composed
// texture island's AUX pipe -- `zhao_texture_aux_pipe_v2` inside
// `zhao_texture_island_v3_top`, whose Sheet READ port left `zhao_console_core`
// as a dangling top-level output group while its RESPONSE was TIED TO ZERO
// inside `zhao_shell_top_v2` ("TIE: page-generation residency is its own clause
// and its own packet; no producer exists in this shell yet"). The store it
// needed was composed in the same module the whole time. Closing that loop is
// what this client is for, and the ROUND ROBIN BELOW IS UNCHANGED IN KIND --
// see THE ARBITRATION LAW section for why a third idle client changes nothing
// about the other two, and why a third BUSY one is still bounded at one beat.
//
// The fragment path is the one client that CANNOT be made to wait indefinitely:
// `zhao_texture_aux_pipe_v2` holds a credit for the whole accepted lifetime, so
// a starved AUX read is a fragment that never retires. Round robin is therefore
// not merely fair here, it is the correctness argument -- which is the same
// reason the two-client version rejected both strict priorities below.
//
// ---------------------------------------------------------------------------
// WHY THIS BLOCK EXISTS
// ---------------------------------------------------------------------------
// `zhao_surface_sheet` has exactly ONE control-and-read port (`req_*` out,
// `pg_*` back).  `zhao_console_core.sv` annotates its instance
// "REAL: SURFACE.STAMP is the only requester", and entry I32 in that file says
// what happens when a second one appears:
//
//     "Whatever reads layer F for the dig is its SECOND requester, and
//      choosing between them is a scheduler, which a composer may not write."
//
// and, narrowed by the terrain7 packet,
//
//     "arbitration is state, state belongs in a file with a contract and a
//      test, and a mux written inline in a composer is an arbiter nobody can
//      point at".
//
// This is that file.  The second requester is `zhao_terrain_sheetseam`, the
// layer-F prefetch that serves `zhao_terrain_bake_v2`'s per-vertex depth mode
// (owner rulings R194 and R221).
//
// ---------------------------------------------------------------------------
// WHY NEITHER EXISTING SHARE DROPS IN, CHECKED RATHER THAN ASSERTED
// ---------------------------------------------------------------------------
// `zhao_terrain_psmux` shares a JOB/VERTEX/COMPLETION triple of
// `{slot, gen, epoch, src_id, flags}`; `zhao_mem_share_n` (which lives inside
// `fpga/rtl/memory/zhao_mem_share2.sv`, NOT in a file of its own -- SEAMDIG
// recorded that trap after `find -name zhao_mem_share_n.sv` came back empty)
// shares a guard socket.  This port's type is
// `{op[1:0], handle[31:0], texel[11:0], src_id[15:0]}` out and
// `{op[1:0], status[1:0], tag[7:0], strength[7:0], src_id[15:0]}` back.
// Neither existing block carries it, which `zhao_console_core.sv` already
// says.  What IS reused is psmux's STRUCTURE and its POLICY, line for line --
// see below -- because re-deciding a ratified arbitration rule is how a
// second, subtly different answer to one question gets written.
//
// ---------------------------------------------------------------------------
// THE POLICY IS NOT A NEW DECISION: IT IS psmux's, AND HERE IS WHY IT FITS
// ---------------------------------------------------------------------------
// ROUND ROBIN, one `last_q` flip-flop, exactly as `zhao_terrain_psmux`:
// "when both ask, the turn goes to whoever was NOT granted last ... it makes
// starvation structurally impossible in either direction.  When only one asks
// it takes the streamer immediately, so the fair rule costs nothing when there
// is no contention."
//
// Strict priority was considered and REJECTED in both directions, and the
// numbers are on the record rather than in an argument:
//
//   * PRIORITY TO THE STAMP.  `design/blocks.yml` gives SURFACE.STAMP
//     "20,000 stamp texels per frame (one texel per ~83 clocks)", so the stamp
//     uses about 1.2 % of this port and the prefetch would still complete in
//     ~1,102 cycles instead of 1,089.  It is nearly free -- and it is unbounded
//     in the only case that matters, a burst that saturates, and an unbounded
//     wait on a bake is a frame that does not finish.
//   * PRIORITY TO THE BAKE.  A 1,089-beat prefetch would lock the stamp out
//     for 1,089 consecutive cycles.  The stamp is a read-modify-write engine
//     on the player's own action.
//
// Round robin bounds BOTH at one beat, so neither number has to be trusted.
//
// GRANULARITY IS ONE TRANSACTION, not one job, and that is the substantive
// difference from psmux.  `zhao_surface_sheet` accepts a new request only when
// the previous response is being drained (`req_ready_o = !clr_active &&
// !pend_valid && pg_slot_free`), so exactly one transaction is outstanding at
// a time and the response stream is strictly in issue order.  `busy_q` is
// therefore ONE TRANSACTION, the arbiter re-arbitrates every beat, and the
// bake's 1,089-beat prefetch interleaves with the stamp at single-beat
// granularity instead of holding the port.
//
// ---------------------------------------------------------------------------
// THE DEMUX IS KEYED ON A CAPTURED OWNER, AND THAT IS THE DANGEROUS SHAPE
// ---------------------------------------------------------------------------
// `pg_*` carries no client id, so the response must be routed by state this
// block latched.  CLAUDE.md's "a detector wired to two operands that move
// together cannot fire" is about exactly this, so the detector here is
// deliberately NOT a difference of two things this block latched together:
// `pg_orphan_o` compares the SHEET'S OWN `pg_valid_i` against `busy_q`.  One
// side is the store's internal response register, the other is our request
// handshake; they are clocked by different events, so the comparison can see a
// timing fault and not only a value fault.
//
// A SECOND, INDEPENDENT CHECK IS AVAILABLE AND IS TAKEN: the sheet echoes the
// request's opcode on `pg_op_o`.  `pg_op_mismatch_o` differences that echo
// against the opcode WE sent, which is a third quantity again -- so a response
// delivered to the wrong owner after a re-order would have to agree with both
// the busy bit and the opcode echo to stay invisible.
//
// `pg_ready_o` is HIGH while idle, deliberately, following psmux: a stray
// response is consumed and COUNTED rather than left to wedge the store for
// ever, because a silent stall is worse evidence than a counted anomaly.
//
// ---------------------------------------------------------------------------
// THE GRANT READS THE CLIENTS' `valid` COMBINATIONALLY, AND BOTH WERE CHECKED
// ---------------------------------------------------------------------------
// That is ordinary for an arbiter and it makes each client's `ready` a
// function of its own `valid`, which is safe only while neither client derives
// `valid` from `ready`. `zhao_terrain_psmux` states the same requirement and
// names its two clients; this one names its own, VERIFIED rather than
// inherited:
//
//   * CLIENT A, `zhao_surface_stamp`: `req_valid_o` is
//     `(acq_valid && !acq_sent) || (cursor_slot && geom_ready && fld_ok &&
//     covered && s1_free_next)`. Its only uses of `req_ready_i` are
//     `read_path_ok` -- which feeds `fld_ready_o` and `advance`, NOT
//     `req_valid_o` -- and the `acq_sent` latch. So `req_valid_o` is a pure
//     function of registered state.
//   * CLIENT B, `zhao_terrain_sheetseam`: `req_valid_o` is
//     `(state_q == S_FILL) && !pf_missed_q && (i_idx_q != Texels)`, all
//     registers.
//
// And if one ever did, Verilator's UNOPTFLAT would say so rather than the
// design quietly oscillating.
//
// WHAT THIS BLOCK IS NOT.  It does not touch the sheet's WRITE port (`wr_*`).
// terrain_rules 7 says layer F is written by SURFACE.STAMP and by nothing
// else, and the store's write port is physically separate from its request
// port (`zhao_surface_sheet` C5: "THE READ PORT AND THE WRITE PORT ARE
// SEPARATE"), so a stamp's writes are never delayed by a bake's reads and
// there is nothing here to arbitrate.  It buffers nothing, reorders nothing
// and renames nothing.
//
// Conservative SystemVerilog subset only (charter S2); no package deps.

module zhao_surface_sheetshare (
    input var logic clk,
    input var logic rst_n,

    // -----------------------------------------------------------------------
    // CLIENT A -- SURFACE.STAMP, the historical sole requester
    // -----------------------------------------------------------------------
    input  var logic        a_req_valid_i,
    output var logic        a_req_ready_o,
    input  var logic [ 1:0] a_req_op_i,
    input  var logic [31:0] a_req_handle_i,
    input  var logic [11:0] a_req_texel_i,
    input  var logic [15:0] a_req_src_id_i,

    output var logic        a_pg_valid_o,
    input  var logic        a_pg_ready_i,

    // -----------------------------------------------------------------------
    // CLIENT B -- TERRAIN.SHEETSEAM, the bake's layer-F prefetch
    // -----------------------------------------------------------------------
    input  var logic        b_req_valid_i,
    output var logic        b_req_ready_o,
    input  var logic [ 1:0] b_req_op_i,
    input  var logic [31:0] b_req_handle_i,
    input  var logic [11:0] b_req_texel_i,
    input  var logic [15:0] b_req_src_id_i,

    output var logic        b_pg_valid_o,
    input  var logic        b_pg_ready_i,

    // -----------------------------------------------------------------------
    // CLIENT C -- TEXTURE.AUX.V2, the FRAGMENT path's layer-F read (2026-09-25)
    // -----------------------------------------------------------------------
    // `zhao_texture_aux_pipe_v2`, inside the composed texture island. OP_READ
    // only, exactly like client B: the island has no write path to layer F and
    // terrain_rules 7 gives that to SURFACE.STAMP alone. Its `src_id` is the
    // island's OWNER tag, not a patch id, and this block neither reads nor
    // interprets it -- the AUX pipe checks its own echo (`sheet_rsp_wrong_src_o`)
    // and that check is a THIRD independent quantity beside this block's own
    // `pg_orphan_o` and `pg_op_mismatch_o`.
    input  var logic        c_req_valid_i,
    output var logic        c_req_ready_o,
    input  var logic [ 1:0] c_req_op_i,
    input  var logic [31:0] c_req_handle_i,
    input  var logic [11:0] c_req_texel_i,
    input  var logic [15:0] c_req_src_id_i,

    output var logic        c_pg_valid_o,
    input  var logic        c_pg_ready_i,

    // -----------------------------------------------------------------------
    // THE SHARED zhao_surface_sheet REQUEST PORT
    // -----------------------------------------------------------------------
    output var logic        s_req_valid_o,
    input  var logic        s_req_ready_i,
    output var logic [ 1:0] s_req_op_o,
    output var logic [31:0] s_req_handle_o,
    output var logic [11:0] s_req_texel_o,
    output var logic [15:0] s_req_src_id_o,

    input  var logic        s_pg_valid_i,
    output var logic        s_pg_ready_o,
    input  var logic [ 1:0] s_pg_op_i,

    // -----------------------------------------------------------------------
    // evidence (spec/counters.md S4: saturate, never wrap)
    // -----------------------------------------------------------------------
    output var logic        busy_o,
    // 0 = A, 1 = B, 2 = C; read while busy_o. WIDENED 1 -> 2 bits 2026-09-25
    // with the third client; 3 is never produced.
    output var logic [ 1:0] owner_o,
    output var logic [31:0] a_reqs_o,
    output var logic [31:0] b_reqs_o,
    output var logic [31:0] c_reqs_o,
    // A response arrived with NO request outstanding. Compares the store's own
    // response register against our request handshake -- two different clock
    // enables, which is the whole point (CLAUDE.md, the lockstep chapter).
    output var logic [31:0] pg_orphan_o,
    // The store's opcode echo disagreed with the opcode we sent for the
    // outstanding request. A third quantity, independent of both of the above.
    output var logic [31:0] pg_op_mismatch_o
);

  logic busy_q;
  logic [1:0] owner_q;  // 0 = A, 1 = B, 2 = C
  logic [1:0] last_q;   // who was granted last
  logic [1:0] op_q;     // the opcode of the outstanding request

  // ---- the grant ----------------------------------------------------------
  // While a transaction is outstanding nothing may be offered: the store can
  // hold exactly one response, so a second request would have to wait on
  // `s_req_ready_i` anyway, and offering one would make the captured owner
  // ambiguous for a cycle.
  //
  // While idle: both asking -> the turn goes to !last_q; otherwise whoever
  // asks. One flip-flop, and starvation is structurally impossible.
  logic [1:0] sel_c;
  logic grant_c;
  logic retire_c;
  logic can_grant_c;
  logic [2:0] req_c;
  logic       any_c;

  assign req_c = {c_req_valid_i, b_req_valid_i, a_req_valid_i};
  assign any_c = |req_c;

  always_comb begin
    // The outstanding transaction retires the cycle its response is taken.
    retire_c = busy_q && s_pg_valid_i && s_pg_ready_o;
    // A NEW REQUEST MAY BE GRANTED IN THAT SAME CYCLE, and this line is worth
    // its own paragraph because leaving it out costs exactly half the
    // bandwidth of the port. `zhao_surface_sheet`'s `req_ready_o` gates on
    // `pg_slot_free = !pg_valid_q || pg_ready_i`, so the store ITSELF accepts
    // a request in the cycle it is handing back the previous response -- it
    // sustains one read per clock. An arbiter that waits for `!busy_q` turns
    // that into one read every TWO clocks, which doubles
    // `zhao_terrain_sheetseam`'s 1,089-beat prefetch to 2,178 cycles while
    // every handshake stays legal and every counter agrees. It was written
    // that way first and measured before it was believed.
    can_grant_c = !busy_q || retire_c;
    // THE TURN, generalised from the two-client `sel_b_c = !last_q`. It is a
    // PRIORITY WALK over the three clients starting one PAST the last grant,
    // written as two ascending passes rather than a modulo for the reason
    // `zhao_geom_clipdoor` records: an `integer` index trips UNUSEDSIGNAL on
    // its top 31 bits and waiving a warning to keep a modulo is a worse trade.
    //
    // IT REDUCES EXACTLY TO THE OLD LAW WHEN C NEVER ASKS, which is why the
    // two-client behaviour this block was measured on is preserved rather than
    // re-argued: last = A, both asking -> start at B -> B; last = B, both
    // asking -> start at C, C idle, wrap -> A. That is `!last_q`.
    sel_c = last_q;
    if (req_c[0] && (last_q == 2'd2)) sel_c = 2'd0;
    else if (req_c[1] && (last_q == 2'd0)) sel_c = 2'd1;
    else if (req_c[2] && (last_q == 2'd1)) sel_c = 2'd2;
    else if (req_c[1] && (last_q == 2'd2)) sel_c = 2'd1;
    else if (req_c[2] && (last_q == 2'd0)) sel_c = 2'd2;
    else if (req_c[0] && (last_q == 2'd1)) sel_c = 2'd0;
    else if (req_c[2] && (last_q == 2'd2)) sel_c = 2'd2;
    else if (req_c[0] && (last_q == 2'd0)) sel_c = 2'd0;
    else if (req_c[1] && (last_q == 2'd1)) sel_c = 2'd1;
    grant_c = can_grant_c && any_c;
  end

  assign s_req_valid_o  = grant_c;
  assign s_req_op_o     = (sel_c == 2'd0) ? a_req_op_i
                        : (sel_c == 2'd1) ? b_req_op_i : c_req_op_i;
  assign s_req_handle_o = (sel_c == 2'd0) ? a_req_handle_i
                        : (sel_c == 2'd1) ? b_req_handle_i : c_req_handle_i;
  assign s_req_texel_o  = (sel_c == 2'd0) ? a_req_texel_i
                        : (sel_c == 2'd1) ? b_req_texel_i : c_req_texel_i;
  assign s_req_src_id_o = (sel_c == 2'd0) ? a_req_src_id_i
                        : (sel_c == 2'd1) ? b_req_src_id_i : c_req_src_id_i;

  assign a_req_ready_o  = grant_c && (sel_c == 2'd0) && s_req_ready_i;
  assign b_req_ready_o  = grant_c && (sel_c == 2'd1) && s_req_ready_i;
  assign c_req_ready_o  = grant_c && (sel_c == 2'd2) && s_req_ready_i;

  // ---- the response, demuxed by the captured owner ------------------------
  // THE DATA WIRES ARE BROADCAST (psmux's rule): `pg_status_o`, `pg_tag_o`,
  // `pg_strength_o` and `pg_src_id_o` go to both clients unchanged and each
  // reads them in the cycles its own `valid` is high. Nothing is copied.
  assign a_pg_valid_o   = s_pg_valid_i && busy_q && (owner_q == 2'd0);
  assign b_pg_valid_o   = s_pg_valid_i && busy_q && (owner_q == 2'd1);
  assign c_pg_valid_o   = s_pg_valid_i && busy_q && (owner_q == 2'd2);
  assign s_pg_ready_o   = !busy_q ? 1'b1
                        : (owner_q == 2'd0) ? a_pg_ready_i
                        : (owner_q == 2'd1) ? b_pg_ready_i : c_pg_ready_i;

  assign busy_o         = busy_q;
  assign owner_o        = owner_q;

  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q <= 1'b0;
      owner_q <= 2'd0;
      last_q <= 2'd0;
      op_q <= 2'd0;
      a_reqs_o <= 32'd0;
      b_reqs_o <= 32'd0;
      c_reqs_o <= 32'd0;
      pg_orphan_o <= 32'd0;
      pg_op_mismatch_o <= 32'd0;
    end else begin
      // A grant and a retirement CAN coincide (see `can_grant_c`), and the
      // grant wins the state because `busy_q` simply stays high with a new
      // owner. The demux below reads the OLD `owner_q` in that cycle, which
      // is registered, so the retiring response still reaches the client that
      // asked for it.
      if (grant_c && s_req_ready_i) begin
        busy_q  <= 1'b1;
        owner_q <= sel_c;
        last_q  <= sel_c;
        op_q    <= s_req_op_o;
        if (sel_c == 2'd0) a_reqs_o <= sat_inc(a_reqs_o);
        else if (sel_c == 2'd1) b_reqs_o <= sat_inc(b_reqs_o);
        else c_reqs_o <= sat_inc(c_reqs_o);
      end else if (retire_c) begin
        busy_q <= 1'b0;
      end

      // COUNTED ON THE HANDSHAKE, NOT ON THE LEVEL. `s_pg_valid_i` is held
      // until its ready comes, so counting the level would count CYCLES OF
      // WAITING and read orders of magnitude high -- the defect
      // `zhao_console_core.sv` records at `terr_pl_slot_overflow_o`.
      // `s_pg_ready_o` is high while idle, so a stray response is consumed
      // once and counted once rather than wedging the store.
      if (s_pg_valid_i && s_pg_ready_o && !busy_q) pg_orphan_o <= sat_inc(pg_orphan_o);

      // The opcode echo, differenced against what WE sent. Independent of
      // `busy_q` and of the owner bit, so a response routed to the wrong
      // client after a re-order has to agree with three separate quantities
      // to stay invisible.
      if (retire_c && (s_pg_op_i != op_q)) pg_op_mismatch_o <= sat_inc(pg_op_mismatch_o);
    end
  end

endmodule : zhao_surface_sheetshare
