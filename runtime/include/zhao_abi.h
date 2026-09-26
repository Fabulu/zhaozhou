// GENERATED FILE - DO NOT EDIT
// Source: spec/commands.zidl via tools/abi-gen (`npm run abi:gen`).
// Law: spec/capture_format.md. Identity (see spec/generated/abi.md):
//   abi_identity_sha256 = 06b5df38e6c838560bc66f7fe8e3359965c1fa87b91b90b91a318cd50373edae
//   zidl_sha256         = 8dd155e8e7eabd5926a636b14ca241bbe6b67c32afca3b6822cdb7026328cbda
#pragma once

#include <cstdint>
#include <cstddef>  // offsetof
#include <vector>

namespace zhao_abi {

constexpr uint16_t ZHAO_ABI_VERSION        = 4;
constexpr uint16_t ZHAO_COMMAND_ALIGNMENT = 16;
constexpr uint16_t ZHAO_OPCODE_WIDTH      = 2; // u16
constexpr uint32_t FRAME_SLOT_BYTES = 1048576u;
constexpr uint16_t QFMT_VERSION = 3u;
constexpr uint8_t RESOURCE_KIND_TWOD_PAGE = 15u;

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
  FORGE_RIBBON = 1,
  FORGE_RADIAL_FAN = 2,
  FORGE_TUBE = 3,
  FORGE_RADIAL_SHELL = 4,
  FORGE_BILLBOARD_SHEET = 5,
};

// enum fog_mode: u8 on the wire (capture_format.md 3.2 step 7)
enum fog_mode : uint8_t {
  FOG_OFF = 0,
  FOG_LINEAR = 1,
};

// enum warp_attribute_mode: u8 on the wire (capture_format.md 3.2 step 7)
enum warp_attribute_mode : uint8_t {
  WARP_ATTR_INLINE4 = 0,
  WARP_ATTR_STREAM4 = 1,
};

constexpr uint16_t ZHAO_OP_NOP = 0x0000; // 16 B, implemented
constexpr uint16_t ZHAO_OP_BEGIN_FRAME = 0x0001; // 32 B, implemented
constexpr uint16_t ZHAO_OP_SEAL_FRAME_PLAN = 0x0003; // 48 B, implemented
constexpr uint16_t ZHAO_OP_END_FRAME = 0x0002; // 32 B, implemented
constexpr uint16_t ZHAO_OP_SET_VIEW = 0x0010; // 112 B, implemented
constexpr uint16_t ZHAO_OP_SET_PRESENTATION_CONTRACT = 0x0020; // 48 B, implemented
constexpr uint16_t ZHAO_OP_TERRAIN_FIELD = 0x0200; // 112 B, implemented
constexpr uint16_t ZHAO_OP_SURFACE_STAMP = 0x0210; // 64 B, implemented
constexpr uint16_t ZHAO_OP_TERRAIN_EPOCH = 0x0220; // 32 B, reserved
constexpr uint16_t ZHAO_OP_SUBMIT_TERRAIN_SET = 0x0230; // 48 B, reserved
constexpr uint16_t ZHAO_OP_DRAW_FORM = 0x0300; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_POPULATION = 0x0301; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_PROCEDURAL = 0x0302; // 64 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_SKY = 0x0310; // 176 B, reserved
constexpr uint16_t ZHAO_OP_SET_ENVIRONMENT = 0x0311; // 48 B, implemented
constexpr uint16_t ZHAO_OP_EMIT_AUDIO_EVENT = 0x0400; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DEBUG_BOOTSTRAP = 0xF001; // 64 B, reserved
constexpr uint16_t ZHAO_OP_DEBUG_FRAME_BLIT = 0xF002; // 48 B, implemented
constexpr uint16_t ZHAO_OP_DEBUG_RUMBLE = 0xF004; // 32 B, implemented
constexpr uint16_t ZHAO_OP_PUBLISH_RESOURCE = 0x0030; // 48 B, implemented
constexpr uint16_t ZHAO_OP_SET_POST = 0x0040; // 32 B, implemented
constexpr uint16_t ZHAO_OP_SET_GRADE_TABLE = 0x0041; // 96 B, implemented
constexpr uint16_t ZHAO_OP_SET_POPULATION = 0x0303; // 48 B, implemented
constexpr uint16_t ZHAO_OP_DEBUG_TRACE_ARM = 0xF003; // 32 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_POSED_FORM = 0x0305; // 48 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_WARPED_FORM = 0x0304; // 96 B, implemented
constexpr uint16_t ZHAO_OP_SET_PLANE = 0x0306; // 64 B, implemented
constexpr uint16_t ZHAO_OP_DRAW_SPRITE = 0x0307; // 64 B, implemented

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

// MaterialSample: 4 bytes (spec/commands.zidl)
struct ZhMaterialSample {
  uint16_t binding_slot;
  uint8_t binding_generation;
  uint8_t modes;
};
static_assert(offsetof(ZhMaterialSample, binding_slot) == 0, "layout drift: MaterialSample.binding_slot");
static_assert(offsetof(ZhMaterialSample, binding_generation) == 2, "layout drift: MaterialSample.binding_generation");
static_assert(offsetof(ZhMaterialSample, modes) == 3, "layout drift: MaterialSample.modes");
static_assert(sizeof(ZhMaterialSample) == 4, "layout drift: MaterialSample size");

// MaterialRecord: 32 bytes (spec/commands.zidl)
struct ZhMaterialRecord {
  uint8_t control;
  uint8_t recipe_weight;
  uint16_t flags;
  ZhMaterialSample sample0;
  ZhMaterialSample sample1;
  ZhMaterialSample sample2;
  uint32_t palette_base;
  uint32_t raster_state;
  uint32_t fragment_state;
  uint32_t fragment_decl;
};
static_assert(offsetof(ZhMaterialRecord, control) == 0, "layout drift: MaterialRecord.control");
static_assert(offsetof(ZhMaterialRecord, recipe_weight) == 1, "layout drift: MaterialRecord.recipe_weight");
static_assert(offsetof(ZhMaterialRecord, flags) == 2, "layout drift: MaterialRecord.flags");
static_assert(offsetof(ZhMaterialRecord, sample0) == 4, "layout drift: MaterialRecord.sample0");
static_assert(offsetof(ZhMaterialRecord, sample1) == 8, "layout drift: MaterialRecord.sample1");
static_assert(offsetof(ZhMaterialRecord, sample2) == 12, "layout drift: MaterialRecord.sample2");
static_assert(offsetof(ZhMaterialRecord, palette_base) == 16, "layout drift: MaterialRecord.palette_base");
static_assert(offsetof(ZhMaterialRecord, raster_state) == 20, "layout drift: MaterialRecord.raster_state");
static_assert(offsetof(ZhMaterialRecord, fragment_state) == 24, "layout drift: MaterialRecord.fragment_state");
static_assert(offsetof(ZhMaterialRecord, fragment_decl) == 28, "layout drift: MaterialRecord.fragment_decl");
static_assert(sizeof(ZhMaterialRecord) == 32, "layout drift: MaterialRecord size");

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

// SealFramePlan 0x0003: 48-byte record (implemented)
struct ZhCmdSealFramePlan {
  uint8_t view_id;
  uint8_t flags;
  uint16_t resource_gen;
  uint16_t view_gen;
  uint16_t giant_instance;
  uint32_t plan_verts;
  uint32_t plan_tris;
  uint32_t plan_chunks;
  uint32_t plan_refs;
  uint32_t giant_refs;
  uint8_t pad[4];
};
static_assert(offsetof(ZhCmdSealFramePlan, view_id) == 0, "layout drift: SealFramePlan.view_id");
static_assert(offsetof(ZhCmdSealFramePlan, flags) == 1, "layout drift: SealFramePlan.flags");
static_assert(offsetof(ZhCmdSealFramePlan, resource_gen) == 2, "layout drift: SealFramePlan.resource_gen");
static_assert(offsetof(ZhCmdSealFramePlan, view_gen) == 4, "layout drift: SealFramePlan.view_gen");
static_assert(offsetof(ZhCmdSealFramePlan, giant_instance) == 6, "layout drift: SealFramePlan.giant_instance");
static_assert(offsetof(ZhCmdSealFramePlan, plan_verts) == 8, "layout drift: SealFramePlan.plan_verts");
static_assert(offsetof(ZhCmdSealFramePlan, plan_tris) == 12, "layout drift: SealFramePlan.plan_tris");
static_assert(offsetof(ZhCmdSealFramePlan, plan_chunks) == 16, "layout drift: SealFramePlan.plan_chunks");
static_assert(offsetof(ZhCmdSealFramePlan, plan_refs) == 20, "layout drift: SealFramePlan.plan_refs");
static_assert(offsetof(ZhCmdSealFramePlan, giant_refs) == 24, "layout drift: SealFramePlan.giant_refs");
static_assert(offsetof(ZhCmdSealFramePlan, pad[0]) == 28, "layout drift: SealFramePlan.pad");
static_assert(sizeof(ZhCmdSealFramePlan) == 32, "layout drift: SealFramePlan payload");

struct ZhRecordSealFramePlan {
  ZhCmdHeader hdr;
  ZhCmdSealFramePlan payload;
};
static_assert(sizeof(ZhRecordSealFramePlan) == 48, "layout drift: SealFramePlan record");

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

