// GENERATED FILE - tools/field/gen_field_host_schema.py - DO NOT EDIT.
// The frozen ZFH2 host-image envelope (spec/form/field-host-image.md),
// emitted from ONE schema into this package and into
// reference/include/zfield/generated/zfield_host_image.hpp so the
// offsets cannot disagree. Regenerate:
//   python tools/field/gen_field_host_schema.py
// and commit. `--check` is the freshness gate.
// schema-sha256: 162fb7f8e391715c3b2078d9ca5c3d0b9a79bc06633ad8e9c7711b03d4b65d1c
//
// THE TWO MASKS ARE DIFFERENT TYPES ON PURPOSE, AND DIFFERENT
// WIDTHS. zfh_required_mask_t is indexed by canonical output
// ORDINAL; zfh_window_mask_t is indexed by contiguous capture-
// WINDOW POSITION (zhao_field_host.sv:854's out_idx_c). They
// coincide only when the output registers are contiguous from
// out_base, and R111 measured that they never are: the three
// shipped Earth programs give window masks 0x17 / 0x1D / 0x17,
// every one leaving three of seven window lanes unwritten.
// The widths differ (7 vs 8) so that a cross-assignment is a
// WIDTH diagnostic under `-Wall`, not a silent truncation.
// OUTPUT_MAP is the translation between them, not a direct wire.

