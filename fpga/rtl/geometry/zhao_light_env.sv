// zhao_light_env.sv -- GEOM.LIGHT's ENVIRONMENT LOADER: one SetEnvironment
// record in, `zhao_light_stream`'s descriptor bank loaded and published.
//
//     CMD.EXEC --e_* (SetEnvironment, committed)--> zhao_light_env
//                                                     |  zhao_field_sin (x4)
//                                                     |  one 18x18 product (x2)
//                                                     v
//                         zhao_light_stream cfg_we / cfg_addr / cfg_data / commit
//
// ---------------------------------------------------------------------------
// WHY THIS FILE EXISTS: ENTRY I48, OWNER RULING R25
// ---------------------------------------------------------------------------
// The bank was host-loaded through `geom_light_cfg_*` because SetEnvironment
// was `reserved` and no law turned its record into bank words. R25 promotes
// 0x0311 to IMPLEMENTED, makes the bank's Q16.16 / u20 values the law, and
// asks for a zref bridge; `reference/include/zref/zref_light_env.hpp` is that
// bridge and this block writes EXACTLY its words (tests/geometry/
// light_env_directed.cpp differences every one, for random records and the
// power-on default).
//
// WHAT IT WRITES, in zhao_light_stream's cfg layout {light[3:0], half, word[2:0]}:
//   light 0, half A, words 0..3   Lx, Ly, Lz (4a's direction law), detail 0
//   light 0, half B, words 0..3   {flags 0, emission 0,0,0, gain b,g,r}, the
//                                  gains being the sun colour's lanes c8 << 8
//   light 0xF (environment) 0..5  ambient r,g,b lanes, spill 0,0,0
// then ONE commit, and `nlights_o` = 1 (the one sun). See the bridge header for
// the sentence each value is derived from; nothing is chosen here.
//
// THE POWER-ON DEFAULT IS A LOAD, NOT A RESET VALUE. 4a: "A frame with no
// SetEnvironment keeps the previous state; the power-on default is sun pitch
// 0x4000 (zenith), sun colour 0xBDF7, ambient 0x4208". So after reset this
// block loads that record through the same path a command takes -- the bank
// is never lit by zeros, and the default cannot drift from the command law
// because it IS the command law applied to the default record. The defaults are
// the named constants below, mirrored from `zref::sky::EnvState`'s initialisers
// and checked against them by the directed test.
//
// THE ENVIRONMENT NEVER CHANGES UNDER A VERTEX. `zhao_light_stream`
// double-buffers its LIGHTS (shadow generation + commit) but writes its
// AMBIENT/SPILL words directly, and a light write into a shadow generation that
// still has terms in flight is REFUSED. So a load raises `hold_o` -- the
// composer gates the stream's vertex input with it -- waits for
// `stream_idle_i`, and only then writes and commits. Between vertices, never
// under one; no write can be refused, and none is.
//
// THE ARITHMETIC IS SHARED, NOT REPLICATED: one `zhao_field_sin` answers all
// four trig reads (latency 2, II 1 -- the same instance law FIELD uses, a second
// instance of the one table), and ONE 18x18 product serves both fx_mul terms
// on consecutive clocks. |sin|, |cos| <= 0x1_0000 fit s18, so the product is
// 36 bits and fx_mul's single rounding (qformats 3) cannot saturate.
//
// TINT and FOG ride the same record and are NOT this block's: see the bridge
// header. They leave CMD.EXEC with the rest of the record, and the composer
// states their consumers.
//
// Conservative SystemVerilog subset only (charter section 2).
`default_nettype none

module zhao_light_env #(
    // 4a's power-on record, mirrored from zref::sky::EnvState. Named so the
    // default is an editable value, and checked against the reference by
    // tests/geometry/light_env_directed.cpp rather than trusted.
    parameter logic [15:0] DEF_SUN_YAW    = 16'h0000,
    parameter logic [15:0] DEF_SUN_PITCH  = 16'h4000,  // zenith
    parameter logic [15:0] DEF_SUN_COLOUR = 16'hBDF7,  // lanes 23,47,23
    parameter logic [15:0] DEF_AMBIENT    = 16'h4208   // lanes 8,16,8
) (
    input  wire clk,
    input  wire rst_n,

    // ---- the committed SetEnvironment record, from CMD.EXEC ------------------
    input  wire        e_valid_i,
    output wire        e_ready_o,
    input  wire [15:0] e_sun_yaw_i,
    input  wire [15:0] e_sun_pitch_i,
    input  wire [15:0] e_sun_colour_i,   // rgb565
    input  wire [15:0] e_ambient_i,      // rgb565

    // ---- zhao_light_stream's bank port ---------------------------------------
    output logic        cfg_we_o,
    output logic        cfg_commit_o,
    output logic [7:0]  cfg_addr_o,
    output logic [31:0] cfg_data_o,

    // ---- the stream's vertex input is held while the bank changes -----------
    output wire         hold_o,
    input  wire         stream_idle_i,

    // ---- the bank's light count, for the per-vertex descriptor --------------
    output logic [3:0]  nlights_o,

    // ---- evidence ------------------------------------------------------------
    output logic [31:0] loads_o,         // bank loads published (power-on included)
    output logic [31:0] records_o,       // SetEnvironment records taken
    output logic [31:0] superseded_o     // a record replaced before it was loaded
);

  // ---- the record to load ------------------------------------------------
  logic        pend_q;       // a record is waiting to be loaded
  logic [15:0] yaw_q, pitch_q, sun_q, amb_q;

  // ---- the loader -----------------------------------------------------------
  localparam logic [2:0] L_IDLE = 3'd0,   // nothing to do
                         L_HOLD = 3'd1,   // hold raised; waiting for the stream to drain
                         L_TRIG = 3'd2,   // four trig reads in flight
                         L_MUL  = 3'd3,   // the two fx_mul products
                         L_WR   = 3'd4,   // fourteen bank words
                         L_PUB  = 3'd5;   // the commit
  logic [2:0] st_q;
  logic [2:0] tk_q;          // trig issue/collect step, 0..5
  logic [3:0] wk_q;          // bank word, 0..13
  logic       mk_q;          // product step, 0..1

  // The record latched for THIS load (a newer record may arrive meanwhile).
  logic [15:0] ld_yaw_q, ld_pitch_q, ld_sun_q, ld_amb_q;

  assign hold_o    = (st_q != L_IDLE);
  // A record is taken whenever offered: a newer environment replaces a pending
  // one that has not started loading (4a: the frame's LAST record wins) and is
  // counted, never queued behind it.
  assign e_ready_o = 1'b1;

  // ---- trig: one table, four reads --------------------------------------------
  // Issue order: sin p, cos p, sin y, cos y on tk 0..3; each answers two clocks
  // later, on tk 2..5.
  logic [15:0]        trig_angle_c;
  logic               trig_cos_c;
  // |fx_sin| <= 0x1_0000, so bits [31:18] are copies of the sign: s18 holds
  // every value the table can answer.
  /* verilator lint_off UNUSEDSIGNAL */
  logic signed [31:0] trig_r;
  /* verilator lint_on UNUSEDSIGNAL */
  always_comb begin
    trig_angle_c = (tk_q < 3'd2) ? ld_pitch_q : ld_yaw_q;
    trig_cos_c   = tk_q[0];
  end

  zhao_field_sin u_sin (
      .clk      (clk),
      .angle_i  (trig_angle_c),
      .is_cos_i (trig_cos_c),
      .result_o (trig_r)
  );

  logic signed [17:0] sp_q, cp_q, sy_q, cy_q;

  // ---- one product, two terms: x = fx_mul(cos p, sin y), z = fx_mul(cos p, cos y)
  logic signed [35:0] prod_c;
  logic signed [31:0] fxm_c;
  logic signed [17:0] mop_c;
  always_comb begin
    mop_c  = mk_q ? cy_q : sy_q;
    prod_c = 36'(cp_q) * 36'(mop_c);
    // qformats 3/4: ONE rescale(., 16), round-half-up, arithmetic shift. The
    // operands are at most 1.0, so the result is at most 1.0: no saturation.
    fxm_c  = 32'((prod_c + 36'sd32768) >>> 16);
  end
  logic signed [31:0] lx_q, lz_q;

  // ---- rgb565 -> 8-bit (4a, bit replication) -> lane (c8 << 8) -------------
  function automatic logic [19:0] lane5(input logic [4:0] c5);
    lane5 = {4'd0, c5, c5[4:2], 8'd0};
  endfunction
  function automatic logic [19:0] lane6(input logic [5:0] c6);
    lane6 = {4'd0, c6, c6[5:4], 8'd0};
  endfunction

  logic [127:0] coeff_c;
  always_comb begin
    coeff_c = '0;
    coeff_c[19:0]  = lane5(ld_sun_q[15:11]);   // gain r
    coeff_c[39:20] = lane6(ld_sun_q[10:5]);    // gain g
    coeff_c[59:40] = lane5(ld_sun_q[4:0]);     // gain b
    // [119:60] emissions 0, [127:120] flags 0
  end

  // ---- the word sequence ------------------------------------------------------
  logic [7:0]  w_addr_c;
  logic [31:0] w_data_c;
  always_comb begin
    w_addr_c = 8'd0;
    w_data_c = 32'd0;
    case (wk_q)
      4'd0:  begin w_addr_c = {4'd0, 1'b0, 3'd0}; w_data_c = lx_q;                 end
      4'd1:  begin w_addr_c = {4'd0, 1'b0, 3'd1}; w_data_c = 32'(sp_q);            end
      4'd2:  begin w_addr_c = {4'd0, 1'b0, 3'd2}; w_data_c = lz_q;                 end
      4'd3:  begin w_addr_c = {4'd0, 1'b0, 3'd3}; w_data_c = 32'd0;                end  // detail
      4'd4:  begin w_addr_c = {4'd0, 1'b1, 3'd0}; w_data_c = coeff_c[31:0];        end
      4'd5:  begin w_addr_c = {4'd0, 1'b1, 3'd1}; w_data_c = coeff_c[63:32];       end
      4'd6:  begin w_addr_c = {4'd0, 1'b1, 3'd2}; w_data_c = coeff_c[95:64];       end
      4'd7:  begin w_addr_c = {4'd0, 1'b1, 3'd3}; w_data_c = coeff_c[127:96];      end
      4'd8:  begin w_addr_c = {4'hF, 1'b0, 3'd0}; w_data_c = {12'd0, lane5(ld_amb_q[15:11])}; end
      4'd9:  begin w_addr_c = {4'hF, 1'b0, 3'd1}; w_data_c = {12'd0, lane6(ld_amb_q[10:5])};  end
      4'd10: begin w_addr_c = {4'hF, 1'b0, 3'd2}; w_data_c = {12'd0, lane5(ld_amb_q[4:0])};   end
      4'd11: begin w_addr_c = {4'hF, 1'b0, 3'd3}; w_data_c = 32'd0;                end  // spill r
      4'd12: begin w_addr_c = {4'hF, 1'b0, 3'd4}; w_data_c = 32'd0;                end  // spill g
      4'd13: begin w_addr_c = {4'hF, 1'b0, 3'd5}; w_data_c = 32'd0;                end  // spill b
      default: ;
    endcase
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      // THE POWER-ON LOAD: the default record is pending from reset.
      pend_q       <= 1'b1;
      yaw_q        <= DEF_SUN_YAW;
      pitch_q      <= DEF_SUN_PITCH;
      sun_q        <= DEF_SUN_COLOUR;
      amb_q        <= DEF_AMBIENT;
      ld_yaw_q     <= '0;
      ld_pitch_q   <= '0;
      ld_sun_q     <= '0;
      ld_amb_q     <= '0;
      st_q         <= L_IDLE;
      tk_q         <= '0;
      wk_q         <= '0;
      mk_q         <= 1'b0;
      sp_q         <= '0;
      cp_q         <= '0;
      sy_q         <= '0;
      cy_q         <= '0;
      lx_q         <= '0;
      lz_q         <= '0;
      cfg_we_o     <= 1'b0;
      cfg_commit_o <= 1'b0;
      cfg_addr_o   <= '0;
      cfg_data_o   <= '0;
      nlights_o    <= 4'd0;
      loads_o      <= '0;
      records_o    <= '0;
      superseded_o <= '0;
    end else begin
      cfg_we_o     <= 1'b0;
      cfg_commit_o <= 1'b0;

      // --- a record from CMD.EXEC: the newest one wins ----------------------
      if (e_valid_i) begin
        if (records_o != 32'hFFFF_FFFF) records_o <= records_o + 32'd1;
        if (pend_q && (superseded_o != 32'hFFFF_FFFF)) superseded_o <= superseded_o + 32'd1;
        pend_q  <= 1'b1;
        yaw_q   <= e_sun_yaw_i;
        pitch_q <= e_sun_pitch_i;
        sun_q   <= e_sun_colour_i;
        amb_q   <= e_ambient_i;
      end

      case (st_q)
        L_IDLE: begin
          // Start only with no record arriving on the same clock, so the
          // record latched for the load is never one clock stale.
          if (pend_q && !e_valid_i) begin
            ld_yaw_q   <= yaw_q;
            ld_pitch_q <= pitch_q;
            ld_sun_q   <= sun_q;
            ld_amb_q   <= amb_q;
            pend_q     <= 1'b0;
            st_q       <= L_HOLD;
          end
        end

        L_HOLD: begin
          // hold_o is already high: the stream takes no new vertex, so once it
          // reports idle it STAYS idle until this block lets go.
          if (stream_idle_i) begin
            tk_q <= '0;
            st_q <= L_TRIG;
          end
        end

        L_TRIG: begin
          // results for the reads issued two clocks ago
          case (tk_q)
            3'd2: sp_q <= 18'(trig_r);
            3'd3: cp_q <= 18'(trig_r);
            3'd4: sy_q <= 18'(trig_r);
            3'd5: cy_q <= 18'(trig_r);
            default: ;
          endcase
          if (tk_q == 3'd5) begin
            mk_q <= 1'b0;
            st_q <= L_MUL;
          end else begin
            tk_q <= tk_q + 3'd1;
          end
        end

        L_MUL: begin
          if (!mk_q) begin
            lx_q <= fxm_c;
            mk_q <= 1'b1;
          end else begin
            lz_q <= fxm_c;
            wk_q <= '0;
            st_q <= L_WR;
          end
        end

        L_WR: begin
          cfg_we_o   <= 1'b1;
          cfg_addr_o <= w_addr_c;
          cfg_data_o <= w_data_c;
          if (wk_q == 4'd13) st_q <= L_PUB;
          else               wk_q <= wk_q + 4'd1;
        end

        L_PUB: begin
          // The last word was registered on the previous clock and lands on
          // this one; the commit is issued this clock and lands on the next,
          // after every write of the load.
          cfg_commit_o <= 1'b1;
          nlights_o    <= 4'd1;
          if (loads_o != 32'hFFFF_FFFF) loads_o <= loads_o + 32'd1;
          st_q <= L_IDLE;
        end

        default: st_q <= L_IDLE;
      endcase
    end
  end

endmodule : zhao_light_env

`default_nettype wire
