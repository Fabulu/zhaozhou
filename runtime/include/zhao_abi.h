// GENERATED FILE - DO NOT EDIT
// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).
// Law: spec/capture_format.md. Identity (see spec/generated/abi.md):
//   abi_identity_sha256 = db6f6b2bbf7c3a383788d21fa594e09519ace1e90943d9d8ed2227f7b199db12
//   zidl_sha256         = b9c5d806ef60c962de3205067d68d2c6a8409ac7cf836f19d3041a0f3488584d
#pragma once

#include <cstdint>
#include <cstddef>  // offsetof
#include <vector>

namespace zhao_abi {

constexpr uint16_t ZHAO_ABI_VERSION        = 3;
constexpr uint16_t ZHAO_COMMAND_ALIGNMENT = 16;
constexpr uint16_t ZHAO_OPCODE_WIDTH      = 2; // u16
constexpr uint32_t FRAME_SLOT_BYTES = 1048576u;
constexpr uint16_t QFMT_VERSION = 2u;

enum zhao_abi_error : uint32_t {
  ZH_ABI_OK = 0,
  ZH_ABI_BAD_MAGIC = 1,
  ZH_ABI_BAD_ABI_VERSION = 2,
  ZH_ABI_RESERVED_FLAG = 3,
  ZH_ABI_BAD_LENGTH = 4,
  ZH_ABI_BAD_HEADER_CRC = 5,
  ZH_ABI_BAD_PAYLOAD_CRC = 6,
  ZH_ABI_UNKNOWN_OPCODE = 7,
  ZH_ABI_RESERVED_FIELD = 8,
  ZH_ABI_BAD_VALUE = 9,
  ZH_ABI_STALE_HANDLE = 10,
  ZH_ABI_TRUNCATED = 11,
  ZH_ABI_DEBUG_FLAG_REQUIRED = 12,
  ZH_ABI_COUNT_MISMATCH = 13,
  ZH_ABI_UNIMPLEMENTED_COMMAND = 14,
};

// enum video_mode: u8 on the wire (capture_format.md 3.2 step 7)
enum video_mode : uint8_t {
  VIDEO_Z60 = 0,
  VIDEO_STORM = 1,
  VIDEO_DUO = 2,
};

// enum forge_kind: u8 on the wire (capture_format.md 3.2 step 7)
enum forge_kind : uint8_t {
  FORGE_HEIGHTFIELD_PATCH = 0,
};

// enum fog_mode: u8 on the wire (capture_format.md 3.2 step 7)
enum fog_mode : uint8_t {
  FOG_OFF = 0,
  FOG_LINEAR = 1,
};

constexpr uint16_t ZHAO_OP_NOP = 0x0000; // 16 B, implemented
constexpr uint16_t ZHAO_OP_BEGIN_FRAME = 0x0001; // 32 B, implemented
constexpr uint16_t ZHAO_OP_END_FRAME = 0x0002; // 32 B, implemented
constexpr uint16_t ZHAO_OP_SET_VIEW = 0x0010; // 96 B, implemented
constexpr uint16_t ZHAO_OP_SET_PRESENTATION_CONTRACT = 0x0020; // 48 B, implemented
constexpr uint16_t ZHAO_OP_TERRAIN_FIELD = 0x0200; // 112 B, implemented
constexpr uint16_t ZHAO_OP_SURFACE_STAMP = 0x0210; // 64 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_FORM = 0x0300; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_POPULATION = 0x0301; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_PROCEDURAL = 0x0302; // 64 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_SKY = 0x0310; // 176 B, reserved
constexpr uint16_t ZHAO_OP_SET_ENVIRONMENT = 0x0311; // 48 B, reserved
constexpr uint16_t ZHAO_OP_EMIT_AUDIO_EVENT = 0x0400; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DEBUG_BOOTSTRAP = 0xF001; // 64 B, reserved
constexpr uint16_t ZHAO_OP_DEBUG_FRAME_BLIT = 0xF002; // 48 B, implemented
constexpr uint16_t ZHAO_OP_DEBUG_RUMBLE = 0xF004; // 32 B, implemented

constexpr uint32_t ZHAO_FRAME_MAGIC        = 0x314B505Au; // 'Z','P','K','1' LE
constexpr uint32_t ZHAO_FRAME_HEADER_BYTES = 36;
constexpr uint32_t ZHAO_FRAME_OVERHEAD     = 40;  // header + payload_crc32c
constexpr uint16_t ZHAO_FRAME_FLAG_CONTAINS_DEBUG = 0x0001;
constexpr uint8_t  ZHAO_COMPL_DONE = 0x01; // completion_flags output bit
constexpr uint8_t  ZHAO_COMPL_ERR  = 0x02;
constexpr uint32_t ZHAO_OFF_MAGIC          = 0;
constexpr uint32_t ZHAO_OFF_ABI_VERSION    = 4;
constexpr uint32_t ZHAO_OFF_FLAGS          = 6;
constexpr uint32_t ZHAO_OFF_FRAME_ID       = 8;
constexpr uint32_t ZHAO_OFF_SEQUENCE       = 12;
constexpr uint32_t ZHAO_OFF_RESOURCE_EPOCH = 16;
constexpr uint32_t ZHAO_OFF_DEADLINE       = 20;
constexpr uint32_t ZHAO_OFF_COMMAND_COUNT  = 24;
constexpr uint32_t ZHAO_OFF_COMMAND_BYTES  = 28;
constexpr uint32_t ZHAO_OFF_HEADER_CRC     = 32;

constexpr uint32_t ZHAO_SOURCE_KIND_NONE = 0;
constexpr uint32_t ZHAO_SOURCE_KIND_COMMAND_SITE = 5;
inline uint32_t zhao_source_id_encode(uint32_t kind, uint32_t module, uint32_t index) {
  return (kind << 28) | (module << 16) | index;
}
inline void zhao_source_id_decode(uint32_t id, uint32_t& kind, uint32_t& module, uint32_t& index) {
  kind = id >> 28; module = (id >> 16) & 0xFFF; index = id & 0xFFFF;
}

// CRC-32C (Castagnoli): poly 0x82F63B78 reflected, init/xorout 0xFFFFFFFF.
constexpr uint32_t ZHAO_CRC32C_TABLE[256] = {
  0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4,
  0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
  0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B,
  0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
  0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B,
  0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
  0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54,
  0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
  0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A,
  0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
  0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5,
  0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
  0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45,
  0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
  0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A,
  0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
  0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48,
  0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
  0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687,
  0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
  0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927,
  0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
  0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8,
  0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
  0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096,
  0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
  0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859,
  0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
  0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9,
  0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
  0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36,
  0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
  0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C,
  0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
  0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043,
  0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
  0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3,
  0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
  0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C,
  0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
  0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652,
  0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
  0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D,
  0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
  0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D,
  0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
  0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2,
  0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
  0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530,
  0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
  0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF,
  0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
  0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F,
  0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
  0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90,
  0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
  0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE,
  0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
  0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321,
  0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
  0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81,
  0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
  0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E,
  0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351,
};
inline uint32_t zhao_crc32c(uint32_t crc, const void* buf, size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(buf);
  crc = ~crc;
  while (len--) crc = ZHAO_CRC32C_TABLE[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

struct ZhWriter {
  std::vector<uint8_t>& out;
  explicit ZhWriter(std::vector<uint8_t>& o) : out(o) {}
  void u8(uint8_t v)   { out.push_back(v); }
  void u16(uint16_t v) { u8(v & 0xFF); u8(uint8_t(v >> 8)); }
  void u32(uint32_t v) { u16(uint16_t(v & 0xFFFF)); u16(uint16_t(v >> 16)); }
  void u64(uint64_t v) { u32(uint32_t(v & 0xFFFFFFFF)); u32(uint32_t(v >> 32)); }
  void i8(int8_t v)    { u8(uint8_t(v)); }
  void i16(int16_t v)  { u16(uint16_t(v)); }
  void i32(int32_t v)  { u32(uint32_t(v)); }
  void i64(int64_t v)  { u64(uint64_t(v)); }
  void fx16(int32_t v) { i32(v); }  // Q16.16
  void fx32(int64_t v) { i64(v); }  // Q32.32
};

struct ZhReader {
  const uint8_t* p; size_t n; size_t pos = 0;
  ZhReader(const uint8_t* ptr, size_t len) : p(ptr), n(len) {}
  bool take8(uint8_t& v)  { if (pos + 1 > n) return false; v = p[pos++]; return true; }
  bool take16(uint16_t& v) { uint8_t a, b; if (!take8(a) || !take8(b)) return false; v = uint16_t(a) | (uint16_t(b) << 8); return true; }
  bool take32(uint32_t& v) { uint16_t a, b; if (!take16(a) || !take16(b)) return false; v = uint32_t(a) | (uint32_t(b) << 16); return true; }
  bool take64(uint64_t& v) { uint32_t a, b; if (!take32(a) || !take32(b)) return false; v = uint64_t(a) | (uint64_t(b) << 32); return true; }
  bool skip(size_t k) { if (pos + k > n) return false; pos += k; return true; }
};

// rectfx: 16 bytes (spec/commands.zidl)
struct ZhRectfx {
  int32_t x0;
  int32_t y0;
  int32_t x1;
  int32_t y1;
};
static_assert(offsetof(ZhRectfx, x0) == 0, "layout drift: rectfx.x0");
static_assert(offsetof(ZhRectfx, y0) == 4, "layout drift: rectfx.y0");
static_assert(offsetof(ZhRectfx, x1) == 8, "layout drift: rectfx.x1");
static_assert(offsetof(ZhRectfx, y1) == 12, "layout drift: rectfx.y1");
static_assert(sizeof(ZhRectfx) == 16, "layout drift: rectfx size");

// transform2fx: 24 bytes (spec/commands.zidl)
struct ZhTransform2fx {
  int32_t tx;
  int32_t ty;
  int32_t r00;
  int32_t r01;
  int32_t r10;
  int32_t r11;
};
static_assert(offsetof(ZhTransform2fx, tx) == 0, "layout drift: transform2fx.tx");
static_assert(offsetof(ZhTransform2fx, ty) == 4, "layout drift: transform2fx.ty");
static_assert(offsetof(ZhTransform2fx, r00) == 8, "layout drift: transform2fx.r00");
static_assert(offsetof(ZhTransform2fx, r01) == 12, "layout drift: transform2fx.r01");
static_assert(offsetof(ZhTransform2fx, r10) == 16, "layout drift: transform2fx.r10");
static_assert(offsetof(ZhTransform2fx, r11) == 20, "layout drift: transform2fx.r11");
static_assert(sizeof(ZhTransform2fx) == 24, "layout drift: transform2fx size");

// mat4fx: 64 bytes (spec/commands.zidl)
struct ZhMat4fx {
  int32_t m00;
  int32_t m01;
  int32_t m02;
  int32_t m03;
  int32_t m10;
  int32_t m11;
  int32_t m12;
  int32_t m13;
  int32_t m20;
  int32_t m21;
  int32_t m22;
  int32_t m23;
  int32_t m30;
  int32_t m31;
  int32_t m32;
  int32_t m33;
};
static_assert(offsetof(ZhMat4fx, m00) == 0, "layout drift: mat4fx.m00");
static_assert(offsetof(ZhMat4fx, m01) == 4, "layout drift: mat4fx.m01");
static_assert(offsetof(ZhMat4fx, m02) == 8, "layout drift: mat4fx.m02");
static_assert(offsetof(ZhMat4fx, m03) == 12, "layout drift: mat4fx.m03");
static_assert(offsetof(ZhMat4fx, m10) == 16, "layout drift: mat4fx.m10");
static_assert(offsetof(ZhMat4fx, m11) == 20, "layout drift: mat4fx.m11");
static_assert(offsetof(ZhMat4fx, m12) == 24, "layout drift: mat4fx.m12");
static_assert(offsetof(ZhMat4fx, m13) == 28, "layout drift: mat4fx.m13");
static_assert(offsetof(ZhMat4fx, m20) == 32, "layout drift: mat4fx.m20");
static_assert(offsetof(ZhMat4fx, m21) == 36, "layout drift: mat4fx.m21");
static_assert(offsetof(ZhMat4fx, m22) == 40, "layout drift: mat4fx.m22");
static_assert(offsetof(ZhMat4fx, m23) == 44, "layout drift: mat4fx.m23");
static_assert(offsetof(ZhMat4fx, m30) == 48, "layout drift: mat4fx.m30");
static_assert(offsetof(ZhMat4fx, m31) == 52, "layout drift: mat4fx.m31");
static_assert(offsetof(ZhMat4fx, m32) == 56, "layout drift: mat4fx.m32");
static_assert(offsetof(ZhMat4fx, m33) == 60, "layout drift: mat4fx.m33");
static_assert(sizeof(ZhMat4fx) == 64, "layout drift: mat4fx size");

// rgb565: 2 bytes (spec/commands.zidl)
struct ZhRgb565 {
  uint16_t bits;
};
static_assert(offsetof(ZhRgb565, bits) == 0, "layout drift: rgb565.bits");
static_assert(sizeof(ZhRgb565) == 2, "layout drift: rgb565 size");

// PadFrame: 20 bytes (spec/commands.zidl)
struct ZhPadFrame {
  uint8_t pad_index;
  uint8_t flags;
  uint16_t sequence;
  uint32_t buttons;
  int16_t lx;
  int16_t ly;
  int16_t rx;
  int16_t ry;
  uint32_t rsv;
};
static_assert(offsetof(ZhPadFrame, pad_index) == 0, "layout drift: PadFrame.pad_index");
static_assert(offsetof(ZhPadFrame, flags) == 1, "layout drift: PadFrame.flags");
static_assert(offsetof(ZhPadFrame, sequence) == 2, "layout drift: PadFrame.sequence");
static_assert(offsetof(ZhPadFrame, buttons) == 4, "layout drift: PadFrame.buttons");
static_assert(offsetof(ZhPadFrame, lx) == 8, "layout drift: PadFrame.lx");
static_assert(offsetof(ZhPadFrame, ly) == 10, "layout drift: PadFrame.ly");
static_assert(offsetof(ZhPadFrame, rx) == 12, "layout drift: PadFrame.rx");
static_assert(offsetof(ZhPadFrame, ry) == 14, "layout drift: PadFrame.ry");
static_assert(offsetof(ZhPadFrame, rsv) == 16, "layout drift: PadFrame.rsv");
static_assert(sizeof(ZhPadFrame) == 20, "layout drift: PadFrame size");

// 16-byte command record header (capture_format.md 3.1)
struct ZhCmdHeader {
  uint16_t opcode;
  uint16_t record_bytes;
  uint32_t source_id;
  uint32_t flags;      // no defined bits in v1 -> must be 0
  uint32_t reserved0;  // must be 0
};
static_assert(sizeof(ZhCmdHeader) == 16, "command header must be 16 bytes");
static_assert(offsetof(ZhCmdHeader, opcode) == 0, "");
static_assert(offsetof(ZhCmdHeader, record_bytes) == 2, "");
static_assert(offsetof(ZhCmdHeader, source_id) == 4, "");
static_assert(offsetof(ZhCmdHeader, flags) == 8, "");
static_assert(offsetof(ZhCmdHeader, reserved0) == 12, "");

// Nop 0x0000: 16-byte record (implemented)
struct ZhRecordNop {
  ZhCmdHeader hdr;
};
static_assert(sizeof(ZhRecordNop) == 16, "layout drift: Nop record");

// BeginFrame 0x0001: 32-byte record (implemented)
struct ZhCmdBeginFrame {
  uint32_t frame_id;
  uint32_t resource_epoch;
  uint32_t flags;
  uint32_t deadline_cycles;
};
static_assert(offsetof(ZhCmdBeginFrame, frame_id) == 0, "layout drift: BeginFrame.frame_id");
static_assert(offsetof(ZhCmdBeginFrame, resource_epoch) == 4, "layout drift: BeginFrame.resource_epoch");
static_assert(offsetof(ZhCmdBeginFrame, flags) == 8, "layout drift: BeginFrame.flags");
static_assert(offsetof(ZhCmdBeginFrame, deadline_cycles) == 12, "layout drift: BeginFrame.deadline_cycles");
static_assert(sizeof(ZhCmdBeginFrame) == 16, "layout drift: BeginFrame payload");

struct ZhRecordBeginFrame {
  ZhCmdHeader hdr;
  ZhCmdBeginFrame payload;
};
static_assert(sizeof(ZhRecordBeginFrame) == 32, "layout drift: BeginFrame record");

// EndFrame 0x0002: 32-byte record (implemented)
struct ZhCmdEndFrame {
  uint32_t completion_flags;
  uint32_t expected_crc_valid;
  uint32_t expected_framebuffer_crc;
  uint8_t pad[4];
};
static_assert(offsetof(ZhCmdEndFrame, completion_flags) == 0, "layout drift: EndFrame.completion_flags");
static_assert(offsetof(ZhCmdEndFrame, expected_crc_valid) == 4, "layout drift: EndFrame.expected_crc_valid");
static_assert(offsetof(ZhCmdEndFrame, expected_framebuffer_crc) == 8, "layout drift: EndFrame.expected_framebuffer_crc");
static_assert(offsetof(ZhCmdEndFrame, pad[0]) == 12, "layout drift: EndFrame.pad");
static_assert(sizeof(ZhCmdEndFrame) == 16, "layout drift: EndFrame payload");

struct ZhRecordEndFrame {
  ZhCmdHeader hdr;
  ZhCmdEndFrame payload;
};
static_assert(sizeof(ZhRecordEndFrame) == 32, "layout drift: EndFrame record");

// SetView 0x0010: 96-byte record (implemented)
struct ZhCmdSetView {
  uint8_t view_id;
  uint8_t viewport_id;
  uint16_t flags;
  ZhMat4fx view_projection;
  int32_t pixel_error;
  uint32_t geometry_tokens;
  uint32_t fragment_tokens;
};
static_assert(offsetof(ZhCmdSetView, view_id) == 0, "layout drift: SetView.view_id");
static_assert(offsetof(ZhCmdSetView, viewport_id) == 1, "layout drift: SetView.viewport_id");
static_assert(offsetof(ZhCmdSetView, flags) == 2, "layout drift: SetView.flags");
static_assert(offsetof(ZhCmdSetView, view_projection) == 4, "layout drift: SetView.view_projection");
static_assert(offsetof(ZhCmdSetView, pixel_error) == 68, "layout drift: SetView.pixel_error");
static_assert(offsetof(ZhCmdSetView, geometry_tokens) == 72, "layout drift: SetView.geometry_tokens");
static_assert(offsetof(ZhCmdSetView, fragment_tokens) == 76, "layout drift: SetView.fragment_tokens");
static_assert(sizeof(ZhCmdSetView) == 80, "layout drift: SetView payload");

struct ZhRecordSetView {
  ZhCmdHeader hdr;
  ZhCmdSetView payload;
};
static_assert(sizeof(ZhRecordSetView) == 96, "layout drift: SetView record");

// SetPresentationContract 0x0020: 48-byte record (implemented)
struct ZhCmdSetPresentationContract {
  video_mode mode;  // enum, 1 B
  uint8_t view_count;
  uint16_t flags;
  uint32_t geometry_tokens[2];
  uint32_t fragment_tokens[2];
  uint32_t shared_tokens;
  uint8_t pad[8];
};
static_assert(offsetof(ZhCmdSetPresentationContract, mode) == 0, "layout drift: SetPresentationContract.mode");
static_assert(offsetof(ZhCmdSetPresentationContract, view_count) == 1, "layout drift: SetPresentationContract.view_count");
static_assert(offsetof(ZhCmdSetPresentationContract, flags) == 2, "layout drift: SetPresentationContract.flags");
static_assert(offsetof(ZhCmdSetPresentationContract, geometry_tokens[0]) == 4, "layout drift: SetPresentationContract.geometry_tokens");
static_assert(offsetof(ZhCmdSetPresentationContract, fragment_tokens[0]) == 12, "layout drift: SetPresentationContract.fragment_tokens");
static_assert(offsetof(ZhCmdSetPresentationContract, shared_tokens) == 20, "layout drift: SetPresentationContract.shared_tokens");
static_assert(offsetof(ZhCmdSetPresentationContract, pad[0]) == 24, "layout drift: SetPresentationContract.pad");
static_assert(sizeof(ZhCmdSetPresentationContract) == 32, "layout drift: SetPresentationContract payload");

struct ZhRecordSetPresentationContract {
  ZhCmdHeader hdr;
  ZhCmdSetPresentationContract payload;
};
static_assert(sizeof(ZhRecordSetPresentationContract) == 48, "layout drift: SetPresentationContract record");

// TerrainField 0x0200: 112-byte record (implemented)
struct ZhCmdTerrainField {
  uint32_t program;  // handle32 {index:24, generation:8} kind=program
  ZhRectfx footprint;
  uint32_t start_tick;
  uint32_t duration_ticks;
  uint8_t parameters[64];
  uint8_t pad[4];
};
static_assert(offsetof(ZhCmdTerrainField, program) == 0, "layout drift: TerrainField.program");
static_assert(offsetof(ZhCmdTerrainField, footprint) == 4, "layout drift: TerrainField.footprint");
static_assert(offsetof(ZhCmdTerrainField, start_tick) == 20, "layout drift: TerrainField.start_tick");
static_assert(offsetof(ZhCmdTerrainField, duration_ticks) == 24, "layout drift: TerrainField.duration_ticks");
static_assert(offsetof(ZhCmdTerrainField, parameters[0]) == 28, "layout drift: TerrainField.parameters");
static_assert(offsetof(ZhCmdTerrainField, pad[0]) == 92, "layout drift: TerrainField.pad");
static_assert(sizeof(ZhCmdTerrainField) == 96, "layout drift: TerrainField payload");

struct ZhRecordTerrainField {
  ZhCmdHeader hdr;
  ZhCmdTerrainField payload;
};
static_assert(sizeof(ZhRecordTerrainField) == 112, "layout drift: TerrainField record");

// SurfaceStamp 0x0210: 64-byte record (implemented)
struct ZhCmdSurfaceStamp {
  uint32_t brush;  // handle32 {index:24, generation:8} kind=brush
  uint32_t patch;  // handle32 {index:24, generation:8} kind=patch
  uint8_t operation;
  uint8_t tag;
  uint16_t strength;
  ZhTransform2fx transform;
  int32_t radius;
  int32_t ring_width;
  uint8_t pad[4];
};
static_assert(offsetof(ZhCmdSurfaceStamp, brush) == 0, "layout drift: SurfaceStamp.brush");
static_assert(offsetof(ZhCmdSurfaceStamp, patch) == 4, "layout drift: SurfaceStamp.patch");
static_assert(offsetof(ZhCmdSurfaceStamp, operation) == 8, "layout drift: SurfaceStamp.operation");
static_assert(offsetof(ZhCmdSurfaceStamp, tag) == 9, "layout drift: SurfaceStamp.tag");
static_assert(offsetof(ZhCmdSurfaceStamp, strength) == 10, "layout drift: SurfaceStamp.strength");
static_assert(offsetof(ZhCmdSurfaceStamp, transform) == 12, "layout drift: SurfaceStamp.transform");
static_assert(offsetof(ZhCmdSurfaceStamp, radius) == 36, "layout drift: SurfaceStamp.radius");
static_assert(offsetof(ZhCmdSurfaceStamp, ring_width) == 40, "layout drift: SurfaceStamp.ring_width");
static_assert(offsetof(ZhCmdSurfaceStamp, pad[0]) == 44, "layout drift: SurfaceStamp.pad");
static_assert(sizeof(ZhCmdSurfaceStamp) == 48, "layout drift: SurfaceStamp payload");

struct ZhRecordSurfaceStamp {
  ZhCmdHeader hdr;
  ZhCmdSurfaceStamp payload;
};
static_assert(sizeof(ZhRecordSurfaceStamp) == 64, "layout drift: SurfaceStamp record");

// DrawForm 0x0300: 32-byte record (implemented)
struct ZhCmdDrawForm {
  uint32_t form;  // handle32 {index:24, generation:8} kind=form
  uint32_t material_set;  // handle32 {index:24, generation:8} kind=material_set
  uint32_t transform;  // handle32 {index:24, generation:8} kind=transform
  uint8_t viewport_mask;
  uint8_t semantic_weight;
  uint16_t flags;
};
static_assert(offsetof(ZhCmdDrawForm, form) == 0, "layout drift: DrawForm.form");
static_assert(offsetof(ZhCmdDrawForm, material_set) == 4, "layout drift: DrawForm.material_set");
static_assert(offsetof(ZhCmdDrawForm, transform) == 8, "layout drift: DrawForm.transform");
static_assert(offsetof(ZhCmdDrawForm, viewport_mask) == 12, "layout drift: DrawForm.viewport_mask");
static_assert(offsetof(ZhCmdDrawForm, semantic_weight) == 13, "layout drift: DrawForm.semantic_weight");
static_assert(offsetof(ZhCmdDrawForm, flags) == 14, "layout drift: DrawForm.flags");
static_assert(sizeof(ZhCmdDrawForm) == 16, "layout drift: DrawForm payload");

struct ZhRecordDrawForm {
  ZhCmdHeader hdr;
  ZhCmdDrawForm payload;
};
static_assert(sizeof(ZhRecordDrawForm) == 32, "layout drift: DrawForm record");

// DrawPopulation 0x0301: 32-byte record (implemented)
struct ZhCmdDrawPopulation {
  uint32_t population;  // handle32 {index:24, generation:8} kind=population
  uint8_t viewport_mask;
  uint8_t semantic_weight;
  uint16_t flags;
  uint8_t pad[8];
};
static_assert(offsetof(ZhCmdDrawPopulation, population) == 0, "layout drift: DrawPopulation.population");
static_assert(offsetof(ZhCmdDrawPopulation, viewport_mask) == 4, "layout drift: DrawPopulation.viewport_mask");
static_assert(offsetof(ZhCmdDrawPopulation, semantic_weight) == 5, "layout drift: DrawPopulation.semantic_weight");
static_assert(offsetof(ZhCmdDrawPopulation, flags) == 6, "layout drift: DrawPopulation.flags");
static_assert(offsetof(ZhCmdDrawPopulation, pad[0]) == 8, "layout drift: DrawPopulation.pad");
static_assert(sizeof(ZhCmdDrawPopulation) == 16, "layout drift: DrawPopulation payload");

struct ZhRecordDrawPopulation {
  ZhCmdHeader hdr;
  ZhCmdDrawPopulation payload;
};
static_assert(sizeof(ZhRecordDrawPopulation) == 32, "layout drift: DrawPopulation record");

// DrawProcedural 0x0302: 64-byte record (implemented)
struct ZhCmdDrawProcedural {
  uint32_t program;  // handle32 {index:24, generation:8} kind=forge_program
  uint32_t material;  // handle32 {index:24, generation:8} kind=material
  ZhTransform2fx transform;
  int32_t screen_error;
  forge_kind kind;  // enum, 1 B
  uint8_t pad[11];
};
static_assert(offsetof(ZhCmdDrawProcedural, program) == 0, "layout drift: DrawProcedural.program");
static_assert(offsetof(ZhCmdDrawProcedural, material) == 4, "layout drift: DrawProcedural.material");
static_assert(offsetof(ZhCmdDrawProcedural, transform) == 8, "layout drift: DrawProcedural.transform");
static_assert(offsetof(ZhCmdDrawProcedural, screen_error) == 32, "layout drift: DrawProcedural.screen_error");
static_assert(offsetof(ZhCmdDrawProcedural, kind) == 36, "layout drift: DrawProcedural.kind");
static_assert(offsetof(ZhCmdDrawProcedural, pad[0]) == 37, "layout drift: DrawProcedural.pad");
static_assert(sizeof(ZhCmdDrawProcedural) == 48, "layout drift: DrawProcedural payload");

struct ZhRecordDrawProcedural {
  ZhCmdHeader hdr;
  ZhCmdDrawProcedural payload;
};
static_assert(sizeof(ZhRecordDrawProcedural) == 64, "layout drift: DrawProcedural record");

// DrawSky 0x0310: 176-byte record (reserved)
struct ZhCmdDrawSky {
  uint32_t sky_set;  // handle32 {index:24, generation:8} kind=sky_set
  ZhMat4fx rot_proj[2];
  int32_t cloud_scroll_u;
  int32_t cloud_scroll_v;
  uint16_t drum_yaw;
  uint8_t viewport_mask;
  uint8_t flags;
  uint8_t reserved0;
  uint8_t reserved1;
  uint8_t pad[14];
};
static_assert(offsetof(ZhCmdDrawSky, sky_set) == 0, "layout drift: DrawSky.sky_set");
static_assert(offsetof(ZhCmdDrawSky, rot_proj[0]) == 4, "layout drift: DrawSky.rot_proj");
static_assert(offsetof(ZhCmdDrawSky, cloud_scroll_u) == 132, "layout drift: DrawSky.cloud_scroll_u");
static_assert(offsetof(ZhCmdDrawSky, cloud_scroll_v) == 136, "layout drift: DrawSky.cloud_scroll_v");
static_assert(offsetof(ZhCmdDrawSky, drum_yaw) == 140, "layout drift: DrawSky.drum_yaw");
static_assert(offsetof(ZhCmdDrawSky, viewport_mask) == 142, "layout drift: DrawSky.viewport_mask");
static_assert(offsetof(ZhCmdDrawSky, flags) == 143, "layout drift: DrawSky.flags");
static_assert(offsetof(ZhCmdDrawSky, reserved0) == 144, "layout drift: DrawSky.reserved0");
static_assert(offsetof(ZhCmdDrawSky, reserved1) == 145, "layout drift: DrawSky.reserved1");
static_assert(offsetof(ZhCmdDrawSky, pad[0]) == 146, "layout drift: DrawSky.pad");
static_assert(sizeof(ZhCmdDrawSky) == 160, "layout drift: DrawSky payload");

struct ZhRecordDrawSky {
  ZhCmdHeader hdr;
  ZhCmdDrawSky payload;
};
static_assert(sizeof(ZhRecordDrawSky) == 176, "layout drift: DrawSky record");

// SetEnvironment 0x0311: 48-byte record (reserved)
struct ZhCmdSetEnvironment {
  uint16_t sun_yaw;
  uint16_t sun_pitch;
  ZhRgb565 sun_colour;
  ZhRgb565 ambient;
  ZhRgb565 tint;
  uint8_t tint_strength;
  fog_mode fog;  // enum, 1 B
  int32_t fog_near;
  int32_t fog_far;
  uint8_t pad[12];
};
static_assert(offsetof(ZhCmdSetEnvironment, sun_yaw) == 0, "layout drift: SetEnvironment.sun_yaw");
static_assert(offsetof(ZhCmdSetEnvironment, sun_pitch) == 2, "layout drift: SetEnvironment.sun_pitch");
static_assert(offsetof(ZhCmdSetEnvironment, sun_colour) == 4, "layout drift: SetEnvironment.sun_colour");
static_assert(offsetof(ZhCmdSetEnvironment, ambient) == 6, "layout drift: SetEnvironment.ambient");
static_assert(offsetof(ZhCmdSetEnvironment, tint) == 8, "layout drift: SetEnvironment.tint");
static_assert(offsetof(ZhCmdSetEnvironment, tint_strength) == 10, "layout drift: SetEnvironment.tint_strength");
static_assert(offsetof(ZhCmdSetEnvironment, fog) == 11, "layout drift: SetEnvironment.fog");
static_assert(offsetof(ZhCmdSetEnvironment, fog_near) == 12, "layout drift: SetEnvironment.fog_near");
static_assert(offsetof(ZhCmdSetEnvironment, fog_far) == 16, "layout drift: SetEnvironment.fog_far");
static_assert(offsetof(ZhCmdSetEnvironment, pad[0]) == 20, "layout drift: SetEnvironment.pad");
static_assert(sizeof(ZhCmdSetEnvironment) == 32, "layout drift: SetEnvironment payload");

struct ZhRecordSetEnvironment {
  ZhCmdHeader hdr;
  ZhCmdSetEnvironment payload;
};
static_assert(sizeof(ZhRecordSetEnvironment) == 48, "layout drift: SetEnvironment record");

// EmitAudioEvent 0x0400: 32-byte record (implemented)
struct ZhCmdEmitAudioEvent {
  uint32_t event_id;
  int16_t pan_fx;
  uint16_t gain;
  uint32_t sample_handle;
  uint32_t timestamp;
};
static_assert(offsetof(ZhCmdEmitAudioEvent, event_id) == 0, "layout drift: EmitAudioEvent.event_id");
static_assert(offsetof(ZhCmdEmitAudioEvent, pan_fx) == 4, "layout drift: EmitAudioEvent.pan_fx");
static_assert(offsetof(ZhCmdEmitAudioEvent, gain) == 6, "layout drift: EmitAudioEvent.gain");
static_assert(offsetof(ZhCmdEmitAudioEvent, sample_handle) == 8, "layout drift: EmitAudioEvent.sample_handle");
static_assert(offsetof(ZhCmdEmitAudioEvent, timestamp) == 12, "layout drift: EmitAudioEvent.timestamp");
static_assert(sizeof(ZhCmdEmitAudioEvent) == 16, "layout drift: EmitAudioEvent payload");

struct ZhRecordEmitAudioEvent {
  ZhCmdHeader hdr;
  ZhCmdEmitAudioEvent payload;
};
static_assert(sizeof(ZhRecordEmitAudioEvent) == 32, "layout drift: EmitAudioEvent record");

// DebugBootstrap 0xF001: 64-byte record (reserved)
struct ZhCmdDebugBootstrap {
  uint8_t data[48];
};
static_assert(offsetof(ZhCmdDebugBootstrap, data[0]) == 0, "layout drift: DebugBootstrap.data");
static_assert(sizeof(ZhCmdDebugBootstrap) == 48, "layout drift: DebugBootstrap payload");

struct ZhRecordDebugBootstrap {
  ZhCmdHeader hdr;
  ZhCmdDebugBootstrap payload;
};
static_assert(sizeof(ZhRecordDebugBootstrap) == 64, "layout drift: DebugBootstrap record");

// DebugFrameBlit 0xF002: 48-byte record (implemented)
struct ZhCmdDebugFrameBlit {
  uint8_t dst_slot;
  video_mode mode;  // enum, 1 B
  uint8_t pad[2];
  uint32_t src_addr_hps;
  uint32_t byte_len;
  uint32_t expected_crc32c;
  uint8_t pad_1[16];
};
static_assert(offsetof(ZhCmdDebugFrameBlit, dst_slot) == 0, "layout drift: DebugFrameBlit.dst_slot");
static_assert(offsetof(ZhCmdDebugFrameBlit, mode) == 1, "layout drift: DebugFrameBlit.mode");
static_assert(offsetof(ZhCmdDebugFrameBlit, pad[0]) == 2, "layout drift: DebugFrameBlit.pad");
static_assert(offsetof(ZhCmdDebugFrameBlit, src_addr_hps) == 4, "layout drift: DebugFrameBlit.src_addr_hps");
static_assert(offsetof(ZhCmdDebugFrameBlit, byte_len) == 8, "layout drift: DebugFrameBlit.byte_len");
static_assert(offsetof(ZhCmdDebugFrameBlit, expected_crc32c) == 12, "layout drift: DebugFrameBlit.expected_crc32c");
static_assert(offsetof(ZhCmdDebugFrameBlit, pad_1[0]) == 16, "layout drift: DebugFrameBlit.pad_1");
static_assert(sizeof(ZhCmdDebugFrameBlit) == 32, "layout drift: DebugFrameBlit payload");

struct ZhRecordDebugFrameBlit {
  ZhCmdHeader hdr;
  ZhCmdDebugFrameBlit payload;
};
static_assert(sizeof(ZhRecordDebugFrameBlit) == 48, "layout drift: DebugFrameBlit record");

// DebugRumble 0xF004: 32-byte record (implemented)
struct ZhCmdDebugRumble {
  uint8_t pad_index;
  uint8_t enable;
  uint8_t strength;
  uint8_t pad[13];
};
static_assert(offsetof(ZhCmdDebugRumble, pad_index) == 0, "layout drift: DebugRumble.pad_index");
static_assert(offsetof(ZhCmdDebugRumble, enable) == 1, "layout drift: DebugRumble.enable");
static_assert(offsetof(ZhCmdDebugRumble, strength) == 2, "layout drift: DebugRumble.strength");
static_assert(offsetof(ZhCmdDebugRumble, pad[0]) == 3, "layout drift: DebugRumble.pad");
static_assert(sizeof(ZhCmdDebugRumble) == 16, "layout drift: DebugRumble payload");

struct ZhRecordDebugRumble {
  ZhCmdHeader hdr;
  ZhCmdDebugRumble payload;
};
static_assert(sizeof(ZhRecordDebugRumble) == 32, "layout drift: DebugRumble record");

inline ZhMat4fx zhao_sample_mat4fx() {
  ZhMat4fx v{};
  v.m00 = 88599;
  v.m01 = 154135;
  v.m02 = 219671;
  v.m03 = 285207;
  v.m10 = 350743;
  v.m11 = 416279;
  v.m12 = 481815;
  v.m13 = 547351;
  v.m20 = 88599;
  v.m21 = 154135;
  v.m22 = 219671;
  v.m23 = 285207;
  v.m30 = 350743;
  v.m31 = 416279;
  v.m32 = 481815;
  v.m33 = 547351;
  return v;
}

inline ZhRectfx zhao_sample_rectfx() {
  ZhRectfx v{};
  v.x0 = 88599;
  v.y0 = 154135;
  v.x1 = 219671;
  v.y1 = 285207;
  return v;
}

inline ZhTransform2fx zhao_sample_transform2fx() {
  ZhTransform2fx v{};
  v.tx = 88599;
  v.ty = 154135;
  v.r00 = 219671;
  v.r01 = 285207;
  v.r10 = 350743;
  v.r11 = 416279;
  return v;
}

inline ZhRgb565 zhao_sample_rgb565() {
  ZhRgb565 v{};
  v.bits = 60833u;
  return v;
}

inline ZhRecordNop zhao_sample_nop() {
  ZhRecordNop r{};
  r.hdr.opcode       = ZHAO_OP_NOP;
  r.hdr.record_bytes = 16;
  r.hdr.source_id    = 1342242816u; // kind 5, module 1, index 0
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  return r;
}

inline ZhRecordBeginFrame zhao_sample_begin_frame() {
  ZhRecordBeginFrame r{};
  r.hdr.opcode       = ZHAO_OP_BEGIN_FRAME;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242817u; // kind 5, module 1, index 1
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.frame_id = 0u;
  r.payload.resource_epoch = 0u;
  r.payload.flags = 0u;
  r.payload.deadline_cycles = 0u;
  return r;
}

inline ZhRecordEndFrame zhao_sample_end_frame() {
  ZhRecordEndFrame r{};
  r.hdr.opcode       = ZHAO_OP_END_FRAME;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242818u; // kind 5, module 1, index 2
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.completion_flags = 0u;
  r.payload.expected_crc_valid = 0u;
  r.payload.expected_framebuffer_crc = 0u;
  return r;
}

inline ZhRecordSetView zhao_sample_set_view() {
  ZhRecordSetView r{};
  r.hdr.opcode       = ZHAO_OP_SET_VIEW;
  r.hdr.record_bytes = 96;
  r.hdr.source_id    = 1342242819u; // kind 5, module 1, index 3
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.view_id = 32u;
  r.payload.viewport_id = 161u;
  r.payload.flags = 33377u;
  r.payload.view_projection = zhao_sample_mat4fx();
  r.payload.pixel_error = 285207;
  r.payload.geometry_tokens = 0u;
  r.payload.fragment_tokens = 0u;
  return r;
}

inline ZhRecordSetPresentationContract zhao_sample_set_presentation_contract() {
  ZhRecordSetPresentationContract r{};
  r.hdr.opcode       = ZHAO_OP_SET_PRESENTATION_CONTRACT;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242820u; // kind 5, module 1, index 4
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.mode = static_cast<video_mode>(0u);
  r.payload.view_count = 133u;
  r.payload.flags = 33377u;
  r.payload.geometry_tokens[0] = 0u;
  r.payload.geometry_tokens[1] = 0u;
  r.payload.fragment_tokens[0] = 0u;
  r.payload.fragment_tokens[1] = 0u;
  r.payload.shared_tokens = 0u;
  return r;
}

inline ZhRecordTerrainField zhao_sample_terrain_field() {
  ZhRecordTerrainField r{};
  r.hdr.opcode       = ZHAO_OP_TERRAIN_FIELD;
  r.hdr.record_bytes = 112;
  r.hdr.source_id    = 1342242821u; // kind 5, module 1, index 5
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.program = 704643073u;
  r.payload.footprint = zhao_sample_rectfx();
  r.payload.start_tick = 0u;
  r.payload.duration_ticks = 0u;
  r.payload.parameters[0] = 31u;
  r.payload.parameters[1] = 21u;
  r.payload.parameters[2] = 255u;
  r.payload.parameters[3] = 30u;
  r.payload.parameters[4] = 155u;
  r.payload.parameters[5] = 81u;
  r.payload.parameters[6] = 159u;
  r.payload.parameters[7] = 149u;
  r.payload.parameters[8] = 160u;
  r.payload.parameters[9] = 162u;
  r.payload.parameters[10] = 127u;
  r.payload.parameters[11] = 136u;
  r.payload.parameters[12] = 173u;
  r.payload.parameters[13] = 166u;
  r.payload.parameters[14] = 179u;
  r.payload.parameters[15] = 252u;
  r.payload.parameters[16] = 209u;
  r.payload.parameters[17] = 106u;
  r.payload.parameters[18] = 199u;
  r.payload.parameters[19] = 112u;
  r.payload.parameters[20] = 52u;
  r.payload.parameters[21] = 18u;
  r.payload.parameters[22] = 240u;
  r.payload.parameters[23] = 38u;
  r.payload.parameters[24] = 152u;
  r.payload.parameters[25] = 74u;
  r.payload.parameters[26] = 148u;
  r.payload.parameters[27] = 250u;
  r.payload.parameters[28] = 92u;
  r.payload.parameters[29] = 122u;
  r.payload.parameters[30] = 212u;
  r.payload.parameters[31] = 50u;
  r.payload.parameters[32] = 80u;
  r.payload.parameters[33] = 190u;
  r.payload.parameters[34] = 208u;
  r.payload.parameters[35] = 254u;
  r.payload.parameters[36] = 108u;
  r.payload.parameters[37] = 138u;
  r.payload.parameters[38] = 60u;
  r.payload.parameters[39] = 58u;
  r.payload.parameters[40] = 164u;
  r.payload.parameters[41] = 26u;
  r.payload.parameters[42] = 216u;
  r.payload.parameters[43] = 78u;
  r.payload.parameters[44] = 232u;
  r.payload.parameters[45] = 2u;
  r.payload.parameters[46] = 44u;
  r.payload.parameters[47] = 242u;
  r.payload.parameters[48] = 44u;
  r.payload.parameters[49] = 66u;
  r.payload.parameters[50] = 44u;
  r.payload.parameters[51] = 234u;
  r.payload.parameters[52] = 160u;
  r.payload.parameters[53] = 178u;
  r.payload.parameters[54] = 216u;
  r.payload.parameters[55] = 34u;
  r.payload.parameters[56] = 156u;
  r.payload.parameters[57] = 174u;
  r.payload.parameters[58] = 52u;
  r.payload.parameters[59] = 50u;
  r.payload.parameters[60] = 204u;
  r.payload.parameters[61] = 42u;
  r.payload.parameters[62] = 232u;
  r.payload.parameters[63] = 190u;
  return r;
}

inline ZhRecordSurfaceStamp zhao_sample_surface_stamp() {
  ZhRecordSurfaceStamp r{};
  r.hdr.opcode       = ZHAO_OP_SURFACE_STAMP;
  r.hdr.record_bytes = 64;
  r.hdr.source_id    = 1342242822u; // kind 5, module 1, index 6
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.brush = 704643073u;
  r.payload.patch = 704643074u;
  r.payload.operation = 221u;
  r.payload.tag = 3u;
  r.payload.strength = 60507u;
  r.payload.transform = zhao_sample_transform2fx();
  r.payload.radius = 285207;
  r.payload.ring_width = 350743;
  return r;
}

inline ZhRecordDrawForm zhao_sample_draw_form() {
  ZhRecordDrawForm r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_FORM;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242823u; // kind 5, module 1, index 7
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.form = 704643073u;
  r.payload.material_set = 704643074u;
  r.payload.transform = 704643075u;
  r.payload.viewport_mask = 16u;
  r.payload.semantic_weight = 94u;
  r.payload.flags = 45880u;
  return r;
}

inline ZhRecordDrawPopulation zhao_sample_draw_population() {
  ZhRecordDrawPopulation r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_POPULATION;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242824u; // kind 5, module 1, index 8
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.population = 704643073u;
  r.payload.viewport_mask = 254u;
  r.payload.semantic_weight = 96u;
  r.payload.flags = 9106u;
  return r;
}

inline ZhRecordDrawProcedural zhao_sample_draw_procedural() {
  ZhRecordDrawProcedural r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_PROCEDURAL;
  r.hdr.record_bytes = 64;
  r.hdr.source_id    = 1342242825u; // kind 5, module 1, index 9
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.program = 704643073u;
  r.payload.material = 704643074u;
  r.payload.transform = zhao_sample_transform2fx();
  r.payload.screen_error = 88599;
  r.payload.kind = static_cast<forge_kind>(0u);
  return r;
}

inline ZhRecordDrawSky zhao_sample_draw_sky() {
  ZhRecordDrawSky r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_SKY;
  r.hdr.record_bytes = 176;
  r.hdr.source_id    = 1342242826u; // kind 5, module 1, index 10
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.sky_set = 704643073u;
  r.payload.rot_proj[0] = zhao_sample_mat4fx();
  r.payload.rot_proj[1] = zhao_sample_mat4fx();
  r.payload.cloud_scroll_u = 154135;
  r.payload.cloud_scroll_v = 219671;
  r.payload.drum_yaw = 44900;
  r.payload.viewport_mask = 215u;
  r.payload.flags = 163u;
  r.payload.reserved0 = 211u;
  r.payload.reserved1 = 167u;
  return r;
}

inline ZhRecordSetEnvironment zhao_sample_set_environment() {
  ZhRecordSetEnvironment r{};
  r.hdr.opcode       = ZHAO_OP_SET_ENVIRONMENT;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242827u; // kind 5, module 1, index 11
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.sun_yaw = 22925;
  r.payload.sun_pitch = 47428;
  r.payload.sun_colour = zhao_sample_rgb565();
  r.payload.ambient = zhao_sample_rgb565();
  r.payload.tint = zhao_sample_rgb565();
  r.payload.tint_strength = 140u;
  r.payload.fog = static_cast<fog_mode>(0u);
  r.payload.fog_near = 547351;
  r.payload.fog_far = 88599;
  return r;
}

inline ZhRecordEmitAudioEvent zhao_sample_emit_audio_event() {
  ZhRecordEmitAudioEvent r{};
  r.hdr.opcode       = ZHAO_OP_EMIT_AUDIO_EVENT;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242828u; // kind 5, module 1, index 12
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.event_id = 0u;
  r.payload.pan_fx = 19374;
  r.payload.gain = 31691u;
  r.payload.sample_handle = 0u;
  r.payload.timestamp = 0u;
  return r;
}

inline ZhRecordDebugBootstrap zhao_sample_debug_bootstrap() {
  ZhRecordDebugBootstrap r{};
  r.hdr.opcode       = ZHAO_OP_DEBUG_BOOTSTRAP;
  r.hdr.record_bytes = 64;
  r.hdr.source_id    = 1342242829u; // kind 5, module 1, index 13
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.data[0] = 113u;
  r.payload.data[1] = 17u;
  r.payload.data[2] = 53u;
  r.payload.data[3] = 185u;
  r.payload.data[4] = 245u;
  r.payload.data[5] = 21u;
  r.payload.data[6] = 109u;
  r.payload.data[7] = 189u;
  r.payload.data[8] = 137u;
  r.payload.data[9] = 201u;
  r.payload.data[10] = 49u;
  r.payload.data[11] = 1u;
  r.payload.data[12] = 201u;
  r.payload.data[13] = 193u;
  r.payload.data[14] = 109u;
  r.payload.data[15] = 93u;
  r.payload.data[16] = 73u;
  r.payload.data[17] = 157u;
  r.payload.data[18] = 233u;
  r.payload.data[19] = 89u;
  r.payload.data[20] = 81u;
  r.payload.data[21] = 41u;
  r.payload.data[22] = 61u;
  r.payload.data[23] = 217u;
  r.payload.data[24] = 149u;
  r.payload.data[25] = 93u;
  r.payload.data[26] = 245u;
  r.payload.data[27] = 157u;
  r.payload.data[28] = 153u;
  r.payload.data[29] = 81u;
  r.payload.data[30] = 117u;
  r.payload.data[31] = 165u;
  r.payload.data[32] = 129u;
  r.payload.data[33] = 213u;
  r.payload.data[34] = 185u;
  r.payload.data[35] = 169u;
  r.payload.data[36] = 113u;
  r.payload.data[37] = 233u;
  r.payload.data[38] = 253u;
  r.payload.data[39] = 237u;
  r.payload.data[40] = 21u;
  r.payload.data[41] = 157u;
  r.payload.data[42] = 21u;
  r.payload.data[43] = 189u;
  r.payload.data[44] = 17u;
  r.payload.data[45] = 25u;
  r.payload.data[46] = 93u;
  r.payload.data[47] = 57u;
  return r;
}

inline ZhRecordDebugFrameBlit zhao_sample_debug_frame_blit() {
  ZhRecordDebugFrameBlit r{};
  r.hdr.opcode       = ZHAO_OP_DEBUG_FRAME_BLIT;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242830u; // kind 5, module 1, index 14
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.dst_slot = 53u;
  r.payload.mode = static_cast<video_mode>(1u);
  r.payload.src_addr_hps = 0u;
  r.payload.byte_len = 0u;
  r.payload.expected_crc32c = 0u;
  return r;
}

inline ZhRecordDebugRumble zhao_sample_debug_rumble() {
  ZhRecordDebugRumble r{};
  r.hdr.opcode       = ZHAO_OP_DEBUG_RUMBLE;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242831u; // kind 5, module 1, index 15
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.pad_index = 113u;
  r.payload.enable = 169u;
  r.payload.strength = 117u;
  return r;
}

inline void zhao_pack_mat4fx(const ZhMat4fx& v, ZhWriter& w) {
  w.u32(v.m00);
  w.u32(v.m01);
  w.u32(v.m02);
  w.u32(v.m03);
  w.u32(v.m10);
  w.u32(v.m11);
  w.u32(v.m12);
  w.u32(v.m13);
  w.u32(v.m20);
  w.u32(v.m21);
  w.u32(v.m22);
  w.u32(v.m23);
  w.u32(v.m30);
  w.u32(v.m31);
  w.u32(v.m32);
  w.u32(v.m33);
}

inline void zhao_pack_rectfx(const ZhRectfx& v, ZhWriter& w) {
  w.u32(v.x0);
  w.u32(v.y0);
  w.u32(v.x1);
  w.u32(v.y1);
}

inline void zhao_pack_transform2fx(const ZhTransform2fx& v, ZhWriter& w) {
  w.u32(v.tx);
  w.u32(v.ty);
  w.u32(v.r00);
  w.u32(v.r01);
  w.u32(v.r10);
  w.u32(v.r11);
}

inline void zhao_pack_rgb565(const ZhRgb565& v, ZhWriter& w) {
  w.u16(v.bits);
}

inline void zhao_pack_nop(const ZhRecordNop& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
}

inline void zhao_pack_begin_frame(const ZhRecordBeginFrame& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.frame_id);
  w.u32(r.payload.resource_epoch);
  w.u32(r.payload.flags);
  w.u32(r.payload.deadline_cycles);
}

inline void zhao_pack_end_frame(const ZhRecordEndFrame& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.completion_flags);
  w.u32(r.payload.expected_crc_valid);
  w.u32(r.payload.expected_framebuffer_crc);
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_set_view(const ZhRecordSetView& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.view_id);
  w.u8(r.payload.viewport_id);
  w.u16(r.payload.flags);
  zhao_pack_mat4fx(r.payload.view_projection, w);
  w.u32(r.payload.pixel_error);
  w.u32(r.payload.geometry_tokens);
  w.u32(r.payload.fragment_tokens);
}

inline void zhao_pack_set_presentation_contract(const ZhRecordSetPresentationContract& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.mode);
  w.u8(r.payload.view_count);
  w.u16(r.payload.flags);
  for (int i = 0; i < 2; ++i) { w.u32(r.payload.geometry_tokens[i]); }
  for (int i = 0; i < 2; ++i) { w.u32(r.payload.fragment_tokens[i]); }
  w.u32(r.payload.shared_tokens);
  for (int i = 0; i < 8; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_terrain_field(const ZhRecordTerrainField& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.program);
  zhao_pack_rectfx(r.payload.footprint, w);
  w.u32(r.payload.start_tick);
  w.u32(r.payload.duration_ticks);
  for (int i = 0; i < 64; ++i) { w.u8(r.payload.parameters[i]); }
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_surface_stamp(const ZhRecordSurfaceStamp& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.brush);
  w.u32(r.payload.patch);
  w.u8(r.payload.operation);
  w.u8(r.payload.tag);
  w.u16(r.payload.strength);
  zhao_pack_transform2fx(r.payload.transform, w);
  w.u32(r.payload.radius);
  w.u32(r.payload.ring_width);
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_draw_form(const ZhRecordDrawForm& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.form);
  w.u32(r.payload.material_set);
  w.u32(r.payload.transform);
  w.u8(r.payload.viewport_mask);
  w.u8(r.payload.semantic_weight);
  w.u16(r.payload.flags);
}

inline void zhao_pack_draw_population(const ZhRecordDrawPopulation& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.population);
  w.u8(r.payload.viewport_mask);
  w.u8(r.payload.semantic_weight);
  w.u16(r.payload.flags);
  for (int i = 0; i < 8; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_draw_procedural(const ZhRecordDrawProcedural& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.program);
  w.u32(r.payload.material);
  zhao_pack_transform2fx(r.payload.transform, w);
  w.u32(r.payload.screen_error);
  w.u8(r.payload.kind);
  for (int i = 0; i < 11; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_draw_sky(const ZhRecordDrawSky& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.sky_set);
  zhao_pack_mat4fx(r.payload.rot_proj[0], w);
  zhao_pack_mat4fx(r.payload.rot_proj[1], w);
  w.u32(r.payload.cloud_scroll_u);
  w.u32(r.payload.cloud_scroll_v);
  w.u16(r.payload.drum_yaw);
  w.u8(r.payload.viewport_mask);
  w.u8(r.payload.flags);
  w.u8(r.payload.reserved0);
  w.u8(r.payload.reserved1);
  for (int i = 0; i < 14; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_set_environment(const ZhRecordSetEnvironment& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u16(r.payload.sun_yaw);
  w.u16(r.payload.sun_pitch);
  zhao_pack_rgb565(r.payload.sun_colour, w);
  zhao_pack_rgb565(r.payload.ambient, w);
  zhao_pack_rgb565(r.payload.tint, w);
  w.u8(r.payload.tint_strength);
  w.u8(r.payload.fog);
  w.u32(r.payload.fog_near);
  w.u32(r.payload.fog_far);
  for (int i = 0; i < 12; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_emit_audio_event(const ZhRecordEmitAudioEvent& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.event_id);
  w.u16(r.payload.pan_fx);
  w.u16(r.payload.gain);
  w.u32(r.payload.sample_handle);
  w.u32(r.payload.timestamp);
}

inline void zhao_pack_debug_bootstrap(const ZhRecordDebugBootstrap& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  for (int i = 0; i < 48; ++i) { w.u8(r.payload.data[i]); }
}

inline void zhao_pack_debug_frame_blit(const ZhRecordDebugFrameBlit& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.dst_slot);
  w.u8(r.payload.mode);
  for (int i = 0; i < 2; ++i) w.u8(r.payload.pad[i]);
  w.u32(r.payload.src_addr_hps);
  w.u32(r.payload.byte_len);
  w.u32(r.payload.expected_crc32c);
  for (int i = 0; i < 16; ++i) w.u8(r.payload.pad_1[i]);
}

inline void zhao_pack_debug_rumble(const ZhRecordDebugRumble& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.pad_index);
  w.u8(r.payload.enable);
  w.u8(r.payload.strength);
  for (int i = 0; i < 13; ++i) w.u8(r.payload.pad[i]);
}

inline bool zhao_unpack_mat4fx(ZhReader& r, ZhMat4fx& out) {
  out = {};
  { uint32_t t; if (!r.take32(t)) return false; out.m00 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m01 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m02 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m03 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m10 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m11 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m12 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m13 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m20 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m21 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m22 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m23 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m30 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m31 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m32 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.m33 = t; }
  return true;
}

inline bool zhao_unpack_rectfx(ZhReader& r, ZhRectfx& out) {
  out = {};
  { uint32_t t; if (!r.take32(t)) return false; out.x0 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.y0 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.x1 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.y1 = t; }
  return true;
}

inline bool zhao_unpack_transform2fx(ZhReader& r, ZhTransform2fx& out) {
  out = {};
  { uint32_t t; if (!r.take32(t)) return false; out.tx = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.ty = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.r00 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.r01 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.r10 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.r11 = t; }
  return true;
}

inline bool zhao_unpack_rgb565(ZhReader& r, ZhRgb565& out) {
  out = {};
  { uint16_t t; if (!r.take16(t)) return false; out.bits = t; }
  return true;
}

inline bool zhao_unpack_nop(ZhReader& r, ZhRecordNop& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  return true;
}

inline bool zhao_unpack_begin_frame(ZhReader& r, ZhRecordBeginFrame& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.frame_id = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.resource_epoch = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.flags = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.deadline_cycles = t; }
  return true;
}

inline bool zhao_unpack_end_frame(ZhReader& r, ZhRecordEndFrame& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.completion_flags = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.expected_crc_valid = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.expected_framebuffer_crc = t; }
  if (!r.skip(4)) return false;
  return true;
}

inline bool zhao_unpack_set_view(ZhReader& r, ZhRecordSetView& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.view_id = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.viewport_id = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.flags = t; }
  if (!zhao_unpack_mat4fx(r, out.payload.view_projection)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.pixel_error = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.geometry_tokens = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.fragment_tokens = t; }
  return true;
}

