// field_host_schema_roundtrip.cpp -- the ZFH2 host-image envelope, packed and
// read back through the GENERATED offsets, plus the ordinal-versus-window
// translation asserted on the measured worked example.
//
// Law:  spec/form/field-host-image.md
//       owner directive sections 11.1-11.5 (commit 6262868c)
// Tests: FT107 (capsule pack/check byte-stable across two clean runs, no
//        timestamp dependency), FT015 (identical inputs yield byte-identical
//        capsules and maps).
//
// ---------------------------------------------------------------------------
// WHAT THIS ASSERTS, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------------------------------
// It asserts CORRECT behaviour, never a defect. In particular the
// ordinal-versus-window case asserts that the translation produces 0x17 and
// 0x0F from crater_ring's real register set -- not that some counter fires.
// A test whose only expected behaviour is that a buggy design returns a wrong
// answer passes only while the defect exists.
//
// It also avoids the shape the composed smoke was caught with today: no check
// here can be satisfied by a quantity that is always zero. Every assertion
// names a specific non-zero value derived from the schema or from a measured
// program.
//
// THE MASK TYPES ARE CHECKED AT COMPILE TIME, NOT HERE. WindowMask and
// RequiredMask cannot be assigned to one another, which is why this file
// cannot contain a runtime test for it -- the committed compile-fail control
// at tests/mutants/field_host_mask_type_confusion_mutant.cpp is that evidence,
// driven by `gen_field_host_schema.py --compile-fail-control` in both
// polarities.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "zfield/generated/zfield_host_image.hpp"

namespace hi = zfield::host_image;

namespace {

int checks = 0;
int failures = 0;

void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("  FAIL: %s\n", what);
  }
}

void check_eq_u32(uint32_t got, uint32_t want, const char* what) {
  ++checks;
  if (got != want) {
    ++failures;
    std::printf("  FAIL: %s -- expected %u (0x%X), got %u (0x%X)\n", what,
                want, want, got, got);
  }
}

// A small, explicit, dependency-free digest so the byte-stability claim can be
// quoted as a number rather than asserted as a feeling. FNV-1a 64.
uint64_t fnv1a(const uint8_t* p, size_t n) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ull;
  }
  return h;
}

// ---- little-endian field access at a schema offset -----------------------

void put_u8(uint8_t* b, uint32_t off, uint8_t v) { b[off] = v; }
void put_u16(uint8_t* b, uint32_t off, uint16_t v) {
  b[off] = uint8_t(v & 0xFF);
  b[off + 1] = uint8_t((v >> 8) & 0xFF);
}
void put_u32(uint8_t* b, uint32_t off, uint32_t v) {
  for (int i = 0; i < 4; ++i) b[off + i] = uint8_t((v >> (8 * i)) & 0xFF);
}
void put_u64(uint8_t* b, uint32_t off, uint64_t v) {
  for (int i = 0; i < 8; ++i) b[off + i] = uint8_t((v >> (8 * i)) & 0xFF);
}
uint8_t get_u8(const uint8_t* b, uint32_t off) { return b[off]; }
uint16_t get_u16(const uint8_t* b, uint32_t off) {
  return uint16_t(uint16_t(b[off]) | (uint16_t(b[off + 1]) << 8));
}
uint32_t get_u32(const uint8_t* b, uint32_t off) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= uint32_t(b[off + i]) << (8 * i);
  return v;
}
uint64_t get_u64(const uint8_t* b, uint32_t off) {
  uint64_t v = 0;
  for (int i = 0; i < 8; ++i) v |= uint64_t(b[off + i]) << (8 * i);
  return v;
}

// ==========================================================================
// 1. The generated offset constants agree with the generated structs.
// ==========================================================================
// The header already carries static_asserts on offsetof and sizeof, so a
// disagreement cannot compile. This re-checks a representative set at RUNTIME
// so that a future change which weakens the static_asserts is still caught,
// and so the numbers appear in a log a human reads.

