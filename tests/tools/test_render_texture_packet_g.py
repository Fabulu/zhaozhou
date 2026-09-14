#!/usr/bin/env python3
"""Packet-G lease/CDC registration, contract, and detector controls."""

from __future__ import annotations

import hashlib
from pathlib import Path
import re
import unittest


REPO = Path(__file__).resolve().parents[2]
SLOT = REPO / "fpga/rtl/video/zhao_video_slotmgr_v2.sv"
CDC = REPO / "fpga/rtl/video/zhao_fb_ready_cdc_v2.sv"
SLOT_MUTANTS = REPO / "tests/mutants/zhao_video_slotmgr_v2_mutants.sv"
CDC_MUTANTS = REPO / "tests/mutants/zhao_fb_ready_cdc_v2_mutants.sv"
SLOT_DRIVER = REPO / "tests/video/video_slotmgr_v2_directed.cpp"
CDC_DRIVER = REPO / "tests/video/fb_ready_cdc_v2_directed.cpp"
SLOT_MANIFEST = REPO / "tests/video/packet_g_slotmgr_v2.sources.txt"
CDC_MANIFEST = REPO / "tests/video/packet_g_cdc_v2.sources.txt"
CONNECTED_MANIFEST = REPO / "tests/video/packet_g_connected.sources.txt"
CONNECTED_TOP = REPO / "tests/video/tb_packet_g_lease_cdc.sv"
CONNECTED_DRIVER = REPO / "tests/video/packet_g_connected_directed.cpp"
CONNECTED_MUTANTS = REPO / "tests/mutants/zhao_packet_g_connected_mutants.sv"
CMAKE = REPO / "tests/CMakeLists.txt"
PROD_MANIFEST = REPO / "design/prod_manifest.yml"
CONTRACT = REPO / "design/contracts/VIDEO.SLOTMGR.md"
PROD_TOP = REPO / "fpga/rtl/prod/zhao_prod_top.sv"
SHELL = REPO / "fpga/rtl/common/zhao_shell_top.sv"
ATTRS = REPO / ".gitattributes"
COLLISION = REPO / "tests/tools/test_packet_g_selector_collision.py"
ASSERTION_CONTROL = REPO / "tests/tools/test_packet_g_assertion_control.py"

SHELL_SHA256 = "00fdd2387ffea985bb6d3d0e2a9b21bde2913478d33333d30d11b64ae5450783"
SLOT_SOURCES = (
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_pkg.sv",
    "tests/mutants/zhao_video_slotmgr_v2_mutants.sv",
    "fpga/rtl/video/zhao_video_slotmgr_v2.sv",
)
CDC_SOURCES = (
    "tests/mutants/zhao_fb_ready_cdc_v2_mutants.sv",
    "fpga/rtl/common/zhao_dc_sdp_ram.sv",
    "fpga/rtl/video/zhao_fb_ready_cdc_v2.sv",
)
CONNECTED_SOURCES = (
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_pkg.sv",
    "fpga/rtl/common/zhao_dc_sdp_ram.sv",
    "fpga/rtl/memory/zhao_mem_guard.sv",
    "tests/mutants/zhao_video_slotmgr_v2_mutants.sv",
    "fpga/rtl/video/zhao_video_slotmgr_v2.sv",
    "tests/mutants/zhao_fb_ready_cdc_v2_mutants.sv",
    "fpga/rtl/video/zhao_fb_ready_cdc_v2.sv",
    "fpga/rtl/video/zhao_video_framectl.sv",
    "tests/mutants/zhao_packet_g_connected_mutants.sv",
    "tests/video/tb_packet_g_lease_cdc.sv",
)
SLOT_SELECTORS = (
    "ZHAO_SLOT_V2_MUTANT_TERM_OMIT_WRITER",
    "ZHAO_SLOT_V2_MUTANT_TERM_SLOT_ONLY",
    "ZHAO_SLOT_V2_MUTANT_FAULT_PUBLISHES",
    "ZHAO_SLOT_V2_MUTANT_SLOT0_BASE",
    "ZHAO_SLOT_V2_MUTANT_SWAP_OMIT_GENERATION",
    "ZHAO_SLOT_V2_MUTANT_LIVE_READY_WRITER",
)
CDC_SELECTORS = (
    "ZHAO_FB_CDC_MUTANT_IGNORE_FULL",
    "ZHAO_FB_CDC_MUTANT_BYPASS_BARRIER",
    "ZHAO_FB_CDC_MUTANT_ZERO_READY_GENERATION",
    "ZHAO_FB_CDC_MUTANT_REPEAT_READ",
)
EXPECTED_TESTS = (
    "packet_g_slotmgr_directed",
    "packet_g_slotmgr_term_omit_writer_mutant",
    "packet_g_slotmgr_term_slot_only_mutant",
    "packet_g_slotmgr_fault_publishes_mutant",
    "packet_g_slotmgr_slot0_base_mutant",
    "packet_g_slotmgr_swap_omit_generation_mutant",
    "packet_g_slotmgr_live_ready_writer_mutant",
    "packet_g_cdc_directed",
    "packet_g_cdc_ignore_full_mutant",
    "packet_g_cdc_bypass_barrier_mutant",
    "packet_g_cdc_zero_ready_generation_mutant",
    "packet_g_cdc_repeat_read_mutant",
    "packet_g_cdc_ignore_full_assertion",
    "packet_g_connected_directed",
    "packet_g_connected_raw_terminal_mutant",
    "packet_g_slot_sv_selector_collision",
    "packet_g_slot_cpp_selector_collision",
    "packet_g_cdc_sv_selector_collision",
    "packet_g_cdc_cpp_selector_collision",
    "packet_g_registration_static",
    "lint_zhao_video_slotmgr_v2",
    "lint_zhao_fb_ready_cdc_v2",
)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require_once(source: str, markers: tuple[str, ...], label: str) -> None:
    for marker in markers:
        if source.count(marker) != 1:
            raise AssertionError(f"{label} marker is not exact/unique: {marker}")


