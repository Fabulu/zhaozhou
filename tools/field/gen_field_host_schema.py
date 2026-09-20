#!/usr/bin/env python3
"""gen_field_host_schema.py -- ONE schema for the ZFH2 host-image envelope,
emitted into C++ and SystemVerilog so the offsets cannot disagree.

  Law:      spec/form/field-host-image.md (authored beside this generator)
  Directive: reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt
             sections 11.1-11.5 (owner commit 6262868c), which says verbatim:
             "Generate C++/SV constants and packer/reader tests from one
             schema. Do not hand-maintain offsets in three languages."

  Emits:
    reference/include/zfield/generated/zfield_host_image.hpp
    fpga/rtl/field/generated/zhao_field_host_image_pkg.sv
    spec/form/field-host-image.md   (the marked generated table region only)

-----------------------------------------------------------------------------
THE CENTRAL REQUIREMENT: TWO MASKS THAT ARE NOT THE SAME NUMBERING
-----------------------------------------------------------------------------
This generator exists mainly to make ONE mistake impossible to make quietly.

  window_mask    indexed by CONTIGUOUS CAPTURE-WINDOW POSITION.
                 zhao_field_host.sv:854 computes
                     out_idx_c = fab_wr_reg - hdr_outbase[cur_slot]
                 valid while that difference is < OUT_LANES. Bit k means
                 "physical register out_base + k was written".
                 Width = OUT_LANES (7 as composed in zhao_console_core).
                 Lives in the header word at [32 +: OUT_LANES] (R101).

  required_mask  indexed by CANONICAL OUTPUT ORDINAL. Bit j means "canonical
                 output j of the profile is declared".
                 Width = the profile's output count (earth 4, warp 6, flow 7,
                 formation 6, stamp 3), carried in a u8.
                 Lives in PROGRAM_META.required_mask.

THEY COINCIDE ONLY WHEN OUTPUT REGISTERS ARE CONTIGUOUS FROM out_base, AND
R111 MEASURED THAT THEY NEVER ARE. tools/field/zprog_output_coverage.py reports
window masks 0x17 / 0x1D / 0x17 for the three shipped Earth programs -- every
one leaves three of the seven window lanes unwritten. Worked example:
crater_ring writes R13,R14,R15,R17 with out_base=13, so the window mask is 0x17
and the ordinal mask is 0x0F. Holes are the NORMAL case.

OUTPUT_MAP is the translation, one row per ORDINAL carrying source_kind and
source_index. It is not a direct wire.

So the two masks get DISTINCT GENERATED TYPE NAMES in both languages:
assigning one to the other is a COMPILE ERROR, not a wrong value. A comment
saying "these are different" is what the console already had, and it is why
R101 shipped. The compile-fail control lives at
tests/mutants/field_host_mask_type_confusion_mutant.cpp and is driven by
`--compile-fail-control`, which requires the mutant to FAIL to compile and its
positive control to SUCCEED.

-----------------------------------------------------------------------------
WHAT --check VERIFIES, AND WHY EACH SIDE IS CLOCKED BY SOMETHING DIFFERENT
-----------------------------------------------------------------------------
CLAUDE.md's metadata-swap chapter says the first question to ask of any checker
is what the two sides of its comparison are driven by. If one source drives
both, the comparison is structurally blind. So these checks use deliberately
independent operands:

  A. schema self-consistency   -- fields contiguous, non-overlapping, summing
                                  to the declared record size. One operand, but
                                  it is an internal invariant, not a comparison.
  B. parser self-tests         -- the field-ir.md arity parser is fired against
                                  the KNOWN-BAD historical row ("(11)" against
                                  twelve fields) and must REJECT it, and against
                                  the corrected row and must ACCEPT it. A
                                  detector that has not been shown to fire has
                                  not been tested.
  C. freshness                 -- regenerated text vs the committed files.
  D. C++/SV offset parity      -- the two GENERATED FILES are parsed back from
                                  disk and compared TO EACH OTHER, not to the
                                  schema. A hand-edit to either side moves one
                                  operand and not the other, so it fires. This
                                  is the negative control the packet owes.
  E. FH21 opcode-shape parity  -- fpga/rtl/field/zhao_field_ops_pkg.sv's
                                  field_long_width vs the generated C++
                                  zfield_optable.hpp dst_width. Two files, two
                                  toolchains, two authors. BIDIRECTIONAL, per
                                  ruling R110: a check that only asks "is
                                  everything declared present?" and never "is
                                  everything present declared?" reports a clean
                                  ledger for a half-missing one.
  F. profile arity vs spec     -- schema vs spec/form/field-ir.md section 7.1.
  G. profile arity vs the      -- schema vs tools/field/zprog_output_coverage.py
     fourth copy                  PROFILE_OUTPUTS, which is a fourth copy of the
                                  same ratified table.

Every parser here strips comments BEFORE matching and resolves against a
DECLARATION, never a substring of raw text. That is ruling R105: a check that
matches raw bytes accepts a name that appears only in a comment -- the obituary
resolving the corpse.

-----------------------------------------------------------------------------
DETERMINISM (directive FT107 / FT015)
-----------------------------------------------------------------------------
No timestamps, no hostnames, no dict iteration order that is not declaration
order. Line endings are canonicalised to LF before comparison and written with
newline="\n". Output is rstrip()ed then given exactly one trailing newline.
Writes are atomic (tempfile + os.replace) so a killed run cannot leave a
truncated generated file -- the zero-byte-backup hazard from 2026-09-06.

The provenance header carries schema-sha256: a hash of the CANONICAL SCHEMA
SERIALISATION only, not of this whole file. It therefore changes exactly when a
layout changes and not when a comment is reworded, which is what makes it
useful as provenance rather than noise.

Usage:
    python tools/field/gen_field_host_schema.py            # write
    python tools/field/gen_field_host_schema.py --check    # verify, RC 1 stale
    python tools/field/gen_field_host_schema.py --compile-fail-control
"""

import hashlib
import io
import os
import re
import subprocess
import sys
import tempfile

# --------------------------------------------------------------------------
# Paths. Resolved from THIS FILE, never from the process working directory --
# CLAUDE.md's "[IO.File] ignores cd" trap has a Python-shaped cousin whenever a
# generator is invoked by ctest from the build tree.
# --------------------------------------------------------------------------
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

CXX_OUT = os.path.join(ROOT, "reference", "include", "zfield", "generated",
                       "zfield_host_image.hpp")
SV_OUT = os.path.join(ROOT, "fpga", "rtl", "field", "generated",
                      "zhao_field_host_image_pkg.sv")
SPEC_OUT = os.path.join(ROOT, "spec", "form", "field-host-image.md")

OPS_PKG = os.path.join(ROOT, "fpga", "rtl", "field", "zhao_field_ops_pkg.sv")
OPTABLE_HPP = os.path.join(ROOT, "reference", "include", "zfield", "generated",
                           "zfield_optable.hpp")
FIELD_IR_MD = os.path.join(ROOT, "spec", "form", "field-ir.md")
COVERAGE_PY = os.path.join(ROOT, "tools", "field", "zprog_output_coverage.py")

MUTANT_CPP = os.path.join(ROOT, "tests", "mutants",
                          "field_host_mask_type_confusion_mutant.cpp")

GEN_REL = "tools/field/gen_field_host_schema.py"

SPEC_BEGIN = "<!-- BEGIN GENERATED: record-layout (gen_field_host_schema.py) -->"
SPEC_END = "<!-- END GENERATED: record-layout -->"

# --------------------------------------------------------------------------
# Scalar kinds. `bytes` is a fixed-length octet array (the magic).
# --------------------------------------------------------------------------
KIND_BYTES = {"u8": 1, "u16": 2, "u32": 4, "u64": 8, "s32": 4, "bytes": 1}
KIND_CXX = {"u8": "uint8_t", "u16": "uint16_t", "u32": "uint32_t",
            "u64": "uint64_t", "s32": "int32_t", "bytes": "uint8_t"}


class F(object):
    """One field in one record."""

    def __init__(self, name, off, kind, count=1, note=""):
        self.name = name
        self.off = off
        self.kind = kind
        self.count = count
        self.note = note

    @property
    def size(self):
        return KIND_BYTES[self.kind] * self.count