void test_offsets_and_sizes() {
  std::printf("== record offsets and sizes ==\n");

  check_eq_u32(hi::ZFH_HDR_BYTES, 64, "header is 64 bytes");
  check_eq_u32(hi::ZFH_SEC_BYTES, 16, "section directory entry is 16 bytes");
  check_eq_u32(hi::ZFH_PM_BYTES, 64, "PROGRAM_META is 64 bytes");
  check_eq_u32(hi::ZFH_IN_BYTES, 8, "INPUT_MAP row is 8 bytes");
  check_eq_u32(hi::ZFH_OUT_BYTES, 8, "OUTPUT_MAP row is 8 bytes");
  check_eq_u32(hi::ZFH_IP_BYTES, 64, "INIT_PROOF is 64 bytes");
  check_eq_u32(hi::ZFH_AM_BYTES, 64, "ASSOCIATION_META is 64 bytes");
  check_eq_u32(hi::ZFH_PRE_BYTES, 8, "PRELOAD row is 8 bytes");

  check_eq_u32(uint32_t(sizeof(hi::ZfhHeader)), hi::ZFH_HDR_BYTES,
               "sizeof(ZfhHeader) matches ZFH_HDR_BYTES");
  check_eq_u32(uint32_t(sizeof(hi::ZfhProgramMeta)), hi::ZFH_PM_BYTES,
               "sizeof(ZfhProgramMeta) matches ZFH_PM_BYTES");
  check_eq_u32(uint32_t(sizeof(hi::ZfhInitProof)), hi::ZFH_IP_BYTES,
               "sizeof(ZfhInitProof) matches ZFH_IP_BYTES");
  check_eq_u32(uint32_t(sizeof(hi::ZfhAssociationMeta)), hi::ZFH_AM_BYTES,
               "sizeof(ZfhAssociationMeta) matches ZFH_AM_BYTES");

  // The directive spells PROGRAM_META's layout as 8 + 8 + 8 + 8 + 4 + 28.
  check_eq_u32(hi::ZFH_PM_OFF_PROFILE, 0, "PROGRAM_META.profile at 0");
  check_eq_u32(hi::ZFH_PM_OFF_REQUIRED_MASK, 6,
               "PROGRAM_META.required_mask at 6");
  check_eq_u32(hi::ZFH_PM_OFF_CANONICAL_INSTRUCTION_COUNT, 8,
               "PROGRAM_META u16 block starts at 8");
  check_eq_u32(hi::ZFH_PM_OFF_VARYING_INPUT_MASK, 16,
               "PROGRAM_META u32 block starts at 16");
  check_eq_u32(hi::ZFH_PM_OFF_LOGICAL_PLAN_IDENTITY, 32,
               "PROGRAM_META.logical_plan_identity at 32");
  check_eq_u32(hi::ZFH_PM_OFF_RESERVED, 36,
               "PROGRAM_META reserved[7] starts at 36");
  check_eq_u32(hi::ZFH_PM_LEN_RESERVED, 28,
               "PROGRAM_META reserved[7] is 28 bytes");

  // INIT_PROOF's masks are 64-bit because physical registers run 0..63.
  check_eq_u32(hi::ZFH_IP_LEN_ASSOCIATION_REGISTER_MASK, 8,
               "INIT_PROOF association_register_mask is 64 bits");
  check_eq_u32(hi::ZFH_IP_LEN_POINT_REGISTER_MASK, 8,
               "INIT_PROOF point_register_mask is 64 bits");
  check_eq_u32(hi::ZFH_IP_LEN_IMMUTABLE_REGISTER_MASK, 8,
               "INIT_PROOF immutable_register_mask is 64 bits");
  check_eq_u32(hi::ZFH_IP_LEN_INITIAL_DEFINED_MASK, 8,
               "INIT_PROOF initial_defined_mask is 64 bits");
  check_eq_u32(hi::ZFH_IP_LEN_VECTOR_WRITE_MASK, 8,
               "INIT_PROOF vector_write_mask is 64 bits");
  check_eq_u32(hi::ZFH_MAX_PHYSICAL_REGISTERS, 64,
               "64 physical registers, so no u32 mask may carry one");

  check_eq_u32(hi::ZFH_HEADER_BYTES, 64, "ZFH_HEADER_BYTES constant");
  check_eq_u32(hi::ZFH_ALIGNMENT, 64, "64-byte alignment");
  check_eq_u32(hi::ZFH_ADDR_UNUSED, 0xFFFF,
               "the unused map address is 0xFFFF, never a valid zero");
  check_eq_u32(hi::ZFH_IMAGE_VERSION, 1, "image_version is 1");
  check_eq_u32(hi::ZFH_HOST_PROTOCOL_VERSION, 2,
               "host_protocol_version is 2");
  check_eq_u32(hi::ZFH_MAX_CANONICAL_INPUTS, 15,
               "15 canonical inputs, warp being the widest");
  check_eq_u32(hi::ZFH_CANONICAL_INPUTS_BYTES, 64,
               "CANONICAL_INPUTS is 60 bytes plus 4 zero pad");
}

// ==========================================================================
// 2. Pack then read back every record type, through the generated offsets.
// ==========================================================================

