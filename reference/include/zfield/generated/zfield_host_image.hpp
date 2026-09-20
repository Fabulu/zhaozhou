// GENERATED FILE - tools/field/gen_field_host_schema.py - DO NOT EDIT.
// The frozen ZFH2 host-image envelope (spec/form/field-host-image.md),
// emitted from ONE schema into this header and into
// fpga/rtl/field/generated/zhao_field_host_image_pkg.sv so the offsets
// cannot disagree. Regenerate:
//   python tools/field/gen_field_host_schema.py
// and commit. `--check` is the freshness gate.
// schema-sha256: 162fb7f8e391715c3b2078d9ca5c3d0b9a79bc06633ad8e9c7711b03d4b65d1c
//
// THE TWO MASKS ARE DIFFERENT TYPES ON PURPOSE. RequiredMask is indexed by
// canonical output ORDINAL; WindowMask is indexed by contiguous capture-
// WINDOW POSITION. They coincide only when output registers are contiguous
// from out_base, and R111 measured that they never are. Assigning one to
// the other does not compile -- see the compile-fail control at
// tests/mutants/field_host_mask_type_confusion_mutant.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

namespace zfield {
namespace host_image {

// ------------------------------------------------ constants ---

// 'Z'
inline constexpr uint32_t ZFH_MAGIC0 = 90;
// 'F'
inline constexpr uint32_t ZFH_MAGIC1 = 70;
// 'H'
inline constexpr uint32_t ZFH_MAGIC2 = 72;
// '2'
inline constexpr uint32_t ZFH_MAGIC3 = 50;
// header.image_version in this revision
inline constexpr uint32_t ZFH_IMAGE_VERSION = 1;
// header.host_protocol_version
inline constexpr uint32_t ZFH_HOST_PROTOCOL_VERSION = 2;
// header.header_bytes
inline constexpr uint32_t ZFH_HEADER_BYTES = 64;
// section bodies and total_bytes are aligned to this
inline constexpr uint32_t ZFH_ALIGNMENT = 64;
// the unused address in a map row -- never an implicitly valid zero
inline constexpr uint32_t ZFH_ADDR_UNUSED = 0xFFFF;
// warp's 15 is the widest profile; bounds the ordinal masks
inline constexpr uint32_t ZFH_MAX_CANONICAL_INPUTS = 15;
// flow's 7 is the widest profile
inline constexpr uint32_t ZFH_MAX_CANONICAL_OUTPUTS = 7;
// the INIT_PROOF masks are 64 bits because of this
inline constexpr uint32_t ZFH_MAX_PHYSICAL_REGISTERS = 64;
// TABLE0..TABLE3
inline constexpr uint32_t ZFH_MAX_TABLES = 4;
// 15 words = 60 bytes plus 4 zero pad bytes (directive 11.4)
inline constexpr uint32_t ZFH_CANONICAL_INPUTS_BYTES = 64;

// -------------------------------------------- section kinds ---

inline constexpr uint16_t ZFH_SECTION_CANONICAL_IMAGE = 0x0001;  // the validated .zprog, exact bytes
inline constexpr uint16_t ZFH_SECTION_PROGRAM_META = 0x0002;  // singleton
inline constexpr uint16_t ZFH_SECTION_PHYSICAL_UOPS = 0x0003;  // 8-byte native words
inline constexpr uint16_t ZFH_SECTION_INPUT_MAP = 0x0004;  // one row per canonical input ordinal
inline constexpr uint16_t ZFH_SECTION_OUTPUT_MAP = 0x0005;  // one row per canonical output ordinal
inline constexpr uint16_t ZFH_SECTION_INIT_PROOF = 0x0006;  // singleton
inline constexpr uint16_t ZFH_SECTION_DEMAND = 0x0007;  // optional; per-resource counts for the selected form
inline constexpr uint16_t ZFH_SECTION_TABLE0 = 0x0010;  // canonical table body bytes, t=0
inline constexpr uint16_t ZFH_SECTION_TABLE1 = 0x0011;  // canonical table body bytes, t=1
inline constexpr uint16_t ZFH_SECTION_TABLE2 = 0x0012;  // canonical table body bytes, t=2
inline constexpr uint16_t ZFH_SECTION_TABLE3 = 0x0013;  // canonical table body bytes, t=3
inline constexpr uint16_t ZFH_SECTION_ASSOCIATION_META = 0x0020;  // singleton
inline constexpr uint16_t ZFH_SECTION_CANONICAL_INPUTS = 0x0021;  // 15 raw words, 60 bytes + 4 zero pad
inline constexpr uint16_t ZFH_SECTION_PREPARED_SCALARS = 0x0022;  // exact values from zfield::prepare
inline constexpr uint16_t ZFH_SECTION_PRELOAD = 0x0023;  // one row per required physical register
inline constexpr uint16_t ZFH_SECTION_LEDGER = 0x0024;  // optional; exact uniform per-cause ledger

inline constexpr int ZFH_SECTION_KIND_COUNT = 16;

// -------------------------------------------- enumerations ---

// header.object_kind (directive 11.1)
enum ZfhObjectKind : uint8_t {
  ZFH_OBJECT_KIND_PROGRAM = 0,
  ZFH_OBJECT_KIND_ASSOCIATION = 1,
};

// PROGRAM_META.execution_form (directive 11.3)
enum ZfhExecutionForm : uint8_t {
  ZFH_EXECUTION_FORM_CANONICAL = 0,
  ZFH_EXECUTION_FORM_PREPARED_REGISTER = 1,
  // requires a checked logical plan with no varying instructions, all
  // output sources valid prepared scalars, and ZERO physical uops. An empty
  // arbitrary physical program is NOT this optimisation.
  ZFH_EXECUTION_FORM_UNIFORM_ONLY = 2,
};

// INPUT_MAP.source_class (directive 11.3)
enum ZfhSourceClass : uint8_t {
  ZFH_SOURCE_CLASS_VARYING_INPUT = 0,
  ZFH_SOURCE_CLASS_UNIFORM_INPUT = 1,
  // only an actually unused input may carry this; the declared value still
  // exists in the association record for validation and capture
  ZFH_SOURCE_CLASS_UNUSED_PROVEN = 2,
};

// OUTPUT_MAP.source_kind (directive 11.3)
enum ZfhSourceKind : uint8_t {
  // source_index is a physical register; a granted write to it updates
  // export_value[j] and sets seen[j]
  ZFH_SOURCE_KIND_VECTOR_REG = 0,
  // source_index is a prepared-scalar index; seeded at point start, so it
  // has NO window position by construction and the export must not wait for
  // a vector write that will never come
  ZFH_SOURCE_KIND_PREPARED_SCALAR = 1,
};

// -------------------------------------------------- flags ---

// OUTPUT_MAP.flags bit 0. Must agree with PROGRAM_META.required_mask.
inline constexpr uint32_t ZFH_OUTPUT_FLAG_REQUIRED = 0x1;
// INIT_PROOF.flags bit 0; set iff execution_form == CANONICAL.
inline constexpr uint32_t ZFH_INITPROOF_FLAG_CANONICAL = 0x1;
// INIT_PROOF.flags bit 1; set iff execution_form is PREPARED_REGISTER or a
// checked UNIFORM_ONLY.
inline constexpr uint32_t ZFH_INITPROOF_FLAG_PREPARED = 0x2;

// ==============================================================
// THE TWO MASKS
// ==============================================================
//
// These are distinct CLASSES, not typedefs, and neither converts
// to the other or to its underlying integer implicitly. That is
// deliberate: a typedef would make the confusion a wrong VALUE,
// and the whole point is to make it a compile ERROR.

inline constexpr int ZFH_WINDOW_MASK_BITS = 7;
// CONTIGUOUS CAPTURE-WINDOW POSITION. Bit k = physical register out_base +
// k was written. Width must equal the composed OUT_LANES; the console
// composes 7. Lives in the loader header word at [32 +: OUT_LANES] (R101,
// zhao_field_host.sv:585,1074).
class WindowMask {
 public:
  using rep = uint8_t;
  constexpr WindowMask() = default;
  // EXPLICIT on purpose: a bare integer does not silently
  // become a mask of either numbering.
  constexpr explicit WindowMask(rep bits) : bits_(bits) {}
  constexpr rep bits() const { return bits_; }
  constexpr bool empty() const { return bits_ == 0; }
  constexpr bool test(int i) const {
    return ((bits_ >> i) & 1u) != 0u;
  }
  constexpr bool covers(WindowMask other) const {
    return (bits_ & other.bits_) == other.bits_;
  }
  friend constexpr bool operator==(WindowMask a, WindowMask b) {
    return a.bits_ == b.bits_;
  }
  friend constexpr bool operator!=(WindowMask a, WindowMask b) {
    return a.bits_ != b.bits_;
  }
  static constexpr int kBits = ZFH_WINDOW_MASK_BITS;