// SetView 0x0010: 112-byte record (implemented)
struct ZhCmdSetView {
  uint8_t view_id;
  uint8_t viewport_id;
  uint16_t flags;
  ZhMat4fx view_projection;
  int32_t pixel_error;
  uint32_t geometry_tokens;
  uint32_t fragment_tokens;
  int32_t eye[3];
  uint8_t pad[4];
};
static_assert(offsetof(ZhCmdSetView, view_id) == 0, "layout drift: SetView.view_id");
static_assert(offsetof(ZhCmdSetView, viewport_id) == 1, "layout drift: SetView.viewport_id");
static_assert(offsetof(ZhCmdSetView, flags) == 2, "layout drift: SetView.flags");
static_assert(offsetof(ZhCmdSetView, view_projection) == 4, "layout drift: SetView.view_projection");
static_assert(offsetof(ZhCmdSetView, pixel_error) == 68, "layout drift: SetView.pixel_error");
static_assert(offsetof(ZhCmdSetView, geometry_tokens) == 72, "layout drift: SetView.geometry_tokens");
static_assert(offsetof(ZhCmdSetView, fragment_tokens) == 76, "layout drift: SetView.fragment_tokens");
static_assert(offsetof(ZhCmdSetView, eye[0]) == 80, "layout drift: SetView.eye");
static_assert(offsetof(ZhCmdSetView, pad[0]) == 92, "layout drift: SetView.pad");
static_assert(sizeof(ZhCmdSetView) == 96, "layout drift: SetView payload");

struct ZhRecordSetView {
  ZhCmdHeader hdr;
  ZhCmdSetView payload;
};
static_assert(sizeof(ZhRecordSetView) == 112, "layout drift: SetView record");

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

// TerrainEpoch 0x0220: 32-byte record (reserved)
struct ZhCmdTerrainEpoch {
  uint32_t epoch;
  uint8_t op;
  uint8_t flags;
  uint16_t reserved;
  uint32_t island_table_handle;
  uint32_t source_id;
};
static_assert(offsetof(ZhCmdTerrainEpoch, epoch) == 0, "layout drift: TerrainEpoch.epoch");
static_assert(offsetof(ZhCmdTerrainEpoch, op) == 4, "layout drift: TerrainEpoch.op");
static_assert(offsetof(ZhCmdTerrainEpoch, flags) == 5, "layout drift: TerrainEpoch.flags");
static_assert(offsetof(ZhCmdTerrainEpoch, reserved) == 6, "layout drift: TerrainEpoch.reserved");
static_assert(offsetof(ZhCmdTerrainEpoch, island_table_handle) == 8, "layout drift: TerrainEpoch.island_table_handle");
static_assert(offsetof(ZhCmdTerrainEpoch, source_id) == 12, "layout drift: TerrainEpoch.source_id");
static_assert(sizeof(ZhCmdTerrainEpoch) == 16, "layout drift: TerrainEpoch payload");

struct ZhRecordTerrainEpoch {
  ZhCmdHeader hdr;
  ZhCmdTerrainEpoch payload;
};
static_assert(sizeof(ZhRecordTerrainEpoch) == 32, "layout drift: TerrainEpoch record");

// SubmitTerrainSet 0x0230: 48-byte record (reserved)
struct ZhCmdSubmitTerrainSet {
  uint32_t resource_epoch;
  uint32_t list_offset;
  uint32_t list_bytes;
  uint32_t list_crc32c;
  uint16_t patch_count;
  uint8_t view_mask;
  uint8_t flags;
  uint32_t sequence;
  uint32_t reserved0;
  uint32_t reserved1;
};
static_assert(offsetof(ZhCmdSubmitTerrainSet, resource_epoch) == 0, "layout drift: SubmitTerrainSet.resource_epoch");
static_assert(offsetof(ZhCmdSubmitTerrainSet, list_offset) == 4, "layout drift: SubmitTerrainSet.list_offset");
static_assert(offsetof(ZhCmdSubmitTerrainSet, list_bytes) == 8, "layout drift: SubmitTerrainSet.list_bytes");
static_assert(offsetof(ZhCmdSubmitTerrainSet, list_crc32c) == 12, "layout drift: SubmitTerrainSet.list_crc32c");
static_assert(offsetof(ZhCmdSubmitTerrainSet, patch_count) == 16, "layout drift: SubmitTerrainSet.patch_count");
static_assert(offsetof(ZhCmdSubmitTerrainSet, view_mask) == 18, "layout drift: SubmitTerrainSet.view_mask");
static_assert(offsetof(ZhCmdSubmitTerrainSet, flags) == 19, "layout drift: SubmitTerrainSet.flags");
static_assert(offsetof(ZhCmdSubmitTerrainSet, sequence) == 20, "layout drift: SubmitTerrainSet.sequence");
static_assert(offsetof(ZhCmdSubmitTerrainSet, reserved0) == 24, "layout drift: SubmitTerrainSet.reserved0");
static_assert(offsetof(ZhCmdSubmitTerrainSet, reserved1) == 28, "layout drift: SubmitTerrainSet.reserved1");
static_assert(sizeof(ZhCmdSubmitTerrainSet) == 32, "layout drift: SubmitTerrainSet payload");

struct ZhRecordSubmitTerrainSet {
  ZhCmdHeader hdr;
  ZhCmdSubmitTerrainSet payload;
};
static_assert(sizeof(ZhRecordSubmitTerrainSet) == 48, "layout drift: SubmitTerrainSet record");

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
  uint32_t material_set;  // handle32 {index:24, generation:8} kind=material_set
  ZhTransform2fx transform;
  int32_t screen_error;
  forge_kind kind;  // enum, 1 B
  uint8_t frame_tick[2];
  uint8_t pad;
  uint16_t material_id;
  uint8_t pad_1[6];
};
static_assert(offsetof(ZhCmdDrawProcedural, program) == 0, "layout drift: DrawProcedural.program");
static_assert(offsetof(ZhCmdDrawProcedural, material_set) == 4, "layout drift: DrawProcedural.material_set");
static_assert(offsetof(ZhCmdDrawProcedural, transform) == 8, "layout drift: DrawProcedural.transform");
static_assert(offsetof(ZhCmdDrawProcedural, screen_error) == 32, "layout drift: DrawProcedural.screen_error");
static_assert(offsetof(ZhCmdDrawProcedural, kind) == 36, "layout drift: DrawProcedural.kind");
static_assert(offsetof(ZhCmdDrawProcedural, frame_tick[0]) == 37, "layout drift: DrawProcedural.frame_tick");
static_assert(offsetof(ZhCmdDrawProcedural, pad) == 39, "layout drift: DrawProcedural.pad");
static_assert(offsetof(ZhCmdDrawProcedural, material_id) == 40, "layout drift: DrawProcedural.material_id");
static_assert(offsetof(ZhCmdDrawProcedural, pad_1[0]) == 42, "layout drift: DrawProcedural.pad_1");
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

// SetEnvironment 0x0311: 48-byte record (implemented)
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
  uint32_t terrain_material_set;  // handle32 {index:24, generation:8} kind=material_set
  uint16_t terrain_material_id;
  uint8_t pad[6];
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
static_assert(offsetof(ZhCmdSetEnvironment, terrain_material_set) == 20, "layout drift: SetEnvironment.terrain_material_set");
static_assert(offsetof(ZhCmdSetEnvironment, terrain_material_id) == 24, "layout drift: SetEnvironment.terrain_material_id");
static_assert(offsetof(ZhCmdSetEnvironment, pad[0]) == 26, "layout drift: SetEnvironment.pad");
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

// PublishResource 0x0030: 48-byte record (implemented)
struct ZhCmdPublishResource {
  uint32_t resource;  // handle32 {index:24, generation:8} kind=resource
  uint32_t hps_addr_lo;
  uint32_t hps_addr_hi;
  uint32_t vram_dst;
  uint32_t length;
  uint32_t crc32c;
  uint16_t new_generation;
  uint16_t epoch;
  uint8_t dst_slot;
  uint8_t kind;
  uint8_t pad[2];
};
static_assert(offsetof(ZhCmdPublishResource, resource) == 0, "layout drift: PublishResource.resource");
static_assert(offsetof(ZhCmdPublishResource, hps_addr_lo) == 4, "layout drift: PublishResource.hps_addr_lo");
static_assert(offsetof(ZhCmdPublishResource, hps_addr_hi) == 8, "layout drift: PublishResource.hps_addr_hi");
static_assert(offsetof(ZhCmdPublishResource, vram_dst) == 12, "layout drift: PublishResource.vram_dst");
static_assert(offsetof(ZhCmdPublishResource, length) == 16, "layout drift: PublishResource.length");
static_assert(offsetof(ZhCmdPublishResource, crc32c) == 20, "layout drift: PublishResource.crc32c");
static_assert(offsetof(ZhCmdPublishResource, new_generation) == 24, "layout drift: PublishResource.new_generation");
static_assert(offsetof(ZhCmdPublishResource, epoch) == 26, "layout drift: PublishResource.epoch");
static_assert(offsetof(ZhCmdPublishResource, dst_slot) == 28, "layout drift: PublishResource.dst_slot");
static_assert(offsetof(ZhCmdPublishResource, kind) == 29, "layout drift: PublishResource.kind");
static_assert(offsetof(ZhCmdPublishResource, pad[0]) == 30, "layout drift: PublishResource.pad");
static_assert(sizeof(ZhCmdPublishResource) == 32, "layout drift: PublishResource payload");

