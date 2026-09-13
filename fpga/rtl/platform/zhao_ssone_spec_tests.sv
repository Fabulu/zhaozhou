// On-device Zhaozhou block specification vectors.
//
// This is not a substitute for the existing differential/formal suites. It
// proves that selected shipping RTL elaborates with the board backend and
// produces the same named results in physical Cyclone V fabric.
`default_nettype none

(* preserve_hierarchy *)
module zhao_ssone_spec_tests #(
    parameter bit INJECT_FAILURE = 1'b0
) (
    input  wire        clk,
    input  wire        rst_n,
    output logic       done_o,
    output logic       pass_o,
    output logic [7:0] fail_code_o,
    output logic [7:0] active_test_o,
    output logic [31:0] completed_o,
    output logic [31:0] signature_o
);

  localparam logic [4:0] LAST_TEST = 5'd15;
  localparam logic [31:0] EXPECTED_SIGNATURE = 32'hE5F1C57F;
  localparam logic [1:0] KIND_CRC = 2'd0;
  localparam logic [1:0] KIND_FILL = 2'd1;
  localparam logic [1:0] KIND_MUL = 2'd2;

  logic [4:0] test_q;
  logic [1:0] kind_c;

  logic [31:0] crc_c_i;
  logic [63:0] crc_d_i;
  logic [3:0]  crc_n_i;
  logic [31:0] crc_expected_c;
  wire  [31:0] crc_actual;

  logic signed [28:0] fill_e_i;
  logic               fill_rnz_i;
  logic               fill_tl_i;
  logic               fill_expected_c;
  wire                fill_actual;

  logic [17:0] mul_ax_i;
  logic [17:0] mul_ay_i;
  logic [17:0] mul_bx_i;
  logic [17:0] mul_by_i;
  logic [35:0] mul_a_expected_c;
  logic [35:0] mul_b_expected_c;
  wire  [35:0] mul_a_actual;
  wire  [35:0] mul_b_actual;

  (* keep, preserve *)
  zhao_crc32c_fold u_crc (
      .c_i(crc_c_i),
      .d_i(crc_d_i),
      .n_i(crc_n_i),
      .c_o(crc_actual)
  );

  (* keep, preserve *)
  zhao_raster_fill #(.W(29)) u_fill (
      .e_i(fill_e_i),
      .rnz_i(fill_rnz_i),
      .tl_i(fill_tl_i),
      .accept_o(fill_actual)
  );

  // Lane A is signed x signed; lane B is unsigned x unsigned. Synthesis uses
  // the physical one-block dual-18 Cyclone V backend, while the directed test
  // selects its bit-exact behavioral backend.
  (* keep, preserve *)
  zhao_dual18_mul #(
      .AX_SIGNED(1'b1),
      .AY_SIGNED(1'b1),
      .BX_SIGNED(1'b0),
      .BY_SIGNED(1'b0)
  ) u_mul (
      .ax_i(mul_ax_i),
      .ay_i(mul_ay_i),
      .bx_i(mul_bx_i),
      .by_i(mul_by_i),
      .resulta_o(mul_a_actual),
      .resultb_o(mul_b_actual)
  );

  always_comb begin
    kind_c = KIND_CRC;

    crc_c_i = 32'd0;
    crc_d_i = 64'd0;
    crc_n_i = 4'd0;
    crc_expected_c = 32'd0;

    fill_e_i = 29'sd0;
    fill_rnz_i = 1'b0;
    fill_tl_i = 1'b0;
    fill_expected_c = 1'b0;

    mul_ax_i = 18'd0;
    mul_ay_i = 18'd0;
    mul_bx_i = 18'd0;
    mul_by_i = 18'd0;
    mul_a_expected_c = 36'd0;
    mul_b_expected_c = 36'd0;

    case (test_q)
      5'd0: begin
        // Positive control for the detector: the directed bench also builds an
        // INJECT_FAILURE instance and requires this vector to latch code 1.
        crc_expected_c = INJECT_FAILURE ? 32'h00000001 : 32'h00000000;
      end
      5'd1: begin
        crc_c_i = 32'hFFFFFFFF;
        crc_d_i = 64'h3837363534333231; // "12345678", low byte first
        crc_n_i = 4'd8;
        crc_expected_c = 32'h9F787F65;
      end
      5'd2: begin
        crc_d_i = 64'h0123456789ABCDEF;
        crc_n_i = 4'd8;
        crc_expected_c = 32'hE9986AA9;
      end
      5'd3: begin
        crc_c_i = 32'h12345678;
        crc_d_i = 64'h0000000000006261; // "ab"
        crc_n_i = 4'd2;
        crc_expected_c = 32'h460A66D4;
      end
      5'd4: begin
        crc_c_i = 32'h89ABCDEF;
        crc_d_i = 64'h8877665544332211;
        crc_n_i = 4'd5;
        crc_expected_c = 32'h3AF6A45D;
      end
      5'd5: begin
        crc_c_i = 32'hD15EA5E5;
        crc_d_i = 64'hFFFFFFFFFFFFFFFF;
        crc_n_i = 4'd9; // illegal counts return the running state
        crc_expected_c = 32'hD15EA5E5;
      end

      5'd6: begin
        kind_c = KIND_FILL;
        fill_e_i = -29'sd1;
        fill_rnz_i = 1'b1;
        fill_tl_i = 1'b1;
        fill_expected_c = 1'b0;
      end
      5'd7: begin
        kind_c = KIND_FILL;
        fill_e_i = 29'sd0;
        fill_tl_i = 1'b1;
        fill_expected_c = 1'b1;
      end
      5'd8: begin
        kind_c = KIND_FILL;
        fill_e_i = 29'sd0;
        fill_rnz_i = 1'b1;
        fill_expected_c = 1'b1;
      end
      5'd9: begin
        kind_c = KIND_FILL;
        fill_e_i = 29'sd0;
        fill_expected_c = 1'b0;
      end
      5'd10: begin
        kind_c = KIND_FILL;
        fill_e_i = 29'sd1;
        fill_expected_c = 1'b1;
      end
      5'd11: begin
        kind_c = KIND_FILL;
        fill_e_i = {1'b1, 28'd0};
        fill_rnz_i = 1'b1;
        fill_tl_i = 1'b1;
        fill_expected_c = 1'b0;
      end

      5'd12: begin
        kind_c = KIND_MUL;
        mul_ax_i = 18'h3FFFF; // -1
        mul_ay_i = 18'h3FFFE; // -2
        mul_bx_i = 18'h3FFFF;
        mul_by_i = 18'h3FFFF;
        mul_a_expected_c = 36'h000000002;
        mul_b_expected_c = 36'hFFFF80001;
      end
      5'd13: begin
        kind_c = KIND_MUL;
        mul_ax_i = 18'h20000; // signed minimum
        mul_ay_i = 18'h00001;
        mul_bx_i = 18'd12345;
        mul_by_i = 18'd6789;
        mul_a_expected_c = 36'hFFFFE0000;
        mul_b_expected_c = 36'h004FED79D;
      end
      5'd14: begin
        kind_c = KIND_MUL;
        mul_ax_i = 18'h1FFFF; // signed maximum
        mul_ay_i = 18'h1FFFF;
        mul_bx_i = 18'h20000;
        mul_by_i = 18'd2;
        mul_a_expected_c = 36'h3FFFC0001;
        mul_b_expected_c = 36'h000040000;
      end
      5'd15: begin
        kind_c = KIND_MUL;
        mul_ax_i = 18'd0;
        mul_ay_i = 18'h3FFFF;
        mul_bx_i = 18'd0;
        mul_by_i = 18'h3FFFF;
        mul_a_expected_c = 36'd0;
        mul_b_expected_c = 36'd0;
      end
      default: begin
        kind_c = KIND_CRC;
      end
    endcase
  end

  logic current_ok_c;
  always_comb begin
    case (kind_c)
      KIND_CRC: current_ok_c = (crc_actual == crc_expected_c);
      KIND_FILL: current_ok_c = (fill_actual == fill_expected_c);
      KIND_MUL: current_ok_c = (mul_a_actual == mul_a_expected_c)
                                  && (mul_b_actual == mul_b_expected_c);
      default: current_ok_c = 1'b0;
    endcase
  end

  assign pass_o = done_o && (fail_code_o == 8'd0)
                  && (signature_o == EXPECTED_SIGNATURE);
  assign active_test_o = 8'(test_q + 5'd1);

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      test_q <= 5'd0;
      done_o <= 1'b0;
      fail_code_o <= 8'd0;
      completed_o <= 32'd0;
      signature_o <= 32'h5A17C0DE;
    end else if (!done_o) begin
      completed_o <= completed_o + 32'd1;
      case (kind_c)
        KIND_CRC: signature_o <= {signature_o[30:0], signature_o[31]} ^ crc_actual;
        KIND_FILL: signature_o <= {signature_o[30:0], signature_o[31]} ^ {31'd0, fill_actual};
        KIND_MUL: signature_o <= {signature_o[30:0], signature_o[31]}
                                 ^ mul_a_actual[31:0]
                                 ^ {mul_b_actual[15:0], mul_b_actual[35:20]};
        default: signature_o <= signature_o;
      endcase
      if (!current_ok_c && (fail_code_o == 8'd0))
        fail_code_o <= 8'(test_q + 5'd1);

      if (test_q == LAST_TEST)
        done_o <= 1'b1;
      else
        test_q <= test_q + 5'd1;
    end
  end

endmodule : zhao_ssone_spec_tests

`default_nettype wire
