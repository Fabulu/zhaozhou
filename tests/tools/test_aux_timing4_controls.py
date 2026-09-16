#!/usr/bin/env python3
"""Timing4 AUX register/borrow controls and selector collisions."""

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
PIPE_MANIFEST = REPO / "tests/texture/aux_pipe_v2_timing4.sources.txt"
DIV_MANIFEST = REPO / "tests/texture/aux_div6_timing4.sources.txt"
MUTANTS = REPO / "tests/mutants/zhao_texture_aux_timing4_mutants.sv"
PIPE = REPO / "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv"
DIV = REPO / "fpga/rtl/texture/zhao_texture_aux_div6.sv"
PIPE_DRIVER = REPO / "tests/texture/texture_aux_pipe_v2_directed.cpp"
DIV_DRIVER = REPO / "tests/texture/texture_aux_div6_directed.cpp"
CMAKE = REPO / "tests/CMakeLists.txt"
ATTRS = REPO / ".gitattributes"
PIPE_SOURCES = (
    "tests/mutants/zhao_texture_aux_timing4_mutants.sv",
    "fpga/rtl/texture/zhao_texture_aux_div6.sv",
    "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv",
)
DIV_SOURCES = (
    "tests/mutants/zhao_texture_aux_timing4_mutants.sv",
    "fpga/rtl/texture/zhao_texture_aux_div6.sv",
)
EXPECTED_TESTS = (
    "texture_aux_pipe_v2_timing4_degenerate_control",
    "texture_aux_pipe_v2_timing4_input_fault_control",
    "texture_aux_div6_timing4_borrow_control",
    "texture_aux_timing4_sv_selector_collision",
    "texture_aux_timing4_cpp_selector_collision",
    "texture_aux_timing4_registration_static",
)


def exact_manifest(path: Path, expected: tuple[str, ...]) -> tuple[str, ...]:
    raw = path.read_bytes()
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise AssertionError(f"{path.name} is not canonical LF text")
    rows = tuple(raw.decode("utf-8").splitlines())
    if rows != expected or len(rows) != len(set(rows)):
        raise AssertionError(f"{path.name} is not exact and ordered")
    for row in rows:
        if (not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", row)
                or ".." in Path(row).parts or "\\" in row
                or not (REPO / row).is_file()):
            raise AssertionError(f"invalid AUX Timing4 source: {row!r}")
    return rows


def require_once(text: str, markers: tuple[str, ...], label: str) -> None:
    for marker in markers:
        if text.count(marker) != 1:
            raise AssertionError(f"{label} marker is not exact: {marker}")


def validate_cmake(text: str) -> None:
    require_once(text, (
        "Packet B/Timing4: AUX registered-fault and borrow controls",
        "texture/aux_pipe_v2_timing4.sources.txt",
        "texture/aux_div6_timing4.sources.txt",
        "ZHAO_AUX_T4_MUTANT_DEGENERATE_DROP",
        "ZHAO_AUX_T4_MUTANT_INPUT_FAULT_DROP",
        "ZHAO_AUX_T4_MUTANT_BORROW_REVERSE",
        "NAME texture_aux_timing4_registration_static",
        "Timing4 AUX CTest inventory must contain exactly 6 names",
    ), "Timing4 AUX CMake")
    match = re.search(
        r"set\(ZHAO_AUX_TIMING4_REQUIRED_TESTS\n(.*?)\)", text, re.DOTALL)
    if match is None:
        raise AssertionError("Timing4 AUX required inventory is absent")
    names = tuple(line.strip() for line in match.group(1).splitlines())
    if names != EXPECTED_TESTS or len(names) != len(set(names)):
        raise AssertionError("Timing4 AUX required inventory differs")