struct ZhRecordPublishResource {
  ZhCmdHeader hdr;
  ZhCmdPublishResource payload;
};
static_assert(sizeof(ZhRecordPublishResource) == 48, "layout drift: PublishResource record");

// SetPost 0x0040: 32-byte record (implemented)
struct ZhCmdSetPost {
  uint8_t bloom_gain;
  uint8_t flags;
  uint8_t flash_amount;
  uint8_t pad;
  int16_t bias_r;
  int16_t bias_g;
  int16_t bias_b;
  ZhRgb565 flash;
  ZhRgb565 ink;
  uint8_t pad_1[2];
};
static_assert(offsetof(ZhCmdSetPost, bloom_gain) == 0, "layout drift: SetPost.bloom_gain");
static_assert(offsetof(ZhCmdSetPost, flags) == 1, "layout drift: SetPost.flags");
static_assert(offsetof(ZhCmdSetPost, flash_amount) == 2, "layout drift: SetPost.flash_amount");
static_assert(offsetof(ZhCmdSetPost, pad) == 3, "layout drift: SetPost.pad");
static_assert(offsetof(ZhCmdSetPost, bias_r) == 4, "layout drift: SetPost.bias_r");
static_assert(offsetof(ZhCmdSetPost, bias_g) == 6, "layout drift: SetPost.bias_g");
static_assert(offsetof(ZhCmdSetPost, bias_b) == 8, "layout drift: SetPost.bias_b");
static_assert(offsetof(ZhCmdSetPost, flash) == 10, "layout drift: SetPost.flash");
static_assert(offsetof(ZhCmdSetPost, ink) == 12, "layout drift: SetPost.ink");
static_assert(offsetof(ZhCmdSetPost, pad_1[0]) == 14, "layout drift: SetPost.pad_1");
static_assert(sizeof(ZhCmdSetPost) == 16, "layout drift: SetPost payload");

struct ZhRecordSetPost {
  ZhCmdHeader hdr;
  ZhCmdSetPost payload;
};
static_assert(sizeof(ZhRecordSetPost) == 32, "layout drift: SetPost record");

// SetGradeTable 0x0041: 96-byte record (implemented)
struct ZhCmdSetGradeTable {
  uint8_t curve;
  uint8_t first;
  uint8_t count;
  uint8_t pad;
  uint8_t vectors[72];
  uint8_t pad_1[4];
};
static_assert(offsetof(ZhCmdSetGradeTable, curve) == 0, "layout drift: SetGradeTable.curve");
static_assert(offsetof(ZhCmdSetGradeTable, first) == 1, "layout drift: SetGradeTable.first");
static_assert(offsetof(ZhCmdSetGradeTable, count) == 2, "layout drift: SetGradeTable.count");
static_assert(offsetof(ZhCmdSetGradeTable, pad) == 3, "layout drift: SetGradeTable.pad");
static_assert(offsetof(ZhCmdSetGradeTable, vectors[0]) == 4, "layout drift: SetGradeTable.vectors");
static_assert(offsetof(ZhCmdSetGradeTable, pad_1[0]) == 76, "layout drift: SetGradeTable.pad_1");
static_assert(sizeof(ZhCmdSetGradeTable) == 80, "layout drift: SetGradeTable payload");

struct ZhRecordSetGradeTable {
  ZhCmdHeader hdr;
  ZhCmdSetGradeTable payload;
};
static_assert(sizeof(ZhRecordSetGradeTable) == 96, "layout drift: SetGradeTable record");

// SetPopulation 0x0303: 48-byte record (implemented)
struct ZhCmdSetPopulation {
  uint32_t population;  // handle32 {index:24, generation:8} kind=population
  int32_t origin_x;
  int32_t origin_y;
  int32_t origin_z;
  uint32_t active_count;
  int32_t plane_c;
  int16_t plane_nx;
  int16_t plane_ny;
  int16_t plane_nz;
  uint16_t flags;
};
static_assert(offsetof(ZhCmdSetPopulation, population) == 0, "layout drift: SetPopulation.population");
static_assert(offsetof(ZhCmdSetPopulation, origin_x) == 4, "layout drift: SetPopulation.origin_x");
static_assert(offsetof(ZhCmdSetPopulation, origin_y) == 8, "layout drift: SetPopulation.origin_y");
static_assert(offsetof(ZhCmdSetPopulation, origin_z) == 12, "layout drift: SetPopulation.origin_z");
static_assert(offsetof(ZhCmdSetPopulation, active_count) == 16, "layout drift: SetPopulation.active_count");
static_assert(offsetof(ZhCmdSetPopulation, plane_c) == 20, "layout drift: SetPopulation.plane_c");
static_assert(offsetof(ZhCmdSetPopulation, plane_nx) == 24, "layout drift: SetPopulation.plane_nx");
static_assert(offsetof(ZhCmdSetPopulation, plane_ny) == 26, "layout drift: SetPopulation.plane_ny");
static_assert(offsetof(ZhCmdSetPopulation, plane_nz) == 28, "layout drift: SetPopulation.plane_nz");
static_assert(offsetof(ZhCmdSetPopulation, flags) == 30, "layout drift: SetPopulation.flags");
static_assert(sizeof(ZhCmdSetPopulation) == 32, "layout drift: SetPopulation payload");

struct ZhRecordSetPopulation {
  ZhCmdHeader hdr;
  ZhCmdSetPopulation payload;
};
static_assert(sizeof(ZhRecordSetPopulation) == 48, "layout drift: SetPopulation record");

// DebugTraceArm 0xF003: 32-byte record (implemented)
struct ZhCmdDebugTraceArm {
  uint8_t stage_mask;
  uint8_t flags;
  uint8_t pad[14];
};
static_assert(offsetof(ZhCmdDebugTraceArm, stage_mask) == 0, "layout drift: DebugTraceArm.stage_mask");
static_assert(offsetof(ZhCmdDebugTraceArm, flags) == 1, "layout drift: DebugTraceArm.flags");
static_assert(offsetof(ZhCmdDebugTraceArm, pad[0]) == 2, "layout drift: DebugTraceArm.pad");
static_assert(sizeof(ZhCmdDebugTraceArm) == 16, "layout drift: DebugTraceArm payload");

struct ZhRecordDebugTraceArm {
  ZhCmdHeader hdr;
  ZhCmdDebugTraceArm payload;
};
static_assert(sizeof(ZhRecordDebugTraceArm) == 32, "layout drift: DebugTraceArm record");

// DrawPosedForm 0x0305: 48-byte record (implemented)
struct ZhCmdDrawPosedForm {
  uint32_t form;  // handle32 {index:24, generation:8} kind=form
  uint32_t material_set;  // handle32 {index:24, generation:8} kind=material_set
  uint32_t transform;  // handle32 {index:24, generation:8} kind=transform
  uint8_t viewport_mask;
  uint8_t semantic_weight;
  uint16_t flags;
  uint16_t clip_id;
  uint16_t frame_no;
  uint8_t sub;
  uint8_t pad[11];
};
static_assert(offsetof(ZhCmdDrawPosedForm, form) == 0, "layout drift: DrawPosedForm.form");
static_assert(offsetof(ZhCmdDrawPosedForm, material_set) == 4, "layout drift: DrawPosedForm.material_set");
static_assert(offsetof(ZhCmdDrawPosedForm, transform) == 8, "layout drift: DrawPosedForm.transform");
static_assert(offsetof(ZhCmdDrawPosedForm, viewport_mask) == 12, "layout drift: DrawPosedForm.viewport_mask");
static_assert(offsetof(ZhCmdDrawPosedForm, semantic_weight) == 13, "layout drift: DrawPosedForm.semantic_weight");
static_assert(offsetof(ZhCmdDrawPosedForm, flags) == 14, "layout drift: DrawPosedForm.flags");
static_assert(offsetof(ZhCmdDrawPosedForm, clip_id) == 16, "layout drift: DrawPosedForm.clip_id");
static_assert(offsetof(ZhCmdDrawPosedForm, frame_no) == 18, "layout drift: DrawPosedForm.frame_no");
static_assert(offsetof(ZhCmdDrawPosedForm, sub) == 20, "layout drift: DrawPosedForm.sub");
static_assert(offsetof(ZhCmdDrawPosedForm, pad[0]) == 21, "layout drift: DrawPosedForm.pad");
static_assert(sizeof(ZhCmdDrawPosedForm) == 32, "layout drift: DrawPosedForm payload");

struct ZhRecordDrawPosedForm {
  ZhCmdHeader hdr;
  ZhCmdDrawPosedForm payload;
};
static_assert(sizeof(ZhRecordDrawPosedForm) == 48, "layout drift: DrawPosedForm record");

