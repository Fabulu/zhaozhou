#!/usr/bin/env python3
"""Fail-closed Texture-V3 interface source, elaboration, and manifest model.

Schema v1 deliberately keeps two independent interface views.  This module parses
an ANSI SystemVerilog declaration without consulting Verilator, and separately
reads Verilator's elaborated JSON tree.  A manifest is acceptable only when the
two views agree exactly and its stored bytes are the canonical JSON encoding.
"""

from __future__ import annotations

import argparse
import ast
import copy
from dataclasses import dataclass
import hashlib
import json
import math
import operator
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from types import MappingProxyType
import unicodedata
from typing import Iterable, Mapping, Sequence


PARSER_VERSION = "1.0.0"
SCHEMA_ID = "zhao.texture.interface"
SCHEMA_VERSION = 1
PRODUCTION_TOP = "zhao_texture_island_v3_top"
SCHEMA_FIXTURE_TOP = "zhao_texture_interface_schema_fixture"
SCHEMA_FIXTURE_PURPOSE = "schema_fixture"
PRODUCTION_INTERFACE_PURPOSE = "production_interface"
SUPPORTED_VERILATOR_VERSION = (
    "Verilator 5.051 devel rev v5.050-176-g7da43d830 (mod)"
)
VERILATOR_JSON_SCHEMA_ID = "verilator-5.051-tree-json-v1"
KNOWN_DUPLICATE_PACKAGE_PATH = "fpga/rtl/common/zhao_render_texture_pkg.sv"
SUPPORTED_DUPLICATE_PACKAGE_SHA256 = "54f7a8399b02634f5cf0fb892fa271ead317406fe8f35caa8cbd80694ff9c162"
SUPPORTED_DUPLICATE_PROFILES = {
    (SCHEMA_FIXTURE_TOP, SCHEMA_FIXTURE_PURPOSE): {
        "count": 41,
        "sha256": "28a1106e736abe08b092f060bbb9cefbc323b92b530cfa854bf250fac1cb166a",
    },
    # RE-PINNED 2026-09-18, for the binding banks' move into M10K.
    #
    # `zhao_texture_binding_resolver_v2` gained per-bank read ports so its two
    # 256-entry page tables would infer as block RAM instead of 38,400
    # flip-flops. That is INTERNAL to a leaf -- no port, parameter or
    # elaboration value of the island changed -- and the count stayed at 105.
    #
    # This is the family of movement the serialiser's own docstring describes:
    # the fingerprint is sensitive to how members group under their parents,
    # and adding declarations to a module in the closure moves that grouping
    # without changing any member's name or location. It records having been
    # re-pinned three times in one session for exactly this.
    #
    # Refreshed only after field-diffing the regenerated manifest against the
    # committed one and confirming `canonical_interface` is unchanged -- the
    # same evidence the CURRENT-hash refresh of 2026-09-16 recorded, and the
    # difference between refreshing a derived fingerprint and quietly moving a
    # frozen one.
    # STILL 105 after the 2026-09-18 legality-bit and CRC-verdict changes, and
    # that is a designed outcome rather than luck.
    #
    # The stored word gained a legality bit. Written as
    # `struct packed { logic legal; binding_row_t row; }` it took the count to
    # 107: a packed struct emits one MEMBERDTYPE per member, and both `legal`
    # and `row` already occur elsewhere in the closure, so both became duplicate
    # markers. Verified by replaying the manifest's own `elaboration.argv` and
    # listing them --
    #
    #     legal | z,293:19,293:24 | /miscsp/0/typesp/276/membersp/0
    #     row   | z,294:19,294:22 | /miscsp/0/typesp/276/membersp/1
    #
    # -- and 107 - 105 was fully accounted for by those two lines.
    #
    # It is a packed VECTOR instead. This fingerprint exists to show the
    # ISLAND's schema did not move; spending two markers on leaf-internal member
    # names makes it permanently noisier and would have forced a re-derivation
    # of the independent oracle in tests/tools, whose own comment warns that
    # fitting its remap to a target digest is the one thing it must never do.
    # The resolver already speaks in packed vectors with a cast, so nothing was
    # given up.
    # RE-PINNED 2026-09-25 (TERRAINAUX), for `zhao_texture_sheetmod`.
    #
    # The island gained ONE instance and one per-owner record array, and the
    # leaf joined the source closure ahead of the root. That is the exact
    # family the 2026-09-18 note above describes -- "adding declarations to a
    # module in the closure moves that grouping without changing any member's
    # name or location" -- and THE COUNT STAYED AT 105, which is what says the
    # duplicate-name exception set itself did not move.
    #
    # Refreshed only after field-diffing the regenerated manifest against the
    # committed one, and the diff is stated exactly rather than summarised as
    # "unchanged", because two of the four hashes DID move and both had to:
    #
    #   ports                      IDENTICAL, all 120
    #   parameters                 IDENTICAL
    #   module_declaration_sha256  IDENTICAL  <- the interface did not move
    #   top_source_sha256          MOVED      <- the island's BODY gained
    #                                            `u_sheetmod` and `sheet_m`
    #   canonical_interface_sha256 MOVED      <- it hashes the source closure
    #                                            and the argv, and the closure
    #                                            gained one leaf
    #
    # So the island's INTERFACE is byte-identical and the two that moved are
    # the two that describe what was deliberately changed. That is the
    # difference between refreshing a derived fingerprint and quietly moving
    # a frozen one; a note claiming all four were unchanged would have been
    # the reassuring kind of wrong this file exists to prevent.
    (PRODUCTION_TOP, PRODUCTION_INTERFACE_PURPOSE): {
        "count": 105,
        "sha256": "a95970fe412b1cb3f054b5749633071a1eb7b15f60c1427ec47f88878885cfec",
    },
}
SUPPORTED_DTYPE_KINDS = frozenset({"BASICDTYPE"})
PRODUCTION_MANIFEST = "fpga/rtl/generated/zhao_texture_island_v3_top.interface.json"

PRODUCTION_SOURCE_CLOSURE = (
    "fpga/rtl/common/zhao_render_texture_pkg.sv",
    "fpga/rtl/field/zhao_field_rcp24_rom.sv",
    "fpga/rtl/raster/zhao_raster_ticketq.sv",
    "fpga/rtl/raster/zhao_raster_ticketq_rh.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_mul.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_v4.sv",
    "fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_mod255.sv",
    "fpga/rtl/texture/zhao_texture_aux_div6.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv",
    "fpga/rtl/texture/zhao_texture_mosaic_v2.sv",
    "fpga/rtl/texture/zhao_texture_palette_res_v2.sv",
    "fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv",
    "fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_v3bank.sv",
    "fpga/rtl/texture/zhao_texture_v3rq.sv",
    "fpga/rtl/texture/zhao_texture_v3own.sv",
    "fpga/rtl/texture/zhao_texture_metajoin_v2.sv",
    "fpga/rtl/texture/zhao_texture_uv_join_v2.sv",
    "fpga/rtl/texture/zhao_texture_early_desc_v2.sv",
    "fpga/rtl/texture/zhao_texture_frag_expand_v2.sv",
    "fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv",
    "fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv",
    "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_material_combine_v3.sv",
    # TEXTURE.SHEETMOD, 2026-09-25 (TERRAINAUX): the surface sheet's visible
    # effect, a LEAF of the selected root, so it precedes it.
    "fpga/rtl/texture/zhao_texture_sheetmod.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
)

PRODUCTION_PARAMETER_VALUES = {
    "MIGRATION_SHADOWS": ("bit_vector", "1'h1"),
    "DEPTH": ("unsigned_integer", "16"),
    "CTXW": ("unsigned_integer", "64"),
    "RCTXW": ("unsigned_integer", "160"),
    "AUXCTXW": ("unsigned_integer", "224"),
    "BINDW": ("unsigned_integer", "8"),
    "LODW": ("unsigned_integer", "8"),
    "GENW": ("unsigned_integer", "8"),
    "LANES": ("unsigned_integer", "4"),
    "SRCW": ("unsigned_integer", "18"),
    "DATAW": ("unsigned_integer", "64"),
    "TOKW": ("unsigned_integer", "18"),
    "AUX_TOKW": ("unsigned_integer", "14"),
    "PAL_SLOTS": ("unsigned_integer", "4"),
    "PAL_ENTRIES": ("unsigned_integer", "256"),
    "BILERP_DSP2": ("bit_vector", "1'h0"),
}

CUSTOM_TOOL_SPECS = {
    "checker": {
        "name": "check_texture_v3_interface_manifest",
        "path": "tools/rtl/check_texture_v3_interface_manifest.py",
        "version": "1.0.0",
    },
    "generator": {
        "name": "gen_texture_v3_interface_manifest",
        "path": "tools/rtl/gen_texture_v3_interface_manifest.py",
        "version": "1.0.0",
    },
    "parser": {
        "name": "texture_v3_interface_parser",
        "path": "tools/rtl/texture_v3_interface_parser.py",
        "version": PARSER_VERSION,
    },
}

# The section-4.5 source map is pinned here rather than inferred from names.  A
# new signal must be deliberately added to both this map and the Boolean law.
QUIET_SOURCE_MAP = {
    "q_owner_idle": "own_ev_quiet_w",
    "q_rcp_idle": "rcp_idle_w",
    "q_persp_idle": "persp_idle_w",
    "q_metajoin_idle": "metajoin_idle_w",
    "q_desc_idle": "desc_idle_w",
    "q_uvjoin_idle": "uvjoin_idle_w",
    "q_expand_idle": "expand_idle_w",
    "q_bind_idle": "binding_data_idle_w",
    "q_plan_idle": "plan_idle_w",
    "q_cache_idle": "cache_idle_w",
    "q_dispatch_idle": "dispatch_idle_w",
    "q_mosaic_idle": "mosaic_idle_w",
    "q_bilerp_idle": "&bilerp_lane_idle_w[3:0]",
    "q_palette_idle": "palette_idle_w",
    "q_palette_cfg_idle": "palette_cfg_idle_w",
    "q_aux_idle": "aux_idle_w",
    "q_combine_idle": "material_read_idle_w && combine_leaf_idle_w",
    "q_frag_offer_valid": "frag_valid_i",
    "q_owner_claim_valid": "owner_claim_valid_w",
    "q_owner_ready_valid": "owner_ready_valid_w",
    "q_owner_combine_valid": "owner_combine_valid_w",
    "q_owner_final_valid": "owner_final_valid_w",
    "q_rcp_req_valid": "rcp_req_valid_w",
    # TIMING4 R1T added a registered reciprocal head between the response and
    # perspective prep. A record parked in that register is work in flight, so
    # quiet must account for it or the island can report itself quiet while
    # holding one.
    "q_rcp_rsp_valid": "rcp_rsp_valid_w || rcp_head_valid_q",
    "q_persp_req_valid": "persp_req_valid_w",
    "q_persp_rsp_valid": "persp_rsp_valid_w",
    "q_metajoin_a_valid": "metajoin_a_valid_w",
    "q_metajoin_b_valid": "metajoin_b_valid_w",
    "q_metajoin_rsp_valid": "metajoin_rsp_valid_w",
    "q_desc_req_valid": "desc_req_valid_w",
    "q_desc_rsp_valid": "desc_rsp_valid_w",
    "q_uvjoin_desc_valid": "uvjoin_desc_valid_w",
    "q_uvjoin_uv_valid": "uvjoin_uv_valid_w",
    "q_uvjoin_rsp_valid": "uvjoin_rsp_valid_w",
    "q_expand_frag_valid": "expand_frag_valid_w",
    "q_expand_sample_valid": "expand_sample_valid_w",
    "q_expand_aux_valid": "expand_aux_valid_w",
    "q_bind_req_valid": "binding_req_valid_w",
    "q_bind_plan_valid": "binding_plan_valid_w",
    "q_bind_refuse_valid": "binding_refuse_valid_w",
    "q_plan_req_valid": "plan_req_valid_w",
    "q_plan_cache_valid": "plan_cache_valid_w",
    "q_cache_req_valid": "cache_req_valid_w",
    "q_fill_req_valid": "fill_req_valid_o",
    "q_fill_rsp_valid": "fill_data_valid_i || fill_refused_i",
    "q_cache_rsp_valid": "cache_rsp_valid_w",
    "q_dispatch_req_valid": "|class_terminal_offer_valid_w[3:0]",
    "q_class_rsp_valid": "dispatch_pending_w[3:0]",
    "q_mosaic_req_valid": "mosaic_req_valid_w",
    "q_mosaic_rsp_valid": "mosaic_rsp_valid_w",
    "q_bilerp_req_valid": "bilerp_req_valid_w[3:0]",
    "q_bilerp_rsp_valid": "bilerp_rsp_valid_w[3:0]",
    "q_palette_req_valid": "palette_req_valid_w",
    "q_palette_rsp_valid": "palette_rsp_valid_w",
    "q_palette_cfg_valid": "pal_load_valid_i",
    "q_palette_cfg_rsp_valid": "1'b0",
    "q_tmu_return_valid": "dispatch_return_valid_w",
    "q_aux_req_valid": "aux_job_valid_w",
    "q_sheet_req_valid": "sheet_req_valid_w",
    "q_sheet_rsp_owed": "aux_sheet_rsp_owed_w",
    "q_sheet_rsp_valid": "pg_valid_i",
    "q_aux_refuse_valid": "aux_refuse_valid_w",
    "q_aux_return_valid": "aux_return_valid_w",
    "q_combine_req_valid": "combine_req_valid_w",
    "q_combine_rsp_valid": "combine_rsp_valid_w",
    "q_retire_valid": "out_valid_o",
    "q_cfg_cmd_valid": "cfg_valid_i",
    "q_cfg_rsp_valid": "cfg_rsp_valid_o",
}

QUIET_AUTHORIZED_EXPRESSION_ALIASES = {
    "q_bilerp_idle": "&bilerp_lane_idle_w[3:0]",
    "q_fill_rsp_valid": "fill_data_valid_i || fill_refused_i",
    "q_dispatch_req_valid": "|class_terminal_offer_valid_w[3:0]",
    "q_palette_cfg_rsp_valid": "1'b0",
    "q_combine_idle": "material_read_idle_w && combine_leaf_idle_w",
}
QUIET_COMBINE_PHYSICAL_LEAVES = (
    "material_read_idle_w",
    "combine_leaf_idle_w",
)

QUIET_CONTROL_SOURCE_MAP = {
    "cfg_loader_idle": "binding_cfg_loader_idle_w",
    "binding_crc_busy": "binding_crc_busy_w",
    "binding_seal_pending": "binding_seal_pending_w",
}

_DATA_QUIET_POSITIVE = (
    "q_owner_idle", "q_rcp_idle", "q_persp_idle", "q_metajoin_idle",
    "q_desc_idle", "q_uvjoin_idle", "q_expand_idle", "q_bind_idle",
    "q_plan_idle", "q_cache_idle", "q_dispatch_idle", "q_mosaic_idle",
    "q_bilerp_idle", "q_palette_idle", "q_aux_idle", "q_combine_idle",
)
_DATA_QUIET_NEGATIVE = (
    "q_owner_claim_valid", "q_owner_ready_valid", "q_owner_combine_valid",
    "q_owner_final_valid", "q_rcp_req_valid", "q_rcp_rsp_valid",
    "q_persp_req_valid", "q_persp_rsp_valid", "q_metajoin_a_valid",
    "q_metajoin_b_valid", "q_metajoin_rsp_valid", "q_desc_req_valid",
    "q_desc_rsp_valid", "q_uvjoin_desc_valid", "q_uvjoin_uv_valid",
    "q_uvjoin_rsp_valid", "q_expand_frag_valid", "q_expand_sample_valid",
    "q_expand_aux_valid", "q_bind_req_valid", "q_bind_plan_valid",
    "q_bind_refuse_valid", "q_plan_req_valid", "q_plan_cache_valid",
    "q_cache_req_valid", "q_fill_req_valid", "q_cache_rsp_valid",
    "q_dispatch_req_valid", "q_class_rsp_valid[3:0]", "q_mosaic_req_valid",
    "q_mosaic_rsp_valid", "q_bilerp_req_valid[3:0]",
    "q_bilerp_rsp_valid[3:0]", "q_palette_req_valid",
    "q_palette_rsp_valid", "q_tmu_return_valid", "q_aux_req_valid",
    "q_sheet_req_valid", "q_sheet_rsp_owed", "q_aux_refuse_valid",
    "q_aux_return_valid", "q_combine_req_valid", "q_combine_rsp_valid",
    "q_retire_valid",
)
_PUBLIC_QUIET_POSITIVE = ("data_quiet", "cfg_loader_idle", "q_palette_cfg_idle")
_PUBLIC_QUIET_NEGATIVE = (
    "binding_crc_busy", "binding_seal_pending", "q_frag_offer_valid",
    "q_fill_rsp_valid", "q_sheet_rsp_valid", "q_palette_cfg_valid",
    "q_palette_cfg_rsp_valid", "q_cfg_cmd_valid", "q_cfg_rsp_valid",
)
_VECTOR_REDUCTION_OPERANDS = {
    "q_class_rsp_valid[3:0]",
    "q_bilerp_req_valid[3:0]",
    "q_bilerp_rsp_valid[3:0]",
}