inline bool zhao_unpack_set_presentation_contract(ZhReader& r, ZhRecordSetPresentationContract& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.mode = static_cast<video_mode>(t); }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.view_count = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.flags = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.geometry_tokens[0] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.geometry_tokens[1] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.fragment_tokens[0] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.fragment_tokens[1] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.shared_tokens = t; }
  if (!r.skip(8)) return false;
  return true;
}

inline bool zhao_unpack_terrain_field(ZhReader& r, ZhRecordTerrainField& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.program = t; }
  if (!zhao_unpack_rectfx(r, out.payload.footprint)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.start_tick = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.duration_ticks = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[0] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[1] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[2] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[3] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[4] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[5] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[6] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[7] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[8] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[9] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[10] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[11] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[12] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[13] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[14] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[15] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[16] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[17] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[18] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[19] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[20] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[21] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[22] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[23] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[24] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[25] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[26] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[27] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[28] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[29] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[30] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[31] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[32] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[33] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[34] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[35] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[36] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[37] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[38] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[39] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[40] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[41] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[42] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[43] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[44] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[45] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[46] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[47] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[48] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[49] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[50] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[51] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[52] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[53] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[54] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[55] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[56] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[57] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[58] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[59] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[60] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[61] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[62] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.parameters[63] = t; }
  if (!r.skip(4)) return false;
  return true;
}