// DrawWarpedForm 0x0304: 96-byte record (implemented)
struct ZhCmdDrawWarpedForm {
  uint32_t form;  // handle32 {index:24, generation:8} kind=form
  uint32_t material_set;  // handle32 {index:24, generation:8} kind=material_set
  uint32_t transform;  // handle32 {index:24, generation:8} kind=transform
  uint8_t viewport_mask;
  uint8_t semantic_weight;
  uint16_t flags;
  uint32_t warp_program;  // handle32 {index:24, generation:8} kind=program
  uint32_t time;
  int32_t params[4];
  int32_t attributes[4];
  uint32_t warp_attributes;  // handle32 {index:24, generation:8} kind=resource
  warp_attribute_mode attribute_mode;  // enum, 1 B
  uint8_t warp_flags;
  uint8_t pad[2];
  int32_t displacement_bound[3];
  uint8_t pad_1[4];
};
static_assert(offsetof(ZhCmdDrawWarpedForm, form) == 0, "layout drift: DrawWarpedForm.form");
static_assert(offsetof(ZhCmdDrawWarpedForm, material_set) == 4, "layout drift: DrawWarpedForm.material_set");
static_assert(offsetof(ZhCmdDrawWarpedForm, transform) == 8, "layout drift: DrawWarpedForm.transform");
static_assert(offsetof(ZhCmdDrawWarpedForm, viewport_mask) == 12, "layout drift: DrawWarpedForm.viewport_mask");
static_assert(offsetof(ZhCmdDrawWarpedForm, semantic_weight) == 13, "layout drift: DrawWarpedForm.semantic_weight");
static_assert(offsetof(ZhCmdDrawWarpedForm, flags) == 14, "layout drift: DrawWarpedForm.flags");
static_assert(offsetof(ZhCmdDrawWarpedForm, warp_program) == 16, "layout drift: DrawWarpedForm.warp_program");
static_assert(offsetof(ZhCmdDrawWarpedForm, time) == 20, "layout drift: DrawWarpedForm.time");
static_assert(offsetof(ZhCmdDrawWarpedForm, params[0]) == 24, "layout drift: DrawWarpedForm.params");
static_assert(offsetof(ZhCmdDrawWarpedForm, attributes[0]) == 40, "layout drift: DrawWarpedForm.attributes");
static_assert(offsetof(ZhCmdDrawWarpedForm, warp_attributes) == 56, "layout drift: DrawWarpedForm.warp_attributes");
static_assert(offsetof(ZhCmdDrawWarpedForm, attribute_mode) == 60, "layout drift: DrawWarpedForm.attribute_mode");
static_assert(offsetof(ZhCmdDrawWarpedForm, warp_flags) == 61, "layout drift: DrawWarpedForm.warp_flags");
static_assert(offsetof(ZhCmdDrawWarpedForm, pad[0]) == 62, "layout drift: DrawWarpedForm.pad");
static_assert(offsetof(ZhCmdDrawWarpedForm, displacement_bound[0]) == 64, "layout drift: DrawWarpedForm.displacement_bound");
static_assert(offsetof(ZhCmdDrawWarpedForm, pad_1[0]) == 76, "layout drift: DrawWarpedForm.pad_1");
static_assert(sizeof(ZhCmdDrawWarpedForm) == 80, "layout drift: DrawWarpedForm payload");

struct ZhRecordDrawWarpedForm {
  ZhCmdHeader hdr;
  ZhCmdDrawWarpedForm payload;
};
static_assert(sizeof(ZhRecordDrawWarpedForm) == 96, "layout drift: DrawWarpedForm record");

// SetPlane 0x0306: 64-byte record (implemented)
struct ZhCmdSetPlane {
  uint8_t slot;
  uint8_t role;
  uint8_t blend;
  uint8_t opacity;
  uint8_t format;
  uint8_t wrap;
  uint8_t view_mask;
  uint8_t palette_id;
  uint16_t width;
  uint16_t height;
  uint16_t flags;
  uint16_t base;
  uint8_t lstride;
  uint8_t lheight;
  uint8_t pad[2];
  int32_t a;
  int32_t b;
  int32_t c;
  int32_t d;
  int32_t u0;
  int32_t v0;
  int32_t line_scroll;
};
static_assert(offsetof(ZhCmdSetPlane, slot) == 0, "layout drift: SetPlane.slot");
static_assert(offsetof(ZhCmdSetPlane, role) == 1, "layout drift: SetPlane.role");
static_assert(offsetof(ZhCmdSetPlane, blend) == 2, "layout drift: SetPlane.blend");
static_assert(offsetof(ZhCmdSetPlane, opacity) == 3, "layout drift: SetPlane.opacity");
static_assert(offsetof(ZhCmdSetPlane, format) == 4, "layout drift: SetPlane.format");
static_assert(offsetof(ZhCmdSetPlane, wrap) == 5, "layout drift: SetPlane.wrap");
static_assert(offsetof(ZhCmdSetPlane, view_mask) == 6, "layout drift: SetPlane.view_mask");
static_assert(offsetof(ZhCmdSetPlane, palette_id) == 7, "layout drift: SetPlane.palette_id");
static_assert(offsetof(ZhCmdSetPlane, width) == 8, "layout drift: SetPlane.width");
static_assert(offsetof(ZhCmdSetPlane, height) == 10, "layout drift: SetPlane.height");
static_assert(offsetof(ZhCmdSetPlane, flags) == 12, "layout drift: SetPlane.flags");
static_assert(offsetof(ZhCmdSetPlane, base) == 14, "layout drift: SetPlane.base");
static_assert(offsetof(ZhCmdSetPlane, lstride) == 16, "layout drift: SetPlane.lstride");
static_assert(offsetof(ZhCmdSetPlane, lheight) == 17, "layout drift: SetPlane.lheight");
static_assert(offsetof(ZhCmdSetPlane, pad[0]) == 18, "layout drift: SetPlane.pad");
static_assert(offsetof(ZhCmdSetPlane, a) == 20, "layout drift: SetPlane.a");
static_assert(offsetof(ZhCmdSetPlane, b) == 24, "layout drift: SetPlane.b");
static_assert(offsetof(ZhCmdSetPlane, c) == 28, "layout drift: SetPlane.c");
static_assert(offsetof(ZhCmdSetPlane, d) == 32, "layout drift: SetPlane.d");
static_assert(offsetof(ZhCmdSetPlane, u0) == 36, "layout drift: SetPlane.u0");
static_assert(offsetof(ZhCmdSetPlane, v0) == 40, "layout drift: SetPlane.v0");
static_assert(offsetof(ZhCmdSetPlane, line_scroll) == 44, "layout drift: SetPlane.line_scroll");
static_assert(sizeof(ZhCmdSetPlane) == 48, "layout drift: SetPlane payload");

struct ZhRecordSetPlane {
  ZhCmdHeader hdr;
  ZhCmdSetPlane payload;
};
static_assert(sizeof(ZhRecordSetPlane) == 64, "layout drift: SetPlane record");

// DrawSprite 0x0307: 64-byte record (implemented)
struct ZhCmdDrawSprite {
  int16_t x;
  int16_t y;
  uint16_t w;
  uint16_t h;
  uint16_t base;
  uint8_t lstride;
  uint8_t lheight;
  uint8_t format;
  uint8_t palette_id;
  uint8_t blend;
  uint8_t view_mask;
  ZhRgb565 tint;
  uint8_t order;
  uint8_t flags;
  uint16_t src_id;
  uint8_t pad[2];
  int32_t u;
  int32_t v;
  int32_t a00;
  int32_t a01;
  int32_t a10;
  int32_t a11;
};
static_assert(offsetof(ZhCmdDrawSprite, x) == 0, "layout drift: DrawSprite.x");
static_assert(offsetof(ZhCmdDrawSprite, y) == 2, "layout drift: DrawSprite.y");
static_assert(offsetof(ZhCmdDrawSprite, w) == 4, "layout drift: DrawSprite.w");
static_assert(offsetof(ZhCmdDrawSprite, h) == 6, "layout drift: DrawSprite.h");
static_assert(offsetof(ZhCmdDrawSprite, base) == 8, "layout drift: DrawSprite.base");
static_assert(offsetof(ZhCmdDrawSprite, lstride) == 10, "layout drift: DrawSprite.lstride");
static_assert(offsetof(ZhCmdDrawSprite, lheight) == 11, "layout drift: DrawSprite.lheight");
static_assert(offsetof(ZhCmdDrawSprite, format) == 12, "layout drift: DrawSprite.format");
static_assert(offsetof(ZhCmdDrawSprite, palette_id) == 13, "layout drift: DrawSprite.palette_id");
static_assert(offsetof(ZhCmdDrawSprite, blend) == 14, "layout drift: DrawSprite.blend");
static_assert(offsetof(ZhCmdDrawSprite, view_mask) == 15, "layout drift: DrawSprite.view_mask");
static_assert(offsetof(ZhCmdDrawSprite, tint) == 16, "layout drift: DrawSprite.tint");
static_assert(offsetof(ZhCmdDrawSprite, order) == 18, "layout drift: DrawSprite.order");
static_assert(offsetof(ZhCmdDrawSprite, flags) == 19, "layout drift: DrawSprite.flags");
static_assert(offsetof(ZhCmdDrawSprite, src_id) == 20, "layout drift: DrawSprite.src_id");
static_assert(offsetof(ZhCmdDrawSprite, pad[0]) == 22, "layout drift: DrawSprite.pad");
static_assert(offsetof(ZhCmdDrawSprite, u) == 24, "layout drift: DrawSprite.u");
static_assert(offsetof(ZhCmdDrawSprite, v) == 28, "layout drift: DrawSprite.v");
static_assert(offsetof(ZhCmdDrawSprite, a00) == 32, "layout drift: DrawSprite.a00");
static_assert(offsetof(ZhCmdDrawSprite, a01) == 36, "layout drift: DrawSprite.a01");
static_assert(offsetof(ZhCmdDrawSprite, a10) == 40, "layout drift: DrawSprite.a10");
static_assert(offsetof(ZhCmdDrawSprite, a11) == 44, "layout drift: DrawSprite.a11");
static_assert(sizeof(ZhCmdDrawSprite) == 48, "layout drift: DrawSprite payload");

