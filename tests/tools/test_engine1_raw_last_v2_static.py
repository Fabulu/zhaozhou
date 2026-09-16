#!/usr/bin/env python3
"""Packet-H ENGINE1 raw-LAST registration and selector controls."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
MANIFEST = REPO / "tests/memory/engine1_raw_last_v2.sources.txt"
RTL = REPO / "fpga/rtl/memory/zhao_engine1_raw_last_v2.sv"
MUTANTS = REPO / "tests/mutants/zhao_engine1_raw_last_v2_mutants.sv"
DRIVER = REPO / "tests/memory/engine1_raw_last_v2_directed.cpp"
CMAKE = REPO / "tests/CMakeLists.txt"
PROD_MANIFEST = REPO / "design/prod_manifest.yml"
ATTRS = REPO / ".gitattributes"
EXPECTED_SOURCES = (
    "tests/mutants/zhao_engine1_raw_last_v2_mutants.sv",
    "fpga/rtl/memory/zhao_engine1_raw_last_v2.sv",
)
EXPECTED_TESTS = (
    "engine1_raw_last_v2_directed",
    "engine1_raw_last_v2_early_last_control",
    "engine1_raw_last_v2_missing_last_control",
    "engine1_raw_last_v2_late_last_control",
    "engine1_raw_last_v2_state_invalid_control",
    "engine1_raw_last_v2_sv_selector_collision",
    "engine1_raw_last_v2_cpp_selector_collision",
    "engine1_raw_last_v2_registration_static",
    "lint_zhao_engine1_raw_last_v2",
)
EXPECTED_LF_ROWS = (
    "fpga/rtl/memory/zhao_engine1_raw_last_v2.sv text eol=lf",
    "tests/memory/engine1_raw_last_v2.sources.txt text eol=lf",
    "tests/memory/engine1_raw_last_v2_directed.cpp text eol=lf",
    "tests/mutants/zhao_engine1_raw_last_v2_mutants.sv text eol=lf",
    "tests/tools/test_engine1_raw_last_v2_static.py text eol=lf",
)


def manifest_rows(text: str) -> tuple[str, ...]:
    rows = tuple(text.splitlines())
    if rows != EXPECTED_SOURCES or len(rows) != len(set(rows)):
        raise AssertionError("raw-LAST source manifest is not exact and ordered")
    for row in rows:
        if (not row or "\\" in row or row.startswith("/") or
                re.match(r"^[A-Za-z]:", row) or ".." in Path(row).parts or
                not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.(?:sv)", row)):
            raise AssertionError(f"invalid raw-LAST source path: {row!r}")
    return rows


def require_markers(text: str, markers: tuple[str, ...], label: str) -> None:
    for marker in markers:
        if text.count(marker) != 1:
            raise AssertionError(f"{label} marker is not exact: {marker}")


def validate_rtl(text: str) -> None:
    require_markers(text, (
        "module zhao_engine1_raw_last_v2",
        "input  logic       guard_accept_i",
        "input  logic [6:0] len_bytes_i",
        "input  logic       verdict_ok_i",
        "input  logic       verdict_denied_i",
        "input  logic       raw16_valid_i",
        "input  logic [15:0] raw16_data_i",
        "input  logic [7:0] controller_retire_halfwords_i",
        "input  logic        framed_raw_ready_i",
        "output logic        framed_raw_valid_o",
        "output logic [15:0] framed_raw_data_o",
        "output logic        framed_raw_last_o",
        "logic [5:0]  controller_retired_halfwords_q;",
        "logic        final_candidate_q;",
        "logic        final_held_q;",
        "assign accepted_length_legal_c = (len_bytes_i == 7'd16)",
        "|| (len_bytes_i == 7'd32)",
        "|| (len_bytes_i == 7'd64);",
        "assign retired_plus_one_c = {1'b0, retired_halfwords_q} + 7'd1;",
        "assign controller_retired_next_c =",
        "assign exact_raw_terminal_c = raw16_valid_i",
        "assign controller_terminal_c = controller_retire_c",
        "assign controller_raw_count_mismatch_c = stream_active_c",
        "assign terminal_raw_mismatch_c = stream_active_c && controller_terminal_c",
        "else if ((state_q == S_ARMED) && final_held_q) begin",
        "assign raw_last_o        = framed_raw_last_o;",
        "assign framed_backpressure_c = stream_active_c && framed_raw_valid_o",
        "assign published_last_mismatch_c = framed_raw_valid_o",
        "assign checked_state_c = `ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_q);",
        "assign protocol_event_c = state_invalid_c",
        "|| controller_retire_wrong_state_c",
        "|| controller_raw_count_mismatch_c",
        "|| terminal_raw_mismatch_c",
        "|| framed_backpressure_c",
        "|| published_last_mismatch_c",
        "|| raw_wrong_state_c;",
        "if (controller_terminal_c) begin",
        "final_held_q <= 1'b1;",
        "if (framed_raw_ready_i) begin",
        "`undef ZHAO_ENGINE1_RAW_LAST_V2_TERMINAL_EXPR",
        "`undef ZHAO_ENGINE1_RAW_LAST_V2_LAST_EXPR",
        "`undef ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR",
    ), "raw-LAST RTL")
    if "assign controller_retire_halfwords_i" in text:
        raise AssertionError("raw-LAST independent controller retirement became derived")
    if re.search(r"assign\s+raw_last_o\s*=.*raw16_valid_i", text):
        raise AssertionError("raw-LAST legacy alias regressed to the raw input cycle")
    if "OVERRUN" in text or "raw_overrun" in text:
        raise AssertionError("raw-LAST RTL retained the redundant overrun detector")


def validate_mutants(text: str) -> None:
    require_markers(text, (
        "ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_EARLY_LAST",
        "ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_MISSING_LAST",
        "ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_LATE_LAST",
        "ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_STATE_INVALID",
        "((next_count) == ((expected_count) - 6'd1))",
        "((next_count) == ((expected_count) + 6'd1))",
        "`define ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_value) 2'b11",
    ), "raw-LAST mutant")
    if text.count("`define ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION") != 4:
        raise AssertionError("raw-LAST mutant selector collision fan-in differs")


def validate_driver(text: str) -> None:
    require_markers(text, (
        "ENGINE1_RAW_LAST_V2_DRIVER_SELECTOR_COLLISION",
        "check_healthy_length(16, 8);",
        "check_healthy_length(32, 16);",
        "check_healthy_length(64, 32);",
        "test_split_controller_credits();",
        "test_final_candidate_may_lead_retirement();",
        "test_real_missing_final_raw_pulse();",
        "test_short_and_mismatched_retirement();",
        "test_healthy_terminal_clears_legality();",
        "split 3+5 controller credits validate one eight-halfword request",
        "later physical terminal retirement releases the captured candidate",
        "physical terminal retirement with a missing final raw pulse faults",
        "terminal retirement with a short raw count faults",
        "terminal raw beat with a mismatched controller count faults",
        "nonterminal downstream backpressure enters fail-stop",
        "FINAL_HELD remains stable while ready is low",
        "denial emits no LAST and retires zero raw halfwords",
        "overlapping accepted request enters fail-stop",
        "verdict without pending request enters fail-stop",
        "simultaneous OK and denial enters fail-stop",
        "raw return before approval enters fail-stop",
        "raw return after terminal enters fail-stop",
        "STATE_INVALID mutant fires the reset-lifetime fail-stop detector",
        "engine1_raw_last_v2: inverse EARLY_LAST control",
        "engine1_raw_last_v2: inverse MISSING_LAST control",
        "engine1_raw_last_v2: inverse LATE_LAST control",
        "engine1_raw_last_v2: inverse STATE_INVALID control",
    ), "raw-LAST driver")


def validate_cmake(text: str) -> None:
    require_markers(text, (
        "Packet H: independent ENGINE1 raw16 LAST",
        "memory/engine1_raw_last_v2.sources.txt",
        "zhao_packet_h_raw_last_test(ph_raw directed",
        "zhao_packet_h_raw_last_test(ph_raw_e early_last_control",
        "zhao_packet_h_raw_last_test(ph_raw_m missing_last_control",
        "zhao_packet_h_raw_last_test(ph_raw_l late_last_control",
        "zhao_packet_h_raw_last_test(ph_raw_s state_invalid_control",
        "foreach(raw_last_collision IN ITEMS sv cpp)",
        "--collision ${raw_last_collision}",
        "test_engine1_raw_last_v2_static.py -q",
        "NAME lint_zhao_engine1_raw_last_v2",
        "Packet-H raw-LAST required CTest inventory must contain exactly 9 names",
    ), "raw-LAST CMake")
    match = re.search(r"set\(ZHAO_PACKET_H_RAW_LAST_REQUIRED_TESTS\n(.*?)\)", text, re.DOTALL)
    if match is None:
        raise AssertionError("raw-LAST required CTest inventory is absent")
    rows = tuple(line.strip() for line in match.group(1).splitlines())
    if rows != EXPECTED_TESTS or len(rows) != len(set(rows)):
        raise AssertionError("raw-LAST required CTest inventory differs")


class RawLastStaticTests(unittest.TestCase):
    def test_manifest_and_files_are_exact(self) -> None:
        rows = manifest_rows(MANIFEST.read_text(encoding="utf-8"))
        for row in rows:
            self.assertTrue((REPO / row).is_file(), row)
        sys.path.insert(0, str(REPO / "tools/quartus"))
        import check_prod_manifest
        tops, excluded = check_prod_manifest.read_manifest(PROD_MANIFEST)
        self.assertNotIn("zhao_engine1_raw_last_v2", tops)
        self.assertEqual(excluded["zhao_engine1_raw_last_v2"][0], "not-yet-adopted")
        attrs = ATTRS.read_text(encoding="utf-8").splitlines()
        for row in EXPECTED_LF_ROWS:
            self.assertEqual(attrs.count(row), 1, row)
        self.assertEqual(attrs.count("design/contracts/*.md text eol=lf"), 1)
        with self.assertRaises(AssertionError):
            manifest_rows("\n".join(reversed(rows)) + "\n")

    def test_rtl_mutant_and_driver_detectors_fire(self) -> None:
        rtl = RTL.read_text(encoding="utf-8")
        mutants = MUTANTS.read_text(encoding="utf-8")
        driver = DRIVER.read_text(encoding="utf-8")
        validate_rtl(rtl)
        validate_mutants(mutants)
        validate_driver(driver)
        with self.assertRaises(AssertionError):
            validate_rtl(rtl.replace(
                "input  logic [7:0] controller_retire_halfwords_i",
                "input  logic [7:0] controller_retire_halfwords_broken_i",
                1,
            ))
        with self.assertRaises(AssertionError):
            validate_rtl(rtl.replace(
                "assign raw_last_o        = framed_raw_last_o;",
                "assign raw_last_o = raw16_valid_i;",
                1,
            ))
        with self.assertRaises(AssertionError):
            validate_mutants(mutants.replace(
                "`define ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_value) 2'b11",
                "`define ZHAO_ENGINE1_RAW_LAST_V2_STATE_EXPR(state_value) state_value",
                1,
            ))
        with self.assertRaises(AssertionError):
            validate_driver(driver.replace("raw return after terminal enters fail-stop", "", 1))

    def test_cmake_registration_is_exact(self) -> None:
        cmake = CMAKE.read_text(encoding="utf-8")
        validate_cmake(cmake)
        with self.assertRaises(AssertionError):
            validate_cmake(cmake.replace("  lint_zhao_engine1_raw_last_v2)", ")", 1))


def collision(profile: str, verilator: Path, cxx: Path) -> int:
    rows = manifest_rows(MANIFEST.read_text(encoding="utf-8"))
    if profile == "sv":
        sys.path.insert(0, str(REPO / "tools/rtl"))
        import texture_v3_interface_parser as interface
        with tempfile.TemporaryDirectory(prefix="packet-h-raw-last-") as temporary:
            command = [
                str(verilator), "--lint-only", "--Mdir", temporary,
                "--top-module", "zhao_engine1_raw_last_v2",
                "-DZHAO_ENGINE1_RAW_LAST_V2_MUTANT_EARLY_LAST",
                "-DZHAO_ENGINE1_RAW_LAST_V2_MUTANT_MISSING_LAST",
                *(str(REPO / row) for row in rows),
            ]
            completed = subprocess.run(
                command, cwd=REPO,
                env=interface.verilator_environment(verilator, REPO),
                capture_output=True, text=True, errors="replace", timeout=120,
                check=False,
            )
        expected = "ZHAO_ENGINE1_RAW_LAST_V2_MUTANT_SELECTOR_COLLISION"
    else:
        command = [
            str(cxx), "-E", "-x", "c++",
            "-DEXPECT_ENGINE1_RAW_LAST_V2_EARLY_LAST_MUTANT",
            "-DEXPECT_ENGINE1_RAW_LAST_V2_MISSING_LAST_MUTANT",
            str(DRIVER),
        ]
        completed = subprocess.run(
            command, cwd=REPO, capture_output=True, text=True,
            errors="replace", timeout=30, check=False,
        )
        expected = "ENGINE1_RAW_LAST_V2_DRIVER_SELECTOR_COLLISION"
    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0 or expected not in diagnostic:
        print(
            f"FAIL: raw-LAST {profile} collision rc={completed.returncode}\n"
            f"{diagnostic[-4000:]}", file=sys.stderr,
        )
        return 1
    print(f"ENGINE1_RAW_LAST_V2_SELECTOR_COLLISION[{profile}] FIRED")
    return 0


def main() -> int:
    if "--collision" not in sys.argv:
        unittest.main()
        return 0
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--collision", choices=("sv", "cpp"), required=True)
    parser.add_argument("--verilator", type=Path, required=True)
    parser.add_argument("--cxx", type=Path, required=True)
    args = parser.parse_args()
    if not args.verilator.is_file() or not args.cxx.is_file():
        parser.error("--verilator and --cxx must name existing files")
    return collision(args.collision, args.verilator.resolve(), args.cxx.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