class R(object):
    """One record layout."""

    def __init__(self, name, sv_prefix, size, doc, fields):
        self.name = name          # C++ struct name, e.g. ZfhHeader
        self.sv_prefix = sv_prefix  # SV localparam prefix, e.g. ZFH_HDR
        self.size = size
        self.doc = doc
        self.fields = fields


# ==========================================================================
# THE SCHEMA. This is the single source of truth. Everything below is a
# projection of it. Directive sections 11.1 through 11.4.
# ==========================================================================

RECORDS = [
    R("ZfhHeader", "ZFH_HDR", 64,
      "Common 64-byte image header (directive 11.1). Present at offset 0 of "
      "every ZFH2 image, PROGRAM and ASSOCIATION alike.",
      [
          F("magic", 0, "bytes", 4, "ASCII 'ZFH2' = 5A 46 48 32"),
          F("image_version", 4, "u16", 1, "= 1 in this revision"),
          F("object_kind", 6, "u8", 1, "0 PROGRAM, 1 ASSOCIATION"),
          F("flags", 7, "u8", 1, "zero in this revision"),
          F("total_bytes", 8, "u32", 1, "positive, 64-byte aligned"),
          F("body_crc32c", 12, "u32", 1,
            "whole image with bytes 12..15 zero"),
          F("host_protocol_version", 16, "u16", 1, "= 2"),
          F("reserved_18", 18, "u16", 1, "reserved, must be zero"),
          F("canonical_program_hash", 20, "u32", 1, ""),
          F("canonical_program_handle32", 24, "u32", 1, ""),
          F("resource_epoch", 28, "u32", 1, ""),
          F("logical_fplan_abi_version", 32, "u32", 1, ""),
          F("execution_fabric_version", 36, "u32", 1, ""),
          F("source_id32", 40, "u32", 1, ""),
          F("section_count", 44, "u16", 1, ""),
          F("header_bytes", 46, "u16", 1, "= 64"),
          F("parent_binding_handle", 48, "u32", 1,
            "zero for an unbound PROGRAM install"),
          F("canonical_full_image_crc32c", 52, "u32", 1,
            "all serialized .zprog bytes, including that file's own embedded "
            "CRC field; never the sole dedup key"),
          F("object_serial", 56, "u32", 1,
            "software identity; hardware also assigns a generation"),
          F("reserved_60", 60, "u32", 1, "reserved, must be zero"),
      ]),

    R("ZfhSectionEntry", "ZFH_SEC", 16,
      "One section-directory entry (directive 11.2). section_count of these "
      "follow the header immediately, sorted by kind, then zero padding to a "
      "64-byte boundary.",
      [
          F("kind", 0, "u16", 1, "a ZFH_SECTION_* value"),
          F("element_bytes", 2, "u16", 1, ""),
          F("element_count", 4, "u32", 1, ""),
          F("offset", 8, "u32", 1, "64-byte aligned body offset"),
          F("byte_length", 12, "u32", 1,
            "checked element_bytes * element_count, widened before multiply"),
      ]),

    R("ZfhProgramMeta", "ZFH_PM", 64,
      "PROGRAM_META, exactly one 64-byte record (directive 11.3). The final "
      "reserved words are not capacity for undocumented policy.",
      [
          F("profile", 0, "u8", 1, "0 earth, 1 warp, 2 flow, 3 formation, "
                                   "4 stamp"),
          F("binding_signature", 1, "u8", 1, ""),
          F("execution_form", 2, "u8", 1,
            "0 CANONICAL, 1 PREPARED_REGISTER, 2 UNIFORM_ONLY"),
          F("flags", 3, "u8", 1, ""),
          F("input_count", 4, "u8", 1, ""),
          F("output_count", 5, "u8", 1, "1..7 at a production binding"),
          F("required_mask", 6, "u8", 1,
            "ORDINAL-INDEXED. Read the two-mask chapter before wiring this. "
            "Zero is a load-time refusal for a strict import."),
          F("table_count", 7, "u8", 1, "0..4"),
          F("canonical_instruction_count", 8, "u16", 1, ""),
          F("physical_uop_count", 10, "u16", 1,
            "includes the terminating END for the non-uniform forms"),
          F("physical_register_count", 12, "u16", 1, ""),
          F("prepared_scalar_count", 14, "u16", 1, ""),
          F("varying_input_mask", 16, "u32", 1,
            "CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits; NOT a "
            "physical RF register mask"),
          F("point_preload_mask", 20, "u32", 1,
            "CANONICAL INPUT ORDINAL mask, at most 15 meaningful bits"),
          F("code_image_crc", 24, "u32", 1, ""),
          F("maps_crc", 28, "u32", 1, ""),
          F("logical_plan_identity", 32, "u32", 1, ""),
          F("reserved", 36, "u32", 7, "reserved, must all be zero"),
      ]),

    R("ZfhInputMapRow", "ZFH_IN", 8,
      "INPUT_MAP, one 8-byte row per canonical input ordinal (directive "
      "11.3). Validate as a complete ordinal set.",
      [
          F("ordinal", 0, "u8", 1, ""),
          F("lane_type", 1, "u8", 1,
            "code from the canonical Field I/O-map registry"),
          F("source_class", 2, "u8", 1,
            "0 VARYING_INPUT, 1 UNIFORM_INPUT, 2 UNUSED_PROVEN"),
          F("flags", 3, "u8", 1, ""),
          F("physical_register", 4, "u16", 1,
            "ZFH_ADDR_UNUSED when not applicable, never an implicitly valid "
            "register zero"),
          F("prepared_slot", 6, "u16", 1, "ZFH_ADDR_UNUSED when unused"),
      ]),

    R("ZfhOutputMapRow", "ZFH_OUT", 8,
      "OUTPUT_MAP, one 8-byte row per canonical output ORDINAL (directive "
      "11.3). THIS RECORD IS THE ORDINAL-TO-WINDOW TRANSLATION. Reserved or "
      "unknown source_kind values refuse.",
      [
          F("ordinal", 0, "u8", 1, "canonical output ordinal j"),
          F("lane_type", 1, "u8", 1, ""),
          F("source_kind", 2, "u8", 1, "0 VECTOR_REG, 1 PREPARED_SCALAR"),
          F("flags", 3, "u8", 1, "bit0 REQUIRED; no undocumented meanings"),
          F("source_index", 4, "u16", 1,
            "physical register for VECTOR_REG, prepared-scalar index for "
            "PREPARED_SCALAR"),
          F("reserved", 6, "u16", 1, "reserved, must be zero"),
      ]),

    R("ZfhInitProof", "ZFH_IP", 64,
      "INIT_PROOF, exactly one 64-byte record (directive 11.3). EVERY MASK "
      "HERE IS INDEXED BY PHYSICAL REGISTER 0..63 -- no u32 mask may silently "
      "discard registers 32..63.",
      [
          F("proof_version", 0, "u16", 1, "= 1"),
          F("record_bytes", 2, "u16", 1, "= 64"),
          F("flags", 4, "u32", 1,
            "bit0 canonical validation, bit1 prepared physical validation; "
            "exactly the one matching execution_form is set"),
          F("association_register_mask", 8, "u64", 1, ""),
          F("point_register_mask", 16, "u64", 1, ""),
          F("immutable_register_mask", 24, "u64", 1,
            "association-owned; must not overlap vector_write_mask"),
          F("initial_defined_mask", 32, "u64", 1,
            "must equal association_register_mask | point_register_mask"),
          F("vector_write_mask", 40, "u64", 1, ""),
          F("code_image_crc", 48, "u32", 1, ""),
          F("maps_crc", 52, "u32", 1, ""),
          F("expected_association_preload_count", 56, "u16", 1,
            "the mask POPULATION COUNT, not the highest register plus one"),
          F("reserved_58", 58, "u16", 1, "reserved, must be zero"),
          F("reserved_60", 60, "u32", 1, "reserved, must be zero"),
      ]),

    R("ZfhAssociationMeta", "ZFH_AM", 64,
      "ASSOCIATION_META, exactly one 64-byte record (directive 11.4). The "
      "varying and uniform input masks are disjoint and together cover every "
      "required used input; unused inputs are declared, not omitted.",
      [
          F("association_serial", 0, "u32", 1, ""),
          F("frame_id", 4, "u32", 1, ""),
          F("client_id", 8, "u8", 1, ""),
          F("profile", 9, "u8", 1, ""),
          F("execution_class", 10, "u8", 1, ""),
          F("flags", 11, "u8", 1, ""),
          F("expected_points", 12, "u32", 1,
            "useful application points, not physical padding lanes"),
          F("varying_input_mask", 16, "u32", 1, "canonical input ordinals"),
          F("uniform_input_mask", 20, "u32", 1, "canonical input ordinals"),
          F("preparation_serial", 24, "u32", 1, ""),
          F("max_group_quantum", 28, "u32", 1, ""),
          F("source_id", 32, "u32", 1, ""),
          F("numeric_uniform_status", 36, "u32", 1,
            "only generated defined bits are used"),
          F("client_binding_cookie", 40, "u64", 1, ""),
          F("reserved", 48, "u32", 4, "reserved, must all be zero"),
      ]),

    R("ZfhPreloadRow", "ZFH_PRE", 8,
      "PRELOAD, one 8-byte row (directive 11.4). Duplicates with conflicting "
      "values refuse; same-value duplicates may be canonicalised by the "
      "packer but must not weaken the installed completeness check.",
      [
          F("physical_register", 0, "u16", 1, "0..63"),
          F("flags", 2, "u16", 1, ""),
          F("value", 4, "s32", 1, ""),
      ]),
]