package zhao_field_host_image_pkg;

  // ---------------------------------------------- constants ---
  // 'Z'
  localparam int unsigned ZFH_MAGIC0 = 90;
  // 'F'
  localparam int unsigned ZFH_MAGIC1 = 70;
  // 'H'
  localparam int unsigned ZFH_MAGIC2 = 72;
  // '2'
  localparam int unsigned ZFH_MAGIC3 = 50;
  // header.image_version in this revision
  localparam int unsigned ZFH_IMAGE_VERSION = 1;
  // header.host_protocol_version
  localparam int unsigned ZFH_HOST_PROTOCOL_VERSION = 2;
  // header.header_bytes
  localparam int unsigned ZFH_HEADER_BYTES = 64;
  // section bodies and total_bytes are aligned to this
  localparam int unsigned ZFH_ALIGNMENT = 64;
  // the unused address in a map row -- never an implicitly valid zero
  localparam int unsigned ZFH_ADDR_UNUSED = 65535;
  // warp's 15 is the widest profile; bounds the ordinal masks
  localparam int unsigned ZFH_MAX_CANONICAL_INPUTS = 15;
  // flow's 7 is the widest profile
  localparam int unsigned ZFH_MAX_CANONICAL_OUTPUTS = 7;
  // the INIT_PROOF masks are 64 bits because of this
  localparam int unsigned ZFH_MAX_PHYSICAL_REGISTERS = 64;
  // TABLE0..TABLE3
  localparam int unsigned ZFH_MAX_TABLES = 4;
  // 15 words = 60 bytes plus 4 zero pad bytes (directive 11.4)
  localparam int unsigned ZFH_CANONICAL_INPUTS_BYTES = 64;

  // ------------------------------------------ section kinds ---
  localparam logic [15:0] ZFH_SECTION_CANONICAL_IMAGE = 16'h0001;  // the validated .zprog, exact bytes
  localparam logic [15:0] ZFH_SECTION_PROGRAM_META = 16'h0002;  // singleton
  localparam logic [15:0] ZFH_SECTION_PHYSICAL_UOPS = 16'h0003;  // 8-byte native words
  localparam logic [15:0] ZFH_SECTION_INPUT_MAP = 16'h0004;  // one row per canonical input ordinal
  localparam logic [15:0] ZFH_SECTION_OUTPUT_MAP = 16'h0005;  // one row per canonical output ordinal
  localparam logic [15:0] ZFH_SECTION_INIT_PROOF = 16'h0006;  // singleton
  localparam logic [15:0] ZFH_SECTION_DEMAND = 16'h0007;  // optional; per-resource counts for the selected form
  localparam logic [15:0] ZFH_SECTION_TABLE0 = 16'h0010;  // canonical table body bytes, t=0
  localparam logic [15:0] ZFH_SECTION_TABLE1 = 16'h0011;  // canonical table body bytes, t=1
  localparam logic [15:0] ZFH_SECTION_TABLE2 = 16'h0012;  // canonical table body bytes, t=2
  localparam logic [15:0] ZFH_SECTION_TABLE3 = 16'h0013;  // canonical table body bytes, t=3
  localparam logic [15:0] ZFH_SECTION_ASSOCIATION_META = 16'h0020;  // singleton
  localparam logic [15:0] ZFH_SECTION_CANONICAL_INPUTS = 16'h0021;  // 15 raw words, 60 bytes + 4 zero pad
  localparam logic [15:0] ZFH_SECTION_PREPARED_SCALARS = 16'h0022;  // exact values from zfield::prepare
  localparam logic [15:0] ZFH_SECTION_PRELOAD = 16'h0023;  // one row per required physical register
  localparam logic [15:0] ZFH_SECTION_LEDGER = 16'h0024;  // optional; exact uniform per-cause ledger

  // ------------------------------------------ enumerations ---
  // header.object_kind (directive 11.1)
  localparam logic [7:0] ZFH_OBJECT_PROGRAM = 8'd0;
  localparam logic [7:0] ZFH_OBJECT_ASSOCIATION = 8'd1;

  // PROGRAM_META.execution_form (directive 11.3)
  localparam logic [7:0] ZFH_FORM_CANONICAL = 8'd0;
  localparam logic [7:0] ZFH_FORM_PREPARED_REGISTER = 8'd1;
  // requires a checked logical plan with no varying instructions, all
  // output sources valid prepared scalars, and ZERO physical uops. An empty
  // arbitrary physical program is NOT this optimisation.
  localparam logic [7:0] ZFH_FORM_UNIFORM_ONLY = 8'd2;

  // INPUT_MAP.source_class (directive 11.3)
  localparam logic [7:0] ZFH_SRCCLASS_VARYING_INPUT = 8'd0;
  localparam logic [7:0] ZFH_SRCCLASS_UNIFORM_INPUT = 8'd1;
  // only an actually unused input may carry this; the declared value still
  // exists in the association record for validation and capture
  localparam logic [7:0] ZFH_SRCCLASS_UNUSED_PROVEN = 8'd2;

  // OUTPUT_MAP.source_kind (directive 11.3)
  // source_index is a physical register; a granted write to it updates
  // export_value[j] and sets seen[j]
  localparam logic [7:0] ZFH_SRCKIND_VECTOR_REG = 8'd0;
  // source_index is a prepared-scalar index; seeded at point start, so it
  // has NO window position by construction and the export must not wait for
  // a vector write that will never come
  localparam logic [7:0] ZFH_SRCKIND_PREPARED_SCALAR = 8'd1;

  // ------------------------------------------------ flags ---
  // OUTPUT_MAP.flags bit 0. Must agree with PROGRAM_META.required_mask.
  localparam int unsigned ZFH_OUTPUT_FLAG_REQUIRED = 1;
  // INIT_PROOF.flags bit 0; set iff execution_form == CANONICAL.
  localparam int unsigned ZFH_INITPROOF_FLAG_CANONICAL = 1;
  // INIT_PROOF.flags bit 1; set iff execution_form is PREPARED_REGISTER or
  // a checked UNIFORM_ONLY.
  localparam int unsigned ZFH_INITPROOF_FLAG_PREPARED = 2;

  // ============================================================
  // THE TWO MASKS -- distinct packed struct types, distinct
  // widths. A packed struct keeps the member name in the way of
  // an accidental bare-vector assignment, and the differing
  // widths make a cross-assignment a WIDTH diagnostic.
  // ============================================================
  localparam int unsigned ZFH_WINDOW_MASK_BITS = 7;
  // CONTIGUOUS CAPTURE-WINDOW POSITION. Bit k = physical register out_base
  // + k was written. Width must equal the composed OUT_LANES; the console
  // composes 7. Lives in the loader header word at [32 +: OUT_LANES] (R101,
  // zhao_field_host.sv:585,1074).
  typedef struct packed {
    logic [ZFH_WINDOW_MASK_BITS-1:0] window_bits;
  } zfh_window_mask_t;

  localparam int unsigned ZFH_REQUIRED_MASK_BITS = 8;
  // CANONICAL OUTPUT ORDINAL. Bit j = canonical output j is declared.
  // Meaningful width is the profile's output count; carried in a u8. Lives
  // in PROGRAM_META.required_mask.
  typedef struct packed {
    logic [ZFH_REQUIRED_MASK_BITS-1:0] required_bits;
  } zfh_required_mask_t;

  // ---------------------------------------------- records ---
  // Common 64-byte image header (directive 11.1). Present at offset 0 of
  // every ZFH2 image, PROGRAM and ASSOCIATION alike.
  localparam int unsigned ZFH_HDR_BYTES = 64;
  localparam int unsigned ZFH_HDR_OFF_MAGIC = 0;  // ASCII 'ZFH2' = 5A 46 48 32
  localparam int unsigned ZFH_HDR_LEN_MAGIC = 4;
  localparam int unsigned ZFH_HDR_OFF_IMAGE_VERSION = 4;  // = 1 in this revision
  localparam int unsigned ZFH_HDR_LEN_IMAGE_VERSION = 2;
  localparam int unsigned ZFH_HDR_OFF_OBJECT_KIND = 6;  // 0 PROGRAM, 1 ASSOCIATION
  localparam int unsigned ZFH_HDR_LEN_OBJECT_KIND = 1;
  localparam int unsigned ZFH_HDR_OFF_FLAGS = 7;  // zero in this revision
  localparam int unsigned ZFH_HDR_LEN_FLAGS = 1;
  localparam int unsigned ZFH_HDR_OFF_TOTAL_BYTES = 8;  // positive, 64-byte aligned
  localparam int unsigned ZFH_HDR_LEN_TOTAL_BYTES = 4;
  localparam int unsigned ZFH_HDR_OFF_BODY_CRC32C = 12;  // whole image with bytes 12..15 zero
  localparam int unsigned ZFH_HDR_LEN_BODY_CRC32C = 4;
  localparam int unsigned ZFH_HDR_OFF_HOST_PROTOCOL_VERSION = 16;  // = 2
  localparam int unsigned ZFH_HDR_LEN_HOST_PROTOCOL_VERSION = 2;
  localparam int unsigned ZFH_HDR_OFF_RESERVED_18 = 18;  // reserved, must be zero
  localparam int unsigned ZFH_HDR_LEN_RESERVED_18 = 2;
  localparam int unsigned ZFH_HDR_OFF_CANONICAL_PROGRAM_HASH = 20;
  localparam int unsigned ZFH_HDR_LEN_CANONICAL_PROGRAM_HASH = 4;
  localparam int unsigned ZFH_HDR_OFF_CANONICAL_PROGRAM_HANDLE32 = 24;
  localparam int unsigned ZFH_HDR_LEN_CANONICAL_PROGRAM_HANDLE32 = 4;
  localparam int unsigned ZFH_HDR_OFF_RESOURCE_EPOCH = 28;
  localparam int unsigned ZFH_HDR_LEN_RESOURCE_EPOCH = 4;
  localparam int unsigned ZFH_HDR_OFF_LOGICAL_FPLAN_ABI_VERSION = 32;
  localparam int unsigned ZFH_HDR_LEN_LOGICAL_FPLAN_ABI_VERSION = 4;
  localparam int unsigned ZFH_HDR_OFF_EXECUTION_FABRIC_VERSION = 36;
  localparam int unsigned ZFH_HDR_LEN_EXECUTION_FABRIC_VERSION = 4;
  localparam int unsigned ZFH_HDR_OFF_SOURCE_ID32 = 40;
  localparam int unsigned ZFH_HDR_LEN_SOURCE_ID32 = 4;
  localparam int unsigned ZFH_HDR_OFF_SECTION_COUNT = 44;
  localparam int unsigned ZFH_HDR_LEN_SECTION_COUNT = 2;
  localparam int unsigned ZFH_HDR_OFF_HEADER_BYTES = 46;  // = 64
  localparam int unsigned ZFH_HDR_LEN_HEADER_BYTES = 2;
  localparam int unsigned ZFH_HDR_OFF_PARENT_BINDING_HANDLE = 48;  // zero for an unbound PROGRAM install
  localparam int unsigned ZFH_HDR_LEN_PARENT_BINDING_HANDLE = 4;
  localparam int unsigned ZFH_HDR_OFF_CANONICAL_FULL_IMAGE_CRC32C = 52;  // all serialized .zprog bytes, including that file's own embedded CRC field; never the sole dedup key
  localparam int unsigned ZFH_HDR_LEN_CANONICAL_FULL_IMAGE_CRC32C = 4;
  localparam int unsigned ZFH_HDR_OFF_OBJECT_SERIAL = 56;  // software identity; hardware also assigns a generation
  localparam int unsigned ZFH_HDR_LEN_OBJECT_SERIAL = 4;
  localparam int unsigned ZFH_HDR_OFF_RESERVED_60 = 60;  // reserved, must be zero
  localparam int unsigned ZFH_HDR_LEN_RESERVED_60 = 4;

  // One section-directory entry (directive 11.2). section_count of these
  // follow the header immediately, sorted by kind, then zero padding to a
  // 64-byte boundary.
  localparam int unsigned ZFH_SEC_BYTES = 16;
  localparam int unsigned ZFH_SEC_OFF_KIND = 0;  // a ZFH_SECTION_* value
  localparam int unsigned ZFH_SEC_LEN_KIND = 2;
  localparam int unsigned ZFH_SEC_OFF_ELEMENT_BYTES = 2;
  localparam int unsigned ZFH_SEC_LEN_ELEMENT_BYTES = 2;
  localparam int unsigned ZFH_SEC_OFF_ELEMENT_COUNT = 4;
  localparam int unsigned ZFH_SEC_LEN_ELEMENT_COUNT = 4;
  localparam int unsigned ZFH_SEC_OFF_OFFSET = 8;  // 64-byte aligned body offset
  localparam int unsigned ZFH_SEC_LEN_OFFSET = 4;
  localparam int unsigned ZFH_SEC_OFF_BYTE_LENGTH = 12;  // checked element_bytes * element_count, widened before multiply
  localparam int unsigned ZFH_SEC_LEN_BYTE_LENGTH = 4;

  // PROGRAM_META, exactly one 64-byte record (directive 11.3). The final
  // reserved words are not capacity for undocumented policy.
  localparam int unsigned ZFH_PM_BYTES = 64;
  localparam int unsigned ZFH_PM_OFF_PROFILE = 0;  // 0 earth, 1 warp, 2 flow, 3 formation, 4 stamp
  localparam int unsigned ZFH_PM_LEN_PROFILE = 1;
  localparam int unsigned ZFH_PM_OFF_BINDING_SIGNATURE = 1;
  localparam int unsigned ZFH_PM_LEN_BINDING_SIGNATURE = 1;
  localparam int unsigned ZFH_PM_OFF_EXECUTION_FORM = 2;  // 0 CANONICAL, 1 PREPARED_REGISTER, 2 UNIFORM_ONLY
  localparam int unsigned ZFH_PM_LEN_EXECUTION_FORM = 1;
  localparam int unsigned ZFH_PM_OFF_FLAGS = 3;
  localparam int unsigned ZFH_PM_LEN_FLAGS = 1;
  localparam int unsigned ZFH_PM_OFF_INPUT_COUNT = 4;
  localparam int unsigned ZFH_PM_LEN_INPUT_COUNT = 1;
  localparam int unsigned ZFH_PM_OFF_OUTPUT_COUNT = 5;  // 1..7 at a production binding
  localparam int unsigned ZFH_PM_LEN_OUTPUT_COUNT = 1;
  localparam int unsigned ZFH_PM_OFF_REQUIRED_MASK = 6;  // ORDINAL-INDEXED. Read the two-mask chapter before wiring this. Zero is a load-time refusal for a strict import.
  localparam int unsigned ZFH_PM_LEN_REQUIRED_MASK = 1;
  localparam int unsigned ZFH_PM_OFF_TABLE_COUNT = 7;  // 0..4
  localparam int unsigned ZFH_PM_LEN_TABLE_COUNT = 1;
  localparam int unsigned ZFH_PM_OFF_CANONICAL_INSTRUCTION_COUNT = 8;
  localparam int unsigned ZFH_PM_LEN_CANONICAL_INSTRUCTION_COUNT = 2;
  localparam int unsigned ZFH_PM_OFF_PHYSICAL_UOP_COUNT = 10;  // includes the terminating END for the non-uniform forms
  localparam int unsigned ZFH_PM_LEN_PHYSICAL_UOP_COUNT = 2;
  localparam int unsigned ZFH_PM_OFF_PHYSICAL_REGISTER_COUNT = 12;
  localparam int unsigned ZFH_PM_LEN_PHYSICAL_REGISTER_COUNT = 2;
  localparam int unsigned ZFH_PM_OFF_PREPARED_SCALAR_COUNT = 14;
  localparam int unsigned ZFH_PM_LEN_PREPARED_SCALAR_COUNT = 2;
  localparam int unsigned ZFH_PM_OFF_VARYING_INPUT_MASK = 16;  // CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits; NOT a physical RF register mask
  localparam int unsigned ZFH_PM_LEN_VARYING_INPUT_MASK = 4;
  localparam int unsigned ZFH_PM_OFF_POINT_PRELOAD_MASK = 20;  // CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits
  localparam int unsigned ZFH_PM_LEN_POINT_PRELOAD_MASK = 4;
  localparam int unsigned ZFH_PM_OFF_CODE_IMAGE_CRC = 24;
  localparam int unsigned ZFH_PM_LEN_CODE_IMAGE_CRC = 4;
  localparam int unsigned ZFH_PM_OFF_MAPS_CRC = 28;
  localparam int unsigned ZFH_PM_LEN_MAPS_CRC = 4;
  localparam int unsigned ZFH_PM_OFF_LOGICAL_PLAN_IDENTITY = 32;
  localparam int unsigned ZFH_PM_LEN_LOGICAL_PLAN_IDENTITY = 4;
  localparam int unsigned ZFH_PM_OFF_RESERVED = 36;  // reserved, must all be zero
  localparam int unsigned ZFH_PM_LEN_RESERVED = 28;

  // INPUT_MAP, one 8-byte row per canonical input ordinal (directive 11.3).
  // Validate as a complete ordinal set.
  localparam int unsigned ZFH_IN_BYTES = 8;
  localparam int unsigned ZFH_IN_OFF_ORDINAL = 0;
  localparam int unsigned ZFH_IN_LEN_ORDINAL = 1;
  localparam int unsigned ZFH_IN_OFF_LANE_TYPE = 1;  // code from the canonical Field I/O-map registry
  localparam int unsigned ZFH_IN_LEN_LANE_TYPE = 1;
  localparam int unsigned ZFH_IN_OFF_SOURCE_CLASS = 2;  // 0 VARYING_INPUT, 1 UNIFORM_INPUT, 2 UNUSED_PROVEN
  localparam int unsigned ZFH_IN_LEN_SOURCE_CLASS = 1;
  localparam int unsigned ZFH_IN_OFF_FLAGS = 3;
  localparam int unsigned ZFH_IN_LEN_FLAGS = 1;
  localparam int unsigned ZFH_IN_OFF_PHYSICAL_REGISTER = 4;  // ZFH_ADDR_UNUSED when not applicable, never an implicitly valid register zero
  localparam int unsigned ZFH_IN_LEN_PHYSICAL_REGISTER = 2;
  localparam int unsigned ZFH_IN_OFF_PREPARED_SLOT = 6;  // ZFH_ADDR_UNUSED when unused
  localparam int unsigned ZFH_IN_LEN_PREPARED_SLOT = 2;

  // OUTPUT_MAP, one 8-byte row per canonical output ORDINAL (directive
  // 11.3). THIS RECORD IS THE ORDINAL-TO-WINDOW TRANSLATION. Reserved or
  // unknown source_kind values refuse.
  localparam int unsigned ZFH_OUT_BYTES = 8;
  localparam int unsigned ZFH_OUT_OFF_ORDINAL = 0;  // canonical output ordinal j
  localparam int unsigned ZFH_OUT_LEN_ORDINAL = 1;
  localparam int unsigned ZFH_OUT_OFF_LANE_TYPE = 1;
  localparam int unsigned ZFH_OUT_LEN_LANE_TYPE = 1;
  localparam int unsigned ZFH_OUT_OFF_SOURCE_KIND = 2;  // 0 VECTOR_REG, 1 PREPARED_SCALAR
  localparam int unsigned ZFH_OUT_LEN_SOURCE_KIND = 1;
  localparam int unsigned ZFH_OUT_OFF_FLAGS = 3;  // bit0 REQUIRED; no undocumented meanings
  localparam int unsigned ZFH_OUT_LEN_FLAGS = 1;
  localparam int unsigned ZFH_OUT_OFF_SOURCE_INDEX = 4;  // physical register for VECTOR_REG, prepared-scalar index for PREPARED_SCALAR
  localparam int unsigned ZFH_OUT_LEN_SOURCE_INDEX = 2;
  localparam int unsigned ZFH_OUT_OFF_RESERVED = 6;  // reserved, must be zero
  localparam int unsigned ZFH_OUT_LEN_RESERVED = 2;

  // INIT_PROOF, exactly one 64-byte record (directive 11.3). EVERY MASK
  // HERE IS INDEXED BY PHYSICAL REGISTER 0..63 -- no u32 mask may silently
  // discard registers 32..63.
  localparam int unsigned ZFH_IP_BYTES = 64;
  localparam int unsigned ZFH_IP_OFF_PROOF_VERSION = 0;  // = 1
  localparam int unsigned ZFH_IP_LEN_PROOF_VERSION = 2;
  localparam int unsigned ZFH_IP_OFF_RECORD_BYTES = 2;  // = 64
  localparam int unsigned ZFH_IP_LEN_RECORD_BYTES = 2;
  localparam int unsigned ZFH_IP_OFF_FLAGS = 4;  // bit0 canonical validation, bit1 prepared physical validation; exactly the one matching execution_form is set
  localparam int unsigned ZFH_IP_LEN_FLAGS = 4;
  localparam int unsigned ZFH_IP_OFF_ASSOCIATION_REGISTER_MASK = 8;
  localparam int unsigned ZFH_IP_LEN_ASSOCIATION_REGISTER_MASK = 8;
  localparam int unsigned ZFH_IP_OFF_POINT_REGISTER_MASK = 16;
  localparam int unsigned ZFH_IP_LEN_POINT_REGISTER_MASK = 8;
  localparam int unsigned ZFH_IP_OFF_IMMUTABLE_REGISTER_MASK = 24;  // association-owned; must not overlap vector_write_mask
  localparam int unsigned ZFH_IP_LEN_IMMUTABLE_REGISTER_MASK = 8;
  localparam int unsigned ZFH_IP_OFF_INITIAL_DEFINED_MASK = 32;  // must equal association_register_mask | point_register_mask
  localparam int unsigned ZFH_IP_LEN_INITIAL_DEFINED_MASK = 8;
  localparam int unsigned ZFH_IP_OFF_VECTOR_WRITE_MASK = 40;
  localparam int unsigned ZFH_IP_LEN_VECTOR_WRITE_MASK = 8;
  localparam int unsigned ZFH_IP_OFF_CODE_IMAGE_CRC = 48;
  localparam int unsigned ZFH_IP_LEN_CODE_IMAGE_CRC = 4;
  localparam int unsigned ZFH_IP_OFF_MAPS_CRC = 52;
  localparam int unsigned ZFH_IP_LEN_MAPS_CRC = 4;
  localparam int unsigned ZFH_IP_OFF_EXPECTED_ASSOCIATION_PRELOAD_COUNT = 56;  // the mask POPULATION COUNT, not the highest register plus one
  localparam int unsigned ZFH_IP_LEN_EXPECTED_ASSOCIATION_PRELOAD_COUNT = 2;
  localparam int unsigned ZFH_IP_OFF_RESERVED_58 = 58;  // reserved, must be zero
  localparam int unsigned ZFH_IP_LEN_RESERVED_58 = 2;
  localparam int unsigned ZFH_IP_OFF_RESERVED_60 = 60;  // reserved, must be zero
  localparam int unsigned ZFH_IP_LEN_RESERVED_60 = 4;

  // ASSOCIATION_META, exactly one 64-byte record (directive 11.4). The
  // varying and uniform input masks are disjoint and together cover every
  // required used input; unused inputs are declared, not omitted.
  localparam int unsigned ZFH_AM_BYTES = 64;
  localparam int unsigned ZFH_AM_OFF_ASSOCIATION_SERIAL = 0;
  localparam int unsigned ZFH_AM_LEN_ASSOCIATION_SERIAL = 4;
  localparam int unsigned ZFH_AM_OFF_FRAME_ID = 4;
  localparam int unsigned ZFH_AM_LEN_FRAME_ID = 4;
  localparam int unsigned ZFH_AM_OFF_CLIENT_ID = 8;
  localparam int unsigned ZFH_AM_LEN_CLIENT_ID = 1;
  localparam int unsigned ZFH_AM_OFF_PROFILE = 9;
  localparam int unsigned ZFH_AM_LEN_PROFILE = 1;
  localparam int unsigned ZFH_AM_OFF_EXECUTION_CLASS = 10;
  localparam int unsigned ZFH_AM_LEN_EXECUTION_CLASS = 1;
  localparam int unsigned ZFH_AM_OFF_FLAGS = 11;
  localparam int unsigned ZFH_AM_LEN_FLAGS = 1;
  localparam int unsigned ZFH_AM_OFF_EXPECTED_POINTS = 12;  // useful application points, not physical padding lanes
  localparam int unsigned ZFH_AM_LEN_EXPECTED_POINTS = 4;
  localparam int unsigned ZFH_AM_OFF_VARYING_INPUT_MASK = 16;  // canonical input ordinals
  localparam int unsigned ZFH_AM_LEN_VARYING_INPUT_MASK = 4;
  localparam int unsigned ZFH_AM_OFF_UNIFORM_INPUT_MASK = 20;  // canonical input ordinals
  localparam int unsigned ZFH_AM_LEN_UNIFORM_INPUT_MASK = 4;
  localparam int unsigned ZFH_AM_OFF_PREPARATION_SERIAL = 24;
  localparam int unsigned ZFH_AM_LEN_PREPARATION_SERIAL = 4;
  localparam int unsigned ZFH_AM_OFF_MAX_GROUP_QUANTUM = 28;
  localparam int unsigned ZFH_AM_LEN_MAX_GROUP_QUANTUM = 4;
  localparam int unsigned ZFH_AM_OFF_SOURCE_ID = 32;
  localparam int unsigned ZFH_AM_LEN_SOURCE_ID = 4;
  localparam int unsigned ZFH_AM_OFF_NUMERIC_UNIFORM_STATUS = 36;  // only generated defined bits are used
  localparam int unsigned ZFH_AM_LEN_NUMERIC_UNIFORM_STATUS = 4;
  localparam int unsigned ZFH_AM_OFF_CLIENT_BINDING_COOKIE = 40;
  localparam int unsigned ZFH_AM_LEN_CLIENT_BINDING_COOKIE = 8;
  localparam int unsigned ZFH_AM_OFF_RESERVED = 48;  // reserved, must all be zero
  localparam int unsigned ZFH_AM_LEN_RESERVED = 16;

  // PRELOAD, one 8-byte row (directive 11.4). Duplicates with conflicting
  // values refuse; same-value duplicates may be canonicalised by the packer
  // but must not weaken the installed completeness check.
  localparam int unsigned ZFH_PRE_BYTES = 8;
  localparam int unsigned ZFH_PRE_OFF_PHYSICAL_REGISTER = 0;  // 0..63
  localparam int unsigned ZFH_PRE_LEN_PHYSICAL_REGISTER = 2;
  localparam int unsigned ZFH_PRE_OFF_FLAGS = 2;
  localparam int unsigned ZFH_PRE_LEN_FLAGS = 2;
  localparam int unsigned ZFH_PRE_OFF_VALUE = 4;
  localparam int unsigned ZFH_PRE_LEN_VALUE = 4;

  // ---------------------------------------- profile arity ---
  // spec/form/field-ir.md section 7.1.
  localparam int unsigned ZFH_PROFILE_EARTH_ID      = 0;
  localparam int unsigned ZFH_PROFILE_EARTH_INPUTS  = 12;
  localparam int unsigned ZFH_PROFILE_EARTH_OUTPUTS = 4;
  localparam int unsigned ZFH_PROFILE_WARP_ID      = 1;
  localparam int unsigned ZFH_PROFILE_WARP_INPUTS  = 15;
  localparam int unsigned ZFH_PROFILE_WARP_OUTPUTS = 6;
  localparam int unsigned ZFH_PROFILE_FLOW_ID      = 2;
  localparam int unsigned ZFH_PROFILE_FLOW_INPUTS  = 13;
  localparam int unsigned ZFH_PROFILE_FLOW_OUTPUTS = 7;
  localparam int unsigned ZFH_PROFILE_FORMATION_ID      = 3;
  localparam int unsigned ZFH_PROFILE_FORMATION_INPUTS  = 12;
  localparam int unsigned ZFH_PROFILE_FORMATION_OUTPUTS = 6;
  localparam int unsigned ZFH_PROFILE_STAMP_ID      = 4;
  localparam int unsigned ZFH_PROFILE_STAMP_INPUTS  = 8;
  localparam int unsigned ZFH_PROFILE_STAMP_OUTPUTS = 3;

endpackage : zhao_field_host_image_pkg