class InterfaceManifestError(ValueError):
    """The source, elaboration, or manifest violates the closed v1 contract."""


@dataclass(frozen=True)
class DimensionView:
    source_expression: str
    left: int
    right: int

    @property
    def size(self) -> int:
        return abs(self.left - self.right) + 1

    @property
    def direction(self) -> str:
        return "ascending" if self.left < self.right else "descending"

    def manifest(self) -> dict[str, object]:
        return {
            "direction": self.direction,
            "left": self.left,
            "right": self.right,
            "size": self.size,
            "source_expression": self.source_expression,
        }


@dataclass(frozen=True)
class ValueView:
    kind: str
    text: str

    def manifest(self) -> dict[str, str]:
        return {"kind": self.kind, "text": self.text}


@dataclass(frozen=True)
class ParameterView:
    ordinal: int
    name: str
    declared_kind: str
    declared_type: str
    source_default_expression: str
    selected_value: ValueView

    def manifest(self) -> dict[str, object]:
        return {
            "declared_kind": self.declared_kind,
            "declared_type": self.declared_type,
            "name": self.name,
            "ordinal": self.ordinal,
            "selected_value": self.selected_value.manifest(),
            "source_default_expression": self.source_default_expression,
        }


@dataclass(frozen=True)
class PortView:
    ordinal: int
    name: str
    direction: str
    declared_type: str
    net_or_var: str
    signed: bool
    source_expression: str
    packed_dimensions: tuple[DimensionView, ...]
    unpacked_dimensions: tuple[DimensionView, ...]

    @property
    def element_width(self) -> int:
        return math.prod(dim.size for dim in self.packed_dimensions) if self.packed_dimensions else 1

    @property
    def unpacked_element_count(self) -> int:
        return (
            math.prod(dim.size for dim in self.unpacked_dimensions)
            if self.unpacked_dimensions else 1
        )

    @property
    def bit_width(self) -> int:
        return self.element_width * self.unpacked_element_count

    def manifest(self) -> dict[str, object]:
        return {
            "bit_width": self.bit_width,
            "declared_type": self.declared_type,
            "direction": self.direction,
            "element_width": self.element_width,
            "name": self.name,
            "net_or_var": self.net_or_var,
            "ordinal": self.ordinal,
            "packed_dimensions": [dim.manifest() for dim in self.packed_dimensions],
            "signed": self.signed,
            "source_expression": self.source_expression,
            "unpacked_dimensions": [dim.manifest() for dim in self.unpacked_dimensions],
            "unpacked_element_count": self.unpacked_element_count,
        }


@dataclass(frozen=True)
class ModuleSourceView:
    name: str
    source_path: str
    declaration_start_byte: int
    declaration_end_byte_exclusive: int
    module_declaration_sha256: str
    parameters: tuple[ParameterView, ...]
    ports: tuple[PortView, ...]


@dataclass(frozen=True)
class ElaboratedParameter:
    ordinal: int
    name: str
    value: int | str
    bit_width: int
    signed: bool
    dtype_kind: str


@dataclass(frozen=True)
class ElaboratedPort:
    ordinal: int
    name: str
    direction: str
    net_or_var: str
    bit_width: int
    element_width: int
    signed: bool
    packed_ranges: tuple[tuple[int, int], ...]
    unpacked_ranges: tuple[tuple[int, int], ...]
    dtype_kind: str


@dataclass(frozen=True)
class ElaborationView:
    parameters: tuple[ElaboratedParameter, ...]
    ports: tuple[ElaboratedPort, ...]
    module_source_paths: frozenset[str]


@dataclass(frozen=True)
class FileSnapshot:
    repo_path: str
    absolute_path: Path
    raw: bytes
    sha256: str


@dataclass(frozen=True)
class SnapshotSet:
    files: Mapping[str, FileSnapshot]

    @classmethod
    def capture(cls, repo_root: Path, repo_paths: Sequence[str]) -> "SnapshotSet":
        captured: dict[str, FileSnapshot] = {}
        for ordinal, value in enumerate(repo_paths):
            repo_path = validate_repo_path(value, f"snapshot path {ordinal}")
            if repo_path in captured:
                continue
            absolute = repo_file_path(repo_root, repo_path, f"snapshot path {ordinal}")
            raw = absolute.read_bytes()
            captured[repo_path] = FileSnapshot(
                repo_path=repo_path,
                absolute_path=absolute,
                raw=raw,
                sha256=sha256_bytes(raw),
            )
        return cls(MappingProxyType(captured))

    def __getitem__(self, repo_path: str) -> FileSnapshot:
        try:
            return self.files[repo_path]
        except KeyError as exc:
            raise InterfaceManifestError(
                f"path {repo_path!r} was not part of the immutable snapshot"
            ) from exc

    def verify_live_unchanged(self) -> None:
        changed: list[str] = []
        for repo_path, snapshot in self.files.items():
            try:
                live = snapshot.absolute_path.read_bytes()
            except OSError as exc:
                raise InterfaceManifestError(
                    f"snapshot recheck could not read {repo_path}: {exc}"
                ) from exc
            if live != snapshot.raw:
                changed.append(repo_path)
        if changed:
            raise InterfaceManifestError(
                "snapshot changed before manifest emit/accept: " + ", ".join(changed)
            )


@dataclass(frozen=True)
class ManifestBuild:
    payload: dict[str, object]
    snapshots: SnapshotSet

    def verify_live_unchanged(self) -> None:
        self.snapshots.verify_live_unchanged()


_IDENTIFIER_RE = re.compile(r"[A-Za-z_$][A-Za-z0-9_$]*")
_HASH_RE = re.compile(r"[0-9a-f]{64}")
_UNSIGNED_DECIMAL_RE = re.compile(r"0|[1-9][0-9]*")
_SIGNED_DECIMAL_RE = re.compile(r"0|-?[1-9][0-9]*")
_BIT_VECTOR_RE = re.compile(r"([1-9][0-9]*)'h([0-9a-f]+)")
_PATH_SEGMENT_RE = re.compile(r"[^/]+")
_NET_WORDS = {
    "wire", "tri", "tri0", "tri1", "supply0", "supply1", "wand",
    "triand", "wor", "trior", "uwire",
}
_VAR_WORDS = {"var"}
_SIGN_WORDS = {"signed", "unsigned"}
_INTEGER_TYPES = {"byte", "shortint", "int", "integer", "longint", "time"}
_SIGNED_DEFAULT_TYPES = {"byte", "shortint", "int", "integer", "longint"}
_FIXED_INTEGER_WIDTHS = {
    "byte": 8,
    "shortint": 16,
    "int": 32,
    "integer": 32,
    "longint": 64,
    "time": 64,
}


_BIN_OPS = {
    ast.Add: operator.add,
    ast.Sub: operator.sub,
    ast.Mult: operator.mul,
    ast.FloorDiv: operator.floordiv,
    ast.Mod: operator.mod,
    ast.LShift: operator.lshift,
    ast.RShift: operator.rshift,
    ast.BitOr: operator.or_,
    ast.BitAnd: operator.and_,
    ast.BitXor: operator.xor,
    ast.Pow: operator.pow,
}
_UNARY_OPS = {ast.UAdd: operator.pos, ast.USub: operator.neg, ast.Invert: operator.invert}