def validate_source_manifest(raw: str, expected: tuple[str, ...]) -> None:
    if not raw.endswith("\n") or "\r" in raw:
        raise AssertionError("Packet-G source manifest must be LF with final newline")
    rows = tuple(raw[:-1].split("\n"))
    if rows != expected or len(rows) != len(set(rows)):
        raise AssertionError("Packet-G source manifest order/content differs")
    for row in rows:
        if not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", row):
            raise AssertionError(f"invalid Packet-G source path: {row}")


def cmake_inventory(source: str) -> tuple[str, ...]:
    match = re.search(
        r"set\(ZHAO_PACKET_G_REQUIRED_TESTS\n(.*?)\)", source, re.DOTALL
    )
    if match is None:
        raise AssertionError("Packet-G CTest inventory missing")
    return tuple(line.strip() for line in match.group(1).splitlines())


def validate_cmake(source: str) -> None:
    if cmake_inventory(source) != EXPECTED_TESTS:
        raise AssertionError("Packet-G CTest inventory is not exact")
    require_once(source, (
        "function(zhao_packet_g_load_sources MANIFEST EXPECTED_COUNT OUT_VAR)",
        '"${ZHAO_PACKET_G_SLOTMGR_MANIFEST}" 4 ZHAO_PACKET_G_SLOTMGR_SOURCES)',
        '"${ZHAO_PACKET_G_CDC_MANIFEST}" 3 ZHAO_PACKET_G_CDC_SOURCES)',
        '"${ZHAO_PACKET_G_CONNECTED_MANIFEST}" 11 ZHAO_PACKET_G_CONNECTED_SOURCES)',
        "Packet-G required CTest inventory must contain exactly 22 names",
        "if(NOT ZHAO_PACKET_G_REQUIRED_TEST_COUNT EQUAL 22)",
        'if(NOT TEST "${required_packet_g_test}")',
        "tools/test_packet_g_selector_collision.py",
        "tools/test_packet_g_assertion_control.py",
        "ZHAO_EXPECT_CDC_ASSERT_IGNORE_FULL",
        "tools/test_render_texture_packet_g.py -q",
    ), "Packet-G CMake")


def validate_manifest(source: str) -> None:
    for module in ("zhao_video_slotmgr_v2", "zhao_fb_ready_cdc_v2"):
        matches = re.findall(rf"^\s*- {module}:\s+([^\n]+)$", source, re.MULTILINE)
        if len(matches) != 1 or not matches[0].startswith("not-yet-adopted  Packet-G"):
            raise AssertionError(f"{module} is not exactly excluded:not-yet-adopted")


