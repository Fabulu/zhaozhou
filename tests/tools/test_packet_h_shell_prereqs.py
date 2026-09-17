#!/usr/bin/env python3
"""Packet-H renderer/video bridge registration and selector controls."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
CMAKE = REPO / "tests/CMakeLists.txt"
PROD_MANIFEST = REPO / "design/prod_manifest.yml"
ATTRS = REPO / ".gitattributes"
CONTRACT = REPO / "design/contracts/VIDEO.SLOTMGR.md"

RENDERER_MANIFEST = REPO / "tests/video/renderer_lease_v2.sources.txt"
RENDERER_RTL = REPO / "fpga/rtl/video/zhao_renderer_lease_v2.sv"
RENDERER_MUTANTS = REPO / "tests/mutants/zhao_renderer_lease_v2_mutants.sv"
RENDERER_DRIVER = REPO / "tests/video/renderer_lease_v2_directed.cpp"
RENDERER_SOURCES = (
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_pkg.sv",
    "tests/mutants/zhao_renderer_lease_v2_mutants.sv",
    "fpga/rtl/video/zhao_renderer_lease_v2.sv",
    "tests/video/tb_renderer_lease_v2.sv",
)

BRIDGE_MANIFEST = REPO / "tests/video/video_ready_bridge_v2.sources.txt"
BRIDGE_RTL = REPO / "fpga/rtl/video/zhao_video_ready_bridge_v2.sv"
BRIDGE_MUTANTS = REPO / "tests/mutants/zhao_video_ready_bridge_v2_mutants.sv"
BRIDGE_DRIVER = REPO / "tests/video/video_ready_bridge_v2_directed.cpp"
BRIDGE_SOURCES = (
    "tests/mutants/zhao_video_ready_bridge_v2_mutants.sv",
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_pkg.sv",
    # The bridge imports zhao_fb_tuple_pkg so that the ONE place it looks inside
    # the 84-bit tuple -- the slot, which used to be a bare `[82]` -- names the
    # layout instead of restating it. See zhao_fb_tuple_pkg.sv's header for what
    # that bare literal cost.
    "fpga/rtl/video/zhao_fb_tuple_pkg.sv",
    "fpga/rtl/video/zhao_video_ready_bridge_v2.sv",
    "tests/video/tb_video_ready_bridge_v2.sv",
)

TERMINAL_MANIFEST = REPO / "tests/video/video_terminal_adapter_v2.sources.txt"
TERMINAL_RTL = REPO / "fpga/rtl/video/zhao_video_terminal_adapter_v2.sv"
TERMINAL_MUTANTS = REPO / "tests/mutants/zhao_video_terminal_adapter_v2_mutants.sv"
TERMINAL_DRIVER = REPO / "tests/video/video_terminal_adapter_v2_directed.cpp"
TERMINAL_SOURCES = (
    "tests/mutants/zhao_video_terminal_adapter_v2_mutants.sv",
    "fpga/rtl/video/zhao_video_terminal_adapter_v2.sv",
    "tests/video/tb_video_terminal_adapter_v2.sv",
)

EXPECTED_TESTS = (
    "renderer_lease_v2_directed",
    "renderer_lease_v2_lease_open_control",
    "renderer_lease_v2_slot_choice_control",
    "renderer_lease_v2_request_hold_control",
    "renderer_lease_v2_writer_response_control",
    "renderer_lease_v2_skip_clear_control",
    "renderer_lease_v2_bad_stride_control",
    "renderer_lease_v2_duo_boundary_control",
    "renderer_lease_v2_stale_identity_control",
    "video_ready_bridge_v2_directed",
    "video_ready_bridge_v2_drop_held_tuple_control",
    "video_ready_bridge_v2_change_held_tuple_control",
    "video_ready_bridge_v2_wrong_onehot_control",
    "video_ready_bridge_v2_blank_ack_bypass_control",
    "video_ready_bridge_v2_early_lease_open_control",
    "video_ready_bridge_v2_retain_reset_tuple_control",
    "video_ready_bridge_v2_unblank_scanout_only_control",
    "video_ready_bridge_v2_unblank_negative_control",
    "video_terminal_adapter_v2_directed",
    "video_terminal_adapter_v2_drop_held_control",
    "video_terminal_adapter_v2_change_held_control",
    "video_terminal_adapter_v2_wrong_writer_control",
    "video_terminal_adapter_v2_publish_wins_control",
    "video_terminal_adapter_v2_render_ready_control",
    "video_terminal_adapter_v2_silent_blit_drop_control",
    "video_terminal_adapter_v2_no_pop_replace_control",
    "video_terminal_adapter_v2_sv_selector_collision",
    "video_terminal_adapter_v2_cpp_selector_collision",
    "lint_zhao_video_terminal_adapter_v2",
    "renderer_lease_v2_sv_selector_collision",
    "renderer_lease_v2_cpp_selector_collision",
    "video_ready_bridge_v2_sv_selector_collision",
    "video_ready_bridge_v2_cpp_selector_collision",
    "lint_zhao_renderer_lease_v2",
    "lint_zhao_video_ready_bridge_v2",
    "packet_h_shell_prereqs_registration_static",
)

LF_ROWS = tuple(
    f"{path} text eol=lf"
    for path in (
        "fpga/rtl/video/zhao_renderer_lease_v2.sv",
        "fpga/rtl/video/zhao_video_terminal_adapter_v2.sv",
        "fpga/rtl/video/zhao_video_ready_bridge_v2.sv",
        "tests/mutants/zhao_renderer_lease_v2_mutants.sv",
        "tests/mutants/zhao_video_terminal_adapter_v2_mutants.sv",
        "tests/mutants/zhao_video_ready_bridge_v2_mutants.sv",
        "tests/video/renderer_lease_v2.sources.txt",
        "tests/video/video_terminal_adapter_v2.sources.txt",
        "tests/video/video_ready_bridge_v2.sources.txt",
        "tests/video/renderer_lease_v2_directed.cpp",
        "tests/video/video_terminal_adapter_v2_directed.cpp",
        "tests/video/video_ready_bridge_v2_directed.cpp",
        "tests/video/tb_renderer_lease_v2.sv",
        "tests/video/tb_video_terminal_adapter_v2.sv",
        "tests/video/tb_video_ready_bridge_v2.sv",
        "tests/tools/test_packet_h_shell_prereqs.py",
    )
)


def exact_manifest(path: Path, expected: tuple[str, ...]) -> tuple[str, ...]:
    rows = tuple(path.read_text(encoding="utf-8").splitlines())
    if rows != expected or len(rows) != len(set(rows)):
        raise AssertionError(f"{path.name} is not exact and ordered")
    for row in rows:
        if (not row or "\\" in row or row.startswith("/") or
                re.match(r"^[A-Za-z]:", row) or ".." in Path(row).parts or
                not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", row)):
            raise AssertionError(f"invalid source path: {row!r}")
        if not (REPO / row).is_file():
            raise AssertionError(f"missing source: {row}")
    return rows


def require_once(text: str, markers: tuple[str, ...], label: str) -> None:
    for marker in markers:
        if text.count(marker) != 1:
            raise AssertionError(f"{label} marker is not exact: {marker}")


def validate_renderer(rtl: str, mutants: str, driver: str) -> None:
    require_once(rtl, (
        "module zhao_renderer_lease_v2",
        "input  logic        lease_open_i",
        "logic request_owned_q;",
        "assign admission_open_c =",
        "assign frame_req_ready_o = rst_n && admission_open_c &&",
        "(admission_open_c || request_owned_q);",
        "if (admission_open_c && !lease_valid_i && any_free_c) begin",
        "2'd0: mode_stride = 16'd768;",
        "2'd1: mode_stride = 16'd640;",
        "`define ZHAO_RENDERER_LEASE_DUO_STRIDE 16'd512",
        "`define ZHAO_RENDERER_LEASE_DUO_VIEW1_OFFSET 32'h0001_8000",
        "assign frame_fault_clear_valid_o = rst_n && (state_q == ST_CLEAR);",
        "assign frame_valid_o = rst_n && (state_q == ST_FRAME);",
    ), "renderer RTL")
    require_once(mutants, (
        "ZHAO_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS",
        "ZHAO_RENDERER_LEASE_MUTANT_SLOT_CHOICE",
        "ZHAO_RENDERER_LEASE_MUTANT_REQUEST_HOLD",
        "ZHAO_RENDERER_LEASE_MUTANT_WRITER_RESPONSE",
        "ZHAO_RENDERER_LEASE_MUTANT_SKIP_CLEAR",
        "ZHAO_RENDERER_LEASE_MUTANT_BAD_STRIDE",
        "ZHAO_RENDERER_LEASE_MUTANT_DUO_BOUNDARY",
        "ZHAO_RENDERER_LEASE_MUTANT_STALE_IDENTITY",
    ), "renderer mutant")
    if mutants.count("`define ZHAO_RENDERER_LEASE_MUTANT_COLLISION") != 8:
        raise AssertionError("renderer collision fan-in differs")
    require_once(driver, (
        "ZHAO_RENDERER_LEASE_V2_CPP_MUTANT_SELECTOR_COLLISION",
        "held request creates exactly one upstream and manager acceptance",
        "closed epoch does not drop owned stalled request",
        "Duo view-0 final pixel offset",
        "Duo view-1 first pixel offset",
        "return zhao::report_and_exit(\"renderer_lease_v2_directed\");",
    ), "renderer driver")


def validate_bridge(rtl: str, mutants: str, driver: str) -> None:
    require_once(rtl, (
        "module zhao_video_ready_bridge_v2",
        "logic [2:0] gpu_reset_release_q;",
        "logic [2:0] vid_reset_release_q;",
        "wire gpu_local_rst_n = gpu_reset_release_q[2];",
        "wire vid_local_rst_n = vid_reset_release_q[2];",
        "always_ff @(posedge vid_clk or negedge vid_local_rst_n) begin",
        "always_ff @(posedge gpu_clk or negedge gpu_local_rst_n) begin",
        "logic        unblank_echo_seen_q;",
        "logic        unblank_scanout_seen_q;",
        "wire unblank_echo_event_c = unblank_candidate_q && echo_pop_c &&",
        "wire unblank_scanout_event_c = unblank_candidate_q &&",
        "unblank_echo_seen_q || unblank_echo_event_c,",
        "unblank_scanout_seen_q || unblank_scanout_event_c);",
        "if (unblank_complete_c) begin",
    ), "bridge RTL")
    pair_reset_users = re.findall(r"always_ff\s*@\([^\n]*negedge\s+pair_rst_n", rtl)
    if len(pair_reset_users) != 2:
        raise AssertionError("only two reset-release synchronizers may use pair_rst_n")
    require_once(mutants, (
        "ZHAO_VIDEO_BRIDGE_MUTANT_DROP_HELD_TUPLE",
        "ZHAO_VIDEO_BRIDGE_MUTANT_CHANGE_HELD_TUPLE",
        "ZHAO_VIDEO_BRIDGE_MUTANT_WRONG_ONEHOT",
        "ZHAO_VIDEO_BRIDGE_MUTANT_BLANK_ACK_BYPASS",
        "ZHAO_VIDEO_BRIDGE_MUTANT_EARLY_LEASE_OPEN",
        "ZHAO_VIDEO_BRIDGE_MUTANT_RETAIN_RESET_TUPLE",
        "ZHAO_VIDEO_BRIDGE_MUTANT_UNBLANK_SCANOUT_ONLY",
    ), "bridge mutant")
    if mutants.count("`define ZHAO_VIDEO_BRIDGE_MUTANT_COLLISION") != 7:
        raise AssertionError("bridge collision fan-in differs")
    require_once(driver, (
        "ZHAO_VIDEO_READY_BRIDGE_V2_CPP_MUTANT_SELECTOR_COLLISION",
        "scanout-only fact cannot unblank A",
        "echo-only fact cannot unblank C",
        "same-edge D facts unblank and drain both holds exactly once",
        "long echo stall keeps blank and exact A identity",
        "video_bridge_unblank_scanout_only",
        "finish(\"video_ready_bridge_v2_directed\");",
    ), "bridge driver")


def validate_terminal(rtl: str, mutants: str, driver: str) -> None:
    require_once(rtl, (
        "module zhao_video_terminal_adapter_v2",
        "input  logic        blit_publish_valid_i",
        "input  logic        blit_release_valid_i",
        "output logic        blit_refused_o",
        "input  logic        renderer_term_valid_i",
        "output logic        renderer_term_ready_o",
        "output logic        term_valid_o",
        "output logic        term_writer_o",
        "output logic [15:0] term_generation_o",
        "assign blit_same_key_c = blit_publish_valid_i && blit_release_valid_i",
        "assign renderer_release_c = renderer_term_fault_i",
        "if (blit_has_c && blit_use_release_c) begin",
        "else if (renderer_term_valid_i && renderer_release_c) begin",
        "assign blit_refused_o = rst_n && (blit_refused_delta_c != 2'd0);",
        "unique case ({push_c, pop_c})",
        "`ifndef ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT",
        "`undef ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT",
        "source_events_captured_o <= sat_inc32(source_events_captured_o);",
        "manager_terms_accepted_o <= sat_inc32(manager_terms_accepted_o);",
    ), "terminal adapter RTL")
    require_once(mutants, (
        "ZHAO_VIDEO_TERM_MUTANT_DROP_HELD",
        "ZHAO_VIDEO_TERM_MUTANT_CHANGE_HELD",
        "ZHAO_VIDEO_TERM_MUTANT_WRONG_WRITER",
        "ZHAO_VIDEO_TERM_MUTANT_PUBLISH_WINS",
        "`define ZHAO_VIDEO_TERM_MUTANT_DISABLE_RELEASE_ASSERT",
        "ZHAO_VIDEO_TERM_MUTANT_RENDER_READY",
        "ZHAO_VIDEO_TERM_MUTANT_SILENT_BLIT_DROP",
        "ZHAO_VIDEO_TERM_MUTANT_NO_POP_REPLACE",
    ), "terminal adapter mutant")
    if mutants.count("`define ZHAO_VIDEO_TERM_MUTANT_COLLISION") != 7:
        raise AssertionError("terminal adapter collision fan-in differs")
    require_once(driver, (
        "ZHAO_VIDEO_TERMINAL_ADAPTER_V2_CPP_MUTANT_SELECTOR_COLLISION",
        "different simultaneous blitter keys report the losing publication",
        "occupied adapter explicitly refuses pulse-only blitter traffic",
        "renderer fault outranks and explicitly refuses clean blitter publish",
        "pop replacement retains the previously losing renderer tuple",
        "randomized accepted-source and manager-retirement counts conserve",
        "finish(\"video_terminal_adapter_v2_directed\");",
    ), "terminal adapter driver")


def validate_cmake(text: str) -> None:
    require_once(text, (
        "Packet H: renderer lease and READY/blank bridge leaves",
        "video/renderer_lease_v2.sources.txt",
        "video/video_ready_bridge_v2.sources.txt",
        "video/video_terminal_adapter_v2.sources.txt",
        "function(zhao_packet_h_renderer_test",
        "function(zhao_packet_h_bridge_test",
        "function(zhao_packet_h_terminal_test",
        "NAME packet_h_shell_prereqs_registration_static",
        "Packet-H shell prerequisite CTest inventory must contain exactly 36 names",
    ), "Packet-H CMake")
    match = re.search(
        r"set\(ZHAO_PACKET_H_SHELL_PREREQ_REQUIRED_TESTS\n(.*?)\)",
        text, re.DOTALL)
    if match is None:
        raise AssertionError("Packet-H shell prerequisite inventory is absent")
    rows = tuple(line.strip() for line in match.group(1).splitlines())
    if rows != EXPECTED_TESTS or len(rows) != len(set(rows)):
        raise AssertionError("Packet-H shell prerequisite CTest inventory differs")


class PacketHShellPrereqTests(unittest.TestCase):
    def test_manifests_and_accounting_are_exact(self) -> None:
        exact_manifest(RENDERER_MANIFEST, RENDERER_SOURCES)
        exact_manifest(BRIDGE_MANIFEST, BRIDGE_SOURCES)
        exact_manifest(TERMINAL_MANIFEST, TERMINAL_SOURCES)
        sys.path.insert(0, str(REPO / "tools/quartus"))
        import check_prod_manifest
        tops, excluded = check_prod_manifest.read_manifest(PROD_MANIFEST)
        self.assertNotIn("zhao_renderer_lease_v2", tops)
        self.assertNotIn("zhao_video_ready_bridge_v2", tops)
        self.assertNotIn("zhao_video_terminal_adapter_v2", tops)
        self.assertEqual(excluded["zhao_renderer_lease_v2"][0], "not-yet-adopted")
        self.assertEqual(excluded["zhao_video_ready_bridge_v2"][0], "not-yet-adopted")
        self.assertEqual(excluded["zhao_video_terminal_adapter_v2"][0],
                         "not-yet-adopted")
        attrs = ATTRS.read_text(encoding="utf-8").splitlines()
        for row in LF_ROWS:
            self.assertEqual(attrs.count(row), 1, row)

    def test_renderer_controls_are_live(self) -> None:
        rtl = RENDERER_RTL.read_text(encoding="utf-8")
        mutants = RENDERER_MUTANTS.read_text(encoding="utf-8")
        driver = RENDERER_DRIVER.read_text(encoding="utf-8")
        validate_renderer(rtl, mutants, driver)
        with self.assertRaises(AssertionError):
            validate_renderer(rtl.replace("input  logic        lease_open_i", "", 1),
                              mutants, driver)
        with self.assertRaises(AssertionError):
            validate_renderer(rtl, mutants.replace(
                "ZHAO_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS", "BROKEN", 1),
                              driver)

    def test_bridge_controls_are_live(self) -> None:
        rtl = BRIDGE_RTL.read_text(encoding="utf-8")
        mutants = BRIDGE_MUTANTS.read_text(encoding="utf-8")
        driver = BRIDGE_DRIVER.read_text(encoding="utf-8")
        validate_bridge(rtl, mutants, driver)
        with self.assertRaises(AssertionError):
            validate_bridge(rtl.replace(
                "unblank_echo_seen_q || unblank_echo_event_c,", "1'b1,", 1),
                            mutants, driver)
        with self.assertRaises(AssertionError):
            validate_bridge(rtl, mutants.replace(
                "ZHAO_VIDEO_BRIDGE_MUTANT_UNBLANK_SCANOUT_ONLY", "BROKEN", 1),
                            driver)

    def test_terminal_controls_are_live(self) -> None:
        rtl = TERMINAL_RTL.read_text(encoding="utf-8")
        mutants = TERMINAL_MUTANTS.read_text(encoding="utf-8")
        driver = TERMINAL_DRIVER.read_text(encoding="utf-8")
        validate_terminal(rtl, mutants, driver)
        with self.assertRaises(AssertionError):
            validate_terminal(rtl.replace(
                "assign blit_refused_o = rst_n && (blit_refused_delta_c != 2'd0);",
                "assign blit_refused_o = 1'b0;", 1), mutants, driver)
        with self.assertRaises(AssertionError):
            validate_terminal(rtl, mutants.replace(
                "ZHAO_VIDEO_TERM_MUTANT_NO_POP_REPLACE", "BROKEN", 1), driver)

    def test_contract_and_cmake_are_exact(self) -> None:
        contract = CONTRACT.read_text(encoding="utf-8")
        require_once(contract, (
            "The synchronized Packet-H `lease_open` level gates",
            "the exact swap echo is accepted\ninto the reverse CDC",
            "Those two completion facts may arrive in either order",
            "Matching same-edge blitter publish+release resolves to one release.",
            "produces `blit_refused_o`",
            "Captured-source and\nmanager-accepted counters must conserve after drain",
        ), "VIDEO.SLOTMGR contract")
        validate_cmake(CMAKE.read_text(encoding="utf-8"))
        with self.assertRaises(AssertionError):
            validate_cmake(CMAKE.read_text(encoding="utf-8").replace(
                "  lint_zhao_video_ready_bridge_v2\n", "", 1))


def run_collision(profile: str, verilator: Path, cxx: Path) -> int:
    family, language = profile.split("-", 1)
    if family == "renderer":
        rows = exact_manifest(RENDERER_MANIFEST, RENDERER_SOURCES)
        sv_defs = (
            "ZHAO_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS",
            "ZHAO_RENDERER_LEASE_MUTANT_SLOT_CHOICE",
        )
        cpp_defs = (
            "ZHAO_EXPECT_RENDERER_LEASE_MUTANT_LEASE_OPEN_BYPASS",
            "ZHAO_EXPECT_RENDERER_LEASE_MUTANT_SLOT_CHOICE",
        )
        top = "zhao_renderer_lease_v2"
        driver = RENDERER_DRIVER
        expected = ("ZHAO_RENDERER_LEASE_V2_MUTANT_SELECTOR_COLLISION"
                    if language == "sv" else
                    "ZHAO_RENDERER_LEASE_V2_CPP_MUTANT_SELECTOR_COLLISION")
    elif family == "bridge":
        rows = exact_manifest(BRIDGE_MANIFEST, BRIDGE_SOURCES)
        sv_defs = (
            "ZHAO_VIDEO_BRIDGE_MUTANT_DROP_HELD_TUPLE",
            "ZHAO_VIDEO_BRIDGE_MUTANT_CHANGE_HELD_TUPLE",
        )
        cpp_defs = (
            "ZHAO_EXPECT_VIDEO_BRIDGE_DROP_HELD_TUPLE",
            "ZHAO_EXPECT_VIDEO_BRIDGE_CHANGE_HELD_TUPLE",
        )
        top = "zhao_video_ready_bridge_v2"
        driver = BRIDGE_DRIVER
        expected = ("ZHAO_VIDEO_READY_BRIDGE_V2_MUTANT_SELECTOR_COLLISION"
                    if language == "sv" else
                    "ZHAO_VIDEO_READY_BRIDGE_V2_CPP_MUTANT_SELECTOR_COLLISION")
    else:
        rows = exact_manifest(TERMINAL_MANIFEST, TERMINAL_SOURCES)
        sv_defs = (
            "ZHAO_VIDEO_TERM_MUTANT_DROP_HELD",
            "ZHAO_VIDEO_TERM_MUTANT_CHANGE_HELD",
        )
        cpp_defs = (
            "ZHAO_EXPECT_VIDEO_TERM_DROP_HELD",
            "ZHAO_EXPECT_VIDEO_TERM_CHANGE_HELD",
        )
        top = "zhao_video_terminal_adapter_v2"
        driver = TERMINAL_DRIVER
        expected = ("ZHAO_VIDEO_TERMINAL_ADAPTER_V2_MUTANT_SELECTOR_COLLISION"
                    if language == "sv" else
                    "ZHAO_VIDEO_TERMINAL_ADAPTER_V2_CPP_MUTANT_SELECTOR_COLLISION")
    temporary: tempfile.TemporaryDirectory[str] | None = None
    if language == "sv":
        temporary = tempfile.TemporaryDirectory(prefix="packet-h-collision-")
        command = [
            str(verilator), "--lint-only", "--Mdir", temporary.name,
            "--top-module", top,
            *(f"-D{value}" for value in sv_defs),
            *(str(REPO / row) for row in rows),
        ]
        environment = os.environ.copy()
        timeout = 120
    else:
        command = [str(cxx), "-E", "-x", "c++",
                   *(f"-D{value}" for value in cpp_defs), str(driver)]
        environment = None
        timeout = 30
    try:
        completed = subprocess.run(
            command, cwd=REPO, env=environment, capture_output=True, text=True,
            errors="replace", timeout=timeout, check=False)
    finally:
        if temporary is not None:
            temporary.cleanup()
    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0 or expected not in diagnostic:
        print(f"FAIL: {profile} collision rc={completed.returncode}\n"
              f"{diagnostic[-4000:]}", file=sys.stderr)
        return 1
    print(f"PACKET_H_SELECTOR_COLLISION[{profile}] FIRED")
    return 0


def expect_failure(executable: Path, diagnostic: str) -> int:
    completed = subprocess.run(
        [str(executable.resolve())], capture_output=True, text=True,
        errors="replace", timeout=30, check=False)
    text = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0 or diagnostic not in text:
        print(f"FAIL: negative control rc={completed.returncode}\n{text[-4000:]}",
              file=sys.stderr)
        return 1
    print("PACKET_H_NEGATIVE_CONTROL FIRED")
    return 0


def main() -> int:
    if "--collision" not in sys.argv and "--negative-exe" not in sys.argv:
        unittest.main()
        return 0
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--collision", choices=(
        "renderer-sv", "renderer-cpp", "bridge-sv", "bridge-cpp",
        "terminal-sv", "terminal-cpp"))
    parser.add_argument("--verilator", type=Path)
    parser.add_argument("--cxx", type=Path)
    parser.add_argument("--negative-exe", type=Path)
    parser.add_argument("--diagnostic")
    args = parser.parse_args()
    if args.collision:
        if not args.verilator or not args.verilator.is_file():
            parser.error("--verilator must name the exact executable")
        if not args.cxx or not args.cxx.is_file():
            parser.error("--cxx must name the configured compiler")
        return run_collision(args.collision, args.verilator.resolve(),
                             args.cxx.resolve())
    if not args.negative_exe or not args.negative_exe.is_file():
        parser.error("--negative-exe must name a built executable")
    if not args.diagnostic:
        parser.error("--diagnostic is required with --negative-exe")
    return expect_failure(args.negative_exe, args.diagnostic)


if __name__ == "__main__":
    raise SystemExit(main())
