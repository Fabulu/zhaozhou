// zhao_raster_attrdiv_svc.sv — the attribute divide as a SERVICE, so throughput
// is a parameter sweep rather than a rewrite.
//
// ENFORCED-BY: tests/raster/raster_attrdiv_svc_directed.cpp:main
//
// ---------------------------------------------------------------------------
// WHY THIS EXISTS
// ---------------------------------------------------------------------------
// `zhao_raster_attrdiv` is exact and measures 36 clocks a divide. One of them
// sustains 46,296 attribute-pixels a frame against a terrain-primary component
// of 276,480 pixels that each need at least `invw24` before early-Z. So a single
// divider is roughly 6x short for depth alone, and the renderer ruling that
// covers it says the divide is "a tagged service with a measured initiation
// rate, not one divider per lane".
//
// This is that service. It is deliberately the SAME SHAPE the Field engine
// arrived at after six sweep rounds: N identical units behind one ready/valid
// port, with N a build parameter, so the answer to "how much divide do we need"
// is a sweep against a real frame instead of an argument.
//
//     UNITS   accepted / 36 clocks   attribute-pixels a frame
//        1            1                      46,296
//        2            2                      92,593
//        4            4                     185,185
//        8            8                     370,370
//
// ---------------------------------------------------------------------------
// ORDER IS A CORRECTNESS PROPERTY, NOT A CONVENIENCE
// ---------------------------------------------------------------------------
// Fragments reach the tile store in raster order and the tile store's laws
// assume it. So this service returns answers IN ISSUE ORDER, and it does so
// without a reorder buffer: units are issued round-robin from `iss_r` and
// retired round-robin from `ret_r`, so the retire pointer simply waits for the
// unit whose turn it is. That is correct whatever each unit's latency turns out
// to be -- if a future divider answers in variable time the service still
// returns in order, it just stalls instead of reordering. Nothing here depends
// on the 36 being constant.
//
// The tag is carried alongside rather than used as identity for the reorder:
// it exists so the CALLER can say which attribute and which pixel this was,
// and it is returned untouched.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO
// ---------------------------------------------------------------------------
// It does not decide how many divides a frame needs -- that is early-Z's
// ordering and the caller's business. It does not skip a divide it could have
// avoided. `accepted_o`, `retired_o` and `stall_clocks_o` are here so a real
// frame can say what UNITS should be, because the Field lane's lesson was that
// the wall is whichever resource REFUSES, and a service that cannot report its
// own refusals cannot be sized.
`default_nettype none

module zhao_raster_attrdiv_svc #(
    parameter int unsigned UNITS = 4,
    parameter int unsigned TAGW  = 16,
    // Forwarded to every unit. UNITS and RADIX are the two independent knobs on
    // this service and the sweep crosses them, because "more units" and
    // "shorter units" cost different things and the fit decides which is
    // cheaper -- not this file.
    parameter int unsigned RADIX = 2
) (
    input var logic clk,
    input var logic rst_n,

    // ---- requests, in order --------------------------------------------------
    input  var logic                  v_valid_i,
    output var logic                  v_ready_o,
    input  var logic signed [95:0]    num_i,
    input  var logic        [46:0]    area_i,
    input  var logic [TAGW-1:0]       tag_i,

    // ---- answers, IN ISSUE ORDER --------------------------------------------
    output var logic                  r_valid_o,
    input  var logic                  r_ready_i,
    output var logic signed [31:0]    q_o,
    output var logic                  q_overflow_o,
    // The unit's Euclidean remainder, forwarded with its answer. The stepping
    // path seeds from it, so it has to survive the service rather than stop at
    // the unit -- and a service that dropped it would force every caller back
    // to a bare divider.
    output var logic [47:0]           rem_o,
    output var logic [TAGW-1:0]       tag_o,

    // ---- evidence, so UNITS is chosen by measurement -------------------------
    output var logic [31:0] accepted_o,
    output var logic [31:0] retired_o,
    // Clocks in which a request was offered and the service had no free unit.
    // This is the number that says whether UNITS is too small; a service that
    // never stalls is either big enough or never asked.
    output var logic [31:0] stall_clocks_o
);

  // A single unit still needs a pointer, and $clog2(1) is 0, which would make a
  // zero-width index. Same guard the Field ring service needed.
  localparam int unsigned PW = (UNITS <= 2) ? 1 : $clog2(UNITS);

  logic [UNITS-1:0]        u_vvalid, u_vready;
  logic [UNITS-1:0]        u_rvalid, u_rready;
  logic signed [31:0]      u_q       [UNITS];
  logic [47:0]             u_rem     [UNITS];
  logic                    u_ovf     [UNITS];
  logic [TAGW-1:0]         u_tag_r   [UNITS];

  logic [PW-1:0] iss_r, ret_r;

  // ---- issue ---------------------------------------------------------------
  // Only the unit whose turn it is may take work: round-robin issue is what
  // makes round-robin retire an ORDER guarantee rather than a coincidence.
  always_comb begin
    u_vvalid = '0;
    for (int unsigned u = 0; u < UNITS; ++u) begin
      u_vvalid[u] = v_valid_i && (PW'(u) == iss_r);
    end
  end
  assign v_ready_o = u_vready[iss_r];

  // ---- retire --------------------------------------------------------------
  always_comb begin
    u_rready = '0;
    for (int unsigned u = 0; u < UNITS; ++u) begin
      u_rready[u] = r_ready_i && (PW'(u) == ret_r);
    end
  end
  assign r_valid_o    = u_rvalid[ret_r];
  assign q_o          = u_q[ret_r];
  assign q_overflow_o = u_ovf[ret_r];
  assign rem_o        = u_rem[ret_r];
  assign tag_o        = u_tag_r[ret_r];

  generate
    for (genvar g = 0; g < int'(UNITS); ++g) begin : g_unit
      zhao_raster_attrdiv #(.RADIX(RADIX)) u_div (
          .clk          (clk),
          .rst_n        (rst_n),
          .v_valid_i    (u_vvalid[g]),
          .v_ready_o    (u_vready[g]),
          .num_i        (num_i),
          .area_i       (area_i),
          .r_valid_o    (u_rvalid[g]),
          .r_ready_i    (u_rready[g]),
          .q_o          (u_q[g]),
          .q_overflow_o (u_ovf[g]),
          .rem_o        (u_rem[g]),
          // Sunk deliberately: the SERVICE counts accepted and retired for the
          // whole pool, which is the number that sizes UNITS. A per-unit divide
          // count would only re-derive it, and a per-unit busy count says
          // nothing the pool's stall clocks do not.
          /* verilator lint_off PINCONNECTEMPTY */
          .divides_o    (),
          .busy_clocks_o()
          /* verilator lint_on PINCONNECTEMPTY */
      );
    end
  endgenerate

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      iss_r          <= '0;
      ret_r          <= '0;
      accepted_o     <= 32'd0;
      retired_o      <= 32'd0;
      stall_clocks_o <= 32'd0;
      for (int unsigned u = 0; u < UNITS; ++u) u_tag_r[u] <= '0;
    end else begin
      if (v_valid_i && v_ready_o) begin
        u_tag_r[iss_r] <= tag_i;
        iss_r          <= (PW'(iss_r) == PW'(UNITS - 1)) ? '0 : (iss_r + PW'(1));
        accepted_o     <= accepted_o + 32'd1;
      end else if (v_valid_i) begin
        stall_clocks_o <= stall_clocks_o + 32'd1;
      end

      if (r_valid_o && r_ready_i) begin
        ret_r     <= (PW'(ret_r) == PW'(UNITS - 1)) ? '0 : (ret_r + PW'(1));
        retired_o <= retired_o + 32'd1;
      end
    end
  end

endmodule : zhao_raster_attrdiv_svc

`default_nettype wire