# ---- section kinds (directive 11.2) --------------------------------------
SECTION_KINDS = [
    ("CANONICAL_IMAGE", 0x0001, "the validated .zprog, exact bytes"),
    ("PROGRAM_META", 0x0002, "singleton"),
    ("PHYSICAL_UOPS", 0x0003, "8-byte native words"),
    ("INPUT_MAP", 0x0004, "one row per canonical input ordinal"),
    ("OUTPUT_MAP", 0x0005, "one row per canonical output ordinal"),
    ("INIT_PROOF", 0x0006, "singleton"),
    ("DEMAND", 0x0007, "optional; per-resource counts for the selected form"),
    ("TABLE0", 0x0010, "canonical table body bytes, t=0"),
    ("TABLE1", 0x0011, "canonical table body bytes, t=1"),
    ("TABLE2", 0x0012, "canonical table body bytes, t=2"),
    ("TABLE3", 0x0013, "canonical table body bytes, t=3"),
    ("ASSOCIATION_META", 0x0020, "singleton"),
    ("CANONICAL_INPUTS", 0x0021, "15 raw words, 60 bytes + 4 zero pad"),
    ("PREPARED_SCALARS", 0x0022, "exact values from zfield::prepare"),
    ("PRELOAD", 0x0023, "one row per required physical register"),
    ("LEDGER", 0x0024, "optional; exact uniform per-cause ledger"),
]

# ---- enumerations --------------------------------------------------------
ENUMS = [
    ("ZfhObjectKind", "ZFH_OBJECT", [
        ("PROGRAM", 0, ""),
        ("ASSOCIATION", 1, ""),
    ], "header.object_kind (directive 11.1)"),

    ("ZfhExecutionForm", "ZFH_FORM", [
        ("CANONICAL", 0, ""),
        ("PREPARED_REGISTER", 1, ""),
        ("UNIFORM_ONLY", 2,
         "requires a checked logical plan with no varying instructions, all "
         "output sources valid prepared scalars, and ZERO physical uops. An "
         "empty arbitrary physical program is NOT this optimisation."),
    ], "PROGRAM_META.execution_form (directive 11.3)"),

    ("ZfhSourceClass", "ZFH_SRCCLASS", [
        ("VARYING_INPUT", 0, ""),
        ("UNIFORM_INPUT", 1, ""),
        ("UNUSED_PROVEN", 2,
         "only an actually unused input may carry this; the declared value "
         "still exists in the association record for validation and capture"),
    ], "INPUT_MAP.source_class (directive 11.3)"),

    ("ZfhSourceKind", "ZFH_SRCKIND", [
        ("VECTOR_REG", 0,
         "source_index is a physical register; a granted write to it updates "
         "export_value[j] and sets seen[j]"),
        ("PREPARED_SCALAR", 1,
         "source_index is a prepared-scalar index; seeded at point start, so "
         "it has NO window position by construction and the export must not "
         "wait for a vector write that will never come"),
    ], "OUTPUT_MAP.source_kind (directive 11.3)"),
]

# ---- flag bits -----------------------------------------------------------
FLAGS = [
    ("ZFH_OUTPUT_FLAG_REQUIRED", 0x01,
     "OUTPUT_MAP.flags bit 0. Must agree with PROGRAM_META.required_mask."),
    ("ZFH_INITPROOF_FLAG_CANONICAL", 0x00000001,
     "INIT_PROOF.flags bit 0; set iff execution_form == CANONICAL."),
    ("ZFH_INITPROOF_FLAG_PREPARED", 0x00000002,
     "INIT_PROOF.flags bit 1; set iff execution_form is PREPARED_REGISTER or "
     "a checked UNIFORM_ONLY."),
]

# ---- scalar constants ----------------------------------------------------
CONSTS = [
    ("ZFH_MAGIC0", 0x5A, "'Z'"),
    ("ZFH_MAGIC1", 0x46, "'F'"),
    ("ZFH_MAGIC2", 0x48, "'H'"),
    ("ZFH_MAGIC3", 0x32, "'2'"),
    ("ZFH_IMAGE_VERSION", 1, "header.image_version in this revision"),
    ("ZFH_HOST_PROTOCOL_VERSION", 2, "header.host_protocol_version"),
    ("ZFH_HEADER_BYTES", 64, "header.header_bytes"),
    ("ZFH_ALIGNMENT", 64,
     "section bodies and total_bytes are aligned to this"),
    ("ZFH_ADDR_UNUSED", 0xFFFF,
     "the unused address in a map row -- never an implicitly valid zero"),
    ("ZFH_MAX_CANONICAL_INPUTS", 15,
     "warp's 15 is the widest profile; bounds the ordinal masks"),
    ("ZFH_MAX_CANONICAL_OUTPUTS", 7,
     "flow's 7 is the widest profile"),
    ("ZFH_MAX_PHYSICAL_REGISTERS", 64,
     "the INIT_PROOF masks are 64 bits because of this"),
    ("ZFH_MAX_TABLES", 4, "TABLE0..TABLE3"),
    ("ZFH_CANONICAL_INPUTS_BYTES", 64,
     "15 words = 60 bytes plus 4 zero pad bytes (directive 11.4)"),
]

# ---- the two masks -------------------------------------------------------
# name, bits-constant, SV type, C++ type, what the index MEANS
MASKS = [
    ("window", "ZFH_WINDOW_MASK_BITS", 7, "zfh_window_mask_t", "WindowMask",
     "CONTIGUOUS CAPTURE-WINDOW POSITION. Bit k = physical register "
     "out_base + k was written. Width must equal the composed OUT_LANES; the "
     "console composes 7. Lives in the loader header word at "
     "[32 +: OUT_LANES] (R101, zhao_field_host.sv:585,1074)."),
    ("required", "ZFH_REQUIRED_MASK_BITS", 8, "zfh_required_mask_t",
     "RequiredMask",
     "CANONICAL OUTPUT ORDINAL. Bit j = canonical output j is declared. "
     "Meaningful width is the profile's output count; carried in a u8. Lives "
     "in PROGRAM_META.required_mask."),
]

# ---- canonical profile arity, spec/form/field-ir.md section 7.1 ----------
# (name, id, input_count, output_count)
PROFILES = [
    ("earth", 0, 12, 4),
    ("warp", 1, 15, 6),
    ("flow", 2, 13, 7),
    ("formation", 3, 12, 6),
    ("stamp", 4, 8, 3),
]

# FH21: canonical opcodes whose service route is commissioned but not yet in
# zhao_field_ops_pkg.sv. Named as OUTSTANDING ROUTES (directive 20.3: "a
# capability not yet in hardware is reported as a named outstanding route, not
# marked executable"), NOT silently skipped. Packet F1 adds both; when it does,
# check E simply stops reporting them. A NEW absentee fails.
OPS_OUTSTANDING = {
    0x17: ("RCP", "canonical RCP -- directive 2.6; F1 adds it through the "
                  "existing exact Field reciprocal leaf, explicitly NOT the "
                  "raster/projector reciprocal"),
    0x21: ("RING", "varying-radius RING -- zhao_field_ops_pkg.sv:68 states it "
                   "'is still absent and stays absent'; F1 adds a bounded "
                   "varying-radius route through shared services"),
}

