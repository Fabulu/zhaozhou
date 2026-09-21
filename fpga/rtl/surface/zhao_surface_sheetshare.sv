// zhao_surface_sheetshare.sv -- SURFACE.SHEET'S TWO-CLIENT REQUEST SHARE.
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
    output var logic        owner_o,  // 0 = A, 1 = B; read while busy_o
    output var logic [31:0] a_reqs_o,
    output var logic [31:0] b_reqs_o,
    // A response arrived with NO request outstanding. Compares the store's own
    // response register against our request handshake -- two different clock
    // enables, which is the whole point (CLAUDE.md, the lockstep chapter).
    output var logic [31:0] pg_orphan_o,
    // The store's opcode echo disagreed with the opcode we sent for the
    // outstanding request. A third quantity, independent of both of the above.
    output var logic [31:0] pg_op_mismatch_o
);

  logic busy_q;
  logic owner_q;  // 0 = A, 1 = B
  logic last_q;  // who was granted last; 0 = A, 1 = B
  logic [1:0] op_q;  // the opcode of the outstanding request

  // ---- the grant ----------------------------------------------------------
  // While a transaction is outstanding nothing may be offered: the store can
  // hold exactly one response, so a second request would have to wait on
  // `s_req_ready_i` anyway, and offering one would make the captured owner
  // ambiguous for a cycle.
  //
  // While idle: both asking -> the turn goes to !last_q; otherwise whoever
  // asks. One flip-flop, and starvation is structurally impossible.
  logic sel_b_c;
  logic grant_c;
  logic retire_c;
  logic can_grant_c;

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
    if (a_req_valid_i && b_req_valid_i) begin
      sel_b_c = !last_q;
    end else begin
      sel_b_c = b_req_valid_i;
    end
    grant_c = can_grant_c && (sel_b_c ? b_req_valid_i : a_req_valid_i);
  end

  assign s_req_valid_o  = grant_c;
  assign s_req_op_o     = sel_b_c ? b_req_op_i : a_req_op_i;
  assign s_req_handle_o = sel_b_c ? b_req_handle_i : a_req_handle_i;
  assign s_req_texel_o  = sel_b_c ? b_req_texel_i : a_req_texel_i;
  assign s_req_src_id_o = sel_b_c ? b_req_src_id_i : a_req_src_id_i;

  assign a_req_ready_o  = grant_c && !sel_b_c && s_req_ready_i;
  assign b_req_ready_o  = grant_c && sel_b_c && s_req_ready_i;

  // ---- the response, demuxed by the captured owner ------------------------
  // THE DATA WIRES ARE BROADCAST (psmux's rule): `pg_status_o`, `pg_tag_o`,
  // `pg_strength_o` and `pg_src_id_o` go to both clients unchanged and each
  // reads them in the cycles its own `valid` is high. Nothing is copied.
  assign a_pg_valid_o   = s_pg_valid_i && busy_q && !owner_q;
  assign b_pg_valid_o   = s_pg_valid_i && busy_q && owner_q;
  assign s_pg_ready_o   = busy_q ? (owner_q ? b_pg_ready_i : a_pg_ready_i) : 1'b1;

  assign busy_o         = busy_q;
  assign owner_o        = owner_q;

  function automatic logic [31:0] sat_inc(input logic [31:0] a);
    sat_inc = (a == 32'hFFFF_FFFF) ? a : a + 32'd1;
  endfunction

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      busy_q <= 1'b0;
      owner_q <= 1'b0;
      last_q <= 1'b0;
      op_q <= 2'd0;
      a_reqs_o <= 32'd0;
      b_reqs_o <= 32'd0;
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
        owner_q <= sel_b_c;
        last_q  <= sel_b_c;
        op_q    <= sel_b_c ? b_req_op_i : a_req_op_i;
        if (sel_b_c) b_reqs_o <= sat_inc(b_reqs_o);
        else a_reqs_o <= sat_inc(a_reqs_o);
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