void test_roundtrip_all_records() {
  std::printf("== pack/read-back, every record type ==\n");

  // ---- header ----
  {
    uint8_t b[64];
    std::memset(b, 0, sizeof b);
    put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 0, uint8_t(hi::ZFH_MAGIC0));
    put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 1, uint8_t(hi::ZFH_MAGIC1));
    put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 2, uint8_t(hi::ZFH_MAGIC2));
    put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 3, uint8_t(hi::ZFH_MAGIC3));
    put_u16(b, hi::ZFH_HDR_OFF_IMAGE_VERSION, uint16_t(hi::ZFH_IMAGE_VERSION));
    put_u8(b, hi::ZFH_HDR_OFF_OBJECT_KIND, hi::ZFH_OBJECT_KIND_PROGRAM);
    put_u32(b, hi::ZFH_HDR_OFF_TOTAL_BYTES, 0x00000400u);
    put_u32(b, hi::ZFH_HDR_OFF_BODY_CRC32C, 0xDEADBEEFu);
    put_u16(b, hi::ZFH_HDR_OFF_HOST_PROTOCOL_VERSION,
            uint16_t(hi::ZFH_HOST_PROTOCOL_VERSION));
    put_u32(b, hi::ZFH_HDR_OFF_CANONICAL_PROGRAM_HASH, 0x12345678u);
    put_u32(b, hi::ZFH_HDR_OFF_RESOURCE_EPOCH, 0x00C0FFEEu);
    put_u16(b, hi::ZFH_HDR_OFF_SECTION_COUNT, 6);
    put_u16(b, hi::ZFH_HDR_OFF_HEADER_BYTES, uint16_t(hi::ZFH_HEADER_BYTES));
    put_u32(b, hi::ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C, 0xABCD1234u);
    put_u32(b, hi::ZFH_HDR_OFF_OBJECT_SERIAL, 0x00000007u);

    check(get_u8(b, hi::ZFH_HDR_OFF_MAGIC + 0) == 0x5A &&
              get_u8(b, hi::ZFH_HDR_OFF_MAGIC + 1) == 0x46 &&
              get_u8(b, hi::ZFH_HDR_OFF_MAGIC + 2) == 0x48 &&
              get_u8(b, hi::ZFH_HDR_OFF_MAGIC + 3) == 0x32,
          "magic reads back as ASCII 'ZFH2' = 5A 46 48 32");
    check_eq_u32(get_u16(b, hi::ZFH_HDR_OFF_IMAGE_VERSION), 1,
                 "header image_version round-trips");
    check_eq_u32(get_u32(b, hi::ZFH_HDR_OFF_TOTAL_BYTES), 0x400u,
                 "header total_bytes round-trips");
    check_eq_u32(get_u32(b, hi::ZFH_HDR_OFF_BODY_CRC32C), 0xDEADBEEFu,
                 "header body_crc32c round-trips");
    check_eq_u32(get_u32(b, hi::ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C),
                 0xABCD1234u,
                 "canonical_full_image_crc32c round-trips and is a SEPARATE "
                 "field from body_crc32c");
    check_eq_u32(get_u16(b, hi::ZFH_HDR_OFF_SECTION_COUNT), 6,
                 "header section_count round-trips");
    check(get_u32(b, hi::ZFH_HDR_OFF_BODY_CRC32C) !=
              get_u32(b, hi::ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C),
          "the two CRCs occupy different bytes");
    check_eq_u32(get_u32(b, hi::ZFH_HDR_OFF_RESERVED_60), 0,
                 "header reserved_60 stays zero");
  }

  // ---- section directory entry ----
  {
    uint8_t b[16];
    std::memset(b, 0, sizeof b);
    put_u16(b, hi::ZFH_SEC_OFF_KIND, hi::ZFH_SECTION_OUTPUT_MAP);
    put_u16(b, hi::ZFH_SEC_OFF_ELEMENT_BYTES, uint16_t(hi::ZFH_OUT_BYTES));
    put_u32(b, hi::ZFH_SEC_OFF_ELEMENT_COUNT, 4);
    put_u32(b, hi::ZFH_SEC_OFF_OFFSET, 256);
    put_u32(b, hi::ZFH_SEC_OFF_BYTE_LENGTH, 4u * hi::ZFH_OUT_BYTES);

    check_eq_u32(get_u16(b, hi::ZFH_SEC_OFF_KIND), 0x0005,
                 "OUTPUT_MAP section kind is 0x0005");
    check_eq_u32(get_u32(b, hi::ZFH_SEC_OFF_BYTE_LENGTH), 32,
                 "byte_length is element_bytes * element_count");
    check_eq_u32(get_u32(b, hi::ZFH_SEC_OFF_OFFSET) % hi::ZFH_ALIGNMENT, 0,
                 "a section body offset is 64-byte aligned");
  }

  // ---- PROGRAM_META ----
  {
    uint8_t b[64];
    std::memset(b, 0, sizeof b);
    put_u8(b, hi::ZFH_PM_OFF_PROFILE, 0);  // earth
    put_u8(b, hi::ZFH_PM_OFF_EXECUTION_FORM,
           hi::ZFH_EXECUTION_FORM_PREPARED_REGISTER);
    put_u8(b, hi::ZFH_PM_OFF_INPUT_COUNT, 12);
    put_u8(b, hi::ZFH_PM_OFF_OUTPUT_COUNT, 4);
    put_u8(b, hi::ZFH_PM_OFF_REQUIRED_MASK, 0x0F);
    put_u8(b, hi::ZFH_PM_OFF_TABLE_COUNT, 2);
    put_u16(b, hi::ZFH_PM_OFF_CANONICAL_INSTRUCTION_COUNT, 29);
    put_u16(b, hi::ZFH_PM_OFF_PHYSICAL_UOP_COUNT, 31);
    put_u16(b, hi::ZFH_PM_OFF_PHYSICAL_REGISTER_COUNT, 24);
    put_u16(b, hi::ZFH_PM_OFF_PREPARED_SCALAR_COUNT, 5);
    put_u32(b, hi::ZFH_PM_OFF_VARYING_INPUT_MASK, 0x0000000Fu);
    put_u32(b, hi::ZFH_PM_OFF_POINT_PRELOAD_MASK, 0x00000003u);
    put_u32(b, hi::ZFH_PM_OFF_CODE_IMAGE_CRC, 0x11112222u);
    put_u32(b, hi::ZFH_PM_OFF_MAPS_CRC, 0x33334444u);
    put_u32(b, hi::ZFH_PM_OFF_LOGICAL_PLAN_IDENTITY, 0x55556666u);

    check_eq_u32(get_u8(b, hi::ZFH_PM_OFF_REQUIRED_MASK), 0x0F,
                 "PROGRAM_META.required_mask round-trips as the ORDINAL mask");
    check_eq_u32(get_u8(b, hi::ZFH_PM_OFF_OUTPUT_COUNT), 4,
                 "earth declares four outputs");
    check_eq_u32(get_u8(b, hi::ZFH_PM_OFF_INPUT_COUNT), 12,
                 "earth declares twelve inputs");
    check_eq_u32(get_u16(b, hi::ZFH_PM_OFF_PHYSICAL_UOP_COUNT), 31,
                 "physical_uop_count round-trips");
    check_eq_u32(get_u32(b, hi::ZFH_PM_OFF_MAPS_CRC), 0x33334444u,
                 "maps_crc round-trips and does not alias code_image_crc");
    check(get_u32(b, hi::ZFH_PM_OFF_CODE_IMAGE_CRC) !=
              get_u32(b, hi::ZFH_PM_OFF_MAPS_CRC),
          "code_image_crc and maps_crc occupy different bytes");
    // The seven reserved words must still be zero after every write above.
    bool reserved_clean = true;
    for (uint32_t i = 0; i < hi::ZFH_PM_LEN_RESERVED; ++i) {
      if (b[hi::ZFH_PM_OFF_RESERVED + i] != 0) reserved_clean = false;
    }
    check(reserved_clean,
          "PROGRAM_META's 28 reserved bytes are untouched by every other "
          "field -- they are not capacity for undocumented policy");
  }

  // ---- INPUT_MAP row ----
  {
    uint8_t b[8];
    std::memset(b, 0, sizeof b);
    put_u8(b, hi::ZFH_IN_OFF_ORDINAL, 3);
    put_u8(b, hi::ZFH_IN_OFF_SOURCE_CLASS, hi::ZFH_SOURCE_CLASS_UNIFORM_INPUT);
    put_u16(b, hi::ZFH_IN_OFF_PHYSICAL_REGISTER, uint16_t(hi::ZFH_ADDR_UNUSED));
    put_u16(b, hi::ZFH_IN_OFF_PREPARED_SLOT, 9);

    check_eq_u32(get_u8(b, hi::ZFH_IN_OFF_ORDINAL), 3,
                 "INPUT_MAP ordinal round-trips");
    check_eq_u32(get_u8(b, hi::ZFH_IN_OFF_SOURCE_CLASS), 1,
                 "UNIFORM_INPUT is source_class 1");
    check_eq_u32(get_u16(b, hi::ZFH_IN_OFF_PHYSICAL_REGISTER), 0xFFFF,
                 "an unused register address is 0xFFFF, NOT register zero");
    check_eq_u32(get_u16(b, hi::ZFH_IN_OFF_PREPARED_SLOT), 9,
                 "prepared_slot round-trips");
  }

  // ---- INIT_PROOF, with registers above 31 deliberately set ----
  {
    uint8_t b[64];
    std::memset(b, 0, sizeof b);
    const uint64_t assoc = 0x0000000100000003ull;  // R0,R1 and R32
    const uint64_t point = 0x8000000000000010ull;  // R4 and R63
    put_u16(b, hi::ZFH_IP_OFF_PROOF_VERSION, 1);
    put_u16(b, hi::ZFH_IP_OFF_RECORD_BYTES, 64);
    put_u32(b, hi::ZFH_IP_OFF_FLAGS, hi::ZFH_INITPROOF_FLAG_PREPARED);
    put_u64(b, hi::ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK, assoc);
    put_u64(b, hi::ZFH_IP_OFF_POINT_REGISTER_MASK, point);
    put_u64(b, hi::ZFH_IP_OFF_INITIAL_DEFINED_MASK, assoc | point);
    put_u64(b, hi::ZFH_IP_OFF_VECTOR_WRITE_MASK, 0x00000000000000F0ull);
    put_u64(b, hi::ZFH_IP_OFF_IMMUTABLE_REGISTER_MASK, 0x0000000100000003ull);
    put_u16(b, hi::ZFH_IP_OFF_EXPECTED_ASSOCIATION_PRELOAD_COUNT, 3);

    check(get_u64(b, hi::ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK) == assoc,
          "association_register_mask round-trips INCLUDING register 32");
    check(get_u64(b, hi::ZFH_IP_OFF_POINT_REGISTER_MASK) == point,
          "point_register_mask round-trips INCLUDING register 63");
    check((get_u64(b, hi::ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK) >> 32) != 0,
          "the high half survives, so no u32 mask silently discarded R32..R63");
    check(get_u64(b, hi::ZFH_IP_OFF_INITIAL_DEFINED_MASK) ==
              (get_u64(b, hi::ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK) |
               get_u64(b, hi::ZFH_IP_OFF_POINT_REGISTER_MASK)),
          "initial_defined_mask == association | point, the directive's law");
    check((get_u64(b, hi::ZFH_IP_OFF_IMMUTABLE_REGISTER_MASK) &
           get_u64(b, hi::ZFH_IP_OFF_VECTOR_WRITE_MASK)) == 0,
          "immutable registers do not overlap vector_write_mask");
    // The preload count is the POPULATION COUNT, not the highest index + 1.
    uint64_t m = assoc;
    uint32_t pop = 0;
    while (m) {
      pop += uint32_t(m & 1u);
      m >>= 1;
    }
    check_eq_u32(pop, 3, "association mask popcount is 3");
    check_eq_u32(get_u16(b, hi::ZFH_IP_OFF_EXPECTED_ASSOCIATION_PRELOAD_COUNT),
                 pop,
                 "expected_association_preload_count is the POPCOUNT, not the "
                 "highest register number plus one (which would be 33)");
  }

  // ---- ASSOCIATION_META ----
  {
    uint8_t b[64];
    std::memset(b, 0, sizeof b);
    put_u32(b, hi::ZFH_AM_OFF_ASSOCIATION_SERIAL, 0x0000002Au);
    put_u32(b, hi::ZFH_AM_OFF_FRAME_ID, 1234);
    put_u8(b, hi::ZFH_AM_OFF_PROFILE, 0);
    put_u32(b, hi::ZFH_AM_OFF_EXPECTED_POINTS, 297);
    put_u32(b, hi::ZFH_AM_OFF_VARYING_INPUT_MASK, 0x0000000Fu);
    put_u32(b, hi::ZFH_AM_OFF_UNIFORM_INPUT_MASK, 0x00000FF0u);
    put_u64(b, hi::ZFH_AM_OFF_CLIENT_BINDING_COOKIE, 0x0123456789ABCDEFull);

    check_eq_u32(get_u32(b, hi::ZFH_AM_OFF_EXPECTED_POINTS), 297,
                 "expected_points counts USEFUL application points");
    check(get_u64(b, hi::ZFH_AM_OFF_CLIENT_BINDING_COOKIE) ==
              0x0123456789ABCDEFull,
          "the 64-bit client_binding_cookie round-trips whole");
    check((get_u32(b, hi::ZFH_AM_OFF_VARYING_INPUT_MASK) &
           get_u32(b, hi::ZFH_AM_OFF_UNIFORM_INPUT_MASK)) == 0,
          "varying and uniform input masks are disjoint");
    bool am_reserved_clean = true;
    for (uint32_t i = 0; i < hi::ZFH_AM_LEN_RESERVED; ++i) {
      if (b[hi::ZFH_AM_OFF_RESERVED + i] != 0) am_reserved_clean = false;
    }
    check(am_reserved_clean, "ASSOCIATION_META reserved[4] stays zero");
  }

  // ---- PRELOAD row, including a negative value ----
  {
    uint8_t b[8];
    std::memset(b, 0, sizeof b);
    put_u16(b, hi::ZFH_PRE_OFF_PHYSICAL_REGISTER, 40);
    put_u32(b, hi::ZFH_PRE_OFF_VALUE, uint32_t(int32_t(-65536)));

    check_eq_u32(get_u16(b, hi::ZFH_PRE_OFF_PHYSICAL_REGISTER), 40,
                 "PRELOAD names a register above 31");
    check(int32_t(get_u32(b, hi::ZFH_PRE_OFF_VALUE)) == -65536,
          "a negative preload value (-1.0 in Q16.16) survives the round trip");
  }
}