# C++ optable svc codes that mean "leaves the executor for a service".
SVC_SERVICE = {2: "CURVE", 3: "DIST", 4: "COLD"}


# ==========================================================================
# Schema validation -- check A
# ==========================================================================

def validate_schema():
    """Fields contiguous, non-overlapping, summing to the declared size."""
    errs = []
    for rec in RECORDS:
        cursor = 0
        for f in rec.fields:
            if f.off != cursor:
                errs.append(
                    "%s.%s starts at %d but the previous field ends at %d "
                    "(a hole or an overlap)" % (rec.name, f.name, f.off,
                                                cursor))
            if f.kind not in KIND_BYTES:
                errs.append("%s.%s has unknown kind %r" %
                            (rec.name, f.name, f.kind))
                continue
            # natural alignment, which is what lets the C++ struct be plain
            unit = KIND_BYTES[f.kind]
            if f.kind != "bytes" and (f.off % unit) != 0:
                errs.append("%s.%s at offset %d is not %d-byte aligned" %
                            (rec.name, f.name, f.off, unit))
            cursor = f.off + f.size
        if cursor != rec.size:
            errs.append("%s fields sum to %d, declared size is %d" %
                        (rec.name, cursor, rec.size))
    names = [r.name for r in RECORDS]
    if len(set(names)) != len(names):
        errs.append("duplicate record name")
    kinds = [v for _, v, _ in SECTION_KINDS]
    if len(set(kinds)) != len(kinds):
        errs.append("duplicate section kind value")
    if kinds != sorted(kinds):
        errs.append("section kinds are not in ascending order; the directive "
                    "requires directory entries sorted by kind")
    for _, bits_name, bits, _, _, _ in MASKS:
        if bits <= 0 or bits > 64:
            errs.append("%s = %d is out of range" % (bits_name, bits))
    wm = [m for m in MASKS if m[0] == "window"][0]
    rm = [m for m in MASKS if m[0] == "required"][0]
    if wm[3] == rm[3] or wm[4] == rm[4]:
        errs.append("the two masks must have DISTINCT type names in both "
                    "languages; that is the whole point of this generator")
    if wm[2] == rm[2]:
        errs.append("the two mask widths are equal, which removes the SV "
                    "width diagnostic that makes a cross-assignment an error")
    return errs


def canonical_schema_text():
    """A stable serialisation of the layout, hashed into the provenance line.

    Deliberately excludes prose: it changes when a LAYOUT changes and not when
    a comment is reworded, which is what makes it useful as provenance.
    """
    out = []
    for rec in RECORDS:
        out.append("record %s %s %d" % (rec.name, rec.sv_prefix, rec.size))
        for f in rec.fields:
            out.append("  %s %d %s %d" % (f.name, f.off, f.kind, f.count))
    for n, v, _ in SECTION_KINDS:
        out.append("section %s 0x%04X" % (n, v))
    for cname, _, members, _ in ENUMS:
        for mn, mv, _ in members:
            out.append("enum %s %s %d" % (cname, mn, mv))
    for n, v, _ in FLAGS:
        out.append("flag %s 0x%X" % (n, v))
    for n, v, _ in CONSTS:
        out.append("const %s %d" % (n, v))
    for mname, bits_name, bits, sv_t, cxx_t, _ in MASKS:
        out.append("mask %s %s %d %s %s" % (mname, bits_name, bits, sv_t,
                                            cxx_t))
    for pn, pid, pin, pout in PROFILES:
        out.append("profile %s %d %d %d" % (pn, pid, pin, pout))
    return "\n".join(out) + "\n"


def schema_sha256():
    return hashlib.sha256(
        canonical_schema_text().encode("utf-8")).hexdigest()


# ==========================================================================
# Comment stripping -- ruling R105. Resolve against declarations, not text.
# ==========================================================================

def strip_cxx_comments(text):
    """Remove // and /* */ comments. Not a parser and not meant to be: a name
    in a string literal still resolves. It removes the one failure mode that
    THE ACT OF DOCUMENTING an absent thing creates."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            out.append(c)
            i += 1
            while i < n:
                if text[i] == "\\" and i + 1 < n:
                    out.append(text[i:i + 2])
                    i += 2
                    continue
                out.append(text[i])
                if text[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def _comment_blindness_self_test():
    """Fire the stripper on the case it exists for. A detector that has not
    been shown to fire has not been tested."""
    hidden = 'int a; // localparam logic [7:0] OP_GHOST = 8\'hEE;\n'
    assert "OP_GHOST" in hidden, "self-test fixture is wrong"
    assert "OP_GHOST" not in strip_cxx_comments(hidden), \
        "strip_cxx_comments failed to remove a // comment"
    block = "/* localparam logic [7:0] OP_GHOST = 8'hEE; */ int b;"
    assert "OP_GHOST" not in strip_cxx_comments(block), \
        "strip_cxx_comments failed to remove a /* */ comment"
    live = "localparam logic [7:0] OP_REAL = 8'h12;"
    assert "OP_REAL" in strip_cxx_comments(live), \
        "strip_cxx_comments removed a live declaration"


# ==========================================================================
# spec/form/field-ir.md section 7.1 arity parser -- checks B and F
# ==========================================================================

_RANGE_RE = re.compile(r"^([A-Za-z_]+)(\d+)\.\.([A-Za-z_]*)(\d+)")


def count_record_fields(desc):
    """Count the lanes actually listed in a field-ir.md 7.1 record cell.

    Each comma-separated token is one lane, except a `pN..pM` range, which is
    M - N + 1 lanes. The trailing parenthetical is NOT consulted -- that is
    precisely the number being audited.
    """
    body = desc.strip()
    body = re.sub(r"\(\s*\d+\s*\)\s*$", "", body).strip()
    total = 0
    for tok in body.split(","):
        tok = tok.strip()
        if not tok:
            continue
        name = tok.split(":")[0].strip()
        name = re.sub(r"^(parent|the)\s+", "", name).strip()
        m = _RANGE_RE.match(name)
        if m:
            lo = int(m.group(2))
            hi = int(m.group(4))
            if hi < lo:
                raise ValueError("descending range %r" % tok)
            total += hi - lo + 1
        else:
            total += 1
    return total


def declared_count(desc):
    m = re.search(r"\(\s*(\d+)\s*\)\s*$", desc.strip())
    if not m:
        return None
    return int(m.group(1))


def _arity_parser_self_test():
    """FIRE IT ON THE DEFECT IT EXISTS FOR.

    The historical Formation row said "(11)" against twelve listed fields, and
    the historical Warp row said "(14)" against fifteen. Both must be REJECTED.
    The corrected rows must be ACCEPTED. A parser that only ever agrees is a
    parser nobody has tested.
    """
    bad_formation = ("index:u32, time:u32, parent rot2:fx,fx, trans2:fx,fx, "
                     "p0..p5:fx (11)")
    good_formation = bad_formation.replace("(11)", "(12)")
    assert count_record_fields(bad_formation) == 12, \
        "arity parser miscounts the Formation record"
    assert declared_count(bad_formation) == 11
    assert count_record_fields(bad_formation) != declared_count(bad_formation), \
        "POSITIVE CONTROL FAILED: the parser accepted the historical (11)"
    assert count_record_fields(good_formation) == declared_count(good_formation)

    bad_warp = ("px,py,pz:fx, nx,ny,nz:fx, a0..a3:fx, time:u32, p0..p3:fx (14)")
    good_warp = bad_warp.replace("(14)", "(15)")
    assert count_record_fields(bad_warp) == 15
    assert count_record_fields(bad_warp) != declared_count(bad_warp), \
        "POSITIVE CONTROL FAILED: the parser accepted the historical (14)"
    assert count_record_fields(good_warp) == declared_count(good_warp)

    # and the remaining three rows, so the fixture is the whole table
    assert count_record_fields("x:fx, z:fx, age:u32, phase:fx, p0..p7:fx") == 12
    assert count_record_fields(
        "px,py,pz, vx,vy,vz:fx, age:u32, seed:u32, dt:fx, p0..p3:fx") == 13
    assert count_record_fields("u,v:unit, age:u32, strength:unit, "
                               "p0..p3:fx") == 8


def read_text(path):
    with io.open(path, encoding="utf-8") as fh:
        return fh.read().replace("\r\n", "\n").replace("\r", "\n")


def check_field_ir_arity():
    """Check F: the schema's profile table against spec/form/field-ir.md 7.1,
    AND each row's parenthetical against the lanes it actually lists."""
    errs = []
    try:
        md = read_text(FIELD_IR_MD)
    except OSError as exc:
        return ["cannot read %s: %s" % (FIELD_IR_MD, exc)]

    by_name = {}
    for line in md.split("\n"):
        if not line.startswith("| "):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) != 4:
            continue
        name, pid, inrec, outrec = cells
        if not re.match(r"^\d+$", pid):
            continue
        by_name[name] = (int(pid), inrec, outrec)

    if not by_name:
        return ["found no profile table rows in %s -- the parser matched "
                "nothing, which is a broken instrument, not a clean result"
                % FIELD_IR_MD]

    for pname, pid, pin, pout in PROFILES:
        if pname not in by_name:
            errs.append("profile %r is in the schema but not in field-ir.md "
                        "7.1" % pname)
            continue
        md_id, inrec, outrec = by_name.pop(pname)
        if md_id != pid:
            errs.append("profile %s: id %d in the schema, %d in field-ir.md"
                        % (pname, pid, md_id))
        for label, cell, want in (("input", inrec, pin),
                                  ("output", outrec, pout)):
            listed = count_record_fields(cell)
            decl = declared_count(cell)
            if decl is None:
                errs.append("profile %s %s record in field-ir.md has no "
                            "parenthetical count" % (pname, label))
            elif decl != listed:
                errs.append("profile %s %s record in field-ir.md declares "
                            "(%d) but LISTS %d lanes -- this is the Warp "
                            "'(14)' and Formation '(11)' defect recurring"
                            % (pname, label, decl, listed))
            if listed != want:
                errs.append("profile %s %s: schema says %d, field-ir.md lists "
                            "%d" % (pname, label, want, listed))
    for leftover in sorted(by_name):
        errs.append("profile %r is in field-ir.md 7.1 but not in the schema"
                    % leftover)
    return errs