def validate_slot(source: str, mutants: str) -> None:
    header_end = source.find(");")
    header = source[:header_end]
    if "render_fb_base_i" in header or "request_base_i" in header:
        raise AssertionError("slot manager admits caller-owned framebuffer base")
    if ("READY without clean publication" in source or
            "ready_events_o > publications_o" in source):
        raise AssertionError("slot manager retains a lockstep-blind READY counter assertion")
    require_once(source, (
        "if (render_req_valid_i && blit_req_valid_i)\n          rr_last_render_q <= request_pick_render_c;",
        "assign ready_valid_o = ready_valid_q || term_clean_publish_c;",
        "term_fault_i || lease_fault_q || fault_match_c;",
        "(ready_valid_q || !ready_ready_i)) begin",
        "request_base_c = `ZHAO_SLOT_V2_DERIVED_BASE(request_slot_c);",
        "assign fault_ready_o = rst_n;",
        "assign term_ready_o = rst_n && (!term_clean_publish_c || ready_room_c);",
        "assign swap_ready_o = rst_n;",
        '$fatal(1, "ZHAO_VIDEO_SLOTMGR_V2_MUTANT_SELECTOR_COLLISION");',
    ), "Packet-G slot manager")
    for selector in SLOT_SELECTORS:
        if mutants.count(f"`ifdef {selector}") != 1:
            raise AssertionError(f"slot mutant selector not exact: {selector}")
    term_match_hook = (
        "`define ZHAO_SLOT_V2_TERM_MATCH(writer, slot, generation, "
        "lease_writer, lease_slot, lease_generation)"
    )
    if mutants.count(term_match_hook) != 2:
        raise AssertionError("slot terminal-match mutants must define exactly two inverses")
    require_once(mutants, (
        "`define ZHAO_SLOT_V2_DERIVED_BASE(slot) ZHAO_FB_SLOT0_BASE",
        "`define ZHAO_SLOT_V2_READY_WRITER(captured) render_req_valid_i",
    ), "Packet-G slot mutants")


def validate_cdc(source: str, mutants: str) -> None:
    if "ZHAO_FB_CDC_MUTANT_ACTIVE" in source or "ZHAO_FB_CDC_MUTANT_ACTIVE" in mutants:
        raise AssertionError("CDC mutant globally suppresses its assertions")
    if source.count("zhao_dc_sdp_ram #(.DATA_W(DATA_W), .ADDR_W(ADDR_W))") != 2:
        raise AssertionError("Packet-G CDC must instantiate exactly two 84x4 RAMs")
    require_once(source, (
        "wire pair_rst_n = gpu_rst_n && vid_rst_n;",
        "wire gpu_local_rst_n = gpu_reset_release_q[2];",
        "wire vid_local_rst_n = vid_reset_release_q[2];",
        "assign gpu_barrier_done_o = gpu_up_q[2] && vid_up_gpu_m3_q;",
        "assign vid_barrier_done_o = vid_up_q[2] && gpu_up_vid_m3_q;",
        "assign ready_full_c = ready_wr_gray_increment_c ==",
        "assign swap_full_c = swap_wr_gray_increment_c ==",
        "(ready_memory_level_o == 3'd0) && swap_empty_c &&",
        "(swap_memory_level_o == 3'd0) && ready_empty_c &&",
        "assert (ready_memory_level_o <= 3'd3)",
        "assert (swap_memory_level_o <= 3'd3)",
        '$fatal(1, "ZHAO_FB_READY_CDC_V2_MUTANT_SELECTOR_COLLISION");',
    ), "Packet-G CDC")
    for selector in CDC_SELECTORS:
        if mutants.count(f"`ifdef {selector}") != 1:
            raise AssertionError(f"CDC mutant selector not exact: {selector}")
    require_once(mutants, (
        "`define ZHAO_FB_CDC_FULL(full) 1'b0",
        "`define ZHAO_FB_CDC_BARRIER(done) 1'b1",
        "`define ZHAO_FB_CDC_READY_TUPLE(tuple)",
        "`define ZHAO_FB_CDC_RD_NEXT(next_value)",
    ), "Packet-G CDC mutants")