struct ZhRecordDrawSprite {
  ZhCmdHeader hdr;
  ZhCmdDrawSprite payload;
};
static_assert(sizeof(ZhRecordDrawSprite) == 64, "layout drift: DrawSprite record");

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

inline ZhRecordSealFramePlan zhao_sample_seal_frame_plan() {
  ZhRecordSealFramePlan r{};
  r.hdr.opcode       = ZHAO_OP_SEAL_FRAME_PLAN;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242818u; // kind 5, module 1, index 2
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.view_id = 32u;
  r.payload.flags = 137u;
  r.payload.resource_gen = 63077u;
  r.payload.view_gen = 2654u;
  r.payload.giant_instance = 48408u;
  r.payload.plan_verts = 0u;
  r.payload.plan_tris = 0u;
  r.payload.plan_chunks = 0u;
  r.payload.plan_refs = 0u;
  r.payload.giant_refs = 0u;
  return r;
}

inline ZhRecordEndFrame zhao_sample_end_frame() {
  ZhRecordEndFrame r{};
  r.hdr.opcode       = ZHAO_OP_END_FRAME;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242819u; // kind 5, module 1, index 3
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
  r.hdr.record_bytes = 112;
  r.hdr.source_id    = 1342242820u; // kind 5, module 1, index 4
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.view_id = 32u;
  r.payload.viewport_id = 161u;
  r.payload.flags = 33377u;
  r.payload.view_projection = zhao_sample_mat4fx();
  r.payload.pixel_error = 285207;
  r.payload.geometry_tokens = 0u;
  r.payload.fragment_tokens = 0u;
  r.payload.eye[0] = 481815;
  r.payload.eye[1] = 547351;
  r.payload.eye[2] = 88599;
  return r;
}