inline bool zhao_unpack_surface_stamp(ZhReader& r, ZhRecordSurfaceStamp& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.brush = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.patch = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.operation = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.tag = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.strength = t; }
  if (!zhao_unpack_transform2fx(r, out.payload.transform)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.radius = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.ring_width = t; }
  if (!r.skip(4)) return false;
  return true;
}

inline bool zhao_unpack_draw_form(ZhReader& r, ZhRecordDrawForm& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.form = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.material_set = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.transform = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.viewport_mask = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.semantic_weight = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.flags = t; }
  return true;
}

inline bool zhao_unpack_draw_population(ZhReader& r, ZhRecordDrawPopulation& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.population = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.viewport_mask = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.semantic_weight = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.flags = t; }
  if (!r.skip(8)) return false;
  return true;
}

inline bool zhao_unpack_draw_procedural(ZhReader& r, ZhRecordDrawProcedural& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.program = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.material = t; }
  if (!zhao_unpack_transform2fx(r, out.payload.transform)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.screen_error = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.kind = static_cast<forge_kind>(t); }
  if (!r.skip(11)) return false;
  return true;
}

inline bool zhao_unpack_draw_sky(ZhReader& r, ZhRecordDrawSky& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.sky_set = t; }
  if (!zhao_unpack_mat4fx(r, out.payload.rot_proj[0])) return false;
  if (!zhao_unpack_mat4fx(r, out.payload.rot_proj[1])) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.cloud_scroll_u = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.cloud_scroll_v = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.drum_yaw = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.viewport_mask = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.reserved0 = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.reserved1 = t; }
  if (!r.skip(14)) return false;
  return true;
}