def check_coverage_probe_copy():
    """Check G: the FOURTH copy of the same ratified table, in
    tools/field/zprog_output_coverage.py's PROFILE_OUTPUTS."""
    try:
        src = read_text(COVERAGE_PY)
    except OSError as exc:
        return ["cannot read %s: %s" % (COVERAGE_PY, exc)]
    m = re.search(r"PROFILE_OUTPUTS\s*=\s*\{(.*?)\}", src, re.S)
    if not m:
        return ["could not find PROFILE_OUTPUTS in %s; if that table was "
                "renamed or removed, update this check rather than deleting "
                "it" % COVERAGE_PY]
    body = strip_cxx_comments(m.group(1))
    body = re.sub(r"#[^\n]*", "", body)
    found = {}
    for pid, pname, pout in re.findall(
            r"(\d+)\s*:\s*\(\s*[\"'](\w+)[\"']\s*,\s*(\d+)\s*\)", body):
        found[int(pid)] = (pname, int(pout))
    if not found:
        return ["PROFILE_OUTPUTS in %s parsed to nothing -- precision at zero "
                "is a tell, not a result" % COVERAGE_PY]
    errs = []
    for pname, pid, _, pout in PROFILES:
        if pid not in found:
            errs.append("profile id %d (%s) missing from PROFILE_OUTPUTS"
                        % (pid, pname))
            continue
        gn, go = found[pid]
        if gn != pname or go != pout:
            errs.append("PROFILE_OUTPUTS[%d] = (%r, %d); schema says (%r, %d)"
                        % (pid, gn, go, pname, pout))
    return errs


# ==========================================================================
# FH21 -- check E. zhao_field_ops_pkg.sv vs the generated C++ optable.
# ==========================================================================

def parse_sv_ops():
    """(opcode -> (name, long_width)) from the SV package's DECLARATIONS."""
    src = strip_cxx_comments(read_text(OPS_PKG))
    codes = {}
    for name, hexv in re.findall(
            r"localparam\s+logic\s*\[\s*7\s*:\s*0\s*\]\s+(\w+)\s*=\s*"
            r"8'h([0-9A-Fa-f]{2})\s*;", src):
        codes[name] = int(hexv, 16)
    if not codes:
        raise ValueError("no opcode localparams parsed from %s" % OPS_PKG)

    fm = re.search(
        r"function\s+automatic\s+logic\s*\[[^\]]*\]\s+field_long_width\b"
        r"(.*?)endfunction", src, re.S)
    if not fm:
        raise ValueError("field_long_width not found in %s" % OPS_PKG)
    cm = re.search(r"case\s*\(\s*op\s*\)(.*?)endcase", fm.group(1), re.S)
    if not cm:
        raise ValueError("no case statement in field_long_width")

    widths = {}
    for arm in cm.group(1).split(";"):
        if "=" not in arm:
            continue
        labels, rhs = arm.rsplit("=", 1)
        wm = re.search(r"2'd(\d+)", rhs)
        if not wm:
            continue
        width = int(wm.group(1))
        labels = labels.rsplit(":", 1)[0]
        for lab in labels.split(","):
            lab = lab.strip()
            if not lab or lab == "default":
                continue
            if lab not in codes:
                raise ValueError(
                    "field_long_width names %r, which is not a localparam in "
                    "%s -- a case label that resolves to nothing" %
                    (lab, OPS_PKG))
            widths[codes[lab]] = (lab, width)
    if not widths:
        raise ValueError("parsed zero case arms from field_long_width")
    return codes, widths


def parse_cxx_optable():
    """(opcode -> (name, dst_width, svc)) from the generated C++ table."""
    src = strip_cxx_comments(read_text(OPTABLE_HPP))
    rows = {}
    for m in re.finditer(
            r"\{\s*0x([0-9a-fA-F]{2})\s*,\s*\"(\w+)\"\s*,\s*(\d+)\s*,\s*"
            r"(\d+)\s*,\s*\{[^}]*\}\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}",
            src):
        code = int(m.group(1), 16)
        rows[code] = (m.group(2), int(m.group(3)), int(m.group(7)))
    if not rows:
        raise ValueError("no OpShape rows parsed from %s" % OPTABLE_HPP)
    return rows


def check_ops_parity(verbose=True):
    """FH21's missing half, BIDIRECTIONAL (ruling R110)."""
    errs = []
    notes = []
    try:
        sv_codes, sv_widths = parse_sv_ops()
        cxx = parse_cxx_optable()
    except (OSError, ValueError) as exc:
        return ["ops parity could not run: %s" % exc], notes

    # Direction 1: everything the SV table declares must match the C++ shape.
    for code, (label, width) in sorted(sv_widths.items()):
        if code >= 0xF0:
            notes.append("  %-14s 0x%02X width %d -- plan-internal micro-op, "
                         "outside the canonical space by design; no canonical "
                         "shape to compare" % (label, code, width))
            continue
        if code not in cxx:
            errs.append("SV declares %s = 0x%02X, which is not a canonical "
                        "opcode in %s" % (label, code, OPTABLE_HPP))
            continue
        cname, dst_width, _svc = cxx[code]
        if width != dst_width:
            errs.append("OPCODE SHAPE DRIFT 0x%02X %s: SV field_long_width = "
                        "%d, C++ optable dst_width = %d" %
                        (code, cname, width, dst_width))

    # Direction 2 -- the half ruling R110 says is always missing. Every
    # canonical op that LEAVES the executor for a service must be in the SV
    # table, or be a NAMED outstanding route.
    for code in sorted(cxx):
        cname, dst_width, svc = cxx[code]
        if svc not in SVC_SERVICE:
            continue
        if code in sv_widths:
            continue
        if code in OPS_OUTSTANDING:
            oname, why = OPS_OUTSTANDING[code]
            notes.append("  OUTSTANDING ROUTE 0x%02X %s (svc %s, dst_width "
                         "%d): %s" % (code, oname, SVC_SERVICE[svc],
                                      dst_width, why))
            continue
        errs.append("0x%02X %s is a service-routed canonical op (svc %s) with "
                    "no entry in %s, and is not a declared outstanding route. "
                    "An op offered by the executor and answerable by nobody "
                    "parks forever." % (code, cname, SVC_SERVICE[svc],
                                        OPS_PKG))

    if verbose:
        print("FH21 opcode-shape parity: %d SV entries against %d canonical "
              "C++ shapes, both directions" % (len(sv_widths), len(cxx)))
        for n in notes:
            print(n)
    return errs, notes