def validate_connected(top: str, driver: str, mutants: str) -> None:
    for module in (
        "zhao_video_slotmgr_v2 u_slotmgr",
        "zhao_fb_ready_cdc_v2 u_cdc",
        "zhao_video_framectl u_framectl",
        "zhao_mem_guard u_render_guard",
        "zhao_mem_guard u_blit_guard",
    ):
        if top.count(module) != 1:
            raise AssertionError(f"connected Packet-G instance not exact: {module}")
    if top.count(".fb_writer(lease_writer_o)") != 2:
        raise AssertionError("both connected framebuffer guards must use captured writer")
    require_once(top, (
        ".gpu_ready_valid_i(seam_ready_valid_w)",
        ".gpu_ready_tuple_i(manager_ready_tuple_w)",
        ".gpu_swap_tuple_o(manager_swap_tuple_w)",
        ".swap_generation_i(manager_swap_tuple_w[81:66])",
        "(video_pending_q && video_swap_room_c)",
        "video_swap_tuple_q <= video_pending_tuple_q;",
    ), "Packet-G connected top")
    require_once(mutants, (
        "`ifdef ZHAO_PACKET_G_CONNECTED_MUTANT_RAW_TERMINAL_READY",
        "((terminal_valid) && (terminal_publish))",
    ), "Packet-G connected mutant")
    require_once(driver, (
        "empty connected frame boundary repeats without swap",
        "returned full tuple becomes exact displayed renderer frame",
        "same-edge fault release creates no READY CDC write",
        "renderer guard accepts matching writer inside lease",
        "blitter guard refuses wrong writer inside same window",
        "pre-reset pending event cannot swap after barrier",
        "[packet_g_connected_raw_terminal]",
    ), "Packet-G connected driver")