inline bool zhao_unpack_set_environment(ZhReader& r, ZhRecordSetEnvironment& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint16_t t; if (!r.take16(t)) return false; out.payload.sun_yaw = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.sun_pitch = t; }
  if (!zhao_unpack_rgb565(r, out.payload.sun_colour)) return false;
  if (!zhao_unpack_rgb565(r, out.payload.ambient)) return false;
  if (!zhao_unpack_rgb565(r, out.payload.tint)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.tint_strength = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.fog = static_cast<fog_mode>(t); }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.fog_near = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.fog_far = t; }
  if (!r.skip(12)) return false;
  return true;
}

inline bool zhao_unpack_emit_audio_event(ZhReader& r, ZhRecordEmitAudioEvent& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.event_id = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.pan_fx = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.gain = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.sample_handle = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.timestamp = t; }
  return true;
}

inline bool zhao_unpack_debug_bootstrap(ZhReader& r, ZhRecordDebugBootstrap& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[0] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[1] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[2] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[3] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[4] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[5] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[6] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[7] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[8] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[9] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[10] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[11] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[12] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[13] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[14] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[15] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[16] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[17] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[18] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[19] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[20] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[21] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[22] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[23] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[24] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[25] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[26] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[27] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[28] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[29] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[30] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[31] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[32] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[33] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[34] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[35] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[36] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[37] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[38] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[39] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[40] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[41] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[42] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[43] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[44] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[45] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[46] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.data[47] = t; }
  return true;
}

