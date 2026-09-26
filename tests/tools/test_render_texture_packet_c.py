#!/usr/bin/env python3
"""Static closure controls for R0 Packet C's synthetic raster/texture stage."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import sys
import unittest


REPO = Path(__file__).resolve().parents[2]
STAGE = REPO / "fpga" / "rtl" / "raster" / "zhao_raster_texture_stage_v3.sv"
SOURCE_MANIFEST = (
    REPO / "tests" / "raster" / "raster_texture_stage_v3.sources.txt"
)

PACKET_B_SOURCES = (
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
    "fpga/rtl/texture/zhao_texture_mosaic_hold.sv",
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
    "fpga/rtl/texture/zhao_texture_sheetmod.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
)
PACKET_C_SOURCES = (
    PACKET_B_SOURCES[:1] +
    ("fpga/rtl/common/zhao_skid2.sv",) +
    PACKET_B_SOURCES[1:] +
    (
        "fpga/rtl/raster/zhao_raster_blend.sv",
        "fpga/rtl/raster/zhao_raster_blend_prod.sv",
        "fpga/rtl/raster/zhao_raster_blend_fin.sv",
        "fpga/rtl/raster/zhao_raster_fragment.sv",
        "tests/mutants/zhao_raster_texture_stage_v3_mutants.sv",
        "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv",
        "tests/raster/tb_raster_texture_stage_v3.sv",
    )
)

# Packet E legitimately changes the V3 implementation while preserving its public
# schema; these hashes pin that refreshed authority. Shell/accounting bytes remain
# the protected Packet-C values.
# Refreshed twice on 2026-09-18, for two timing changes inside
# zhao_texture_binding_resolver_v2: the page banks moving into inferred M10K,
# then the row gaining a stored legality bit and the CRC verdict gaining its own
# state. Each time the manifest was field-diffed against the committed one
# before this line was touched, and each time the result was the same shape:
#
#     1,949 leaves before and after; 3 changed, 0 added, 0 removed
#       source_closure[21]          the resolver's own source hash
#       tools/parser                its duplicate-marker fingerprint
#       canonical_interface_sha256  derived from the two above
#
# So no port, parameter, dtype or elaboration value moved, and the PUBLIC SCHEMA
# this constant protects is untouched; only the provenance of the bytes behind
# it did. That distinction is the whole reason to re-pin rather than widen.
#
# The duplicate-marker count is 105 both times. It briefly went to 107 while the
# stored legality bit was written as a packed struct, whose two members are
# names that already occur in the closure; it is a packed vector instead, for
# the reason recorded in tools/rtl/texture_v3_interface_parser.py's pin.
# Island + interface-manifest hashes refreshed 2026-09-18 for the two
# texture timing changes (cache-pipe mask ordering, COMBINE fence).
# The field diff that justifies it is recorded once, beside the pin in
# tests/tools/test_render_texture_packet_e.py -- five hash fields, ports
# 119 -> 119, parameters 16 -> 16.
# REFRESHED 2026-09-26 (TERRAINTEX): the island gained `u_mosaic_hold`, the
# per-owner store that holds TEXTURE.MOSAIC's pick until the sample that
# asked for it can leave, and the gate in front of the binding resolver.
# Body only -- ports 120 -> 120, parameters 16 -> 16 and
# module_declaration_sha256 byte-identical at 8766d3dd2a88dce...; the
# interface artifact moved only because it hashes the island's source and
# the source closure, which gained zhao_texture_mosaic_hold.sv.
INTERFACE_SHA256 = "6377b5f75da0977eb55b3bf1f9beb5fa673528e19729b18837f487f1138f3e1b"  # R9: cnt_texture_samples_o
PACKET_B_TOP_SHA256 = "18b5d63e560dd32a75836085f1625d83cd3fc565deb94cf8756e1cb62789a599"  # R9: cnt_texture_samples_o
# RE-PINNED under owner ruling R39 (provisional, 2026-09-19): the ONLY change
# to the protected V1 shell is R32's tie-off of MEM.GUARD's new region inputs,
# 3 lines x 4 zhao_mem_guard instances = 12 lines, each
#   .res_valid (1'b0) / .res_base (32'd0) / .res_span (32'd0)   // TIE: ...
# (the R32 write arm names TERRAIN_BUILD alone; V1 has no such client).
# No behaviour moves. Previous pin: 00fdd2387ffea985...
PROTECTED_SHELL_SHA256 = "9ab87fd9ceeb5efb1333c2023cc4cf9a565b6f4d75633facfa33c9f6640b91cc"
# RE-PINNED under owner ruling R39 (2026-09-19), for its OWN reason: this
# pin was already stale at 8736ba33. zhao_prod_top.sv is GENERATED by
# tools/quartus/gen_prod_top.py and must be regenerated after any port change
# (CLAUDE.md); it moved with R17/R18/R32 (MEM.GUARD, MEASURE.TOKENS,
# CMD.EXEC ports) and the post lane. Previous pin: 96121488fabef50e...
PROD_TOP_SHA256 = "bd49c6b3f997eb194a8f681f5d4961586ed2e7aa1919d0947efa259c1ff27668"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_prod_top_bytes(raw: bytes) -> None:
    if hashlib.sha256(raw).hexdigest() != PROD_TOP_SHA256:
        raise AssertionError("Packet-C changed the pre-packet production-top bytes")


def validate_source_rows(rows: tuple[str, ...]) -> None:
    if rows != PACKET_C_SOURCES:
        raise AssertionError("Packet-C source manifest is not the exact ordered inventory")
    if len(rows) != len(set(rows)):
        raise AssertionError("Packet-C source manifest contains a duplicate")
    for row in rows:
        if not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", row):
            raise AssertionError("invalid Packet-C source path: " + repr(row))
        if ".." in Path(row).parts or not (REPO / row).is_file():
            raise AssertionError("missing/escaping Packet-C source path: " + row)


def read_source_manifest() -> tuple[str, ...]:
    text = SOURCE_MANIFEST.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        raise AssertionError("Packet-C source manifest needs one final newline")
    rows = tuple(text.splitlines())
    if not rows or any(not row or row != row.strip() or row.startswith("#") for row in rows):
        raise AssertionError("Packet-C source manifest contains a blank/comment/padded row")
    validate_source_rows(rows)
    return rows


def validate_stage_shape(text: str) -> None:
    if text.count("module zhao_raster_texture_stage_v3 #(") != 1:
        raise AssertionError("Packet-C stage must have one canonical parameterized module")
    if text.count("zhao_texture_island_v3_top #(") != 1:
        raise AssertionError("Packet-C stage must instantiate exactly one selected V3 island")
    if "zhao_texture_v3own" in text:
        raise AssertionError("Packet-C stage must not add a second lifecycle owner")
    required = (
        "parameter bit MIGRATION_SHADOWS = 1'b0",
        "parameter bit BILERP_DSP2 = 1'b0",
        ".MIGRATION_SHADOWS(MIGRATION_SHADOWS)",
        ".BILERP_DSP2(BILERP_DSP2)",
        "import zhao_render_texture_pkg::*;",
        "unpack_raster_pretex",
        "input  logic [489:0] cand_data_i",
        "output logic         lifetime_structural_fault_o",
        "logic         retire_head_v_q;",
        "assign retire_head_reload_credit_w = sequence_abort_q || frag_ready_i;",
        "assign v3_out_ready_w = !retire_head_v_q || retire_head_reload_credit_w;",
        "assign quiet_o = v3_quiet_w && !retire_head_v_q;",
        ".lifetime_structural_fault_o(lifetime_structural_fault_o)",
        "retire_head_ctx_q       <= v3_out_retire_ctx_w;",
    )
    for marker in required:
        if marker not in text:
            raise AssertionError("Packet-C stage lost required shape: " + marker)
    if not re.search(r"output\s+logic\s+\[31:0\]\s+sequence_drop_count_o", text):
        raise AssertionError("Packet-C stage lost 32-bit sequence-drop output")


def active_cmake_text(text: str) -> str:
    """Remove line/bracket comments so commented registrations cannot pass."""
    bracket = re.compile(r"#\[(=*)\[.*?\]\1\]", re.DOTALL)
    text = bracket.sub(
        lambda match: "".join("\n" if char == "\n" else " "
                              for char in match.group(0)),
        text,
    )
    active: list[str] = []
    for line in text.splitlines(keepends=True):
        quoted = False
        escaped = False
        comment_at = None
        for index, char in enumerate(line):
            if escaped:
                escaped = False
                continue
            if char == "\\" and quoted:
                escaped = True
                continue
            if char == '"':
                quoted = not quoted
            elif char == "#" and not quoted:
                comment_at = index
                break
        if comment_at is None:
            active.append(line)
        else:
            active.append(line[:comment_at] + ("\n" if line.endswith("\n") else ""))
    return "".join(active)


def validate_cmake_registration(text: str) -> None:
    active = active_cmake_text(text)
    packet_c_start = "set(ZHAO_PACKET_C_SOURCE_MANIFEST"
    if active.count(packet_c_start) != 1:
        raise AssertionError("Packet-C CMake section marker is not exact/unique")
    section_start = active.index(packet_c_start)
    packet_d_start = "set(ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST"
    section_end = active.find(packet_d_start, section_start)
    if section_end < 0:
        section_end = len(active)
    active = active[section_start:section_end]
    required_once = (
        packet_c_start,
        "${CMAKE_CURRENT_SOURCE_DIR}/raster/raster_texture_stage_v3.sources.txt)",
        'file(READ "${ZHAO_PACKET_C_SOURCE_MANIFEST}" ZHAO_PACKET_C_SOURCE_MANIFEST_RAW)',
        "Packet-C source manifest contains a blank record",
        'file(STRINGS "${ZHAO_PACKET_C_SOURCE_MANIFEST}"',
        "foreach(relative_source IN LISTS ZHAO_PACKET_C_RELATIVE_SOURCES)",
        'list(FIND ZHAO_PACKET_C_SEEN "${relative_source}" duplicate_index)',
        'if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")',
        'list(APPEND ZHAO_PACKET_C_SOURCES "${CMAKE_SOURCE_DIR}/${relative_source}")',
        "Packet-C source manifest must contain exactly 36 SV paths",
        "Packet-C source manifest lost package-first/mutant-stage/top-last order",
        "add_executable(pc_dir raster/raster_texture_stage_v3_directed.cpp)",
        "add_executable(pc_seq raster/raster_texture_stage_v3_directed.cpp)",
        "add_executable(pc_old raster/raster_texture_stage_v3_directed.cpp)",
        "target_compile_definitions(pc_seq PRIVATE PACKET_C_EXPECT_IDENTITY_ABORT=1)",
        "target_compile_definitions(pc_old PRIVATE PACKET_C_EXPECT_OLD_READY=1)",
        "-DZHAO_PACKET_C_MUTANT_IDENTITY_ONLY)",
        "-DZHAO_PACKET_C_MUTANT_OLD_READY)",
        "add_test(NAME raster_texture_stage_v3_directed COMMAND pc_dir)",
        "add_test(NAME raster_texture_stage_v3_identity_abort_control COMMAND pc_seq)",
        "add_test(NAME raster_texture_stage_v3_old_ready_control COMMAND pc_old)",
        "add_test(NAME raster_texture_stage_v3_registration_static",
    )
    for marker in required_once:
        if active.count(marker) != 1:
            raise AssertionError("Packet-C CMake marker is not exact/unique: " + marker)
    if active.count("TOP_MODULE tb_raster_texture_stage_v3") != 3:
        raise AssertionError("Packet-C must elaborate the exact wrapper three times")
    if active.count("SOURCES ${ZHAO_PACKET_C_SOURCES}") != 3:
        raise AssertionError("Packet-C profiles do not share one source authority")
    if active.count("-GMIGRATION_SHADOWS=0") != 3:
        raise AssertionError("Packet-C profiles are not all explicit production shape")
    if "-GMIGRATION_SHADOWS=1" in active:
        raise AssertionError("Packet-C section contains a laboratory shadow profile")
    if active.count("target_link_libraries(pc_") != 3 or active.count(
            "PRIVATE zhao_harness zhao_zref") != 3:
        raise AssertionError("Packet-C profiles do not all link harness and zref")
    if active.count('LABELS "fast;nightly;packet-c;mutant"') != 2:
        raise AssertionError("Packet-C mutant labels are incomplete")
    if active.count('FAIL_REGULAR_EXPRESSION "packet-c directed FAIL"') != 2:
        raise AssertionError("Packet-C mutant tests do not fail on ordinary errors")
    if "target_compile_definitions(pc_dir" in active:
        raise AssertionError("healthy Packet-C target unexpectedly selects a mutant")


class PacketCClosureTests(unittest.TestCase):
    def test_exact_test_only_source_manifest(self) -> None:
        rows = read_source_manifest()
        self.assertEqual(
            tuple(row for row in rows if row in PACKET_B_SOURCES),
            PACKET_B_SOURCES,
        )
        self.assertEqual(len(rows), 36)
        self.assertEqual(
            rows[-3:],
            (
                "tests/mutants/zhao_raster_texture_stage_v3_mutants.sv",
                "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv",
                "tests/raster/tb_raster_texture_stage_v3.sv",
            ),
        )

    def test_source_manifest_detectors_fire(self) -> None:
        mutations = (
            PACKET_C_SOURCES[:-1],
            PACKET_C_SOURCES + (PACKET_C_SOURCES[-1],),
            PACKET_C_SOURCES[:2] + (PACKET_C_SOURCES[0],) + PACKET_C_SOURCES[2:],
            (PACKET_C_SOURCES[1], PACKET_C_SOURCES[0]) + PACKET_C_SOURCES[2:],
            PACKET_C_SOURCES[:-1] + ("tests/raster/not_real_packet_c.sv",),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation[-1]):
                with self.assertRaises(AssertionError):
                    validate_source_rows(mutation)

    def test_cmake_registration_is_exact_and_active(self) -> None:
        cmake = (REPO / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
        validate_cmake_registration(cmake)

    def test_cmake_registration_detectors_fire(self) -> None:
        cmake = (REPO / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
        section = "set(ZHAO_PACKET_C_SOURCE_MANIFEST"
        split = cmake.index(section)
        prefix, packet_c = cmake[:split], cmake[split:]
        mutations = (
            prefix + packet_c.replace(
                "-GMIGRATION_SHADOWS=0", "-GMIGRATION_SHADOWS=1", 1
            ),
            prefix + packet_c.replace("SOURCES ${ZHAO_PACKET_C_SOURCES}", "SOURCES", 1),
            prefix + packet_c.replace(
                "Packet-C source manifest contains a blank record",
                "Packet-C source manifest",
                1,
            ),
            prefix + packet_c.replace(
                'list(FIND ZHAO_PACKET_C_SEEN "${relative_source}" duplicate_index)',
                "",
                1,
            ),
            prefix + packet_c.replace(
                "PRIVATE zhao_harness zhao_zref", "PRIVATE zhao_harness", 1
            ),
            prefix + packet_c.replace(
                'FAIL_REGULAR_EXPRESSION "packet-c directed FAIL"', "", 1
            ),
            prefix + packet_c.replace(
                "-DZHAO_PACKET_C_MUTANT_OLD_READY)",
                "-DZHAO_PACKET_C_MUTANT_IDENTITY_ONLY)",
                1,
            ),
            prefix + packet_c.replace(
                "add_test(NAME raster_texture_stage_v3_directed COMMAND pc_dir)",
                "# add_test(NAME raster_texture_stage_v3_directed COMMAND pc_dir)",
                1,
            ),
        )
        for mutation in mutations:
            with self.assertRaises(AssertionError):
                validate_cmake_registration(mutation)

    def test_stage_uses_typed_packet_and_one_selected_owner_tree(self) -> None:
        text = STAGE.read_text(encoding="utf-8")
        validate_stage_shape(text)
        for marker in (
            "zhao_texture_island_v3_top #(",
            ".MIGRATION_SHADOWS(MIGRATION_SHADOWS)",
            "unpack_raster_pretex",
        ):
            with self.subTest(marker=marker):
                with self.assertRaises(AssertionError):
                    validate_stage_shape(text.replace(marker, "", 1))
        with self.assertRaises(AssertionError):
            validate_stage_shape(text + "\n  zhao_texture_island_v3_top #(\n")
        with self.assertRaisesRegex(AssertionError, "lost required shape"):
            validate_stage_shape(text.replace(
                "assign retire_head_reload_credit_w = sequence_abort_q || frag_ready_i;",
                "assign retire_head_reload_credit_w = sequence_mismatch_w || frag_ready_i;",
                1,
            ))
        with self.assertRaisesRegex(AssertionError, "lost required shape"):
            validate_stage_shape(text.replace(
                "assign quiet_o = v3_quiet_w && !retire_head_v_q;",
                "assign quiet_o = v3_quiet_w;",
                1,
            ))

    def test_current_v3_artifacts_and_protected_shell_bytes_are_pinned(self) -> None:
        interface = REPO / "fpga" / "rtl" / "generated" / "zhao_texture_island_v3_top.interface.json"
        packet_b_top = REPO / "fpga" / "rtl" / "texture" / "zhao_texture_island_v3_top.sv"
        protected_shell = REPO / "fpga" / "rtl" / "common" / "zhao_shell_top.sv"
        self.assertEqual(sha256(interface), INTERFACE_SHA256)
        self.assertEqual(sha256(packet_b_top), PACKET_B_TOP_SHA256)
        self.assertEqual(sha256(protected_shell), PROTECTED_SHELL_SHA256)
        artifact = json.loads(interface.read_text(encoding="utf-8"))
        self.assertEqual(tuple(row["path"] for row in artifact["source_closure"]), PACKET_B_SOURCES)

    def test_stage_is_excluded_and_absent_from_production_closures(self) -> None:
        tools = REPO / "tools" / "quartus"
        if str(tools) not in sys.path:
            sys.path.insert(0, str(tools))
        import check_prod_manifest

        tops, excluded = check_prod_manifest.read_manifest(
            REPO / "design" / "prod_manifest.yml"
        )
        self.assertNotIn("zhao_raster_texture_stage_v3", tops)
        self.assertEqual(excluded["zhao_raster_texture_stage_v3"][0], "not-yet-adopted")
        self.assertIn("Packet D", excluded["zhao_raster_texture_stage_v3"][1])

        prod_top = REPO / "fpga" / "rtl" / "prod" / "zhao_prod_top.sv"
        forbidden = (
            REPO / "fpga" / "quartus" / "prod_fit_sources.txt",
            prod_top,
        )
        for path in forbidden:
            with self.subTest(path=path.relative_to(REPO)):
                self.assertNotIn("zhao_raster_texture_stage_v3", path.read_text(encoding="utf-8"))
        fit_targets = (REPO / "design" / "fit_targets.yml").read_text(encoding="utf-8")
        # This asserted `count(...) == 1` until 2026-09-18, and that was the
        # wrong instrument for the claim. `fit_targets.yml` is the
        # CHARACTERISATION list, not a production closure -- the production
        # claims are the two `assertNotIn`s above, against prod_fit_sources.txt
        # and zhao_prod_top.sv, and those still hold. Counting occurrences
        # across the whole file instead asserted "exactly one fit target names
        # this stage", which is a statement about how many characterisation
        # closures happen to contain it, and it went red the moment
        # zhao_shell_top_v2 was registered -- a composed shell that genuinely
        # instantiates the stage and must list it. Same category error as the
        # one corrected in test_render_texture_packet_d.py.
        #
        # The claim worth holding is that the stage is not itself a fit TOP,
        # because that is what "not adopted" means here.
        self.assertNotIn("- top: zhao_raster_texture_stage_v3\n", fit_targets)
        self.assertIn("/zhao_raster_texture_stage_v3.sv", fit_targets)
        validate_prod_top_bytes(prod_top.read_bytes())
        with self.assertRaises(AssertionError):
            validate_prod_top_bytes(prod_top.read_bytes() + b"\n")
        self.assertEqual(check_prod_manifest.check_top_fresh(), [])


if __name__ == "__main__":
    unittest.main()
