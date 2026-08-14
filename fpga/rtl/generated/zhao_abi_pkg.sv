// GENERATED FILE - DO NOT EDIT
// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).
// Law: spec/capture_format.md.
//
// BYTE-IDENTITY HAZARD (plan R1): packed structs are declared in REVERSE
// field order (SV packs MSB-first, the wire is little-endian LSB-first).
// pack/unpack address fields via explicit bit ranges from this layout —
// golden vectors (tests/abi/golden) are the enforcement, not reasoning.

package zhao_abi_pkg;

  // ---------------------------------------------------------- ABI ---
  localparam int unsigned ZHAO_ABI_VERSION        = 1;
  localparam int unsigned ZHAO_COMMAND_ALIGNMENT = 16;
  // exported ABI constants: consumed by importing modules (stub top, probe),
  // not necessarily referenced inside this package.
  /* verilator lint_off UNUSEDPARAM */
  localparam logic [31:0] FRAME_SLOT_BYTES = 32'd1048576;
  /* verilator lint_on UNUSEDPARAM */

  // frame packet (capture_format.md 3)
  localparam logic [31:0] ZHAO_FRAME_MAGIC        = 32'h314B505A;  // 'Z','P','K','1' LE
  localparam int unsigned ZHAO_FRAME_HEADER_BYTES = 36;
  localparam int unsigned ZHAO_FRAME_OVERHEAD     = 40;
  localparam logic [15:0] ZHAO_FRAME_FLAG_CONTAINS_DEBUG = 16'h0001;
  /* verilator lint_off UNUSEDPARAM */
  localparam logic [7:0]  ZHAO_COMPL_DONE = 8'h01;  // status-output bits (stub top)
  localparam logic [7:0]  ZHAO_COMPL_ERR  = 8'h02;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_OFF_MAGIC = 0;
  localparam int unsigned ZHAO_OFF_ABI_VERSION = 4;
  localparam int unsigned ZHAO_OFF_FLAGS = 6;
  localparam int unsigned ZHAO_OFF_FRAME_ID = 8;
  localparam int unsigned ZHAO_OFF_SEQUENCE = 12;
  localparam int unsigned ZHAO_OFF_RESOURCE_EPOCH = 16;
  localparam int unsigned ZHAO_OFF_DEADLINE = 20;
  localparam int unsigned ZHAO_OFF_COMMAND_COUNT = 24;
  localparam int unsigned ZHAO_OFF_COMMAND_BYTES = 28;
  localparam int unsigned ZHAO_OFF_HEADER_CRC = 32;

  // error codes — shared verbatim across C++/TS/SV (8-bit codes in v1)
  typedef enum logic [7:0] {
    ZH_ABI_OK = 8'd0,
    ZH_ABI_BAD_MAGIC = 8'd1,
    ZH_ABI_BAD_ABI_VERSION = 8'd2,
    ZH_ABI_RESERVED_FLAG = 8'd3,
    ZH_ABI_BAD_LENGTH = 8'd4,
    ZH_ABI_BAD_HEADER_CRC = 8'd5,
    ZH_ABI_BAD_PAYLOAD_CRC = 8'd6,
    ZH_ABI_UNKNOWN_OPCODE = 8'd7,
    ZH_ABI_RESERVED_FIELD = 8'd8,
    ZH_ABI_BAD_VALUE = 8'd9,
    ZH_ABI_STALE_HANDLE = 8'd10,
    ZH_ABI_TRUNCATED = 8'd11,
    ZH_ABI_DEBUG_FLAG_REQUIRED = 8'd12,
    ZH_ABI_COUNT_MISMATCH = 8'd13,
    ZH_ABI_UNIMPLEMENTED_COMMAND = 8'd14
  } zhao_abi_error_e;

  // opcodes
  localparam logic [15:0] ZHAO_OP_NOP = 16'h0000;
  localparam logic [15:0] ZHAO_OP_BEGIN_FRAME = 16'h0001;
  localparam logic [15:0] ZHAO_OP_END_FRAME = 16'h0002;
  localparam logic [15:0] ZHAO_OP_SET_VIEW = 16'h0010;
  localparam logic [15:0] ZHAO_OP_SET_PRESENTATION_CONTRACT = 16'h0020;
  localparam logic [15:0] ZHAO_OP_TERRAIN_FIELD = 16'h0200;
  localparam logic [15:0] ZHAO_OP_SURFACE_STAMP = 16'h0210;
  localparam logic [15:0] ZHAO_OP_DRAW_FORM = 16'h0300;
  localparam logic [15:0] ZHAO_OP_DRAW_POPULATION = 16'h0301;
  localparam logic [15:0] ZHAO_OP_DRAW_PROCEDURAL = 16'h0302;
  localparam logic [15:0] ZHAO_OP_EMIT_AUDIO_EVENT = 16'h0400;
  localparam logic [15:0] ZHAO_OP_DEBUG_BOOTSTRAP = 16'hF001;
  localparam logic [15:0] ZHAO_DEBUG_OPCODE_LO = 16'hF000;
  localparam logic [15:0] ZHAO_DEBUG_OPCODE_HI = 16'hF0FF;

  // CRC-32C (Castagnoli), capture_format.md 2: poly 0x82F63B78 reflected,
  // init/xorout 0xFFFFFFFF. Same table as the C++/TS modules (goldens lock).
  localparam logic [31:0] ZHAO_CRC32C_TABLE [0:255] = '{
    32'h00000000, 32'hF26B8303, 32'hE13B70F7, 32'h1350F3F4,
    32'hC79A971F, 32'h35F1141C, 32'h26A1E7E8, 32'hD4CA64EB,
    32'h8AD958CF, 32'h78B2DBCC, 32'h6BE22838, 32'h9989AB3B,
    32'h4D43CFD0, 32'hBF284CD3, 32'hAC78BF27, 32'h5E133C24,
    32'h105EC76F, 32'hE235446C, 32'hF165B798, 32'h030E349B,
    32'hD7C45070, 32'h25AFD373, 32'h36FF2087, 32'hC494A384,
    32'h9A879FA0, 32'h68EC1CA3, 32'h7BBCEF57, 32'h89D76C54,
    32'h5D1D08BF, 32'hAF768BBC, 32'hBC267848, 32'h4E4DFB4B,
    32'h20BD8EDE, 32'hD2D60DDD, 32'hC186FE29, 32'h33ED7D2A,
    32'hE72719C1, 32'h154C9AC2, 32'h061C6936, 32'hF477EA35,
    32'hAA64D611, 32'h580F5512, 32'h4B5FA6E6, 32'hB93425E5,
    32'h6DFE410E, 32'h9F95C20D, 32'h8CC531F9, 32'h7EAEB2FA,
    32'h30E349B1, 32'hC288CAB2, 32'hD1D83946, 32'h23B3BA45,
    32'hF779DEAE, 32'h05125DAD, 32'h1642AE59, 32'hE4292D5A,
    32'hBA3A117E, 32'h4851927D, 32'h5B016189, 32'hA96AE28A,
    32'h7DA08661, 32'h8FCB0562, 32'h9C9BF696, 32'h6EF07595,
    32'h417B1DBC, 32'hB3109EBF, 32'hA0406D4B, 32'h522BEE48,
    32'h86E18AA3, 32'h748A09A0, 32'h67DAFA54, 32'h95B17957,
    32'hCBA24573, 32'h39C9C670, 32'h2A993584, 32'hD8F2B687,
    32'h0C38D26C, 32'hFE53516F, 32'hED03A29B, 32'h1F682198,
    32'h5125DAD3, 32'hA34E59D0, 32'hB01EAA24, 32'h42752927,
    32'h96BF4DCC, 32'h64D4CECF, 32'h77843D3B, 32'h85EFBE38,
    32'hDBFC821C, 32'h2997011F, 32'h3AC7F2EB, 32'hC8AC71E8,
    32'h1C661503, 32'hEE0D9600, 32'hFD5D65F4, 32'h0F36E6F7,
    32'h61C69362, 32'h93AD1061, 32'h80FDE395, 32'h72966096,
    32'hA65C047D, 32'h5437877E, 32'h4767748A, 32'hB50CF789,
    32'hEB1FCBAD, 32'h197448AE, 32'h0A24BB5A, 32'hF84F3859,
    32'h2C855CB2, 32'hDEEEDFB1, 32'hCDBE2C45, 32'h3FD5AF46,
    32'h7198540D, 32'h83F3D70E, 32'h90A324FA, 32'h62C8A7F9,
    32'hB602C312, 32'h44694011, 32'h5739B3E5, 32'hA55230E6,
    32'hFB410CC2, 32'h092A8FC1, 32'h1A7A7C35, 32'hE811FF36,
    32'h3CDB9BDD, 32'hCEB018DE, 32'hDDE0EB2A, 32'h2F8B6829,
    32'h82F63B78, 32'h709DB87B, 32'h63CD4B8F, 32'h91A6C88C,
    32'h456CAC67, 32'hB7072F64, 32'hA457DC90, 32'h563C5F93,
    32'h082F63B7, 32'hFA44E0B4, 32'hE9141340, 32'h1B7F9043,
    32'hCFB5F4A8, 32'h3DDE77AB, 32'h2E8E845F, 32'hDCE5075C,
    32'h92A8FC17, 32'h60C37F14, 32'h73938CE0, 32'h81F80FE3,
    32'h55326B08, 32'hA759E80B, 32'hB4091BFF, 32'h466298FC,
    32'h1871A4D8, 32'hEA1A27DB, 32'hF94AD42F, 32'h0B21572C,
    32'hDFEB33C7, 32'h2D80B0C4, 32'h3ED04330, 32'hCCBBC033,
    32'hA24BB5A6, 32'h502036A5, 32'h4370C551, 32'hB11B4652,
    32'h65D122B9, 32'h97BAA1BA, 32'h84EA524E, 32'h7681D14D,
    32'h2892ED69, 32'hDAF96E6A, 32'hC9A99D9E, 32'h3BC21E9D,
    32'hEF087A76, 32'h1D63F975, 32'h0E330A81, 32'hFC588982,
    32'hB21572C9, 32'h407EF1CA, 32'h532E023E, 32'hA145813D,
    32'h758FE5D6, 32'h87E466D5, 32'h94B49521, 32'h66DF1622,
    32'h38CC2A06, 32'hCAA7A905, 32'hD9F75AF1, 32'h2B9CD9F2,
    32'hFF56BD19, 32'h0D3D3E1A, 32'h1E6DCDEE, 32'hEC064EED,
    32'hC38D26C4, 32'h31E6A5C7, 32'h22B65633, 32'hD0DDD530,
    32'h0417B1DB, 32'hF67C32D8, 32'hE52CC12C, 32'h1747422F,
    32'h49547E0B, 32'hBB3FFD08, 32'hA86F0EFC, 32'h5A048DFF,
    32'h8ECEE914, 32'h7CA56A17, 32'h6FF599E3, 32'h9D9E1AE0,
    32'hD3D3E1AB, 32'h21B862A8, 32'h32E8915C, 32'hC083125F,
    32'h144976B4, 32'hE622F5B7, 32'hF5720643, 32'h07198540,
    32'h590AB964, 32'hAB613A67, 32'hB831C993, 32'h4A5A4A90,
    32'h9E902E7B, 32'h6CFBAD78, 32'h7FAB5E8C, 32'h8DC0DD8F,
    32'hE330A81A, 32'h115B2B19, 32'h020BD8ED, 32'hF0605BEE,
    32'h24AA3F05, 32'hD6C1BC06, 32'hC5914FF2, 32'h37FACCF1,
    32'h69E9F0D5, 32'h9B8273D6, 32'h88D28022, 32'h7AB90321,
    32'hAE7367CA, 32'h5C18E4C9, 32'h4F48173D, 32'hBD23943E,
    32'hF36E6F75, 32'h0105EC76, 32'h12551F82, 32'hE03E9C81,
    32'h34F4F86A, 32'hC69F7B69, 32'hD5CF889D, 32'h27A40B9E,
    32'h79B737BA, 32'h8BDCB4B9, 32'h988C474D, 32'h6AE7C44E,
    32'hBE2DA0A5, 32'h4C4623A6, 32'h5F16D052, 32'hAD7D5351
  };

  // per-byte step (capture_format.md 2.2) — synthesizable alternative form
  function automatic logic [31:0] zhao_crc32c_step(input logic [31:0] c,
                                                  input logic [7:0]  d);
    logic [31:0] crc;
    begin
      crc = c ^ {24'b0, d};
      for (int i = 0; i < 8; i++)
        crc = (crc >> 1) ^ (crc[0] ? 32'h82F63B78 : 32'b0);
      zhao_crc32c_step = crc;
    end
  endfunction

  // finalized CRC over n bytes of p starting at off (running form of the spec)
  function automatic logic [31:0] zhao_crc32c_bytes(input logic [7:0] p [],
                                                   input int unsigned off,
                                                   input int unsigned n);
    logic [31:0] c;
    begin
      c = 32'hFFFFFFFF;
      for (int unsigned i = 0; i < n; i++)
        c = ZHAO_CRC32C_TABLE[c[7:0] ^ p[off+i]] ^ (c >> 8);
      zhao_crc32c_bytes = ~c;
    end
  endfunction

  function automatic logic [15:0] zhao_get16(input logic [7:0] p [],
                                            input int unsigned off);
    zhao_get16 = {p[off+1], p[off]};
  endfunction

  function automatic logic [31:0] zhao_get32(input logic [7:0] p [],
                                            input int unsigned off);
    zhao_get32 = {p[off+3], p[off+2], p[off+1], p[off]};
  endfunction

  // rectfx: 16 B (spec/commands.zidl). REVERSE field order.
  typedef struct packed {
    logic [31:0] y1;  // fx16 = Q16.16 in 32 bits (qformats.md) @12
    logic [31:0] x1;  // fx16 = Q16.16 in 32 bits (qformats.md) @8
    logic [31:0] y0;  // fx16 = Q16.16 in 32 bits (qformats.md) @4
    logic [31:0] x0;  // fx16 = Q16.16 in 32 bits (qformats.md) @0
  } zhao_rectfx_t;

  // transform2fx: 24 B (spec/commands.zidl). REVERSE field order.
  typedef struct packed {
    logic [31:0] r11;  // fx16 = Q16.16 in 32 bits (qformats.md) @20
    logic [31:0] r10;  // fx16 = Q16.16 in 32 bits (qformats.md) @16
    logic [31:0] r01;  // fx16 = Q16.16 in 32 bits (qformats.md) @12
    logic [31:0] r00;  // fx16 = Q16.16 in 32 bits (qformats.md) @8
    logic [31:0] ty;  // fx16 = Q16.16 in 32 bits (qformats.md) @4
    logic [31:0] tx;  // fx16 = Q16.16 in 32 bits (qformats.md) @0
  } zhao_transform2fx_t;

  // mat4fx: 64 B (spec/commands.zidl). REVERSE field order.
  typedef struct packed {
    logic [31:0] m33;  // fx16 = Q16.16 in 32 bits (qformats.md) @60
    logic [31:0] m32;  // fx16 = Q16.16 in 32 bits (qformats.md) @56
    logic [31:0] m31;  // fx16 = Q16.16 in 32 bits (qformats.md) @52
    logic [31:0] m30;  // fx16 = Q16.16 in 32 bits (qformats.md) @48
    logic [31:0] m23;  // fx16 = Q16.16 in 32 bits (qformats.md) @44
    logic [31:0] m22;  // fx16 = Q16.16 in 32 bits (qformats.md) @40
    logic [31:0] m21;  // fx16 = Q16.16 in 32 bits (qformats.md) @36
    logic [31:0] m20;  // fx16 = Q16.16 in 32 bits (qformats.md) @32
    logic [31:0] m13;  // fx16 = Q16.16 in 32 bits (qformats.md) @28
    logic [31:0] m12;  // fx16 = Q16.16 in 32 bits (qformats.md) @24
    logic [31:0] m11;  // fx16 = Q16.16 in 32 bits (qformats.md) @20
    logic [31:0] m10;  // fx16 = Q16.16 in 32 bits (qformats.md) @16
    logic [31:0] m03;  // fx16 = Q16.16 in 32 bits (qformats.md) @12
    logic [31:0] m02;  // fx16 = Q16.16 in 32 bits (qformats.md) @8
    logic [31:0] m01;  // fx16 = Q16.16 in 32 bits (qformats.md) @4
    logic [31:0] m00;  // fx16 = Q16.16 in 32 bits (qformats.md) @0
  } zhao_mat4fx_t;

  // Nop 0x0000: 16-B record (implemented).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_nop_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_NOP_BYTES = 16;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_NOP_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_NOP_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_NOP_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_NOP_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_NOP_OFF_H_RESERVED0 = 12;

  // BeginFrame 0x0001: 32-B record (implemented).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [31:0] deadline_cycles;  // u32 @28
    logic [31:0] flags;  // u32 @24
    logic [31:0] resource_epoch;  // u32 @20
    logic [31:0] frame_id;  // u32 @16
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_begin_frame_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_BEGIN_FRAME_BYTES = 32;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_FRAME_ID = 16;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_RESOURCE_EPOCH = 20;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_FLAGS = 24;
  localparam int unsigned ZHAO_BEGIN_FRAME_OFF_DEADLINE_CYCLES = 28;

  // EndFrame 0x0002: 32-B record (implemented).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [31:0] pad;  // 4 zero byte(s) @28
    logic [31:0] expected_framebuffer_crc;  // u32 @24
    logic [31:0] expected_crc_valid;  // u32 @20
    logic [31:0] completion_flags;  // u32 @16
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_end_frame_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_END_FRAME_BYTES = 32;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_END_FRAME_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_END_FRAME_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_END_FRAME_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_END_FRAME_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_END_FRAME_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_END_FRAME_OFF_COMPLETION_FLAGS = 16;
  localparam int unsigned ZHAO_END_FRAME_OFF_EXPECTED_CRC_VALID = 20;
  localparam int unsigned ZHAO_END_FRAME_OFF_EXPECTED_FRAMEBUFFER_CRC = 24;
  localparam int unsigned ZHAO_END_FRAME_OFF_PAD = 28;

  // SetView 0x0010: 96-B record (implemented).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [31:0] fragment_tokens;  // u32 @92
    logic [31:0] geometry_tokens;  // u32 @88
    logic [31:0] pixel_error;  // fx16 = Q16.16 in 32 bits (qformats.md) @84
    zhao_mat4fx_t view_projection;  // 64 B @20
    logic [15:0] flags;  // u16 @18
    logic [7:0] viewport_id;  // u8 @17
    logic [7:0] view_id;  // u8 @16
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_set_view_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_SET_VIEW_BYTES = 96;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_SET_VIEW_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_SET_VIEW_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_SET_VIEW_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_SET_VIEW_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_SET_VIEW_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_SET_VIEW_OFF_VIEW_ID = 16;
  localparam int unsigned ZHAO_SET_VIEW_OFF_VIEWPORT_ID = 17;
  localparam int unsigned ZHAO_SET_VIEW_OFF_FLAGS = 18;
  localparam int unsigned ZHAO_SET_VIEW_OFF_VIEW_PROJECTION = 20;
  localparam int unsigned ZHAO_SET_VIEW_OFF_PIXEL_ERROR = 84;
  localparam int unsigned ZHAO_SET_VIEW_OFF_GEOMETRY_TOKENS = 88;
  localparam int unsigned ZHAO_SET_VIEW_OFF_FRAGMENT_TOKENS = 92;

  // SetPresentationContract 0x0020: 48-B record (implemented).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [63:0] pad;  // 8 zero byte(s) @40
    logic [31:0] shared_tokens;  // u32 @36
    logic [31:0] fragment_tokens_1;  // u32 @32
    logic [31:0] fragment_tokens_0;  // u32 @28
    logic [31:0] geometry_tokens_1;  // u32 @24
    logic [31:0] geometry_tokens_0;  // u32 @20
    logic [15:0] flags;  // u16 @18
    logic [7:0] view_count;  // u8 @17
    logic [7:0] mode;  // u8 @16
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_set_presentation_contract_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_BYTES = 48;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_MODE = 16;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_VIEW_COUNT = 17;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_FLAGS = 18;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_0 = 20;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_1 = 24;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_0 = 28;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_1 = 32;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_SHARED_TOKENS = 36;
  localparam int unsigned ZHAO_SET_PRESENTATION_CONTRACT_OFF_PAD = 40;

  // TerrainField 0x0200: 112-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [31:0] pad;  // 4 zero byte(s) @108
    logic [7:0] parameters_63;  // u8 @107
    logic [7:0] parameters_62;  // u8 @106
    logic [7:0] parameters_61;  // u8 @105
    logic [7:0] parameters_60;  // u8 @104
    logic [7:0] parameters_59;  // u8 @103
    logic [7:0] parameters_58;  // u8 @102
    logic [7:0] parameters_57;  // u8 @101
    logic [7:0] parameters_56;  // u8 @100
    logic [7:0] parameters_55;  // u8 @99
    logic [7:0] parameters_54;  // u8 @98
    logic [7:0] parameters_53;  // u8 @97
    logic [7:0] parameters_52;  // u8 @96
    logic [7:0] parameters_51;  // u8 @95
    logic [7:0] parameters_50;  // u8 @94
    logic [7:0] parameters_49;  // u8 @93
    logic [7:0] parameters_48;  // u8 @92
    logic [7:0] parameters_47;  // u8 @91
    logic [7:0] parameters_46;  // u8 @90
    logic [7:0] parameters_45;  // u8 @89
    logic [7:0] parameters_44;  // u8 @88
    logic [7:0] parameters_43;  // u8 @87
    logic [7:0] parameters_42;  // u8 @86
    logic [7:0] parameters_41;  // u8 @85
    logic [7:0] parameters_40;  // u8 @84
    logic [7:0] parameters_39;  // u8 @83
    logic [7:0] parameters_38;  // u8 @82
    logic [7:0] parameters_37;  // u8 @81
    logic [7:0] parameters_36;  // u8 @80
    logic [7:0] parameters_35;  // u8 @79
    logic [7:0] parameters_34;  // u8 @78
    logic [7:0] parameters_33;  // u8 @77
    logic [7:0] parameters_32;  // u8 @76
    logic [7:0] parameters_31;  // u8 @75
    logic [7:0] parameters_30;  // u8 @74
    logic [7:0] parameters_29;  // u8 @73
    logic [7:0] parameters_28;  // u8 @72
    logic [7:0] parameters_27;  // u8 @71
    logic [7:0] parameters_26;  // u8 @70
    logic [7:0] parameters_25;  // u8 @69
    logic [7:0] parameters_24;  // u8 @68
    logic [7:0] parameters_23;  // u8 @67
    logic [7:0] parameters_22;  // u8 @66
    logic [7:0] parameters_21;  // u8 @65
    logic [7:0] parameters_20;  // u8 @64
    logic [7:0] parameters_19;  // u8 @63
    logic [7:0] parameters_18;  // u8 @62
    logic [7:0] parameters_17;  // u8 @61
    logic [7:0] parameters_16;  // u8 @60
    logic [7:0] parameters_15;  // u8 @59
    logic [7:0] parameters_14;  // u8 @58
    logic [7:0] parameters_13;  // u8 @57
    logic [7:0] parameters_12;  // u8 @56
    logic [7:0] parameters_11;  // u8 @55
    logic [7:0] parameters_10;  // u8 @54
    logic [7:0] parameters_9;  // u8 @53
    logic [7:0] parameters_8;  // u8 @52
    logic [7:0] parameters_7;  // u8 @51
    logic [7:0] parameters_6;  // u8 @50
    logic [7:0] parameters_5;  // u8 @49
    logic [7:0] parameters_4;  // u8 @48
    logic [7:0] parameters_3;  // u8 @47
    logic [7:0] parameters_2;  // u8 @46
    logic [7:0] parameters_1;  // u8 @45
    logic [7:0] parameters_0;  // u8 @44
    logic [31:0] duration_ticks;  // u32 @40
    logic [31:0] start_tick;  // u32 @36
    zhao_rectfx_t footprint;  // 16 B @20
    logic [31:0] program_f;  // handle32 @16  // handle32 {index:24, generation:8}
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_terrain_field_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_TERRAIN_FIELD_BYTES = 112;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PROGRAM = 16;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_FOOTPRINT = 20;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_START_TICK = 36;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_DURATION_TICKS = 40;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_0 = 44;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_1 = 45;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_2 = 46;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_3 = 47;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_4 = 48;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_5 = 49;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_6 = 50;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_7 = 51;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_8 = 52;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_9 = 53;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_10 = 54;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_11 = 55;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_12 = 56;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_13 = 57;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_14 = 58;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_15 = 59;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_16 = 60;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_17 = 61;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_18 = 62;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_19 = 63;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_20 = 64;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_21 = 65;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_22 = 66;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_23 = 67;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_24 = 68;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_25 = 69;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_26 = 70;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_27 = 71;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_28 = 72;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_29 = 73;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_30 = 74;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_31 = 75;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_32 = 76;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_33 = 77;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_34 = 78;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_35 = 79;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_36 = 80;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_37 = 81;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_38 = 82;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_39 = 83;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_40 = 84;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_41 = 85;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_42 = 86;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_43 = 87;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_44 = 88;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_45 = 89;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_46 = 90;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_47 = 91;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_48 = 92;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_49 = 93;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_50 = 94;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_51 = 95;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_52 = 96;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_53 = 97;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_54 = 98;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_55 = 99;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_56 = 100;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_57 = 101;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_58 = 102;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_59 = 103;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_60 = 104;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_61 = 105;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_62 = 106;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_63 = 107;
  localparam int unsigned ZHAO_TERRAIN_FIELD_OFF_PAD = 108;

  // SurfaceStamp 0x0210: 64-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [95:0] pad;  // 12 zero byte(s) @52
    zhao_transform2fx_t transform;  // 24 B @28
    logic [15:0] strength;  // u16 @26
    logic [7:0] tag;  // u8 @25
    logic [7:0] operation;  // u8 @24
    logic [31:0] patch;  // handle32 @20  // handle32 {index:24, generation:8}
    logic [31:0] brush;  // handle32 @16  // handle32 {index:24, generation:8}
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_surface_stamp_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_SURFACE_STAMP_BYTES = 64;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_BRUSH = 16;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_PATCH = 20;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_OPERATION = 24;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_TAG = 25;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_STRENGTH = 26;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_TRANSFORM = 28;
  localparam int unsigned ZHAO_SURFACE_STAMP_OFF_PAD = 52;

  // DrawForm 0x0300: 32-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [15:0] flags;  // u16 @30
    logic [7:0] semantic_weight;  // u8 @29
    logic [7:0] viewport_mask;  // u8 @28
    logic [31:0] transform;  // handle32 @24  // handle32 {index:24, generation:8}
    logic [31:0] material_set;  // handle32 @20  // handle32 {index:24, generation:8}
    logic [31:0] form;  // handle32 @16  // handle32 {index:24, generation:8}
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_draw_form_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_DRAW_FORM_BYTES = 32;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_DRAW_FORM_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_FORM = 16;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_MATERIAL_SET = 20;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_TRANSFORM = 24;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_VIEWPORT_MASK = 28;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_SEMANTIC_WEIGHT = 29;
  localparam int unsigned ZHAO_DRAW_FORM_OFF_FLAGS = 30;

  // DrawPopulation 0x0301: 32-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [63:0] pad;  // 8 zero byte(s) @24
    logic [15:0] flags;  // u16 @22
    logic [7:0] semantic_weight;  // u8 @21
    logic [7:0] viewport_mask;  // u8 @20
    logic [31:0] population;  // handle32 @16  // handle32 {index:24, generation:8}
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_draw_population_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_DRAW_POPULATION_BYTES = 32;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_POPULATION = 16;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_VIEWPORT_MASK = 20;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_SEMANTIC_WEIGHT = 21;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_FLAGS = 22;
  localparam int unsigned ZHAO_DRAW_POPULATION_OFF_PAD = 24;

  // DrawProcedural 0x0302: 64-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [95:0] pad;  // 12 zero byte(s) @52
    logic [31:0] screen_error;  // fx16 = Q16.16 in 32 bits (qformats.md) @48
    zhao_transform2fx_t transform;  // 24 B @24
    logic [31:0] material;  // handle32 @20  // handle32 {index:24, generation:8}
    logic [31:0] program_f;  // handle32 @16  // handle32 {index:24, generation:8}
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_draw_procedural_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_BYTES = 64;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_PROGRAM = 16;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_MATERIAL = 20;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_TRANSFORM = 24;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_SCREEN_ERROR = 48;
  localparam int unsigned ZHAO_DRAW_PROCEDURAL_OFF_PAD = 52;

  // EmitAudioEvent 0x0400: 32-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [31:0] timestamp;  // u32 @28
    logic [31:0] sample_handle;  // u32 @24
    logic [15:0] gain;  // u16 @22
    logic [15:0] pan_fx;  // i16 @20
    logic [31:0] event_id;  // u32 @16
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_emit_audio_event_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_BYTES = 32;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_EVENT_ID = 16;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_PAN_FX = 20;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_GAIN = 22;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_SAMPLE_HANDLE = 24;
  localparam int unsigned ZHAO_EMIT_AUDIO_EVENT_OFF_TIMESTAMP = 28;

  // DebugBootstrap 0xF001: 64-B record (reserved).
  // Command header fields first on the wire, then payload; declared reversed.
  typedef struct packed {
    logic [7:0] data_47;  // u8 @63
    logic [7:0] data_46;  // u8 @62
    logic [7:0] data_45;  // u8 @61
    logic [7:0] data_44;  // u8 @60
    logic [7:0] data_43;  // u8 @59
    logic [7:0] data_42;  // u8 @58
    logic [7:0] data_41;  // u8 @57
    logic [7:0] data_40;  // u8 @56
    logic [7:0] data_39;  // u8 @55
    logic [7:0] data_38;  // u8 @54
    logic [7:0] data_37;  // u8 @53
    logic [7:0] data_36;  // u8 @52
    logic [7:0] data_35;  // u8 @51
    logic [7:0] data_34;  // u8 @50
    logic [7:0] data_33;  // u8 @49
    logic [7:0] data_32;  // u8 @48
    logic [7:0] data_31;  // u8 @47
    logic [7:0] data_30;  // u8 @46
    logic [7:0] data_29;  // u8 @45
    logic [7:0] data_28;  // u8 @44
    logic [7:0] data_27;  // u8 @43
    logic [7:0] data_26;  // u8 @42
    logic [7:0] data_25;  // u8 @41
    logic [7:0] data_24;  // u8 @40
    logic [7:0] data_23;  // u8 @39
    logic [7:0] data_22;  // u8 @38
    logic [7:0] data_21;  // u8 @37
    logic [7:0] data_20;  // u8 @36
    logic [7:0] data_19;  // u8 @35
    logic [7:0] data_18;  // u8 @34
    logic [7:0] data_17;  // u8 @33
    logic [7:0] data_16;  // u8 @32
    logic [7:0] data_15;  // u8 @31
    logic [7:0] data_14;  // u8 @30
    logic [7:0] data_13;  // u8 @29
    logic [7:0] data_12;  // u8 @28
    logic [7:0] data_11;  // u8 @27
    logic [7:0] data_10;  // u8 @26
    logic [7:0] data_9;  // u8 @25
    logic [7:0] data_8;  // u8 @24
    logic [7:0] data_7;  // u8 @23
    logic [7:0] data_6;  // u8 @22
    logic [7:0] data_5;  // u8 @21
    logic [7:0] data_4;  // u8 @20
    logic [7:0] data_3;  // u8 @19
    logic [7:0] data_2;  // u8 @18
    logic [7:0] data_1;  // u8 @17
    logic [7:0] data_0;  // u8 @16
    logic [15:0] h_opcode;  // u16 @0
    logic [15:0] h_record_bytes;  // u16 @2
    logic [31:0] h_source_id;  // u32 @4
    logic [31:0] h_flags;  // u32 @8
    logic [31:0] h_reserved0;  // u32 @12
  } zhao_rec_debug_bootstrap_t;

  /* verilator lint_off UNUSEDPARAM */
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_BYTES = 64;
  /* verilator lint_on UNUSEDPARAM */
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_H_OPCODE = 0;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_H_RECORD_BYTES = 2;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_H_SOURCE_ID = 4;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_H_FLAGS = 8;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_H_RESERVED0 = 12;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_0 = 16;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_1 = 17;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_2 = 18;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_3 = 19;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_4 = 20;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_5 = 21;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_6 = 22;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_7 = 23;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_8 = 24;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_9 = 25;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_10 = 26;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_11 = 27;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_12 = 28;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_13 = 29;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_14 = 30;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_15 = 31;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_16 = 32;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_17 = 33;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_18 = 34;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_19 = 35;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_20 = 36;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_21 = 37;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_22 = 38;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_23 = 39;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_24 = 40;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_25 = 41;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_26 = 42;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_27 = 43;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_28 = 44;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_29 = 45;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_30 = 46;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_31 = 47;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_32 = 48;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_33 = 49;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_34 = 50;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_35 = 51;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_36 = 52;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_37 = 53;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_38 = 54;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_39 = 55;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_40 = 56;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_41 = 57;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_42 = 58;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_43 = 59;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_44 = 60;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_45 = 61;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_46 = 62;
  localparam int unsigned ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_47 = 63;

  function automatic logic [127:0] zhao_pack_rectfx(input zhao_rectfx_t c);
    logic [127:0] v;
    begin
      v[0 +: 32] = c.x0;
      v[32 +: 32] = c.y0;
      v[64 +: 32] = c.x1;
      v[96 +: 32] = c.y1;
      zhao_pack_rectfx = v;
    end
  endfunction

  function automatic zhao_rectfx_t zhao_unpack_rectfx(input logic [127:0] v);
    zhao_rectfx_t c;
    begin
      c.x0 = v[0 +: 32];
      c.y0 = v[32 +: 32];
      c.x1 = v[64 +: 32];
      c.y1 = v[96 +: 32];
      zhao_unpack_rectfx = c;
    end
  endfunction

  function automatic logic [191:0] zhao_pack_transform2fx(input zhao_transform2fx_t c);
    logic [191:0] v;
    begin
      v[0 +: 32] = c.tx;
      v[32 +: 32] = c.ty;
      v[64 +: 32] = c.r00;
      v[96 +: 32] = c.r01;
      v[128 +: 32] = c.r10;
      v[160 +: 32] = c.r11;
      zhao_pack_transform2fx = v;
    end
  endfunction

  function automatic zhao_transform2fx_t zhao_unpack_transform2fx(input logic [191:0] v);
    zhao_transform2fx_t c;
    begin
      c.tx = v[0 +: 32];
      c.ty = v[32 +: 32];
      c.r00 = v[64 +: 32];
      c.r01 = v[96 +: 32];
      c.r10 = v[128 +: 32];
      c.r11 = v[160 +: 32];
      zhao_unpack_transform2fx = c;
    end
  endfunction

  function automatic logic [511:0] zhao_pack_mat4fx(input zhao_mat4fx_t c);
    logic [511:0] v;
    begin
      v[0 +: 32] = c.m00;
      v[32 +: 32] = c.m01;
      v[64 +: 32] = c.m02;
      v[96 +: 32] = c.m03;
      v[128 +: 32] = c.m10;
      v[160 +: 32] = c.m11;
      v[192 +: 32] = c.m12;
      v[224 +: 32] = c.m13;
      v[256 +: 32] = c.m20;
      v[288 +: 32] = c.m21;
      v[320 +: 32] = c.m22;
      v[352 +: 32] = c.m23;
      v[384 +: 32] = c.m30;
      v[416 +: 32] = c.m31;
      v[448 +: 32] = c.m32;
      v[480 +: 32] = c.m33;
      zhao_pack_mat4fx = v;
    end
  endfunction

  function automatic zhao_mat4fx_t zhao_unpack_mat4fx(input logic [511:0] v);
    zhao_mat4fx_t c;
    begin
      c.m00 = v[0 +: 32];
      c.m01 = v[32 +: 32];
      c.m02 = v[64 +: 32];
      c.m03 = v[96 +: 32];
      c.m10 = v[128 +: 32];
      c.m11 = v[160 +: 32];
      c.m12 = v[192 +: 32];
      c.m13 = v[224 +: 32];
      c.m20 = v[256 +: 32];
      c.m21 = v[288 +: 32];
      c.m22 = v[320 +: 32];
      c.m23 = v[352 +: 32];
      c.m30 = v[384 +: 32];
      c.m31 = v[416 +: 32];
      c.m32 = v[448 +: 32];
      c.m33 = v[480 +: 32];
      zhao_unpack_mat4fx = c;
    end
  endfunction

  function automatic logic [127:0] zhao_pack_nop(input zhao_rec_nop_t c);
    logic [127:0] v;
    begin
      v[ZHAO_NOP_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_NOP_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_NOP_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_NOP_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_NOP_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      zhao_pack_nop = v;
    end
  endfunction

  function automatic zhao_rec_nop_t zhao_unpack_nop(input logic [127:0] v);
    zhao_rec_nop_t c;
    begin
      c.h_opcode = v[ZHAO_NOP_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_NOP_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_NOP_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_NOP_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_NOP_OFF_H_RESERVED0*8 +: 32];
      zhao_unpack_nop = c;
    end
  endfunction

  function automatic logic [255:0] zhao_pack_begin_frame(input zhao_rec_begin_frame_t c);
    logic [255:0] v;
    begin
      v[ZHAO_BEGIN_FRAME_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_BEGIN_FRAME_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_BEGIN_FRAME_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_BEGIN_FRAME_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_BEGIN_FRAME_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_BEGIN_FRAME_OFF_FRAME_ID*8 +: 32] = c.frame_id;
      v[ZHAO_BEGIN_FRAME_OFF_RESOURCE_EPOCH*8 +: 32] = c.resource_epoch;
      v[ZHAO_BEGIN_FRAME_OFF_FLAGS*8 +: 32] = c.flags;
      v[ZHAO_BEGIN_FRAME_OFF_DEADLINE_CYCLES*8 +: 32] = c.deadline_cycles;
      zhao_pack_begin_frame = v;
    end
  endfunction

  function automatic zhao_rec_begin_frame_t zhao_unpack_begin_frame(input logic [255:0] v);
    zhao_rec_begin_frame_t c;
    begin
      c.h_opcode = v[ZHAO_BEGIN_FRAME_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_BEGIN_FRAME_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_BEGIN_FRAME_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_BEGIN_FRAME_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_BEGIN_FRAME_OFF_H_RESERVED0*8 +: 32];
      c.frame_id = v[ZHAO_BEGIN_FRAME_OFF_FRAME_ID*8 +: 32];
      c.resource_epoch = v[ZHAO_BEGIN_FRAME_OFF_RESOURCE_EPOCH*8 +: 32];
      c.flags = v[ZHAO_BEGIN_FRAME_OFF_FLAGS*8 +: 32];
      c.deadline_cycles = v[ZHAO_BEGIN_FRAME_OFF_DEADLINE_CYCLES*8 +: 32];
      zhao_unpack_begin_frame = c;
    end
  endfunction

  function automatic logic [255:0] zhao_pack_end_frame(input zhao_rec_end_frame_t c);
    logic [255:0] v;
    begin
      v[ZHAO_END_FRAME_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_END_FRAME_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_END_FRAME_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_END_FRAME_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_END_FRAME_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_END_FRAME_OFF_COMPLETION_FLAGS*8 +: 32] = c.completion_flags;
      v[ZHAO_END_FRAME_OFF_EXPECTED_CRC_VALID*8 +: 32] = c.expected_crc_valid;
      v[ZHAO_END_FRAME_OFF_EXPECTED_FRAMEBUFFER_CRC*8 +: 32] = c.expected_framebuffer_crc;
      v[ZHAO_END_FRAME_OFF_PAD*8 +: 32] = c.pad;
      zhao_pack_end_frame = v;
    end
  endfunction

  function automatic zhao_rec_end_frame_t zhao_unpack_end_frame(input logic [255:0] v);
    zhao_rec_end_frame_t c;
    begin
      c.h_opcode = v[ZHAO_END_FRAME_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_END_FRAME_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_END_FRAME_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_END_FRAME_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_END_FRAME_OFF_H_RESERVED0*8 +: 32];
      c.completion_flags = v[ZHAO_END_FRAME_OFF_COMPLETION_FLAGS*8 +: 32];
      c.expected_crc_valid = v[ZHAO_END_FRAME_OFF_EXPECTED_CRC_VALID*8 +: 32];
      c.expected_framebuffer_crc = v[ZHAO_END_FRAME_OFF_EXPECTED_FRAMEBUFFER_CRC*8 +: 32];
      c.pad = v[ZHAO_END_FRAME_OFF_PAD*8 +: 32];
      zhao_unpack_end_frame = c;
    end
  endfunction

  function automatic logic [767:0] zhao_pack_set_view(input zhao_rec_set_view_t c);
    logic [767:0] v;
    begin
      v[ZHAO_SET_VIEW_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_SET_VIEW_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_SET_VIEW_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_SET_VIEW_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_SET_VIEW_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_SET_VIEW_OFF_VIEW_ID*8 +: 8] = c.view_id;
      v[ZHAO_SET_VIEW_OFF_VIEWPORT_ID*8 +: 8] = c.viewport_id;
      v[ZHAO_SET_VIEW_OFF_FLAGS*8 +: 16] = c.flags;
      v[ZHAO_SET_VIEW_OFF_VIEW_PROJECTION*8 +: 512] = c.view_projection;
      v[ZHAO_SET_VIEW_OFF_PIXEL_ERROR*8 +: 32] = c.pixel_error;
      v[ZHAO_SET_VIEW_OFF_GEOMETRY_TOKENS*8 +: 32] = c.geometry_tokens;
      v[ZHAO_SET_VIEW_OFF_FRAGMENT_TOKENS*8 +: 32] = c.fragment_tokens;
      zhao_pack_set_view = v;
    end
  endfunction

  function automatic zhao_rec_set_view_t zhao_unpack_set_view(input logic [767:0] v);
    zhao_rec_set_view_t c;
    begin
      c.h_opcode = v[ZHAO_SET_VIEW_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_SET_VIEW_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_SET_VIEW_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_SET_VIEW_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_SET_VIEW_OFF_H_RESERVED0*8 +: 32];
      c.view_id = v[ZHAO_SET_VIEW_OFF_VIEW_ID*8 +: 8];
      c.viewport_id = v[ZHAO_SET_VIEW_OFF_VIEWPORT_ID*8 +: 8];
      c.flags = v[ZHAO_SET_VIEW_OFF_FLAGS*8 +: 16];
      c.view_projection = v[ZHAO_SET_VIEW_OFF_VIEW_PROJECTION*8 +: 512];
      c.pixel_error = v[ZHAO_SET_VIEW_OFF_PIXEL_ERROR*8 +: 32];
      c.geometry_tokens = v[ZHAO_SET_VIEW_OFF_GEOMETRY_TOKENS*8 +: 32];
      c.fragment_tokens = v[ZHAO_SET_VIEW_OFF_FRAGMENT_TOKENS*8 +: 32];
      zhao_unpack_set_view = c;
    end
  endfunction

  function automatic logic [383:0] zhao_pack_set_presentation_contract(input zhao_rec_set_presentation_contract_t c);
    logic [383:0] v;
    begin
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_MODE*8 +: 8] = c.mode;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_VIEW_COUNT*8 +: 8] = c.view_count;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_FLAGS*8 +: 16] = c.flags;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_0*8 +: 32] = c.geometry_tokens_0;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_1*8 +: 32] = c.geometry_tokens_1;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_0*8 +: 32] = c.fragment_tokens_0;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_1*8 +: 32] = c.fragment_tokens_1;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_SHARED_TOKENS*8 +: 32] = c.shared_tokens;
      v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_PAD*8 +: 64] = c.pad;
      zhao_pack_set_presentation_contract = v;
    end
  endfunction

  function automatic zhao_rec_set_presentation_contract_t zhao_unpack_set_presentation_contract(input logic [383:0] v);
    zhao_rec_set_presentation_contract_t c;
    begin
      c.h_opcode = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_H_RESERVED0*8 +: 32];
      c.mode = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_MODE*8 +: 8];
      c.view_count = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_VIEW_COUNT*8 +: 8];
      c.flags = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_FLAGS*8 +: 16];
      c.geometry_tokens_0 = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_0*8 +: 32];
      c.geometry_tokens_1 = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_GEOMETRY_TOKENS_1*8 +: 32];
      c.fragment_tokens_0 = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_0*8 +: 32];
      c.fragment_tokens_1 = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_FRAGMENT_TOKENS_1*8 +: 32];
      c.shared_tokens = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_SHARED_TOKENS*8 +: 32];
      c.pad = v[ZHAO_SET_PRESENTATION_CONTRACT_OFF_PAD*8 +: 64];
      zhao_unpack_set_presentation_contract = c;
    end
  endfunction

  function automatic logic [895:0] zhao_pack_terrain_field(input zhao_rec_terrain_field_t c);
    logic [895:0] v;
    begin
      v[ZHAO_TERRAIN_FIELD_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_TERRAIN_FIELD_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_TERRAIN_FIELD_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_TERRAIN_FIELD_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_TERRAIN_FIELD_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_TERRAIN_FIELD_OFF_PROGRAM*8 +: 32] = c.program_f;
      v[ZHAO_TERRAIN_FIELD_OFF_FOOTPRINT*8 +: 128] = c.footprint;
      v[ZHAO_TERRAIN_FIELD_OFF_START_TICK*8 +: 32] = c.start_tick;
      v[ZHAO_TERRAIN_FIELD_OFF_DURATION_TICKS*8 +: 32] = c.duration_ticks;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_0*8 +: 8] = c.parameters_0;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_1*8 +: 8] = c.parameters_1;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_2*8 +: 8] = c.parameters_2;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_3*8 +: 8] = c.parameters_3;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_4*8 +: 8] = c.parameters_4;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_5*8 +: 8] = c.parameters_5;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_6*8 +: 8] = c.parameters_6;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_7*8 +: 8] = c.parameters_7;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_8*8 +: 8] = c.parameters_8;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_9*8 +: 8] = c.parameters_9;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_10*8 +: 8] = c.parameters_10;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_11*8 +: 8] = c.parameters_11;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_12*8 +: 8] = c.parameters_12;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_13*8 +: 8] = c.parameters_13;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_14*8 +: 8] = c.parameters_14;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_15*8 +: 8] = c.parameters_15;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_16*8 +: 8] = c.parameters_16;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_17*8 +: 8] = c.parameters_17;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_18*8 +: 8] = c.parameters_18;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_19*8 +: 8] = c.parameters_19;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_20*8 +: 8] = c.parameters_20;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_21*8 +: 8] = c.parameters_21;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_22*8 +: 8] = c.parameters_22;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_23*8 +: 8] = c.parameters_23;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_24*8 +: 8] = c.parameters_24;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_25*8 +: 8] = c.parameters_25;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_26*8 +: 8] = c.parameters_26;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_27*8 +: 8] = c.parameters_27;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_28*8 +: 8] = c.parameters_28;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_29*8 +: 8] = c.parameters_29;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_30*8 +: 8] = c.parameters_30;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_31*8 +: 8] = c.parameters_31;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_32*8 +: 8] = c.parameters_32;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_33*8 +: 8] = c.parameters_33;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_34*8 +: 8] = c.parameters_34;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_35*8 +: 8] = c.parameters_35;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_36*8 +: 8] = c.parameters_36;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_37*8 +: 8] = c.parameters_37;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_38*8 +: 8] = c.parameters_38;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_39*8 +: 8] = c.parameters_39;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_40*8 +: 8] = c.parameters_40;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_41*8 +: 8] = c.parameters_41;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_42*8 +: 8] = c.parameters_42;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_43*8 +: 8] = c.parameters_43;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_44*8 +: 8] = c.parameters_44;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_45*8 +: 8] = c.parameters_45;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_46*8 +: 8] = c.parameters_46;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_47*8 +: 8] = c.parameters_47;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_48*8 +: 8] = c.parameters_48;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_49*8 +: 8] = c.parameters_49;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_50*8 +: 8] = c.parameters_50;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_51*8 +: 8] = c.parameters_51;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_52*8 +: 8] = c.parameters_52;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_53*8 +: 8] = c.parameters_53;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_54*8 +: 8] = c.parameters_54;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_55*8 +: 8] = c.parameters_55;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_56*8 +: 8] = c.parameters_56;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_57*8 +: 8] = c.parameters_57;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_58*8 +: 8] = c.parameters_58;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_59*8 +: 8] = c.parameters_59;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_60*8 +: 8] = c.parameters_60;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_61*8 +: 8] = c.parameters_61;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_62*8 +: 8] = c.parameters_62;
      v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_63*8 +: 8] = c.parameters_63;
      v[ZHAO_TERRAIN_FIELD_OFF_PAD*8 +: 32] = c.pad;
      zhao_pack_terrain_field = v;
    end
  endfunction

  function automatic zhao_rec_terrain_field_t zhao_unpack_terrain_field(input logic [895:0] v);
    zhao_rec_terrain_field_t c;
    begin
      c.h_opcode = v[ZHAO_TERRAIN_FIELD_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_TERRAIN_FIELD_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_TERRAIN_FIELD_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_TERRAIN_FIELD_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_TERRAIN_FIELD_OFF_H_RESERVED0*8 +: 32];
      c.program_f = v[ZHAO_TERRAIN_FIELD_OFF_PROGRAM*8 +: 32];
      c.footprint = v[ZHAO_TERRAIN_FIELD_OFF_FOOTPRINT*8 +: 128];
      c.start_tick = v[ZHAO_TERRAIN_FIELD_OFF_START_TICK*8 +: 32];
      c.duration_ticks = v[ZHAO_TERRAIN_FIELD_OFF_DURATION_TICKS*8 +: 32];
      c.parameters_0 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_0*8 +: 8];
      c.parameters_1 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_1*8 +: 8];
      c.parameters_2 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_2*8 +: 8];
      c.parameters_3 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_3*8 +: 8];
      c.parameters_4 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_4*8 +: 8];
      c.parameters_5 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_5*8 +: 8];
      c.parameters_6 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_6*8 +: 8];
      c.parameters_7 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_7*8 +: 8];
      c.parameters_8 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_8*8 +: 8];
      c.parameters_9 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_9*8 +: 8];
      c.parameters_10 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_10*8 +: 8];
      c.parameters_11 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_11*8 +: 8];
      c.parameters_12 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_12*8 +: 8];
      c.parameters_13 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_13*8 +: 8];
      c.parameters_14 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_14*8 +: 8];
      c.parameters_15 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_15*8 +: 8];
      c.parameters_16 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_16*8 +: 8];
      c.parameters_17 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_17*8 +: 8];
      c.parameters_18 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_18*8 +: 8];
      c.parameters_19 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_19*8 +: 8];
      c.parameters_20 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_20*8 +: 8];
      c.parameters_21 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_21*8 +: 8];
      c.parameters_22 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_22*8 +: 8];
      c.parameters_23 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_23*8 +: 8];
      c.parameters_24 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_24*8 +: 8];
      c.parameters_25 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_25*8 +: 8];
      c.parameters_26 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_26*8 +: 8];
      c.parameters_27 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_27*8 +: 8];
      c.parameters_28 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_28*8 +: 8];
      c.parameters_29 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_29*8 +: 8];
      c.parameters_30 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_30*8 +: 8];
      c.parameters_31 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_31*8 +: 8];
      c.parameters_32 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_32*8 +: 8];
      c.parameters_33 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_33*8 +: 8];
      c.parameters_34 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_34*8 +: 8];
      c.parameters_35 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_35*8 +: 8];
      c.parameters_36 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_36*8 +: 8];
      c.parameters_37 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_37*8 +: 8];
      c.parameters_38 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_38*8 +: 8];
      c.parameters_39 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_39*8 +: 8];
      c.parameters_40 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_40*8 +: 8];
      c.parameters_41 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_41*8 +: 8];
      c.parameters_42 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_42*8 +: 8];
      c.parameters_43 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_43*8 +: 8];
      c.parameters_44 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_44*8 +: 8];
      c.parameters_45 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_45*8 +: 8];
      c.parameters_46 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_46*8 +: 8];
      c.parameters_47 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_47*8 +: 8];
      c.parameters_48 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_48*8 +: 8];
      c.parameters_49 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_49*8 +: 8];
      c.parameters_50 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_50*8 +: 8];
      c.parameters_51 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_51*8 +: 8];
      c.parameters_52 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_52*8 +: 8];
      c.parameters_53 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_53*8 +: 8];
      c.parameters_54 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_54*8 +: 8];
      c.parameters_55 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_55*8 +: 8];
      c.parameters_56 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_56*8 +: 8];
      c.parameters_57 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_57*8 +: 8];
      c.parameters_58 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_58*8 +: 8];
      c.parameters_59 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_59*8 +: 8];
      c.parameters_60 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_60*8 +: 8];
      c.parameters_61 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_61*8 +: 8];
      c.parameters_62 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_62*8 +: 8];
      c.parameters_63 = v[ZHAO_TERRAIN_FIELD_OFF_PARAMETERS_63*8 +: 8];
      c.pad = v[ZHAO_TERRAIN_FIELD_OFF_PAD*8 +: 32];
      zhao_unpack_terrain_field = c;
    end
  endfunction

  function automatic logic [511:0] zhao_pack_surface_stamp(input zhao_rec_surface_stamp_t c);
    logic [511:0] v;
    begin
      v[ZHAO_SURFACE_STAMP_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_SURFACE_STAMP_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_SURFACE_STAMP_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_SURFACE_STAMP_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_SURFACE_STAMP_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_SURFACE_STAMP_OFF_BRUSH*8 +: 32] = c.brush;
      v[ZHAO_SURFACE_STAMP_OFF_PATCH*8 +: 32] = c.patch;
      v[ZHAO_SURFACE_STAMP_OFF_OPERATION*8 +: 8] = c.operation;
      v[ZHAO_SURFACE_STAMP_OFF_TAG*8 +: 8] = c.tag;
      v[ZHAO_SURFACE_STAMP_OFF_STRENGTH*8 +: 16] = c.strength;
      v[ZHAO_SURFACE_STAMP_OFF_TRANSFORM*8 +: 192] = c.transform;
      v[ZHAO_SURFACE_STAMP_OFF_PAD*8 +: 96] = c.pad;
      zhao_pack_surface_stamp = v;
    end
  endfunction

  function automatic zhao_rec_surface_stamp_t zhao_unpack_surface_stamp(input logic [511:0] v);
    zhao_rec_surface_stamp_t c;
    begin
      c.h_opcode = v[ZHAO_SURFACE_STAMP_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_SURFACE_STAMP_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_SURFACE_STAMP_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_SURFACE_STAMP_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_SURFACE_STAMP_OFF_H_RESERVED0*8 +: 32];
      c.brush = v[ZHAO_SURFACE_STAMP_OFF_BRUSH*8 +: 32];
      c.patch = v[ZHAO_SURFACE_STAMP_OFF_PATCH*8 +: 32];
      c.operation = v[ZHAO_SURFACE_STAMP_OFF_OPERATION*8 +: 8];
      c.tag = v[ZHAO_SURFACE_STAMP_OFF_TAG*8 +: 8];
      c.strength = v[ZHAO_SURFACE_STAMP_OFF_STRENGTH*8 +: 16];
      c.transform = v[ZHAO_SURFACE_STAMP_OFF_TRANSFORM*8 +: 192];
      c.pad = v[ZHAO_SURFACE_STAMP_OFF_PAD*8 +: 96];
      zhao_unpack_surface_stamp = c;
    end
  endfunction

  function automatic logic [255:0] zhao_pack_draw_form(input zhao_rec_draw_form_t c);
    logic [255:0] v;
    begin
      v[ZHAO_DRAW_FORM_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_DRAW_FORM_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_DRAW_FORM_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_DRAW_FORM_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_DRAW_FORM_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_DRAW_FORM_OFF_FORM*8 +: 32] = c.form;
      v[ZHAO_DRAW_FORM_OFF_MATERIAL_SET*8 +: 32] = c.material_set;
      v[ZHAO_DRAW_FORM_OFF_TRANSFORM*8 +: 32] = c.transform;
      v[ZHAO_DRAW_FORM_OFF_VIEWPORT_MASK*8 +: 8] = c.viewport_mask;
      v[ZHAO_DRAW_FORM_OFF_SEMANTIC_WEIGHT*8 +: 8] = c.semantic_weight;
      v[ZHAO_DRAW_FORM_OFF_FLAGS*8 +: 16] = c.flags;
      zhao_pack_draw_form = v;
    end
  endfunction

  function automatic zhao_rec_draw_form_t zhao_unpack_draw_form(input logic [255:0] v);
    zhao_rec_draw_form_t c;
    begin
      c.h_opcode = v[ZHAO_DRAW_FORM_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_DRAW_FORM_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_DRAW_FORM_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_DRAW_FORM_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_DRAW_FORM_OFF_H_RESERVED0*8 +: 32];
      c.form = v[ZHAO_DRAW_FORM_OFF_FORM*8 +: 32];
      c.material_set = v[ZHAO_DRAW_FORM_OFF_MATERIAL_SET*8 +: 32];
      c.transform = v[ZHAO_DRAW_FORM_OFF_TRANSFORM*8 +: 32];
      c.viewport_mask = v[ZHAO_DRAW_FORM_OFF_VIEWPORT_MASK*8 +: 8];
      c.semantic_weight = v[ZHAO_DRAW_FORM_OFF_SEMANTIC_WEIGHT*8 +: 8];
      c.flags = v[ZHAO_DRAW_FORM_OFF_FLAGS*8 +: 16];
      zhao_unpack_draw_form = c;
    end
  endfunction

  function automatic logic [255:0] zhao_pack_draw_population(input zhao_rec_draw_population_t c);
    logic [255:0] v;
    begin
      v[ZHAO_DRAW_POPULATION_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_DRAW_POPULATION_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_DRAW_POPULATION_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_DRAW_POPULATION_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_DRAW_POPULATION_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_DRAW_POPULATION_OFF_POPULATION*8 +: 32] = c.population;
      v[ZHAO_DRAW_POPULATION_OFF_VIEWPORT_MASK*8 +: 8] = c.viewport_mask;
      v[ZHAO_DRAW_POPULATION_OFF_SEMANTIC_WEIGHT*8 +: 8] = c.semantic_weight;
      v[ZHAO_DRAW_POPULATION_OFF_FLAGS*8 +: 16] = c.flags;
      v[ZHAO_DRAW_POPULATION_OFF_PAD*8 +: 64] = c.pad;
      zhao_pack_draw_population = v;
    end
  endfunction

  function automatic zhao_rec_draw_population_t zhao_unpack_draw_population(input logic [255:0] v);
    zhao_rec_draw_population_t c;
    begin
      c.h_opcode = v[ZHAO_DRAW_POPULATION_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_DRAW_POPULATION_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_DRAW_POPULATION_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_DRAW_POPULATION_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_DRAW_POPULATION_OFF_H_RESERVED0*8 +: 32];
      c.population = v[ZHAO_DRAW_POPULATION_OFF_POPULATION*8 +: 32];
      c.viewport_mask = v[ZHAO_DRAW_POPULATION_OFF_VIEWPORT_MASK*8 +: 8];
      c.semantic_weight = v[ZHAO_DRAW_POPULATION_OFF_SEMANTIC_WEIGHT*8 +: 8];
      c.flags = v[ZHAO_DRAW_POPULATION_OFF_FLAGS*8 +: 16];
      c.pad = v[ZHAO_DRAW_POPULATION_OFF_PAD*8 +: 64];
      zhao_unpack_draw_population = c;
    end
  endfunction

  function automatic logic [511:0] zhao_pack_draw_procedural(input zhao_rec_draw_procedural_t c);
    logic [511:0] v;
    begin
      v[ZHAO_DRAW_PROCEDURAL_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_DRAW_PROCEDURAL_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_DRAW_PROCEDURAL_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_DRAW_PROCEDURAL_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_DRAW_PROCEDURAL_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_DRAW_PROCEDURAL_OFF_PROGRAM*8 +: 32] = c.program_f;
      v[ZHAO_DRAW_PROCEDURAL_OFF_MATERIAL*8 +: 32] = c.material;
      v[ZHAO_DRAW_PROCEDURAL_OFF_TRANSFORM*8 +: 192] = c.transform;
      v[ZHAO_DRAW_PROCEDURAL_OFF_SCREEN_ERROR*8 +: 32] = c.screen_error;
      v[ZHAO_DRAW_PROCEDURAL_OFF_PAD*8 +: 96] = c.pad;
      zhao_pack_draw_procedural = v;
    end
  endfunction

  function automatic zhao_rec_draw_procedural_t zhao_unpack_draw_procedural(input logic [511:0] v);
    zhao_rec_draw_procedural_t c;
    begin
      c.h_opcode = v[ZHAO_DRAW_PROCEDURAL_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_DRAW_PROCEDURAL_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_DRAW_PROCEDURAL_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_DRAW_PROCEDURAL_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_DRAW_PROCEDURAL_OFF_H_RESERVED0*8 +: 32];
      c.program_f = v[ZHAO_DRAW_PROCEDURAL_OFF_PROGRAM*8 +: 32];
      c.material = v[ZHAO_DRAW_PROCEDURAL_OFF_MATERIAL*8 +: 32];
      c.transform = v[ZHAO_DRAW_PROCEDURAL_OFF_TRANSFORM*8 +: 192];
      c.screen_error = v[ZHAO_DRAW_PROCEDURAL_OFF_SCREEN_ERROR*8 +: 32];
      c.pad = v[ZHAO_DRAW_PROCEDURAL_OFF_PAD*8 +: 96];
      zhao_unpack_draw_procedural = c;
    end
  endfunction

  function automatic logic [255:0] zhao_pack_emit_audio_event(input zhao_rec_emit_audio_event_t c);
    logic [255:0] v;
    begin
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_EVENT_ID*8 +: 32] = c.event_id;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_PAN_FX*8 +: 16] = c.pan_fx;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_GAIN*8 +: 16] = c.gain;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_SAMPLE_HANDLE*8 +: 32] = c.sample_handle;
      v[ZHAO_EMIT_AUDIO_EVENT_OFF_TIMESTAMP*8 +: 32] = c.timestamp;
      zhao_pack_emit_audio_event = v;
    end
  endfunction

  function automatic zhao_rec_emit_audio_event_t zhao_unpack_emit_audio_event(input logic [255:0] v);
    zhao_rec_emit_audio_event_t c;
    begin
      c.h_opcode = v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_EMIT_AUDIO_EVENT_OFF_H_RESERVED0*8 +: 32];
      c.event_id = v[ZHAO_EMIT_AUDIO_EVENT_OFF_EVENT_ID*8 +: 32];
      c.pan_fx = v[ZHAO_EMIT_AUDIO_EVENT_OFF_PAN_FX*8 +: 16];
      c.gain = v[ZHAO_EMIT_AUDIO_EVENT_OFF_GAIN*8 +: 16];
      c.sample_handle = v[ZHAO_EMIT_AUDIO_EVENT_OFF_SAMPLE_HANDLE*8 +: 32];
      c.timestamp = v[ZHAO_EMIT_AUDIO_EVENT_OFF_TIMESTAMP*8 +: 32];
      zhao_unpack_emit_audio_event = c;
    end
  endfunction

  function automatic logic [511:0] zhao_pack_debug_bootstrap(input zhao_rec_debug_bootstrap_t c);
    logic [511:0] v;
    begin
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_OPCODE*8 +: 16] = c.h_opcode;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_RECORD_BYTES*8 +: 16] = c.h_record_bytes;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_SOURCE_ID*8 +: 32] = c.h_source_id;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_FLAGS*8 +: 32] = c.h_flags;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_RESERVED0*8 +: 32] = c.h_reserved0;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_0*8 +: 8] = c.data_0;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_1*8 +: 8] = c.data_1;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_2*8 +: 8] = c.data_2;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_3*8 +: 8] = c.data_3;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_4*8 +: 8] = c.data_4;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_5*8 +: 8] = c.data_5;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_6*8 +: 8] = c.data_6;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_7*8 +: 8] = c.data_7;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_8*8 +: 8] = c.data_8;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_9*8 +: 8] = c.data_9;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_10*8 +: 8] = c.data_10;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_11*8 +: 8] = c.data_11;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_12*8 +: 8] = c.data_12;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_13*8 +: 8] = c.data_13;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_14*8 +: 8] = c.data_14;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_15*8 +: 8] = c.data_15;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_16*8 +: 8] = c.data_16;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_17*8 +: 8] = c.data_17;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_18*8 +: 8] = c.data_18;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_19*8 +: 8] = c.data_19;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_20*8 +: 8] = c.data_20;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_21*8 +: 8] = c.data_21;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_22*8 +: 8] = c.data_22;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_23*8 +: 8] = c.data_23;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_24*8 +: 8] = c.data_24;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_25*8 +: 8] = c.data_25;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_26*8 +: 8] = c.data_26;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_27*8 +: 8] = c.data_27;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_28*8 +: 8] = c.data_28;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_29*8 +: 8] = c.data_29;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_30*8 +: 8] = c.data_30;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_31*8 +: 8] = c.data_31;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_32*8 +: 8] = c.data_32;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_33*8 +: 8] = c.data_33;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_34*8 +: 8] = c.data_34;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_35*8 +: 8] = c.data_35;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_36*8 +: 8] = c.data_36;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_37*8 +: 8] = c.data_37;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_38*8 +: 8] = c.data_38;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_39*8 +: 8] = c.data_39;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_40*8 +: 8] = c.data_40;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_41*8 +: 8] = c.data_41;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_42*8 +: 8] = c.data_42;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_43*8 +: 8] = c.data_43;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_44*8 +: 8] = c.data_44;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_45*8 +: 8] = c.data_45;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_46*8 +: 8] = c.data_46;
      v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_47*8 +: 8] = c.data_47;
      zhao_pack_debug_bootstrap = v;
    end
  endfunction

  function automatic zhao_rec_debug_bootstrap_t zhao_unpack_debug_bootstrap(input logic [511:0] v);
    zhao_rec_debug_bootstrap_t c;
    begin
      c.h_opcode = v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_OPCODE*8 +: 16];
      c.h_record_bytes = v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_RECORD_BYTES*8 +: 16];
      c.h_source_id = v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_SOURCE_ID*8 +: 32];
      c.h_flags = v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_FLAGS*8 +: 32];
      c.h_reserved0 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_H_RESERVED0*8 +: 32];
      c.data_0 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_0*8 +: 8];
      c.data_1 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_1*8 +: 8];
      c.data_2 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_2*8 +: 8];
      c.data_3 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_3*8 +: 8];
      c.data_4 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_4*8 +: 8];
      c.data_5 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_5*8 +: 8];
      c.data_6 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_6*8 +: 8];
      c.data_7 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_7*8 +: 8];
      c.data_8 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_8*8 +: 8];
      c.data_9 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_9*8 +: 8];
      c.data_10 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_10*8 +: 8];
      c.data_11 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_11*8 +: 8];
      c.data_12 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_12*8 +: 8];
      c.data_13 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_13*8 +: 8];
      c.data_14 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_14*8 +: 8];
      c.data_15 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_15*8 +: 8];
      c.data_16 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_16*8 +: 8];
      c.data_17 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_17*8 +: 8];
      c.data_18 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_18*8 +: 8];
      c.data_19 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_19*8 +: 8];
      c.data_20 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_20*8 +: 8];
      c.data_21 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_21*8 +: 8];
      c.data_22 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_22*8 +: 8];
      c.data_23 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_23*8 +: 8];
      c.data_24 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_24*8 +: 8];
      c.data_25 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_25*8 +: 8];
      c.data_26 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_26*8 +: 8];
      c.data_27 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_27*8 +: 8];
      c.data_28 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_28*8 +: 8];
      c.data_29 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_29*8 +: 8];
      c.data_30 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_30*8 +: 8];
      c.data_31 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_31*8 +: 8];
      c.data_32 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_32*8 +: 8];
      c.data_33 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_33*8 +: 8];
      c.data_34 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_34*8 +: 8];
      c.data_35 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_35*8 +: 8];
      c.data_36 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_36*8 +: 8];
      c.data_37 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_37*8 +: 8];
      c.data_38 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_38*8 +: 8];
      c.data_39 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_39*8 +: 8];
      c.data_40 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_40*8 +: 8];
      c.data_41 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_41*8 +: 8];
      c.data_42 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_42*8 +: 8];
      c.data_43 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_43*8 +: 8];
      c.data_44 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_44*8 +: 8];
      c.data_45 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_45*8 +: 8];
      c.data_46 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_46*8 +: 8];
      c.data_47 = v[ZHAO_DEBUG_BOOTSTRAP_OFF_DATA_47*8 +: 8];
      zhao_unpack_debug_bootstrap = c;
    end
  endfunction

  // 0 = unknown opcode (capture_format.md 3.2 step 5)
  function automatic int unsigned zhao_opcode_record_bytes(input logic [15:0] op);
    begin
      case (op)
        ZHAO_OP_NOP: zhao_opcode_record_bytes = 16;
        ZHAO_OP_BEGIN_FRAME: zhao_opcode_record_bytes = 32;
        ZHAO_OP_END_FRAME: zhao_opcode_record_bytes = 32;
        ZHAO_OP_SET_VIEW: zhao_opcode_record_bytes = 96;
        ZHAO_OP_SET_PRESENTATION_CONTRACT: zhao_opcode_record_bytes = 48;
        ZHAO_OP_TERRAIN_FIELD: zhao_opcode_record_bytes = 112;
        ZHAO_OP_SURFACE_STAMP: zhao_opcode_record_bytes = 64;
        ZHAO_OP_DRAW_FORM: zhao_opcode_record_bytes = 32;
        ZHAO_OP_DRAW_POPULATION: zhao_opcode_record_bytes = 32;
        ZHAO_OP_DRAW_PROCEDURAL: zhao_opcode_record_bytes = 64;
        ZHAO_OP_EMIT_AUDIO_EVENT: zhao_opcode_record_bytes = 32;
        ZHAO_OP_DEBUG_BOOTSTRAP: zhao_opcode_record_bytes = 64;
        default: zhao_opcode_record_bytes = 0;
      endcase
    end
  endfunction

  // true if any byte in p[base+off .. base+off+n) is nonzero
  function automatic logic zhao_bytes_nonzero(input logic [7:0] p [],
                                             input int unsigned base,
                                             input int unsigned off,
                                             input int unsigned n);
    begin
      zhao_bytes_nonzero = 1'b0;
      for (int unsigned i = 0; i < n; i++)
        if (p[base+off+i] != 8'h00) zhao_bytes_nonzero = 1'b1;
    end
  endfunction

  // true if any declared pad byte of the record at stream offset off is nonzero
  function automatic logic zhao_record_pad_nonzero(input logic [15:0] op,
                                                  input logic [7:0] p [],
                                                  input int unsigned base);
    begin
      zhao_record_pad_nonzero = 1'b0;
      case (op)
        ZHAO_OP_END_FRAME: begin
          if (zhao_bytes_nonzero(p, base, 28, 4)) zhao_record_pad_nonzero = 1'b1;
        end
        ZHAO_OP_SET_PRESENTATION_CONTRACT: begin
          if (zhao_bytes_nonzero(p, base, 40, 8)) zhao_record_pad_nonzero = 1'b1;
        end
        ZHAO_OP_TERRAIN_FIELD: begin
          if (zhao_bytes_nonzero(p, base, 108, 4)) zhao_record_pad_nonzero = 1'b1;
        end
        ZHAO_OP_SURFACE_STAMP: begin
          if (zhao_bytes_nonzero(p, base, 52, 12)) zhao_record_pad_nonzero = 1'b1;
        end
        ZHAO_OP_DRAW_POPULATION: begin
          if (zhao_bytes_nonzero(p, base, 24, 8)) zhao_record_pad_nonzero = 1'b1;
        end
        ZHAO_OP_DRAW_PROCEDURAL: begin
          if (zhao_bytes_nonzero(p, base, 52, 12)) zhao_record_pad_nonzero = 1'b1;
        end
        default: zhao_record_pad_nonzero = 1'b0;
      endcase
    end
  endfunction

  // sealed frame header (capture_format.md 3), REVERSE field order
  typedef struct packed {
    logic [31:0] header_crc;
    logic [31:0] command_bytes;
    logic [31:0] command_count;
    logic [31:0] deadline_cycles;
    logic [31:0] resource_epoch;
    logic [31:0] sequence_f;  // .zidl 'sequence' (SV keyword)
    logic [31:0] frame_id;
    logic [15:0] flags;
    logic [15:0] abi_version;
    logic [31:0] magic;
  } zhao_frame_hdr_t;

  function automatic logic [287:0] zhao_pack_frame_hdr(input zhao_frame_hdr_t c);
    logic [287:0] v;
    begin
      v[ZHAO_OFF_MAGIC*8         +: 32] = c.magic;
      v[ZHAO_OFF_ABI_VERSION*8   +: 16] = c.abi_version;
      v[ZHAO_OFF_FLAGS*8         +: 16] = c.flags;
      v[ZHAO_OFF_FRAME_ID*8      +: 32] = c.frame_id;
      v[ZHAO_OFF_SEQUENCE*8      +: 32] = c.sequence_f;
      v[ZHAO_OFF_RESOURCE_EPOCH*8+: 32] = c.resource_epoch;
      v[ZHAO_OFF_DEADLINE*8      +: 32] = c.deadline_cycles;
      v[ZHAO_OFF_COMMAND_COUNT*8 +: 32] = c.command_count;
      v[ZHAO_OFF_COMMAND_BYTES*8 +: 32] = c.command_bytes;
      v[ZHAO_OFF_HEADER_CRC*8    +: 32] = c.header_crc;
      zhao_pack_frame_hdr = v;
    end
  endfunction

  function automatic zhao_frame_hdr_t zhao_unpack_frame_hdr(input logic [7:0] p []);
    zhao_frame_hdr_t c;
    begin
      c.magic          = zhao_get32(p, ZHAO_OFF_MAGIC);
      c.abi_version    = zhao_get16(p, ZHAO_OFF_ABI_VERSION);
      c.flags          = zhao_get16(p, ZHAO_OFF_FLAGS);
      c.frame_id       = zhao_get32(p, ZHAO_OFF_FRAME_ID);
      c.sequence_f     = zhao_get32(p, ZHAO_OFF_SEQUENCE);
      c.resource_epoch = zhao_get32(p, ZHAO_OFF_RESOURCE_EPOCH);
      c.deadline_cycles= zhao_get32(p, ZHAO_OFF_DEADLINE);
      c.command_count  = zhao_get32(p, ZHAO_OFF_COMMAND_COUNT);
      c.command_bytes  = zhao_get32(p, ZHAO_OFF_COMMAND_BYTES);
      c.header_crc     = zhao_get32(p, ZHAO_OFF_HEADER_CRC);
      zhao_unpack_frame_hdr = c;
    end
  endfunction

  // $bits sanity for every generated struct (probe asserts this at reset).
  function automatic logic zhao_layout_ok();
    begin
      zhao_layout_ok = 1'b1;
      if ($bits(zhao_rec_nop_t) != 8*16) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_begin_frame_t) != 8*32) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_end_frame_t) != 8*32) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_set_view_t) != 8*96) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_set_presentation_contract_t) != 8*48) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_terrain_field_t) != 8*112) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_surface_stamp_t) != 8*64) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_draw_form_t) != 8*32) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_draw_population_t) != 8*32) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_draw_procedural_t) != 8*64) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_emit_audio_event_t) != 8*32) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rec_debug_bootstrap_t) != 8*64) zhao_layout_ok = 1'b0;
      if ($bits(zhao_rectfx_t) != 8*16) zhao_layout_ok = 1'b0;
      if ($bits(zhao_transform2fx_t) != 8*24) zhao_layout_ok = 1'b0;
      if ($bits(zhao_mat4fx_t) != 8*64) zhao_layout_ok = 1'b0;
    end
  endfunction

  // Fail-safe validation order — byte-for-byte the same contract as the C++
  // (zref_frame) and TS (frame.ts) validators. On any error, no partial state.
  function automatic zhao_abi_error_e zhao_frame_validate(
      input logic [7:0] pkt [],
      input int unsigned len,
      input int unsigned slot_bytes,
      output int unsigned commands_consumed
  );
    logic [31:0] magic, command_bytes, command_count, hcrc, pcrc, calc;
    logic [15:0] abi_version;
    int unsigned off, rec_bytes, seen, want;
    logic [15:0] opcode;
    logic any_debug;
    begin
      commands_consumed = 0;
      any_debug = 1'b0;
      seen = 0;

      // 1. magic (whenever 4 bytes exist), then header completeness, abi, flags
      if (len < 4) begin
        zhao_frame_validate = ZH_ABI_BAD_LENGTH;
        return zhao_frame_validate;
      end
      magic = zhao_get32(pkt, ZHAO_OFF_MAGIC);
      if (magic != ZHAO_FRAME_MAGIC) begin
        zhao_frame_validate = ZH_ABI_BAD_MAGIC;
        return zhao_frame_validate;
      end
      if (len < ZHAO_FRAME_HEADER_BYTES) begin
        zhao_frame_validate = ZH_ABI_BAD_LENGTH;
        return zhao_frame_validate;
      end
      abi_version = zhao_get16(pkt, ZHAO_OFF_ABI_VERSION);
      if (abi_version != ZHAO_ABI_VERSION[15:0]) begin
        zhao_frame_validate = ZH_ABI_BAD_ABI_VERSION;
        return zhao_frame_validate;
      end
      if ((zhao_get16(pkt, ZHAO_OFF_FLAGS) & ~ZHAO_FRAME_FLAG_CONTAINS_DEBUG) != 16'h0000) begin
        zhao_frame_validate = ZH_ABI_RESERVED_FLAG;
        return zhao_frame_validate;
      end

      // 2. bounds
      command_bytes = zhao_get32(pkt, ZHAO_OFF_COMMAND_BYTES);
      command_count = zhao_get32(pkt, ZHAO_OFF_COMMAND_COUNT);
      if ((command_bytes % ZHAO_COMMAND_ALIGNMENT) != 0) begin
        zhao_frame_validate = ZH_ABI_BAD_LENGTH;
        return zhao_frame_validate;
      end
      if ((ZHAO_FRAME_OVERHEAD + command_bytes) > slot_bytes) begin
        zhao_frame_validate = ZH_ABI_BAD_LENGTH;
        return zhao_frame_validate;
      end
      if ((command_count * 16) > command_bytes) begin
        zhao_frame_validate = ZH_ABI_BAD_LENGTH;
        return zhao_frame_validate;
      end
      if (len != (ZHAO_FRAME_OVERHEAD + command_bytes)) begin
        zhao_frame_validate = ZH_ABI_BAD_LENGTH;
        return zhao_frame_validate;
      end

      // 3. header CRC over bytes [0,32)
      calc = zhao_crc32c_bytes(pkt, 0, 32);
      hcrc = zhao_get32(pkt, ZHAO_OFF_HEADER_CRC);
      if (calc != hcrc) begin
        zhao_frame_validate = ZH_ABI_BAD_HEADER_CRC;
        return zhao_frame_validate;
      end

      // 4. payload CRC over the command stream
      calc = zhao_crc32c_bytes(pkt, ZHAO_FRAME_HEADER_BYTES, command_bytes);
      pcrc = zhao_get32(pkt, ZHAO_FRAME_HEADER_BYTES + command_bytes);
      if (calc != pcrc) begin
        zhao_frame_validate = ZH_ABI_BAD_PAYLOAD_CRC;
        return zhao_frame_validate;
      end

      // 5./6./9./10. record walk
      off = 0;
      while (off < command_bytes) begin
        if ((off + 16) > command_bytes) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_TRUNCATED;
          return zhao_frame_validate;
        end
        opcode    = zhao_get16(pkt, ZHAO_FRAME_HEADER_BYTES + off);
        rec_bytes = {16'b0, zhao_get16(pkt, ZHAO_FRAME_HEADER_BYTES + off + 2)};
        if ((rec_bytes % ZHAO_COMMAND_ALIGNMENT) != 0 || rec_bytes < 16) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_BAD_LENGTH;
          return zhao_frame_validate;
        end
        if ((off + rec_bytes) > command_bytes) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_BAD_LENGTH;
          return zhao_frame_validate;
        end
        want = zhao_opcode_record_bytes(16'(opcode));
        if (want == 0) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_UNKNOWN_OPCODE;
          return zhao_frame_validate;
        end
        if (rec_bytes != want) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_BAD_LENGTH;
          return zhao_frame_validate;
        end
        if (zhao_get32(pkt, ZHAO_FRAME_HEADER_BYTES + off + 8) != 32'h0) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_RESERVED_FLAG;
          return zhao_frame_validate;
        end
        if (zhao_get32(pkt, ZHAO_FRAME_HEADER_BYTES + off + 12) != 32'h0) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_RESERVED_FIELD;
          return zhao_frame_validate;
        end
        if (zhao_record_pad_nonzero(16'(opcode), pkt, ZHAO_FRAME_HEADER_BYTES + off)) begin
          commands_consumed = seen;
          zhao_frame_validate = ZH_ABI_RESERVED_FIELD;
          return zhao_frame_validate;
        end
        if ((16'(opcode) >= ZHAO_DEBUG_OPCODE_LO) && (16'(opcode) <= ZHAO_DEBUG_OPCODE_HI))
          any_debug = 1'b1;
        off = off + rec_bytes;
        seen = seen + 1;
      end

      commands_consumed = seen;
      if (seen != command_count) begin
        zhao_frame_validate = ZH_ABI_COUNT_MISMATCH;
        return zhao_frame_validate;
      end
      if (any_debug && ((zhao_get16(pkt, ZHAO_OFF_FLAGS) & ZHAO_FRAME_FLAG_CONTAINS_DEBUG) == 16'h0000)) begin
        zhao_frame_validate = ZH_ABI_DEBUG_FLAG_REQUIRED;
        return zhao_frame_validate;
      end
      zhao_frame_validate = ZH_ABI_OK;
    end
  endfunction

endpackage : zhao_abi_pkg
