#!/usr/bin/env python3
"""Packet-F G8A generated-wrapper, fit registration, and runner controls."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import unittest


REPO = Path(__file__).resolve().parents[2]
GENERATOR_PATH = REPO / "tools/quartus/gen_raster_texture_v3_fit_top.py"
WRAPPER = REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv"
MANIFEST = REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.manifest.json"

spec = importlib.util.spec_from_file_location("g8a_generator", GENERATOR_PATH)
if spec is None or spec.loader is None:
    raise RuntimeError("could not load G8A generator")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
EXPECTED_SOURCES = tuple(generator.SOURCE_CLOSURE)

receipt_spec = importlib.util.spec_from_file_location(
    "g8a_receipt", REPO / "tools/quartus/g8a_receipt.py"
)
if receipt_spec is None or receipt_spec.loader is None:
    raise RuntimeError("could not load G8A receipt tool")
receipt = importlib.util.module_from_spec(receipt_spec)
receipt_spec.loader.exec_module(receipt)

PROTECTED = {
    "fpga/rtl/common/zhao_shell_top.sv":
        "00fdd2387ffea985bb6d3d0e2a9b21bde2913478d33333d30d11b64ae5450783",
    "fpga/rtl/generated/zhao_texture_island_v3_top.interface.json":
        "e77e43a1f6e2baf9b7ee4bbc78093c28b6ad1be683d10babbde698f988ada5b5",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv":
        "4ba2cba9df8c6e6baaf1c68a236b91612fc6b3fff69be76ab3098b335bc50348",
}
EXPECTED_TESTS = (
    "raster_texture_v3_fit_top_directed",
    "lint_raster_texture_v3_fit_top",
    "raster_texture_v3_fit_top_generated_freshness",
    "packet_f_g8a_registration_static",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require_once(text: str, markers: tuple[str, ...], label: str) -> None:
    for marker in markers:
        if text.count(marker) != 1:
            raise AssertionError(f"{label} marker is not exact/unique: {marker}")


def fit_target_sources(text: str) -> tuple[tuple[str, ...], dict[str, int]]:
    lines = text.splitlines()
    starts = [index for index, line in enumerate(lines)
              if line.strip() == "- top: zhao_raster_texture_v3_fit_top"]
    if len(starts) != 1:
        raise AssertionError("G8A fit target is not exact/unique")
    sources: list[str] = []
    rules: dict[str, int] = {}
    in_sources = False
    in_rules = False
    for line in lines[starts[0] + 1:]:
        if re.match(r"^\s{2}- top:", line):
            break
        stripped = line.strip()
        if stripped == "sources:":
            in_sources = True
            in_rules = False
        elif stripped == "rules:":
            in_sources = False
            in_rules = True
        elif in_sources and re.fullmatch(r"- (?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", stripped):
            sources.append(stripped[2:])
        elif in_rules:
            match = re.fullmatch(r"(max_alms|max_dsp|min_fmax_mhz): (\d+)", stripped)
            if match:
                rules[match.group(1)] = int(match.group(2))
    return tuple(sources), rules


def cmake_sources(text: str) -> tuple[str, ...]:
    match = re.search(
        r"set\(ZHAO_G8A_RASTER_TEXTURE_SOURCES\n(.*?)\)", text, re.DOTALL
    )
    if match is None:
        raise AssertionError("G8A CMake source list is missing")
    rows = []
    for line in match.group(1).splitlines():
        line = line.strip()
        prefix = "${CMAKE_SOURCE_DIR}/"
        if not line.startswith(prefix):
            raise AssertionError("G8A CMake source is not repo-rooted: " + line)
        rows.append(line[len(prefix):])
    return tuple(rows)


def validate_runner(text: str) -> None:
    require_once(text, (
        "[switch]$PhysicalPins",
        "$dirtyTree = (& git -C $RepoRoot -c core.autocrlf=true status --porcelain) -join ''",
        "$treeClean = [string]::IsNullOrWhiteSpace($dirtyTree)",
        "if ($PhysicalPins -and $Module.Count -ne 1)",
        "$_ -notmatch '^set_global_assignment -name (?:SYSTEMVERILOG|VERILOG|VHDL)_FILE'",
        "if (-not $PhysicalPins) {",
        "$qsf += 'set_instance_assignment -name VIRTUAL_PIN ON -to *'",
        "$qsf += '# Physical top ports retained by run_block_fit.ps1 -PhysicalPins.'",
        "($rowModule + '.qsf')",
        "($rowModule + '.sdc')",
        "treeCleanAtHead = $treeClean; rtlCleanAtHead = $rtlClean;",
        "'physical-top-ports'",
        "'virtual-top-ports'",
        "I/O mode is recorded per row.",
    ), "G8A block-fit runner")


def validate_cmake(text: str) -> None:
    sources = cmake_sources(text)
    if sources != EXPECTED_SOURCES or len(sources) != len(set(sources)):
        raise AssertionError("G8A CMake source closure is not exact")
    require_once(text, (
        "G8A raster/texture fit closure must contain exactly 43 sources",
        "add_executable(pf_g8a raster/raster_texture_v3_fit_top_directed.cpp)",
        "TOP_MODULE zhao_raster_texture_v3_fit_top",
        "SOURCES ${ZHAO_G8A_RASTER_TEXTURE_SOURCES}",
        "-DQUARTUS_SYNTHESIS=1",
        "gen_raster_texture_v3_fit_top.py --check",
        "tools/test_raster_texture_v3_fit_top.py -q",
        'LABELS "fast;nightly;packet-f;g8a"',
        "Packet-F required CTest inventory must contain exactly 4 names",
        'if(NOT TEST "${required_packet_f_test}")',
    ), "G8A CMake")
    match = re.search(r"set\(ZHAO_PACKET_F_REQUIRED_TESTS\n(.*?)\)", text, re.DOTALL)
    if match is None:
        raise AssertionError("Packet-F CTest inventory missing")
    inventory = tuple(line.strip() for line in match.group(1).splitlines())
    if inventory != EXPECTED_TESTS or len(inventory) != len(set(inventory)):
        raise AssertionError("Packet-F CTest inventory is not exact")


def synthetic_map_report(*, shadows: bool = False, texjoin: bool = False,
                         parameter: str = "0") -> str:
    entities = [
        ("zhao_raster_texture_v3_fit_top", "|zhao_raster_texture_v3_fit_top"),
        ("zhao_raster_tile_pipe_v2", "|top|zhao_raster_tile_pipe_v2:u_tile"),
        ("zhao_raster_texture_stage_v3", "|top|tile|zhao_raster_texture_stage_v3:u_texture_stage"),
        ("zhao_texture_island_v3_top", "|top|tile|stage|zhao_texture_island_v3_top:u_texture_v3"),
        ("zhao_texture_v3own", "|top|tile|stage|v3|zhao_texture_v3own:u_own"),
    ]
    if texjoin:
        entities.append(("zhao_raster_texjoin_v2", "|top|zhao_raster_texjoin_v2:u_bad"))
    rows = ["Analysis & Synthesis Resource Utilization by Entity"]
    for entity, hierarchy in entities:
        rows.append(
            f"; |{entity} ; 100 (10) ; 200 (20) ; 32768 ; 0 ; 18 ; 0 ; "
            f"{hierarchy} ; {entity} ; work ;"
        )
    rows.extend([
        "Analysis & Synthesis RAM Summary",
        "zhao_raster_tilestore:u_tilestore ram0 ram1",
        "Parameter Settings for User Entity Instance: "
        "zhao_raster_tile_pipe_v2:u_tile|zhao_raster_texture_stage_v3:u_texture_stage|"
        "zhao_texture_island_v3_top:u_texture_v3",
        "; Parameter Name ; Value ; Type ;",
        f"; MIGRATION_SHADOWS ; {parameter} ; Unsigned Binary ;",
    ])
    if shadows:
        rows.append("g_migration_shadows|shadow_metadata_m[0]~reg")
    return "\n".join(rows)


class G8AFitTopTests(unittest.TestCase):
    def test_generated_manifest_and_all_source_hashes_are_exact(self) -> None:
        payload = json.loads(MANIFEST.read_text(encoding="utf-8"))
        self.assertEqual(
            set(payload),
            {"external_ports", "hashes", "hierarchy", "limitations", "module",
             "product_profile", "schema_id", "schema_version", "source_closure",
             "traffic_profile"},
        )
        self.assertEqual(payload["schema_id"], "zhao.g8a.fit_top")
        self.assertEqual(payload["schema_version"], 1)
        self.assertEqual(payload["module"], "zhao_raster_texture_v3_fit_top")
        rows = payload["source_closure"]
        self.assertEqual(tuple(row["path"] for row in rows), EXPECTED_SOURCES)
        self.assertEqual([row["ordinal"] for row in rows], list(range(43)))
        self.assertEqual(len({row["path"] for row in rows}), 43)
        for row in rows:
            self.assertEqual(row["sha256"], sha256(REPO / row["path"]))
        self.assertEqual(payload["hashes"]["generated_rtl_sha256"], sha256(WRAPPER))
        self.assertEqual(payload["hashes"]["generator_sha256"], sha256(GENERATOR_PATH))
        self.assertEqual(
            payload["hashes"]["template_sha256"],
            sha256(REPO / "tools/quartus/templates/zhao_raster_texture_v3_fit_top.sv.in"),
        )
        self.assertEqual(payload["product_profile"]["value"], "1'b0")
        self.assertEqual(payload["hierarchy"]["owner"]["required_count"], 1)
        self.assertEqual(payload["hierarchy"]["texjoin_required_count"], 0)

    def test_wrapper_has_small_registered_boundary_and_legal_activity(self) -> None:
        text = WRAPPER.read_text(encoding="utf-8")
        require_once(text, (
            "module zhao_raster_texture_v3_fit_top (",
            "input  logic       clk",
            "input  logic       rst_n",
            "output logic [7:0] fit_signature_o",
            "output logic [7:0] fit_epoch_o",
            "(* preserve_hierarchy *) zhao_raster_tile_pipe_v2 u_tile (",
            ".job_bx_i(21'sd4096)",
            "job_meta_w[297:296] = 2'd1;",
            "job_meta_w[676:581] = 96'h000000000000400000000000;",
            "cfg_crc_w = 32'hc60b_5076;",
            ".fill_refused_i(1'b0)",
            "assign fill_data_w = (fill_beat_q == 3'd0) ? 16'h0005 : 16'h0000;",
            ".pal_load_idx_i(palette_index_q)",
            ".pal_load_rgb565_i((palette_index_q == 8'd5) ? 16'h07e0 : 16'h0000)",
            "if (palette_index_q == 8'hff)",
            "assign fb_ready_w = stimulus_lfsr_q[0] || stimulus_lfsr_q[3];",
            "task zhao_g8a_get_activity(",
            "`ifndef QUARTUS_SYNTHESIS",
        ), "G8A wrapper")
        self.assertNotIn("zhao_raster_texjoin", text)
        self.assertNotIn("zhao_texture_island_v3_top", text)

    def test_product_v3_parameter_is_explicit_in_selected_tile_hierarchy(self) -> None:
        tile = (REPO / "fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv").read_text(
            encoding="utf-8"
        )
        stage = (REPO / "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv").read_text(
            encoding="utf-8"
        )
        self.assertEqual(
            tile.count("zhao_raster_texture_stage_v3 #(.MIGRATION_SHADOWS(1'b0))"), 1
        )
        self.assertEqual(stage.count(".MIGRATION_SHADOWS(MIGRATION_SHADOWS)"), 1)
        self.assertEqual(stage.count("zhao_texture_island_v3_top #("), 1)
        self.assertNotIn("zhao_raster_texjoin", "\n".join(EXPECTED_SOURCES))

    def test_fit_target_and_cmake_match_generated_authority(self) -> None:
        fit_sources, rules = fit_target_sources(
            (REPO / "design/fit_targets.yml").read_text(encoding="utf-8")
        )
        self.assertEqual(fit_sources, EXPECTED_SOURCES)
        self.assertEqual(rules, {"max_alms": 29999, "max_dsp": 84,
                                 "min_fmax_mhz": 100})
        validate_cmake((REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8"))

    def test_fit_target_and_cmake_detectors_fire(self) -> None:
        fit = (REPO / "design/fit_targets.yml").read_text(encoding="utf-8")
        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        for mutation in (
            fit.replace(
                "      max_alms: 29999\n      max_dsp: 84\n      min_fmax_mhz: 100",
                "      max_alms: 29999\n      max_dsp: 85\n      min_fmax_mhz: 100",
                1,
            ),
            fit.replace(
                "      max_alms: 29999\n      max_dsp: 84\n      min_fmax_mhz: 100",
                "      max_alms: 29999\n      max_dsp: 84",
                1,
            ),
            fit.replace(
                "      - fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv\n"
                "      - fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv",
                "      - fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv\n"
                "      - fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv",
                1,
            ),
        ):
            sources, rules = fit_target_sources(mutation)
            self.assertTrue(sources != EXPECTED_SOURCES or rules != {
                "max_alms": 29999, "max_dsp": 84, "min_fmax_mhz": 100})
        for mutation in (
            cmake.replace("SOURCES ${ZHAO_G8A_RASTER_TEXTURE_SOURCES}", "SOURCES", 1),
            cmake.replace("  packet_f_g8a_registration_static)", ")", 1),
        ):
            with self.assertRaises(AssertionError):
                validate_cmake(mutation)

    def test_physical_pin_runner_path_is_explicit_and_detectable(self) -> None:
        runner = (REPO / "tools/quartus/run_block_fit.ps1").read_text(encoding="utf-8")
        validate_runner(runner)
        with self.assertRaises(AssertionError):
            validate_runner(runner.replace("if (-not $PhysicalPins) {", "if ($true) {", 1))
        with self.assertRaises(AssertionError):
            validate_runner(runner.replace("treeCleanAtHead = $treeClean;", "", 1))

    def test_receipt_parsers_fire_on_hierarchy_parameter_and_shadow_faults(self) -> None:
        clean = synthetic_map_report()
        entities = receipt.parse_entity_rows(clean)
        hierarchy = receipt.validate_hierarchy(entities, clean)
        self.assertEqual(hierarchy["texjoin_count"], 0)
        self.assertEqual(receipt.parse_v3_parameter(clean)["value"], "0")
        self.assertTrue(receipt.validate_ram(clean)["pass"])
        with self.assertRaises(receipt.ReceiptError):
            bad = synthetic_map_report(texjoin=True)
            receipt.validate_hierarchy(receipt.parse_entity_rows(bad), bad)
        with self.assertRaises(receipt.ReceiptError):
            bad = synthetic_map_report(shadows=True)
            receipt.validate_hierarchy(receipt.parse_entity_rows(bad), bad)
        with self.assertRaises(receipt.ReceiptError):
            receipt.parse_v3_parameter(synthetic_map_report(parameter="1"))

    def test_receipt_source_digest_matches_block_runner_algorithm(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        digest, text, count = receipt.manifest_source_digest(manifest)
        self.assertEqual(count, 43)
        self.assertEqual(len(text.splitlines()), 43)
        self.assertRegex(digest, r"^[0-9a-f]{64}$")
        self.assertIn("ZHAO_RASTER_TEXTURE_V3_FIT_TOP.SV", text.upper())
        broken = json.loads(json.dumps(manifest))
        broken["source_closure"][1]["ordinal"] = 9
        with self.assertRaises(receipt.ReceiptError):
            receipt.manifest_source_digest(broken)

    def test_retained_qsf_sdc_parser_rejects_extra_or_virtual_sources(self) -> None:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        qsf_lines = [
            "set_global_assignment -name DEVICE 5CSEBA6U23I7",
            "set_global_assignment -name TOP_LEVEL_ENTITY zhao_raster_texture_v3_fit_top",
            "set_global_assignment -name SDC_FILE blockfit.sdc",
            "set_global_assignment -name SEED 1",
            "# Physical top ports retained by run_block_fit.ps1 -PhysicalPins.",
        ]
        qsf_lines.extend(
            f'set_global_assignment -name SYSTEMVERILOG_FILE "C:/snap/{Path(path).name}"'
            for path in EXPECTED_SOURCES
        )
        qsf = "\n".join(qsf_lines)
        sdc = "create_clock -name clk        -period 10.000 [get_ports {clk}]\n"
        parsed = receipt.validate_fit_configuration(qsf, sdc, manifest)
        self.assertEqual(parsed["source_count"], 43)
        for mutation in (
            qsf + '\nset_global_assignment -name SYSTEMVERILOG_FILE "C:/snap/extra.sv"',
            qsf + "\nset_instance_assignment -name VIRTUAL_PIN ON -to *",
        ):
            with self.assertRaises(receipt.ReceiptError):
                receipt.validate_fit_configuration(mutation, sdc, manifest)
        with self.assertRaises(receipt.ReceiptError):
            receipt.validate_fit_configuration(qsf, sdc.replace("10.000", "12.000"), manifest)

    def test_receipt_closed_schema_rejects_false_positive_fields(self) -> None:
        payload = {
            "device": "5CSEBA6U23I7",
            "entity_attribution": {},
            "fit_configuration": {
                "physical_pin_comment": True,
                "source_count": 43,
                "source_order": [Path(path).name for path in EXPECTED_SOURCES],
                "top": receipt.MODULE,
                "virtual_pin_assignments": 0,
            },
            "gate": {"pass": False},
            "hierarchy": {
                "entity_counts": dict(receipt.REQUIRED_ENTITY_COUNTS),
                "shadow_state_matches": [],
                "texjoin_count": 0,
            },
            "io_mode": "physical-top-ports",
            "limitations": [],
            "module": receipt.MODULE,
            "parameters": {"MIGRATION_SHADOWS": {"instance": "u", "value": "0"}},
            "ram_witness": {},
            "raw_artifacts": {},
            "resources": {"virtualPins": 0},
            "row_status": "failed:structure",
            "rule_violations": ["Fmax below 100"],
            "schema_id": "zhao.g8a.fit_receipt",
            "schema_version": 1,
            "source": {
                "commit": "f" * 40,
                "digest": "e" * 64,
                "fit_manifest_path": "x",
                "fit_manifest_sha256": "d" * 64,
                "rtl_clean_at_head": True,
                "sources_hashed": 43,
                "tree_clean_at_head": True,
            },
            "tool": {"name": "Quartus", "version": "17.0.2"},
        }
        receipt.validate_receipt(payload)
        for mutation in (
            {**payload, "io_mode": "virtual-top-ports"},
            {**payload, "resources": {"virtualPins": 1}},
            {**payload, "hierarchy": {**payload["hierarchy"], "texjoin_count": 1}},
            {**payload, "extra": 1},
        ):
            with self.assertRaises(receipt.ReceiptError):
                receipt.validate_receipt(mutation)

    def test_one_fit_runner_and_receipt_tool_are_fail_closed(self) -> None:
        fit_runner = (REPO / "tools/quartus/run_g8a_fit.ps1").read_text(encoding="utf-8")
        receipt_tool = (REPO / "tools/quartus/g8a_receipt.py").read_text(encoding="utf-8")
        require_once(fit_runner, (
            "G8A requires a completely clean committed tree",
            "Another Quartus process is already running",
            "A G8A receipt already exists",
            "A $RowName row already exists",
            "gen_raster_texture_v3_fit_top.py') --check",
            "test_raster_texture_v3_fit_top.py') -q",
            "-RowLabel '@g8a'",
            "-Seed 1",
            "-PhysicalPins",
            "g8a_receipt.py') --write",
        ), "G8A one-fit runner")
        require_once(receipt_tool, (
            'ROW_NAME = MODULE + "@g8a"',
            'row.get("treeCleanAtHead") is not True',
            'if row["ioMode"] != "physical-top-ports":',
            'numeric["virtualPins"] != 0',
            'numeric["fmaxMhz"] >= 100',
            'numeric["alms"] <= 29999',
            'numeric["dspBlocks"] <= 84',
            'top_entity["pins"] != 18',
            'parse_entity_rows(map_text)',
            'parse_v3_parameter(map_text)',
            'validate_ram(map_text)',
            'validate_fit_configuration(qsf_text, sdc_text, manifest)',
            'row["sourceDigest"] != expected_digest',
        ), "G8A receipt tool")
        with self.assertRaises(AssertionError):
            require_once(fit_runner.replace("-PhysicalPins", "", 1),
                         ("-PhysicalPins",), "mutated runner")

    def test_every_hashed_text_input_has_checkout_stable_lf(self) -> None:
        attrs = (REPO / ".gitattributes").read_text(encoding="utf-8").splitlines()
        rows = set(line for line in attrs if line and not line.startswith("#"))
        self.assertIn("fpga/rtl/generated/** text eol=lf", rows)
        for relative in EXPECTED_SOURCES[:-1]:
            if relative.startswith("fpga/rtl/generated/"):
                continue
            self.assertIn(f"{relative} text eol=lf", rows, relative)
        self.assertIn(
            "tools/quartus/gen_raster_texture_v3_fit_top.py text eol=lf", rows
        )
        self.assertIn(
            "tools/quartus/templates/zhao_raster_texture_v3_fit_top.sv.in text eol=lf",
            rows,
        )

    def test_wrapper_is_probe_only_and_protected_bytes_hold(self) -> None:
        tools = REPO / "tools/quartus"
        import sys
        if str(tools) not in sys.path:
            sys.path.insert(0, str(tools))
        import check_prod_manifest

        tops, excluded = check_prod_manifest.read_manifest(REPO / "design/prod_manifest.yml")
        self.assertNotIn("zhao_raster_texture_v3_fit_top", tops)
        self.assertEqual(excluded["zhao_raster_texture_v3_fit_top"][0], "probe")
        self.assertNotIn(
            "zhao_raster_texture_v3_fit_top",
            (REPO / "fpga/rtl/prod/zhao_prod_top.sv").read_text(encoding="utf-8"),
        )
        for relative, expected in PROTECTED.items():
            self.assertEqual(sha256(REPO / relative), expected)


if __name__ == "__main__":
    unittest.main()
