// zhao_raster_attrinterp.sv — RASTER.INTERP: step one attribute plane across a
// tile's covered pixels, in raster order, with adds only.
//
// ENFORCED-BY: tests/raster/raster_attrinterp_directed.cpp:main
//
// ---------------------------------------------------------------------------
// THE HALF PIXEL IS THE WHOLE TRAP
// ---------------------------------------------------------------------------
// GEOM.ATTRSETUP emits the numerator plane anchored at the COORDINATE ORIGIN:
//
//     N(X,Y) = N0 + (dNdx/256)*X + (dNdy/256)*Y      X, Y in S 12.8 subpixels
//
// with dNdx and dNdy already scaled to one PIXEL of step. But the oracle does
// not sample pixels at their corners. `reference/src/zrender/rast.cpp` and
// RASTER.EDGEWALK both sample at the CENTRE of pixel p, which spec §8 puts at
//
//     256*p + 128 subpixels
//
// so the value at the centre of pixel (p,q) is NOT `N0 + dNdx*p + dNdy*q`. It
// is that plus HALF a step in each axis:
//
//     N(p,q) = N0 + dNdx*p + dNdy*q + dNdx/2 + dNdy/2
//
// Both halves are exact, because dNdx and dNdy are multiples of 256 by
// construction -- ATTRSETUP shifts by PIXEL_SHIFT = 8 as its last act.
//
// Leaving that term out is the kind of error this project has already made once
// in a test and once in a shift: everything still interpolates smoothly, every
// gradient is still right, and every attribute is off by half a pixel of slope
// in both axes. On a steep gradient that is a visible seam against zref and it
// would move every golden capture CRC; on a flat one it is invisible. So the
// centre is applied HERE, once per tile, where the ruling says to apply it:
// "evaluate the planes at the tile's first pixel centre, step d/dy per row,
// d/dx per column".
//
// ---------------------------------------------------------------------------
// WHAT IT COSTS, AND WHY THAT IS THE RIGHT SHAPE
// ---------------------------------------------------------------------------
// Per TILE: two multiplies to place the plane at the tile's first pixel centre.
// Per ROW: one small shift-add to add row*dNdy. Per PIXEL: one add. That is the
// point of the plane form -- the expensive half of attribute interpolation is
// the divide downstream, at a measured 36 clocks, and this block feeds it at one
// pixel a clock so it is never the thing that refuses.
//
// It walks all 16 columns of a covered row and emits only the covered ones,
// rather than jumping between set bits. A skip network would save clocks on
// sparse coverage, but at 16 clocks a row worst case this block is already 36x
// faster than a single divider consuming it, so the complexity would buy
// nothing measurable. If the divide ever gets cheap enough for that to matter,
// `pixels_o` and the row count are here to say so first.
//
// ---------------------------------------------------------------------------
// WHAT IT DOES NOT DO
// ---------------------------------------------------------------------------
// It does not divide -- RASTER.ATTRDIV.SVC does, and the separation is what
// lets one interpolator feed N dividers. It does not know which attribute this
// is: invw24, u_over_w, v_over_w, colour and alpha are all the same plane
// stepped the same way, which is what makes the attribute COUNT a scheduling
// decision rather than a width decision. It does not decide coverage; it
// consumes RASTER.EDGEWALK's rows unchanged, so the two cannot disagree about
// which pixels exist.
`default_nettype none

module zhao_raster_attrinterp (
    input var logic clk,
    input var logic rst_n,

    // ---- one attribute plane for one tile ------------------------------------
    input  var logic               job_valid_i,
    output var logic               job_ready_o,
    input  var logic signed [95:0] job_n0_i,
    input  var logic signed [71:0] job_dndx_i,
    input  var logic signed [71:0] job_dndy_i,
    // The tile's top-left PIXEL, exactly as RASTER.EDGEWALK takes it.
    input  var logic signed [11:0] job_tile_x_i,
    input  var logic signed [11:0] job_tile_y_i,

    // ---- coverage in: RASTER.EDGEWALK's non-empty rows, unchanged ------------
    input  var logic        cov_valid_i,
    output var logic        cov_ready_o,
    input  var logic [3:0]  cov_row_i,
    input  var logic [15:0] cov_mask_i,
    input  var logic        cov_last_i,

    // ---- numerators out, raster order ---------------------------------------
    output var logic               n_valid_o,
    input  var logic               n_ready_i,
    output var logic signed [95:0] n_num_o,
    output var logic        [3:0]  n_row_o,
    output var logic        [3:0]  n_col_o,
    output var logic               n_last_o,  // last covered pixel of this JOB

    // ---- evidence ------------------------------------------------------------
    output var logic [31:0] pixels_o,
    output var logic [31:0] rows_o
);

  localparam logic [1:0] S_IDLE = 2'd0;
  localparam logic [1:0] S_ROW  = 2'd1;
  localparam logic [1:0] S_WALK = 2'd2;

  logic [1:0]         st_r;
  logic signed [95:0] base_r;   // the plane at the tile's FIRST pixel centre
  logic signed [95:0] dndx_r;
  logic signed [95:0] dndy_r;
  logic signed [95:0] acc_r;    // the plane at the current pixel
  logic [15:0]        mask_r;
  logic [3:0]         row_r;
  logic [3:0]         col_r;
  logic               last_row_r;

  // ---- the tile's first pixel centre ---------------------------------------
  // N0 + dNdx*tile_x + dNdy*tile_y + dNdx/2 + dNdy/2. The two halves are exact:
  // ATTRSETUP's last act is a shift by 8, so both gradients are multiples of 256
  // and their arithmetic right shift by one loses nothing.
  logic signed [95:0] dndx_c, dndy_c, base_c;
  always_comb begin
    dndx_c = 96'(job_dndx_i);
    dndy_c = 96'(job_dndy_i);
    base_c = 96'(job_n0_i)
           + dndx_c * 96'(job_tile_x_i)
           + dndy_c * 96'(job_tile_y_i)
           + (dndx_c >>> 1)
           + (dndy_c >>> 1);
  end

  // ---- row*dNdy, as four conditional adds rather than a multiply -----------
  logic signed [95:0] rowoff_c, rowbase_c;
  always_comb begin
    rowoff_c = 96'sd0;
    if (cov_row_i[0]) rowoff_c = rowoff_c + dndy_r;
    if (cov_row_i[1]) rowoff_c = rowoff_c + (dndy_r <<< 1);
    if (cov_row_i[2]) rowoff_c = rowoff_c + (dndy_r <<< 2);
    if (cov_row_i[3]) rowoff_c = rowoff_c + (dndy_r <<< 3);
    rowbase_c = base_r + rowoff_c;
  end

  // ---- is this the last covered pixel of the job? --------------------------
  // No set bit strictly above the current column, and this was the last row.
  logic no_more_cols_c;
  always_comb begin
    no_more_cols_c = 1'b1;
    for (int unsigned c = 0; c < 16; ++c) begin
      if ((c > {28'd0, col_r}) && mask_r[c]) no_more_cols_c = 1'b0;
    end
  end

  assign job_ready_o = (st_r == S_IDLE);
  assign cov_ready_o = (st_r == S_ROW);

  assign n_valid_o = (st_r == S_WALK) && mask_r[col_r];
  assign n_num_o   = acc_r;
  assign n_row_o   = row_r;
  assign n_col_o   = col_r;
  assign n_last_o  = last_row_r && no_more_cols_c;

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st_r       <= S_IDLE;
      base_r     <= 96'sd0;
      dndx_r     <= 96'sd0;
      dndy_r     <= 96'sd0;
      acc_r      <= 96'sd0;
      mask_r     <= 16'd0;
      row_r      <= 4'd0;
      col_r      <= 4'd0;
      last_row_r <= 1'b0;
      pixels_o   <= 32'd0;
      rows_o     <= 32'd0;
    end else begin
      case (st_r)
        S_IDLE: begin
          if (job_valid_i && job_ready_o) begin
            base_r <= base_c;
            dndx_r <= dndx_c;
            dndy_r <= dndy_c;
            st_r   <= S_ROW;
          end
        end

        S_ROW: begin
          if (cov_valid_i && cov_ready_o) begin
            acc_r      <= rowbase_c;
            mask_r     <= cov_mask_i;
            row_r      <= cov_row_i;
            col_r      <= 4'd0;
            last_row_r <= cov_last_i;
            rows_o     <= rows_o + 32'd1;
            st_r       <= S_WALK;
          end
        end

        S_WALK: begin
          // An uncovered column costs a clock and produces nothing; a covered
          // one waits for the consumer. Either way the accumulator advances by
          // exactly one pixel of dNdx, which is what keeps the walk exact.
          if (!n_valid_o || n_ready_i) begin
            if (n_valid_o) pixels_o <= pixels_o + 32'd1;
            if (col_r == 4'd15) begin
              st_r <= last_row_r ? S_IDLE : S_ROW;
            end else begin
              col_r <= col_r + 4'd1;
              acc_r <= acc_r + dndx_r;
            end
          end
        end

        default: st_r <= S_IDLE;
      endcase
    end
  end

endmodule : zhao_raster_attrinterp

`default_nettype wire