class PacketGTests(unittest.TestCase):
    def test_source_manifests_are_exact_and_ordered(self) -> None:
        validate_source_manifest(text(SLOT_MANIFEST), SLOT_SOURCES)
        validate_source_manifest(text(CDC_MANIFEST), CDC_SOURCES)
        validate_source_manifest(text(CONNECTED_MANIFEST), CONNECTED_SOURCES)
        with self.assertRaises(AssertionError):
            validate_source_manifest("\n".join(reversed(SLOT_SOURCES)) + "\n", SLOT_SOURCES)
        with self.assertRaises(AssertionError):
            validate_source_manifest("\n".join(CDC_SOURCES) + "\n\n", CDC_SOURCES)
        attrs = set(text(ATTRS).splitlines())
        for rule in (
            "fpga/rtl/video/zhao_video_slotmgr_v2.sv text eol=lf",
            "fpga/rtl/video/zhao_fb_ready_cdc_v2.sv text eol=lf",
            "tests/mutants/zhao_video_slotmgr_v2_mutants.sv text eol=lf",
            "tests/mutants/zhao_fb_ready_cdc_v2_mutants.sv text eol=lf",
            "tests/mutants/zhao_packet_g_connected_mutants.sv text eol=lf",
            "tests/video/packet_g_*.sources.txt text eol=lf",
            "tests/video/tb_packet_g_lease_cdc.sv text eol=lf",
            "tests/video/video_slotmgr_v2_directed.cpp text eol=lf",
            "tests/video/fb_ready_cdc_v2_directed.cpp text eol=lf",
            "tests/video/packet_g_connected_directed.cpp text eol=lf",
            "tests/tools/test_packet_g_assertion_control.py text eol=lf",
            "tests/tools/test_packet_g_selector_collision.py text eol=lf",
            "tests/tools/test_render_texture_packet_g.py text eol=lf",
        ):
            self.assertIn(rule, attrs)
        with self.assertRaises(AssertionError):
            self.assertIn(
                "fpga/rtl/video/zhao_video_slotmgr_v2.sv text eol=lf",
                attrs - {"fpga/rtl/video/zhao_video_slotmgr_v2.sv text eol=lf"},
            )

    def test_cmake_inventory_and_manifest_consumption_are_exact(self) -> None:
        source = text(CMAKE)
        validate_cmake(source)
        with self.assertRaises(AssertionError):
            validate_cmake(source.replace(
                "if(NOT ZHAO_PACKET_G_REQUIRED_TEST_COUNT EQUAL 22)",
                "if(NOT ZHAO_PACKET_G_REQUIRED_TEST_COUNT EQUAL 21)", 1))
        with self.assertRaises(AssertionError):
            validate_cmake(source.replace("  lint_zhao_fb_ready_cdc_v2)", ")", 1))

    def test_both_v2_modules_are_excluded_once(self) -> None:
        source = text(PROD_MANIFEST)
        validate_manifest(source)
        with self.assertRaises(AssertionError):
            validate_manifest(source.replace("zhao_fb_ready_cdc_v2: not-yet-adopted",
                                             "zhao_fb_ready_cdc_v2: probe", 1))

    def test_slot_manager_laws_and_mutants_are_present(self) -> None:
        source, mutants = text(SLOT), text(SLOT_MUTANTS)
        validate_slot(source, mutants)
        with self.assertRaises(AssertionError):
            validate_slot(source.replace("ready_valid_q || term_clean_publish_c",
                                         "ready_valid_q", 1), mutants)
        with self.assertRaises(AssertionError):
            validate_slot(source, mutants.replace(SLOT_SELECTORS[0], "BROKEN", 1))
        with self.assertRaises(AssertionError):
            validate_slot(source + "\nassert (ready_events_o > publications_o);\n",
                          mutants)

    def test_cdc_laws_and_mutants_are_present(self) -> None:
        source, mutants = text(CDC), text(CDC_MUTANTS)
        validate_cdc(source, mutants)
        with self.assertRaises(AssertionError):
            validate_cdc(source.replace(" && swap_empty_c", "", 1), mutants)
        with self.assertRaises(AssertionError):
            validate_cdc(source, mutants.replace(CDC_SELECTORS[0], "BROKEN", 1))

    def test_cpp_driver_selector_guards_are_live(self) -> None:
        require_once(text(SLOT_DRIVER), (
            "ZHAO_VIDEO_SLOTMGR_V2_CPP_MUTANT_SELECTOR_COLLISION",
            "generation wraps after 65536 accepted leases",
            "neither writer starves across repeated contentions",
            "held loser receives exact refusal identity",
        ), "Packet-G slot driver")
        cdc_driver = text(CDC_DRIVER)
        cdc_markers = (
            "ZHAO_FB_READY_CDC_V2_CPP_MUTANT_SELECTOR_COLLISION",
            "GPU stalled-payload drift detector fires",
            "VID stalled-payload drift detector fires",
            "pending pre-reset RAM read cannot survive",
            "reverse occupancy barrier reopens",
            "only post-reset swap tuple survives",
            "pending pre-reset swap RAM read cannot survive",
            "held pre-reset swap output cannot reappear",
        )
        require_once(cdc_driver, cdc_markers, "Packet-G CDC driver")
        with self.assertRaises(AssertionError):
            require_once(
                cdc_driver.replace("reverse occupancy barrier reopens", "", 1),
                cdc_markers,
                "mutated Packet-G CDC driver",
            )
        require_once(text(ASSERTION_CONTROL), (
            "fb_ready_cdc_v2: READY FIFO RAM ownership exceeded three",
            'diagnostic.count("Assertion failed") != 1',
            "PACKET_G_ASSERTION_CONTROL[cdc-ignore-full] FIRED",
        ), "Packet-G assertion control")

    def test_collision_helper_has_four_exact_profiles(self) -> None:
        source = text(COLLISION)
        for profile in ("slot-sv", "slot-cpp", "cdc-sv", "cdc-cpp"):
            self.assertEqual(source.count(f'"{profile}"'), 1)
        require_once(source, (
            "subprocess.TimeoutExpired",
            "unexpectedly compiled",
            "missed exact diagnostic",
        ), "Packet-G selector helper")

    def test_connected_seam_uses_real_blocks_and_has_inverse(self) -> None:
        top, driver, mutants = (
            text(CONNECTED_TOP), text(CONNECTED_DRIVER), text(CONNECTED_MUTANTS)
        )
        validate_connected(top, driver, mutants)
        with self.assertRaises(AssertionError):
            validate_connected(
                top.replace(".gpu_ready_valid_i(seam_ready_valid_w)",
                            ".gpu_ready_valid_i(term_valid_i)", 1),
                driver, mutants,
            )

    def test_contract_names_the_closed_v2_laws(self) -> None:
        source = text(CONTRACT)
        require_once(source, (
            "## Packet-G V2 successor (excluded, not yet adopted)",
            "A clean terminal accepted while the READY hold is empty offers its 84-bit tuple",
            "a shared pair reset immediately",
            "total channel ownership to four",
            "passes only after observing its named defect.",
        ), "Packet-G contract")

    def test_protected_shell_is_byte_exact(self) -> None:
        self.assertEqual(hashlib.sha256(SHELL.read_bytes()).hexdigest(), SHELL_SHA256)

    def test_excluded_v2_modules_do_not_enter_generated_production_top(self) -> None:
        source = text(PROD_TOP)
        self.assertNotIn("zhao_video_slotmgr_v2", source)
        self.assertNotIn("zhao_fb_ready_cdc_v2", source)


if __name__ == "__main__":
    unittest.main()