// ==========================================================================
// 3. THE ORDINAL-VERSUS-WINDOW TRANSLATION, on the measured worked example.
// ==========================================================================
// This is the reason the schema exists. crater_ring writes R13,R14,R15,R17
// with out_base = 13, measured by tools/field/zprog_output_coverage.py.
// The window mask is 0x17 and the ordinal mask is 0x0F, and OUTPUT_MAP is the
// translation between them.

struct OutRow {
  uint8_t ordinal;
  uint8_t source_kind;
  uint16_t source_index;
  bool required;
};

// Derive the window mask from OUTPUT_MAP. This is the direction the host needs
// and it is NOT the identity: a PREPARED_SCALAR ordinal contributes no window
// bit at all.
bool derive_window_mask(const std::vector<OutRow>& rows, uint16_t out_base,
                        uint32_t out_lanes, hi::WindowMask* out,
                        const char** refusal) {
  uint32_t bits = 0;
  for (const OutRow& r : rows) {
    if (r.source_kind == hi::ZFH_SOURCE_KIND_PREPARED_SCALAR) {
      // Legitimate, and it has no window position by construction.
      continue;
    }
    if (r.source_index < out_base) {
      *refusal = "BAD_IMAGE: output register below out_base";
      return false;
    }
    const uint32_t k = uint32_t(r.source_index) - uint32_t(out_base);
    if (k >= out_lanes) {
      // The window cannot observe this write, so the ordinal could never be
      // seen and the point would hang or refuse for the wrong reason.
      *refusal = "BAD_IMAGE: output register outside the capture window";
      return false;
    }
    bits |= (1u << k);
  }
  *out = hi::WindowMask(hi::WindowMask::rep(bits));
  return true;
}

