// zhao_texture_rsp_dispatch.sv — the cache-response FIFO and decode dispatcher.
//
// BESIDE the current TMU. Nothing instantiates it yet.
//
// ---------------------------------------------------------------------------
// THE COUPLING THIS REMOVES, NAMED BY THE BRIEF
// ---------------------------------------------------------------------------
// REARCHITECTUREADVICE.md:
//
//   > Place a raw-response FIFO of at least four entries after the cache.
//   > A decode dispatcher routes each response to: CLUT-index/palette path;
//   > direct-nearest path; bilinear-footprint FIFO.
//   >
//   > The current global relationship resembling
//   >     cache response ready = filter not busy
//   > must disappear. A busy bilinear channel lane must not stop an unrelated
//   > nearest or CLUT response from entering local response storage.
//
// In `zhao_texture_tmu_pipe.sv` that relationship is literal:
//
//     assign cac_ready_o = !fl_v;
//
// One busy filter lane refuses every response, including the ones that have
// nothing to do with filtering. A CLUT index and a direct-nearest texel both
// wait on a bilinear job they share no resource with.
//
// ---------------------------------------------------------------------------
// WHAT MAKES THIS CORRECT RATHER THAN JUST DECOUPLED
// ---------------------------------------------------------------------------
// Splitting one stream into three is easy; splitting it without reordering
// within a class, and without one full class silently dropping responses, is
// the part worth testing. So:
//
//   * the raw FIFO absorbs the cache's bursts and is the ONLY thing
//     `rsp_ready_o` depends on -- never a downstream channel's ready;
//   * each class has its own small queue, and a full one stalls ONLY its own
//     class at the dispatch head;
//   * order WITHIN a class is preserved, because that is what the ROB
//     downstream relies on. Order ACROSS classes is not, and must not be:
//     preserving it is exactly the coupling being removed.
//
// The head-of-line case is the honest limit: if the class queue at the FIFO
// head is full, dispatch stalls even though another class could proceed. That
// is a bounded, visible cost (`hol_stall_o` counts it) rather than a hidden
// one, and fixing it needs per-class pre-sorting the brief does not ask for.
//
// ---------------------------------------------------------------------------
// DEFECT (b), REPAIRED 2026-09-06: AN UNKNOWN CLASS WAS POPPED AND DESTROYED
// ---------------------------------------------------------------------------
// THE DEFECT. The `default:` arm of the head-room decode read
//
//     default: head_room = 1'b1;   // an unknown class is dropped, not stalled
//
// and the comment was an accurate description of a wrong behaviour. With
// head_room forced high, `dispatch_fire` popped the raw FIFO, and the per-class
// push loop below matched no queue at all because it only ran i = 0..2. The
// response evaporated.
//
// THE EVIDENCE THAT IT WAS INVISIBLE. `accepted_o` counts RAW PUSHES, not
// dispatches, so a destroyed response still incremented it. No counter, no
// sticky, nothing in the evidence ports moved. The only observable was the
// hang, three blocks downstream.
//
// THE CONSEQUENCE, AND WHY IT IS A DEADLOCK RATHER THAN A WRONG PIXEL.
// `zhao_texture_fragrob.sv:470-472` retires on
//
//     head_done_c = val_q[head] && (arr_q[head] == req_q[head]) && ...
//
// -- arrivals EXACTLY equalling requests, with no timeout, no watchdog and no
// error completion anywhere in that file. Retirement is also strictly
// ALLOCATION ORDERED (:422-444). So a fragment whose sample was destroyed here
// never satisfies `head_done_c`, sits at the head forever, and every later
// fragment queues behind it. `live_cnt_q` saturates, `frag_ready_o` drops, and
// THE WHOLE ISLAND STOPS -- the "48 samples fetched, 0 fragments out" signature
// the island top's own header already names for an earlier aux-token bug.
//
// WHAT BREAKS WITHOUT THE FIX. One malformed class bit pair anywhere between
// the planner's source id and the cache's echo -- one bit flip, one id-space
// overflow into bits [15:14], one future block that forgets to sanitise --
// takes the island down permanently, silently, with every counter reading
// healthy.
//
// THE REPAIR. Class 3 gets its OWN queue and its own terminal output
// (`err_*`), exactly like the three legal classes, and `err_unknown_class_o`
// counts it. The response is no longer consumed without producing something:
// the token -- and therefore the owning FRAGROB slot, sample index and
// generation -- is RETAINED and handed to the island, which turns it into a
// counted error completion. The sample completes, the fragment retires, and
// the fault becomes a visible wrong pixel instead of a hang. That is the
// invariant from section 5A.7 of the pre-fit brief: every request accepted
// into the sample path produces EXACTLY ONE terminal completion carrying the
// identity it was issued under, and an error must be as retirable as a
// success.
//
// NOTE ON THE FULL-QUEUE CASE. An unknown class now stalls the head when its
// own queue is full, and `hol_stall_o` counts that like any other class. That
// is correct: a full error queue means the island is not draining errors, and
// stalling visibly is better than resuming the destruction this repair exists
// to remove. The island ties the error drain to FRAGROB's response port, which
// is unconditionally ready (`fragrob:395`), so the queue drains one per clock.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_texture_rsp_dispatch #(
    parameter int unsigned RAWN  = 4,   // brief: "at least four entries"
    parameter int unsigned CHN   = 4,   // per-class queue depth
    parameter int unsigned DATAW = 64,
    parameter int unsigned TOKW  = 16
) (
    input var logic clk,
    input var logic rst_n,

    // ---- raw responses from the cache ---------------------------------------
    input  var logic             rsp_valid_i,
    output var logic             rsp_ready_o,
    input  var logic [DATAW-1:0] rsp_data_i,
    input  var logic [TOKW-1:0]  rsp_tok_i,
    // The class is decided upstream (it follows from the request's format and
    // filter bits, which T1 already sanitised). Re-deriving it here would put
    // mode decode on the response path for no reason.
    input  var logic [1:0]       rsp_class_i,   // see CLS_*

    // ---- CLUT index / palette path ------------------------------------------
    output var logic             clut_valid_o,
    input  var logic             clut_ready_i,
    output var logic [DATAW-1:0] clut_data_o,
    output var logic [TOKW-1:0]  clut_tok_o,

    // ---- direct nearest path -------------------------------------------------
    output var logic             near_valid_o,
    input  var logic             near_ready_i,
    output var logic [DATAW-1:0] near_data_o,
    output var logic [TOKW-1:0]  near_tok_o,

    // ---- bilinear footprint FIFO --------------------------------------------
    output var logic             bil_valid_o,
    input  var logic             bil_ready_i,
    output var logic [DATAW-1:0] bil_data_o,
    output var logic [TOKW-1:0]  bil_tok_o,

    // ---- unknown class: a TERMINAL ERROR path, not a hole -------------------
    // This carries the response whose class was not one of {CLUT, NEAR, BIL}.
    // It exists so the token survives: the caller must be able to complete the
    // owning sample with an error rather than leave it un-arrived forever. The
    // DATA is forwarded too, purely so a diagnostic can see what came back; it
    // is not decodable, because the thing that was wrong is the routing.
    output var logic             err_valid_o,
    input  var logic             err_ready_i,
    output var logic [DATAW-1:0] err_data_o,
    output var logic [TOKW-1:0]  err_tok_o,

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]      accepted_o,
    output var logic [31:0]      hol_stall_o,   // dispatch blocked by ONE class
    // Responses dispatched to the error path because their class was 3. Was
    // ZERO OBSERVABLE before this repair: `accepted_o` counts raw pushes, so a
    // destroyed response still incremented it and nothing else moved.
    output var logic [31:0]      err_unknown_class_o,
    output var logic [2:0]       occupancy_o
);

  localparam logic [1:0] CLS_CLUT = 2'd0;
  localparam logic [1:0] CLS_NEAR = 2'd1;
  localparam logic [1:0] CLS_BIL  = 2'd2;
  // NOT A LEGAL REQUEST CLASS. It is the encoding no upstream may produce, and
  // it is given a queue here precisely so that its arrival is survivable.
  localparam logic [1:0] CLS_ERR  = 2'd3;
  localparam int NCLS = 4;

  localparam int RW = $clog2(RAWN);
  localparam int CW = $clog2(CHN);

  // ---- the raw FIFO --------------------------------------------------------
  logic [DATAW-1:0] raw_d [RAWN];
  logic [TOKW-1:0]  raw_t [RAWN];
  logic [1:0]       raw_c [RAWN];
  logic [RW-1:0]    raw_wp, raw_rp;
  logic [RW:0]      raw_n;

  // RSP_READY DEPENDS ON THE RAW FIFO ALONE. This single line is the defect
  // being removed: it must never mention clut_ready_i, near_ready_i or
  // bil_ready_i.
  assign rsp_ready_o = (raw_n != (RW + 1)'(RAWN));

  // ---- per-class queues ----------------------------------------------------
  // FOUR independent little FIFOs -- three legal classes and the error class.
  // A full one stalls its own class only.
  logic [DATAW-1:0] cq_d [NCLS][CHN];
  logic [TOKW-1:0]  cq_t [NCLS][CHN];
  logic [CW-1:0]    cq_wp [NCLS], cq_rp [NCLS];
  logic [CW:0]      cq_n [NCLS];

  logic [1:0] head_cls;
  logic       head_v;
  assign head_v   = (raw_n != '0);
  assign head_cls = raw_c[raw_rp];

  // Can the head be dispatched? Only its OWN class's queue matters.
  logic head_room;
  always_comb begin
    head_room = 1'b0;
    case (head_cls)
      CLS_CLUT: head_room = (cq_n[0] != (CW + 1)'(CHN));
      CLS_NEAR: head_room = (cq_n[1] != (CW + 1)'(CHN));
      CLS_BIL:  head_room = (cq_n[2] != (CW + 1)'(CHN));
      // DEFECT (b). This was `head_room = 1'b1` -- "an unknown class is
      // dropped, not stalled". It popped the raw FIFO into nothing, the
      // owning sample never arrived at FRAGROB, and because FRAGROB retires
      // in allocation order with no timeout, the island deadlocked. The
      // unknown class now queues like any other and leaves through `err_*`.
      default:  head_room = (cq_n[3] != (CW + 1)'(CHN));
    endcase
  end

  logic dispatch_fire;
  assign dispatch_fire = head_v && head_room;

  assign clut_valid_o = (cq_n[0] != '0);
  assign clut_data_o  = cq_d[0][cq_rp[0]];
  assign clut_tok_o   = cq_t[0][cq_rp[0]];
  assign near_valid_o = (cq_n[1] != '0);
  assign near_data_o  = cq_d[1][cq_rp[1]];
  assign near_tok_o   = cq_t[1][cq_rp[1]];
  assign bil_valid_o  = (cq_n[2] != '0);
  assign bil_data_o   = cq_d[2][cq_rp[2]];
  assign bil_tok_o    = cq_t[2][cq_rp[2]];
  assign err_valid_o  = (cq_n[3] != '0);
  assign err_data_o   = cq_d[3][cq_rp[3]];
  assign err_tok_o    = cq_t[3][cq_rp[3]];

  assign occupancy_o = 3'(raw_n);

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      raw_wp <= '0; raw_rp <= '0; raw_n <= '0;
      accepted_o <= 32'd0;
      hol_stall_o <= 32'd0;
      err_unknown_class_o <= 32'd0;
      for (int i = 0; i < NCLS; i++) begin
        cq_wp[i] <= '0; cq_rp[i] <= '0; cq_n[i] <= '0;
      end
    end else begin
      // The raw FIFO's own count moves ONCE. Push and pop can coincide, and
      // two non-blocking assignments to raw_n would lose one of them -- the
      // same fault the perspective lane's free counter had, and it only shows
      // up when the downstream stalls.
      begin
        automatic logic psh = rsp_valid_i && rsp_ready_o;
        automatic logic pop = dispatch_fire;
        if (psh && !pop)      raw_n <= raw_n + 1'b1;
        else if (!psh && pop) raw_n <= raw_n - 1'b1;

        if (psh) begin
          raw_d[raw_wp] <= rsp_data_i;
          raw_t[raw_wp] <= rsp_tok_i;
          raw_c[raw_wp] <= rsp_class_i;
          raw_wp <= (raw_wp == RW'(RAWN - 1)) ? '0 : raw_wp + RW'(1);
          accepted_o <= accepted_o + 32'd1;
        end
        if (pop) raw_rp <= (raw_rp == RW'(RAWN - 1)) ? '0 : raw_rp + RW'(1);
      end

      // Head-of-line: a full class queue holds up a head that another class
      // could have taken. Counted rather than hidden.
      if (head_v && !head_room) hol_stall_o <= hol_stall_o + 32'd1;

      // DEFECT (b) EVIDENCE. Counted at DISPATCH, not at push, because the
      // push counter (`accepted_o`) is exactly the one that made the old
      // destruction invisible.
      if (dispatch_fire && (head_cls == CLS_ERR))
        err_unknown_class_o <= err_unknown_class_o + 32'd1;

      // ---- per-class push and pop, each counted once ---------------------
      for (int i = 0; i < NCLS; i++) begin
        automatic logic cpsh = dispatch_fire && (head_cls == 2'(i));
        automatic logic cpop = (i == 0) ? (clut_valid_o && clut_ready_i)
                             : (i == 1) ? (near_valid_o && near_ready_i)
                             : (i == 2) ? (bil_valid_o  && bil_ready_i)
                                        : (err_valid_o  && err_ready_i);
        if (cpsh && !cpop)      cq_n[i] <= cq_n[i] + 1'b1;
        else if (!cpsh && cpop) cq_n[i] <= cq_n[i] - 1'b1;

        if (cpsh) begin
          cq_d[i][cq_wp[i]] <= raw_d[raw_rp];
          cq_t[i][cq_wp[i]] <= raw_t[raw_rp];
          cq_wp[i] <= (cq_wp[i] == CW'(CHN - 1)) ? '0 : cq_wp[i] + CW'(1);
        end
        if (cpop) cq_rp[i] <= (cq_rp[i] == CW'(CHN - 1)) ? '0 : cq_rp[i] + CW'(1);
      end
    end
  end

endmodule : zhao_texture_rsp_dispatch

`default_nettype wire