 private:
  rep bits_ = 0;
};

inline constexpr int ZFH_REQUIRED_MASK_BITS = 8;
// CANONICAL OUTPUT ORDINAL. Bit j = canonical output j is declared.
// Meaningful width is the profile's output count; carried in a u8. Lives in
// PROGRAM_META.required_mask.
class RequiredMask {
 public:
  using rep = uint8_t;
  constexpr RequiredMask() = default;
  // EXPLICIT on purpose: a bare integer does not silently
  // become a mask of either numbering.
  constexpr explicit RequiredMask(rep bits) : bits_(bits) {}
  constexpr rep bits() const { return bits_; }
  constexpr bool empty() const { return bits_ == 0; }
  constexpr bool test(int i) const {
    return ((bits_ >> i) & 1u) != 0u;
  }
  constexpr bool covers(RequiredMask other) const {
    return (bits_ & other.bits_) == other.bits_;
  }
  friend constexpr bool operator==(RequiredMask a, RequiredMask b) {
    return a.bits_ == b.bits_;
  }
  friend constexpr bool operator!=(RequiredMask a, RequiredMask b) {
    return a.bits_ != b.bits_;
  }
  static constexpr int kBits = ZFH_REQUIRED_MASK_BITS;

 private:
  rep bits_ = 0;
};

// The separation, asserted rather than asserted-in-prose. Spelt
// out rather than using std::is_same_v so this header needs no
// <type_traits> and compiles on every toolchain the tree uses.
template <class A, class B>
struct ZfhSameType {
  static constexpr bool value = false;
};
template <class A>
struct ZfhSameType<A, A> {
  static constexpr bool value = true;
};
static_assert(!ZfhSameType<WindowMask, RequiredMask>::value,
              "the two masks collapsed into one type");

// ------------------------------------------------- records ---

// Common 64-byte image header (directive 11.1). Present at offset 0 of
// every ZFH2 image, PROGRAM and ASSOCIATION alike.
struct ZfhHeader {
  uint8_t magic[4];  // ASCII 'ZFH2' = 5A 46 48 32
  uint16_t image_version;  // = 1 in this revision
  uint8_t object_kind;  // 0 PROGRAM, 1 ASSOCIATION
  uint8_t flags;  // zero in this revision
  uint32_t total_bytes;  // positive, 64-byte aligned
  uint32_t body_crc32c;  // whole image with bytes 12..15 zero
  uint16_t host_protocol_version;  // = 2
  uint16_t reserved_18;  // reserved, must be zero
  uint32_t canonical_program_hash;
  uint32_t canonical_program_handle32;
  uint32_t resource_epoch;
  uint32_t logical_fplan_abi_version;
  uint32_t execution_fabric_version;
  uint32_t source_id32;
  uint16_t section_count;
  uint16_t header_bytes;  // = 64
  uint32_t parent_binding_handle;  // zero for an unbound PROGRAM install
  uint32_t canonical_full_image_crc32c;  // all serialized .zprog bytes, including that file's own embedded CRC field; never the sole dedup key
  uint32_t object_serial;  // software identity; hardware also assigns a generation
  uint32_t reserved_60;  // reserved, must be zero
};
inline constexpr uint32_t ZFH_HDR_BYTES = 64;
static_assert(sizeof(ZfhHeader) == 64,
              "ZfhHeader must be exactly 64 bytes");
inline constexpr uint32_t ZFH_HDR_OFF_MAGIC = 0;
inline constexpr uint32_t ZFH_HDR_LEN_MAGIC = 4;
static_assert(offsetof(ZfhHeader, magic) == 0,
              "ZfhHeader.magic moved off offset 0");
static_assert(sizeof(ZfhHeader::magic) == 4,
              "ZfhHeader.magic changed width");
inline constexpr uint32_t ZFH_HDR_OFF_IMAGE_VERSION = 4;
inline constexpr uint32_t ZFH_HDR_LEN_IMAGE_VERSION = 2;
static_assert(offsetof(ZfhHeader, image_version) == 4,
              "ZfhHeader.image_version moved off offset 4");
static_assert(sizeof(ZfhHeader::image_version) == 2,
              "ZfhHeader.image_version changed width");
inline constexpr uint32_t ZFH_HDR_OFF_OBJECT_KIND = 6;
inline constexpr uint32_t ZFH_HDR_LEN_OBJECT_KIND = 1;
static_assert(offsetof(ZfhHeader, object_kind) == 6,
              "ZfhHeader.object_kind moved off offset 6");
static_assert(sizeof(ZfhHeader::object_kind) == 1,
              "ZfhHeader.object_kind changed width");
inline constexpr uint32_t ZFH_HDR_OFF_FLAGS = 7;
inline constexpr uint32_t ZFH_HDR_LEN_FLAGS = 1;
static_assert(offsetof(ZfhHeader, flags) == 7,
              "ZfhHeader.flags moved off offset 7");
static_assert(sizeof(ZfhHeader::flags) == 1,
              "ZfhHeader.flags changed width");
inline constexpr uint32_t ZFH_HDR_OFF_TOTAL_BYTES = 8;
inline constexpr uint32_t ZFH_HDR_LEN_TOTAL_BYTES = 4;
static_assert(offsetof(ZfhHeader, total_bytes) == 8,
              "ZfhHeader.total_bytes moved off offset 8");
static_assert(sizeof(ZfhHeader::total_bytes) == 4,
              "ZfhHeader.total_bytes changed width");
inline constexpr uint32_t ZFH_HDR_OFF_BODY_CRC32C = 12;
inline constexpr uint32_t ZFH_HDR_LEN_BODY_CRC32C = 4;
static_assert(offsetof(ZfhHeader, body_crc32c) == 12,
              "ZfhHeader.body_crc32c moved off offset 12");
static_assert(sizeof(ZfhHeader::body_crc32c) == 4,
              "ZfhHeader.body_crc32c changed width");
inline constexpr uint32_t ZFH_HDR_OFF_HOST_PROTOCOL_VERSION = 16;
inline constexpr uint32_t ZFH_HDR_LEN_HOST_PROTOCOL_VERSION = 2;
static_assert(offsetof(ZfhHeader, host_protocol_version) == 16,
              "ZfhHeader.host_protocol_version moved off offset 16");
static_assert(sizeof(ZfhHeader::host_protocol_version) == 2,
              "ZfhHeader.host_protocol_version changed width");
inline constexpr uint32_t ZFH_HDR_OFF_RESERVED_18 = 18;
inline constexpr uint32_t ZFH_HDR_LEN_RESERVED_18 = 2;
static_assert(offsetof(ZfhHeader, reserved_18) == 18,
              "ZfhHeader.reserved_18 moved off offset 18");
static_assert(sizeof(ZfhHeader::reserved_18) == 2,
              "ZfhHeader.reserved_18 changed width");
inline constexpr uint32_t ZFH_HDR_OFF_CANONICAL_PROGRAM_HASH = 20;
inline constexpr uint32_t ZFH_HDR_LEN_CANONICAL_PROGRAM_HASH = 4;
static_assert(offsetof(ZfhHeader, canonical_program_hash) == 20,
              "ZfhHeader.canonical_program_hash moved off offset 20");
static_assert(sizeof(ZfhHeader::canonical_program_hash) == 4,
              "ZfhHeader.canonical_program_hash changed width");
inline constexpr uint32_t ZFH_HDR_OFF_CANONICAL_PROGRAM_HANDLE32 = 24;
inline constexpr uint32_t ZFH_HDR_LEN_CANONICAL_PROGRAM_HANDLE32 = 4;
static_assert(offsetof(ZfhHeader, canonical_program_handle32) == 24,
              "ZfhHeader.canonical_program_handle32 moved off offset 24");
static_assert(sizeof(ZfhHeader::canonical_program_handle32) == 4,
              "ZfhHeader.canonical_program_handle32 changed width");
inline constexpr uint32_t ZFH_HDR_OFF_RESOURCE_EPOCH = 28;
inline constexpr uint32_t ZFH_HDR_LEN_RESOURCE_EPOCH = 4;
static_assert(offsetof(ZfhHeader, resource_epoch) == 28,
              "ZfhHeader.resource_epoch moved off offset 28");
static_assert(sizeof(ZfhHeader::resource_epoch) == 4,
              "ZfhHeader.resource_epoch changed width");
inline constexpr uint32_t ZFH_HDR_OFF_LOGICAL_FPLAN_ABI_VERSION = 32;
inline constexpr uint32_t ZFH_HDR_LEN_LOGICAL_FPLAN_ABI_VERSION = 4;
static_assert(offsetof(ZfhHeader, logical_fplan_abi_version) == 32,
              "ZfhHeader.logical_fplan_abi_version moved off offset 32");
static_assert(sizeof(ZfhHeader::logical_fplan_abi_version) == 4,
              "ZfhHeader.logical_fplan_abi_version changed width");
inline constexpr uint32_t ZFH_HDR_OFF_EXECUTION_FABRIC_VERSION = 36;
inline constexpr uint32_t ZFH_HDR_LEN_EXECUTION_FABRIC_VERSION = 4;
static_assert(offsetof(ZfhHeader, execution_fabric_version) == 36,
              "ZfhHeader.execution_fabric_version moved off offset 36");
static_assert(sizeof(ZfhHeader::execution_fabric_version) == 4,
              "ZfhHeader.execution_fabric_version changed width");
inline constexpr uint32_t ZFH_HDR_OFF_SOURCE_ID32 = 40;
inline constexpr uint32_t ZFH_HDR_LEN_SOURCE_ID32 = 4;
static_assert(offsetof(ZfhHeader, source_id32) == 40,
              "ZfhHeader.source_id32 moved off offset 40");
static_assert(sizeof(ZfhHeader::source_id32) == 4,
              "ZfhHeader.source_id32 changed width");
inline constexpr uint32_t ZFH_HDR_OFF_SECTION_COUNT = 44;
inline constexpr uint32_t ZFH_HDR_LEN_SECTION_COUNT = 2;
static_assert(offsetof(ZfhHeader, section_count) == 44,
              "ZfhHeader.section_count moved off offset 44");
static_assert(sizeof(ZfhHeader::section_count) == 2,
              "ZfhHeader.section_count changed width");
inline constexpr uint32_t ZFH_HDR_OFF_HEADER_BYTES = 46;
inline constexpr uint32_t ZFH_HDR_LEN_HEADER_BYTES = 2;
static_assert(offsetof(ZfhHeader, header_bytes) == 46,
              "ZfhHeader.header_bytes moved off offset 46");
static_assert(sizeof(ZfhHeader::header_bytes) == 2,
              "ZfhHeader.header_bytes changed width");
inline constexpr uint32_t ZFH_HDR_OFF_PARENT_BINDING_HANDLE = 48;
inline constexpr uint32_t ZFH_HDR_LEN_PARENT_BINDING_HANDLE = 4;
static_assert(offsetof(ZfhHeader, parent_binding_handle) == 48,
              "ZfhHeader.parent_binding_handle moved off offset 48");
static_assert(sizeof(ZfhHeader::parent_binding_handle) == 4,
              "ZfhHeader.parent_binding_handle changed width");
inline constexpr uint32_t ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C = 52;
inline constexpr uint32_t ZFH_HDR_LEN_CANONICAL_FULL_IMAGE_CRC32C = 4;
static_assert(offsetof(ZfhHeader, canonical_full_image_crc32c) == 52,
              "ZfhHeader.canonical_full_image_crc32c moved off offset 52");
static_assert(sizeof(ZfhHeader::canonical_full_image_crc32c) == 4,
              "ZfhHeader.canonical_full_image_crc32c changed width");
inline constexpr uint32_t ZFH_HDR_OFF_OBJECT_SERIAL = 56;
inline constexpr uint32_t ZFH_HDR_LEN_OBJECT_SERIAL = 4;
static_assert(offsetof(ZfhHeader, object_serial) == 56,
              "ZfhHeader.object_serial moved off offset 56");
static_assert(sizeof(ZfhHeader::object_serial) == 4,
              "ZfhHeader.object_serial changed width");
inline constexpr uint32_t ZFH_HDR_OFF_RESERVED_60 = 60;
inline constexpr uint32_t ZFH_HDR_LEN_RESERVED_60 = 4;
static_assert(offsetof(ZfhHeader, reserved_60) == 60,
              "ZfhHeader.reserved_60 moved off offset 60");
static_assert(sizeof(ZfhHeader::reserved_60) == 4,
              "ZfhHeader.reserved_60 changed width");

// One section-directory entry (directive 11.2). section_count of these
// follow the header immediately, sorted by kind, then zero padding to a
// 64-byte boundary.
struct ZfhSectionEntry {
  uint16_t kind;  // a ZFH_SECTION_* value
  uint16_t element_bytes;
  uint32_t element_count;
  uint32_t offset;  // 64-byte aligned body offset
  uint32_t byte_length;  // checked element_bytes * element_count, widened before multiply
};
inline constexpr uint32_t ZFH_SEC_BYTES = 16;
static_assert(sizeof(ZfhSectionEntry) == 16,
              "ZfhSectionEntry must be exactly 16 bytes");
inline constexpr uint32_t ZFH_SEC_OFF_KIND = 0;
inline constexpr uint32_t ZFH_SEC_LEN_KIND = 2;
static_assert(offsetof(ZfhSectionEntry, kind) == 0,
              "ZfhSectionEntry.kind moved off offset 0");
static_assert(sizeof(ZfhSectionEntry::kind) == 2,
              "ZfhSectionEntry.kind changed width");
inline constexpr uint32_t ZFH_SEC_OFF_ELEMENT_BYTES = 2;
inline constexpr uint32_t ZFH_SEC_LEN_ELEMENT_BYTES = 2;
static_assert(offsetof(ZfhSectionEntry, element_bytes) == 2,
              "ZfhSectionEntry.element_bytes moved off offset 2");
static_assert(sizeof(ZfhSectionEntry::element_bytes) == 2,
              "ZfhSectionEntry.element_bytes changed width");
inline constexpr uint32_t ZFH_SEC_OFF_ELEMENT_COUNT = 4;
inline constexpr uint32_t ZFH_SEC_LEN_ELEMENT_COUNT = 4;
static_assert(offsetof(ZfhSectionEntry, element_count) == 4,
              "ZfhSectionEntry.element_count moved off offset 4");
static_assert(sizeof(ZfhSectionEntry::element_count) == 4,
              "ZfhSectionEntry.element_count changed width");
inline constexpr uint32_t ZFH_SEC_OFF_OFFSET = 8;
inline constexpr uint32_t ZFH_SEC_LEN_OFFSET = 4;
static_assert(offsetof(ZfhSectionEntry, offset) == 8,
              "ZfhSectionEntry.offset moved off offset 8");
static_assert(sizeof(ZfhSectionEntry::offset) == 4,
              "ZfhSectionEntry.offset changed width");
inline constexpr uint32_t ZFH_SEC_OFF_BYTE_LENGTH = 12;
inline constexpr uint32_t ZFH_SEC_LEN_BYTE_LENGTH = 4;
static_assert(offsetof(ZfhSectionEntry, byte_length) == 12,
              "ZfhSectionEntry.byte_length moved off offset 12");
static_assert(sizeof(ZfhSectionEntry::byte_length) == 4,
              "ZfhSectionEntry.byte_length changed width");

// PROGRAM_META, exactly one 64-byte record (directive 11.3). The final
// reserved words are not capacity for undocumented policy.
struct ZfhProgramMeta {
  uint8_t profile;  // 0 earth, 1 warp, 2 flow, 3 formation, 4 stamp
  uint8_t binding_signature;
  uint8_t execution_form;  // 0 CANONICAL, 1 PREPARED_REGISTER, 2 UNIFORM_ONLY
  uint8_t flags;
  uint8_t input_count;
  uint8_t output_count;  // 1..7 at a production binding
  uint8_t required_mask;  // ORDINAL-INDEXED. Read the two-mask chapter before wiring this. Zero is a load-time refusal for a strict import.
  uint8_t table_count;  // 0..4
  uint16_t canonical_instruction_count;
  uint16_t physical_uop_count;  // includes the terminating END for the non-uniform forms
  uint16_t physical_register_count;
  uint16_t prepared_scalar_count;
  uint32_t varying_input_mask;  // CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits; NOT a physical RF register mask
  uint32_t point_preload_mask;  // CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits
  uint32_t code_image_crc;
  uint32_t maps_crc;
  uint32_t logical_plan_identity;
  uint32_t reserved[7];  // reserved, must all be zero
};
inline constexpr uint32_t ZFH_PM_BYTES = 64;
static_assert(sizeof(ZfhProgramMeta) == 64,
              "ZfhProgramMeta must be exactly 64 bytes");
inline constexpr uint32_t ZFH_PM_OFF_PROFILE = 0;
inline constexpr uint32_t ZFH_PM_LEN_PROFILE = 1;
static_assert(offsetof(ZfhProgramMeta, profile) == 0,
              "ZfhProgramMeta.profile moved off offset 0");
static_assert(sizeof(ZfhProgramMeta::profile) == 1,
              "ZfhProgramMeta.profile changed width");
inline constexpr uint32_t ZFH_PM_OFF_BINDING_SIGNATURE = 1;
inline constexpr uint32_t ZFH_PM_LEN_BINDING_SIGNATURE = 1;
static_assert(offsetof(ZfhProgramMeta, binding_signature) == 1,
              "ZfhProgramMeta.binding_signature moved off offset 1");
static_assert(sizeof(ZfhProgramMeta::binding_signature) == 1,
              "ZfhProgramMeta.binding_signature changed width");
inline constexpr uint32_t ZFH_PM_OFF_EXECUTION_FORM = 2;
inline constexpr uint32_t ZFH_PM_LEN_EXECUTION_FORM = 1;
static_assert(offsetof(ZfhProgramMeta, execution_form) == 2,
              "ZfhProgramMeta.execution_form moved off offset 2");
static_assert(sizeof(ZfhProgramMeta::execution_form) == 1,
              "ZfhProgramMeta.execution_form changed width");
inline constexpr uint32_t ZFH_PM_OFF_FLAGS = 3;
inline constexpr uint32_t ZFH_PM_LEN_FLAGS = 1;
static_assert(offsetof(ZfhProgramMeta, flags) == 3,
              "ZfhProgramMeta.flags moved off offset 3");
static_assert(sizeof(ZfhProgramMeta::flags) == 1,
              "ZfhProgramMeta.flags changed width");
inline constexpr uint32_t ZFH_PM_OFF_INPUT_COUNT = 4;
inline constexpr uint32_t ZFH_PM_LEN_INPUT_COUNT = 1;
static_assert(offsetof(ZfhProgramMeta, input_count) == 4,
              "ZfhProgramMeta.input_count moved off offset 4");
static_assert(sizeof(ZfhProgramMeta::input_count) == 1,
              "ZfhProgramMeta.input_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_OUTPUT_COUNT = 5;
inline constexpr uint32_t ZFH_PM_LEN_OUTPUT_COUNT = 1;
static_assert(offsetof(ZfhProgramMeta, output_count) == 5,
              "ZfhProgramMeta.output_count moved off offset 5");
static_assert(sizeof(ZfhProgramMeta::output_count) == 1,
              "ZfhProgramMeta.output_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_REQUIRED_MASK = 6;
inline constexpr uint32_t ZFH_PM_LEN_REQUIRED_MASK = 1;
static_assert(offsetof(ZfhProgramMeta, required_mask) == 6,
              "ZfhProgramMeta.required_mask moved off offset 6");
static_assert(sizeof(ZfhProgramMeta::required_mask) == 1,
              "ZfhProgramMeta.required_mask changed width");
inline constexpr uint32_t ZFH_PM_OFF_TABLE_COUNT = 7;
inline constexpr uint32_t ZFH_PM_LEN_TABLE_COUNT = 1;
static_assert(offsetof(ZfhProgramMeta, table_count) == 7,
              "ZfhProgramMeta.table_count moved off offset 7");
static_assert(sizeof(ZfhProgramMeta::table_count) == 1,
              "ZfhProgramMeta.table_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_CANONICAL_INSTRUCTION_COUNT = 8;
inline constexpr uint32_t ZFH_PM_LEN_CANONICAL_INSTRUCTION_COUNT = 2;
static_assert(offsetof(ZfhProgramMeta, canonical_instruction_count) == 8,
              "ZfhProgramMeta.canonical_instruction_count moved off offset 8");
static_assert(sizeof(ZfhProgramMeta::canonical_instruction_count) == 2,
              "ZfhProgramMeta.canonical_instruction_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_PHYSICAL_UOP_COUNT = 10;
inline constexpr uint32_t ZFH_PM_LEN_PHYSICAL_UOP_COUNT = 2;
static_assert(offsetof(ZfhProgramMeta, physical_uop_count) == 10,
              "ZfhProgramMeta.physical_uop_count moved off offset 10");
static_assert(sizeof(ZfhProgramMeta::physical_uop_count) == 2,
              "ZfhProgramMeta.physical_uop_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_PHYSICAL_REGISTER_COUNT = 12;
inline constexpr uint32_t ZFH_PM_LEN_PHYSICAL_REGISTER_COUNT = 2;
static_assert(offsetof(ZfhProgramMeta, physical_register_count) == 12,
              "ZfhProgramMeta.physical_register_count moved off offset 12");
static_assert(sizeof(ZfhProgramMeta::physical_register_count) == 2,
              "ZfhProgramMeta.physical_register_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_PREPARED_SCALAR_COUNT = 14;
inline constexpr uint32_t ZFH_PM_LEN_PREPARED_SCALAR_COUNT = 2;
static_assert(offsetof(ZfhProgramMeta, prepared_scalar_count) == 14,
              "ZfhProgramMeta.prepared_scalar_count moved off offset 14");
static_assert(sizeof(ZfhProgramMeta::prepared_scalar_count) == 2,
              "ZfhProgramMeta.prepared_scalar_count changed width");
inline constexpr uint32_t ZFH_PM_OFF_VARYING_INPUT_MASK = 16;
inline constexpr uint32_t ZFH_PM_LEN_VARYING_INPUT_MASK = 4;
static_assert(offsetof(ZfhProgramMeta, varying_input_mask) == 16,
              "ZfhProgramMeta.varying_input_mask moved off offset 16");
static_assert(sizeof(ZfhProgramMeta::varying_input_mask) == 4,
              "ZfhProgramMeta.varying_input_mask changed width");
inline constexpr uint32_t ZFH_PM_OFF_POINT_PRELOAD_MASK = 20;
inline constexpr uint32_t ZFH_PM_LEN_POINT_PRELOAD_MASK = 4;
static_assert(offsetof(ZfhProgramMeta, point_preload_mask) == 20,
              "ZfhProgramMeta.point_preload_mask moved off offset 20");
static_assert(sizeof(ZfhProgramMeta::point_preload_mask) == 4,
              "ZfhProgramMeta.point_preload_mask changed width");
inline constexpr uint32_t ZFH_PM_OFF_CODE_IMAGE_CRC = 24;
inline constexpr uint32_t ZFH_PM_LEN_CODE_IMAGE_CRC = 4;
static_assert(offsetof(ZfhProgramMeta, code_image_crc) == 24,
              "ZfhProgramMeta.code_image_crc moved off offset 24");
static_assert(sizeof(ZfhProgramMeta::code_image_crc) == 4,
              "ZfhProgramMeta.code_image_crc changed width");
inline constexpr uint32_t ZFH_PM_OFF_MAPS_CRC = 28;
inline constexpr uint32_t ZFH_PM_LEN_MAPS_CRC = 4;
static_assert(offsetof(ZfhProgramMeta, maps_crc) == 28,
              "ZfhProgramMeta.maps_crc moved off offset 28");
static_assert(sizeof(ZfhProgramMeta::maps_crc) == 4,
              "ZfhProgramMeta.maps_crc changed width");
inline constexpr uint32_t ZFH_PM_OFF_LOGICAL_PLAN_IDENTITY = 32;
inline constexpr uint32_t ZFH_PM_LEN_LOGICAL_PLAN_IDENTITY = 4;
static_assert(offsetof(ZfhProgramMeta, logical_plan_identity) == 32,
              "ZfhProgramMeta.logical_plan_identity moved off offset 32");
static_assert(sizeof(ZfhProgramMeta::logical_plan_identity) == 4,
              "ZfhProgramMeta.logical_plan_identity changed width");
inline constexpr uint32_t ZFH_PM_OFF_RESERVED = 36;
inline constexpr uint32_t ZFH_PM_LEN_RESERVED = 28;
static_assert(offsetof(ZfhProgramMeta, reserved) == 36,
              "ZfhProgramMeta.reserved moved off offset 36");
static_assert(sizeof(ZfhProgramMeta::reserved) == 28,
              "ZfhProgramMeta.reserved changed width");

// INPUT_MAP, one 8-byte row per canonical input ordinal (directive 11.3).
// Validate as a complete ordinal set.
struct ZfhInputMapRow {
  uint8_t ordinal;
  uint8_t lane_type;  // code from the canonical Field I/O-map registry
  uint8_t source_class;  // 0 VARYING_INPUT, 1 UNIFORM_INPUT, 2 UNUSED_PROVEN
  uint8_t flags;
  uint16_t physical_register;  // ZFH_ADDR_UNUSED when not applicable, never an implicitly valid register zero
  uint16_t prepared_slot;  // ZFH_ADDR_UNUSED when unused
};
inline constexpr uint32_t ZFH_IN_BYTES = 8;
static_assert(sizeof(ZfhInputMapRow) == 8,
              "ZfhInputMapRow must be exactly 8 bytes");
inline constexpr uint32_t ZFH_IN_OFF_ORDINAL = 0;
inline constexpr uint32_t ZFH_IN_LEN_ORDINAL = 1;
static_assert(offsetof(ZfhInputMapRow, ordinal) == 0,
              "ZfhInputMapRow.ordinal moved off offset 0");
static_assert(sizeof(ZfhInputMapRow::ordinal) == 1,
              "ZfhInputMapRow.ordinal changed width");
inline constexpr uint32_t ZFH_IN_OFF_LANE_TYPE = 1;
inline constexpr uint32_t ZFH_IN_LEN_LANE_TYPE = 1;
static_assert(offsetof(ZfhInputMapRow, lane_type) == 1,
              "ZfhInputMapRow.lane_type moved off offset 1");
static_assert(sizeof(ZfhInputMapRow::lane_type) == 1,
              "ZfhInputMapRow.lane_type changed width");
inline constexpr uint32_t ZFH_IN_OFF_SOURCE_CLASS = 2;
inline constexpr uint32_t ZFH_IN_LEN_SOURCE_CLASS = 1;
static_assert(offsetof(ZfhInputMapRow, source_class) == 2,
              "ZfhInputMapRow.source_class moved off offset 2");
static_assert(sizeof(ZfhInputMapRow::source_class) == 1,
              "ZfhInputMapRow.source_class changed width");
inline constexpr uint32_t ZFH_IN_OFF_FLAGS = 3;
inline constexpr uint32_t ZFH_IN_LEN_FLAGS = 1;
static_assert(offsetof(ZfhInputMapRow, flags) == 3,
              "ZfhInputMapRow.flags moved off offset 3");
static_assert(sizeof(ZfhInputMapRow::flags) == 1,
              "ZfhInputMapRow.flags changed width");
inline constexpr uint32_t ZFH_IN_OFF_PHYSICAL_REGISTER = 4;
inline constexpr uint32_t ZFH_IN_LEN_PHYSICAL_REGISTER = 2;
static_assert(offsetof(ZfhInputMapRow, physical_register) == 4,
              "ZfhInputMapRow.physical_register moved off offset 4");
static_assert(sizeof(ZfhInputMapRow::physical_register) == 2,
              "ZfhInputMapRow.physical_register changed width");
inline constexpr uint32_t ZFH_IN_OFF_PREPARED_SLOT = 6;
inline constexpr uint32_t ZFH_IN_LEN_PREPARED_SLOT = 2;
static_assert(offsetof(ZfhInputMapRow, prepared_slot) == 6,
              "ZfhInputMapRow.prepared_slot moved off offset 6");
static_assert(sizeof(ZfhInputMapRow::prepared_slot) == 2,
              "ZfhInputMapRow.prepared_slot changed width");

// OUTPUT_MAP, one 8-byte row per canonical output ORDINAL (directive 11.3).
// THIS RECORD IS THE ORDINAL-TO-WINDOW TRANSLATION. Reserved or unknown
// source_kind values refuse.
struct ZfhOutputMapRow {
  uint8_t ordinal;  // canonical output ordinal j
  uint8_t lane_type;
  uint8_t source_kind;  // 0 VECTOR_REG, 1 PREPARED_SCALAR
  uint8_t flags;  // bit0 REQUIRED; no undocumented meanings
  uint16_t source_index;  // physical register for VECTOR_REG, prepared-scalar index for PREPARED_SCALAR
  uint16_t reserved;  // reserved, must be zero
};
inline constexpr uint32_t ZFH_OUT_BYTES = 8;
static_assert(sizeof(ZfhOutputMapRow) == 8,
              "ZfhOutputMapRow must be exactly 8 bytes");
inline constexpr uint32_t ZFH_OUT_OFF_ORDINAL = 0;
inline constexpr uint32_t ZFH_OUT_LEN_ORDINAL = 1;
static_assert(offsetof(ZfhOutputMapRow, ordinal) == 0,
              "ZfhOutputMapRow.ordinal moved off offset 0");
static_assert(sizeof(ZfhOutputMapRow::ordinal) == 1,
              "ZfhOutputMapRow.ordinal changed width");
inline constexpr uint32_t ZFH_OUT_OFF_LANE_TYPE = 1;
inline constexpr uint32_t ZFH_OUT_LEN_LANE_TYPE = 1;
static_assert(offsetof(ZfhOutputMapRow, lane_type) == 1,
              "ZfhOutputMapRow.lane_type moved off offset 1");
static_assert(sizeof(ZfhOutputMapRow::lane_type) == 1,
              "ZfhOutputMapRow.lane_type changed width");
inline constexpr uint32_t ZFH_OUT_OFF_SOURCE_KIND = 2;
inline constexpr uint32_t ZFH_OUT_LEN_SOURCE_KIND = 1;
static_assert(offsetof(ZfhOutputMapRow, source_kind) == 2,
              "ZfhOutputMapRow.source_kind moved off offset 2");
static_assert(sizeof(ZfhOutputMapRow::source_kind) == 1,
              "ZfhOutputMapRow.source_kind changed width");
inline constexpr uint32_t ZFH_OUT_OFF_FLAGS = 3;
inline constexpr uint32_t ZFH_OUT_LEN_FLAGS = 1;
static_assert(offsetof(ZfhOutputMapRow, flags) == 3,
              "ZfhOutputMapRow.flags moved off offset 3");
static_assert(sizeof(ZfhOutputMapRow::flags) == 1,
              "ZfhOutputMapRow.flags changed width");
inline constexpr uint32_t ZFH_OUT_OFF_SOURCE_INDEX = 4;
inline constexpr uint32_t ZFH_OUT_LEN_SOURCE_INDEX = 2;
static_assert(offsetof(ZfhOutputMapRow, source_index) == 4,
              "ZfhOutputMapRow.source_index moved off offset 4");
static_assert(sizeof(ZfhOutputMapRow::source_index) == 2,
              "ZfhOutputMapRow.source_index changed width");
inline constexpr uint32_t ZFH_OUT_OFF_RESERVED = 6;
inline constexpr uint32_t ZFH_OUT_LEN_RESERVED = 2;
static_assert(offsetof(ZfhOutputMapRow, reserved) == 6,
              "ZfhOutputMapRow.reserved moved off offset 6");
static_assert(sizeof(ZfhOutputMapRow::reserved) == 2,
              "ZfhOutputMapRow.reserved changed width");

// INIT_PROOF, exactly one 64-byte record (directive 11.3). EVERY MASK HERE
// IS INDEXED BY PHYSICAL REGISTER 0..63 -- no u32 mask may silently discard
// registers 32..63.
struct ZfhInitProof {
  uint16_t proof_version;  // = 1
  uint16_t record_bytes;  // = 64
  uint32_t flags;  // bit0 canonical validation, bit1 prepared physical validation; exactly the one matching execution_form is set
  uint64_t association_register_mask;
  uint64_t point_register_mask;
  uint64_t immutable_register_mask;  // association-owned; must not overlap vector_write_mask
  uint64_t initial_defined_mask;  // must equal association_register_mask | point_register_mask
  uint64_t vector_write_mask;
  uint32_t code_image_crc;
  uint32_t maps_crc;
  uint16_t expected_association_preload_count;  // the mask POPULATION COUNT, not the highest register plus one
  uint16_t reserved_58;  // reserved, must be zero
  uint32_t reserved_60;  // reserved, must be zero
};
inline constexpr uint32_t ZFH_IP_BYTES = 64;
static_assert(sizeof(ZfhInitProof) == 64,
              "ZfhInitProof must be exactly 64 bytes");
inline constexpr uint32_t ZFH_IP_OFF_PROOF_VERSION = 0;
inline constexpr uint32_t ZFH_IP_LEN_PROOF_VERSION = 2;
static_assert(offsetof(ZfhInitProof, proof_version) == 0,
              "ZfhInitProof.proof_version moved off offset 0");
static_assert(sizeof(ZfhInitProof::proof_version) == 2,
              "ZfhInitProof.proof_version changed width");
inline constexpr uint32_t ZFH_IP_OFF_RECORD_BYTES = 2;
inline constexpr uint32_t ZFH_IP_LEN_RECORD_BYTES = 2;
static_assert(offsetof(ZfhInitProof, record_bytes) == 2,
              "ZfhInitProof.record_bytes moved off offset 2");
static_assert(sizeof(ZfhInitProof::record_bytes) == 2,
              "ZfhInitProof.record_bytes changed width");
inline constexpr uint32_t ZFH_IP_OFF_FLAGS = 4;
inline constexpr uint32_t ZFH_IP_LEN_FLAGS = 4;
static_assert(offsetof(ZfhInitProof, flags) == 4,
              "ZfhInitProof.flags moved off offset 4");
static_assert(sizeof(ZfhInitProof::flags) == 4,
              "ZfhInitProof.flags changed width");
inline constexpr uint32_t ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK = 8;
inline constexpr uint32_t ZFH_IP_LEN_ASSOCIATION_REGISTER_MASK = 8;
static_assert(offsetof(ZfhInitProof, association_register_mask) == 8,
              "ZfhInitProof.association_register_mask moved off offset 8");
static_assert(sizeof(ZfhInitProof::association_register_mask) == 8,
              "ZfhInitProof.association_register_mask changed width");
inline constexpr uint32_t ZFH_IP_OFF_POINT_REGISTER_MASK = 16;
inline constexpr uint32_t ZFH_IP_LEN_POINT_REGISTER_MASK = 8;
static_assert(offsetof(ZfhInitProof, point_register_mask) == 16,
              "ZfhInitProof.point_register_mask moved off offset 16");
static_assert(sizeof(ZfhInitProof::point_register_mask) == 8,
              "ZfhInitProof.point_register_mask changed width");
inline constexpr uint32_t ZFH_IP_OFF_IMMUTABLE_REGISTER_MASK = 24;
inline constexpr uint32_t ZFH_IP_LEN_IMMUTABLE_REGISTER_MASK = 8;
static_assert(offsetof(ZfhInitProof, immutable_register_mask) == 24,
              "ZfhInitProof.immutable_register_mask moved off offset 24");
static_assert(sizeof(ZfhInitProof::immutable_register_mask) == 8,
              "ZfhInitProof.immutable_register_mask changed width");
inline constexpr uint32_t ZFH_IP_OFF_INITIAL_DEFINED_MASK = 32;
inline constexpr uint32_t ZFH_IP_LEN_INITIAL_DEFINED_MASK = 8;
static_assert(offsetof(ZfhInitProof, initial_defined_mask) == 32,
              "ZfhInitProof.initial_defined_mask moved off offset 32");
static_assert(sizeof(ZfhInitProof::initial_defined_mask) == 8,
              "ZfhInitProof.initial_defined_mask changed width");
inline constexpr uint32_t ZFH_IP_OFF_VECTOR_WRITE_MASK = 40;
inline constexpr uint32_t ZFH_IP_LEN_VECTOR_WRITE_MASK = 8;
static_assert(offsetof(ZfhInitProof, vector_write_mask) == 40,
              "ZfhInitProof.vector_write_mask moved off offset 40");
static_assert(sizeof(ZfhInitProof::vector_write_mask) == 8,
              "ZfhInitProof.vector_write_mask changed width");
inline constexpr uint32_t ZFH_IP_OFF_CODE_IMAGE_CRC = 48;
inline constexpr uint32_t ZFH_IP_LEN_CODE_IMAGE_CRC = 4;
static_assert(offsetof(ZfhInitProof, code_image_crc) == 48,
              "ZfhInitProof.code_image_crc moved off offset 48");
static_assert(sizeof(ZfhInitProof::code_image_crc) == 4,
              "ZfhInitProof.code_image_crc changed width");
inline constexpr uint32_t ZFH_IP_OFF_MAPS_CRC = 52;
inline constexpr uint32_t ZFH_IP_LEN_MAPS_CRC = 4;
static_assert(offsetof(ZfhInitProof, maps_crc) == 52,
              "ZfhInitProof.maps_crc moved off offset 52");
static_assert(sizeof(ZfhInitProof::maps_crc) == 4,
              "ZfhInitProof.maps_crc changed width");
inline constexpr uint32_t ZFH_IP_OFF_EXPECTED_ASSOCIATION_PRELOAD_COUNT = 56;
inline constexpr uint32_t ZFH_IP_LEN_EXPECTED_ASSOCIATION_PRELOAD_COUNT = 2;
static_assert(offsetof(ZfhInitProof, expected_association_preload_count) == 56,
              "ZfhInitProof.expected_association_preload_count moved off offset 56");
static_assert(sizeof(ZfhInitProof::expected_association_preload_count) == 2,
              "ZfhInitProof.expected_association_preload_count changed width");
inline constexpr uint32_t ZFH_IP_OFF_RESERVED_58 = 58;
inline constexpr uint32_t ZFH_IP_LEN_RESERVED_58 = 2;
static_assert(offsetof(ZfhInitProof, reserved_58) == 58,
              "ZfhInitProof.reserved_58 moved off offset 58");
static_assert(sizeof(ZfhInitProof::reserved_58) == 2,
              "ZfhInitProof.reserved_58 changed width");
inline constexpr uint32_t ZFH_IP_OFF_RESERVED_60 = 60;
inline constexpr uint32_t ZFH_IP_LEN_RESERVED_60 = 4;
static_assert(offsetof(ZfhInitProof, reserved_60) == 60,
              "ZfhInitProof.reserved_60 moved off offset 60");
static_assert(sizeof(ZfhInitProof::reserved_60) == 4,
              "ZfhInitProof.reserved_60 changed width");

// ASSOCIATION_META, exactly one 64-byte record (directive 11.4). The
// varying and uniform input masks are disjoint and together cover every
// required used input; unused inputs are declared, not omitted.
struct ZfhAssociationMeta {
  uint32_t association_serial;
  uint32_t frame_id;
  uint8_t client_id;
  uint8_t profile;
  uint8_t execution_class;
  uint8_t flags;
  uint32_t expected_points;  // useful application points, not physical padding lanes
  uint32_t varying_input_mask;  // canonical input ordinals
  uint32_t uniform_input_mask;  // canonical input ordinals
  uint32_t preparation_serial;
  uint32_t max_group_quantum;
  uint32_t source_id;
  uint32_t numeric_uniform_status;  // only generated defined bits are used
  uint64_t client_binding_cookie;
  uint32_t reserved[4];  // reserved, must all be zero
};
inline constexpr uint32_t ZFH_AM_BYTES = 64;
static_assert(sizeof(ZfhAssociationMeta) == 64,
              "ZfhAssociationMeta must be exactly 64 bytes");
inline constexpr uint32_t ZFH_AM_OFF_ASSOCIATION_SERIAL = 0;
inline constexpr uint32_t ZFH_AM_LEN_ASSOCIATION_SERIAL = 4;
static_assert(offsetof(ZfhAssociationMeta, association_serial) == 0,
              "ZfhAssociationMeta.association_serial moved off offset 0");
static_assert(sizeof(ZfhAssociationMeta::association_serial) == 4,
              "ZfhAssociationMeta.association_serial changed width");
inline constexpr uint32_t ZFH_AM_OFF_FRAME_ID = 4;
inline constexpr uint32_t ZFH_AM_LEN_FRAME_ID = 4;
static_assert(offsetof(ZfhAssociationMeta, frame_id) == 4,
              "ZfhAssociationMeta.frame_id moved off offset 4");
static_assert(sizeof(ZfhAssociationMeta::frame_id) == 4,
              "ZfhAssociationMeta.frame_id changed width");
inline constexpr uint32_t ZFH_AM_OFF_CLIENT_ID = 8;
inline constexpr uint32_t ZFH_AM_LEN_CLIENT_ID = 1;
static_assert(offsetof(ZfhAssociationMeta, client_id) == 8,
              "ZfhAssociationMeta.client_id moved off offset 8");
static_assert(sizeof(ZfhAssociationMeta::client_id) == 1,
              "ZfhAssociationMeta.client_id changed width");
inline constexpr uint32_t ZFH_AM_OFF_PROFILE = 9;
inline constexpr uint32_t ZFH_AM_LEN_PROFILE = 1;
static_assert(offsetof(ZfhAssociationMeta, profile) == 9,
              "ZfhAssociationMeta.profile moved off offset 9");
static_assert(sizeof(ZfhAssociationMeta::profile) == 1,
              "ZfhAssociationMeta.profile changed width");
inline constexpr uint32_t ZFH_AM_OFF_EXECUTION_CLASS = 10;
inline constexpr uint32_t ZFH_AM_LEN_EXECUTION_CLASS = 1;
static_assert(offsetof(ZfhAssociationMeta, execution_class) == 10,
              "ZfhAssociationMeta.execution_class moved off offset 10");
static_assert(sizeof(ZfhAssociationMeta::execution_class) == 1,
              "ZfhAssociationMeta.execution_class changed width");
inline constexpr uint32_t ZFH_AM_OFF_FLAGS = 11;
inline constexpr uint32_t ZFH_AM_LEN_FLAGS = 1;
static_assert(offsetof(ZfhAssociationMeta, flags) == 11,
              "ZfhAssociationMeta.flags moved off offset 11");
static_assert(sizeof(ZfhAssociationMeta::flags) == 1,
              "ZfhAssociationMeta.flags changed width");
inline constexpr uint32_t ZFH_AM_OFF_EXPECTED_POINTS = 12;
inline constexpr uint32_t ZFH_AM_LEN_EXPECTED_POINTS = 4;
static_assert(offsetof(ZfhAssociationMeta, expected_points) == 12,
              "ZfhAssociationMeta.expected_points moved off offset 12");
static_assert(sizeof(ZfhAssociationMeta::expected_points) == 4,
              "ZfhAssociationMeta.expected_points changed width");
inline constexpr uint32_t ZFH_AM_OFF_VARYING_INPUT_MASK = 16;
inline constexpr uint32_t ZFH_AM_LEN_VARYING_INPUT_MASK = 4;
static_assert(offsetof(ZfhAssociationMeta, varying_input_mask) == 16,
              "ZfhAssociationMeta.varying_input_mask moved off offset 16");
static_assert(sizeof(ZfhAssociationMeta::varying_input_mask) == 4,
              "ZfhAssociationMeta.varying_input_mask changed width");
inline constexpr uint32_t ZFH_AM_OFF_UNIFORM_INPUT_MASK = 20;
inline constexpr uint32_t ZFH_AM_LEN_UNIFORM_INPUT_MASK = 4;
static_assert(offsetof(ZfhAssociationMeta, uniform_input_mask) == 20,
              "ZfhAssociationMeta.uniform_input_mask moved off offset 20");
static_assert(sizeof(ZfhAssociationMeta::uniform_input_mask) == 4,
              "ZfhAssociationMeta.uniform_input_mask changed width");
inline constexpr uint32_t ZFH_AM_OFF_PREPARATION_SERIAL = 24;
inline constexpr uint32_t ZFH_AM_LEN_PREPARATION_SERIAL = 4;
static_assert(offsetof(ZfhAssociationMeta, preparation_serial) == 24,
              "ZfhAssociationMeta.preparation_serial moved off offset 24");
static_assert(sizeof(ZfhAssociationMeta::preparation_serial) == 4,
              "ZfhAssociationMeta.preparation_serial changed width");
inline constexpr uint32_t ZFH_AM_OFF_MAX_GROUP_QUANTUM = 28;
inline constexpr uint32_t ZFH_AM_LEN_MAX_GROUP_QUANTUM = 4;
static_assert(offsetof(ZfhAssociationMeta, max_group_quantum) == 28,
              "ZfhAssociationMeta.max_group_quantum moved off offset 28");
static_assert(sizeof(ZfhAssociationMeta::max_group_quantum) == 4,
              "ZfhAssociationMeta.max_group_quantum changed width");
inline constexpr uint32_t ZFH_AM_OFF_SOURCE_ID = 32;
inline constexpr uint32_t ZFH_AM_LEN_SOURCE_ID = 4;
static_assert(offsetof(ZfhAssociationMeta, source_id) == 32,
              "ZfhAssociationMeta.source_id moved off offset 32");
static_assert(sizeof(ZfhAssociationMeta::source_id) == 4,
              "ZfhAssociationMeta.source_id changed width");
inline constexpr uint32_t ZFH_AM_OFF_NUMERIC_UNIFORM_STATUS = 36;
inline constexpr uint32_t ZFH_AM_LEN_NUMERIC_UNIFORM_STATUS = 4;
static_assert(offsetof(ZfhAssociationMeta, numeric_uniform_status) == 36,
              "ZfhAssociationMeta.numeric_uniform_status moved off offset 36");
static_assert(sizeof(ZfhAssociationMeta::numeric_uniform_status) == 4,
              "ZfhAssociationMeta.numeric_uniform_status changed width");
inline constexpr uint32_t ZFH_AM_OFF_CLIENT_BINDING_COOKIE = 40;
inline constexpr uint32_t ZFH_AM_LEN_CLIENT_BINDING_COOKIE = 8;
static_assert(offsetof(ZfhAssociationMeta, client_binding_cookie) == 40,
              "ZfhAssociationMeta.client_binding_cookie moved off offset 40");
static_assert(sizeof(ZfhAssociationMeta::client_binding_cookie) == 8,
              "ZfhAssociationMeta.client_binding_cookie changed width");
inline constexpr uint32_t ZFH_AM_OFF_RESERVED = 48;
inline constexpr uint32_t ZFH_AM_LEN_RESERVED = 16;
static_assert(offsetof(ZfhAssociationMeta, reserved) == 48,
              "ZfhAssociationMeta.reserved moved off offset 48");
static_assert(sizeof(ZfhAssociationMeta::reserved) == 16,
              "ZfhAssociationMeta.reserved changed width");

// PRELOAD, one 8-byte row (directive 11.4). Duplicates with conflicting
// values refuse; same-value duplicates may be canonicalised by the packer
// but must not weaken the installed completeness check.
struct ZfhPreloadRow {
  uint16_t physical_register;  // 0..63
  uint16_t flags;
  int32_t value;
};
inline constexpr uint32_t ZFH_PRE_BYTES = 8;
static_assert(sizeof(ZfhPreloadRow) == 8,
              "ZfhPreloadRow must be exactly 8 bytes");
inline constexpr uint32_t ZFH_PRE_OFF_PHYSICAL_REGISTER = 0;
inline constexpr uint32_t ZFH_PRE_LEN_PHYSICAL_REGISTER = 2;
static_assert(offsetof(ZfhPreloadRow, physical_register) == 0,
              "ZfhPreloadRow.physical_register moved off offset 0");
static_assert(sizeof(ZfhPreloadRow::physical_register) == 2,
              "ZfhPreloadRow.physical_register changed width");
inline constexpr uint32_t ZFH_PRE_OFF_FLAGS = 2;
inline constexpr uint32_t ZFH_PRE_LEN_FLAGS = 2;
static_assert(offsetof(ZfhPreloadRow, flags) == 2,
              "ZfhPreloadRow.flags moved off offset 2");
static_assert(sizeof(ZfhPreloadRow::flags) == 2,
              "ZfhPreloadRow.flags changed width");
inline constexpr uint32_t ZFH_PRE_OFF_VALUE = 4;
inline constexpr uint32_t ZFH_PRE_LEN_VALUE = 4;
static_assert(offsetof(ZfhPreloadRow, value) == 4,
              "ZfhPreloadRow.value moved off offset 4");
static_assert(sizeof(ZfhPreloadRow::value) == 4,
              "ZfhPreloadRow.value changed width");

// ------------------------------------------ profile arity ---
// spec/form/field-ir.md section 7.1. Checked against that file by
// `tools/field/gen_field_host_schema.py --check`, so the parenthetical
// defect that produced Warp's (14) and Formation's (11) cannot recur
// silently.

struct ZfhProfileArity {
  const char* name;
  uint8_t id;
  uint8_t input_count;
  uint8_t output_count;
};
inline constexpr ZfhProfileArity ZFH_PROFILES[] = {
    {"earth", 0, 12, 4},
    {"warp", 1, 15, 6},
    {"flow", 2, 13, 7},
    {"formation", 3, 12, 6},
    {"stamp", 4, 8, 3},
};
inline constexpr int ZFH_PROFILE_COUNT = 5;

}  // namespace host_image
}  // namespace zfield