class AuxTiming4Tests(unittest.TestCase):
    def test_manifests_and_attributes(self) -> None:
        exact_manifest(PIPE_MANIFEST, PIPE_SOURCES)
        exact_manifest(DIV_MANIFEST, DIV_SOURCES)
        attrs = ATTRS.read_text(encoding="utf-8").splitlines()
        for row in (
            "tests/mutants/zhao_texture_aux_timing4_mutants.sv text eol=lf",
            "tests/texture/aux_pipe_v2_timing4.sources.txt text eol=lf",
            "tests/texture/aux_div6_timing4.sources.txt text eol=lf",
            "tests/texture/texture_aux_pipe_v2_directed.cpp text eol=lf",
            "tests/texture/texture_aux_div6_directed.cpp text eol=lf",
            "tests/tools/test_aux_timing4_controls.py text eol=lf",
        ):
            self.assertEqual(attrs.count(row), 1, row)

    def test_registered_fault_boundary(self) -> None:
        text = PIPE.read_text(encoding="utf-8")
        require_once(text, (
            "logic                    a0_degenerate_q;",
            "logic                    a0_input_fault_q;",
            "a0_degenerate_q <= `ZHAO_AUX_T4_DEGENERATE_CAPTURE(",
            "a0_input_fault_q <= `ZHAO_AUX_T4_INPUT_FAULT_CAPTURE(",
            "if (a0_degenerate_q)",
            "if (a0_input_fault_q)",
            "if (frame_fault_clear_i)\n        frame_fault_o <= 1'b0;",
        ), "AUX A0")
        self.assertNotIn("if (job_force_refuse_i)\n          frame_fault_o", text)

    def test_borrow_step(self) -> None:
        text = DIV.read_text(encoding="utf-8")
        require_once(text, (
            "logic [REM_W:0]   difference;",
            "difference = {1'b0, r} - {1'b0, shifted};",
            "hit        = `ZHAO_AUX_T4_DIV_HIT(difference);",
            "step       = {hit, (hit ? difference[REM_W-1:0] : r)};",
        ), "AUX divider")
        self.assertNotIn("r >= shifted", text[text.index("function automatic"):])

    def test_mutants_and_drivers(self) -> None:
        mutant = MUTANTS.read_text(encoding="utf-8")
        require_once(mutant, (
            "ZHAO_AUX_T4_MUTANT_DEGENERATE_DROP",
            "ZHAO_AUX_T4_MUTANT_INPUT_FAULT_DROP",
            "ZHAO_AUX_T4_MUTANT_BORROW_REVERSE",
            'ZHAO_AUX_TIMING4_MUTANT_SELECTOR_COLLISION',
        ), "AUX Timing4 mutant")
        pipe_driver = PIPE_DRIVER.read_text(encoding="utf-8")
        div_driver = DIV_DRIVER.read_text(encoding="utf-8")
        for marker in (
            "AUX Timing4 A0 degenerate mutant FIRED exactly once",
            "AUX Timing4 A0 input-fault mutant FIRED exactly once",
            "registered A0 input fault has priority over same-edge clear",
        ):
            self.assertIn(marker, pipe_driver)
        for marker in (
            "six requests are in flight simultaneously (II=1, not serial)",
            "AUX Timing4 divider borrow mutant DETECTED mismatches=%d",
        ):
            self.assertIn(marker, div_driver)

    def test_cmake(self) -> None:
        validate_cmake(CMAKE.read_text(encoding="utf-8"))


def collision(profile: str, verilator: Path, cxx: Path) -> int:
    if profile == "sv":
        rows = exact_manifest(PIPE_MANIFEST, PIPE_SOURCES)
        with tempfile.TemporaryDirectory(prefix="aux-t4-collision-") as temporary:
            completed = subprocess.run(
                [str(verilator), "--lint-only", "--Mdir", temporary,
                 "--top-module", "zhao_texture_aux_pipe_v2",
                 "-DZHAO_AUX_T4_MUTANT_DEGENERATE_DROP",
                 "-DZHAO_AUX_T4_MUTANT_BORROW_REVERSE",
                 *(str(REPO / row) for row in rows)],
                cwd=REPO, env=os.environ.copy(), capture_output=True, text=True,
                errors="replace", timeout=120, check=False)
        expected = "ZHAO_AUX_TIMING4_MUTANT_SELECTOR_COLLISION"
    else:
        completed = subprocess.run(
            [str(cxx), "-E", "-x", "c++",
             "-DZHAO_AUX_T4_DEGENERATE_MUTANT_CONTROL",
             "-DZHAO_AUX_T4_INPUT_FAULT_MUTANT_CONTROL", str(PIPE_DRIVER)],
            cwd=REPO, capture_output=True, text=True, errors="replace",
            timeout=30, check=False)
        expected = "ZHAO_AUX_T4_A0_DRIVER_SELECTOR_COLLISION"
    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0 or expected not in diagnostic:
        print(f"FAIL: AUX Timing4 {profile} collision rc={completed.returncode}\n"
              f"{diagnostic[-4000:]}", file=sys.stderr)
        return 1
    print(f"AUX_TIMING4_SELECTOR_COLLISION[{profile}] FIRED")
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
        parser.error("tool arguments must name existing files")
    return collision(args.collision, args.verilator.resolve(), args.cxx.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