inline bool zhao_unpack_debug_frame_blit(ZhReader& r, ZhRecordDebugFrameBlit& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.dst_slot = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.mode = static_cast<video_mode>(t); }
  if (!r.skip(2)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.src_addr_hps = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.byte_len = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.expected_crc32c = t; }
  if (!r.skip(16)) return false;
  return true;
}

inline bool zhao_unpack_debug_rumble(ZhReader& r, ZhRecordDebugRumble& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.pad_index = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.enable = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.strength = t; }
  if (!r.skip(13)) return false;
  return true;
}

struct ZhCommandInfo {
  const char* name;
  uint16_t opcode;
  uint16_t record_bytes;
  bool implemented;
  const uint16_t* pad_offsets;  // payload-relative must-be-zero bytes
  uint16_t pad_count;
};
constexpr uint16_t ZHAO_PADS_END_FRAME[] = {12, 13, 14, 15};
constexpr uint16_t ZHAO_PADS_SET_PRESENTATION_CONTRACT[] = {24, 25, 26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_TERRAIN_FIELD[] = {92, 93, 94, 95};
constexpr uint16_t ZHAO_PADS_SURFACE_STAMP[] = {44, 45, 46, 47};
constexpr uint16_t ZHAO_PADS_DRAW_POPULATION[] = {8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint16_t ZHAO_PADS_DRAW_PROCEDURAL[] = {37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
constexpr uint16_t ZHAO_PADS_DRAW_SKY[] = {146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159};
constexpr uint16_t ZHAO_PADS_SET_ENVIRONMENT[] = {20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_DEBUG_FRAME_BLIT[] = {2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_DEBUG_RUMBLE[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr ZhCommandInfo ZHAO_COMMAND_TABLE[] = {
  {"Nop", 0x0000, 16, true, nullptr, 0},
  {"BeginFrame", 0x0001, 32, true, nullptr, 0},
  {"EndFrame", 0x0002, 32, true, ZHAO_PADS_END_FRAME, 4},
  {"SetView", 0x0010, 96, true, nullptr, 0},
  {"SetPresentationContract", 0x0020, 48, true, ZHAO_PADS_SET_PRESENTATION_CONTRACT, 8},
  {"TerrainField", 0x0200, 112, true, ZHAO_PADS_TERRAIN_FIELD, 4},
  {"SurfaceStamp", 0x0210, 64, true, ZHAO_PADS_SURFACE_STAMP, 4},
  {"DrawForm", 0x0300, 32, true, nullptr, 0},
  {"DrawPopulation", 0x0301, 32, true, ZHAO_PADS_DRAW_POPULATION, 8},
  {"DrawProcedural", 0x0302, 64, true, ZHAO_PADS_DRAW_PROCEDURAL, 11},
  {"DrawSky", 0x0310, 176, false, ZHAO_PADS_DRAW_SKY, 14},
  {"SetEnvironment", 0x0311, 48, false, ZHAO_PADS_SET_ENVIRONMENT, 12},
  {"EmitAudioEvent", 0x0400, 32, true, nullptr, 0},
  {"DebugBootstrap", 0xF001, 64, false, nullptr, 0},
  {"DebugFrameBlit", 0xF002, 48, true, ZHAO_PADS_DEBUG_FRAME_BLIT, 18},
  {"DebugRumble", 0xF004, 32, true, ZHAO_PADS_DEBUG_RUMBLE, 13},
};
constexpr size_t ZHAO_COMMAND_COUNT = 16;
constexpr uint16_t ZHAO_MAX_RECORD_BYTES = 176;
inline const ZhCommandInfo* zhao_command_info(uint16_t opcode) {
  for (const auto& e : ZHAO_COMMAND_TABLE) if (e.opcode == opcode) return &e;
  return nullptr;
}

inline bool zhao_enum_value_ok(uint16_t opcode, const uint8_t* p) {
  switch (opcode) {
    case ZHAO_OP_SET_PRESENTATION_CONTRACT: {
      const uint32_t v0 = uint32_t(p[0]);  // mode: video_mode
      if (!(v0 == 0u || v0 == 1u || v0 == 2u)) return false;
      return true;
    }
    case ZHAO_OP_DRAW_PROCEDURAL: {
      const uint32_t v0 = uint32_t(p[36]);  // kind: forge_kind
      if (!(v0 == 0u)) return false;
      return true;
    }
    case ZHAO_OP_SET_ENVIRONMENT: {
      const uint32_t v0 = uint32_t(p[11]);  // fog: fog_mode
      if (!(v0 == 0u || v0 == 1u)) return false;
      return true;
    }
    case ZHAO_OP_DEBUG_FRAME_BLIT: {
      const uint32_t v0 = uint32_t(p[1]);  // mode: video_mode
      if (!(v0 == 0u || v0 == 1u || v0 == 2u)) return false;
      return true;
    }
    default: return true;
  }
}

// .zcap ABI_INFO identity (capture_format.md 4.2)
inline constexpr const char* ZHAO_GENERATOR_NAME = "zhaozhou-abi-gen";
inline constexpr uint8_t ZHAO_GENERATOR_SHA256[32] = {0xDB, 0x6F, 0x6B, 0x2B, 0xBF, 0x7C, 0x3A, 0x38, 0x37, 0x88, 0xD2, 0x1F, 0xA5, 0x94, 0xE0, 0x95, 0x19, 0xAC, 0xE1, 0xE9, 0x09, 0x43, 0xD9, 0xD8, 0xED, 0x22, 0x27, 0xF7, 0xB1, 0x99, 0xDB, 0x12};
inline constexpr uint8_t ZHAO_ZIDL_SHA256[32] = {0xB9, 0xC5, 0xD8, 0x06, 0xEF, 0x60, 0xC9, 0x62, 0xDE, 0x32, 0x05, 0x06, 0x7D, 0x68, 0xD2, 0xC6, 0xA8, 0x40, 0x9A, 0xC7, 0xCF, 0x83, 0x6F, 0x19, 0xD3, 0x04, 0x1A, 0x0F, 0x34, 0x88, 0x58, 0x4D};
inline constexpr uint32_t ZHAO_ZCAP_SCHEMA_VERSION = 1;

}  // namespace zhao_abi