hi::RequiredMask derive_required_mask(const std::vector<OutRow>& rows) {
  uint32_t bits = 0;
  for (const OutRow& r : rows) {
    if (r.required) bits |= (1u << r.ordinal);
  }
  return hi::RequiredMask(hi::RequiredMask::rep(bits));
}

void test_ordinal_vs_window() {
  std::printf("== ordinal-versus-window translation (crater_ring) ==\n");

  const uint16_t out_base = 13;
  const uint32_t out_lanes = 7;  // as composed in zhao_console_core

  std::vector<OutRow> crater_ring = {
      {0, hi::ZFH_SOURCE_KIND_VECTOR_REG, 13, true},
      {1, hi::ZFH_SOURCE_KIND_VECTOR_REG, 14, true},
      {2, hi::ZFH_SOURCE_KIND_VECTOR_REG, 15, true},
      {3, hi::ZFH_SOURCE_KIND_VECTOR_REG, 17, true},
  };

  hi::WindowMask win;
  const char* refusal = nullptr;
  const bool ok = derive_window_mask(crater_ring, out_base, out_lanes, &win,
                                     &refusal);
  check(ok, "crater_ring's output map is installable");
  check_eq_u32(win.bits(), 0x17,
               "window mask is 0x17 -- window positions 0,1,2,4, with a HOLE "
               "at 3 because R16 is not written");

  const hi::RequiredMask req = derive_required_mask(crater_ring);
  check_eq_u32(req.bits(), 0x0F,
               "ordinal mask is 0x0F -- four contiguous canonical ordinals");

  // THE WHOLE POINT, asserted as a value and enforced as a type.
  check(uint32_t(win.bits()) != uint32_t(req.bits()),
        "the two masks DIFFER in value for a real shipped program, so wiring "
        "one to the other would be a plausible wrong number");

  // The completeness decision uses the ORDINAL mask against ordinal `seen`.
  const hi::RequiredMask seen_all(0x0F);
  const hi::RequiredMask seen_partial(0x07);
  check(seen_all.covers(req),
        "all four ordinals seen -> the required set is covered");
  check(!seen_partial.covers(req),
        "three of four ordinals seen -> NOT covered; this is ST_PARTIAL, and "
        "it is the case the pre-R101 any-output test called SUCCESS");

  // A uniform output has no window position, and must not be waited for.
  std::vector<OutRow> with_uniform = {
      {0, hi::ZFH_SOURCE_KIND_VECTOR_REG, 13, true},
      {1, hi::ZFH_SOURCE_KIND_PREPARED_SCALAR, 2, true},
      {2, hi::ZFH_SOURCE_KIND_VECTOR_REG, 15, true},
  };
  hi::WindowMask win2;
  check(derive_window_mask(with_uniform, out_base, out_lanes, &win2, &refusal),
        "a map mixing a prepared scalar with vector registers is installable");
  check_eq_u32(win2.bits(), 0x05,
               "the PREPARED_SCALAR ordinal contributes NO window bit, so the "
               "window mask is 0x05 while three ordinals are required");
  check_eq_u32(derive_required_mask(with_uniform).bits(), 0x07,
               "all three ordinals are still required");
  check(derive_required_mask(with_uniform).bits() != win2.bits(),
        "uniform outputs make the two masks differ by construction, not by "
        "accident");

  // Two ordinals aliasing one register is LEGAL (directive 7.1).
  std::vector<OutRow> aliased = {
      {0, hi::ZFH_SOURCE_KIND_VECTOR_REG, 13, true},
      {1, hi::ZFH_SOURCE_KIND_VECTOR_REG, 13, true},
  };
  hi::WindowMask win3;
  check(derive_window_mask(aliased, out_base, out_lanes, &win3, &refusal),
        "two ordinals naming one register is legal");
  check_eq_u32(win3.bits(), 0x01, "aliasing sets one window bit");
  check_eq_u32(derive_required_mask(aliased).bits(), 0x03,
               "aliasing still requires BOTH ordinals");

  // THE REFUSAL, seen to fire: a register outside the capture window.
  std::vector<OutRow> out_of_window = {
      {0, hi::ZFH_SOURCE_KIND_VECTOR_REG, 13, true},
      {1, hi::ZFH_SOURCE_KIND_VECTOR_REG, 25, true},  // 25 - 13 = 12 >= 7
  };
  hi::WindowMask win4;
  refusal = nullptr;
  check(!derive_window_mask(out_of_window, out_base, out_lanes, &win4,
                            &refusal),
        "an output register outside [out_base, out_base+OUT_LANES) is REFUSED "
        "at load time rather than silently unobservable");
  check(refusal != nullptr &&
            std::strstr(refusal, "outside the capture window") != nullptr,
        "the refusal names the capture window");

  // And a register BELOW out_base, the other end of the same hazard.
  std::vector<OutRow> below = {
      {0, hi::ZFH_SOURCE_KIND_VECTOR_REG, 12, true},
  };
  refusal = nullptr;
  check(!derive_window_mask(below, out_base, out_lanes, &win4, &refusal),
        "an output register BELOW out_base is refused too");

  // required_mask == 0 with declared outputs is R111's standing hazard.
  std::vector<OutRow> undeclared = {
      {0, hi::ZFH_SOURCE_KIND_VECTOR_REG, 13, false},
      {1, hi::ZFH_SOURCE_KIND_VECTOR_REG, 14, false},
  };
  check(derive_required_mask(undeclared).empty(),
        "a map declaring no REQUIRED ordinal yields an empty required mask -- "
        "which a strict production import must REFUSE, because zero is the "
        "value a plan writer produces by omission");
  check(!derive_required_mask(crater_ring).empty(),
        "and a properly declared program does NOT produce zero, so the "
        "refusal discriminates rather than rejecting everything");
}