# ==========================================================================
# Emitters
# ==========================================================================

def _wrap(prefix, text, width=76):
    words = text.split()
    lines = []
    cur = prefix
    for w in words:
        if len(cur) + 1 + len(w) > width and cur.strip() != prefix.strip():
            lines.append(cur.rstrip())
            cur = prefix + w
        else:
            cur = cur + (" " if cur.strip() != prefix.strip() else "") + w
    if cur.strip() != prefix.strip():
        lines.append(cur.rstrip())
    return lines


def emit_cxx():
    sha = schema_sha256()
    L = []
    L.append("// GENERATED FILE - %s - DO NOT EDIT." % GEN_REL)
    L.append("// The frozen ZFH2 host-image envelope "
             "(spec/form/field-host-image.md),")
    L.append("// emitted from ONE schema into this header and into")
    L.append("// fpga/rtl/field/generated/zhao_field_host_image_pkg.sv so the "
             "offsets")
    L.append("// cannot disagree. Regenerate:")
    L.append("//   python %s" % GEN_REL)
    L.append("// and commit. `--check` is the freshness gate.")
    L.append("// schema-sha256: %s" % sha)
    L.append("//")
    L.append("// THE TWO MASKS ARE DIFFERENT TYPES ON PURPOSE. RequiredMask is "
             "indexed by")
    L.append("// canonical output ORDINAL; WindowMask is indexed by contiguous "
             "capture-")
    L.append("// WINDOW POSITION. They coincide only when output registers are "
             "contiguous")
    L.append("// from out_base, and R111 measured that they never are. "
             "Assigning one to")
    L.append("// the other does not compile -- see the compile-fail control at")
    L.append("// tests/mutants/field_host_mask_type_confusion_mutant.cpp.")
    L.append("#pragma once")
    L.append("")
    L.append("#include <cstddef>")
    L.append("#include <cstdint>")
    L.append("")
    L.append("namespace zfield {")
    L.append("namespace host_image {")
    L.append("")

    L.append("// ------------------------------------------------ constants ---")
    L.append("")
    for name, val, note in CONSTS:
        if note:
            L.extend(_wrap("// ", note))
        lit = ("0x%04X" % val) if val > 255 else str(val)
        L.append("inline constexpr uint32_t %s = %s;" % (name, lit))
    L.append("")

    L.append("// -------------------------------------------- section kinds ---")
    L.append("")
    for name, val, note in SECTION_KINDS:
        cmt = ("  // %s" % note) if note else ""
        L.append("inline constexpr uint16_t ZFH_SECTION_%s = 0x%04X;%s"
                 % (name, val, cmt))
    L.append("")
    L.append("inline constexpr int ZFH_SECTION_KIND_COUNT = %d;"
             % len(SECTION_KINDS))
    L.append("")

    L.append("// -------------------------------------------- enumerations ---")
    L.append("")
    for cname, _svp, members, doc in ENUMS:
        L.extend(_wrap("// ", doc))
        L.append("enum %s : uint8_t {" % cname)
        for mn, mv, note in members:
            if note:
                L.extend(_wrap("  // ", note))
            L.append("  %s_%s = %d," % (_enum_prefix(cname), mn, mv))
        L.append("};")
        L.append("")

    L.append("// -------------------------------------------------- flags ---")
    L.append("")
    for name, val, note in FLAGS:
        if note:
            L.extend(_wrap("// ", note))
        L.append("inline constexpr uint32_t %s = 0x%X;" % (name, val))
    L.append("")

    # ---- the two mask types ----
    L.append("// ==============================================================")
    L.append("// THE TWO MASKS")
    L.append("// ==============================================================")
    L.append("//")
    L.append("// These are distinct CLASSES, not typedefs, and neither converts")
    L.append("// to the other or to its underlying integer implicitly. That is")
    L.append("// deliberate: a typedef would make the confusion a wrong VALUE,")
    L.append("// and the whole point is to make it a compile ERROR.")
    L.append("")
    for mname, bits_name, bits, _sv_t, cxx_t, doc in MASKS:
        L.append("inline constexpr int %s = %d;" % (bits_name, bits))
        L.extend(_wrap("// ", doc))
        L.append("class %s {" % cxx_t)
        L.append(" public:")
        L.append("  using rep = uint%d_t;" % (8 if bits <= 8 else 16))
        L.append("  constexpr %s() = default;" % cxx_t)
        L.append("  // EXPLICIT on purpose: a bare integer does not silently")
        L.append("  // become a mask of either numbering.")
        L.append("  constexpr explicit %s(rep bits) : bits_(bits) {}" % cxx_t)
        L.append("  constexpr rep bits() const { return bits_; }")
        L.append("  constexpr bool empty() const { return bits_ == 0; }")
        L.append("  constexpr bool test(int i) const {")
        L.append("    return ((bits_ >> i) & 1u) != 0u;")
        L.append("  }")
        L.append("  constexpr bool covers(%s other) const {" % cxx_t)
        L.append("    return (bits_ & other.bits_) == other.bits_;")
        L.append("  }")
        L.append("  friend constexpr bool operator==(%s a, %s b) {"
                 % (cxx_t, cxx_t))
        L.append("    return a.bits_ == b.bits_;")
        L.append("  }")
        L.append("  friend constexpr bool operator!=(%s a, %s b) {"
                 % (cxx_t, cxx_t))
        L.append("    return a.bits_ != b.bits_;")
        L.append("  }")
        L.append("  static constexpr int kBits = %s;" % bits_name)
        L.append("")
        L.append(" private:")
        L.append("  rep bits_ = 0;")
        L.append("};")
        L.append("")
    wm = [m for m in MASKS if m[0] == "window"][0]
    rm = [m for m in MASKS if m[0] == "required"][0]
    L.append("// The separation, asserted rather than asserted-in-prose. Spelt")
    L.append("// out rather than using std::is_same_v so this header needs no")
    L.append("// <type_traits> and compiles on every toolchain the tree uses.")
    L.append("template <class A, class B>")
    L.append("struct ZfhSameType {")
    L.append("  static constexpr bool value = false;")
    L.append("};")
    L.append("template <class A>")
    L.append("struct ZfhSameType<A, A> {")
    L.append("  static constexpr bool value = true;")
    L.append("};")
    L.append("static_assert(!ZfhSameType<%s, %s>::value," % (wm[4], rm[4]))
    L.append("              \"the two masks collapsed into one type\");")
    L.append("")

    # ---- records ----
    L.append("// ------------------------------------------------- records ---")
    L.append("")
    for rec in RECORDS:
        L.extend(_wrap("// ", rec.doc))
        L.append("struct %s {" % rec.name)
        for f in rec.fields:
            ctype = KIND_CXX[f.kind]
            arr = ("[%d]" % f.count) if (f.count > 1 or f.kind == "bytes") \
                else ""
            cmt = ("  // %s" % f.note) if f.note else ""
            L.append("  %s %s%s;%s" % (ctype, f.name, arr, cmt))
        L.append("};")
        L.append("inline constexpr uint32_t %s_BYTES = %d;"
                 % (rec.sv_prefix, rec.size))
        L.append("static_assert(sizeof(%s) == %d," % (rec.name, rec.size))
        L.append("              \"%s must be exactly %d bytes\");"
                 % (rec.name, rec.size))
        for f in rec.fields:
            L.append("inline constexpr uint32_t %s_OFF_%s = %d;"
                     % (rec.sv_prefix, f.name.upper(), f.off))
            L.append("inline constexpr uint32_t %s_LEN_%s = %d;"
                     % (rec.sv_prefix, f.name.upper(), f.size))
            L.append("static_assert(offsetof(%s, %s) == %d,"
                     % (rec.name, f.name, f.off))
            L.append("              \"%s.%s moved off offset %d\");"
                     % (rec.name, f.name, f.off))
            L.append("static_assert(sizeof(%s::%s) == %d,"
                     % (rec.name, f.name, f.size))
            L.append("              \"%s.%s changed width\");"
                     % (rec.name, f.name))
        L.append("")

    # ---- profile arity ----
    L.append("// ------------------------------------------ profile arity ---")
    L.append("// spec/form/field-ir.md section 7.1. Checked against that file "
             "by")
    L.append("// `%s --check`, so the parenthetical" % GEN_REL)
    L.append("// defect that produced Warp's (14) and Formation's (11) cannot "
             "recur")
    L.append("// silently.")
    L.append("")
    L.append("struct ZfhProfileArity {")
    L.append("  const char* name;")
    L.append("  uint8_t id;")
    L.append("  uint8_t input_count;")
    L.append("  uint8_t output_count;")
    L.append("};")
    L.append("inline constexpr ZfhProfileArity ZFH_PROFILES[] = {")
    for pname, pid, pin, pout in PROFILES:
        L.append("    {\"%s\", %d, %d, %d}," % (pname, pid, pin, pout))
    L.append("};")
    L.append("inline constexpr int ZFH_PROFILE_COUNT = %d;" % len(PROFILES))
    L.append("")
    L.append("}  // namespace host_image")
    L.append("}  // namespace zfield")
    return "\n".join(L).rstrip() + "\n"


