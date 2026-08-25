// zhao_field_sin_rom.sv - SIN_Q16, the quarter-wave sine table.
//
// GENERATED, NOT TRANSCRIBED, from
// reference/include/zref/generated/zref_tables.hpp -- 257 hand-copied constants
// is where a silent error hides, and a wrong entry does not fail loudly. It
// puts one angle slightly off, which reads as a wobble rather than a break.
//
// tests/differential/field_sin_directed.cpp checks every entry against the
// reference table, because a generated file goes stale as easily as a copied
// one: the generator is not run by the build.
//
// 257 entries, quarter wave, 0 .. 0x10000 inclusive -- so seventeen bits, and
// the endpoint really is the full 1.0 rather than one ulp below it.
//
// ---------------------------------------------------------------------------
// WAVE 7, 2026-08-25: A SYNCHRONOUS DUAL-PORT TABLE, which is what the wave
// plan asked for originally.
// ---------------------------------------------------------------------------
// This was a combinational `unique case` over 257 entries, instantiated TWICE
// (`u_base` and `u_next`), so the interpolation paid for roughly 8,700 bits of
// LUT multiplexer and put one of them on the critical path -- `u_base` appears
// in the 43.89 MHz worst path by name.
//
// It is now ONE array with two synchronous read ports. Per QUARTUS_GOTCHAS S10
// this is the shape that INFERS: the reads are synchronous, no reset touches
// the array, and there are no byte enables. The entries are unchanged, and the
// per-entry differential still checks every one against the reference.
//
// The cost is one cycle of latency, which the owner bought explicitly on
// 2026-08-25: at 100 MHz even eight clocks per simple op is 2.2x the real
// throughput of six clocks at 33.86 MHz. The currency is real time, not cycles.
module zhao_field_sin_rom (
    input  logic        clk,
    input  logic [ 8:0] idx_a_i,   // 0..256
    input  logic [ 8:0] idx_b_i,   // 0..256
    output logic [16:0] val_a_o,
    output logic [16:0] val_b_o
);

  // No reset anywhere near `tbl`, and none on the outputs -- either would stop
  // this inferring and hand the entries back to logic.
  logic [16:0] tbl [0:256];

  initial begin
    tbl[  0] = 17'h00000;
    tbl[  1] = 17'h00192;
    tbl[  2] = 17'h00324;
    tbl[  3] = 17'h004B6;
    tbl[  4] = 17'h00648;
    tbl[  5] = 17'h007DA;
    tbl[  6] = 17'h0096C;
    tbl[  7] = 17'h00AFE;
    tbl[  8] = 17'h00C90;
    tbl[  9] = 17'h00E21;
    tbl[ 10] = 17'h00FB3;
    tbl[ 11] = 17'h01144;
    tbl[ 12] = 17'h012D5;
    tbl[ 13] = 17'h01466;
    tbl[ 14] = 17'h015F7;
    tbl[ 15] = 17'h01787;
    tbl[ 16] = 17'h01918;
    tbl[ 17] = 17'h01AA8;
    tbl[ 18] = 17'h01C38;
    tbl[ 19] = 17'h01DC7;
    tbl[ 20] = 17'h01F56;
    tbl[ 21] = 17'h020E5;
    tbl[ 22] = 17'h02274;
    tbl[ 23] = 17'h02402;
    tbl[ 24] = 17'h02590;
    tbl[ 25] = 17'h0271E;
    tbl[ 26] = 17'h028AB;
    tbl[ 27] = 17'h02A38;
    tbl[ 28] = 17'h02BC4;
    tbl[ 29] = 17'h02D50;
    tbl[ 30] = 17'h02EDC;
    tbl[ 31] = 17'h03067;
    tbl[ 32] = 17'h031F1;
    tbl[ 33] = 17'h0337C;
    tbl[ 34] = 17'h03505;
    tbl[ 35] = 17'h0368E;
    tbl[ 36] = 17'h03817;
    tbl[ 37] = 17'h0399F;
    tbl[ 38] = 17'h03B27;
    tbl[ 39] = 17'h03CAE;
    tbl[ 40] = 17'h03E34;
    tbl[ 41] = 17'h03FBA;
    tbl[ 42] = 17'h0413F;
    tbl[ 43] = 17'h042C3;
    tbl[ 44] = 17'h04447;
    tbl[ 45] = 17'h045CB;
    tbl[ 46] = 17'h0474D;
    tbl[ 47] = 17'h048CF;
    tbl[ 48] = 17'h04A50;
    tbl[ 49] = 17'h04BD1;
    tbl[ 50] = 17'h04D50;
    tbl[ 51] = 17'h04ECF;
    tbl[ 52] = 17'h0504D;
    tbl[ 53] = 17'h051CB;
    tbl[ 54] = 17'h05348;
    tbl[ 55] = 17'h054C3;
    tbl[ 56] = 17'h0563E;
    tbl[ 57] = 17'h057B9;
    tbl[ 58] = 17'h05932;
    tbl[ 59] = 17'h05AAA;
    tbl[ 60] = 17'h05C22;
    tbl[ 61] = 17'h05D99;
    tbl[ 62] = 17'h05F0F;
    tbl[ 63] = 17'h06084;
    tbl[ 64] = 17'h061F8;
    tbl[ 65] = 17'h0636B;
    tbl[ 66] = 17'h064DD;
    tbl[ 67] = 17'h0664E;
    tbl[ 68] = 17'h067BE;
    tbl[ 69] = 17'h0692D;
    tbl[ 70] = 17'h06A9B;
    tbl[ 71] = 17'h06C08;
    tbl[ 72] = 17'h06D74;
    tbl[ 73] = 17'h06EDF;
    tbl[ 74] = 17'h07049;
    tbl[ 75] = 17'h071B2;
    tbl[ 76] = 17'h0731A;
    tbl[ 77] = 17'h07480;
    tbl[ 78] = 17'h075E6;
    tbl[ 79] = 17'h0774A;
    tbl[ 80] = 17'h078AD;
    tbl[ 81] = 17'h07A10;
    tbl[ 82] = 17'h07B70;
    tbl[ 83] = 17'h07CD0;
    tbl[ 84] = 17'h07E2F;
    tbl[ 85] = 17'h07F8C;
    tbl[ 86] = 17'h080E8;
    tbl[ 87] = 17'h08243;
    tbl[ 88] = 17'h0839C;
    tbl[ 89] = 17'h084F5;
    tbl[ 90] = 17'h0864C;
    tbl[ 91] = 17'h087A1;
    tbl[ 92] = 17'h088F6;
    tbl[ 93] = 17'h08A49;
    tbl[ 94] = 17'h08B9A;
    tbl[ 95] = 17'h08CEB;
    tbl[ 96] = 17'h08E3A;
    tbl[ 97] = 17'h08F88;
    tbl[ 98] = 17'h090D4;
    tbl[ 99] = 17'h0921F;
    tbl[100] = 17'h09368;
    tbl[101] = 17'h094B0;
    tbl[102] = 17'h095F7;
    tbl[103] = 17'h0973C;
    tbl[104] = 17'h09880;
    tbl[105] = 17'h099C2;
    tbl[106] = 17'h09B03;
    tbl[107] = 17'h09C42;
    tbl[108] = 17'h09D80;
    tbl[109] = 17'h09EBC;
    tbl[110] = 17'h09FF7;
    tbl[111] = 17'h0A130;
    tbl[112] = 17'h0A268;
    tbl[113] = 17'h0A39E;
    tbl[114] = 17'h0A4D2;
    tbl[115] = 17'h0A605;
    tbl[116] = 17'h0A736;
    tbl[117] = 17'h0A866;
    tbl[118] = 17'h0A994;
    tbl[119] = 17'h0AAC1;
    tbl[120] = 17'h0ABEB;
    tbl[121] = 17'h0AD14;
    tbl[122] = 17'h0AE3C;
    tbl[123] = 17'h0AF62;
    tbl[124] = 17'h0B086;
    tbl[125] = 17'h0B1A8;
    tbl[126] = 17'h0B2C9;
    tbl[127] = 17'h0B3E8;
    tbl[128] = 17'h0B505;
    tbl[129] = 17'h0B620;
    tbl[130] = 17'h0B73A;
    tbl[131] = 17'h0B852;
    tbl[132] = 17'h0B968;
    tbl[133] = 17'h0BA7D;
    tbl[134] = 17'h0BB8F;
    tbl[135] = 17'h0BCA0;
    tbl[136] = 17'h0BDAF;
    tbl[137] = 17'h0BEBC;
    tbl[138] = 17'h0BFC7;
    tbl[139] = 17'h0C0D1;
    tbl[140] = 17'h0C1D8;
    tbl[141] = 17'h0C2DE;
    tbl[142] = 17'h0C3E2;
    tbl[143] = 17'h0C4E4;
    tbl[144] = 17'h0C5E4;
    tbl[145] = 17'h0C6E2;
    tbl[146] = 17'h0C7DE;
    tbl[147] = 17'h0C8D9;
    tbl[148] = 17'h0C9D1;
    tbl[149] = 17'h0CAC7;
    tbl[150] = 17'h0CBBC;
    tbl[151] = 17'h0CCAE;
    tbl[152] = 17'h0CD9F;
    tbl[153] = 17'h0CE8E;
    tbl[154] = 17'h0CF7A;
    tbl[155] = 17'h0D065;
    tbl[156] = 17'h0D14D;
    tbl[157] = 17'h0D234;
    tbl[158] = 17'h0D318;
    tbl[159] = 17'h0D3FB;
    tbl[160] = 17'h0D4DB;
    tbl[161] = 17'h0D5BA;
    tbl[162] = 17'h0D696;
    tbl[163] = 17'h0D770;
    tbl[164] = 17'h0D848;
    tbl[165] = 17'h0D91E;
    tbl[166] = 17'h0D9F2;
    tbl[167] = 17'h0DAC4;
    tbl[168] = 17'h0DB94;
    tbl[169] = 17'h0DC62;
    tbl[170] = 17'h0DD2D;
    tbl[171] = 17'h0DDF7;
    tbl[172] = 17'h0DEBE;
    tbl[173] = 17'h0DF83;
    tbl[174] = 17'h0E046;
    tbl[175] = 17'h0E107;
    tbl[176] = 17'h0E1C6;
    tbl[177] = 17'h0E282;
    tbl[178] = 17'h0E33C;
    tbl[179] = 17'h0E3F4;
    tbl[180] = 17'h0E4AA;
    tbl[181] = 17'h0E55E;
    tbl[182] = 17'h0E610;
    tbl[183] = 17'h0E6BF;
    tbl[184] = 17'h0E76C;
    tbl[185] = 17'h0E817;
    tbl[186] = 17'h0E8BF;
    tbl[187] = 17'h0E966;
    tbl[188] = 17'h0EA0A;
    tbl[189] = 17'h0EAAB;
    tbl[190] = 17'h0EB4B;
    tbl[191] = 17'h0EBE8;
    tbl[192] = 17'h0EC83;
    tbl[193] = 17'h0ED1C;
    tbl[194] = 17'h0EDB3;
    tbl[195] = 17'h0EE47;
    tbl[196] = 17'h0EED9;
    tbl[197] = 17'h0EF68;
    tbl[198] = 17'h0EFF5;
    tbl[199] = 17'h0F080;
    tbl[200] = 17'h0F109;
    tbl[201] = 17'h0F18F;
    tbl[202] = 17'h0F213;
    tbl[203] = 17'h0F295;
    tbl[204] = 17'h0F314;
    tbl[205] = 17'h0F391;
    tbl[206] = 17'h0F40C;
    tbl[207] = 17'h0F484;
    tbl[208] = 17'h0F4FA;
    tbl[209] = 17'h0F56E;
    tbl[210] = 17'h0F5DF;
    tbl[211] = 17'h0F64E;
    tbl[212] = 17'h0F6BA;
    tbl[213] = 17'h0F724;
    tbl[214] = 17'h0F78C;
    tbl[215] = 17'h0F7F1;
    tbl[216] = 17'h0F854;
    tbl[217] = 17'h0F8B4;
    tbl[218] = 17'h0F913;
    tbl[219] = 17'h0F96E;
    tbl[220] = 17'h0F9C8;
    tbl[221] = 17'h0FA1F;
    tbl[222] = 17'h0FA73;
    tbl[223] = 17'h0FAC5;
    tbl[224] = 17'h0FB15;
    tbl[225] = 17'h0FB62;
    tbl[226] = 17'h0FBAD;
    tbl[227] = 17'h0FBF5;
    tbl[228] = 17'h0FC3B;
    tbl[229] = 17'h0FC7F;
    tbl[230] = 17'h0FCC0;
    tbl[231] = 17'h0FCFE;
    tbl[232] = 17'h0FD3B;
    tbl[233] = 17'h0FD74;
    tbl[234] = 17'h0FDAC;
    tbl[235] = 17'h0FDE1;
    tbl[236] = 17'h0FE13;
    tbl[237] = 17'h0FE43;
    tbl[238] = 17'h0FE71;
    tbl[239] = 17'h0FE9C;
    tbl[240] = 17'h0FEC4;
    tbl[241] = 17'h0FEEB;
    tbl[242] = 17'h0FF0E;
    tbl[243] = 17'h0FF30;
    tbl[244] = 17'h0FF4E;
    tbl[245] = 17'h0FF6B;
    tbl[246] = 17'h0FF85;
    tbl[247] = 17'h0FF9C;
    tbl[248] = 17'h0FFB1;
    tbl[249] = 17'h0FFC4;
    tbl[250] = 17'h0FFD4;
    tbl[251] = 17'h0FFE1;
    tbl[252] = 17'h0FFEC;
    tbl[253] = 17'h0FFF5;
    tbl[254] = 17'h0FFFB;
    tbl[255] = 17'h0FFFF;
    tbl[256] = 17'h10000;
  end

  always_ff @(posedge clk) begin
    val_a_o <= tbl[idx_a_i];
    val_b_o <= tbl[idx_b_i];
  end

endmodule : zhao_field_sin_rom