// ==========================================================================
// 4. Byte stability -- FT107 / FT015.
// ==========================================================================

std::vector<uint8_t> build_capsule() {
  // A complete small PROGRAM image: header, three directory entries, three
  // 64-byte-aligned bodies. Nothing here reads a clock, an environment
  // variable, a pointer value or a hash-map order.
  const uint32_t kSections = 3;
  const uint32_t dir_bytes = kSections * hi::ZFH_SEC_BYTES;
  uint32_t body0 = hi::ZFH_HEADER_BYTES + dir_bytes;
  body0 = (body0 + hi::ZFH_ALIGNMENT - 1) / hi::ZFH_ALIGNMENT *
          hi::ZFH_ALIGNMENT;
  const uint32_t pm_at = body0;
  const uint32_t om_at = pm_at + hi::ZFH_ALIGNMENT;
  const uint32_t ip_at = om_at + hi::ZFH_ALIGNMENT;
  const uint32_t total = ip_at + hi::ZFH_ALIGNMENT;

  std::vector<uint8_t> img(total, 0);
  uint8_t* b = img.data();

  put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 0, uint8_t(hi::ZFH_MAGIC0));
  put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 1, uint8_t(hi::ZFH_MAGIC1));
  put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 2, uint8_t(hi::ZFH_MAGIC2));
  put_u8(b, hi::ZFH_HDR_OFF_MAGIC + 3, uint8_t(hi::ZFH_MAGIC3));
  put_u16(b, hi::ZFH_HDR_OFF_IMAGE_VERSION, uint16_t(hi::ZFH_IMAGE_VERSION));
  put_u8(b, hi::ZFH_HDR_OFF_OBJECT_KIND, hi::ZFH_OBJECT_KIND_PROGRAM);
  put_u32(b, hi::ZFH_HDR_OFF_TOTAL_BYTES, total);
  put_u16(b, hi::ZFH_HDR_OFF_HOST_PROTOCOL_VERSION,
          uint16_t(hi::ZFH_HOST_PROTOCOL_VERSION));
  put_u32(b, hi::ZFH_HDR_OFF_CANONICAL_PROGRAM_HASH, 0x5A5A1234u);
  put_u16(b, hi::ZFH_HDR_OFF_SECTION_COUNT, uint16_t(kSections));
  put_u16(b, hi::ZFH_HDR_OFF_HEADER_BYTES, uint16_t(hi::ZFH_HEADER_BYTES));
  put_u32(b, hi::ZFH_HDR_OFF_OBJECT_SERIAL, 7);

  // Directory entries, ascending by kind as the directive requires.
  struct Ent {
    uint16_t kind;
    uint16_t ebytes;
    uint32_t ecount;
    uint32_t at;
  };
  const Ent ents[kSections] = {
      {hi::ZFH_SECTION_PROGRAM_META, uint16_t(hi::ZFH_PM_BYTES), 1, pm_at},
      {hi::ZFH_SECTION_OUTPUT_MAP, uint16_t(hi::ZFH_OUT_BYTES), 4, om_at},
      {hi::ZFH_SECTION_INIT_PROOF, uint16_t(hi::ZFH_IP_BYTES), 1, ip_at},
  };
  for (uint32_t i = 0; i < kSections; ++i) {
    const uint32_t e = hi::ZFH_HEADER_BYTES + i * hi::ZFH_SEC_BYTES;
    put_u16(b, e + hi::ZFH_SEC_OFF_KIND, ents[i].kind);
    put_u16(b, e + hi::ZFH_SEC_OFF_ELEMENT_BYTES, ents[i].ebytes);
    put_u32(b, e + hi::ZFH_SEC_OFF_ELEMENT_COUNT, ents[i].ecount);
    put_u32(b, e + hi::ZFH_SEC_OFF_OFFSET, ents[i].at);
    put_u32(b, e + hi::ZFH_SEC_OFF_BYTE_LENGTH,
            uint32_t(ents[i].ebytes) * ents[i].ecount);
  }

  // PROGRAM_META
  put_u8(b, pm_at + hi::ZFH_PM_OFF_PROFILE, 0);
  put_u8(b, pm_at + hi::ZFH_PM_OFF_EXECUTION_FORM,
         hi::ZFH_EXECUTION_FORM_PREPARED_REGISTER);
  put_u8(b, pm_at + hi::ZFH_PM_OFF_INPUT_COUNT, 12);
  put_u8(b, pm_at + hi::ZFH_PM_OFF_OUTPUT_COUNT, 4);
  put_u8(b, pm_at + hi::ZFH_PM_OFF_REQUIRED_MASK, 0x0F);
  put_u16(b, pm_at + hi::ZFH_PM_OFF_PHYSICAL_UOP_COUNT, 31);

  // OUTPUT_MAP, crater_ring
  const uint16_t src[4] = {13, 14, 15, 17};
  for (uint32_t j = 0; j < 4; ++j) {
    const uint32_t r = om_at + j * hi::ZFH_OUT_BYTES;
    put_u8(b, r + hi::ZFH_OUT_OFF_ORDINAL, uint8_t(j));
    put_u8(b, r + hi::ZFH_OUT_OFF_SOURCE_KIND,
           hi::ZFH_SOURCE_KIND_VECTOR_REG);
    put_u8(b, r + hi::ZFH_OUT_OFF_FLAGS,
           uint8_t(hi::ZFH_OUTPUT_FLAG_REQUIRED));
    put_u16(b, r + hi::ZFH_OUT_OFF_SOURCE_INDEX, src[j]);
  }

  // INIT_PROOF
  put_u16(b, ip_at + hi::ZFH_IP_OFF_PROOF_VERSION, 1);
  put_u16(b, ip_at + hi::ZFH_IP_OFF_RECORD_BYTES, 64);
  put_u32(b, ip_at + hi::ZFH_IP_OFF_FLAGS, hi::ZFH_INITPROOF_FLAG_PREPARED);
  put_u64(b, ip_at + hi::ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK,
          0x0000000000000FFFull);
  put_u64(b, ip_at + hi::ZFH_IP_OFF_POINT_REGISTER_MASK, 0x0000000000003000ull);
  put_u64(b, ip_at + hi::ZFH_IP_OFF_INITIAL_DEFINED_MASK,
          0x0000000000003FFFull);
  return img;
}