inline ZhRecordSetPresentationContract zhao_sample_set_presentation_contract() {
  ZhRecordSetPresentationContract r{};
  r.hdr.opcode       = ZHAO_OP_SET_PRESENTATION_CONTRACT;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242821u; // kind 5, module 1, index 5
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
  r.hdr.source_id    = 1342242822u; // kind 5, module 1, index 6
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
  r.hdr.source_id    = 1342242823u; // kind 5, module 1, index 7
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

inline ZhRecordTerrainEpoch zhao_sample_terrain_epoch() {
  ZhRecordTerrainEpoch r{};
  r.hdr.opcode       = ZHAO_OP_TERRAIN_EPOCH;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242824u; // kind 5, module 1, index 8
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.epoch = 0u;
  r.payload.op = 206u;
  r.payload.flags = 220u;
  r.payload.reserved = 33122u;
  r.payload.island_table_handle = 0u;
  r.payload.source_id = 0u;
  return r;
}

inline ZhRecordSubmitTerrainSet zhao_sample_submit_terrain_set() {
  ZhRecordSubmitTerrainSet r{};
  r.hdr.opcode       = ZHAO_OP_SUBMIT_TERRAIN_SET;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242825u; // kind 5, module 1, index 9
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.resource_epoch = 0u;
  r.payload.list_offset = 0u;
  r.payload.list_bytes = 0u;
  r.payload.list_crc32c = 0u;
  r.payload.patch_count = 60985u;
  r.payload.view_mask = 24u;
  r.payload.flags = 158u;
  r.payload.sequence = 0u;
  r.payload.reserved0 = 0u;
  r.payload.reserved1 = 0u;
  return r;
}

inline ZhRecordDrawForm zhao_sample_draw_form() {
  ZhRecordDrawForm r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_FORM;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242826u; // kind 5, module 1, index 10
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
  r.hdr.source_id    = 1342242827u; // kind 5, module 1, index 11
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
  r.hdr.source_id    = 1342242828u; // kind 5, module 1, index 12
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.program = 704643073u;
  r.payload.material_set = 704643074u;
  r.payload.transform = zhao_sample_transform2fx();
  r.payload.screen_error = 88599;
  r.payload.kind = static_cast<forge_kind>(3u);
  r.payload.frame_tick[0] = 215u;
  r.payload.frame_tick[1] = 157u;
  r.payload.material_id = 62550u;
  return r;
}

inline ZhRecordDrawSky zhao_sample_draw_sky() {
  ZhRecordDrawSky r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_SKY;
  r.hdr.record_bytes = 176;
  r.hdr.source_id    = 1342242829u; // kind 5, module 1, index 13
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
  r.hdr.source_id    = 1342242830u; // kind 5, module 1, index 14
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
  r.payload.terrain_material_set = 704643082u;
  r.payload.terrain_material_id = 10022u;
  return r;
}

inline ZhRecordEmitAudioEvent zhao_sample_emit_audio_event() {
  ZhRecordEmitAudioEvent r{};
  r.hdr.opcode       = ZHAO_OP_EMIT_AUDIO_EVENT;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242831u; // kind 5, module 1, index 15
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
  r.hdr.source_id    = 1342242832u; // kind 5, module 1, index 16
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
  r.hdr.source_id    = 1342242833u; // kind 5, module 1, index 17
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
  r.hdr.source_id    = 1342242834u; // kind 5, module 1, index 18
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.pad_index = 113u;
  r.payload.enable = 169u;
  r.payload.strength = 117u;
  return r;
}

inline ZhRecordPublishResource zhao_sample_publish_resource() {
  ZhRecordPublishResource r{};
  r.hdr.opcode       = ZHAO_OP_PUBLISH_RESOURCE;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242835u; // kind 5, module 1, index 19
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.resource = 704643073u;
  r.payload.hps_addr_lo = 0u;
  r.payload.hps_addr_hi = 0u;
  r.payload.vram_dst = 0u;
  r.payload.length = 0u;
  r.payload.crc32c = 0u;
  r.payload.new_generation = 20931u;
  r.payload.epoch = 64290u;
  r.payload.dst_slot = 185u;
  r.payload.kind = 85u;
  return r;
}

inline ZhRecordSetPost zhao_sample_set_post() {
  ZhRecordSetPost r{};
  r.hdr.opcode       = ZHAO_OP_SET_POST;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242836u; // kind 5, module 1, index 20
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.bloom_gain = 134u;
  r.payload.flags = 137u;
  r.payload.flash_amount = 145u;
  r.payload.bias_r = 30028;
  r.payload.bias_g = 10970;
  r.payload.bias_b = 22138;
  r.payload.flash = zhao_sample_rgb565();
  r.payload.ink = zhao_sample_rgb565();
  return r;
}

inline ZhRecordSetGradeTable zhao_sample_set_grade_table() {
  ZhRecordSetGradeTable r{};
  r.hdr.opcode       = ZHAO_OP_SET_GRADE_TABLE;
  r.hdr.record_bytes = 96;
  r.hdr.source_id    = 1342242837u; // kind 5, module 1, index 21
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.curve = 205u;
  r.payload.first = 53u;
  r.payload.count = 109u;
  r.payload.vectors[0] = 65u;
  r.payload.vectors[1] = 153u;
  r.payload.vectors[2] = 137u;
  r.payload.vectors[3] = 145u;
  r.payload.vectors[4] = 129u;
  r.payload.vectors[5] = 85u;
  r.payload.vectors[6] = 5u;
  r.payload.vectors[7] = 173u;
  r.payload.vectors[8] = 201u;
  r.payload.vectors[9] = 193u;
  r.payload.vectors[10] = 185u;
  r.payload.vectors[11] = 65u;
  r.payload.vectors[12] = 173u;
  r.payload.vectors[13] = 41u;
  r.payload.vectors[14] = 117u;
  r.payload.vectors[15] = 13u;
  r.payload.vectors[16] = 9u;
  r.payload.vectors[17] = 85u;
  r.payload.vectors[18] = 129u;
  r.payload.vectors[19] = 73u;
  r.payload.vectors[20] = 122u;
  r.payload.vectors[21] = 122u;
  r.payload.vectors[22] = 232u;
  r.payload.vectors[23] = 226u;
  r.payload.vectors[24] = 118u;
  r.payload.vectors[25] = 6u;
  r.payload.vectors[26] = 164u;
  r.payload.vectors[27] = 158u;
  r.payload.vectors[28] = 18u;
  r.payload.vectors[29] = 178u;
  r.payload.vectors[30] = 85u;
  r.payload.vectors[31] = 13u;
  r.payload.vectors[32] = 201u;
  r.payload.vectors[33] = 181u;
  r.payload.vectors[34] = 89u;
  r.payload.vectors[35] = 65u;
  r.payload.vectors[36] = 253u;
  r.payload.vectors[37] = 89u;
  r.payload.vectors[38] = 93u;
  r.payload.vectors[39] = 181u;
  r.payload.vectors[40] = 73u;
  r.payload.vectors[41] = 193u;
  r.payload.vectors[42] = 69u;
  r.payload.vectors[43] = 145u;
  r.payload.vectors[44] = 141u;
  r.payload.vectors[45] = 21u;
  r.payload.vectors[46] = 137u;
  r.payload.vectors[47] = 149u;
  r.payload.vectors[48] = 225u;
  r.payload.vectors[49] = 153u;
  r.payload.vectors[50] = 114u;
  r.payload.vectors[51] = 90u;
  r.payload.vectors[52] = 72u;
  r.payload.vectors[53] = 162u;
  r.payload.vectors[54] = 246u;
  r.payload.vectors[55] = 30u;
  r.payload.vectors[56] = 76u;
  r.payload.vectors[57] = 166u;
  r.payload.vectors[58] = 165u;
  r.payload.vectors[59] = 181u;
  r.payload.vectors[60] = 57u;
  r.payload.vectors[61] = 153u;
  r.payload.vectors[62] = 61u;
  r.payload.vectors[63] = 129u;
  r.payload.vectors[64] = 77u;
  r.payload.vectors[65] = 221u;
  r.payload.vectors[66] = 129u;
  r.payload.vectors[67] = 117u;
  r.payload.vectors[68] = 193u;
  r.payload.vectors[69] = 193u;
  r.payload.vectors[70] = 201u;
  r.payload.vectors[71] = 233u;
  return r;
}

inline ZhRecordSetPopulation zhao_sample_set_population() {
  ZhRecordSetPopulation r{};
  r.hdr.opcode       = ZHAO_OP_SET_POPULATION;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242838u; // kind 5, module 1, index 22
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.population = 704643073u;
  r.payload.origin_x = 0;
  r.payload.origin_y = 0;
  r.payload.origin_z = 0;
  r.payload.active_count = 0u;
  r.payload.plane_c = 0;
  r.payload.plane_nx = 4151;
  r.payload.plane_ny = 59330;
  r.payload.plane_nz = 55757;
  r.payload.flags = 644u;
  return r;
}

inline ZhRecordDebugTraceArm zhao_sample_debug_trace_arm() {
  ZhRecordDebugTraceArm r{};
  r.hdr.opcode       = ZHAO_OP_DEBUG_TRACE_ARM;
  r.hdr.record_bytes = 32;
  r.hdr.source_id    = 1342242839u; // kind 5, module 1, index 23
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.stage_mask = 165u;
  r.payload.flags = 137u;
  return r;
}

inline ZhRecordDrawPosedForm zhao_sample_draw_posed_form() {
  ZhRecordDrawPosedForm r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_POSED_FORM;
  r.hdr.record_bytes = 48;
  r.hdr.source_id    = 1342242840u; // kind 5, module 1, index 24
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.form = 704643073u;
  r.payload.material_set = 704643074u;
  r.payload.transform = 704643075u;
  r.payload.viewport_mask = 16u;
  r.payload.semantic_weight = 94u;
  r.payload.flags = 45880u;
  r.payload.clip_id = 59139u;
  r.payload.frame_no = 64773u;
  r.payload.sub = 205u;
  return r;
}

inline ZhRecordDrawWarpedForm zhao_sample_draw_warped_form() {
  ZhRecordDrawWarpedForm r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_WARPED_FORM;
  r.hdr.record_bytes = 96;
  r.hdr.source_id    = 1342242841u; // kind 5, module 1, index 25
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.form = 704643073u;
  r.payload.material_set = 704643074u;
  r.payload.transform = 704643075u;
  r.payload.viewport_mask = 16u;
  r.payload.semantic_weight = 94u;
  r.payload.flags = 45880u;
  r.payload.warp_program = 704643079u;
  r.payload.time = 0u;
  r.payload.params[0] = 88599;
  r.payload.params[1] = 154135;
  r.payload.params[2] = 219671;
  r.payload.params[3] = 285207;
  r.payload.attributes[0] = 350743;
  r.payload.attributes[1] = 416279;
  r.payload.attributes[2] = 481815;
  r.payload.attributes[3] = 547351;
  r.payload.warp_attributes = 704643089u;
  r.payload.attribute_mode = static_cast<warp_attribute_mode>(1u);
  r.payload.warp_flags = 184u;
  r.payload.displacement_bound[0] = 416279;
  r.payload.displacement_bound[1] = 481815;
  r.payload.displacement_bound[2] = 547351;
  return r;
}

inline ZhRecordSetPlane zhao_sample_set_plane() {
  ZhRecordSetPlane r{};
  r.hdr.opcode       = ZHAO_OP_SET_PLANE;
  r.hdr.record_bytes = 64;
  r.hdr.source_id    = 1342242842u; // kind 5, module 1, index 26
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.slot = 125u;
  r.payload.role = 253u;
  r.payload.blend = 33u;
  r.payload.opacity = 245u;
  r.payload.format = 141u;
  r.payload.wrap = 129u;
  r.payload.view_mask = 125u;
  r.payload.palette_id = 157u;
  r.payload.width = 9545u;
  r.payload.height = 22272u;
  r.payload.flags = 15507u;
  r.payload.base = 7226u;
  r.payload.lstride = 253u;
  r.payload.lheight = 217u;
  r.payload.a = 88599;
  r.payload.b = 154135;
  r.payload.c = 219671;
  r.payload.d = 285207;
  r.payload.u0 = 350743;
  r.payload.v0 = 416279;
  r.payload.line_scroll = 481815;
  return r;
}

inline ZhRecordDrawSprite zhao_sample_draw_sprite() {
  ZhRecordDrawSprite r{};
  r.hdr.opcode       = ZHAO_OP_DRAW_SPRITE;
  r.hdr.record_bytes = 64;
  r.hdr.source_id    = 1342242843u; // kind 5, module 1, index 27
  r.hdr.flags        = 0u;
  r.hdr.reserved0    = 0u;
  r.payload.x = 10553;
  r.payload.y = 10616;
  r.payload.w = 31735u;
  r.payload.h = 28502u;
  r.payload.base = 35129u;
  r.payload.lstride = 240u;
  r.payload.lheight = 90u;
  r.payload.format = 56u;
  r.payload.palette_id = 102u;
  r.payload.blend = 180u;
  r.payload.view_mask = 2u;
  r.payload.tint = zhao_sample_rgb565();
  r.payload.order = 13u;
  r.payload.flags = 231u;
  r.payload.src_id = 25795u;
  r.payload.u = 154135;
  r.payload.v = 219671;
  r.payload.a00 = 285207;
  r.payload.a01 = 350743;
  r.payload.a10 = 416279;
  r.payload.a11 = 481815;
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

inline void zhao_pack_seal_frame_plan(const ZhRecordSealFramePlan& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.view_id);
  w.u8(r.payload.flags);
  w.u16(r.payload.resource_gen);
  w.u16(r.payload.view_gen);
  w.u16(r.payload.giant_instance);
  w.u32(r.payload.plan_verts);
  w.u32(r.payload.plan_tris);
  w.u32(r.payload.plan_chunks);
  w.u32(r.payload.plan_refs);
  w.u32(r.payload.giant_refs);
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad[i]);
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
  for (int i = 0; i < 3; ++i) { w.u32(r.payload.eye[i]); }
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad[i]);
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

inline void zhao_pack_terrain_epoch(const ZhRecordTerrainEpoch& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.epoch);
  w.u8(r.payload.op);
  w.u8(r.payload.flags);
  w.u16(r.payload.reserved);
  w.u32(r.payload.island_table_handle);
  w.u32(r.payload.source_id);
}

inline void zhao_pack_submit_terrain_set(const ZhRecordSubmitTerrainSet& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.resource_epoch);
  w.u32(r.payload.list_offset);
  w.u32(r.payload.list_bytes);
  w.u32(r.payload.list_crc32c);
  w.u16(r.payload.patch_count);
  w.u8(r.payload.view_mask);
  w.u8(r.payload.flags);
  w.u32(r.payload.sequence);
  w.u32(r.payload.reserved0);
  w.u32(r.payload.reserved1);
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
  w.u32(r.payload.material_set);
  zhao_pack_transform2fx(r.payload.transform, w);
  w.u32(r.payload.screen_error);
  w.u8(r.payload.kind);
  for (int i = 0; i < 2; ++i) { w.u8(r.payload.frame_tick[i]); }
  for (int i = 0; i < 1; ++i) w.u8(r.payload.pad);
  w.u16(r.payload.material_id);
  for (int i = 0; i < 6; ++i) w.u8(r.payload.pad_1[i]);
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
  w.u32(r.payload.terrain_material_set);
  w.u16(r.payload.terrain_material_id);
  for (int i = 0; i < 6; ++i) w.u8(r.payload.pad[i]);
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

