// zhao_part_record.sv — the particle128 codec, amendment C2 / ruling R3.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS
// ---------------------------------------------------------------------------
// Every other particle block reads and writes the same 128-bit record, so the
// bit positions want to exist in exactly one place. This is that place.
//
//   bits   0..17   position X        18..35  position Y     36..53  position Z
//         54..64   velocity X        65..75  velocity Y     76..86  velocity Z
//         87..96   age               97..103 species       104..109 size
//        110..115  spin             116..119 flags         120..127 variation
//
// `PARTICLE_FORMAT_VERSION = 1`. Not provisional: qformats §10 was marked
// provisional and carried a layout that agreed with the ruling on nothing but
// the total, and amendment C2 replaced it whole.
//
// ---------------------------------------------------------------------------
// PURELY COMBINATIONAL, AND THAT IS THE POINT
// ---------------------------------------------------------------------------
// A codec that registers its output invites callers to build pipelines around
// a field extraction, and then the bit positions live in the pipeline's timing
// as well as in its logic. This block is wires and sign extension. Whoever
// needs a register puts one after it.
//
// ---------------------------------------------------------------------------
// THE TWO DERIVED VALUES THE LAW NAMES
// ---------------------------------------------------------------------------
//   radius  = base_radius_fx16 * size / 16, with ONE final round-half-up.
//             One rounding on the whole product -- not a shift of a rounded
//             multiply, which is a different function at every odd input.
//   angle16 = spin << 10. The stored phase wraps mod 64 by being six bits.
//
// `size` IS A WORLD-SCALE MULTIPLIER, NEVER CAMERA-SPACE PIXELS. That sentence
// is in the ruling because the pre-C2 §10 said the opposite, and
// `zhao_part_expand.sv` still contains a `size << 4` built on the old reading.
// It carries a banner saying so; it is not fixed there and must not be fixed
// by making this block agree with it.
// ---------------------------------------------------------------------------
`default_nettype none

module zhao_part_record (
    // ---- unpack --------------------------------------------------------------
    input  var logic [127:0]      rec_i,

    output var logic signed [17:0] pos_x_o,
    output var logic signed [17:0] pos_y_o,
    output var logic signed [17:0] pos_z_o,
    output var logic signed [10:0] vel_x_o,
    output var logic signed [10:0] vel_y_o,
    output var logic signed [10:0] vel_z_o,
    output var logic [9:0]         age_o,
    output var logic [6:0]         species_o,
    output var logic [5:0]         size_o,
    output var logic [5:0]         spin_o,
    output var logic [3:0]         flags_o,
    output var logic [7:0]         variation_o,

    // ---- pack ----------------------------------------------------------------
    input  var logic signed [17:0] pos_x_i,
    input  var logic signed [17:0] pos_y_i,
    input  var logic signed [17:0] pos_z_i,
    input  var logic signed [10:0] vel_x_i,
    input  var logic signed [10:0] vel_y_i,
    input  var logic signed [10:0] vel_z_i,
    input  var logic [9:0]         age_i,
    input  var logic [6:0]         species_i,
    input  var logic [5:0]         size_i,
    input  var logic [5:0]         spin_i,
    input  var logic [3:0]         flags_i,
    input  var logic [7:0]         variation_i,
    output var logic [127:0]       rec_o,

    // ---- derived -------------------------------------------------------------
    input  var logic signed [31:0] base_radius_i,   // fx16
    output var logic signed [31:0] radius_o,
    output var logic [15:0]        angle16_o
);

  // ---- unpack: bit ranges, once ------------------------------------------
  assign pos_x_o     = $signed(rec_i[  0 +: 18]);
  assign pos_y_o     = $signed(rec_i[ 18 +: 18]);
  assign pos_z_o     = $signed(rec_i[ 36 +: 18]);
  assign vel_x_o     = $signed(rec_i[ 54 +: 11]);
  assign vel_y_o     = $signed(rec_i[ 65 +: 11]);
  assign vel_z_o     = $signed(rec_i[ 76 +: 11]);
  assign age_o       = rec_i[ 87 +: 10];
  assign species_o   = rec_i[ 97 +:  7];
  assign size_o      = rec_i[104 +:  6];
  assign spin_o      = rec_i[110 +:  6];
  assign flags_o     = rec_i[116 +:  4];
  assign variation_o = rec_i[120 +:  8];

  // ---- pack: the same ranges, the other way ------------------------------
  // Written as one concatenation, MOST significant field first, so the layout
  // reads down the page in the order the spec table reads across. A field that
  // moves has to move in both places or the round-trip test fails, which is
  // the only reason it is safe for the ranges to appear twice.
  assign rec_o = {variation_i,          // 120..127
                  flags_i,              // 116..119
                  spin_i,               // 110..115
                  size_i,               // 104..109
                  species_i,            //  97..103
                  age_i,                //  87..96
                  vel_z_i,              //  76..86
                  vel_y_i,              //  65..75
                  vel_x_i,              //  54..64
                  pos_z_i,              //  36..53
                  pos_y_i,              //  18..35
                  pos_x_i};             //   0..17

  // ---- radius: ONE rounding, on the whole product -------------------------
  // `(base * size + 8) >> 4`, not `(base >> 4) * size` and not
  // `((base * size) >> 4)` unrounded. Round-half-up on the product is the law;
  // the other two forms differ from it at every odd sixteenth and would look
  // right in any test that only used even sizes.
  logic signed [39:0] prod_c;
  assign prod_c   = $signed(base_radius_i) * $signed({1'b0, size_i});
  assign radius_o = 32'((prod_c + 40'sd8) >>> 4);

  // ---- angle16 = spin << 10 -----------------------------------------------
  // Six bits shifted left ten fills sixteen exactly, so the phase wraps mod 64
  // by construction rather than by a mask.
  assign angle16_o = {spin_i, 10'd0};

endmodule : zhao_part_record

`default_nettype wire