void test_byte_stability() {
  std::printf("== byte stability (FT107, FT015) ==\n");
  const std::vector<uint8_t> a = build_capsule();
  const std::vector<uint8_t> b = build_capsule();

  check(a.size() == b.size(), "two builds produce the same length");
  check(a.size() > 0 && std::memcmp(a.data(), b.data(), a.size()) == 0,
        "two builds of identical inputs are BYTE-IDENTICAL");
  check_eq_u32(uint32_t(a.size()) % hi::ZFH_ALIGNMENT, 0,
               "total_bytes is 64-byte aligned");
  check_eq_u32(get_u32(a.data(), hi::ZFH_HDR_OFF_TOTAL_BYTES),
               uint32_t(a.size()),
               "header total_bytes equals the real length");

  // Directory entries ascending by kind, non-overlapping, aligned.
  const uint32_t n = get_u16(a.data(), hi::ZFH_HDR_OFF_SECTION_COUNT);
  check_eq_u32(n, 3, "three sections");
  uint32_t prev_kind = 0;
  bool ascending = true, aligned = true, in_bounds = true;
  for (uint32_t i = 0; i < n; ++i) {
    const uint32_t e = hi::ZFH_HEADER_BYTES + i * hi::ZFH_SEC_BYTES;
    const uint32_t kind = get_u16(a.data(), e + hi::ZFH_SEC_OFF_KIND);
    const uint32_t at = get_u32(a.data(), e + hi::ZFH_SEC_OFF_OFFSET);
    const uint32_t len = get_u32(a.data(), e + hi::ZFH_SEC_OFF_BYTE_LENGTH);
    const uint32_t eb = get_u16(a.data(), e + hi::ZFH_SEC_OFF_ELEMENT_BYTES);
    const uint32_t ec = get_u32(a.data(), e + hi::ZFH_SEC_OFF_ELEMENT_COUNT);
    if (i > 0 && kind <= prev_kind) ascending = false;
    prev_kind = kind;
    if (at % hi::ZFH_ALIGNMENT != 0) aligned = false;
    if (uint64_t(at) + len > a.size()) in_bounds = false;
    check_eq_u32(len, eb * ec, "byte_length == element_bytes * element_count");
  }
  check(ascending, "directory entries are sorted ascending by kind");
  check(aligned, "every section body is 64-byte aligned");
  check(in_bounds, "every section body lies inside total_bytes");

  const uint64_t h = fnv1a(a.data(), a.size());
  std::printf("  capsule: %u bytes, fnv1a-64 = 0x%016llX\n",
              uint32_t(a.size()), (unsigned long long)h);
  std::printf("  (run this target twice and compare the line above: FT107 is "
              "byte stability across two clean runs)\n");
  // The digest must not be the degenerate FNV offset basis, which is what an
  // empty buffer would produce. A hash quoted from nothing is the shape this
  // repo distrusts.
  check(h != 1469598103934665603ull,
        "the digest is of real content, not of an empty buffer");
}