def _enum_prefix(cname):
    """ZfhObjectKind -> ZFH_OBJECT_KIND; used for the C++ member names."""
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", cname).upper()
    return s


def emit_sv():
    sha = schema_sha256()
    L = []
    L.append("// GENERATED FILE - %s - DO NOT EDIT." % GEN_REL)
    L.append("// The frozen ZFH2 host-image envelope "
             "(spec/form/field-host-image.md),")
    L.append("// emitted from ONE schema into this package and into")
    L.append("// reference/include/zfield/generated/zfield_host_image.hpp so "
             "the")
    L.append("// offsets cannot disagree. Regenerate:")
    L.append("//   python %s" % GEN_REL)
    L.append("// and commit. `--check` is the freshness gate.")
    L.append("// schema-sha256: %s" % sha)
    L.append("//")
    L.append("// THE TWO MASKS ARE DIFFERENT TYPES ON PURPOSE, AND DIFFERENT")
    L.append("// WIDTHS. zfh_required_mask_t is indexed by canonical output")
    L.append("// ORDINAL; zfh_window_mask_t is indexed by contiguous capture-")
    L.append("// WINDOW POSITION (zhao_field_host.sv:854's out_idx_c). They")
    L.append("// coincide only when the output registers are contiguous from")
    L.append("// out_base, and R111 measured that they never are: the three")
    L.append("// shipped Earth programs give window masks 0x17 / 0x1D / 0x17,")
    L.append("// every one leaving three of seven window lanes unwritten.")
    L.append("// The widths differ (%d vs %d) so that a cross-assignment is a"
             % (MASKS[0][2], MASKS[1][2]))
    L.append("// WIDTH diagnostic under `-Wall`, not a silent truncation.")
    L.append("// OUTPUT_MAP is the translation between them, not a direct "
             "wire.")
    L.append("")
    L.append("package zhao_field_host_image_pkg;")
    L.append("")

    L.append("  // ---------------------------------------------- constants ---")
    for name, val, note in CONSTS:
        if note:
            L.extend(_wrap("  // ", note))
        L.append("  localparam int unsigned %s = %d;" % (name, val))
    L.append("")

    L.append("  // ------------------------------------------ section kinds ---")
    for name, val, note in SECTION_KINDS:
        cmt = ("  // %s" % note) if note else ""
        L.append("  localparam logic [15:0] ZFH_SECTION_%s = 16'h%04X;%s"
                 % (name, val, cmt))
    L.append("")

    L.append("  // ------------------------------------------ enumerations ---")
    for cname, svp, members, doc in ENUMS:
        L.extend(_wrap("  // ", doc))
        for mn, mv, note in members:
            if note:
                L.extend(_wrap("  // ", note))
            L.append("  localparam logic [7:0] %s_%s = 8'd%d;"
                     % (svp, mn, mv))
        L.append("")

    L.append("  // ------------------------------------------------ flags ---")
    for name, val, note in FLAGS:
        if note:
            L.extend(_wrap("  // ", note))
        L.append("  localparam int unsigned %s = %d;" % (name, val))
    L.append("")

    L.append("  // ============================================================")
    L.append("  // THE TWO MASKS -- distinct packed struct types, distinct")
    L.append("  // widths. A packed struct keeps the member name in the way of")
    L.append("  // an accidental bare-vector assignment, and the differing")
    L.append("  // widths make a cross-assignment a WIDTH diagnostic.")
    L.append("  // ============================================================")
    for mname, bits_name, bits, sv_t, _cxx_t, doc in MASKS:
        L.append("  localparam int unsigned %s = %d;" % (bits_name, bits))
        L.extend(_wrap("  // ", doc))
        L.append("  typedef struct packed {")
        L.append("    logic [%s-1:0] %s_bits;" % (bits_name, mname))
        L.append("  } %s;" % sv_t)
        L.append("")

    L.append("  // ---------------------------------------------- records ---")
    for rec in RECORDS:
        L.extend(_wrap("  // ", rec.doc))
        L.append("  localparam int unsigned %s_BYTES = %d;"
                 % (rec.sv_prefix, rec.size))
        for f in rec.fields:
            note = ("  // %s" % f.note) if f.note else ""
            L.append("  localparam int unsigned %s_OFF_%s = %d;%s"
                     % (rec.sv_prefix, f.name.upper(), f.off, note))
            L.append("  localparam int unsigned %s_LEN_%s = %d;"
                     % (rec.sv_prefix, f.name.upper(), f.size))
        L.append("")

    L.append("  // ---------------------------------------- profile arity ---")
    L.append("  // spec/form/field-ir.md section 7.1.")
    for pname, pid, pin, pout in PROFILES:
        L.append("  localparam int unsigned ZFH_PROFILE_%s_ID      = %d;"
                 % (pname.upper(), pid))
        L.append("  localparam int unsigned ZFH_PROFILE_%s_INPUTS  = %d;"
                 % (pname.upper(), pin))
        L.append("  localparam int unsigned ZFH_PROFILE_%s_OUTPUTS = %d;"
                 % (pname.upper(), pout))
    L.append("")
    L.append("endpackage : zhao_field_host_image_pkg")
    return "\n".join(L).rstrip() + "\n"


def emit_spec_region():
    """The generated table region of spec/form/field-host-image.md."""
    L = []
    L.append(SPEC_BEGIN)
    L.append("")
    L.append("*Generated by `%s`. Do not edit between the markers; edit the "
             "schema and regenerate.*" % GEN_REL)
    L.append("")
    L.append("`schema-sha256: %s`" % schema_sha256())
    L.append("")
    for rec in RECORDS:
        L.append("### %s -- %d bytes" % (rec.name, rec.size))
        L.append("")
        L.append(rec.doc)
        L.append("")
        L.append("| Offset | Bytes | Type | Field | Note |")
        L.append("|---|---|---|---|---|")
        for f in rec.fields:
            kind = f.kind if f.count == 1 else "%s[%d]" % (f.kind, f.count)
            L.append("| %d | %d | %s | `%s` | %s |"
                     % (f.off, f.size, kind, f.name, f.note or ""))
        L.append("")
    L.append("### Section kinds")
    L.append("")
    L.append("| Kind | Value | Note |")
    L.append("|---|---|---|")
    for name, val, note in SECTION_KINDS:
        L.append("| `%s` | `0x%04X` | %s |" % (name, val, note))
    L.append("")
    L.append("### Profile arity (`spec/form/field-ir.md` 7.1)")
    L.append("")
    L.append("| Profile | id | Inputs | Outputs |")
    L.append("|---|---|---|---|")
    for pname, pid, pin, pout in PROFILES:
        L.append("| %s | %d | %d | %d |" % (pname, pid, pin, pout))
    L.append("")
    L.append(SPEC_END)
    return "\n".join(L)


def splice_spec(existing):
    """Replace the marked region of the spec file, keeping authored prose."""
    region = emit_spec_region()
    i = existing.find(SPEC_BEGIN)
    j = existing.find(SPEC_END)
    if i < 0 or j < 0:
        raise ValueError(
            "%s is missing the generated-region markers %r / %r"
            % (SPEC_OUT, SPEC_BEGIN, SPEC_END))
    return existing[:i] + region + existing[j + len(SPEC_END):]


# ==========================================================================
# Check D -- parse the two GENERATED files back and compare them TO EACH OTHER
# ==========================================================================

_LAYOUT_NAME = r"(\w+_(?:OFF|LEN)_\w+|\w+_BYTES)"


