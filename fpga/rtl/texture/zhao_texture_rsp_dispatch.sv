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

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0]      accepted_o,
    output var logic [31:0]      hol_stall_o,   // dispatch blocked by ONE class
    output var logic [2:0]       occupancy_o
);

  localparam logic [1:0] CLS_CLUT = 2'd0;
  localparam logic [1:0] CLS_NEAR = 2'd1;
  localparam logic [1:0] CLS_BIL  = 2'd2;

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
  // Three independent little FIFOs. A full one stalls its own class only.
  logic [DATAW-1:0] cq_d [3][CHN];
  logic [TOKW-1:0]  cq_t [3][CHN];
  logic [CW-1:0]    cq_wp [3], cq_rp [3];
  logic [CW:0]      cq_n [3];

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
      default:  head_room = 1'b1;   // an unknown class is dropped, not stalled
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

  assign occupancy_o = 3'(raw_n);

  // ------------------------------------------------------------ sequential --
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      raw_wp <= '0; raw_rp <= '0; raw_n <= '0;
      accepted_o <= 32'd0;
      hol_stall_o <= 32'd0;
      for (int i = 0; i < 3; i++) begin
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

      // ---- per-class push and pop, each counted once ---------------------
      for (int i = 0; i < 3; i++) begin
        automatic logic cpsh = dispatch_fire && (head_cls == 2'(i));
        automatic logic cpop = (i == 0) ? (clut_valid_o && clut_ready_i)
                             : (i == 1) ? (near_valid_o && near_ready_i)
                                        : (bil_valid_o  && bil_ready_i);
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