// ==========================================================================
// 5. Profile arity -- the (11)/(14) defect, pinned in code.
// ==========================================================================

void test_profile_arity() {
  std::printf("== profile arity (field-ir.md 7.1) ==\n");
  check_eq_u32(uint32_t(hi::ZFH_PROFILE_COUNT), 5, "five profiles");

  struct Want {
    const char* name;
    uint8_t id, in, out;
  };
  const Want want[5] = {
      {"earth", 0, 12, 4}, {"warp", 1, 15, 6},    {"flow", 2, 13, 7},
      {"formation", 3, 12, 6}, {"stamp", 4, 8, 3},
  };
  for (int i = 0; i < hi::ZFH_PROFILE_COUNT; ++i) {
    const hi::ZfhProfileArity& p = hi::ZFH_PROFILES[i];
    check(std::strcmp(p.name, want[i].name) == 0, "profile name in order");
    check_eq_u32(p.id, want[i].id, "profile id");
    check_eq_u32(p.input_count, want[i].in, "profile input count");
    check_eq_u32(p.output_count, want[i].out, "profile output count");
  }
  // Named explicitly, because both of these were WRONG in the spec and the
  // corrections are what the generator now gates.
  check_eq_u32(hi::ZFH_PROFILES[3].input_count, 12,
               "formation has TWELVE inputs -- the spec said (11) until "
               "2026-09-20 against twelve listed fields");
  check_eq_u32(hi::ZFH_PROFILES[1].input_count, 15,
               "warp has FIFTEEN inputs -- the spec said (14) until W01");
  check(hi::ZFH_PROFILES[2].output_count == 7 &&
            hi::ZFH_PROFILES[2].output_count <= hi::ZFH_MAX_CANONICAL_OUTPUTS,
        "flow's seven outputs is the widest profile and fits the bus");
  check(hi::ZFH_PROFILES[1].input_count <= hi::ZFH_MAX_CANONICAL_INPUTS,
        "warp's fifteen inputs fits ZFH_MAX_CANONICAL_INPUTS");
}

// ==========================================================================
// 6. Section kinds and enumerations.
// ==========================================================================

void test_section_kinds() {
  std::printf("== section kinds and enumerations ==\n");
  check_eq_u32(hi::ZFH_SECTION_CANONICAL_IMAGE, 0x0001, "CANONICAL_IMAGE");
  check_eq_u32(hi::ZFH_SECTION_PROGRAM_META, 0x0002, "PROGRAM_META");
  check_eq_u32(hi::ZFH_SECTION_INPUT_MAP, 0x0004, "INPUT_MAP");
  check_eq_u32(hi::ZFH_SECTION_OUTPUT_MAP, 0x0005, "OUTPUT_MAP");
  check_eq_u32(hi::ZFH_SECTION_INIT_PROOF, 0x0006, "INIT_PROOF");
  check_eq_u32(hi::ZFH_SECTION_TABLE0, 0x0010, "TABLE0");
  check_eq_u32(hi::ZFH_SECTION_TABLE3, 0x0013, "TABLE3");
  check_eq_u32(hi::ZFH_SECTION_ASSOCIATION_META, 0x0020, "ASSOCIATION_META");
  check_eq_u32(hi::ZFH_SECTION_PRELOAD, 0x0023, "PRELOAD");
  check_eq_u32(hi::ZFH_SECTION_KIND_COUNT, 16, "sixteen section kinds");
  check(hi::ZFH_SECTION_TABLE3 - hi::ZFH_SECTION_TABLE0 + 1 ==
            hi::ZFH_MAX_TABLES,
        "the TABLE kinds are exactly ZFH_MAX_TABLES consecutive values");

  check_eq_u32(hi::ZFH_OBJECT_KIND_PROGRAM, 0, "PROGRAM is object kind 0");
  check_eq_u32(hi::ZFH_OBJECT_KIND_ASSOCIATION, 1,
               "ASSOCIATION is object kind 1");
  check_eq_u32(hi::ZFH_EXECUTION_FORM_UNIFORM_ONLY, 2, "UNIFORM_ONLY is 2");
  check_eq_u32(hi::ZFH_SOURCE_CLASS_UNUSED_PROVEN, 2, "UNUSED_PROVEN is 2");
  check_eq_u32(hi::ZFH_SOURCE_KIND_VECTOR_REG, 0, "VECTOR_REG is 0");
  check_eq_u32(hi::ZFH_SOURCE_KIND_PREPARED_SCALAR, 1,
               "PREPARED_SCALAR is 1");
  check_eq_u32(hi::ZFH_OUTPUT_FLAG_REQUIRED, 1, "OUTPUT_MAP REQUIRED is bit 0");
  check(hi::ZFH_INITPROOF_FLAG_CANONICAL != hi::ZFH_INITPROOF_FLAG_PREPARED,
        "the two INIT_PROOF validation flags are distinct bits");
  check((hi::ZFH_INITPROOF_FLAG_CANONICAL &
         hi::ZFH_INITPROOF_FLAG_PREPARED) == 0,
        "and they do not overlap, so exactly one can be set");
}

}  // namespace

int main() {
  std::printf("field_host_schema_roundtrip: ZFH2 envelope, "
              "spec/form/field-host-image.md\n");
  test_offsets_and_sizes();
  test_roundtrip_all_records();
  test_ordinal_vs_window();
  test_byte_stability();
  test_profile_arity();
  test_section_kinds();

  std::printf("field_host_schema_roundtrip: %d checks, %d failures\n", checks,
              failures);
  if (failures == 0) std::printf("field_host_schema_roundtrip: all green\n");
  return failures == 0 ? 0 : 1;
}