inline void zhao_pack_publish_resource(const ZhRecordPublishResource& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.resource);
  w.u32(r.payload.hps_addr_lo);
  w.u32(r.payload.hps_addr_hi);
  w.u32(r.payload.vram_dst);
  w.u32(r.payload.length);
  w.u32(r.payload.crc32c);
  w.u16(r.payload.new_generation);
  w.u16(r.payload.epoch);
  w.u8(r.payload.dst_slot);
  w.u8(r.payload.kind);
  for (int i = 0; i < 2; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_set_post(const ZhRecordSetPost& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.bloom_gain);
  w.u8(r.payload.flags);
  w.u8(r.payload.flash_amount);
  for (int i = 0; i < 1; ++i) w.u8(r.payload.pad);
  w.u16(r.payload.bias_r);
  w.u16(r.payload.bias_g);
  w.u16(r.payload.bias_b);
  zhao_pack_rgb565(r.payload.flash, w);
  zhao_pack_rgb565(r.payload.ink, w);
  for (int i = 0; i < 2; ++i) w.u8(r.payload.pad_1[i]);
}

inline void zhao_pack_set_grade_table(const ZhRecordSetGradeTable& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.curve);
  w.u8(r.payload.first);
  w.u8(r.payload.count);
  for (int i = 0; i < 1; ++i) w.u8(r.payload.pad);
  for (int i = 0; i < 72; ++i) { w.u8(r.payload.vectors[i]); }
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad_1[i]);
}

inline void zhao_pack_set_population(const ZhRecordSetPopulation& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.population);
  w.u32(r.payload.origin_x);
  w.u32(r.payload.origin_y);
  w.u32(r.payload.origin_z);
  w.u32(r.payload.active_count);
  w.u32(r.payload.plane_c);
  w.u16(r.payload.plane_nx);
  w.u16(r.payload.plane_ny);
  w.u16(r.payload.plane_nz);
  w.u16(r.payload.flags);
}

inline void zhao_pack_debug_trace_arm(const ZhRecordDebugTraceArm& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.stage_mask);
  w.u8(r.payload.flags);
  for (int i = 0; i < 14; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_draw_posed_form(const ZhRecordDrawPosedForm& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.form);
  w.u32(r.payload.material_set);
  w.u32(r.payload.transform);
  w.u8(r.payload.viewport_mask);
  w.u8(r.payload.semantic_weight);
  w.u16(r.payload.flags);
  w.u16(r.payload.clip_id);
  w.u16(r.payload.frame_no);
  w.u8(r.payload.sub);
  for (int i = 0; i < 11; ++i) w.u8(r.payload.pad[i]);
}

inline void zhao_pack_draw_warped_form(const ZhRecordDrawWarpedForm& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u32(r.payload.form);
  w.u32(r.payload.material_set);
  w.u32(r.payload.transform);
  w.u8(r.payload.viewport_mask);
  w.u8(r.payload.semantic_weight);
  w.u16(r.payload.flags);
  w.u32(r.payload.warp_program);
  w.u32(r.payload.time);
  for (int i = 0; i < 4; ++i) { w.u32(r.payload.params[i]); }
  for (int i = 0; i < 4; ++i) { w.u32(r.payload.attributes[i]); }
  w.u32(r.payload.warp_attributes);
  w.u8(r.payload.attribute_mode);
  w.u8(r.payload.warp_flags);
  for (int i = 0; i < 2; ++i) w.u8(r.payload.pad[i]);
  for (int i = 0; i < 3; ++i) { w.u32(r.payload.displacement_bound[i]); }
  for (int i = 0; i < 4; ++i) w.u8(r.payload.pad_1[i]);
}

inline void zhao_pack_set_plane(const ZhRecordSetPlane& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u8(r.payload.slot);
  w.u8(r.payload.role);
  w.u8(r.payload.blend);
  w.u8(r.payload.opacity);
  w.u8(r.payload.format);
  w.u8(r.payload.wrap);
  w.u8(r.payload.view_mask);
  w.u8(r.payload.palette_id);
  w.u16(r.payload.width);
  w.u16(r.payload.height);
  w.u16(r.payload.flags);
  w.u16(r.payload.base);
  w.u8(r.payload.lstride);
  w.u8(r.payload.lheight);
  for (int i = 0; i < 2; ++i) w.u8(r.payload.pad[i]);
  w.u32(r.payload.a);
  w.u32(r.payload.b);
  w.u32(r.payload.c);
  w.u32(r.payload.d);
  w.u32(r.payload.u0);
  w.u32(r.payload.v0);
  w.u32(r.payload.line_scroll);
}

inline void zhao_pack_draw_sprite(const ZhRecordDrawSprite& r, std::vector<uint8_t>& out) {
  ZhWriter w(out);
  w.u16(r.hdr.opcode); w.u16(r.hdr.record_bytes); w.u32(r.hdr.source_id);
  w.u32(r.hdr.flags); w.u32(r.hdr.reserved0);
  w.u16(r.payload.x);
  w.u16(r.payload.y);
  w.u16(r.payload.w);
  w.u16(r.payload.h);
  w.u16(r.payload.base);
  w.u8(r.payload.lstride);
  w.u8(r.payload.lheight);
  w.u8(r.payload.format);
  w.u8(r.payload.palette_id);
  w.u8(r.payload.blend);
  w.u8(r.payload.view_mask);
  zhao_pack_rgb565(r.payload.tint, w);
  w.u8(r.payload.order);
  w.u8(r.payload.flags);
  w.u16(r.payload.src_id);
  for (int i = 0; i < 2; ++i) w.u8(r.payload.pad[i]);
  w.u32(r.payload.u);
  w.u32(r.payload.v);
  w.u32(r.payload.a00);
  w.u32(r.payload.a01);
  w.u32(r.payload.a10);
  w.u32(r.payload.a11);
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

inline bool zhao_unpack_seal_frame_plan(ZhReader& r, ZhRecordSealFramePlan& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.view_id = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.resource_gen = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.view_gen = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.giant_instance = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.plan_verts = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.plan_tris = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.plan_chunks = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.plan_refs = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.giant_refs = t; }
  if (!r.skip(4)) return false;
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
  { uint32_t t; if (!r.take32(t)) return false; out.payload.eye[0] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.eye[1] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.eye[2] = t; }
  if (!r.skip(4)) return false;
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

inline bool zhao_unpack_terrain_epoch(ZhReader& r, ZhRecordTerrainEpoch& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.epoch = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.op = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.reserved = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.island_table_handle = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.source_id = t; }
  return true;
}

inline bool zhao_unpack_submit_terrain_set(ZhReader& r, ZhRecordSubmitTerrainSet& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.resource_epoch = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.list_offset = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.list_bytes = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.list_crc32c = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.patch_count = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.view_mask = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.sequence = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.reserved0 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.reserved1 = t; }
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
  { uint32_t t; if (!r.take32(t)) return false; out.payload.material_set = t; }
  if (!zhao_unpack_transform2fx(r, out.payload.transform)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.screen_error = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.kind = static_cast<forge_kind>(t); }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.frame_tick[0] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.frame_tick[1] = t; }
  if (!r.skip(1)) return false;
  { uint16_t t; if (!r.take16(t)) return false; out.payload.material_id = t; }
  if (!r.skip(6)) return false;
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
  { uint32_t t; if (!r.take32(t)) return false; out.payload.terrain_material_set = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.terrain_material_id = t; }
  if (!r.skip(6)) return false;
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

inline bool zhao_unpack_publish_resource(ZhReader& r, ZhRecordPublishResource& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.resource = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.hps_addr_lo = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.hps_addr_hi = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.vram_dst = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.length = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.crc32c = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.new_generation = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.epoch = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.dst_slot = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.kind = t; }
  if (!r.skip(2)) return false;
  return true;
}

inline bool zhao_unpack_set_post(ZhReader& r, ZhRecordSetPost& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.bloom_gain = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flash_amount = t; }
  if (!r.skip(1)) return false;
  { uint16_t t; if (!r.take16(t)) return false; out.payload.bias_r = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.bias_g = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.bias_b = t; }
  if (!zhao_unpack_rgb565(r, out.payload.flash)) return false;
  if (!zhao_unpack_rgb565(r, out.payload.ink)) return false;
  if (!r.skip(2)) return false;
  return true;
}

inline bool zhao_unpack_set_grade_table(ZhReader& r, ZhRecordSetGradeTable& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.curve = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.first = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.count = t; }
  if (!r.skip(1)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[0] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[1] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[2] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[3] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[4] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[5] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[6] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[7] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[8] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[9] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[10] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[11] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[12] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[13] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[14] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[15] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[16] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[17] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[18] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[19] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[20] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[21] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[22] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[23] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[24] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[25] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[26] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[27] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[28] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[29] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[30] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[31] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[32] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[33] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[34] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[35] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[36] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[37] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[38] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[39] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[40] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[41] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[42] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[43] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[44] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[45] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[46] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[47] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[48] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[49] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[50] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[51] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[52] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[53] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[54] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[55] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[56] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[57] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[58] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[59] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[60] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[61] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[62] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[63] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[64] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[65] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[66] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[67] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[68] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[69] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[70] = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.vectors[71] = t; }
  if (!r.skip(4)) return false;
  return true;
}

inline bool zhao_unpack_set_population(ZhReader& r, ZhRecordSetPopulation& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.population = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.origin_x = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.origin_y = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.origin_z = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.active_count = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.plane_c = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.plane_nx = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.plane_ny = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.plane_nz = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.flags = t; }
  return true;
}

inline bool zhao_unpack_debug_trace_arm(ZhReader& r, ZhRecordDebugTraceArm& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.stage_mask = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  if (!r.skip(14)) return false;
  return true;
}

