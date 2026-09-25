#!/usr/bin/env python3
"""G8A ATTR3/BIL2 registration and mutant-selector controls."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
BLOCK_REPORT = REPO / "reports/synthesis/zhao_block_fit.json"
BLOCKPATHS = REPO / "reports/synthesis/blockpaths"
ATTR_MAP_ROW = "zhao_attr_mul72x13_dsp3@g8a-dspr-attr3-map"
BIL_MAP_ROW = "zhao_texture_bilerp_lane_dsp2@g8a-dspr-bil2-map"
ATTR_MANIFEST = REPO / "tests/raster/raster_attrgrad_dsp3.sources.txt"
BIL_MANIFEST = REPO / "tests/texture/texture_bilerp_lane_dsp2.sources.txt"
CMAKE = REPO / "tests/CMakeLists.txt"
ATTRS = REPO / ".gitattributes"
MUL27 = REPO / "fpga/rtl/common/zhao_mul27_exact.sv"
ATTR_WORKER = REPO / "fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv"
ATTR_LANE = REPO / "fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv"
BIL_LANE = REPO / "fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv"
ATTR_MUTANTS = REPO / "tests/mutants/zhao_attr_dsp3_mutants.sv"
BIL_MUTANT = REPO / "tests/mutants/zhao_texture_bilerp_lane_dsp2_mutant.sv"
ATTR_DRIVER = REPO / "tests/raster/raster_attrgrad_dsp3_diff.cpp"
BIL_DRIVER = REPO / "tests/texture/texture_bilerp_lane_dsp2_diff.cpp"
TILE = REPO / "fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv"
STAGE = REPO / "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv"
ISLAND = REPO / "fpga/rtl/texture/zhao_texture_island_v3_top.sv"
BIN = REPO / "fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv"
G8A_TEMPLATE = REPO / "tools/quartus/templates/zhao_raster_texture_v3_fit_top.sv.in"
G8A_WRAPPER = REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv"
G8A_MANIFEST = REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.manifest.json"
PROD_MANIFEST = REPO / "design/prod_manifest.yml"
PROD_CHECKER = REPO / "tools/quartus/check_prod_manifest.py"
OWNERSHIP_CHECKER = REPO / "tools/quartus/check_ownership_roles.py"
BLOCK_RUNNER = REPO / "tools/quartus/run_block_fit.ps1"
TIMING3_RUNNER = REPO / "tools/quartus/run_g8a_timing3_fit.ps1"

EXPECTED_ATTR_SOURCES = (
    "fpga/rtl/common/zhao_mul27_exact.sv",
    "fpga/rtl/raster/zhao_raster_attrdiv_v2.sv",
    "fpga/rtl/raster/zhao_raster_attrgrad_v2.sv",
    "tests/mutants/zhao_attr_dsp3_mutants.sv",
    "fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv",
    "fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv",
    "tests/raster/tb_raster_attrgrad_dsp3_pair.sv",
)
EXPECTED_BIL_SOURCES = (
    "fpga/rtl/common/zhao_dual18_mul.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv",
    "tests/mutants/zhao_texture_bilerp_lane_dsp2_mutant.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv",
    "tests/texture/tb_texture_bilerp_lane_dsp2.sv",
)
EXPECTED_TESTS = (
    "dsp_rescue_attr3_radix2",
    "dsp_rescue_attr3_radix4",
    "dsp_rescue_attr3_signed_middle_mutant",
    "dsp_rescue_attr3_narrow_shift_mutant",
    "dsp_rescue_attr3_omit_base_y_mutant",
    "dsp_rescue_attr3_idle_early_mutant",
    "dsp_rescue_attr3_offset_ready_assertion",
    "dsp_rescue_bil2_diff",
    "dsp_rescue_bil2_collapse_resultb_mutant",
    "dsp_rescue_bil2_vertical_capture_mutant",
    "g8a_dsp_attr_mid_shift_selector_collision",
    "g8a_dsp_attr_mid_omit_selector_collision",
    "g8a_dsp_attr_shift_omit_selector_collision",
    "g8a_dsp_attr_mid_delay_selector_collision",
    "g8a_dsp_attr_mid_idle_selector_collision",
    "g8a_dsp_bil2_selector_collision",
    "g8a_dsp_rescue_registration_static",
)
EXPECTED_LF_ROWS = (
    "fpga/rtl/common/zhao_dual18_mul.sv text eol=lf",
    "fpga/rtl/common/zhao_mul27_exact.sv text eol=lf",
    "fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv text eol=lf",
    "fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv text eol=lf",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv text eol=lf",
    "tests/mutants/zhao_attr_dsp3_mutants.sv text eol=lf",
    "tests/mutants/zhao_texture_bilerp_lane_dsp2_mutant.sv text eol=lf",
    "tests/raster/raster_attrgrad_dsp3.sources.txt text eol=lf",
    "tests/raster/raster_attrgrad_dsp3_diff.cpp text eol=lf",
    "tests/raster/tb_raster_attrgrad_dsp3_pair.sv text eol=lf",
    "tests/texture/tb_texture_bilerp_lane_dsp2.sv text eol=lf",
    "tests/texture/texture_bilerp_lane_dsp2.sources.txt text eol=lf",
    "tests/texture/texture_bilerp_lane_dsp2_diff.cpp text eol=lf",
    "tests/tools/test_g8a_dsp_rescue.py text eol=lf",
)
COLLISIONS = {
    "attr-mid-shift": (
        "zhao_raster_attrgrad_dsp3",
        EXPECTED_ATTR_SOURCES,
        (
            "ZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE",
            "ZHAO_ATTR_DSP3_MUTANT_NARROW_HIGH_SHIFT",
        ),
        "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION",
        (),
    ),
    "attr-mid-omit": (
        "zhao_raster_attrgrad_dsp3",
        EXPECTED_ATTR_SOURCES,
        (
            "ZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE",
            "ZHAO_ATTR_DSP3_MUTANT_OMIT_BASE_Y",
        ),
        "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION",
        (),
    ),
    "attr-shift-omit": (
        "zhao_raster_attrgrad_dsp3",
        EXPECTED_ATTR_SOURCES,
        (
            "ZHAO_ATTR_DSP3_MUTANT_NARROW_HIGH_SHIFT",
            "ZHAO_ATTR_DSP3_MUTANT_OMIT_BASE_Y",
        ),
        "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION",
        (),
    ),
    "attr-mid-delay": (
        "zhao_raster_attrgrad_dsp3",
        EXPECTED_ATTR_SOURCES,
        (
            "ZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE",
            "ZHAO_ATTR_DSP3_MUTANT_DELAY_OFFSET_READY",
        ),
        "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION",
        (),
    ),
    "attr-mid-idle": (
        "zhao_raster_attrgrad_dsp3",
        EXPECTED_ATTR_SOURCES,
        (
            "ZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE",
            "ZHAO_ATTR_DSP3_MUTANT_IDLE_EARLY",
        ),
        "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION",
        (),
    ),
    "bil2": (
        "zhao_texture_bilerp_lane_dsp2",
        EXPECTED_BIL_SOURCES,
        (
            "ZHAO_BIL2_MUTANT_COLLAPSE_RESULTB",
            "ZHAO_BIL2_MUTANT_BYPASS_VERTICAL_CAPTURE",
        ),
        "ZHAO_BIL2_MUTANT_SELECTOR_COLLISION",
        ("ZHAO_DUAL18_BEHAVIORAL",),
    ),
}


def manifest_rows(path: Path, expected: tuple[str, ...]) -> tuple[str, ...]:
    raw = path.read_bytes()
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise AssertionError(f"{path.name} is not final-LF canonical text")
    rows = tuple(raw.decode("utf-8").splitlines())
    if rows != expected or len(rows) != len(set(rows)):
        raise AssertionError(f"{path.name} is not the exact ordered source closure")
    for row in rows:
        if (not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", row)
                or ".." in Path(row).parts or "\\" in row
                or not (REPO / row).is_file()):
            raise AssertionError(f"invalid or missing DSP source path: {row!r}")
    return rows


def require_once(text: str, markers: tuple[str, ...], label: str) -> None:
    for marker in markers:
        if text.count(marker) != 1:
            raise AssertionError(f"{label} marker is not exact/unique: {marker}")


def cmake_section(text: str) -> str:
    start = "# DSPR1 ATTR3 candidate:"
    end = "# Packet D2: old/V2 binner parity"
    if text.count(start) != 1 or text.count(end) != 1:
        raise AssertionError("G8A DSP CMake section boundary differs")
    return text[text.index(start):text.index(end)]


def validate_cmake(text: str) -> None:
    section = cmake_section(text)
    require_once(section, (
        "ATTR3 source manifest contains a blank record",
        "ATTR3 source manifest is not the exact ordered seven-file closure",
        "duplicate ATTR3 source-manifest path",
        "BIL2 source manifest contains a blank record",
        "BIL2 source manifest is not the exact ordered five-file closure",
        "duplicate BIL2 source-manifest path",
        "zhao_attr3_target(dspr_attr3_r2 2 --assert)",
        "zhao_attr3_target(dspr_attr3_r4 4 --assert)",
        "zhao_attr3_target(dspr_attr3_mid 2 -DZHAO_ATTR_DSP3_MUTANT_SIGNED_MIDDLE)",
        "zhao_attr3_target(dspr_attr3_shift 2 -DZHAO_ATTR_DSP3_MUTANT_NARROW_HIGH_SHIFT)",
        "zhao_attr3_target(dspr_attr3_omit 2 -DZHAO_ATTR_DSP3_MUTANT_OMIT_BASE_Y)",
        "zhao_attr3_target(dspr_attr3_idle 2 -DZHAO_ATTR_DSP3_MUTANT_IDLE_EARLY)",
        "zhao_attr3_target(dspr_attr3_offset_guard 2",
        "-DZHAO_ATTR_DSP3_MUTANT_DELAY_OFFSET_READY)",
        "EXPECT_ATTR3_OFFSET_GUARD=1",
        'PASS_REGULAR_EXPRESSION "ATTR3 OFFSET GUARD FIRED"',
        "-DZHAO_BIL2_MUTANT_COLLAPSE_RESULTB",
        "-DZHAO_BIL2_MUTANT_BYPASS_VERTICAL_CAPTURE",
        "EXPECT_BIL2_BYPASS_VERTICAL_CAPTURE=1",
        "function(zhao_g8a_dsp_selector_control NAME PROFILE)",
        "test_g8a_dsp_rescue.py -q",
        "G8A DSP required CTest inventory must contain exactly 17 names",
    ), "G8A DSP CMake")
    if section.count("-DZHAO_DUAL18_BEHAVIORAL") != 3:
        raise AssertionError("BIL2 behavioral backend is not selected by all controls")
    match = re.search(r"set\(ZHAO_G8A_DSP_REQUIRED_TESTS\n(.*?)\)", section, re.DOTALL)
    if match is None:
        raise AssertionError("G8A DSP required-test inventory is absent")
    names = tuple(line.strip() for line in match.group(1).splitlines())
    if names != EXPECTED_TESTS or len(names) != len(set(names)):
        raise AssertionError("G8A DSP required-test inventory differs")


def validate_attr_structure() -> None:
    leaf = MUL27.read_text(encoding="utf-8")
    worker = ATTR_WORKER.read_text(encoding="utf-8")
    lane = ATTR_LANE.read_text(encoding="utf-8")
    require_once(leaf, (
        "(* preserve_hierarchy *)\nmodule zhao_mul27_exact",
        "(* multstyle = \"dsp\" *) logic signed [53:0] product_c;",
        "assign product_c = a_i * b_i;",
    ), "signed27 leaf")
    require_once(worker, (
        "(* preserve_hierarchy *)\nmodule zhao_attr_mul72x13_dsp3",
        "assign a0_c = $signed({3'b000, in_a_i[23:0]});",
        "assign a1_c = `ZHAO_ATTR_DSP3_MIDDLE_EXT(in_a_i);",
        "assign ah_c = $signed({{3{in_a_i[71]}}, in_a_i[71:48]});",
        "assign b_c  = $signed({{14{in_b_i[12]}}, in_b_i});",
        "logic signed [37:0] p0_q, p1_q;",
        "logic signed [36:0] ph0_q, ph1_q;",
        "logic signed [61:0] s01_q;",
        "logic signed [84:0] product_q;",
        "s01_q <= p0_ext_c + (p1_ext_c <<< 24);",
    ), "ATTR3 worker")
    if len(re.findall(r"\bzhao_mul27_exact\s+u_p[01h]\b", worker)) != 3:
        raise AssertionError("ATTR3 worker does not contain exactly three signed27 leaves")
    require_once(lane, (
        "(* preserve_hierarchy *)\nmodule zhao_raster_attrgrad_dsp3",
        "zhao_attr_mul72x13_dsp3 u_mul",
        "mul_in_op_c = M_BASE_Y;",
        "mul_in_op_c = M_OFFSET;",
        "base_min_y0_r <= base_min_y0_r + (dndx_r >>> 1);",
        "base_min_y0_r <= base_min_y0_r + (dndy_r >>> 1);",
        "assign idle_o = `ZHAO_ATTR_DSP3_IDLE_EXPR(job_ready_o && !q_valid_o);",
        "offset_ready_r <= `ZHAO_ATTR_DSP3_OFFSET_READY_VALUE;",
        "logic [31:0] verify_mul_issued_q, verify_mul_retired_q;",
        "logic verify_offset_guard_fault_q;",
        "if (dv_rvalid && (st_r == S_ROW_WAIT) && !offset_ready_r)",
        "a_offset_ready_before_row_result : assert (!(dv_rvalid &&",
        "(verify_mul_issued_q == verify_mul_retired_q)",
        "a_idle_has_no_micro_work",
    ), "ATTR3 lane")
    if lane.count("mul_in_op_c = M_BASE_X;") != 2:
        raise AssertionError("ATTR3 BASE_X issue/default structure differs")


def validate_bil_structure() -> None:
    lane = BIL_LANE.read_text(encoding="utf-8")
    require_once(lane, (
        "(* preserve_hierarchy *)\nmodule zhao_texture_bilerp_lane_dsp2",
        "// synthesis translate_off",
        "if (b0_valid_q && !$isunknown({pu0_raw_c, pu1_raw_c})) begin",
        "logic signed [17:0] fu_18_c;",
        "fu_18_c  = $signed({10'b0, b0_fu_q});",
        "zhao_dual18_mul #(",
        ".AX_SIGNED(1'b1), .AY_SIGNED(1'b1),",
        ".BX_SIGNED(1'b1), .BY_SIGNED(1'b1)",
        ".resulta_o(pu0_raw_c), .resultb_o(pu1_raw_c)",
        "pv_c            = 27'(dv_c * fv_s_c);",
        "logic signed [17:0] b2_a_q;",
        "logic signed [26:0] b2_pv_q;",
        "b2_finish_a_c   = `ZHAO_BIL2_FINISH_A(b2_a_q, b1_a_q);",
        "b2_finish_pv_c  = `ZHAO_BIL2_FINISH_PV(b2_pv_q, pv_c);",
        "b2_sum_c        = (b2_a_ext_c <<< 8) + b2_finish_pv_c;",
        "out_o       = b2_filtered_c;",
        "b2_a_q    <= b1_a_q;",
        "b2_pv_q   <= pv_c;",
    ), "BIL2 lane")
    if "b2_filtered_q" in lane:
        raise AssertionError("BIL2 retained the old vertical-DSP-to-output register cone")
    if len(re.findall(r"\bzhao_dual18_mul\s*#", lane)) != 1:
        raise AssertionError("BIL2 lane does not contain exactly one packed horizontal pair")


def validate_mutant_guards() -> None:
    attr = ATTR_MUTANTS.read_text(encoding="utf-8")
    bil = BIL_MUTANT.read_text(encoding="utf-8")
    if attr.count('`error "ZHAO_ATTR_DSP3_MUTANT_SELECTOR_COLLISION"') != 5:
        raise AssertionError("ATTR3 selector sentinel does not guard all five mutants")
    if (attr.count("`ifdef ZHAO_ATTR_DSP3_MUTANT_ACTIVE") != 5 or
            attr.count("`define ZHAO_ATTR_DSP3_MUTANT_ACTIVE") != 5):
        raise AssertionError("ATTR3 mutants do not share one collision sentinel")
    if attr.count("`define ZHAO_ATTR_DSP3_MUTANT_DISABLE_LANE_ASSERTIONS") != 2:
        raise AssertionError("ATTR3 timing/idle controls do not suppress aborting assertions")
    require_once(attr, (
        "`define ZHAO_ATTR_DSP3_MIDDLE_EXT(value)",
        "`define ZHAO_ATTR_DSP3_RECOMBINE(s01, high)",
        "`define ZHAO_ATTR_DSP3_BASE_Y_RESULT(value) 96'sd0",
        "`define ZHAO_ATTR_DSP3_OFFSET_READY_VALUE 1'b0",
        "`define ZHAO_ATTR_DSP3_IDLE_EXPR(value) 1'b1",
        "`undef ZHAO_ATTR_DSP3_MUTANT_ACTIVE",
    ), "ATTR3 mutants")
    require_once(bil, (
        '`error "ZHAO_BIL2_MUTANT_SELECTOR_COLLISION"',
        "`define ZHAO_BIL2_RESULTB(resulta, resultb) resulta",
        "`define ZHAO_BIL2_FINISH_A(held, live) live",
        "`define ZHAO_BIL2_FINISH_PV(held, live) live",
    ), "BIL2 mutant")


def validate_driver_coverage() -> None:
    attr = ATTR_DRIVER.read_text(encoding="utf-8")
    bil = BIL_DRIVER.read_text(encoding="utf-8")
    require_once(attr, (
        "static void reset_after_gradient(const Job& j, int clocks_after_cov_ready)",
        "gradient_ready = dut->old_cov_ready_o && dut->new_cov_ready_o;",
        "for (int phase = 0; phase < 6; ++phase) reset_after_gradient(setup_probe, phase);",
        "if (dut->new_offset_guard_fault_o)",
        '"ATTR3 OFFSET GUARD FIRED at cycle %llu\\n"',
    ), "ATTR3 reset coverage")
    require_once(bil, (
        "std::unordered_set<uint64_t> random_tuples;",
        "const uint32_t texels = next_random(&seed);",
        "const uint32_t fractions = next_random(&seed);",
        '"BIL2 random sweep contains 4000 distinct six-byte tuples"',
        '"BIL2 latency remains three clocks"',
        '"BIL2 reaches full three-stage occupancy"',
        '"BIL2 vertical-capture bypass mutant is detected under stalls"',
        '"BIL2 vertical-capture bypass mutant FIRED differences=%d holds=%d\\n"',
    ), "BIL2 random coverage")


def validate_parent_selection() -> None:
    island = ISLAND.read_text(encoding="utf-8")
    stage = STAGE.read_text(encoding="utf-8")
    tile = TILE.read_text(encoding="utf-8")
    bin_pipe = BIN.read_text(encoding="utf-8")
    template = G8A_TEMPLATE.read_text(encoding="utf-8")
    wrapper = G8A_WRAPPER.read_text(encoding="utf-8")

    require_once(island, (
        "parameter bit BILERP_DSP2 = 1'b0",
        "if (BILERP_DSP2) begin : g_dsp2",
        "zhao_texture_bilerp_lane_dsp2 #(.TOKW(TOKW)) u_bilerp",
        "end else begin : g_v2",
        "zhao_texture_bilerp_lane_v2 #(.TOKW(TOKW)) u_bilerp",
    ), "V3 BIL2 selection")
    require_once(stage, (
        "parameter bit BILERP_DSP2 = 1'b0",
        ".BILERP_DSP2(BILERP_DSP2)",
    ), "Packet-C BIL2 propagation")
    require_once(tile, (
        "parameter bit ATTR_DSP3 = 1'b0",
        "parameter bit BILERP_DSP2 = 1'b0",
        "if (ATTR_DSP3) begin : g_dsp3",
        "zhao_raster_attrgrad_dsp3 u_attrgrad",
        "end else begin : g_v2",
        "zhao_raster_attrgrad_v2 u_attrgrad",
        ".BILERP_DSP2(BILERP_DSP2)",
    ), "Packet-D DSP selection")
    require_once(bin_pipe, (
        "parameter bit ATTR_DSP3           = 1'b0",
        "parameter bit BILERP_DSP2         = 1'b0",
        ".ATTR_DSP3(ATTR_DSP3)",
        ".BILERP_DSP2(BILERP_DSP2)",
    ), "geom-bin DSP propagation")
    for text, label in ((template, "G8A template"), (wrapper, "G8A wrapper")):
        require_once(text, (
            "parameter bit ATTR_DSP3 = 1'b0",
            "parameter bit BILERP_DSP2 = 1'b0",
            ".ATTR_DSP3(ATTR_DSP3)",
            ".BILERP_DSP2(BILERP_DSP2)",
        ), label)

    manifest = json.loads(G8A_MANIFEST.read_text(encoding="utf-8"))
    rows = manifest.get("source_closure")
    if not isinstance(rows, list) or len(rows) != 48:
        raise AssertionError("G8A generated manifest is not the exact 48-source profile")
    paths = tuple(row.get("path") for row in rows if isinstance(row, dict))
    expected_candidates = (
        "fpga/rtl/common/zhao_mul27_exact.sv",
        "fpga/rtl/common/zhao_dual18_mul.sv",
        "fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv",
        "fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv",
        "fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv",
    )
    for path in expected_candidates:
        if paths.count(path) != 1:
            raise AssertionError(f"G8A candidate source is not exact: {path}")
    if manifest.get("fit_top_parameters") != {
            "ATTR_DSP3": "1'b1", "BILERP_DSP2": "1'b1"}:
        raise AssertionError("G8A generated manifest lost selected DSP parameters")

    production = PROD_MANIFEST.read_text(encoding="utf-8")
    require_once(production, (
        "    BILERP_DSP2: 1'b0",
        "  - zhao_mul27_exact: not-yet-adopted ",
        "  - zhao_attr_mul72x13_dsp3: not-yet-adopted ",
        "  - zhao_raster_attrgrad_dsp3: not-yet-adopted ",
        "  - zhao_texture_bilerp_lane_dsp2: not-yet-adopted ",
    ), "production candidate disposition")
    require_once(PROD_CHECKER.read_text(encoding="utf-8"), (
        "def apply_parameterized_elaboration(",
        "parameter_overrides=overrides",
        "edges, parameter_elaboration_observations = apply_parameterized_elaboration(",
    ), "parameter-aware production accounting")
    require_once(OWNERSHIP_CHECKER.read_text(encoding="utf-8"), (
        "base_environment=None, parameter_overrides=None,",
        '] + ["-G%s=%s" % item for item in parameter_overrides.items()] + [',
    ), "parameterized elaboration boundary")

    cmake = CMAKE.read_text(encoding="utf-8")
    require_once(cmake, (
        "G8A raster/texture fit closure must contain exactly 49 sources",
        "VERILATOR_ARGS --assert -GATTR_DSP3=1 -GBILERP_DSP2=1",
        "-DQUARTUS_SYNTHESIS=1 -DSYNTHESIS=1 -DZHAO_DUAL18_BEHAVIORAL",
    ), "connected G8A simulation selection")
    require_once(BLOCK_RUNNER.read_text(encoding="utf-8"), (
        "[string[]]$VerilogMacros,",
        "-VerilogMacros entry '$macro' is not canonical NAME or NAME=VALUE.",
        "$qsf += ('set_global_assignment -name VERILOG_MACRO \"' + $macro + '\"')",
        "$row.verilogMacros = @($VerilogMacros)",
    ), "block-fit backend selection")
    require_once(TIMING3_RUNNER.read_text(encoding="utf-8"), (
        "-TopParameters @('ATTR_DSP3=1', 'BILERP_DSP2=1')",
        "-VerilogMacros @('ZHAO_DUAL18_CYCLONEV=1')",
    ), "Timing3 DSP selection")


def validate_map_evidence() -> None:
    report = json.loads(BLOCK_REPORT.read_text(encoding="utf-8"))
    rows = report.get("blocks")
    if not isinstance(rows, list):
        raise AssertionError("block-fit ledger lacks rows")
    if len({row.get("module") for row in rows if isinstance(row, dict)}) != len(rows):
        raise AssertionError("block-fit ledger contains duplicate module rows")

    specs = {
        ATTR_MAP_ROW: {
            "commit": "ce9b2c024f632fa0fd18433e4901aa94b7bcb7a2",
            "dsp": 3,
            "registers": 306,
            "entities": {
                "zhao_attr_mul72x13_dsp3": 1,
                "zhao_mul27_exact": 3,
            },
            "sources": ("zhao_attr_mul72x13_dsp3.sv", "zhao_mul27_exact.sv"),
            "macro": None,
        },
        BIL_MAP_ROW: {
            "commit": "d87001c5be815305574a8e57d8913560482bda9c",
            "dsp": 2,
            "registers": 195,
            "entities": {
                "zhao_texture_bilerp_lane_dsp2": 1,
                "zhao_dual18_mul": 1,
            },
            "sources": ("zhao_dual18_mul.sv", "zhao_texture_bilerp_lane_dsp2.sv"),
            "macro": "ZHAO_DUAL18_CYCLONEV=1",
        },
    }
    sys.path.insert(0, str(REPO / "tools/quartus"))
    import g8a_receipt

    for name, spec in specs.items():
        matches = [row for row in rows
                   if isinstance(row, dict) and row.get("module") == name]
        if len(matches) != 1:
            raise AssertionError(f"MapOnly ledger row is not exact: {name}")
        row = matches[0]
        expected = {
            "status": "map_only",
            "sourceCommit": spec["commit"],
            "treeCleanAtHead": True,
            "rtlCleanAtHead": True,
            "sourcesHashed": 2,
            "dspBlocks": spec["dsp"],
            "registers": spec["registers"],
            "blockMemoryBits": 0,
            "partial": True,
            "partialStage": "analysis_and_synthesis",
        }
        for field, value in expected.items():
            if row.get(field) != value:
                raise AssertionError(
                    f"MapOnly row {name} field {field} differs: {row.get(field)!r}")
        if spec["macro"] is None:
            if row.get("verilogMacros") is not None:
                raise AssertionError("ATTR3 MapOnly row unexpectedly selected a macro")
        elif row.get("verilogMacros") != [spec["macro"]]:
            raise AssertionError("BIL2 MapOnly row lost its vendor backend")

        paths = {suffix: BLOCKPATHS / f"{name}.{suffix}" for suffix in (
            "sources.sha256", "qsf", "qpf", "map.summary", "map.rpt", "map.log",
        )}
        for path in paths.values():
            if not path.is_file() or path.stat().st_size == 0:
                raise AssertionError(f"MapOnly raw artifact is absent/empty: {path}")
        source_lines = paths["sources.sha256"].read_text(
            encoding="utf-8-sig").rstrip("\r\n").splitlines()
        if len(source_lines) != 2 or tuple(
                line.rsplit("  ", 1)[-1] for line in source_lines) != spec["sources"]:
            raise AssertionError(f"MapOnly source manifest differs: {name}")
        qsf = paths["qsf"].read_text(encoding="utf-8-sig")
        if qsf.count(f"set_global_assignment -name TOP_LEVEL_ENTITY {name.split('@')[0]}") != 1:
            raise AssertionError(f"MapOnly top selection differs: {name}")
        vendor_marker = 'set_global_assignment -name VERILOG_MACRO "ZHAO_DUAL18_CYCLONEV=1"'
        if qsf.count(vendor_marker) != (1 if spec["macro"] else 0):
            raise AssertionError(f"MapOnly backend selection differs: {name}")
        map_text = paths["map.rpt"].read_text(encoding="utf-8", errors="replace")
        counts: dict[str, int] = {}
        for entity in g8a_receipt.parse_entity_rows(map_text):
            counts[entity["entity"]] = counts.get(entity["entity"], 0) + 1
        if counts != spec["entities"]:
            raise AssertionError(f"MapOnly entity hierarchy differs: {name}: {counts}")
        log = paths["map.log"].read_text(encoding="utf-8", errors="replace")
        if "Analysis & Synthesis was successful" not in log or "Error (" in log:
            raise AssertionError(f"MapOnly raw log is not a successful run: {name}")


class G8aDspRescueStaticTests(unittest.TestCase):
    def test_manifests_and_lf_rules_are_exact(self) -> None:
        attr_rows = manifest_rows(ATTR_MANIFEST, EXPECTED_ATTR_SOURCES)
        manifest_rows(BIL_MANIFEST, EXPECTED_BIL_SOURCES)
        with tempfile.TemporaryDirectory(prefix="g8a-dsp-manifest-") as temporary:
            damaged = Path(temporary) / "reordered.sources.txt"
            damaged.write_text("\n".join(reversed(attr_rows)) + "\n", encoding="utf-8")
            with self.assertRaises(AssertionError):
                manifest_rows(damaged, EXPECTED_ATTR_SOURCES)
        attrs = ATTRS.read_text(encoding="utf-8").splitlines()
        for row in EXPECTED_LF_ROWS:
            self.assertEqual(attrs.count(row), 1, row)

    def test_candidate_structures_are_pinned(self) -> None:
        validate_attr_structure()
        validate_bil_structure()
        validate_mutant_guards()
        validate_driver_coverage()

    def test_maponly_prerequisites_are_raw_bound(self) -> None:
        validate_map_evidence()

    def test_parent_selection_and_evidence_are_pinned(self) -> None:
        validate_parent_selection()

    def test_cmake_registration_is_exact(self) -> None:
        cmake = CMAKE.read_text(encoding="utf-8")
        validate_cmake(cmake)
        with self.assertRaises(AssertionError):
            validate_cmake(cmake.replace(
                "  g8a_dsp_bil2_selector_collision\n", "", 1))


def collision(profile: str, verilator: Path) -> int:
    top, expected_rows, selectors, expected, extras = COLLISIONS[profile]
    rows = manifest_rows(
        ATTR_MANIFEST if profile.startswith("attr-") else BIL_MANIFEST,
        expected_rows,
    )
    sys.path.insert(0, str(REPO / "tools/rtl"))
    import texture_v3_interface_parser as interface

    with tempfile.TemporaryDirectory(prefix=f"g8a-dsp-{profile}-") as temporary:
        command = [
            str(verilator), "--lint-only", "--Mdir", temporary,
            "--top-module", top,
            *(f"-D{define}" for define in (*selectors, *extras)),
            *(str(REPO / row) for row in rows),
        ]
        try:
            completed = subprocess.run(
                command, cwd=REPO,
                env=interface.verilator_environment(verilator, REPO),
                capture_output=True, text=True, errors="replace", timeout=120,
                check=False,
            )
        except subprocess.TimeoutExpired:
            print(f"FAIL: G8A DSP selector control {profile} timed out", file=sys.stderr)
            return 1
    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0 or expected not in diagnostic:
        print(
            f"FAIL: G8A DSP selector control {profile} rc={completed.returncode}\n"
            f"{diagnostic[-4000:]}", file=sys.stderr,
        )
        return 1
    print(f"G8A_DSP_SELECTOR_COLLISION[{profile}] FIRED")
    return 0


def main() -> int:
    if "--collision" not in sys.argv:
        unittest.main()
        return 0
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--collision", choices=tuple(COLLISIONS), required=True)
    parser.add_argument("--verilator", type=Path, required=True)
    args = parser.parse_args()
    if not args.verilator.is_file():
        parser.error("--verilator must name an existing executable")
    return collision(args.collision, args.verilator.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