def parse_cxx_offsets(text):
    """Every layout symbol the C++ header declares. Comments stripped first,
    so a name that appears only in prose does not resolve (ruling R105)."""
    src = strip_cxx_comments(text)
    return {n: int(v) for n, v in re.findall(
        r"inline\s+constexpr\s+uint32_t\s+" + _LAYOUT_NAME +
        r"\s*=\s*(\d+)\s*;", src)}


def parse_sv_offsets(text):
    """The same symbols from the SV package, parsed independently."""
    src = strip_cxx_comments(text)
    return {n: int(v) for n, v in re.findall(
        r"localparam\s+int\s+unsigned\s+" + _LAYOUT_NAME +
        r"\s*=\s*(\d+)\s*;", src)}


def check_cxx_sv_parity():
    """The negative control this packet owes. The two operands are the two
    committed files, parsed independently -- NOT the schema, which would move
    both together and make the comparison structurally blind."""
    errs = []
    try:
        cxx = parse_cxx_offsets(read_text(CXX_OUT))
        sv = parse_sv_offsets(read_text(SV_OUT))
    except OSError as exc:
        return ["C++/SV parity could not run: %s" % exc]
    if not cxx:
        return ["parsed ZERO offsets from %s -- precision at zero is a tell"
                % CXX_OUT]
    if not sv:
        return ["parsed ZERO offsets from %s -- precision at zero is a tell"
                % SV_OUT]
    for name in sorted(set(cxx) | set(sv)):
        if name not in cxx:
            errs.append("%s is in the SV package but not in the C++ header"
                        % name)
        elif name not in sv:
            errs.append("%s is in the C++ header but not in the SV package"
                        % name)
        elif cxx[name] != sv[name]:
            errs.append("OFFSET MISMATCH %s: C++ says %d, SV says %d"
                        % (name, cxx[name], sv[name]))
    if not errs:
        print("C++/SV offset parity: %d symbols agree across both generated "
              "files" % len(cxx))
    return errs


# ==========================================================================
# Atomic write
# ==========================================================================

def write_atomic(path, text):
    d = os.path.dirname(path)
    if d and not os.path.isdir(d):
        os.makedirs(d)
    fd, tmp = tempfile.mkstemp(dir=d or ".", suffix=".tmp")
    try:
        with io.open(fd, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
            fh.flush()
            os.fsync(fh.fileno())
        os.replace(tmp, path)
    except BaseException:
        if os.path.exists(tmp):
            os.unlink(tmp)
        raise


# ==========================================================================
# The compile-fail control
# ==========================================================================

def compile_fail_control():
    """Prove the type separation is a COMPILE ERROR, in both polarities.

    Negative control: the mutant, which assigns a RequiredMask into a
    WindowMask-typed slot, must FAIL to compile.
    Positive control: the SAME file with ZFH_MASK_CONFUSION undefined, which
    does the legal thing, must SUCCEED.

    Both halves are required. A compile-fail test that has never been seen to
    compile is a test that might be failing for a typo.
    """
    cxx = os.environ.get("CXX", "g++")
    inc = os.path.join(ROOT, "reference", "include")
    if not os.path.exists(MUTANT_CPP):
        print("MISSING: %s" % MUTANT_CPP)
        return 2

    def run(defines):
        cmd = [cxx, "-std=c++17", "-fsyntax-only", "-I", inc]
        cmd += defines + [MUTANT_CPP]
        try:
            p = subprocess.run(cmd, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
        except OSError as exc:
            print("cannot run %r: %s" % (cxx, exc))
            return None, ""
        return p.returncode, p.stdout.decode("utf-8", "replace")

    print("=== POSITIVE CONTROL: legal use must COMPILE ===")
    rc_ok, out_ok = run([])
    if rc_ok is None:
        return 2
    print("rc=%d" % rc_ok)
    if rc_ok != 0:
        print(out_ok)
        print("FAIL: the positive control did not compile, so a failing "
              "negative control would prove nothing.")
        return 1

    print("=== NEGATIVE CONTROL: mask confusion must NOT COMPILE ===")
    rc_bad, out_bad = run(["-DZFH_MASK_CONFUSION=1"])
    print("rc=%d" % rc_bad)
    tail = [l for l in out_bad.split("\n") if l.strip()][:12]
    for l in tail:
        print("  | %s" % l)
    if rc_bad == 0:
        print("FAIL: assigning a RequiredMask to a WindowMask COMPILED. The "
              "type separation is not real, and R101's defect class is back.")
        return 1
    print("OK: the two masks cannot be confused without a compile error.")
    return 0


# ==========================================================================
# main
# ==========================================================================

def main(argv):
    args = argv[1:]
    check = "--check" in args
    if "--compile-fail-control" in args:
        return compile_fail_control()

    # Self-tests FIRST, always, in both modes -- gen_console_board.py's
    # pattern. A generator whose own parsers are untested is a generator that
    # reports reassurance.
    _comment_blindness_self_test()
    _arity_parser_self_test()

    errs = validate_schema()
    if errs:
        print("SCHEMA IS INVALID:")
        for e in errs:
            print("  %s" % e)
        return 2
    print("schema self-check: %d records, %d fields, all contiguous and "
          "naturally aligned"
          % (len(RECORDS), sum(len(r.fields) for r in RECORDS)))
    print("schema-sha256: %s" % schema_sha256())
    print("parser self-tests: comment-blindness OK; arity parser REJECTS the "
          "historical (11) and (14) and ACCEPTS the corrected rows")

    cxx_text = emit_cxx()
    sv_text = emit_sv()

    try:
        spec_existing = read_text(SPEC_OUT)
    except OSError:
        spec_existing = None

    rc = 0
    if check:
        for path, want in ((CXX_OUT, cxx_text), (SV_OUT, sv_text)):
            try:
                have = read_text(path)
            except OSError:
                print("STALE: %s does not exist. Run: python %s"
                      % (path, GEN_REL))
                rc = 1
                continue
            if have != want:
                print("STALE: %s does not match the generator. Run: python %s"
                      % (path, GEN_REL))
                rc = 1
        if spec_existing is None:
            print("STALE: %s does not exist." % SPEC_OUT)
            rc = 1
        else:
            try:
                if splice_spec(spec_existing) != spec_existing:
                    print("STALE: %s generated region does not match. Run: "
                          "python %s" % (SPEC_OUT, GEN_REL))
                    rc = 1
            except ValueError as exc:
                print("REFUSED: %s" % exc)
                rc = 2
        if rc == 0:
            print("fresh: all three generated artifacts match the schema")
    else:
        write_atomic(CXX_OUT, cxx_text)
        write_atomic(SV_OUT, sv_text)
        print("wrote %s" % os.path.relpath(CXX_OUT, ROOT))
        print("wrote %s" % os.path.relpath(SV_OUT, ROOT))
        if spec_existing is None:
            print("NOTE: %s does not exist yet; author it with the two "
                  "markers and re-run." % os.path.relpath(SPEC_OUT, ROOT))
        else:
            write_atomic(SPEC_OUT, splice_spec(spec_existing))
            print("wrote %s (generated region)"
                  % os.path.relpath(SPEC_OUT, ROOT))

    # Cross-checks run in BOTH modes: they are about the tree, not about
    # freshness, and a write that leaves the tree inconsistent is not a
    # success.
    problems = []

    d = check_cxx_sv_parity()
    if d:
        print("C++/SV OFFSET PARITY FAILED:")
        for e in d:
            print("  %s" % e)
        problems.extend(d)

    e_errs, _notes = check_ops_parity()
    if e_errs:
        print("FH21 OPCODE-SHAPE PARITY FAILED:")
        for e in e_errs:
            print("  %s" % e)
        problems.extend(e_errs)

    f_errs = check_field_ir_arity()
    if f_errs:
        print("PROFILE ARITY vs spec/form/field-ir.md FAILED:")
        for e in f_errs:
            print("  %s" % e)
        problems.extend(f_errs)
    else:
        print("profile arity: %d profiles agree with field-ir.md 7.1, and "
              "every row's parenthetical equals the lanes it lists"
              % len(PROFILES))

    g_errs = check_coverage_probe_copy()
    if g_errs:
        print("PROFILE ARITY vs zprog_output_coverage.py FAILED:")
        for e in g_errs:
            print("  %s" % e)
        problems.extend(g_errs)
    else:
        print("profile arity: zprog_output_coverage.py's PROFILE_OUTPUTS "
              "agrees (the fourth copy of this table)")

    if problems:
        return 1
    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