inline bool zhao_unpack_draw_posed_form(ZhReader& r, ZhRecordDrawPosedForm& out) {
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
  { uint16_t t; if (!r.take16(t)) return false; out.payload.clip_id = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.frame_no = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.sub = t; }
  if (!r.skip(11)) return false;
  return true;
}

inline bool zhao_unpack_draw_warped_form(ZhReader& r, ZhRecordDrawWarpedForm& out) {
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
  { uint32_t t; if (!r.take32(t)) return false; out.payload.warp_program = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.time = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.params[0] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.params[1] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.params[2] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.params[3] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.attributes[0] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.attributes[1] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.attributes[2] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.attributes[3] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.warp_attributes = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.attribute_mode = static_cast<warp_attribute_mode>(t); }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.warp_flags = t; }
  if (!r.skip(2)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.displacement_bound[0] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.displacement_bound[1] = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.displacement_bound[2] = t; }
  if (!r.skip(4)) return false;
  return true;
}

inline bool zhao_unpack_set_plane(ZhReader& r, ZhRecordSetPlane& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.slot = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.role = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.blend = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.opacity = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.format = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.wrap = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.view_mask = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.palette_id = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.width = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.height = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.flags = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.base = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.lstride = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.lheight = t; }
  if (!r.skip(2)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.a = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.b = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.c = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.d = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.u0 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.v0 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.line_scroll = t; }
  return true;
}

inline bool zhao_unpack_draw_sprite(ZhReader& r, ZhRecordDrawSprite& out) {
  out = {};
  if (!r.take16(out.hdr.opcode) || !r.take16(out.hdr.record_bytes) ||
      !r.take32(out.hdr.source_id) || !r.take32(out.hdr.flags) ||
      !r.take32(out.hdr.reserved0)) return false;
  { uint16_t t; if (!r.take16(t)) return false; out.payload.x = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.y = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.w = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.h = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.base = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.lstride = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.lheight = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.format = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.palette_id = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.blend = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.view_mask = t; }
  if (!zhao_unpack_rgb565(r, out.payload.tint)) return false;
  { uint8_t t; if (!r.take8(t)) return false; out.payload.order = t; }
  { uint8_t t; if (!r.take8(t)) return false; out.payload.flags = t; }
  { uint16_t t; if (!r.take16(t)) return false; out.payload.src_id = t; }
  if (!r.skip(2)) return false;
  { uint32_t t; if (!r.take32(t)) return false; out.payload.u = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.v = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.a00 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.a01 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.a10 = t; }
  { uint32_t t; if (!r.take32(t)) return false; out.payload.a11 = t; }
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
constexpr uint16_t ZHAO_PADS_SEAL_FRAME_PLAN[] = {28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_END_FRAME[] = {12, 13, 14, 15};
constexpr uint16_t ZHAO_PADS_SET_VIEW[] = {92, 93, 94, 95};
constexpr uint16_t ZHAO_PADS_SET_PRESENTATION_CONTRACT[] = {24, 25, 26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_TERRAIN_FIELD[] = {92, 93, 94, 95};
constexpr uint16_t ZHAO_PADS_SURFACE_STAMP[] = {44, 45, 46, 47};
constexpr uint16_t ZHAO_PADS_DRAW_POPULATION[] = {8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint16_t ZHAO_PADS_DRAW_PROCEDURAL[] = {39, 42, 43, 44, 45, 46, 47};
constexpr uint16_t ZHAO_PADS_DRAW_SKY[] = {146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159};
constexpr uint16_t ZHAO_PADS_SET_ENVIRONMENT[] = {26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_DEBUG_FRAME_BLIT[] = {2, 3, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_DEBUG_RUMBLE[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint16_t ZHAO_PADS_PUBLISH_RESOURCE[] = {30, 31};
constexpr uint16_t ZHAO_PADS_SET_POST[] = {3, 14, 15};
constexpr uint16_t ZHAO_PADS_SET_GRADE_TABLE[] = {3, 76, 77, 78, 79};
constexpr uint16_t ZHAO_PADS_DEBUG_TRACE_ARM[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint16_t ZHAO_PADS_DRAW_POSED_FORM[] = {21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
constexpr uint16_t ZHAO_PADS_DRAW_WARPED_FORM[] = {62, 63, 76, 77, 78, 79};
constexpr uint16_t ZHAO_PADS_SET_PLANE[] = {18, 19};
constexpr uint16_t ZHAO_PADS_DRAW_SPRITE[] = {22, 23};
constexpr ZhCommandInfo ZHAO_COMMAND_TABLE[] = {
  {"Nop", 0x0000, 16, true, nullptr, 0},
  {"BeginFrame", 0x0001, 32, true, nullptr, 0},
  {"SealFramePlan", 0x0003, 48, true, ZHAO_PADS_SEAL_FRAME_PLAN, 4},
  {"EndFrame", 0x0002, 32, true, ZHAO_PADS_END_FRAME, 4},
  {"SetView", 0x0010, 112, true, ZHAO_PADS_SET_VIEW, 4},
  {"SetPresentationContract", 0x0020, 48, true, ZHAO_PADS_SET_PRESENTATION_CONTRACT, 8},
  {"TerrainField", 0x0200, 112, true, ZHAO_PADS_TERRAIN_FIELD, 4},
  {"SurfaceStamp", 0x0210, 64, true, ZHAO_PADS_SURFACE_STAMP, 4},
  {"TerrainEpoch", 0x0220, 32, false, nullptr, 0},
  {"SubmitTerrainSet", 0x0230, 48, false, nullptr, 0},
  {"DrawForm", 0x0300, 32, true, nullptr, 0},
  {"DrawPopulation", 0x0301, 32, true, ZHAO_PADS_DRAW_POPULATION, 8},
  {"DrawProcedural", 0x0302, 64, true, ZHAO_PADS_DRAW_PROCEDURAL, 7},
  {"DrawSky", 0x0310, 176, false, ZHAO_PADS_DRAW_SKY, 14},
  {"SetEnvironment", 0x0311, 48, true, ZHAO_PADS_SET_ENVIRONMENT, 6},
  {"EmitAudioEvent", 0x0400, 32, true, nullptr, 0},
  {"DebugBootstrap", 0xF001, 64, false, nullptr, 0},
  {"DebugFrameBlit", 0xF002, 48, true, ZHAO_PADS_DEBUG_FRAME_BLIT, 18},
  {"DebugRumble", 0xF004, 32, true, ZHAO_PADS_DEBUG_RUMBLE, 13},
  {"PublishResource", 0x0030, 48, true, ZHAO_PADS_PUBLISH_RESOURCE, 2},
  {"SetPost", 0x0040, 32, true, ZHAO_PADS_SET_POST, 3},
  {"SetGradeTable", 0x0041, 96, true, ZHAO_PADS_SET_GRADE_TABLE, 5},
  {"SetPopulation", 0x0303, 48, true, nullptr, 0},
  {"DebugTraceArm", 0xF003, 32, true, ZHAO_PADS_DEBUG_TRACE_ARM, 14},
  {"DrawPosedForm", 0x0305, 48, true, ZHAO_PADS_DRAW_POSED_FORM, 11},
  {"DrawWarpedForm", 0x0304, 96, true, ZHAO_PADS_DRAW_WARPED_FORM, 6},
  {"SetPlane", 0x0306, 64, true, ZHAO_PADS_SET_PLANE, 2},
  {"DrawSprite", 0x0307, 64, true, ZHAO_PADS_DRAW_SPRITE, 2},
};
constexpr size_t ZHAO_COMMAND_COUNT = 28;
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
      if (!(v0 == 0u || v0 == 1u || v0 == 2u || v0 == 3u || v0 == 4u || v0 == 5u)) return false;
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
    case ZHAO_OP_DRAW_WARPED_FORM: {
      const uint32_t v0 = uint32_t(p[60]);  // attribute_mode: warp_attribute_mode
      if (!(v0 == 0u || v0 == 1u)) return false;
      return true;
    }
    default: return true;
  }
}

// .zcap ABI_INFO identity (capture_format.md 4.2)
inline constexpr const char* ZHAO_GENERATOR_NAME = "zhaozhou-abi-gen";
inline constexpr uint8_t ZHAO_GENERATOR_SHA256[32] = {0x06, 0xB5, 0xDF, 0x38, 0xE6, 0xC8, 0x38, 0x56, 0x0B, 0xC6, 0x6F, 0x7F, 0xE8, 0xE3, 0x35, 0x99, 0x65, 0xC1, 0xFA, 0x87, 0xB9, 0x1B, 0x90, 0xB9, 0x1A, 0x31, 0x8C, 0xD5, 0x03, 0x73, 0xED, 0xAE};
inline constexpr uint8_t ZHAO_ZIDL_SHA256[32] = {0x8D, 0xD1, 0x55, 0xE8, 0xE7, 0xEA, 0xBD, 0x59, 0x26, 0xA6, 0x36, 0xB1, 0x4C, 0xA2, 0x41, 0xBB, 0xE6, 0xB6, 0x7C, 0x32, 0xAF, 0xCA, 0x3B, 0x68, 0x22, 0xCD, 0xB7, 0x02, 0x63, 0x28, 0xCB, 0xDA};
inline constexpr uint32_t ZHAO_ZCAP_SCHEMA_VERSION = 1;

}  // namespace zhao_abi