def sha256_bytes(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _lf(text: str) -> str:
    return text.replace("\r\n", "\n").replace("\r", "\n")


def _mask_comments(text: str) -> str:
    """Replace comments with spaces while preserving character offsets/newlines."""
    output = list(text)
    i = 0
    state = "code"
    while i < len(text):
        char = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if state == "code":
            if char == '"':
                state = "string"
                i += 1
            elif char == "/" and nxt == "/":
                output[i] = output[i + 1] = " "
                state = "line"
                i += 2
            elif char == "/" and nxt == "*":
                output[i] = output[i + 1] = " "
                state = "block"
                i += 2
            else:
                i += 1
        elif state == "string":
            if char == "\\":
                i += 2
            elif char == '"':
                state = "code"
                i += 1
            else:
                i += 1
        elif state == "line":
            if char == "\n":
                state = "code"
            else:
                output[i] = " "
            i += 1
        else:
            if char == "*" and nxt == "/":
                output[i] = output[i + 1] = " "
                state = "code"
                i += 2
            else:
                if char not in "\r\n":
                    output[i] = " "
                i += 1
    if state == "block":
        raise InterfaceManifestError("unterminated block comment")
    if state == "string":
        raise InterfaceManifestError("unterminated SystemVerilog string")
    return "".join(output)


def _skip_space(text: str, offset: int) -> int:
    while offset < len(text) and text[offset].isspace():
        offset += 1
    return offset


def _balanced_end(text: str, opening_at: int, opening: str, closing: str) -> int:
    if opening_at >= len(text) or text[opening_at] != opening:
        raise InterfaceManifestError(f"expected {opening!r} at offset {opening_at}")
    depth = 0
    quote = False
    i = opening_at
    while i < len(text):
        char = text[i]
        if quote:
            if char == "\\":
                i += 2
                continue
            if char == '"':
                quote = False
            i += 1
            continue
        if char == '"':
            quote = True
        elif char == opening:
            depth += 1
        elif char == closing:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise InterfaceManifestError(f"unterminated {opening}{closing} group")


def _split_spans(text: str, start: int, end: int, separator: str = ",") -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    quote = False
    escaped = False
    item_start = start
    for i in range(start, end):
        char = text[i]
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quote = False
            continue
        if char == '"':
            quote = True
        elif char in "([{":
            stack.append(char)
        elif char in ")]}":
            if not stack or stack[-1] != pairs[char]:
                raise InterfaceManifestError(f"unbalanced {char!r} in declaration")
            stack.pop()
        elif char == separator and not stack:
            spans.append((item_start, i))
            item_start = i + 1
    if quote or stack:
        raise InterfaceManifestError("unbalanced group or string in declaration")
    spans.append((item_start, end))
    return spans


def _code_trim_span(masked: str, start: int, end: int) -> tuple[int, int]:
    while start < end and masked[start].isspace():
        start += 1
    while end > start and masked[end - 1].isspace():
        end -= 1
    return start, end


def _top_level_equal(text: str) -> int:
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    quote = False
    escaped = False
    positions: list[int] = []
    for index, char in enumerate(text):
        if quote:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quote = False
            continue
        if char == '"':
            quote = True
        elif char in "([{":
            stack.append(char)
        elif char in ")]}":
            if not stack or stack[-1] != pairs[char]:
                raise InterfaceManifestError(f"unbalanced {char!r} in declaration")
            stack.pop()
        elif char == "=" and not stack:
            previous = text[index - 1] if index else ""
            following = text[index + 1] if index + 1 < len(text) else ""
            if previous not in "=!<>" and following not in "=>":
                positions.append(index)
    if quote or stack:
        raise InterfaceManifestError(f"unbalanced group or string in {text!r}")
    if len(positions) != 1:
        raise InterfaceManifestError(f"expected one top-level assignment '=' in {text!r}")
    return positions[0]


def _top_level_identifiers(text: str) -> list[tuple[str, int, int]]:
    identifiers: list[tuple[str, int, int]] = []
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    quote = False
    i = 0
    while i < len(text):
        char = text[i]
        if quote:
            if char == "\\":
                i += 2
                continue
            if char == '"':
                quote = False
            i += 1
            continue
        if char == '"':
            quote = True
            i += 1
        elif char in "([{":
            stack.append(char)
            i += 1
        elif char in ")]}":
            if not stack or stack[-1] != pairs[char]:
                raise InterfaceManifestError(f"unbalanced {char!r} in declaration")
            stack.pop()
            i += 1
        elif not stack:
            match = _IDENTIFIER_RE.match(text, i)
            if match:
                identifiers.append((match.group(0), match.start(), match.end()))
                i = match.end()
            else:
                i += 1
        else:
            i += 1
    return identifiers


def _dimension_spans(text: str) -> list[tuple[int, int]]:
    spans: list[tuple[int, int]] = []
    i = 0
    quote = False
    while i < len(text):
        char = text[i]
        if quote:
            if char == "\\":
                i += 2
                continue
            if char == '"':
                quote = False
            i += 1
        elif char == '"':
            quote = True
            i += 1
        elif char == "[":
            close = _balanced_end(text, i, "[", "]")
            spans.append((i, close + 1))
            i = close + 1
        else:
            i += 1
    return spans


def _replace_sv_numbers(expression: str) -> str:
    pattern = re.compile(r"(?:(\d+)\s*)?'([sS]?)([dDhHbBoO])([0-9a-fA-F_xXzZ?]+)")

    def replace(match: re.Match[str]) -> str:
        width_text = match.group(1)
        signed = bool(match.group(2))
        digits = match.group(4).replace("_", "")
        if re.search(r"[xXzZ?]", digits):
            raise InterfaceManifestError(
                f"unknown digit in constant expression {expression!r}"
            )
        base = {"d": 10, "h": 16, "b": 2, "o": 8}[match.group(3).lower()]
        value = int(digits, base)
        if width_text is not None:
            width = int(width_text)
            if width <= 0:
                raise InterfaceManifestError(
                    f"nonpositive literal width in constant expression {expression!r}"
                )
            value &= (1 << width) - 1
            if signed and (value & (1 << (width - 1))):
                value -= 1 << width
        elif signed:
            raise InterfaceManifestError(
                f"unsized signed literal is unsupported in {expression!r}"
            )
        return str(value)

    return pattern.sub(replace, expression.replace("$clog2", "clog2"))


def eval_sv_integer(expression: str, constants: Mapping[str, int] | None = None) -> int:
    constants = constants or {}
    evaluation_constants = dict(constants)
    qualified_placeholders: dict[str, str] = {}

    def replace_qualified(match: re.Match[str]) -> str:
        qualified = match.group(0)
        if qualified not in constants:
            raise InterfaceManifestError(
                f"unknown qualified integral constant {qualified!r} in {expression!r}"
            )
        placeholder = qualified_placeholders.setdefault(
            qualified, f"__pkg_constant_{len(qualified_placeholders)}"
        )
        evaluation_constants[placeholder] = int(constants[qualified])
        return placeholder

    qualified_re = re.compile(
        r"[A-Za-z_$][A-Za-z0-9_$]*::[A-Za-z_$][A-Za-z0-9_$]*"
    )
    rewritten = qualified_re.sub(replace_qualified, expression)
    rewritten = _replace_sv_numbers(rewritten)
    # Python parses / as true division.  The interface expressions use integral
    # values; converting it to // keeps the evaluator integer-only and fail-closed.
    rewritten = re.sub(r"(?<!/)/(?!/)", "//", rewritten)
    try:
        tree = ast.parse(rewritten, mode="eval")
    except SyntaxError as exc:
        raise InterfaceManifestError(
            f"unsupported constant expression {expression!r}"
        ) from exc

    def visit(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return visit(node.body)
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return int(node.value)
        if isinstance(node, ast.Name):
            if node.id not in evaluation_constants:
                raise InterfaceManifestError(
                    f"unresolved identifier {node.id!r} in {expression!r}"
                )
            return int(evaluation_constants[node.id])
        if isinstance(node, ast.BinOp) and type(node.op) in _BIN_OPS:
            return int(_BIN_OPS[type(node.op)](visit(node.left), visit(node.right)))
        if isinstance(node, ast.UnaryOp) and type(node.op) in _UNARY_OPS:
            return int(_UNARY_OPS[type(node.op)](visit(node.operand)))
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
            if node.func.id == "clog2" and len(node.args) == 1:
                value = visit(node.args[0])
                if value <= 0:
                    raise InterfaceManifestError("$clog2 argument must be positive")
                return (value - 1).bit_length()
        raise InterfaceManifestError(f"unsupported constant expression {expression!r}")

    return visit(tree)


def _top_level_range_colon(text: str) -> int:
    stack: list[str] = []
    pairs = {")": "(", "]": "[", "}": "{"}
    positions: list[int] = []
    for index, char in enumerate(text):
        if char in "([{":
            stack.append(char)
        elif char in ")]}":
            if not stack or stack[-1] != pairs[char]:
                raise InterfaceManifestError(f"unbalanced {char!r} in dimension")
            stack.pop()
        elif char == ":" and not stack:
            previous = text[index - 1] if index else ""
            following = text[index + 1] if index + 1 < len(text) else ""
            if previous != ":" and following != ":":
                positions.append(index)
    if stack or len(positions) != 1:
        raise InterfaceManifestError(
            f"dimension body {text!r} does not contain one range colon"
        )
    return positions[0]


def _parse_dimension(source_text: str, constants: Mapping[str, int]) -> DimensionView:
    if not source_text.startswith("[") or not source_text.endswith("]"):
        raise InterfaceManifestError(f"malformed dimension {source_text!r}")
    inner = source_text[1:-1]
    colon = _top_level_range_colon(inner)
    left_text = inner[:colon].strip()
    right_text = inner[colon + 1:].strip()
    return DimensionView(
        source_expression=_lf(source_text),
        left=eval_sv_integer(left_text, constants),
        right=eval_sv_integer(right_text, constants),
    )


def _type_without_surface_modifiers(type_text: str) -> str:
    without_dims = re.sub(r"\[[^\]]*\]", " ", type_text)
    words = _IDENTIFIER_RE.findall(without_dims)
    filtered = [
        word for word in words
        if word not in _NET_WORDS | _VAR_WORDS | _SIGN_WORDS
    ]
    if not filtered:
        return "logic"
    if "::" in without_dims:
        result = without_dims
        for word in _NET_WORDS | _VAR_WORDS | _SIGN_WORDS:
            result = re.sub(r"\b" + re.escape(word) + r"\b", " ", result)
        return re.sub(r"\s+", " ", result).strip()
    return filtered[-1]


def _declared_signed(type_text: str, declared_type: str) -> bool:
    words = set(_IDENTIFIER_RE.findall(type_text))
    if "unsigned" in words:
        return False
    if "signed" in words:
        return True
    return declared_type in _SIGNED_DEFAULT_TYPES


def _parameter_shape(
    declared_type: str,
    constants: Mapping[str, int] | None = None,
) -> tuple[str, int | None, bool]:
    constants = constants or {}
    words = set(_IDENTIFIER_RE.findall(declared_type))
    if "string" in words:
        return "string", None, False
    dimensions = _dimension_spans(declared_type)
    base_words = [word for word in _IDENTIFIER_RE.findall(declared_type) if word not in _SIGN_WORDS]
    base = base_words[-1] if base_words else ""
    signed = "signed" in words or ("unsigned" not in words and base in _SIGNED_DEFAULT_TYPES)
    if dimensions or base in {"bit", "logic", "reg"}:
        width = 1
        for start, end in dimensions:
            dim = _parse_dimension(declared_type[start:end], constants)
            width *= dim.size
        return "bit_vector", width, signed
    if base in _INTEGER_TYPES:
        return "signed_integer" if signed else "unsigned_integer", None, signed
    raise InterfaceManifestError(f"unsupported parameter type {declared_type!r}")


def _decode_sv_string(expression: str) -> str:
    expression = expression.strip()
    if len(expression) < 2 or expression[0] != '"' or expression[-1] != '"':
        raise InterfaceManifestError(f"string parameter is not a quoted SV string: {expression!r}")
    body = expression[1:-1]
    result: list[str] = []
    i = 0
    escapes = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"'}
    while i < len(body):
        if body[i] != "\\":
            result.append(body[i])
            i += 1
            continue
        i += 1
        if i >= len(body) or body[i] not in escapes:
            raise InterfaceManifestError(f"unsupported SV string escape in {expression!r}")
        result.append(escapes[body[i]])
        i += 1
    value = "".join(result)
    if unicodedata.normalize("NFC", value) != value:
        raise InterfaceManifestError("string parameter value is not NFC")
    return value


def canonical_parameter_value(
    declared_type: str,
    expression: str,
    constants: Mapping[str, int],
) -> tuple[ValueView, int | str]:
    kind, width, signed = _parameter_shape(declared_type, constants)
    if kind == "string":
        value = _decode_sv_string(expression)
        return ValueView("string", value), value
    value = eval_sv_integer(expression, constants)
    if kind == "bit_vector":
        assert width is not None
        minimum = -(1 << (width - 1)) if signed else 0
        maximum = (1 << width) - 1
        if value < minimum or value > maximum:
            qualifier = "signed" if signed else "unsigned"
            raise InterfaceManifestError(
                f"value {value} does not fit {qualifier} {width}-bit parameter"
            )
        bits = value & maximum
        digits = (width + 3) // 4
        semantic = bits
        if signed and (bits & (1 << (width - 1))):
            semantic -= 1 << width
        return ValueView("bit_vector", f"{width}'h{bits:0{digits}x}"), semantic
    if kind == "unsigned_integer" and value < 0:
        raise InterfaceManifestError("unsigned parameter selected a negative value")
    return ValueView(kind, str(value)), value


def _module_header_spans(masked: str, module_name: str) -> tuple[int, int, int | None, int | None, int, int]:
    matches = list(re.finditer(r"\bmodule\s+" + re.escape(module_name) + r"\b", masked))
    if len(matches) != 1:
        raise InterfaceManifestError(
            f"expected exactly one module {module_name!r}, found {len(matches)}"
        )
    declaration_start = matches[0].start()
    at = matches[0].end()
    while True:
        at = _skip_space(masked, at)
        if not masked.startswith("import", at) or (
            at + 6 < len(masked) and (masked[at + 6].isalnum() or masked[at + 6] in "_$")
        ):
            break
        semi = masked.find(";", at)
        if semi < 0:
            raise InterfaceManifestError("unterminated module-header import")
        at = semi + 1
    parameter_start: int | None = None
    parameter_end: int | None = None
    at = _skip_space(masked, at)
    if at < len(masked) and masked[at] == "#":
        at = _skip_space(masked, at + 1)
        if at >= len(masked) or masked[at] != "(":
            raise InterfaceManifestError("malformed module parameter list")
        parameter_start = at + 1
        close = _balanced_end(masked, at, "(", ")")
        parameter_end = close
        at = close + 1
    at = _skip_space(masked, at)
    if at >= len(masked) or masked[at] != "(":
        raise InterfaceManifestError("module does not have an ANSI port list")
    port_start = at + 1
    close = _balanced_end(masked, at, "(", ")")
    semi = _skip_space(masked, close + 1)
    if semi >= len(masked) or masked[semi] != ";":
        raise InterfaceManifestError("missing semicolon after module declaration")
    return declaration_start, semi + 1, parameter_start, parameter_end, port_start, close


def parse_module_source(
    raw: bytes,
    *,
    source_path: str,
    module_name: str,
    overrides: Mapping[str, str] | None = None,
    qualified_constants: Mapping[str, int] | None = None,
) -> ModuleSourceView:
    if raw.startswith(b"\xef\xbb\xbf"):
        raise InterfaceManifestError(f"{source_path}: UTF-8 BOM is forbidden")
    try:
        source = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise InterfaceManifestError(f"{source_path}: invalid UTF-8") from exc
    if unicodedata.normalize("NFC", source) != source:
        raise InterfaceManifestError(f"{source_path}: source text is not NFC")
    masked = _mask_comments(source)
    (
        declaration_start,
        declaration_end,
        parameter_start,
        parameter_end,
        port_start,
        port_end,
    ) = _module_header_spans(masked, module_name)

    overrides = dict(overrides or {})
    parameters: list[ParameterView] = []
    constants: dict[str, int] = dict(qualified_constants or {})
    inherited_kind: str | None = None
    inherited_type: str | None = None
    if parameter_start is not None and parameter_end is not None:
        for start, end in _split_spans(masked, parameter_start, parameter_end):
            start, end = _code_trim_span(masked, start, end)
            if start == end:
                continue
            clean_fragment = masked[start:end]
            raw_fragment = source[start:end]
            equal = _top_level_equal(clean_fragment)
            left_clean = clean_fragment[:equal]
            ids = _top_level_identifiers(left_clean)
            if not ids:
                raise InterfaceManifestError(f"parameter has no name: {raw_fragment!r}")
            name, name_start, _name_end = ids[-1]
            prefix = left_clean[:name_start]
            kind_match = re.match(r"\s*(parameter|localparam)\b", prefix)
            if kind_match:
                declared_kind = kind_match.group(1)
                type_start = kind_match.end()
                declared_type = _lf(raw_fragment[type_start:name_start]).strip()
                if not declared_type:
                    raise InterfaceManifestError(
                        f"parameter {name!r} has an implicit type; schema v1 requires an explicit type"
                    )
                inherited_kind = declared_kind
                inherited_type = declared_type
            else:
                if inherited_kind is None or inherited_type is None or len(ids) != 1:
                    raise InterfaceManifestError(
                        f"parameter continuation {raw_fragment!r} is ambiguous"
                    )
                declared_kind = inherited_kind
                declared_type = inherited_type
            expression_start = equal + 1
            while expression_start < len(raw_fragment) and clean_fragment[expression_start].isspace():
                expression_start += 1
            expression_end = len(raw_fragment)
            while expression_end > expression_start and clean_fragment[expression_end - 1].isspace():
                expression_end -= 1
            default_expression = _lf(raw_fragment[expression_start:expression_end])
            selected_expression = overrides.pop(name, default_expression)
            selected, value = canonical_parameter_value(
                declared_type, selected_expression, constants
            )
            if isinstance(value, int):
                constants[name] = value
            parameters.append(
                ParameterView(
                    ordinal=len(parameters),
                    name=name,
                    declared_kind=declared_kind,
                    declared_type=declared_type,
                    source_default_expression=default_expression,
                    selected_value=selected,
                )
            )
    if overrides:
        raise InterfaceManifestError(
            "parameter override names are absent from the declaration: "
            + ", ".join(sorted(overrides))
        )

    ports: list[PortView] = []
    inherited: tuple[str, str, str, bool, tuple[DimensionView, ...]] | None = None
    for start, end in _split_spans(masked, port_start, port_end):
        start, end = _code_trim_span(masked, start, end)
        if start == end:
            continue
        clean_fragment = masked[start:end]
        raw_fragment = source[start:end]
        direction_match = re.match(r"(input|output|inout)\b", clean_fragment)
        if direction_match:
            direction = direction_match.group(1)
            remainder_start = direction_match.end()
            remainder_clean = clean_fragment[remainder_start:]
            ids = _top_level_identifiers(remainder_clean)
            if not ids:
                raise InterfaceManifestError(f"port has no name: {raw_fragment!r}")
            name, relative_name_start, relative_name_end = ids[-1]
            name_start = remainder_start + relative_name_start
            name_end = remainder_start + relative_name_end
            type_text = raw_fragment[remainder_start:name_start]
            packed = tuple(
                _parse_dimension(type_text[a:b], constants)
                for a, b in _dimension_spans(type_text)
            )
            declared_type = _type_without_surface_modifiers(type_text)
            signed = _declared_signed(type_text, declared_type)
            words = set(_IDENTIFIER_RE.findall(type_text))
            if words & _VAR_WORDS:
                net_or_var = "variable"
            elif words & _NET_WORDS:
                net_or_var = "net"
            elif direction in {"input", "inout"}:
                net_or_var = "net"
            else:
                # An output with an explicit data type is a variable port.
                net_or_var = "variable" if type_text.strip() else "net"
            inherited = (direction, declared_type, net_or_var, signed, packed)
        else:
            if inherited is None:
                raise InterfaceManifestError(
                    f"port continuation has no inherited declaration: {raw_fragment!r}"
                )
            ids = _top_level_identifiers(clean_fragment)
            if len(ids) != 1:
                raise InterfaceManifestError(f"ambiguous port continuation {raw_fragment!r}")
            name, name_start, name_end = ids[0]
            direction, declared_type, net_or_var, signed, packed = inherited
        suffix_clean = clean_fragment[name_end:]
        suffix_raw = raw_fragment[name_end:]
        unpacked_spans = _dimension_spans(suffix_clean)
        residue = list(suffix_clean)
        for a, b in unpacked_spans:
            residue[a:b] = " " * (b - a)
        if "".join(residue).strip():
            raise InterfaceManifestError(
                f"unsupported text after port {name!r}: {''.join(residue).strip()!r}"
            )
        unpacked = tuple(
            _parse_dimension(suffix_raw[a:b], constants) for a, b in unpacked_spans
        )
        if any(port.name == name for port in ports):
            raise InterfaceManifestError(f"duplicate port name {name!r}")
        ports.append(
            PortView(
                ordinal=len(ports),
                name=name,
                direction=direction,
                declared_type=declared_type,
                net_or_var=net_or_var,
                signed=signed,
                source_expression=_lf(raw_fragment),
                packed_dimensions=packed,
                unpacked_dimensions=unpacked,
            )
        )
    if not ports:
        raise InterfaceManifestError(f"module {module_name!r} has no ports")

    start_byte = len(source[:declaration_start].encode("utf-8"))
    end_byte = len(source[:declaration_end].encode("utf-8"))
    declaration_raw = raw[start_byte:end_byte]
    try:
        declaration_text = declaration_raw.decode("utf-8")
    except UnicodeDecodeError as exc:  # pragma: no cover - whole source already decoded
        raise InterfaceManifestError("module declaration is not UTF-8") from exc
    declaration_hash = sha256_bytes(_lf(declaration_text).encode("utf-8"))
    return ModuleSourceView(
        name=module_name,
        source_path=source_path,
        declaration_start_byte=start_byte,
        declaration_end_byte_exclusive=end_byte,
        module_declaration_sha256=declaration_hash,
        parameters=tuple(parameters),
        ports=tuple(ports),
    )


def canonical_json_bytes(payload: Mapping[str, object]) -> bytes:
    try:
        text = json.dumps(
            payload,
            ensure_ascii=False,
            allow_nan=False,
            sort_keys=True,
            separators=(",", ":"),
        )
    except (TypeError, ValueError) as exc:
        raise InterfaceManifestError(f"payload is not canonical-JSON serializable: {exc}") from exc
    return text.encode("utf-8")


def canonical_interface_sha256(payload: Mapping[str, object]) -> str:
    clone = copy.deepcopy(dict(payload))
    hashes = clone.get("hashes")
    if not isinstance(hashes, dict) or "canonical_interface_sha256" not in hashes:
        raise InterfaceManifestError("payload has no canonical digest member to omit")
    del hashes["canonical_interface_sha256"]
    return sha256_bytes(canonical_json_bytes(clone))


class _DuplicateKey(Exception):
    pass


def _strict_pairs(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateKey(key)
        result[key] = value
    return result


def load_manifest_bytes(raw: bytes) -> dict[str, object]:
    if raw.startswith(b"\xef\xbb\xbf"):
        raise InterfaceManifestError("manifest UTF-8 BOM is forbidden")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise InterfaceManifestError("manifest is not valid UTF-8") from exc

    def reject_float(value: str) -> object:
        raise InterfaceManifestError(f"floating-point JSON number is forbidden: {value}")

    def reject_constant(value: str) -> object:
        raise InterfaceManifestError(f"non-finite JSON number is forbidden: {value}")

    try:
        payload = json.loads(
            text,
            object_pairs_hook=_strict_pairs,
            parse_float=reject_float,
            parse_constant=reject_constant,
        )
    except _DuplicateKey as exc:
        raise InterfaceManifestError(f"duplicate JSON object key {exc.args[0]!r}") from exc
    except json.JSONDecodeError as exc:
        raise InterfaceManifestError(f"invalid manifest JSON: {exc}") from exc
    if not isinstance(payload, dict):
        raise InterfaceManifestError("manifest root is not an object")
    return payload


def _exact_keys(value: object, keys: set[str], label: str) -> dict[str, object]:
    if not isinstance(value, dict):
        raise InterfaceManifestError(f"{label} must be an object")
    actual = set(value)
    if actual != keys:
        raise InterfaceManifestError(
            f"{label} members differ: missing={sorted(keys - actual)}, extra={sorted(actual - keys)}"
        )
    return value


def _string(value: object, label: str, *, nonempty: bool = False) -> str:
    if not isinstance(value, str):
        raise InterfaceManifestError(f"{label} must be a string")
    if unicodedata.normalize("NFC", value) != value:
        raise InterfaceManifestError(f"{label} is not NFC")
    if nonempty and not value:
        raise InterfaceManifestError(f"{label} must not be empty")
    return value


def _integer(value: object, label: str, *, minimum: int | None = None) -> int:
    if type(value) is not int:
        raise InterfaceManifestError(f"{label} must be a JSON integer")
    if minimum is not None and value < minimum:
        raise InterfaceManifestError(f"{label} must be at least {minimum}")
    return value


def _boolean(value: object, label: str) -> bool:
    if type(value) is not bool:
        raise InterfaceManifestError(f"{label} must be a JSON Boolean")
    return value


def validate_repo_path(value: object, label: str) -> str:
    path = _string(value, label, nonempty=True)
    if "\\" in path or path.startswith("/") or re.match(r"^[A-Za-z]:", path):
        raise InterfaceManifestError(f"{label} must be a /-separated repo-relative path")
    pieces = path.split("/")
    if any(piece in {"", ".", ".."} for piece in pieces):
        raise InterfaceManifestError(f"{label} contains an empty, '.' or '..' segment")
    if PurePosixPath(path).as_posix() != path or not all(_PATH_SEGMENT_RE.fullmatch(p) for p in pieces):
        raise InterfaceManifestError(f"{label} is not canonical")
    return path


def repo_file_path(repo_root: Path, value: object, label: str) -> Path:
    """Resolve a canonical repo path while enforcing segment case on Windows."""
    path = validate_repo_path(value, label)
    current = repo_root.resolve()
    for segment in path.split("/"):
        try:
            names = {entry.name for entry in current.iterdir()}
        except OSError as exc:
            raise InterfaceManifestError(f"cannot inspect {label} parent {current}: {exc}") from exc
        if segment not in names:
            case_matches = sorted(name for name in names if name.casefold() == segment.casefold())
            if case_matches:
                raise InterfaceManifestError(
                    f"{label} has wrong case at {segment!r}; stored spelling is {case_matches[0]!r}"
                )
            raise InterfaceManifestError(f"{label} is missing at segment {segment!r}: {path}")
        current = current / segment
    if not current.is_file():
        raise InterfaceManifestError(f"{label} is not a file: {path}")
    try:
        current.resolve().relative_to(repo_root.resolve())
    except ValueError as exc:
        raise InterfaceManifestError(f"{label} escapes the repository through a link: {path}") from exc
    return current


def _validate_value(value: object, label: str) -> tuple[str, str]:
    row = _exact_keys(value, {"kind", "text"}, label)
    kind = _string(row["kind"], f"{label}.kind")
    text = _string(row["text"], f"{label}.text")
    if kind == "unsigned_integer":
        if not _UNSIGNED_DECIMAL_RE.fullmatch(text):
            raise InterfaceManifestError(f"{label} is not a canonical unsigned integer")
    elif kind == "signed_integer":
        if not _SIGNED_DECIMAL_RE.fullmatch(text):
            raise InterfaceManifestError(f"{label} is not a canonical signed integer")
    elif kind == "bit_vector":
        match = _BIT_VECTOR_RE.fullmatch(text)
        if not match:
            raise InterfaceManifestError(f"{label} is not a canonical bit vector")
        width = int(match.group(1))
        digits = match.group(2)
        if len(digits) != (width + 3) // 4:
            raise InterfaceManifestError(f"{label} has the wrong hexadecimal digit count")
        if int(digits, 16) >= (1 << width):
            raise InterfaceManifestError(f"{label} has nonzero unused high bits")
    elif kind == "string":
        pass
    else:
        raise InterfaceManifestError(f"{label}.kind is unsupported: {kind!r}")
    return kind, text


def _validate_dimension(value: object, label: str) -> tuple[int, int, int]:
    row = _exact_keys(
        value,
        {"direction", "left", "right", "size", "source_expression"},
        label,
    )
    direction = _string(row["direction"], f"{label}.direction")
    if direction not in {"ascending", "descending"}:
        raise InterfaceManifestError(f"{label}.direction is invalid")
    left = _integer(row["left"], f"{label}.left")
    right = _integer(row["right"], f"{label}.right")
    size = _integer(row["size"], f"{label}.size", minimum=1)
    _string(row["source_expression"], f"{label}.source_expression", nonempty=True)
    if size != abs(left - right) + 1:
        raise InterfaceManifestError(f"{label}.size does not match its bounds")
    if left < right and direction != "ascending":
        raise InterfaceManifestError(f"{label} has wrong direction for ascending bounds")
    if left >= right and direction != "descending":
        raise InterfaceManifestError(
            f"{label} has wrong direction for descending or singleton bounds"
        )
    return left, right, size


def _parameter_cli_text(kind: str, text: str) -> str:
    if kind == "string":
        return json.dumps(text, ensure_ascii=False)
    return text


def expected_elaboration_argv(
    top_module: str,
    parameter_rows: Sequence[Mapping[str, object]],
    source_paths: Sequence[str],
) -> list[str]:
    argv = ["verilator", "--json-only", "--top-module", top_module]
    for index, row in enumerate(parameter_rows):
        name = _string(row.get("name"), f"parameter_overrides[{index}].name", nonempty=True)
        kind, text = _validate_value(row.get("value"), f"parameter_overrides[{index}].value")
        argv.append(f"-G{name}={_parameter_cli_text(kind, text)}")
    argv.extend(source_paths)
    return argv


def validate_manifest_payload(
    payload: Mapping[str, object],
    *,
    expected_top: str | None = None,
    expected_source_closure: Sequence[str] | None = None,
    verify_canonical_digest: bool = True,
) -> None:
    root = _exact_keys(
        payload,
        {
            "elaboration", "hashes", "module", "parameters", "ports",
            "schema_id", "schema_version", "source_closure", "tools",
        },
        "root",
    )
    if _string(root["schema_id"], "schema_id") != SCHEMA_ID:
        raise InterfaceManifestError("schema_id is not zhao.texture.interface")
    if _integer(root["schema_version"], "schema_version") != SCHEMA_VERSION:
        raise InterfaceManifestError("schema_version is not 1")

    module = _exact_keys(
        root["module"],
        {"declaration_end_byte_exclusive", "declaration_start_byte", "name", "source_path"},
        "module",
    )
    module_name = _string(module["name"], "module.name", nonempty=True)
    source_path = validate_repo_path(module["source_path"], "module.source_path")
    start = _integer(module["declaration_start_byte"], "module.declaration_start_byte", minimum=0)
    end = _integer(
        module["declaration_end_byte_exclusive"],
        "module.declaration_end_byte_exclusive",
        minimum=1,
    )
    if end <= start:
        raise InterfaceManifestError("module declaration byte span is empty or reversed")
    if expected_top is not None and module_name != expected_top:
        raise InterfaceManifestError(
            f"module.name {module_name!r} does not match expected top {expected_top!r}"
        )

    parameters = root["parameters"]
    if not isinstance(parameters, list):
        raise InterfaceManifestError("parameters must be an array")
    parameter_names: list[str] = []
    for ordinal, value in enumerate(parameters):
        row = _exact_keys(
            value,
            {
                "declared_kind", "declared_type", "name", "ordinal",
                "selected_value", "source_default_expression",
            },
            f"parameters[{ordinal}]",
        )
        _string(row["declared_kind"], f"parameters[{ordinal}].declared_kind", nonempty=True)
        _string(row["declared_type"], f"parameters[{ordinal}].declared_type", nonempty=True)
        name = _string(row["name"], f"parameters[{ordinal}].name", nonempty=True)
        if _integer(row["ordinal"], f"parameters[{ordinal}].ordinal", minimum=0) != ordinal:
            raise InterfaceManifestError(f"parameters[{ordinal}] ordinal is not contiguous")
        _validate_value(row["selected_value"], f"parameters[{ordinal}].selected_value")
        _string(
            row["source_default_expression"],
            f"parameters[{ordinal}].source_default_expression",
            nonempty=True,
        )
        parameter_names.append(name)
    if len(set(parameter_names)) != len(parameter_names):
        raise InterfaceManifestError("parameters contain duplicate names")

    ports = root["ports"]
    if not isinstance(ports, list) or not ports:
        raise InterfaceManifestError("ports must be a nonempty array")
    port_names: list[str] = []
    for ordinal, value in enumerate(ports):
        row = _exact_keys(
            value,
            {
                "bit_width", "declared_type", "direction", "element_width", "name",
                "net_or_var", "ordinal", "packed_dimensions", "signed",
                "source_expression", "unpacked_dimensions", "unpacked_element_count",
            },
            f"ports[{ordinal}]",
        )
        name = _string(row["name"], f"ports[{ordinal}].name", nonempty=True)
        direction = _string(row["direction"], f"ports[{ordinal}].direction")
        if direction not in {"input", "output", "inout"}:
            raise InterfaceManifestError(f"ports[{ordinal}].direction is invalid")
        net_or_var = _string(row["net_or_var"], f"ports[{ordinal}].net_or_var")
        if net_or_var not in {"net", "variable"}:
            raise InterfaceManifestError(f"ports[{ordinal}].net_or_var is invalid")
        _string(row["declared_type"], f"ports[{ordinal}].declared_type", nonempty=True)
        _string(row["source_expression"], f"ports[{ordinal}].source_expression", nonempty=True)
        _boolean(row["signed"], f"ports[{ordinal}].signed")
        if _integer(row["ordinal"], f"ports[{ordinal}].ordinal", minimum=0) != ordinal:
            raise InterfaceManifestError(f"ports[{ordinal}] ordinal is not contiguous")
        packed = row["packed_dimensions"]
        unpacked = row["unpacked_dimensions"]
        if not isinstance(packed, list) or not isinstance(unpacked, list):
            raise InterfaceManifestError(f"ports[{ordinal}] dimensions must be arrays")
        packed_sizes = [
            _validate_dimension(item, f"ports[{ordinal}].packed_dimensions[{index}]")[2]
            for index, item in enumerate(packed)
        ]
        unpacked_sizes = [
            _validate_dimension(item, f"ports[{ordinal}].unpacked_dimensions[{index}]")[2]
            for index, item in enumerate(unpacked)
        ]
        element_width = _integer(row["element_width"], f"ports[{ordinal}].element_width", minimum=1)
        count = _integer(
            row["unpacked_element_count"],
            f"ports[{ordinal}].unpacked_element_count",
            minimum=1,
        )
        bit_width = _integer(row["bit_width"], f"ports[{ordinal}].bit_width", minimum=1)
        expected_element = math.prod(packed_sizes) if packed_sizes else 1
        expected_count = math.prod(unpacked_sizes) if unpacked_sizes else 1
        if element_width != expected_element:
            raise InterfaceManifestError(f"ports[{ordinal}].element_width is inconsistent")
        if count != expected_count:
            raise InterfaceManifestError(f"ports[{ordinal}].unpacked_element_count is inconsistent")
        if bit_width != element_width * count:
            raise InterfaceManifestError(f"ports[{ordinal}].bit_width is inconsistent")
        port_names.append(name)
    if len(set(port_names)) != len(port_names):
        raise InterfaceManifestError("ports contain duplicate names")

    closure = root["source_closure"]
    if not isinstance(closure, list) or not closure:
        raise InterfaceManifestError("source_closure must be a nonempty array")
    closure_paths: list[str] = []
    seen_module = False
    for ordinal, value in enumerate(closure):
        row = _exact_keys(value, {"kind", "ordinal", "path", "sha256"}, f"source_closure[{ordinal}]")
        kind = _string(row["kind"], f"source_closure[{ordinal}].kind")
        if kind not in {"systemverilog_package", "systemverilog_module"}:
            raise InterfaceManifestError(f"source_closure[{ordinal}].kind is invalid")
        if kind == "systemverilog_module":
            seen_module = True
        elif seen_module:
            raise InterfaceManifestError("a package appears after a module in source_closure")
        if _integer(row["ordinal"], f"source_closure[{ordinal}].ordinal", minimum=0) != ordinal:
            raise InterfaceManifestError(f"source_closure[{ordinal}] ordinal is not contiguous")
        closure_paths.append(validate_repo_path(row["path"], f"source_closure[{ordinal}].path"))
        digest = _string(row["sha256"], f"source_closure[{ordinal}].sha256")
        if not _HASH_RE.fullmatch(digest) or digest == "0" * 64:
            raise InterfaceManifestError(f"source_closure[{ordinal}].sha256 is invalid or sentinel")
    if len(set(closure_paths)) != len(closure_paths):
        raise InterfaceManifestError("source_closure contains duplicate paths")
    if closure_paths[-1] != source_path:
        raise InterfaceManifestError("selected top source is not last in source_closure")
    if expected_source_closure is not None and closure_paths != list(expected_source_closure):
        raise InterfaceManifestError(
            f"source_closure order differs: expected={list(expected_source_closure)!r}, actual={closure_paths!r}"
        )

    hashes = _exact_keys(
        root["hashes"],
        {"canonical_interface_sha256", "module_declaration_sha256", "top_source_sha256"},
        "hashes",
    )
    for name in ("canonical_interface_sha256", "module_declaration_sha256", "top_source_sha256"):
        digest = _string(hashes[name], f"hashes.{name}")
        if not _HASH_RE.fullmatch(digest) or digest == "0" * 64:
            raise InterfaceManifestError(f"hashes.{name} is invalid or sentinel")
    top_rows = [row for row in closure if row["path"] == source_path]
    if len(top_rows) != 1 or top_rows[0]["sha256"] != hashes["top_source_sha256"]:
        raise InterfaceManifestError("top_source_sha256 does not equal the top closure row")

    elaboration = _exact_keys(
        root["elaboration"],
        {"argv", "cwd", "parameter_overrides", "top_module"},
        "elaboration",
    )
    if _string(elaboration["cwd"], "elaboration.cwd") != ".":
        raise InterfaceManifestError("elaboration.cwd must be exactly '.'")
    elaboration_top = _string(elaboration["top_module"], "elaboration.top_module", nonempty=True)
    if elaboration_top != module_name:
        raise InterfaceManifestError("elaboration.top_module differs from module.name")
    overrides = elaboration["parameter_overrides"]
    if not isinstance(overrides, list):
        raise InterfaceManifestError("elaboration.parameter_overrides must be an array")
    override_names: list[str] = []
    for ordinal, value in enumerate(overrides):
        row = _exact_keys(value, {"name", "value"}, f"parameter_overrides[{ordinal}]")
        override_names.append(_string(row["name"], f"parameter_overrides[{ordinal}].name", nonempty=True))
        _validate_value(row["value"], f"parameter_overrides[{ordinal}].value")
    if override_names != parameter_names:
        raise InterfaceManifestError("parameter_overrides names/order differ from parameters")
    for ordinal, (override, parameter) in enumerate(zip(overrides, parameters)):
        if override["value"] != parameter["selected_value"]:
            raise InterfaceManifestError(f"parameter_overrides[{ordinal}] value differs from selected_value")
    argv = elaboration["argv"]
    if not isinstance(argv, list) or any(not isinstance(item, str) for item in argv):
        raise InterfaceManifestError("elaboration.argv must be an array of strings")
    expected_argv = expected_elaboration_argv(module_name, overrides, closure_paths)
    if argv != expected_argv:
        raise InterfaceManifestError(
            f"elaboration.argv differs: expected={expected_argv!r}, actual={argv!r}"
        )

    tools = _exact_keys(root["tools"], {"checker", "elaborator", "generator", "parser", "runtime"}, "tools")
    for role, spec in CUSTOM_TOOL_SPECS.items():
        row = _exact_keys(tools[role], {"name", "path", "sha256", "version"}, f"tools.{role}")
        if _string(row["name"], f"tools.{role}.name") != spec["name"]:
            raise InterfaceManifestError(f"tools.{role}.name is not schema-v1 canonical")
        if validate_repo_path(row["path"], f"tools.{role}.path") != spec["path"]:
            raise InterfaceManifestError(f"tools.{role}.path is not schema-v1 canonical")
        digest = _string(row["sha256"], f"tools.{role}.sha256")
        if not _HASH_RE.fullmatch(digest) or digest == "0" * 64:
            raise InterfaceManifestError(f"tools.{role}.sha256 is invalid or sentinel")
        if _string(row["version"], f"tools.{role}.version") != spec["version"]:
            raise InterfaceManifestError(
                f"tools.{role}.version must be {spec['version']}"
            )
    elaborator = _exact_keys(tools["elaborator"], {"name", "version", "version_command"}, "tools.elaborator")
    if _string(elaborator["name"], "tools.elaborator.name") != "Verilator":
        raise InterfaceManifestError("tools.elaborator.name must be Verilator")
    recorded_verilator_version = _string(
        elaborator["version"], "tools.elaborator.version", nonempty=True
    )
    if recorded_verilator_version != SUPPORTED_VERILATOR_VERSION:
        raise InterfaceManifestError(
            "tools.elaborator.version does not equal the pinned supported Verilator version"
        )
    if elaborator["version_command"] != ["verilator", "--version"]:
        raise InterfaceManifestError("tools.elaborator.version_command is not canonical")
    runtime = _exact_keys(tools["runtime"], {"name", "version"}, "tools.runtime")
    if _string(runtime["name"], "tools.runtime.name") != "CPython":
        raise InterfaceManifestError("tools.runtime.name must be CPython")
    _string(runtime["version"], "tools.runtime.version", nonempty=True)

    def reject_forbidden(value: object, label: str) -> None:
        if value is None:
            raise InterfaceManifestError(f"{label} contains forbidden null")
        if isinstance(value, float):
            raise InterfaceManifestError(f"{label} contains a forbidden floating-point number")
        if isinstance(value, str) and unicodedata.normalize("NFC", value) != value:
            raise InterfaceManifestError(f"{label} contains a non-NFC string")
        if isinstance(value, dict):
            for key, child in value.items():
                reject_forbidden(child, f"{label}.{key}")
        elif isinstance(value, list):
            for index, child in enumerate(value):
                reject_forbidden(child, f"{label}[{index}]")
    reject_forbidden(root, "root")

    if verify_canonical_digest:
        expected_digest = canonical_interface_sha256(root)
        if hashes["canonical_interface_sha256"] != expected_digest:
            raise InterfaceManifestError(
                "canonical_interface_sha256 does not match the payload with only its own member omitted"
            )


def validate_manifest_bytes(
    raw: bytes,
    *,
    expected_top: str | None = None,
    expected_source_closure: Sequence[str] | None = None,
) -> dict[str, object]:
    payload = load_manifest_bytes(raw)
    validate_manifest_payload(
        payload,
        expected_top=expected_top,
        expected_source_closure=expected_source_closure,
    )
    canonical = canonical_json_bytes(payload)
    if raw != canonical:
        raise InterfaceManifestError(
            "stored manifest bytes are not canonical (whitespace, ordering, BOM, or trailing LF differs)"
        )
    return payload


def _walk_json(node: object) -> Iterable[Mapping[str, object]]:
    if isinstance(node, dict):
        yield node
        for key, value in node.items():
            if key in {
                "addr", "dtypep", "varp", "varScopep", "refDTypep", "modp",
                "classOrPackagep", "sensIfacep", "typeTablep", "constPoolp",
            }:
                continue
            yield from _walk_json(value)
    elif isinstance(node, list):
        for child in node:
            yield from _walk_json(child)


def _range_text(value: object) -> tuple[int, int] | None:
    if not isinstance(value, str):
        return None
    match = re.fullmatch(r"\[?(-?\d+):(-?\d+)\]?", value.strip())
    return (int(match.group(1)), int(match.group(2))) if match else None


class _ElaboratedTypes:
    def __init__(self, root: Mapping[str, object]):
        self.by_addr: dict[str, Mapping[str, object]] = {}
        for node in _walk_json(root):
            address = node.get("addr")
            kind = node.get("type")
            if isinstance(address, str) and isinstance(kind, str) and kind.endswith("DTYPE"):
                self.by_addr[address] = node
        self.cache: dict[
            str,
            tuple[int, int, tuple[tuple[int, int], ...], tuple[tuple[int, int], ...], bool, str],
        ] = {}

    def info(
        self, address: object, depth: int = 0
    ) -> tuple[int, int, tuple[tuple[int, int], ...], tuple[tuple[int, int], ...], bool, str]:
        if not isinstance(address, str) or depth > 32:
            raise InterfaceManifestError(f"unresolved Verilator dtype reference {address!r}")
        if address in self.cache:
            return self.cache[address]
        node = self.by_addr.get(address)
        if node is None:
            raise InterfaceManifestError(f"Verilator dtype {address!r} is absent")
        kind = str(node.get("type"))
        signed = bool(node.get("signed"))
        if kind == "BASICDTYPE":
            declared = _range_text(node.get("range"))
            width = abs(declared[0] - declared[1]) + 1 if declared else 1
            result = (width, width, (declared,) if declared else (), (), signed, kind)
        elif kind in {"REFDTYPE", "MEMBERDTYPE", "ENUMDTYPE", "TYPEDEFDTYPE"}:
            target = node.get("refDTypep") or node.get("dtypep")
            total, element, packed, unpacked, target_signed, target_kind = self.info(target, depth + 1)
            result = (
                total,
                element,
                () if kind in {"REFDTYPE", "TYPEDEFDTYPE"} else packed,
                unpacked,
                signed or target_signed,
                target_kind,
            )
        elif kind == "STRUCTDTYPE":
            members = node.get("membersp")
            if not isinstance(members, list) or not members:
                raise InterfaceManifestError("Verilator packed struct has no members")
            total = 0
            for member in members:
                if not isinstance(member, dict):
                    continue
                member_total, _element, _packed, unpacked, _signed, _kind = self.info(
                    member.get("refDTypep") or member.get("dtypep"), depth + 1
                )
                if unpacked:
                    raise InterfaceManifestError("unpacked member in a packed port struct")
                total += member_total
            result = (total, total, (), (), signed, kind)
        elif kind == "UNPACKARRAYDTYPE":
            declared = _range_text(node.get("declRange"))
            if declared is None:
                raise InterfaceManifestError("Verilator unpacked range is unresolved")
            sub_total, sub_element, packed, sub_unpacked, sub_signed, sub_kind = self.info(
                node.get("refDTypep") or node.get("dtypep"), depth + 1
            )
            count = abs(declared[0] - declared[1]) + 1
            result = (
                count * sub_total,
                sub_element,
                packed,
                (declared,) + sub_unpacked,
                signed or sub_signed,
                sub_kind,
            )
        elif kind == "PACKARRAYDTYPE":
            declared = _range_text(node.get("declRange"))
            if declared is None:
                raise InterfaceManifestError("Verilator packed range is unresolved")
            sub_total, _sub_element, sub_packed, unpacked, sub_signed, sub_kind = self.info(
                node.get("refDTypep") or node.get("dtypep"), depth + 1
            )
            count = abs(declared[0] - declared[1]) + 1
            total = count * sub_total
            result = (
                total,
                total,
                (declared,) + sub_packed,
                unpacked,
                signed or sub_signed,
                sub_kind,
            )
        else:
            raise InterfaceManifestError(f"unsupported Verilator dtype kind {kind!r}")
        self.cache[address] = result
        return result


def _verilator_constant(value: object) -> int | str:
    if not isinstance(value, str):
        raise InterfaceManifestError(f"Verilator constant name is invalid: {value!r}")
    if value.startswith('"') and value.endswith('"'):
        return _decode_sv_string(value)
    match = re.fullmatch(r"(?:(\d+))?'([sS]?)([hHbBoOdD])(-?[0-9a-fA-F_xXzZ?]+)", value)
    if match:
        width_text = match.group(1)
        signed = bool(match.group(2))
        digits = match.group(4).replace("_", "")
        if re.search(r"[xXzZ?]", digits):
            raise InterfaceManifestError(f"Verilator constant has unknown bits: {value!r}")
        negative = digits.startswith("-")
        magnitude = digits[1:] if negative else digits
        base = {"h": 16, "b": 2, "o": 8, "d": 10}[match.group(3).lower()]
        parsed = int(magnitude, base)
        parsed = -parsed if negative else parsed
        if width_text is not None:
            width = int(width_text)
            if width <= 0:
                raise InterfaceManifestError(
                    f"Verilator constant has nonpositive width: {value!r}"
                )
            parsed &= (1 << width) - 1
            if signed and (parsed & (1 << (width - 1))):
                parsed -= 1 << width
        elif signed:
            raise InterfaceManifestError(
                f"Verilator constant has unsupported unsized signed value: {value!r}"
            )
        return parsed
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    raise InterfaceManifestError(f"unsupported Verilator constant {value!r}")


def validate_verilator_json_schema(
    root: Mapping[str, object],
    meta: Mapping[str, object],
    *,
    top_module: str | None = None,
    duplicate_purpose: str | None = None,
    verilator_version: str | None = None,
    source_sha256: Mapping[str, str] | None = None,
) -> None:
    """Validate the one pinned Verilator tree/meta structure this parser supports."""
    if root.get("type") != "NETLIST" or root.get("name") != "$root":
        raise InterfaceManifestError(
            f"unsupported {VERILATOR_JSON_SCHEMA_ID}: root is not NETLIST/$root"
        )
    modules = root.get("modulesp")
    misc = root.get("miscsp")
    if not isinstance(modules, list) or not isinstance(misc, list):
        raise InterfaceManifestError(
            f"unsupported {VERILATOR_JSON_SCHEMA_ID}: modulesp/miscsp arrays are absent"
        )
    type_tables = [node for node in misc if isinstance(node, dict) and node.get("type") == "TYPETABLE"]
    if len(type_tables) != 1 or not isinstance(type_tables[0].get("typesp"), list):
        raise InterfaceManifestError(
            f"unsupported {VERILATOR_JSON_SCHEMA_ID}: expected one TYPETABLE.typesp array"
        )
    if set(meta) != {"files", "pointers", "ptrFieldNames"}:
        raise InterfaceManifestError(
            f"unsupported {VERILATOR_JSON_SCHEMA_ID}: metadata members differ: "
            f"{sorted(meta)}"
        )
    files = meta.get("files")
    pointers = meta.get("pointers")
    pointer_fields = meta.get("ptrFieldNames")
    if (
        not isinstance(files, dict)
        or not isinstance(pointers, dict)
        or not isinstance(pointer_fields, list)
        or any(not isinstance(item, str) for item in pointer_fields)
    ):
        raise InterfaceManifestError(
            f"unsupported {VERILATOR_JSON_SCHEMA_ID}: files/pointers/ptrFieldNames have wrong types"
        )
    for file_id, row in files.items():
        if not isinstance(file_id, str) or not isinstance(row, dict):
            raise InterfaceManifestError(
                f"unsupported {VERILATOR_JSON_SCHEMA_ID}: malformed file row"
            )
        if set(row) != {"filename", "realpath", "language"} or any(
            not isinstance(row[key], str) for key in row
        ):
            raise InterfaceManifestError(
                f"unsupported {VERILATOR_JSON_SCHEMA_ID}: malformed file row {file_id!r}"
            )
    markers = verilator_duplicate_markers(root)
    package_hash = (
        source_sha256.get(KNOWN_DUPLICATE_PACKAGE_PATH)
        if source_sha256 is not None else None
    )
    validate_verilator_duplicate_fingerprint(
        markers,
        top_module=top_module,
        purpose=duplicate_purpose,
        verilator_version=verilator_version,
        package_sha256=package_hash,
    )
    for module in modules:
        if not isinstance(module, dict) or module.get("type") not in {"MODULE", "PACKAGE"}:
            raise InterfaceManifestError(
                f"unsupported {VERILATOR_JSON_SCHEMA_ID}: modulesp contains "
                f"node type {module.get('type') if isinstance(module, dict) else type(module).__name__!r}"
            )
        for member in ("addr", "loc", "origName", "stmtsp"):
            if member not in module:
                raise InterfaceManifestError(
                    f"unsupported {VERILATOR_JSON_SCHEMA_ID}: MODULE lacks {member}"
                )


def parse_elaboration_tree(
    root: Mapping[str, object],
    meta: Mapping[str, object],
    *,
    top_module: str,
    duplicate_purpose: str | None = None,
    verilator_version: str | None = None,
    source_sha256: Mapping[str, str] | None = None,
) -> ElaborationView:
    validate_verilator_json_schema(
        root,
        meta,
        top_module=top_module,
        duplicate_purpose=duplicate_purpose,
        verilator_version=verilator_version,
        source_sha256=source_sha256,
    )
    top_level_modules = root["modulesp"]
    assert isinstance(top_level_modules, list)
    modules = [
        node for node in top_level_modules
        if node.get("type") == "MODULE" and node.get("origName") == top_module
    ]
    # @CONST-POOL@ is filtered by origName; a parameter-specialised module may
    # still have the top's origName exactly once.
    if len(modules) != 1:
        raise InterfaceManifestError(
            f"expected one elaborated MODULE {top_module!r}, found {len(modules)}"
        )
    module = modules[0]
    types = _ElaboratedTypes(root)
    statements = module.get("stmtsp")
    if not isinstance(statements, list):
        raise InterfaceManifestError("Verilator top module has no statement list")

    parameters: list[ElaboratedParameter] = []
    ports: list[ElaboratedPort] = []
    for node in statements:
        if not isinstance(node, dict) or node.get("type") != "VAR":
            continue
        if node.get("isGParam"):
            name = node.get("origName") or node.get("name")
            values = node.get("valuep")
            if not isinstance(name, str) or not isinstance(values, list) or len(values) != 1:
                raise InterfaceManifestError("malformed elaborated parameter")
            const = values[0]
            if not isinstance(const, dict) or const.get("type") != "CONST":
                raise InterfaceManifestError(f"parameter {name!r} is not a constant after elaboration")
            total, _element, _packed, _unpacked, signed, kind = types.info(node.get("dtypep"))
            parameters.append(
                ElaboratedParameter(
                    ordinal=len(parameters),
                    name=name,
                    value=_verilator_constant(const.get("name")),
                    bit_width=total,
                    signed=signed,
                    dtype_kind=kind,
                )
            )
        if node.get("isPrimaryIO"):
            name = node.get("origName") or node.get("name")
            direction = node.get("direction")
            if not isinstance(name, str) or direction not in {"INPUT", "OUTPUT", "INOUT"}:
                raise InterfaceManifestError("malformed elaborated primary I/O")
            total, element, packed, unpacked, signed, kind = types.info(node.get("dtypep"))
            var_type = node.get("varType")
            if var_type == "VAR":
                net_or_var = "variable"
            elif var_type in {"WIRE", "TRIWIRE", "PORT"}:
                net_or_var = "net"
            else:
                raise InterfaceManifestError(
                    f"port {name!r} has unsupported elaborated kind {var_type!r}"
                )
            ports.append(
                ElaboratedPort(
                    ordinal=len(ports),
                    name=name,
                    direction=str(direction).lower(),
                    net_or_var=net_or_var,
                    bit_width=total,
                    element_width=element,
                    signed=signed,
                    packed_ranges=packed,
                    unpacked_ranges=unpacked,
                    dtype_kind=kind,
                )
            )
    if not ports:
        raise InterfaceManifestError("Verilator elaboration has no primary I/O")

    files = meta.get("files")
    if not isinstance(files, dict):
        raise InterfaceManifestError("Verilator metadata has no files map")
    module_by_address = {
        item.get("addr"): item
        for item in top_level_modules
        if (
            isinstance(item, dict)
            and item.get("type") == "MODULE"
            and isinstance(item.get("addr"), str)
        )
    }
    top_address = module.get("addr")
    if not isinstance(top_address, str):
        raise InterfaceManifestError("Verilator selected top has no module address")
    reachable_addresses = {top_address}
    pending_addresses = [top_address]
    while pending_addresses:
        owner_address = pending_addresses.pop()
        owner = module_by_address.get(owner_address)
        if owner is None:
            raise InterfaceManifestError(
                f"Verilator instance graph references absent owner {owner_address!r}"
            )
        for node in _walk_json(owner):
            if node.get("type") != "CELL":
                continue
            target = node.get("modp")
            if not isinstance(target, str) or target not in module_by_address:
                raise InterfaceManifestError(
                    f"Verilator CELL {node.get('name')!r} has unresolved module target {target!r}"
                )
            if target not in reachable_addresses:
                reachable_addresses.add(target)
                pending_addresses.append(target)

    module_sources: set[str] = set()
    for address in reachable_addresses:
        elaborated_module = module_by_address[address]
        original_name = elaborated_module.get("origName")
        if not isinstance(original_name, str) or original_name.startswith("@"):
            raise InterfaceManifestError(
                f"selected instance graph contains invalid module name {original_name!r}"
            )
        loc = elaborated_module.get("loc")
        if not isinstance(loc, str) or "," not in loc:
            raise InterfaceManifestError(f"elaborated module {original_name!r} has no source location")
        file_id = loc.split(",", 1)[0]
        file_row = files.get(file_id)
        if not isinstance(file_row, dict):
            raise InterfaceManifestError(f"Verilator source id {file_id!r} is absent")
        filename = file_row.get("filename")
        if not isinstance(filename, str):
            raise InterfaceManifestError("Verilator source filename is invalid")
        normalized = filename.replace("\\", "/")
        if normalized.startswith("<"):
            raise InterfaceManifestError(
                f"selected module {original_name!r} has a non-source filename {normalized!r}"
            )
        module_sources.add(normalized)
    return ElaborationView(
        parameters=tuple(parameters),
        ports=tuple(ports),
        module_source_paths=frozenset(module_sources),
    )


def compare_source_and_elaboration(
    source: ModuleSourceView,
    elaborated: ElaborationView,
) -> None:
    errors: list[str] = []
    source_param_names = [item.name for item in source.parameters]
    elaborated_param_names = [item.name for item in elaborated.parameters]
    if source_param_names != elaborated_param_names:
        errors.append(
            f"parameter names/order differ: source={source_param_names}, elaboration={elaborated_param_names}"
        )
    by_parameter = {item.name: item for item in elaborated.parameters}
    for parameter in source.parameters:
        other = by_parameter.get(parameter.name)
        if other is None:
            continue
        kind = parameter.selected_value.kind
        text = parameter.selected_value.text
        if other.dtype_kind not in SUPPORTED_DTYPE_KINDS:
            errors.append(
                f"parameter {parameter.name!r} dtype_kind {other.dtype_kind!r} is unsupported"
            )
        declared_words = [
            word for word in _IDENTIFIER_RE.findall(parameter.declared_type)
            if word not in _SIGN_WORDS
        ]
        declared_base = declared_words[-1] if declared_words else ""
        fixed_width = _FIXED_INTEGER_WIDTHS.get(declared_base)
        if fixed_width is not None and other.bit_width != fixed_width:
            errors.append(
                f"parameter {parameter.name!r} fixed integer width: "
                f"source type {declared_base!r} requires {fixed_width}, "
                f"elaboration={other.bit_width}"
            )
        if kind == "string":
            expected_value: int | str = text
            actual_value: int | str = other.value
        elif kind == "bit_vector":
            match = _BIT_VECTOR_RE.fullmatch(text)
            assert match is not None
            expected_value = int(match.group(2), 16)
            expected_width = int(match.group(1))
            actual_value = (other.value & ((1 << expected_width) - 1)
                            if isinstance(other.value, int) else other.value)
            if other.bit_width != expected_width:
                errors.append(
                    f"parameter {parameter.name!r} width: source={expected_width}, elaboration={other.bit_width}"
                )
        else:
            expected_value = int(text)
            actual_value = other.value
        if actual_value != expected_value:
            errors.append(
                f"parameter {parameter.name!r} value: source={expected_value!r}, elaboration={actual_value!r}"
            )
        if kind == "unsigned_integer" and other.signed:
            errors.append(f"parameter {parameter.name!r} elaborated signed but is unsigned_integer")
        if kind == "signed_integer" and not other.signed:
            errors.append(f"parameter {parameter.name!r} elaborated unsigned but is signed_integer")

    source_port_names = [item.name for item in source.ports]
    elaborated_port_names = [item.name for item in elaborated.ports]
    if source_port_names != elaborated_port_names:
        errors.append(
            f"port names/order differ: source={source_port_names}, elaboration={elaborated_port_names}"
        )
    by_port = {item.name: item for item in elaborated.ports}
    for port in source.ports:
        other = by_port.get(port.name)
        if other is None:
            continue
        expected_packed = tuple((dim.left, dim.right) for dim in port.packed_dimensions)
        expected_unpacked = tuple((dim.left, dim.right) for dim in port.unpacked_dimensions)
        if other.dtype_kind not in SUPPORTED_DTYPE_KINDS:
            errors.append(
                f"port {port.name!r} dtype_kind {other.dtype_kind!r} is unsupported"
            )
        comparisons = (
            ("direction", port.direction, other.direction),
            ("net/variable kind", port.net_or_var, other.net_or_var),
            ("signedness", port.signed, other.signed),
            ("packed dimensions", expected_packed, other.packed_ranges),
            ("unpacked dimensions", expected_unpacked, other.unpacked_ranges),
            ("element width", port.element_width, other.element_width),
            ("$bits", port.bit_width, other.bit_width),
        )
        for label, expected, actual in comparisons:
            if expected != actual:
                errors.append(
                    f"port {port.name!r} {label}: source={expected!r}, elaboration={actual!r}"
                )
    if errors:
        raise InterfaceManifestError("source/elaboration mismatch: " + "; ".join(errors))


def _path_identity(path: str | os.PathLike[str]) -> str:
    return os.path.normcase(os.path.normpath(os.path.abspath(str(path))))


def find_verilator(repo_root: Path, explicit: str | None = None) -> Path:
    if explicit:
        candidate = Path(explicit).resolve()
        if candidate.is_file():
            return candidate
        raise InterfaceManifestError(f"selected Verilator executable is missing: {candidate}")
    if "ZHAO_VERILATOR" in os.environ:
        configured = os.environ.get("ZHAO_VERILATOR", "")
        candidate = Path(configured).resolve() if configured else None
        if candidate is None or not candidate.is_file():
            raise InterfaceManifestError(
                f"explicit ZHAO_VERILATOR is missing or not a file: {configured!r}"
            )
        return candidate
    if "VERILATOR_ROOT" in os.environ:
        configured_root = os.environ.get("VERILATOR_ROOT", "")
        if not configured_root:
            raise InterfaceManifestError("explicit VERILATOR_ROOT is empty")
        root = Path(configured_root).resolve()
        candidates = [
            root.parents[1] / "bin" / "verilator_bin.exe",
            root.parents[1] / "bin" / "verilator_bin",
        ]
        for candidate in candidates:
            if candidate.is_file():
                return candidate.resolve()
        raise InterfaceManifestError(
            "explicit VERILATOR_ROOT has no derived Verilator executable: "
            + ", ".join(str(candidate) for candidate in candidates)
        )
    candidates: list[Path] = []
    suite = repo_root.parent / ".tools" / "oss-cad-suite" / "bin"
    candidates.extend([suite / "verilator_bin.exe", suite / "verilator_bin"])
    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()
    discovered = shutil.which("verilator_bin")
    if discovered:
        return Path(discovered).resolve()
    raise InterfaceManifestError("Verilator is required; no source-only fallback exists")


def verilator_environment(executable: Path, repo_root: Path) -> dict[str, str]:
    environment = os.environ.copy()
    selected = executable.resolve()
    suite = selected.parent.parent
    suite_bin = suite / "bin"
    suite_lib = suite / "lib"
    verilator_root = suite / "share" / "verilator"
    for label, path in (
        ("suite bin", suite_bin),
        ("suite lib", suite_lib),
        ("VERILATOR_ROOT", verilator_root),
    ):
        if not path.is_dir():
            raise InterfaceManifestError(
                f"Verilator loader environment lacks {label}: {path}"
            )
    prefixes = [suite_bin, suite_lib]
    configured_winlibs = environment.get("ZHAO_WINLIBS_BIN")
    if not configured_winlibs:
        candidates = [repo_root.parents[1] / "dsstuff" / "mingw64" / "bin"]
        if len(suite.parents) > 2:
            candidates.append(suite.parents[2] / "dsstuff" / "mingw64" / "bin")
        configured_winlibs = next(
            (str(candidate) for candidate in candidates if candidate.is_dir()), None
        )
        if os.name == "nt" and configured_winlibs is None:
            raise InterfaceManifestError(
                "pinned Verilator requires MinGW runtime, but no explicit candidate exists: "
                + ", ".join(str(candidate) for candidate in candidates)
            )
    if configured_winlibs:
        winlibs = Path(configured_winlibs).resolve()
        if not winlibs.is_dir():
            raise InterfaceManifestError(f"configured winlibs directory is missing: {winlibs}")
        prefixes.append(winlibs)
    prefix_keys = {_path_identity(path) for path in prefixes}
    inherited = [
        item for item in environment.get("PATH", "").split(os.pathsep)
        if item and _path_identity(item) not in prefix_keys
    ]
    environment["PATH"] = os.pathsep.join([str(path) for path in prefixes] + inherited)
    environment["VERILATOR_ROOT"] = str(verilator_root)
    return environment


def verilator_version(executable: Path, repo_root: Path) -> str:
    completed = subprocess.run(
        [str(executable), "--version"],
        cwd=repo_root,
        env=verilator_environment(executable, repo_root),
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        detail = (completed.stdout + completed.stderr).decode("utf-8", errors="replace")
        raise InterfaceManifestError(
            f"verilator --version failed with RC {completed.returncode}: {detail[-2000:]}"
        )
    try:
        output = completed.stdout.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise InterfaceManifestError("verilator --version output is not UTF-8") from exc
    normalized = _lf(output)
    lines = normalized.split("\n")
    first = lines[0] if lines else ""
    if not first:
        raise InterfaceManifestError("verilator --version returned no first line")
    return first


def validate_supported_verilator_version(version: str) -> None:
    if version != SUPPORTED_VERILATOR_VERSION:
        raise InterfaceManifestError(
            "unsupported Verilator version for pinned JSON schema "
            f"{VERILATOR_JSON_SCHEMA_ID}: expected {SUPPORTED_VERILATOR_VERSION!r}, "
            f"found {version!r}"
        )


def _copy_shadow_sources(
    shadow_root: Path,
    snapshots: SnapshotSet,
    source_paths: Sequence[str],
) -> None:
    for path in source_paths:
        relative = Path(*path.split("/"))
        destination = shadow_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(snapshots[path].raw)


def _repair_verilator_windows_meta(text: str) -> str:
    filename_field = re.compile(r'("filename"\s*:\s*")([^"\r\n]*)(")')

    def repair(match: re.Match[str]) -> str:
        value = re.sub(r"\\+", "/", match.group(2))
        return match.group(1) + value + match.group(3)

    return filename_field.sub(repair, text)


class _VerilatorJsonObject(dict[str, object]):
    identical_name_duplicates: int



def _verilator_pairs(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result = _VerilatorJsonObject()
    result.identical_name_duplicates = 0
    for key, value in pairs:
        if key in result:
            if key == "name" and result[key] == value:
                result.identical_name_duplicates += 1
                if result.identical_name_duplicates > 1:
                    raise _DuplicateKey("multiple identical name duplicates")
                continue
            raise _DuplicateKey(key)
        result[key] = value
    return result


def _verilator_duplicate_objects(payload: object) -> list[_VerilatorJsonObject]:
    marked: list[_VerilatorJsonObject] = []
    if isinstance(payload, _VerilatorJsonObject):
        if payload.identical_name_duplicates:
            marked.append(payload)
        for value in payload.values():
            marked.extend(_verilator_duplicate_objects(value))
    elif isinstance(payload, list):
        for value in payload:
            marked.extend(_verilator_duplicate_objects(value))
    return marked


@dataclass(frozen=True, order=True)
class VerilatorDuplicateMarker:
    json_pointer: str
    parent_struct_addr: str
    member_name: str
    loc: str

    def canonical_row(self) -> dict[str, str]:
        return {
            "json_pointer": self.json_pointer,
            "loc": self.loc,
            "member_name": self.member_name,
            "parent_struct_addr": self.parent_struct_addr,
        }


def _json_pointer_part(value: str) -> str:
    return value.replace("~", "~0").replace("/", "~1")


def verilator_duplicate_markers(payload: object) -> tuple[VerilatorDuplicateMarker, ...]:
    markers: list[VerilatorDuplicateMarker] = []

    def walk(
        node: object,
        pointer: str,
        member_parent: _VerilatorJsonObject | None = None,
    ) -> None:
        if isinstance(node, _VerilatorJsonObject):
            if node.identical_name_duplicates:
                if not (
                    member_parent is not None
                    and node.get("type") == "MEMBERDTYPE"
                    and isinstance(node.get("name"), str)
                    and isinstance(node.get("addr"), str)
                    and isinstance(node.get("loc"), str)
                    and (
                        isinstance(node.get("dtypep"), str)
                        or isinstance(node.get("refDTypep"), str)
                    )
                    and node.identical_name_duplicates == 1
                ):
                    raise InterfaceManifestError(
                        "identical name duplicate is not one known package MEMBERDTYPE node "
                        "under TYPETABLE.typesp/STRUCTDTYPE.membersp"
                    )
                parent_addr = member_parent.get("addr")
                if not isinstance(parent_addr, str):
                    raise InterfaceManifestError(
                        "duplicate MEMBERDTYPE parent STRUCTDTYPE has no identity address"
                    )
                markers.append(
                    VerilatorDuplicateMarker(
                        json_pointer=pointer,
                        parent_struct_addr=parent_addr,
                        member_name=str(node["name"]),
                        loc=str(node["loc"]),
                    )
                )
            for key, value in node.items():
                child_pointer = pointer + "/" + _json_pointer_part(key)
                if (
                    key == "membersp"
                    and node.get("type") == "STRUCTDTYPE"
                    and isinstance(value, list)
                ):
                    for index, child in enumerate(value):
                        walk(child, child_pointer + f"/{index}", node)
                else:
                    walk(value, child_pointer)
        elif isinstance(node, list):
            for index, value in enumerate(node):
                walk(value, pointer + f"/{index}")

    walk(payload, "")
    return tuple(sorted(markers))


def canonical_verilator_duplicate_marker_bytes(
    markers: Sequence[VerilatorDuplicateMarker],
) -> bytes:
    """Serialise the duplicate-marker set for fingerprinting.

    parent_struct_addr is Verilator's own internal label for the parent struct
    ("(RXPB)"), reassigned wholesale whenever elaboration order moves. Adding
    one register to the island relabelled 98 of 105 rows while every
    json_pointer, member_name and loc stayed identical. Hashing it makes the
    fingerprint fire on changes it is not trying to detect, and it was re-pinned
    three times in one session for exactly that.

    What this fingerprint is FOR is which members Verilator had to rename, and
    how they group under their parents. So the label is replaced by its GROUP
    ORDINAL, assigned by first appearance in sorted order. A pure relabelling
    now hashes identically; members moving between parents, or the number of
    distinct parents changing, still move the ordinals and still fire.
    DuplicateFingerprintGroupingTests is the negative control for that claim.
    """
    ordered = sorted(markers)
    groups: dict[str, str] = {}
    rows = []
    for marker in ordered:
        row = marker.canonical_row()
        addr = row.pop("parent_struct_addr")
        if addr not in groups:
            groups[addr] = f"g{len(groups)}"
        row["parent_struct_group"] = groups[addr]
        rows.append(row)
    return json.dumps(
        rows,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def verilator_duplicate_marker_sha256(
    markers: Sequence[VerilatorDuplicateMarker],
) -> str:
    return sha256_bytes(canonical_verilator_duplicate_marker_bytes(markers))


def validate_verilator_duplicate_fingerprint(
    markers: Sequence[VerilatorDuplicateMarker],
    *,
    top_module: str | None,
    purpose: str | None,
    verilator_version: str | None,
    package_sha256: str | None,
) -> None:
    if (
        not markers
        and package_sha256 is None
        and (top_module, purpose) not in SUPPORTED_DUPLICATE_PROFILES
    ):
        # Only a generic elaboration which did not compile the pinned package has
        # no Verilator duplicate exception to prove. A known profile is strict
        # even when marker discovery and package provenance are both absent.
        return
    if verilator_version != SUPPORTED_VERILATOR_VERSION:
        raise InterfaceManifestError(
            "duplicate-name exception requires the pinned Verilator version"
        )
    if package_sha256 != SUPPORTED_DUPLICATE_PACKAGE_SHA256:
        raise InterfaceManifestError(
            "duplicate-name exception package bytes differ: expected "
            f"{SUPPORTED_DUPLICATE_PACKAGE_SHA256!r}, found {package_sha256!r}"
        )
    profile = SUPPORTED_DUPLICATE_PROFILES.get((top_module, purpose))
    if profile is None:
        raise InterfaceManifestError(
            "duplicate-name exception has no supported top/purpose profile: "
            f"top={top_module!r}, purpose={purpose!r}"
        )
    marker_digest = verilator_duplicate_marker_sha256(markers)
    if len(markers) != profile["count"] or marker_digest != profile["sha256"]:
        raise InterfaceManifestError(
            "duplicate-name exception fingerprint differs: "
            f"count={len(markers)}, sha256={marker_digest}"
        )


def _validate_verilator_duplicate_shape(payload: object) -> None:
    # Walking is the structural gate: every marked object must be exactly a
    # MEMBERDTYPE directly owned by a STRUCTDTYPE.membersp edge.  Version,
    # package-byte, location and complete-set fingerprint checks follow only
    # after metadata and immutable source snapshots are available.
    verilator_duplicate_markers(payload)


def _load_verilator_json(path: Path, *, repair_windows_filename_escape: bool = False) -> object:
    """Load a temporary Verilator tree artifact.

    Verilator 5.051 on Windows writes relative ``filename`` fields with one
    unescaped native separator while correctly escaping absolute ``realpath``
    values.  Canonicalize separator runs only inside those filename fields and
    only for the elaborator-owned metadata file.  Manifest JSON never uses this
    compatibility path and remains byte-for-byte strict.
    """
    text = path.read_text(encoding="utf-8")
    try:
        payload = json.loads(text, object_pairs_hook=_verilator_pairs)
        _validate_verilator_duplicate_shape(payload)
        return payload
    except _DuplicateKey as exc:
        raise InterfaceManifestError(
            f"Verilator JSON contains duplicate key {exc.args[0]!r}"
        ) from exc
    except json.JSONDecodeError:
        if not repair_windows_filename_escape:
            raise
        repaired = _repair_verilator_windows_meta(text)
        if repaired == text:
            raise
        try:
            payload = json.loads(repaired, object_pairs_hook=_verilator_pairs)
            _validate_verilator_duplicate_shape(payload)
            return payload
        except _DuplicateKey as exc:
            raise InterfaceManifestError(
                f"Verilator JSON contains duplicate key {exc.args[0]!r}"
            ) from exc


def run_verilator_query(
    *,
    repo_root: Path,
    top_module: str,
    parameters: Sequence[ParameterView],
    source_paths: Sequence[str],
    snapshots: SnapshotSet,
    duplicate_purpose: str | None = None,
    verilator: str | None = None,
) -> tuple[ElaborationView, str, list[str]]:
    executable = find_verilator(repo_root, verilator)
    version = verilator_version(executable, repo_root)
    validate_supported_verilator_version(version)
    override_rows = [
        {"name": item.name, "value": item.selected_value.manifest()}
        for item in parameters
    ]
    argv = expected_elaboration_argv(top_module, override_rows, source_paths)
    with tempfile.TemporaryDirectory(prefix="zhao-texture-interface-elab-") as temporary:
        shadow = Path(temporary)
        _copy_shadow_sources(shadow, snapshots, source_paths)
        command = [str(executable), *argv[1:]]
        completed = subprocess.run(
            command,
            cwd=shadow,
            env=verilator_environment(executable, repo_root),
            capture_output=True,
            text=True,
            errors="replace",
            check=False,
        )
        tree_path = shadow / "obj_dir" / f"V{top_module}.tree.json"
        meta_path = shadow / "obj_dir" / f"V{top_module}.tree.meta.json"
        if completed.returncode != 0 or not tree_path.is_file() or not meta_path.is_file():
            diagnostic = (completed.stdout or "") + (completed.stderr or "")
            raise InterfaceManifestError(
                f"Verilator elaboration failed with RC {completed.returncode}:\n{diagnostic[-6000:]}"
            )
        try:
            tree = _load_verilator_json(tree_path)
            meta = _load_verilator_json(
                meta_path, repair_windows_filename_escape=True
            )
        except (OSError, json.JSONDecodeError) as exc:
            raise InterfaceManifestError(f"could not read Verilator elaboration JSON: {exc}") from exc
        if not isinstance(tree, dict) or not isinstance(meta, dict):
            raise InterfaceManifestError("Verilator elaboration JSON roots are not objects")
        view = parse_elaboration_tree(
            tree,
            meta,
            top_module=top_module,
            duplicate_purpose=duplicate_purpose,
            verilator_version=version,
            source_sha256={
                path: snapshots[path].sha256 for path in source_paths
            },
        )
    return view, version, argv


def _source_kind(raw: bytes, path: str) -> str:
    try:
        source = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise InterfaceManifestError(f"{path}: invalid UTF-8") from exc
    masked = _mask_comments(source)
    package = bool(re.search(r"\bpackage\s+[A-Za-z_$][A-Za-z0-9_$]*\s*;", masked))
    module = bool(re.search(r"\bmodule\s+[A-Za-z_$][A-Za-z0-9_$]*\b", masked))
    if package:
        # The Packet-A package source also carries its private elaboration guard
        # module; compile-role classification remains package-first.
        return "systemverilog_package"
    if module:
        return "systemverilog_module"
    raise InterfaceManifestError(
        f"{path}: source declares neither a package nor a module"
    )


@dataclass(frozen=True)
class _PackageConstantDeclaration:
    package: str
    name: str
    declared_type: str
    expression: str


_QUALIFIED_CONSTANT_RE = re.compile(
    r"[A-Za-z_$][A-Za-z0-9_$]*::[A-Za-z_$][A-Za-z0-9_$]*"
)
_INTEGRAL_DECLARED_TYPES = {
    "bit", "logic", "reg", "byte", "shortint", "int", "integer",
    "longint", "time",
}


def _is_integral_declared_type(declared_type: str) -> bool:
    without_dimensions = re.sub(r"\[[^\]]*\]", " ", declared_type)
    words = [
        word for word in _IDENTIFIER_RE.findall(without_dimensions)
        if word not in _SIGN_WORDS
    ]
    return bool(words) and words[-1] in _INTEGRAL_DECLARED_TYPES


def _relaxed_module_header_span(
    masked: str, module_match: re.Match[str]
) -> tuple[int, int]:
    """Return a module declaration span, allowing a parameterized portless guard."""
    at = module_match.end()
    at = _skip_space(masked, at)
    if at < len(masked) and masked[at] == "#":
        at = _skip_space(masked, at + 1)
        if at >= len(masked) or masked[at] != "(":
            raise InterfaceManifestError("malformed module parameter list")
        at = _balanced_end(masked, at, "(", ")") + 1
    at = _skip_space(masked, at)
    if at < len(masked) and masked[at] == "(":
        at = _balanced_end(masked, at, "(", ")") + 1
    at = _skip_space(masked, at)
    if at >= len(masked) or masked[at] != ";":
        raise InterfaceManifestError("malformed module declaration terminator")
    return module_match.start(), at + 1


def resolve_package_integral_constants(
    snapshots: SnapshotSet,
    source_paths: Sequence[str],
) -> Mapping[str, int]:
    """Resolve qualified constants requested by snapshotted module headers only."""
    package_files: dict[str, str] = {}
    declarations: dict[str, _PackageConstantDeclaration] = {}
    requested: set[str] = set()

    for path in source_paths:
        try:
            text = snapshots[path].raw.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise InterfaceManifestError(f"{path}: invalid UTF-8") from exc
        masked = _mask_comments(text)
        for module_match in re.finditer(
            r"(?m)^[ \t]*module[ \t]+([A-Za-z_$][A-Za-z0-9_$]*)\b", masked
        ):
            start, end = _relaxed_module_header_span(masked, module_match)
            header = masked[start:end]
            for dimension_start, dimension_end in _dimension_spans(header):
                requested.update(
                    _QUALIFIED_CONSTANT_RE.findall(
                        header[dimension_start:dimension_end]
                    )
                )

        for package_match in re.finditer(
            r"(?m)^[ \t]*package[ \t]+([A-Za-z_$][A-Za-z0-9_$]*)\s*;", masked
        ):
            package = package_match.group(1)
            previous = package_files.get(package)
            if previous is not None:
                raise InterfaceManifestError(
                    f"ambiguous package {package!r} is declared in both {previous} and {path}"
                )
            package_files[package] = path
            end_match = re.search(r"\bendpackage\b", masked[package_match.end():])
            if end_match is None:
                raise InterfaceManifestError(f"package {package!r} has no endpackage")
            body_end = package_match.end() + end_match.start()
            body = masked[package_match.end():body_end]
            for constant_match in re.finditer(
                r"\b(localparam|parameter)\b(.*?);", body, re.S
            ):
                kind = constant_match.group(1)
                item = constant_match.group(2).strip()
                try:
                    equal = _top_level_equal(item)
                except InterfaceManifestError as exc:
                    raise InterfaceManifestError(
                        f"ambiguous {kind} declaration in package {package!r}: {item!r}"
                    ) from exc
                left = item[:equal]
                identifiers = _top_level_identifiers(left)
                if not identifiers:
                    raise InterfaceManifestError(
                        f"package {package!r} {kind} has no declared name"
                    )
                name, name_start, _name_end = identifiers[-1]
                declared_type = left[:name_start].strip()
                expression = item[equal + 1:].strip()
                qualified = f"{package}::{name}"
                if qualified in declarations:
                    raise InterfaceManifestError(
                        f"ambiguous package constant {qualified!r} is declared more than once"
                    )
                declarations[qualified] = _PackageConstantDeclaration(
                    package=package,
                    name=name,
                    declared_type=declared_type,
                    expression=expression,
                )

    state: dict[str, str] = {}
    resolved: dict[str, int] = {}

    def resolve(qualified: str, stack: tuple[str, ...]) -> int:
        status = state.get(qualified)
        if status == "done":
            return resolved[qualified]
        if status == "visiting":
            raise InterfaceManifestError(
                "cyclic package integral constants: " + " -> ".join(stack + (qualified,))
            )
        declaration = declarations.get(qualified)
        if declaration is None:
            raise InterfaceManifestError(
                f"unknown package integral constant {qualified!r}"
            )
        if not _is_integral_declared_type(declaration.declared_type):
            raise InterfaceManifestError(
                f"nonintegral package constant {qualified!r} has type "
                f"{declaration.declared_type!r}"
            )
        state[qualified] = "visiting"
        environment: dict[str, int] = {}
        for dependency in _QUALIFIED_CONSTANT_RE.findall(declaration.expression):
            environment[dependency] = resolve(dependency, stack + (qualified,))

        without_qualified = _QUALIFIED_CONSTANT_RE.sub("0", declaration.expression)
        without_literals = _replace_sv_numbers(without_qualified).replace("$clog2", "clog2")
        for identifier in _IDENTIFIER_RE.findall(without_literals):
            if identifier == "clog2":
                continue
            dependency = f"{declaration.package}::{identifier}"
            if dependency not in declarations:
                raise InterfaceManifestError(
                    f"unknown integral dependency {identifier!r} in {qualified!r}"
                )
            environment[identifier] = resolve(dependency, stack + (qualified,))
        value = eval_sv_integer(declaration.expression, environment)
        resolved[qualified] = value
        state[qualified] = "done"
        return value

    for qualified in sorted(requested):
        resolve(qualified, ())
    return MappingProxyType(dict(resolved))


def _static_source_closure(
    snapshots: SnapshotSet,
    source_paths: Sequence[str],
    top_module: str,
) -> tuple[set[str], dict[str, str]]:
    module_sources: dict[str, str] = {}
    module_bodies: dict[str, str] = {}
    package_sources: dict[str, str] = {}
    package_texts: dict[str, str] = {}
    for path in source_paths:
        raw = snapshots[path].raw
        source = raw.decode("utf-8")
        masked = _mask_comments(source)
        for match in re.finditer(r"\bpackage\s+([A-Za-z_$][A-Za-z0-9_$]*)\s*;", masked):
            name = match.group(1)
            if name in package_sources:
                raise InterfaceManifestError(f"package {name!r} is declared more than once")
            package_sources[name] = path
            package_texts[name] = masked
        module_matches = list(re.finditer(r"\bmodule\s+([A-Za-z_$][A-Za-z0-9_$]*)\b", masked))
        for match in module_matches:
            name = match.group(1)
            end_match = re.search(r"\bendmodule\b", masked[match.end():])
            if end_match is None:
                raise InterfaceManifestError(f"module {name!r} has no endmodule")
            body_end = match.end() + end_match.end()
            if name in module_sources:
                raise InterfaceManifestError(f"module {name!r} is declared more than once")
            module_sources[name] = path
            module_bodies[name] = masked[match.start():body_end]
    if top_module not in module_sources:
        raise InterfaceManifestError(f"selected top module {top_module!r} is absent from source closure")

    names = set(module_sources)
    edges: dict[str, set[str]] = {name: set() for name in names}
    for owner, body in module_bodies.items():
        for candidate in names - {owner}:
            pattern = (
                r"(?<![\w:$])" + re.escape(candidate) +
                r"(?![\w$])\s*(?:#\s*\(.*?\)\s*)?"
                r"[A-Za-z_$][A-Za-z0-9_$]*\s*\("
            )
            if re.search(pattern, body, re.S):
                edges[owner].add(candidate)
    reachable = {top_module}
    pending = [top_module]
    while pending:
        owner = pending.pop()
        for child in edges.get(owner, ()):
            if child not in reachable:
                reachable.add(child)
                pending.append(child)
    expected_paths = {module_sources[name] for name in reachable}

    referenced_packages: set[str] = set()
    package_ref_re = re.compile(
        r"(?:\bimport\s+)?\b([A-Za-z_$][A-Za-z0-9_$]*)::(?:\*|[A-Za-z_$][A-Za-z0-9_$]*)"
    )
    for name in reachable:
        referenced_packages.update(package_ref_re.findall(module_bodies[name]))
    package_pending = list(referenced_packages)
    while package_pending:
        package = package_pending.pop()
        if package not in package_sources:
            continue
        expected_paths.add(package_sources[package])
        for dependency in package_ref_re.findall(package_texts[package]):
            if dependency not in referenced_packages:
                referenced_packages.add(dependency)
                package_pending.append(dependency)
    return expected_paths, module_sources


def _referenced_package_paths(
    snapshots: SnapshotSet,
    source_paths: Sequence[str],
    seed_paths: Iterable[str],
) -> set[str]:
    package_ref_re = re.compile(
        r"(?:\bimport\s+)?\b([A-Za-z_$][A-Za-z0-9_$]*)::"
        r"(?:\*|[A-Za-z_$][A-Za-z0-9_$]*)"
    )
    package_sources: dict[str, str] = {}
    package_texts: dict[str, str] = {}
    texts_by_path: dict[str, str] = {}
    for path in source_paths:
        text = _mask_comments(snapshots[path].raw.decode("utf-8"))
        texts_by_path[path] = text
        for match in re.finditer(
            r"\bpackage\s+([A-Za-z_$][A-Za-z0-9_$]*)\s*;", text
        ):
            name = match.group(1)
            if name in package_sources and package_sources[name] != path:
                raise InterfaceManifestError(f"package {name!r} is declared more than once")
            package_sources[name] = path
            package_texts[name] = text
    referenced: set[str] = set()
    for path in seed_paths:
        if path in texts_by_path:
            referenced.update(package_ref_re.findall(texts_by_path[path]))
    pending = list(referenced)
    result: set[str] = set()
    while pending:
        package = pending.pop()
        path = package_sources.get(package)
        if path is None:
            continue
        result.add(path)
        for dependency in package_ref_re.findall(package_texts[package]):
            if dependency not in referenced:
                referenced.add(dependency)
                pending.append(dependency)
    return result


def validate_source_closure(
    *,
    repo_root: Path,
    source_paths: Sequence[str],
    top_module: str,
    elaborated_module_paths: Iterable[str] | None = None,
    production: bool = False,
    snapshots: SnapshotSet | None = None,
) -> list[dict[str, object]]:
    paths = [validate_repo_path(path, f"source path {index}") for index, path in enumerate(source_paths)]
    snapshots = snapshots or SnapshotSet.capture(repo_root, paths)
    if len(set(paths)) != len(paths):
        raise InterfaceManifestError("source closure contains duplicate paths")
    if production and tuple(paths) != PRODUCTION_SOURCE_CLOSURE:
        raise InterfaceManifestError("Packet-B production source closure/order differs from the pinned contract")
    rows: list[dict[str, object]] = []
    seen_module = False
    for ordinal, path in enumerate(paths):
        snapshot = snapshots[path]
        raw = snapshot.raw
        kind = _source_kind(raw, path)
        if kind == "systemverilog_module":
            seen_module = True
        elif seen_module:
            raise InterfaceManifestError(f"package {path!r} appears after a module source")
        rows.append(
            {"kind": kind, "ordinal": ordinal, "path": path, "sha256": snapshot.sha256}
        )
    top_paths = [path for path, row in zip(paths, rows) if row["kind"] == "systemverilog_module" and re.search(
        r"\bmodule\s+" + re.escape(top_module) + r"\b",
        _mask_comments(snapshots[path].raw.decode("utf-8")),
    )]
    if top_paths != [paths[-1]]:
        raise InterfaceManifestError("selected top source must occur exactly once and be last")
    actual_paths = set(paths)
    if elaborated_module_paths is None:
        expected_paths, _module_sources = _static_source_closure(
            snapshots, paths, top_module
        )
    else:
        elaborated = {path.replace("\\", "/") for path in elaborated_module_paths}
        unknown = elaborated - actual_paths
        if unknown:
            raise InterfaceManifestError(
                f"Verilator elaborated module source(s) outside closure: {sorted(unknown)}"
            )
        expected_paths = elaborated | _referenced_package_paths(
            snapshots, paths, elaborated
        )
    if expected_paths != actual_paths:
        raise InterfaceManifestError(
            "source closure is not the recursively elaborated selected-root module "
            "closure plus imported packages: "
            f"missing={sorted(expected_paths - actual_paths)}, "
            f"unreachable_extra={sorted(actual_paths - expected_paths)}"
        )
    return rows


def _normalize_sv_expression(text: str) -> str:
    return re.sub(r"\s+", "", text)


def load_quiet_fixture_bytes(raw: bytes) -> dict[str, object]:
    payload = load_manifest_bytes(raw)
    root = _exact_keys(
        payload,
        {"aliases", "controls", "expression_leaf_controls", "schema_version"},
        "quiet fixture root",
    )
    if _integer(root["schema_version"], "quiet fixture schema_version") != 1:
        raise InterfaceManifestError("quiet fixture schema_version must be integer 1")
    aliases = root["aliases"]
    controls = root["controls"]
    leaf_controls = root["expression_leaf_controls"]
    if (
        not isinstance(aliases, list)
        or not isinstance(controls, list)
        or not isinstance(leaf_controls, list)
    ):
        raise InterfaceManifestError(
            "quiet fixture aliases/controls/expression_leaf_controls must be arrays"
        )
    alias_names: list[str] = []
    for ordinal, value in enumerate(aliases):
        row = _exact_keys(
            value, {"name", "source_expression", "width"},
            f"quiet fixture aliases[{ordinal}]",
        )
        alias_names.append(
            _string(row["name"], f"quiet fixture aliases[{ordinal}].name", nonempty=True)
        )
        _string(
            row["source_expression"],
            f"quiet fixture aliases[{ordinal}].source_expression",
            nonempty=True,
        )
        _integer(row["width"], f"quiet fixture aliases[{ordinal}].width", minimum=1)
    if len(set(alias_names)) != len(alias_names):
        raise InterfaceManifestError("quiet fixture contains duplicate alias names")
    leaf_keys: list[tuple[str, str]] = []
    for ordinal, value in enumerate(leaf_controls):
        row = _exact_keys(
            value, {"alias", "leaf", "polarity"},
            f"quiet fixture expression_leaf_controls[{ordinal}]",
        )
        alias = _string(
            row["alias"],
            f"quiet fixture expression_leaf_controls[{ordinal}].alias",
            nonempty=True,
        )
        leaf = _string(
            row["leaf"],
            f"quiet fixture expression_leaf_controls[{ordinal}].leaf",
            nonempty=True,
        )
        polarity = _string(
            row["polarity"],
            f"quiet fixture expression_leaf_controls[{ordinal}].polarity",
        )
        if alias not in alias_names:
            raise InterfaceManifestError(
                f"quiet fixture expression leaf names unknown alias {alias!r}"
            )
        if polarity not in {"positive", "negative"}:
            raise InterfaceManifestError(
                f"quiet fixture expression leaf {ordinal} has invalid polarity"
            )
        leaf_keys.append((alias, leaf))
    if len(set(leaf_keys)) != len(leaf_keys):
        raise InterfaceManifestError("quiet fixture contains duplicate expression leaves")
    control_keys: list[tuple[str, str]] = []
    for ordinal, value in enumerate(controls):
        row = _exact_keys(
            value, {"equation", "operand", "polarity"},
            f"quiet fixture controls[{ordinal}]",
        )
        equation = _string(row["equation"], f"quiet fixture controls[{ordinal}].equation")
        operand = _string(row["operand"], f"quiet fixture controls[{ordinal}].operand", nonempty=True)
        polarity = _string(row["polarity"], f"quiet fixture controls[{ordinal}].polarity")
        if equation not in {"data_quiet", "quiet_o"}:
            raise InterfaceManifestError(f"quiet fixture control {ordinal} has invalid equation")
        if polarity not in {"positive", "negative"}:
            raise InterfaceManifestError(f"quiet fixture control {ordinal} has invalid polarity")
        control_keys.append((equation, operand))
    if len(set(control_keys)) != len(control_keys):
        raise InterfaceManifestError("quiet fixture contains duplicate equation/operand controls")
    return payload


def quiet_expected_controls() -> tuple[dict[str, str], ...]:
    rows: list[dict[str, str]] = []
    for equation, positive, negative in (
        ("data_quiet", _DATA_QUIET_POSITIVE, _DATA_QUIET_NEGATIVE),
        ("quiet_o", _PUBLIC_QUIET_POSITIVE, _PUBLIC_QUIET_NEGATIVE),
    ):
        rows.extend(
            {"equation": equation, "operand": operand, "polarity": "positive"}
            for operand in positive
        )
        rows.extend(
            {"equation": equation, "operand": operand, "polarity": "negative"}
            for operand in negative
        )
    return tuple(rows)


def quiet_term_text(operand: str, polarity: str) -> str:
    if polarity == "positive":
        return operand
    if operand in _VECTOR_REDUCTION_OPERANDS:
        return f"!(|{operand})"
    return f"!{operand}"


def _split_boolean_terms(expression: str) -> list[str]:
    expression = expression.strip()
    # Strip one pair surrounding the complete conjunction, but not !(|vector).
    if expression.startswith("(") and expression.endswith(")"):
        try:
            if _balanced_end(expression, 0, "(", ")") == len(expression) - 1:
                expression = expression[1:-1]
        except InterfaceManifestError:
            pass
    spans: list[tuple[int, int]] = []
    stack: list[str] = []
    start = 0
    i = 0
    while i < len(expression):
        char = expression[i]
        if char in "([{":
            stack.append(char)
        elif char in ")]}":
            if not stack:
                raise InterfaceManifestError("unbalanced quiet expression")
            stack.pop()
        elif char == "&" and i + 1 < len(expression) and expression[i + 1] == "&" and not stack:
            spans.append((start, i))
            start = i + 2
            i += 1
        i += 1
    if stack:
        raise InterfaceManifestError("unbalanced quiet expression")
    spans.append((start, len(expression)))
    return [_normalize_sv_expression(expression[a:b]) for a, b in spans if expression[a:b].strip()]


def _assignment_rhs(masked: str, name: str) -> str:
    matches = list(re.finditer(r"\bassign\s+" + re.escape(name) + r"\s*=\s*(.*?);", masked, re.S))
    if len(matches) != 1:
        raise InterfaceManifestError(
            f"quiet contract requires exactly one continuous assignment to {name!r}, found {len(matches)}"
        )
    return matches[0].group(1)


def audit_quiet_contract(source: str, *, require_source_map: bool = True) -> None:
    masked = _mask_comments(source)
    errors: list[str] = []
    for equation, positive, negative in (
        ("data_quiet", _DATA_QUIET_POSITIVE, _DATA_QUIET_NEGATIVE),
        ("quiet_o", _PUBLIC_QUIET_POSITIVE, _PUBLIC_QUIET_NEGATIVE),
    ):
        try:
            actual = _split_boolean_terms(_assignment_rhs(masked, equation))
        except InterfaceManifestError as exc:
            errors.append(str(exc))
            continue
        expected = [
            _normalize_sv_expression(quiet_term_text(operand, "positive"))
            for operand in positive
        ] + [
            _normalize_sv_expression(quiet_term_text(operand, "negative"))
            for operand in negative
        ]
        missing = [term for term in expected if term not in actual]
        extra = [term for term in actual if term not in expected]
        duplicates = sorted({term for term in actual if actual.count(term) > 1})
        if missing or extra or duplicates or len(actual) != len(expected):
            errors.append(
                f"{equation} operand/polarity mismatch: missing={missing}, extra={extra}, duplicates={duplicates}"
            )
    if require_source_map:
        expected_map = dict(QUIET_SOURCE_MAP)
        expected_map.update(QUIET_CONTROL_SOURCE_MAP)
        vector_aliases = {
            "q_class_rsp_valid", "q_bilerp_req_valid", "q_bilerp_rsp_valid"
        }
        for alias in expected_map:
            declarations = re.findall(
                r"\blogic\s*(\[[^\]]+\])?\s+" + re.escape(alias) + r"\s*;",
                masked,
            )
            expected_dimension = "[3:0]" if alias in vector_aliases else ""
            actual_dimensions = [_normalize_sv_expression(item or "") for item in declarations]
            if len(actual_dimensions) != 1 or actual_dimensions[0] != expected_dimension:
                errors.append(
                    f"{alias} declaration differs: expected logic "
                    f"{expected_dimension + ' ' if expected_dimension else ''}{alias};, "
                    f"found dimensions={actual_dimensions!r}"
                )
        assigned_q = set(
            re.findall(r"\bassign\s+(q_[A-Za-z_$][A-Za-z0-9_$]*)\s*=", masked)
        )
        expected_q = set(QUIET_SOURCE_MAP)
        if assigned_q != expected_q:
            errors.append(
                "quiet source-map aliases differ: "
                f"missing={sorted(expected_q - assigned_q)}, extra={sorted(assigned_q - expected_q)}"
            )
        for alias, expected_rhs in expected_map.items():
            try:
                rhs = _assignment_rhs(masked, alias)
            except InterfaceManifestError as exc:
                errors.append(str(exc))
                continue
            if "." in _normalize_sv_expression(rhs):
                errors.append(f"{alias} uses forbidden hierarchical child-state reference {rhs.strip()!r}")
            if _normalize_sv_expression(rhs) != _normalize_sv_expression(expected_rhs):
                errors.append(
                    f"{alias} source differs: expected={expected_rhs!r}, actual={rhs.strip()!r}"
                )
            lhs_pattern = re.compile(
                r"(?<![.\w])" + re.escape(alias) + r"\s*(?:\[[^\]]+\]\s*)?(?:<=|=(?!=))"
            )
            if len(lhs_pattern.findall(masked)) != 1:
                errors.append(f"{alias} does not have exactly one driver")
    if errors:
        raise InterfaceManifestError("quiet contract mismatch: " + "; ".join(errors))


REQUIRED_ROOT_ABI_PORTS = {
    "frame_fault_clear_valid_i": ("input", 1),
    "frame_fault_clear_ready_o": ("output", 1),
    "frame_fault_o": ("output", 1),
}


def audit_required_root_ports(source: ModuleSourceView) -> None:
    by_name = {port.name: port for port in source.ports}
    errors: list[str] = []
    for name, (direction, width) in REQUIRED_ROOT_ABI_PORTS.items():
        port = by_name.get(name)
        if port is None:
            errors.append(f"missing required root ABI port {name}")
            continue
        if port.direction != direction or port.bit_width != width:
            errors.append(
                f"root ABI port {name} must be {direction} width {width}, "
                f"found {port.direction} width {port.bit_width}"
            )
    if errors:
        raise InterfaceManifestError("root interface contract mismatch: " + "; ".join(errors))


def audit_production_interface(
    source: ModuleSourceView,
    snapshots: SnapshotSet,
    qualified_constants: Mapping[str, int],
) -> None:
    values = {item.name: (item.selected_value.kind, item.selected_value.text) for item in source.parameters}
    if values != PRODUCTION_PARAMETER_VALUES:
        raise InterfaceManifestError(
            "production parameter set/value contract differs: "
            f"expected={PRODUCTION_PARAMETER_VALUES!r}, actual={values!r}"
        )
    names = {port.name for port in source.ports}
    if "frag_invw24_i" not in names or "frag_depth_i" in names:
        raise InterfaceManifestError(
            "versioned top must expose frag_invw24_i and must not expose frag_depth_i"
        )
    audit_required_root_ports(source)
    top_text = snapshots[source.source_path].raw.decode("utf-8")
    audit_quiet_contract(top_text, require_source_map=True)

    required_leaf_ports = {
        "fpga/rtl/texture/zhao_texture_early_desc_v2.sv": {
            "desc_pad_fault_o": ("output", "logic", "variable", False, 32),
        },
        "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv": {
            "sheet_rsp_owed_o": ("output", "logic", "variable", False, 1),
        },
        "fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv": {
            "cfg_loader_idle_o": ("output", "logic", "variable", False, 1),
            "binding_crc_busy_o": ("output", "logic", "variable", False, 1),
            "binding_seal_pending_o": ("output", "logic", "variable", False, 1),
        },
    }
    for path, expected_ports in required_leaf_ports.items():
        module_name = Path(path).stem
        raw = snapshots[path].raw
        leaf = parse_module_source(
            raw,
            source_path=path,
            module_name=module_name,
            qualified_constants=qualified_constants,
        )
        by_name = {port.name: port for port in leaf.ports}
        for name, expected in expected_ports.items():
            port = by_name.get(name)
            if port is None:
                raise InterfaceManifestError(f"{module_name} is missing required port {name}")
            actual = (
                port.direction, port.declared_type, port.net_or_var, port.signed, port.bit_width
            )
            if actual != expected:
                raise InterfaceManifestError(
                    f"{module_name}.{name} differs: expected={expected!r}, actual={actual!r}"
                )


def build_manifest_artifact(
    *,
    repo_root: Path,
    top_module: str,
    source_paths: Sequence[str],
    overrides: Mapping[str, str] | None = None,
    verilator: str | None = None,
    production: bool | None = None,
) -> ManifestBuild:
    repo_root = repo_root.resolve()
    production = top_module == PRODUCTION_TOP if production is None else production
    if production and top_module != PRODUCTION_TOP:
        raise InterfaceManifestError("production mode requires zhao_texture_island_v3_top")
    normalized_paths = [validate_repo_path(path, f"source path {index}") for index, path in enumerate(source_paths)]
    if not normalized_paths:
        raise InterfaceManifestError("source closure must not be empty")
    snapshot_paths = list(normalized_paths) + [
        spec["path"] for spec in CUSTOM_TOOL_SPECS.values()
    ]
    snapshots = SnapshotSet.capture(repo_root, snapshot_paths)
    qualified_constants = resolve_package_integral_constants(
        snapshots, normalized_paths
    )
    top_path = normalized_paths[-1]
    top_snapshot = snapshots[top_path]
    source_view = parse_module_source(
        top_snapshot.raw,
        source_path=top_path,
        module_name=top_module,
        overrides=overrides,
        qualified_constants=qualified_constants,
    )
    if top_module == SCHEMA_FIXTURE_TOP:
        duplicate_purpose = SCHEMA_FIXTURE_PURPOSE
    elif top_module == PRODUCTION_TOP:
        duplicate_purpose = PRODUCTION_INTERFACE_PURPOSE
    else:
        duplicate_purpose = None
    elaborated, version, argv = run_verilator_query(
        repo_root=repo_root,
        top_module=top_module,
        parameters=source_view.parameters,
        source_paths=normalized_paths,
        snapshots=snapshots,
        duplicate_purpose=duplicate_purpose,
        verilator=verilator,
    )
    compare_source_and_elaboration(source_view, elaborated)
    closure_rows = validate_source_closure(
        repo_root=repo_root,
        source_paths=normalized_paths,
        top_module=top_module,
        elaborated_module_paths=elaborated.module_source_paths,
        production=production,
        snapshots=snapshots,
    )
    if production:
        audit_production_interface(source_view, snapshots, qualified_constants)

    tools: dict[str, object] = {}
    for role, spec in CUSTOM_TOOL_SPECS.items():
        snapshot = snapshots[spec["path"]]
        tools[role] = {
            "name": spec["name"],
            "path": spec["path"],
            "sha256": snapshot.sha256,
            "version": spec["version"],
        }
    tools["elaborator"] = {
        "name": "Verilator",
        "version": version,
        "version_command": ["verilator", "--version"],
    }
    tools["runtime"] = {"name": "CPython", "version": platform.python_version()}

    top_raw = top_snapshot.raw
    payload: dict[str, object] = {
        "elaboration": {
            "argv": argv,
            "cwd": ".",
            "parameter_overrides": [
                {"name": item.name, "value": item.selected_value.manifest()}
                for item in source_view.parameters
            ],
            "top_module": top_module,
        },
        "hashes": {
            "canonical_interface_sha256": "0" * 64,
            "module_declaration_sha256": source_view.module_declaration_sha256,
            "top_source_sha256": sha256_bytes(top_raw),
        },
        "module": {
            "declaration_end_byte_exclusive": source_view.declaration_end_byte_exclusive,
            "declaration_start_byte": source_view.declaration_start_byte,
            "name": source_view.name,
            "source_path": source_view.source_path,
        },
        "parameters": [item.manifest() for item in source_view.parameters],
        "ports": [item.manifest() for item in source_view.ports],
        "schema_id": SCHEMA_ID,
        "schema_version": SCHEMA_VERSION,
        "source_closure": closure_rows,
        "tools": tools,
    }
    payload["hashes"]["canonical_interface_sha256"] = canonical_interface_sha256(payload)  # type: ignore[index]
    validate_manifest_payload(
        payload,
        expected_top=top_module,
        expected_source_closure=normalized_paths,
    )
    return ManifestBuild(payload=payload, snapshots=snapshots)


def build_manifest_payload(
    *,
    repo_root: Path,
    top_module: str,
    source_paths: Sequence[str],
    overrides: Mapping[str, str] | None = None,
    verilator: str | None = None,
    production: bool | None = None,
) -> dict[str, object]:
    artifact = build_manifest_artifact(
        repo_root=repo_root,
        top_module=top_module,
        source_paths=source_paths,
        overrides=overrides,
        verilator=verilator,
        production=production,
    )
    artifact.verify_live_unchanged()
    return artifact.payload


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="one SystemVerilog source to parse")
    parser.add_argument("--module", required=True)
    parser.add_argument("--source-path", help="repo-relative path recorded in the view")
    parser.add_argument("-G", "--parameter", action="append", default=[], metavar="NAME=VALUE")
    return parser


def parse_parameter_arguments(values: Sequence[str]) -> dict[str, str]:
    result: dict[str, str] = {}
    for value in values:
        if "=" not in value:
            raise InterfaceManifestError(f"parameter override must be NAME=VALUE: {value!r}")
        name, expression = value.split("=", 1)
        if not _IDENTIFIER_RE.fullmatch(name) or not expression:
            raise InterfaceManifestError(f"malformed parameter override {value!r}")
        if name in result:
            raise InterfaceManifestError(f"duplicate parameter override {name!r}")
        result[name] = expression
    return result


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        source_path = args.source_path or args.source.as_posix()
        view = parse_module_source(
            args.source.read_bytes(),
            source_path=source_path,
            module_name=args.module,
            overrides=parse_parameter_arguments(args.parameter),
        )
        payload = {
            "module": {
                "declaration_end_byte_exclusive": view.declaration_end_byte_exclusive,
                "declaration_start_byte": view.declaration_start_byte,
                "name": view.name,
                "source_path": view.source_path,
            },
            "module_declaration_sha256": view.module_declaration_sha256,
            "parameters": [item.manifest() for item in view.parameters],
            "ports": [item.manifest() for item in view.ports],
        }
        print(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True))
    except (OSError, InterfaceManifestError) as exc:
        print(f"texture-v3 interface parse failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
