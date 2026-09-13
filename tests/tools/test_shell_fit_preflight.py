#!/usr/bin/env python3
"""Positive-control tests for shell-fit preflight and evidence parsers."""

from __future__ import annotations

import copy
from contextlib import redirect_stderr, redirect_stdout
from decimal import Decimal
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest import mock


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "quartus"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
ARCHIVED_CLOCKS = REPO / "reports" / "characterization" / "clocks.rpt"
ARCHIVED_STA = REPO / "reports" / "characterization" / "zhao_shell_fit.sta.rpt"
ARCHIVED_TIMING_METRICS = REPO / "reports" / "characterization" / "timing_metrics.tsv"
ARCHIVED_CLOCK_TRANSFERS = REPO / "reports" / "characterization" / "clock_transfers.rpt"
ARCHIVED_UNCONSTRAINED = REPO / "reports" / "characterization" / "unconstrained_paths.rpt"
ARCHIVED_SETUP_PATHS = REPO / "reports" / "characterization" / "setup_paths.rpt"
ARCHIVED_HOLD_PATHS = REPO / "reports" / "characterization" / "hold_paths.rpt"
ARCHIVED_RECOVERY_PATHS = REPO / "reports" / "characterization" / "recovery_paths.rpt"
ARCHIVED_REMOVAL_PATHS = REPO / "reports" / "characterization" / "removal_paths.rpt"
ARCHIVED_MAP_SUMMARY = REPO / "reports" / "synthesis" / "blockpaths" / "zhao_geom_project.map.summary"
ARCHIVED_MAP_REPORT = REPO / "reports" / "synthesis" / "blockpaths" / "zhao_geom_project.map.rpt"
ARCHIVED_WRAPPED_WARNING = (
    REPO / "reports" / "composed" / "wumen-ff932e5-20260822T104634Z" / "run.log"
)
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import shell_fit_reports as reports_module
from shell_fit_qsf import (
    parse_cmake_source_pool,
    parse_qpf,
    parse_qsf,
    read_sdc_closure,
    validate_qsf,
    validate_sdc_closure,
)
from shell_fit_reports import (
    bind_receipt_to_evidence,
    build_receipt_from_evidence,
    collect_critical_warnings,
    main as reports_main,
    parse_clock_constraints,
    parse_fit_summary,
    parse_fitter_hierarchy,
    parse_git_evidence,
    parse_git_index_flags,
    load_published_receipt_pair,
    parse_map_hierarchy,
    parse_map_summary,
    parse_post_map_connectivity,
    parse_receipt,
    parse_timequest_status,
    read_git_blobs_at_commit,
    read_git_index_concealment,
    require_exact_hierarchy_row,
    require_exact_map_hierarchy_row,
    require_shell_hierarchy,
    require_shell_map_hierarchy,
    scan_virtual_clock_warnings,
    timing_evidence,
    validate_clock_constraints,
    validate_fit_summary,
    validate_map_hierarchy,
    validate_map_summary,
    validate_post_map_connectivity,
    validate_receipt,
    validate_report_messages,
    validate_stage_logs,
)
from shell_ports import ShellPortError


class QsfPreflightTests(unittest.TestCase):
    EXPECTED = (
        "fpga/rtl/common/zhao_pkg.sv",
        "fpga/rtl/generated/zhao_abi_pkg.sv",
        "fpga/rtl/generated/zhao_shell_fit_top.sv",
    )

    EXPECTED_SDC = ("tests/tools/fixtures/shell_fit_constraints_clean.sdc",)

    def parse_fixture(self, name: str):
        path = FIXTURES / name
        return parse_qsf(
            path.read_text(encoding="utf-8"), repo=REPO, qsf_dir=path.parent
        )

    def test_clean_qsf_and_ordered_source_pool(self) -> None:
        cmake = FIXTURES / "shell_fit_source_pool.cmake"
        source_pool = parse_cmake_source_pool(
            cmake.read_text(encoding="utf-8"),
            variable="SHELL_FIT_FIXTURE_RTL",
            repo=REPO,
            cmake_dir=cmake.parent,
        )
        self.assertEqual(source_pool, self.EXPECTED[:-1])
        model = self.parse_fixture("shell_fit_qsf_clean.qsf")
        validate_qsf(
            model,
            expected_top="zhao_shell_fit_top",
            expected_sources=(*source_pool, self.EXPECTED[-1]),
            expected_constraints=self.EXPECTED_SDC,
        )
        self.assertEqual(model.virtual_pin_targets, ())
        self.assertEqual(model.physical_pin_targets, ())
        self.assertEqual(model.wildcard_targets, ())

    def test_real_cmake_shell_pool_is_nonvacuous_and_unique(self) -> None:
        cmake = REPO / "tests" / "CMakeLists.txt"
        source_pool = parse_cmake_source_pool(
            cmake.read_text(encoding="utf-8"),
            variable="ZHAO_SHELL_RTL",
            repo=REPO,
            cmake_dir=cmake.parent,
        )
        self.assertEqual(len(source_pool), 55)
        self.assertEqual(len(source_pool), len(set(source_pool)))
        self.assertIn("fpga/rtl/common/zhao_shell_top.sv", source_pool)

    def test_one_virtual_pin_fires(self) -> None:
        model = self.parse_fixture("shell_fit_qsf_one_virtual_pin.qsf")
        with self.assertRaisesRegex(
            ShellPortError,
            "QSF contains VIRTUAL_PIN assignments to 'fit_signature_o'",
        ):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_top_mismatch_fires(self) -> None:
        model = self.parse_fixture("shell_fit_qsf_clean.qsf")
        with self.assertRaisesRegex(
            ShellPortError,
            "QSF top 'zhao_shell_fit_top' != expected 'wrong_top'",
        ):
            validate_qsf(
                model,
                expected_top="wrong_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_duplicate_source_fires(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text += (
            "set_global_assignment -name SYSTEMVERILOG_FILE "
            "../../../fpga/rtl/common/zhao_pkg.sv\n"
        )
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(ShellPortError, "QSF source pool has duplicates"):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_source_order_mismatch_fires_at_first_ordinal(self) -> None:
        model = self.parse_fixture("shell_fit_qsf_clean.qsf")
        reordered = (self.EXPECTED[1], self.EXPECTED[0], self.EXPECTED[2])
        with self.assertRaisesRegex(
            ShellPortError,
            "QSF source order diverges at ordinal 0",
        ):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=reordered,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_wildcard_assignment_fires(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text += "set_location_assignment PIN_A1 -to fit_*\n"
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(
            ShellPortError, "QSF contains wildcard assignment targets 'fit_\\*'"
        ):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_physical_pin_assignment_fires(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text += "set_location_assignment PIN_A1 -to fit_signature_o\n"
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(
            ShellPortError, "QSF contains physical pin assignments"
        ):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_sourced_hidden_assignment_is_rejected_as_active_tcl(self) -> None:
        path = FIXTURES / "shell_fit_qsf_sourced_hidden_virtual_pin.qsf"
        with self.assertRaisesRegex(
            ShellPortError,
            "unsupported active QSF command on line 1: 'source'",
        ):
            parse_qsf(
                path.read_text(encoding="utf-8"), repo=REPO, qsf_dir=path.parent
            )

    def test_trailing_semicolon_source_is_rejected(self) -> None:
        path = FIXTURES / "shell_fit_qsf_trailing_source.qsf"
        with self.assertRaisesRegex(ShellPortError, "unsupported Tcl command boundary"):
            parse_qsf(path.read_text(encoding="utf-8"), repo=REPO, qsf_dir=path.parent)

    def test_qip_source_assignment_is_rejected(self) -> None:
        path = FIXTURES / "shell_fit_qsf_qip_source.qsf"
        with self.assertRaisesRegex(ShellPortError, "unsupported source-bearing QSF assignment"):
            parse_qsf(path.read_text(encoding="utf-8"), repo=REPO, qsf_dir=path.parent)

    def test_other_source_bearing_assignment_is_rejected(self) -> None:
        text = (
            "set_global_assignment -name TOP_LEVEL_ENTITY zhao_shell_fit_top\n"
            "set_global_assignment -name VHDL_FILE hidden.vhd\n"
        )
        with self.assertRaisesRegex(ShellPortError, "unsupported source-bearing QSF assignment"):
            parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)

    def test_quoted_semicolon_is_data_not_a_command_boundary(self) -> None:
        model = parse_qsf(
            'set_global_assignment -name TOP_LEVEL_ENTITY "zhao;top"\n',
            repo=REPO,
            qsf_dir=FIXTURES,
        )
        self.assertEqual(model.top, "zhao;top")

    def test_other_active_tcl_is_rejected(self) -> None:
        with self.assertRaisesRegex(
            ShellPortError,
            "unsupported Tcl grouping on QSF line 1",
        ):
            parse_qsf(
                "if {1} {set_global_assignment -name VIRTUAL_PIN ON -to fit_*}\n",
                repo=REPO,
                qsf_dir=FIXTURES,
            )

    def test_hidden_qsf_source_fires_pool_parity(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text += "set_global_assignment -name SYSTEMVERILOG_FILE hidden_qsf_source.sv\n"
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(ShellPortError, "QSF source pool mismatch"):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_missing_qsf_sdc_fires(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text = "\n".join(line for line in text.splitlines() if "SDC_FILE" not in line)
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(ShellPortError, "QSF SDC pool mismatch"):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_extra_or_different_qsf_sdc_fires(self) -> None:
        base = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        for replacement in (
            "different_constraints.sdc",
            "shell_fit_constraints_clean.sdc\nset_global_assignment -name SDC_FILE extra.sdc",
        ):
            with self.subTest(replacement=replacement):
                text = base.replace("shell_fit_constraints_clean.sdc", replacement)
                model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
                with self.assertRaisesRegex(ShellPortError, "QSF SDC pool mismatch"):
                    validate_qsf(
                        model,
                        expected_top="zhao_shell_fit_top",
                        expected_sources=self.EXPECTED,
                        expected_constraints=self.EXPECTED_SDC,
                    )

    def test_duplicate_qsf_sdc_fires(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text += "set_global_assignment -name SDC_FILE shell_fit_constraints_clean.sdc\n"
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(ShellPortError, "QSF SDC pool has duplicates"):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_qsf_cli_accepts_clean_fixture(self) -> None:
        completed = subprocess.run(
            [
                sys.executable,
                str(TOOLS / "shell_fit_qsf.py"),
                "--repo-root",
                str(REPO),
                "--cmake",
                str(FIXTURES / "shell_fit_source_pool.cmake"),
                "--qsf",
                str(FIXTURES / "shell_fit_qsf_clean.qsf"),
                "--qpf",
                str(FIXTURES / "shell_fit_clean.qpf"),
                "--sdc",
                str(FIXTURES / "shell_fit_constraints_clean.sdc"),
                "--variable",
                "SHELL_FIT_FIXTURE_RTL",
            ],
            text=True, encoding="utf-8", errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout)
    def test_qpf_revision_and_version_are_exact(self) -> None:
        clean = (FIXTURES / "shell_fit_clean.qpf").read_text(encoding="utf-8")
        model = parse_qpf(clean)
        self.assertEqual(model.project_revision, "zhao_shell_fit")
        self.assertEqual(model.quartus_version, "17.0")
        for old, new, message in (
            ('PROJECT_REVISION = "zhao_shell_fit"', 'PROJECT_REVISION = "relaxed"',
             "PROJECT_REVISION"),
            ('QUARTUS_VERSION = "17.0"', 'QUARTUS_VERSION = "99.0"',
             "QPF QUARTUS_VERSION"),
        ):
            with self.subTest(field=message), self.assertRaisesRegex(ShellPortError, message):
                parse_qpf(clean.replace(old, new))

    def test_every_pinned_qsf_setting_mutation_fires(self) -> None:
        clean = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        model = self.parse_fixture("shell_fit_qsf_clean.qsf")
        assignments = dict(model.global_assignments)
        for name, value in assignments.items():
            if name not in reports_module.REQUIRED_QSF_SETTINGS:
                continue
            with self.subTest(name=name):
                mutated = clean.replace(
                    f"-name {name} \"{value}\"",
                    f"-name {name} \"wrong-{value}\"",
                )
                if mutated == clean:
                    mutated = clean.replace(
                        f"-name {name} {value}", f"-name {name} wrong-{value}"
                    )
                changed = parse_qsf(mutated, repo=REPO, qsf_dir=FIXTURES)
                with self.assertRaisesRegex(ShellPortError, f"effective setting {name}"):
                    validate_qsf(
                        changed,
                        expected_top="zhao_shell_fit_top",
                        expected_sources=self.EXPECTED,
                        expected_constraints=self.EXPECTED_SDC,
                    )

    def test_unbound_qsf_design_setting_fires(self) -> None:
        text = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        text += "set_global_assignment -name AUTO_RESOURCE_SHARING OFF\n"
        model = parse_qsf(text, repo=REPO, qsf_dir=FIXTURES)
        with self.assertRaisesRegex(ShellPortError, "unbound global assignments"):
            validate_qsf(
                model,
                expected_top="zhao_shell_fit_top",
                expected_sources=self.EXPECTED,
                expected_constraints=self.EXPECTED_SDC,
            )

    def test_literal_target_optimization_and_retiming_assignments_fire(self) -> None:
        clean = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        for name, value, target in (
            ("AUTO_RESOURCE_SHARING", "OFF", "u_shell"),
            ("PHYSICAL_SYNTHESIS_REGISTER_RETIMING", "ON", "u_shell|pipeline_q"),
        ):
            with self.subTest(name=name):
                model = parse_qsf(
                    clean
                    + f"set_instance_assignment -name {name} {value} -to {target}\n",
                    repo=REPO,
                    qsf_dir=FIXTURES,
                )
                with self.assertRaisesRegex(
                    ShellPortError, "unapproved instance assignments"
                ):
                    validate_qsf(
                        model,
                        expected_top="zhao_shell_fit_top",
                        expected_sources=self.EXPECTED,
                        expected_constraints=self.EXPECTED_SDC,
                    )

    def test_qsf_assignment_commands_require_exact_argument_shape(self) -> None:
        clean = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        controls = (
            (
                "global extra target",
                "set_global_assignment -name SEED 1 -to u_shell\n",
                "set_global_assignment.*exact shape",
            ),
            (
                "instance extra entity",
                "set_instance_assignment -name AUTO_RESOURCE_SHARING OFF -to u_shell -entity x\n",
                "set_instance_assignment.*exact shape",
            ),
            (
                "location missing target",
                "set_location_assignment PIN_A1 -to\n",
                "set_location_assignment.*exact shape",
            ),
        )
        for label, mutation, message in controls:
            with self.subTest(label=label), self.assertRaisesRegex(
                ShellPortError, message
            ):
                parse_qsf(clean + mutation, repo=REPO, qsf_dir=FIXTURES)

    def test_qsf_subprocess_rejects_wrong_device(self) -> None:
        clean = (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8")
        handle = tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", suffix=".qsf", dir=FIXTURES, delete=False
        )
        try:
            with handle:
                handle.write(clean.replace("DEVICE 5CSEBA6U23I7", "DEVICE 5CSEBA4U23C6"))
            completed = subprocess.run(
                [sys.executable, str(TOOLS / "shell_fit_qsf.py"),
                 "--repo-root", str(REPO),
                 "--cmake", str(FIXTURES / "shell_fit_source_pool.cmake"),
                 "--variable", "SHELL_FIT_FIXTURE_RTL",
                 "--qsf", handle.name,
                 "--qpf", str(FIXTURES / "shell_fit_clean.qpf"),
                 "--sdc", str(FIXTURES / "shell_fit_constraints_clean.sdc")],
                text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
            )
        finally:
            Path(handle.name).unlink(missing_ok=True)
        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertIn("effective setting DEVICE", completed.stdout)

    def test_qsf_subprocess_rejects_sourced_relaxed_constraint(self) -> None:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", suffix=".sdc", dir=FIXTURES, delete=False
        ) as child:
            child.write("set_false_path -from [get_clocks {gpu_clk}] -to [get_clocks {vid_clk}]\n")
            child_path = Path(child.name)
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", suffix=".sdc", dir=FIXTURES, delete=False
        ) as root:
            root.write((FIXTURES / "shell_fit_constraints_clean.sdc").read_text(encoding="utf-8"))
            root.write(f"source {child_path.name}\n")
            root_path = Path(root.name)
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", suffix=".qsf", dir=FIXTURES, delete=False
        ) as qsf:
            qsf.write(
                (FIXTURES / "shell_fit_qsf_clean.qsf").read_text(encoding="utf-8").replace(
                    "shell_fit_constraints_clean.sdc", root_path.name
                )
            )
            qsf_path = Path(qsf.name)
        try:
            completed = subprocess.run(
                [sys.executable, str(TOOLS / "shell_fit_qsf.py"),
                 "--repo-root", str(REPO),
                 "--cmake", str(FIXTURES / "shell_fit_source_pool.cmake"),
                 "--variable", "SHELL_FIT_FIXTURE_RTL",
                 "--qsf", str(qsf_path),
                 "--qpf", str(FIXTURES / "shell_fit_clean.qpf"),
                 "--sdc", str(root_path)],
                text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
            )
        finally:
            for path in (qsf_path, root_path, child_path):
                path.unlink(missing_ok=True)
        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertIn("forbidden or unsupported command 'set_false_path'", completed.stdout)


class ReportPreflightTests(unittest.TestCase):
    SOURCE_COMMIT = "0123456789abcdef0123456789abcdef01234567"

    def fixture(self, name: str) -> str:
        return (FIXTURES / name).read_text(encoding="utf-8")

    def make_mutated_runner(
        self, root: Path, replacements: tuple[tuple[str, str], ...]
    ) -> Path:
        runner = root / "tools" / "quartus" / "run_shell_fit.ps1"
        runner.parent.mkdir(parents=True, exist_ok=True)
        text = (TOOLS / "run_shell_fit.ps1").read_text(encoding="utf-8")
        for old, new in replacements:
            self.assertEqual(text.count(old), 1, f"runner seam must match once: {old}")
            text = text.replace(old, new, 1)
        runner.write_text(text, encoding="utf-8")
        return runner

    def make_probe_only_runner(self, root: Path) -> Path:
        return self.make_mutated_runner(
            root,
            ((
                "    Assert-PinnedPythonInstallation $requestedFull $actual",
                "    # Disposable positive-control copy: exercise post-pin probes.",
            ),),
        )

    def assert_runner_observation_rejected(
        self,
        replacements: tuple[tuple[str, str], ...],
        expected_message: str,
    ) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runner = self.make_mutated_runner(root, replacements)
            completed = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(runner),
                    "-PreflightOnly",
                    "-PythonExe",
                    sys.executable,
                ],
                text=True, encoding="utf-8", errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            reports_exist = (root / "reports").exists()
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn(expected_message, completed.stdout)
        self.assertNotIn("PASS shell_fit_top_clean_characterization", completed.stdout)
        self.assertFalse(reports_exist)

    def reports_argv(self, repo: Path) -> list[str]:
        def at(path: str) -> str:
            return str(repo / path)

        return [
            "--repo-root",
            str(repo),
            "--summary",
            at("tests/tools/fixtures/shell_fit_summary_zero_virtual_pins.txt"),
            "--sta",
            at("reports/characterization/zhao_shell_fit.sta.rpt"),
            "--clocks",
            at("reports/characterization/clocks.rpt"),
            "--hierarchy",
            at("tests/tools/fixtures/shell_fit_hierarchy_with_shell.rpt"),
            "--map-summary",
            at("tests/tools/fixtures/shell_fit_map_clean.summary"),
            "--map-report",
            at("tests/tools/fixtures/shell_fit_map_clean.rpt"),
            "--receipt",
            at("tests/tools/fixtures/shell_fit_receipt_v3.json"),
            "--manifest",
            at("fpga/rtl/generated/zhao_shell_fit_top.manifest.json"),
            "--rtl",
            at("fpga/rtl/generated/zhao_shell_fit_top.sv"),
            "--shell",
            at("fpga/rtl/common/zhao_shell_top.sv"),
            "--package",
            at("fpga/rtl/common/zhao_pkg.sv"),
            "--policy",
            at("design/shell_fit_ports.yml"),
            "--generator",
            at("tools/quartus/gen_shell_fit_top.py"),
            "--parser",
            at("tools/quartus/shell_ports.py"),
            "--packet",
            at("tests/tools/fixtures/shell_fit_frame_blit.bin"),
            "--cmake",
            at("tests/tools/fixtures/shell_fit_source_pool.cmake"),
            "--cmake-variable",
            "SHELL_FIT_FIXTURE_RTL",
            "--qsf",
            at("tests/tools/fixtures/shell_fit_qsf_clean.qsf"),
            "--sdc",
            at("tests/tools/fixtures/shell_fit_constraints_clean.sdc"),
            "--qsf-parser",
            at("tools/quartus/shell_fit_qsf.py"),
            "--evidence-parser",
            at("tools/quartus/shell_fit_reports.py"),
            "--git-capture",
            at("tools/quartus/capture_shell_fit_git.py"),
            "--runner",
            at("tools/quartus/run_shell_fit.ps1"),
            "--report-script",
            at("fpga/quartus/shell_fit/report.tcl"),
            "--post-map-script",
            at("fpga/quartus/shell_fit/post_map_connectivity.tcl"),
            "--project",
            at("fpga/quartus/shell_fit/zhao_shell_fit.qpf"),
            "--timing-metrics",
            at("tests/tools/fixtures/shell_fit_timing_metrics_clean.tsv"),
            "--clock-transfers",
            at("reports/characterization/clock_transfers.rpt"),
            "--unconstrained-paths",
            at("tests/tools/fixtures/shell_fit_unconstrained_clean.rpt"),
            "--setup-paths",
            at("tests/tools/fixtures/shell_fit_setup_paths_clean.rpt"),
            "--hold-paths",
            at("tests/tools/fixtures/shell_fit_hold_paths_clean.rpt"),
            "--recovery-paths",
            at("tests/tools/fixtures/shell_fit_recovery_paths_clean.rpt"),
            "--removal-paths",
            at("tests/tools/fixtures/shell_fit_removal_paths_clean.rpt"),
            "--post-map-connectivity",
            at("tests/tools/fixtures/shell_fit_post_map_connectivity.tsv"),
            "--map-stdout",
            at("tests/tools/fixtures/shell_fit_map.stdout.log"),
            "--map-stderr",
            at("tests/tools/fixtures/shell_fit_map.stderr.log"),
            "--post-map-stdout",
            at("tests/tools/fixtures/shell_fit_post_map.stdout.log"),
            "--post-map-stderr",
            at("tests/tools/fixtures/shell_fit_post_map.stderr.log"),
            "--fit-stdout",
            at("tests/tools/fixtures/shell_fit_fit.stdout.log"),
            "--fit-stderr",
            at("tests/tools/fixtures/shell_fit_fit.stderr.log"),
            "--timequest-stdout",
            at("tests/tools/fixtures/shell_fit_timequest.stdout.log"),
            "--timequest-stderr",
            at("tests/tools/fixtures/shell_fit_timequest.stderr.log"),
            "--processors",
            "4",
            "--git-head",
            at("tests/tools/fixtures/shell_fit_git_head.txt"),
            "--git-status",
            at("tests/tools/fixtures/shell_fit_git_status.txt"),
            "--git-worktree-diff",
            at("tests/tools/fixtures/shell_fit_git_worktree_diff.txt"),
            "--git-staged-diff",
            at("tests/tools/fixtures/shell_fit_git_staged_diff.txt"),
            "--git-index-flags",
            at("tests/tools/fixtures/shell_fit_git_index_flags.txt"),
        ]

    def make_clean_cli_repository(
        self, repo: Path, *, add_unrelated_tracked_file: bool = False
    ) -> str:
        """Copy fixtures into a real clean repo and seal dynamic HEAD evidence."""
        evidence = self.evidence()
        relative_paths = (
            set(evidence["source_paths"].values())
            | set(evidence["evidence_paths"].values())
            | set(evidence["compile_source_pool"])
            | {
                "fpga/rtl/generated/zhao_shell_fit_top.manifest.json",
                "tests/tools/fixtures/shell_fit_receipt_v3.json",
            }
        )
        for relative in relative_paths:
            destination = repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / relative, destination)

        dynamic_paths = {
            "tests/tools/fixtures/shell_fit_git_head.txt",
            "tests/tools/fixtures/shell_fit_receipt_v3.json",
        }
        if add_unrelated_tracked_file:
            (repo / "unrelated-tracked.txt").write_text(
                "committed unrelated content\n", encoding="utf-8"
            )
        for arguments in (
            ("init",),
            ("config", "user.name", "Shell Fit Test"),
            ("config", "user.email", "shell-fit-test@example.invalid"),
            ("config", "core.autocrlf", "false"),
            (
                "add",
                "--",
                *sorted(relative_paths - dynamic_paths),
                *(("unrelated-tracked.txt",) if add_unrelated_tracked_file else ()),
            ),
            ("commit", "-m", "clean shell-fit fixture"),
        ):
            completed = subprocess.run(
                ["git", "-C", str(repo), *arguments],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
        source_commit = subprocess.run(
            ["git", "-C", str(repo), "rev-parse", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", errors="replace",
            check=True,
        ).stdout.strip()

        git_head_path = repo / "tests/tools/fixtures/shell_fit_git_head.txt"
        git_head_bytes = f"{source_commit}\n".encode("ascii")
        git_head_path.write_bytes(git_head_bytes)
        receipt_path = repo / "tests/tools/fixtures/shell_fit_receipt_v3.json"
        receipt = json.loads(receipt_path.read_bytes())
        receipt["sourceCommit"] = source_commit
        receipt["evidenceArtifacts"]["gitHead"]["sha256"] = hashlib.sha256(
            git_head_bytes
        ).hexdigest()
        receipt_path.write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        return source_commit

    def commit_fixture_change(self, repo: Path, relative: str, data: bytes) -> str:
        path = repo / relative
        path.write_bytes(data)
        for arguments in (("add", "--", relative), ("commit", "-m", "mutate evidence fixture")):
            completed = subprocess.run(
                ["git", "-C", str(repo), *arguments],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
        commit = subprocess.run(
            ["git", "-C", str(repo), "rev-parse", "HEAD"],
            check=True,
            stdout=subprocess.PIPE,
            text=True, encoding="utf-8", errors="replace",
        ).stdout.strip()
        (repo / "tests/tools/fixtures/shell_fit_git_head.txt").write_bytes(
            (commit + "\n").encode("ascii")
        )
        return commit

    def make_runner_repository(self, repo: Path) -> str:
        fixed = {
            "fpga/quartus/shell_fit/zhao_shell_fit.qpf",
            "fpga/quartus/shell_fit/zhao_shell_fit.qsf",
            "fpga/quartus/shell_fit/zhao_shell_fit.sdc",
            "fpga/quartus/shell_fit/report.tcl",
            "fpga/quartus/shell_fit/post_map_connectivity.tcl",
            "fpga/rtl/generated/zhao_abi_pkg.sv",
            "fpga/rtl/generated/zhao_shell_fit_top.sv",
            "fpga/rtl/generated/zhao_shell_fit_top.manifest.json",
            "tools/quartus/run_shell_fit.ps1",
            "tools/quartus/capture_shell_fit_git.py",
            "tools/quartus/shell_fit_qsf.py",
            "tools/quartus/shell_fit_reports.py",
            "tools/quartus/shell_ports.py",
            "tools/quartus/gen_shell_fit_top.py",
            "tests/CMakeLists.txt",
            "design/shell_fit_ports.yml",
            "tests/tools/fixtures/shell_fit_frame_blit.bin",
        }
        fixed.update(
            parse_cmake_source_pool(
                (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8"),
                variable="ZHAO_SHELL_RTL",
                repo=REPO,
                cmake_dir=REPO / "tests",
            )
        )
        for relative in fixed:
            destination = repo / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPO / relative, destination)

        fake_fixture_names = (
            "shell_fit_summary_zero_virtual_pins.txt",
            "shell_fit_hierarchy_with_shell.rpt",
            "shell_fit_map_clean.summary",
            "shell_fit_map_clean.rpt",
            "shell_fit_post_map_connectivity.tsv",
            "shell_fit_timing_metrics_clean.tsv",
            "shell_fit_unconstrained_clean.rpt",
            "shell_fit_setup_paths_clean.rpt",
            "shell_fit_hold_paths_clean.rpt",
            "shell_fit_recovery_paths_clean.rpt",
            "shell_fit_removal_paths_clean.rpt",
        )
        for name in fake_fixture_names:
            destination = repo / "tests/tools/fixtures" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(FIXTURES / name, destination)
        for source, name in (
            (ARCHIVED_STA, "shell_fit_sta_clean.rpt"),
            (ARCHIVED_CLOCKS, "shell_fit_clocks_clean.rpt"),
            (ARCHIVED_CLOCK_TRANSFERS, "shell_fit_clock_transfers_clean.rpt"),
        ):
            shutil.copyfile(source, repo / "tests/tools/fixtures" / name)

        fake_stage = repo / "fpga/quartus/shell_fit/zhao_shell_fit"
        fake_stage.write_text(
            "from pathlib import Path\n"
            "import os, shutil, sys, time\n"
            "cwd = Path.cwd()\n"
            "repo = cwd.parents[2]\n"
            "fixtures = repo / 'tests/tools/fixtures'\n"
            "out = cwd / 'output_files'\n"
            "char = out / 'characterization'\n"
            "out.mkdir(parents=True, exist_ok=True)\n"
            "char.mkdir(parents=True, exist_ok=True)\n"
            "exe = Path(sys.executable).name.lower()\n"
            "if 'quartus_map' in exe:\n"
            "    counter = os.environ.get('ZHAO_TEST_MAP_COUNT')\n"
            "    if counter:\n"
            "        with open(counter, 'a', encoding='ascii') as stream: stream.write('map\\n')\n"
            "    marker = os.environ.get('ZHAO_TEST_MAP_STARTED')\n"
            "    if marker:\n"
            "        Path(marker).write_text('started', encoding='ascii')\n"
            "        release = Path(os.environ['ZHAO_TEST_MAP_CONTINUE'])\n"
            "        for _ in range(600):\n"
            "            if release.exists(): break\n"
            "            time.sleep(0.05)\n"
            "        else: raise SystemExit('timed out waiting for test release')\n"
            "    shutil.copyfile(fixtures/'shell_fit_map_clean.summary', out/'zhao_shell_fit.map.summary')\n"
            "    shutil.copyfile(fixtures/'shell_fit_map_clean.rpt', out/'zhao_shell_fit.map.rpt')\n"
            "    print('Info: Quartus Prime Analysis & Synthesis was successful. 0 errors, 7 warnings')\n"
            "elif 'quartus_fit' in exe:\n"
            "    shutil.copyfile(fixtures/'shell_fit_summary_zero_virtual_pins.txt', out/'zhao_shell_fit.fit.summary')\n"
            "    shutil.copyfile(fixtures/'shell_fit_hierarchy_with_shell.rpt', out/'zhao_shell_fit.fit.rpt')\n"
            "    print('Info: Quartus Prime Fitter was successful. 0 errors, 9 warnings')\n"
            "elif '--post_map' in sys.argv:\n"
            "    shutil.copyfile(fixtures/'shell_fit_post_map_connectivity.tsv', char/'post_map_connectivity.tsv')\n"
            "    print('Info: Wrote post-map shell boundary witness to output_files/characterization/post_map_connectivity.tsv')\n"
            "    print('Info: Quartus Prime TimeQuest Timing Analyzer was successful. 0 errors, 1 warning')\n"
            "else:\n"
            "    copies = {\n"
            "      'shell_fit_sta_clean.rpt':'zhao_shell_fit.sta.rpt',\n"
            "      'shell_fit_clocks_clean.rpt':'characterization/clocks.rpt',\n"
            "      'shell_fit_clock_transfers_clean.rpt':'characterization/clock_transfers.rpt',\n"
            "      'shell_fit_timing_metrics_clean.tsv':'characterization/timing_metrics.tsv',\n"
            "      'shell_fit_unconstrained_clean.rpt':'characterization/unconstrained_paths.rpt',\n"
            "      'shell_fit_setup_paths_clean.rpt':'characterization/setup_paths.rpt',\n"
            "      'shell_fit_hold_paths_clean.rpt':'characterization/hold_paths.rpt',\n"
            "      'shell_fit_recovery_paths_clean.rpt':'characterization/recovery_paths.rpt',\n"
            "      'shell_fit_removal_paths_clean.rpt':'characterization/removal_paths.rpt'}\n"
            "    for src, dst in copies.items():\n"
            "        target = out/dst; target.parent.mkdir(parents=True, exist_ok=True)\n"
            "        shutil.copyfile(fixtures/src, target)\n"
            "    if os.environ.get('ZHAO_TEST_FAIL_TIMING'):\n"
            "        metrics = char/'timing_metrics.tsv'\n"
            "        metrics.write_text(metrics.read_text().replace('analysis\\tsetup\\t0.321\\t0', 'analysis\\tsetup\\t-0.100\\t1'))\n"
            "        setup = char/'setup_paths.rpt'\n"
            "        setup.write_text(setup.read_text().replace('Found 200 setup paths (0 violated). Worst case slack is 0.321', 'Found 200 setup paths (1 violated). Worst case slack is -0.100'))\n"
            "    print('Info: Quartus Prime TimeQuest Timing Analyzer was successful. 0 errors, 3 warnings')\n",
            encoding="utf-8",
        )
        (repo / "unrelated.txt").write_text("initial\n", encoding="utf-8")
        for arguments in (
            ("init",),
            ("config", "user.name", "Shell Fit Runner Test"),
            ("config", "user.email", "shell-fit-runner@example.invalid"),
            ("config", "core.autocrlf", "false"),
            ("add", "--", "."),
            ("commit", "-m", "clean runner fixture"),
        ):
            completed = subprocess.run(
                ["git", "-C", str(repo), *arguments],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
        return subprocess.run(
            ["git", "-C", str(repo), "rev-parse", "HEAD"],
            check=True,
            stdout=subprocess.PIPE,
            text=True, encoding="utf-8", errors="replace",
        ).stdout.strip()

    def make_fake_quartus_bin(self, root: Path) -> Path:
        binary = root / "fake-quartus"
        binary.mkdir()
        for name in ("quartus_map.exe", "quartus_fit.exe", "quartus_sta.exe"):
            shutil.copyfile(sys.executable, binary / name)
        return binary

    def run_reports_emit_subprocess(self, repo: Path):
        emitted = repo / "reports/synthesis/subprocess-shell-fit.json"
        argv = self.reports_argv(repo)
        receipt_at = argv.index("--receipt")
        argv[receipt_at] = "--emit-receipt"
        argv[receipt_at + 1] = str(emitted)
        completed = subprocess.run(
            [sys.executable, str(repo / "tools/quartus/shell_fit_reports.py"), *argv],
            text=True, encoding="utf-8", errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        return completed, emitted

    def assert_hidden_tracked_edit_is_rejected(
        self, *, index_option: str, expected_tag: bytes
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            self.make_clean_cli_repository(
                temporary_repo, add_unrelated_tracked_file=True
            )
            clean_output = io.StringIO()
            with redirect_stdout(clean_output), redirect_stderr(clean_output):
                clean_result = reports_main(self.reports_argv(temporary_repo))
            self.assertEqual(clean_result, 0, clean_output.getvalue())

            completed = subprocess.run(
                [
                    "git",
                    "-C",
                    str(temporary_repo),
                    "update-index",
                    index_option,
                    "--",
                    "unrelated-tracked.txt",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            (temporary_repo / "unrelated-tracked.txt").write_text(
                "dirty content hidden from status and diff\n", encoding="utf-8"
            )

            for arguments in (
                ("status", "--short", "--untracked-files=no"),
                ("diff", "--no-ext-diff", "--no-textconv", "--binary", "--"),
                (
                    "diff",
                    "--cached",
                    "--no-ext-diff",
                    "--no-textconv",
                    "--binary",
                    "--",
                ),
            ):
                hidden = subprocess.run(
                    ["git", "-C", str(temporary_repo), *arguments],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=True,
                ).stdout
                self.assertEqual(hidden, b"")

            concealment = read_git_index_concealment(temporary_repo)
            self.assertEqual(
                concealment, expected_tag + b" unrelated-tracked.txt\x00"
            )
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                result = reports_main(self.reports_argv(temporary_repo))
        self.assertEqual(result, 1, output.getvalue())
        self.assertIn(
            "captured git evidence differs from direct repository state",
            output.getvalue(),
        )
        self.assertIn("gitIndexFlags", output.getvalue())

    def assert_injected_index_listing_result(
        self, raw: bytes, *, expected_result: int, expected_message: str
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            self.make_clean_cli_repository(
                temporary_repo, add_unrelated_tracked_file=True
            )
            original_run_git = reports_module._run_git

            def injected_run_git(repo: Path, *arguments: str) -> bytes:
                if arguments == (
                    "--no-replace-objects",
                    "ls-files",
                    "--cached",
                    "-v",
                    "-z",
                ):
                    return raw
                return original_run_git(repo, *arguments)

            output = io.StringIO()
            with mock.patch.object(
                reports_module, "_run_git", side_effect=injected_run_git
            ), redirect_stdout(output), redirect_stderr(output):
                result = reports_main(self.reports_argv(temporary_repo))
        self.assertEqual(result, expected_result, output.getvalue())
        self.assertIn(expected_message, output.getvalue())

    def evidence(self):
        summary = parse_fit_summary(self.fixture("shell_fit_summary_zero_virtual_pins.txt"))
        hierarchy_rows = parse_fitter_hierarchy(
            self.fixture("shell_fit_hierarchy_with_shell.rpt")
        )
        receipt = json.loads(self.fixture("shell_fit_receipt_v3.json"))
        manifest_path = REPO / "fpga/rtl/generated/zhao_shell_fit_top.manifest.json"
        rtl_path = REPO / "fpga/rtl/generated/zhao_shell_fit_top.sv"
        manifest_bytes = manifest_path.read_bytes()
        source_path_objects = {
            "shell": REPO / "fpga/rtl/common/zhao_shell_top.sv",
            "package": REPO / "fpga/rtl/common/zhao_pkg.sv",
            "policy": REPO / "design/shell_fit_ports.yml",
            "generator": TOOLS / "gen_shell_fit_top.py",
            "parser": TOOLS / "shell_ports.py",
            "packet": FIXTURES / "shell_fit_frame_blit.bin",
            "cmake": FIXTURES / "shell_fit_source_pool.cmake",
            "qsf": FIXTURES / "shell_fit_qsf_clean.qsf",
            "sdc": FIXTURES / "shell_fit_constraints_clean.sdc",
            "qsfParser": TOOLS / "shell_fit_qsf.py",
            "evidenceParser": TOOLS / "shell_fit_reports.py",
            "gitCapture": TOOLS / "capture_shell_fit_git.py",
            "runner": TOOLS / "run_shell_fit.ps1",
            "reportScript": REPO / "fpga/quartus/shell_fit/report.tcl",
            "postMapScript": REPO / "fpga/quartus/shell_fit/post_map_connectivity.tcl",
            "project": REPO / "fpga/quartus/shell_fit/zhao_shell_fit.qpf",
        }
        evidence_path_objects = {
            "summary": FIXTURES / "shell_fit_summary_zero_virtual_pins.txt",
            "sta": ARCHIVED_STA,
            "clocks": ARCHIVED_CLOCKS,
            "hierarchy": FIXTURES / "shell_fit_hierarchy_with_shell.rpt",
            "mapSummary": FIXTURES / "shell_fit_map_clean.summary",
            "mapReport": FIXTURES / "shell_fit_map_clean.rpt",
            "timingMetrics": FIXTURES / "shell_fit_timing_metrics_clean.tsv",
            "clockTransfers": ARCHIVED_CLOCK_TRANSFERS,
            "unconstrainedPaths": FIXTURES / "shell_fit_unconstrained_clean.rpt",
            "setupPaths": FIXTURES / "shell_fit_setup_paths_clean.rpt",
            "holdPaths": FIXTURES / "shell_fit_hold_paths_clean.rpt",
            "recoveryPaths": FIXTURES / "shell_fit_recovery_paths_clean.rpt",
            "removalPaths": FIXTURES / "shell_fit_removal_paths_clean.rpt",
            "postMapConnectivity": FIXTURES / "shell_fit_post_map_connectivity.tsv",
            "mapStdout": FIXTURES / "shell_fit_map.stdout.log",
            "mapStderr": FIXTURES / "shell_fit_map.stderr.log",
            "postMapStdout": FIXTURES / "shell_fit_post_map.stdout.log",
            "postMapStderr": FIXTURES / "shell_fit_post_map.stderr.log",
            "fitStdout": FIXTURES / "shell_fit_fit.stdout.log",
            "fitStderr": FIXTURES / "shell_fit_fit.stderr.log",
            "timequestStdout": FIXTURES / "shell_fit_timequest.stdout.log",
            "timequestStderr": FIXTURES / "shell_fit_timequest.stderr.log",
            "gitHead": FIXTURES / "shell_fit_git_head.txt",
            "gitStatus": FIXTURES / "shell_fit_git_status.txt",
            "gitWorktreeDiff": FIXTURES / "shell_fit_git_worktree_diff.txt",
            "gitStagedDiff": FIXTURES / "shell_fit_git_staged_diff.txt",
            "gitIndexFlags": FIXTURES / "shell_fit_git_index_flags.txt",
        }
        source_bytes = {name: path.read_bytes() for name, path in source_path_objects.items()}
        evidence_bytes = {
            name: path.read_bytes() for name, path in evidence_path_objects.items()
        }
        map_report_text = evidence_bytes["mapReport"].decode("utf-8")
        source_paths = {
            name: path.relative_to(REPO).as_posix()
            for name, path in source_path_objects.items()
        }
        rtl_bytes = rtl_path.read_bytes()
        compile_source_pool = (
            "fpga/rtl/common/zhao_pkg.sv",
            "fpga/rtl/generated/zhao_abi_pkg.sv",
            "fpga/rtl/generated/zhao_shell_fit_top.sv",
        )
        compile_source_bytes = {
            path: (REPO / path).read_bytes() for path in compile_source_pool
        }
        compile_source_git_blob_bytes = dict(compile_source_bytes)
        qpf_model = parse_qpf(source_bytes["project"].decode("utf-8"))
        qsf_model = parse_qsf(
            source_bytes["qsf"].decode("utf-8"), repo=REPO,
            qsf_dir=source_path_objects["qsf"].parent,
        )
        sdc_closure = read_sdc_closure(source_path_objects["sdc"], repo=REPO)
        post_map_witness = parse_post_map_connectivity(
            evidence_bytes["postMapConnectivity"].decode("utf-8")
        )
        validate_post_map_connectivity(post_map_witness, map_report_text)
        stage_logs = {
            name: evidence_bytes[name].decode("utf-8")
            for name in (
                "mapStdout", "mapStderr", "postMapStdout", "postMapStderr",
                "fitStdout", "fitStderr", "timequestStdout", "timequestStderr",
            )
        }
        timing = timing_evidence(
            metrics_text=evidence_bytes["timingMetrics"].decode("utf-8"),
            path_reports={
                name: evidence_bytes[f"{name}Paths"].decode("utf-8")
                for name in ("setup", "hold", "recovery", "removal")
            },
            unconstrained_text=evidence_bytes["unconstrainedPaths"].decode("utf-8"),
            transfers_text=evidence_bytes["clockTransfers"].decode("utf-8"),
            critical_warnings=collect_critical_warnings(stage_logs),
        )
        git_blob_bytes = {
            **source_bytes,
            "generatedRtl": rtl_bytes,
            "manifest": manifest_bytes,
            **{f"sdcClosure:{entry.path}": entry.data for entry in sdc_closure},
        }
        return {
            "summary": summary,
            "map_summary": parse_map_summary(
                evidence_bytes["mapSummary"].decode("utf-8"),
                map_report_text=map_report_text,
            ),
            "map_hierarchy_rows": parse_map_hierarchy(map_report_text),
            "timequest_status": parse_timequest_status(
                evidence_bytes["sta"].decode("utf-8")
            ),
            "git_evidence": parse_git_evidence(
                head=evidence_bytes["gitHead"],
                status=evidence_bytes["gitStatus"],
                worktree_diff=evidence_bytes["gitWorktreeDiff"],
                staged_diff=evidence_bytes["gitStagedDiff"],
                index_flags=evidence_bytes["gitIndexFlags"],
            ),
            "git_blob_bytes": git_blob_bytes,
            "compile_source_pool": compile_source_pool,
            "compile_source_bytes": compile_source_bytes,
            "compile_source_git_blob_bytes": compile_source_git_blob_bytes,
            "selected_sdc_path": source_paths["sdc"],
            "qpf_model": qpf_model,
            "qsf_model": qsf_model,
            "sdc_closure": sdc_closure,
            "post_map_witness": post_map_witness,
            "timing": timing,
            "processors": 4,
            "hierarchy_rows": hierarchy_rows,
            "receipt": receipt,
            "manifest_bytes": manifest_bytes,
            "manifest": json.loads(manifest_bytes),
            "rtl_bytes": rtl_bytes,
            "source_bytes": source_bytes,
            "source_paths": source_paths,
            "evidence_bytes": evidence_bytes,
            "evidence_paths": {
                name: path.relative_to(REPO).as_posix()
                for name, path in evidence_path_objects.items()
            },
        }

    def build(self, evidence=None):
        evidence = evidence or self.evidence()
        return build_receipt_from_evidence(
            manifest=evidence["manifest"],
            manifest_bytes=evidence["manifest_bytes"],
            rtl_bytes=evidence["rtl_bytes"],
            summary=evidence["summary"],
            map_summary=evidence["map_summary"],
            map_hierarchy_rows=evidence["map_hierarchy_rows"],
            timequest_status=evidence["timequest_status"],
            hierarchy_rows=evidence["hierarchy_rows"],
            git_evidence=evidence["git_evidence"],
            compile_source_pool=evidence["compile_source_pool"],
            selected_sdc_path=evidence["selected_sdc_path"],
            source_bytes=evidence["source_bytes"],
            source_paths=evidence["source_paths"],
            evidence_bytes=evidence["evidence_bytes"],
            evidence_paths=evidence["evidence_paths"],
            qpf_model=evidence["qpf_model"],
            qsf_model=evidence["qsf_model"],
            sdc_closure=evidence["sdc_closure"],
            post_map_witness=evidence["post_map_witness"],
            timing=evidence["timing"],
            processors=evidence["processors"],
        )

    def bind(self, receipt, evidence=None) -> None:
        evidence = evidence or self.evidence()
        bind_receipt_to_evidence(
            receipt,
            manifest=evidence["manifest"],
            manifest_bytes=evidence["manifest_bytes"],
            rtl_bytes=evidence["rtl_bytes"],
            summary=evidence["summary"],
            map_summary=evidence["map_summary"],
            map_hierarchy_rows=evidence["map_hierarchy_rows"],
            timequest_status=evidence["timequest_status"],
            hierarchy_rows=evidence["hierarchy_rows"],
            git_evidence=evidence["git_evidence"],
            git_blob_bytes=evidence["git_blob_bytes"],
            compile_source_pool=evidence["compile_source_pool"],
            compile_source_bytes=evidence["compile_source_bytes"],
            compile_source_git_blob_bytes=evidence["compile_source_git_blob_bytes"],
            selected_sdc_path=evidence["selected_sdc_path"],
            source_bytes=evidence["source_bytes"],
            source_paths=evidence["source_paths"],
            evidence_bytes=evidence["evidence_bytes"],
            evidence_paths=evidence["evidence_paths"],
            qpf_model=evidence["qpf_model"],
            qsf_model=evidence["qsf_model"],
            sdc_closure=evidence["sdc_closure"],
            post_map_witness=evidence["post_map_witness"],
            timing=evidence["timing"],
            processors=evidence["processors"],
        )

    def test_zero_virtual_pin_summary(self) -> None:
        summary = parse_fit_summary(self.fixture("shell_fit_summary_zero_virtual_pins.txt"))
        self.assertEqual(summary.alms, 9876)
        self.assertEqual(summary.real_pins, 10)
        self.assertEqual(summary.virtual_pins, 0)
        self.assertEqual(
            summary.tool_version,
            "17.0.2 Build 602 07/19/2017 SJ Lite Edition",
        )
        self.assertEqual(summary.ram_blocks, Decimal("132"))
        validate_fit_summary(summary)

    def test_one_virtual_pin_summary_fires(self) -> None:
        summary = parse_fit_summary(self.fixture("shell_fit_summary_one_virtual_pin.txt"))
        with self.assertRaisesRegex(
            ShellPortError, "Total virtual pins is 1, expected 0"
        ):
            validate_fit_summary(summary)

    def test_missing_real_quartus_version_field_fires(self) -> None:
        text = self.fixture("shell_fit_summary_zero_virtual_pins.txt").replace(
            "; Quartus Prime Version            ; 17.0.2 Build 602 07/19/2017 SJ Lite Edition ;\n",
            "",
        )
        with self.assertRaisesRegex(ShellPortError, "expected one 'Quartus Prime Version' field, found 0"):
            parse_fit_summary(text)

    def test_ambiguous_real_ram_block_field_fires(self) -> None:
        text = self.fixture("shell_fit_summary_zero_virtual_pins.txt")
        text += "; Total RAM Blocks ; 133 ;\n"
        with self.assertRaisesRegex(ShellPortError, "expected one 'Total RAM Blocks' field, found 2"):
            parse_fit_summary(text)

    def test_genuine_raw_sta_messages_clean_fixture(self) -> None:
        validate_report_messages(ARCHIVED_STA.read_text(encoding="utf-8"))

    def test_empty_messages_artifact_fires(self) -> None:
        with self.assertRaisesRegex(ShellPortError, "messages artifact is empty"):
            validate_report_messages("")

    def test_prose_only_messages_artifact_fires(self) -> None:
        with self.assertRaisesRegex(ShellPortError, "not recognizable raw Quartus output"):
            validate_report_messages("Everything looked clean to the operator.\n")

    def test_synthetic_one_line_info_claim_fires(self) -> None:
        with self.assertRaisesRegex(ShellPortError, "not recognizable raw Quartus output"):
            validate_report_messages("Info: Quartus completed cleanly with no virtual clocks.\n")

    def test_genuine_wrapped_virtual_clock_warning_fires(self) -> None:
        with self.assertRaisesRegex(ShellPortError, "forbidden virtual-clock warning"):
            scan_virtual_clock_warnings(
                {"archivedRunLog": ARCHIVED_WRAPPED_WARNING.read_text(encoding="utf-8")}
            )

    def test_warning_in_another_bound_artifact_fires(self) -> None:
        wrapped = (
            'Critical Warning (15725): clock port is fed by virtual pin "gpu_clk~input"; '
            "timing analysis treats input to the clock port as a\n"
            "ripple clock\n"
        )
        with self.assertRaisesRegex(ShellPortError, "artifact 'mapReport'.*forbidden"):
            scan_virtual_clock_warnings(
                {
                    "sta": ARCHIVED_STA.read_text(encoding="utf-8"),
                    "mapReport": self.fixture("shell_fit_map_clean.rpt") + wrapped,
                }
            )

    def test_genuine_report_clocks_are_exact(self) -> None:
        clocks = parse_clock_constraints(ARCHIVED_CLOCKS.read_text(encoding="utf-8"))
        self.assertEqual(
            [(clock.name, clock.clock_type, clock.period_ns, clock.targets) for clock in clocks],
            [
                ("audio_clk", "Base", Decimal("40.000"), ("audio_clk",)),
                ("gpu_clk", "Base", Decimal("10.000"), ("gpu_clk",)),
                ("vid_clk", "Base", Decimal("20.000"), ("vid_clk",)),
            ],
        )
        validate_clock_constraints(clocks)

    def test_genuine_sta_success_is_separate_from_report_clocks(self) -> None:
        self.assertEqual(
            parse_timequest_status(ARCHIVED_STA.read_text(encoding="utf-8")),
            "successful",
        )
        with self.assertRaisesRegex(ShellPortError, "raw STA report identity is absent"):
            parse_timequest_status(ARCHIVED_CLOCKS.read_text(encoding="utf-8"))

    def test_old_synthetic_clock_schema_is_rejected(self) -> None:
        with self.assertRaisesRegex(ShellPortError, "genuine TimeQuest report_clocks table is absent"):
            parse_clock_constraints(self.fixture("shell_fit_clocks_ignored_prose.rpt"))

    def test_wrong_clock_period_fires(self) -> None:
        text = ARCHIVED_CLOCKS.read_text(encoding="utf-8").replace(
            "; vid_clk    ; Base ; 20.000 ;", "; vid_clk    ; Base ; 19.999 ;"
        )
        with self.assertRaisesRegex(
            ShellPortError,
            "clock 'vid_clk' period is 19.999 ns, expected 20.000 ns",
        ):
            validate_clock_constraints(parse_clock_constraints(text))

    def test_missing_clock_row_fires_exact_set_check(self) -> None:
        text = ARCHIVED_CLOCKS.read_text(encoding="utf-8")
        text = "\n".join(line for line in text.splitlines() if "audio_clk  ; Base" not in line)
        with self.assertRaisesRegex(
            ShellPortError,
            r"constrained clock set mismatch: missing=\['audio_clk'\], extra=\[\]",
        ):
            validate_clock_constraints(parse_clock_constraints(text))

    def test_fractional_u_shell_hierarchy_is_preserved(self) -> None:
        rows = parse_fitter_hierarchy(self.fixture("shell_fit_hierarchy_with_shell.rpt"))
        shell = require_shell_hierarchy(rows)
        self.assertEqual(shell.alms_needed, Decimal("7421.6"))
        self.assertEqual(shell.alms_needed_self, Decimal("7421.6"))
        self.assertEqual(shell.final_placement_alms, Decimal("7650.2"))
        self.assertEqual(shell.final_placement_alms_self, Decimal("7650.2"))
        self.assertEqual(shell.dense_recoverable_alms, Decimal("241.7"))
        self.assertEqual(shell.unavailable_alms, Decimal("13.1"))
        self.assertEqual(shell.memory_alms, Decimal("0.0"))
        self.assertEqual(shell.combinational_aluts, 8999)
        self.assertEqual(shell.combinational_aluts_self, 8999)
        self.assertEqual(shell.registers, 14321)
        self.assertEqual(shell.registers_self, 14321)
        self.assertEqual(shell.io_registers, 0)
        self.assertEqual(shell.memory_bits, 2097152)
        self.assertEqual(shell.ram_blocks, Decimal("128"))
        self.assertEqual(shell.dsp_blocks, Decimal("84"))
        self.assertEqual(shell.library_name, "work")

    def test_hierarchy_parsers_retain_every_well_formed_row(self) -> None:
        fitter = parse_fitter_hierarchy(
            self.fixture("shell_fit_hierarchy_with_shell.rpt")
        )
        mapped = parse_map_hierarchy(self.fixture("shell_fit_map_clean.rpt"))
        expected_entities = {
            "zhao_shell_fit_top",
            "zhao_shell_top",
            "zhao_shell_fit_stimulus",
            "zhao_shell_fit_gpu_sink",
            "zhao_shell_fit_video_sink",
            "zhao_shell_fit_audio_sink",
            "zhao_shell_fit_helper",
        }
        self.assertEqual({row.entity_name for row in fitter}, expected_entities)
        self.assertEqual({row.entity_name for row in mapped}, expected_entities)
        self.assertEqual(len(fitter), len(expected_entities))
        self.assertEqual(len(mapped), len(expected_entities))

    def test_malformed_hierarchy_cell_rows_fail_closed(self) -> None:
        controls = (
            (
                parse_fitter_hierarchy,
                self.fixture("shell_fit_hierarchy_with_shell.rpt").replace(
                    "; zhao_shell_top          ; work         ;",
                    "; zhao_shell_top          ;",
                    1,
                ),
                "malformed fitter hierarchy row",
            ),
            (
                parse_map_hierarchy,
                self.fixture("shell_fit_map_clean.rpt").replace(
                    "; zhao_shell_top                  ; work         ;",
                    "; zhao_shell_top                  ;",
                    1,
                ),
                "malformed map hierarchy row",
            ),
        )
        for parser, text, message in controls:
            with self.subTest(parser=parser.__name__), self.assertRaisesRegex(
                ShellPortError, message
            ):
                parser(text)

    def test_hierarchy_rows_without_leading_delimiter_fail_closed(self) -> None:
        controls = (
            (
                parse_fitter_hierarchy,
                self.fixture("shell_fit_hierarchy_with_shell.rpt").replace(
                    ";    |zhao_shell_top:u_shell",
                    "     |zhao_shell_top:u_shell",
                    1,
                ),
                "malformed fitter hierarchy row.*missing leading ';' delimiter",
            ),
            (
                parse_map_hierarchy,
                self.fixture("shell_fit_map_clean.rpt").replace(
                    ";    |zhao_shell_top:u_shell|",
                    "     |zhao_shell_top:u_shell|",
                    1,
                ),
                "malformed map hierarchy row.*missing leading ';' delimiter",
            ),
        )
        for parser, text, message in controls:
            with self.subTest(parser=parser.__name__), self.assertRaisesRegex(
                ShellPortError, message
            ):
                parser(text)

    def test_descendant_rich_hierarchy_matches_exact_shell_identity(self) -> None:
        rows = parse_fitter_hierarchy(
            self.fixture("shell_fit_hierarchy_descendant_rich.rpt")
        )
        shell = require_shell_hierarchy(rows)
        self.assertEqual(shell.node, "|zhao_shell_top:u_shell")
        self.assertEqual(
            shell.full_hierarchy_name,
            "|zhao_shell_fit_top|zhao_shell_top:u_shell",
        )
        self.assertEqual(shell.entity_name, "zhao_shell_top")

    def test_descendant_only_hierarchy_does_not_match_shell(self) -> None:
        rows = parse_fitter_hierarchy(
            self.fixture("shell_fit_hierarchy_descendant_only.rpt")
        )
        with self.assertRaisesRegex(
            ShellPortError,
            "expected one exact fitted hierarchy row for 'zhao_shell_top:u_shell', found 0",
        ):
            require_shell_hierarchy(rows)

    def test_entity_name_mismatch_does_not_match_exact_row(self) -> None:
        text = self.fixture("shell_fit_hierarchy_descendant_rich.rpt").replace(
            "; zhao_shell_top ; work ;", "; wrong_entity ; work ;", 1
        )
        rows = parse_fitter_hierarchy(text)
        with self.assertRaisesRegex(ShellPortError, "found 0"):
            require_exact_hierarchy_row(
                rows,
                top_module="zhao_shell_fit_top",
                module="zhao_shell_top",
                instance="u_shell",
            )

    def test_fitter_library_name_mismatch_does_not_match_exact_row(self) -> None:
        text = self.fixture("shell_fit_hierarchy_descendant_rich.rpt").replace(
            "; zhao_shell_top ; work ;", "; zhao_shell_top ; forged ;", 1
        )
        rows = parse_fitter_hierarchy(text)
        with self.assertRaisesRegex(ShellPortError, "found 0"):
            require_exact_hierarchy_row(
                rows,
                top_module="zhao_shell_fit_top",
                module="zhao_shell_top",
                instance="u_shell",
            )

    def test_genuine_archived_map_summary_and_entity_table(self) -> None:
        summary_text = ARCHIVED_MAP_SUMMARY.read_text(encoding="utf-8")
        report_text = ARCHIVED_MAP_REPORT.read_text(encoding="utf-8")
        summary = parse_map_summary(summary_text, map_report_text=report_text)
        self.assertEqual(summary.top_entity, "zhao_geom_project")
        self.assertEqual(summary.combinational_aluts, 9049)
        self.assertEqual(summary.registers, 5909)
        validate_map_summary(summary, expected_top="zhao_geom_project")
        rows = parse_map_hierarchy(report_text)
        self.assertGreater(len(rows), 100)
        validate_map_hierarchy(summary, rows)
        core = require_exact_map_hierarchy_row(
            rows,
            top_module="zhao_geom_project",
            module="zhao_project_core",
            instance="u_core",
        )
        self.assertEqual(core.combinational_aluts, 9008)
        self.assertEqual(core.combinational_aluts_self, 8292)
        self.assertEqual(core.registers, 5877)
        self.assertEqual(core.registers_self, 5455)

    def test_shell_map_top_mismatch_fires(self) -> None:
        summary_text = self.fixture("shell_fit_map_clean.summary").replace(
            "Top-level Entity Name : zhao_shell_fit_top",
            "Top-level Entity Name : zhao_shell_top",
        )
        report_text = self.fixture("shell_fit_map_clean.rpt")
        summary = parse_map_summary(summary_text, map_report_text=report_text)
        with self.assertRaisesRegex(ShellPortError, "map top entity"):
            validate_map_summary(summary)

    def test_shell_map_attribution_is_exact(self) -> None:
        summary_text = self.fixture("shell_fit_map_clean.summary")
        report_text = self.fixture("shell_fit_map_clean.rpt")
        summary = parse_map_summary(summary_text, map_report_text=report_text)
        rows = parse_map_hierarchy(report_text)
        validate_map_summary(summary)
        validate_map_hierarchy(summary, rows)
        shell = require_shell_map_hierarchy(rows)
        self.assertEqual(shell.node, "|zhao_shell_top:u_shell|")
        self.assertEqual(shell.combinational_aluts, 8999)
        self.assertEqual(shell.combinational_aluts_self, 8999)
        self.assertEqual(shell.registers, 14321)

    def test_shell_map_descendant_only_or_entity_mismatch_fires(self) -> None:
        report = self.fixture("shell_fit_map_clean.rpt")
        missing = report.replace(
            ";    |zhao_shell_top:u_shell|                  ; 8999 (8999)         ; 14321 (14321)             ; 2097152           ; 84         ; 0    ; 0            ; |zhao_shell_fit_top|zhao_shell_top:u_shell                       ; zhao_shell_top                  ; work         ;\n",
            "",
        )
        with self.assertRaisesRegex(ShellPortError, "found 0"):
            require_shell_map_hierarchy(parse_map_hierarchy(missing))
        mismatched = report.replace("; zhao_shell_top                  ; work", "; wrong_entity                   ; work", 1)
        with self.assertRaisesRegex(ShellPortError, "found 0"):
            require_shell_map_hierarchy(parse_map_hierarchy(mismatched))

    def test_git_cleanliness_is_derived_from_raw_boundary(self) -> None:
        clean = parse_git_evidence(
            head=(FIXTURES / "shell_fit_git_head.txt").read_bytes(),
            status=b"",
            worktree_diff=b"",
            staged_diff=b"",
            index_flags=b"",
        )
        self.assertEqual(clean.source_commit, self.SOURCE_COMMIT)
        with self.assertRaisesRegex(ShellPortError, "externally captured git/stage evidence is dirty"):
            parse_git_evidence(
                head=(FIXTURES / "shell_fit_git_head.txt").read_bytes(),
                status=b" M fpga/rtl/common/zhao_shell_top.sv\n",
                worktree_diff=b"",
                staged_diff=b"",
                index_flags=b"",
            )
        with self.assertRaisesRegex(ShellPortError, "index concealment flags"):
            parse_git_evidence(
                head=(FIXTURES / "shell_fit_git_head.txt").read_bytes(),
                status=b"",
                worktree_diff=b"",
                staged_diff=b"",
                index_flags=b"h unrelated-tracked.txt\x00",
            )

    def test_git_index_flag_parser_accepts_clean_opaque_paths(self) -> None:
        self.assertEqual(
            parse_git_index_flags(
                b"H ordinary.txt\x00"
                b"H  leading-space.txt\x00"
                b"H unicode-\xe7\x8c\xab.txt\x00"
            ),
            b"",
        )

    def test_git_index_flag_parser_fails_closed_on_every_other_shape(self) -> None:
        controls = {
            "unknown tag": (b"Z path.txt\x00", "non-clean or unknown tag"),
            "truncated final record": (b"H path.txt", "truncated non-NUL record"),
            "empty path": (b"H \x00", "empty path"),
            "malformed separator": (b"H\tpath.txt\x00", "exact tag/space separator"),
            "empty interior record": (
                b"H path.txt\x00\x00",
                "malformed empty record",
            ),
        }
        for tag in b"MRCK?U":
            controls[f"non-clean tag {chr(tag)}"] = (
                bytes((tag,)) + b" path.txt\x00",
                "non-clean or unknown tag",
            )
        for name, (raw, message) in controls.items():
            with self.subTest(name=name), self.assertRaisesRegex(
                ShellPortError, message
            ):
                parse_git_index_flags(raw)

    def test_git_blob_lookup_reads_exact_real_commit_tree(self) -> None:
        commit = subprocess.run(
            ["git", "-C", str(REPO), "rev-parse", "HEAD"],
            check=True,
            stdout=subprocess.PIPE,
            text=True, encoding="utf-8", errors="replace",
        ).stdout.strip()
        paths = {
            "shell": "fpga/rtl/common/zhao_shell_top.sv",
            "package": "fpga/rtl/common/zhao_pkg.sv",
        }
        blobs = read_git_blobs_at_commit(
            REPO, source_commit=commit, source_paths=paths
        )
        for name, path in paths.items():
            raw = subprocess.run(
                [
                    "git",
                    "-C",
                    str(REPO),
                    "--no-replace-objects",
                    "show",
                    f"{commit}:{path}",
                ],
                check=True,
                stdout=subprocess.PIPE,
            ).stdout
            self.assertEqual(blobs[name], raw)

    def test_nonexistent_source_commit_fires(self) -> None:
        with self.assertRaisesRegex(ShellPortError, "git .* failed"):
            read_git_blobs_at_commit(
                REPO,
                source_commit="f" * 40,
                source_paths={"shell": "fpga/rtl/common/zhao_shell_top.sv"},
            )

    def test_git_blob_byte_drift_fires_receipt_binding(self) -> None:
        evidence = self.evidence()
        evidence["git_blob_bytes"] = dict(evidence["git_blob_bytes"])
        evidence["git_blob_bytes"]["generator"] += b"\nold committed blob"
        with self.assertRaisesRegex(ShellPortError, "differ from Git blobs"):
            self.bind(evidence["receipt"], evidence)

    def test_non_special_compile_pool_byte_drift_fires_receipt_binding(self) -> None:
        evidence = self.evidence()
        abi = "fpga/rtl/generated/zhao_abi_pkg.sv"
        evidence["compile_source_bytes"] = dict(evidence["compile_source_bytes"])
        evidence["compile_source_bytes"][abi] += b"\nlocal dirty ABI mutation"
        with self.assertRaisesRegex(
            ShellPortError,
            "compile source pool specimen bytes differ from Git blobs.*zhao_abi_pkg",
        ):
            self.bind(evidence["receipt"], evidence)

    def test_line_ending_only_compile_pool_drift_is_still_rejected(self) -> None:
        evidence = self.evidence()
        abi = "fpga/rtl/generated/zhao_abi_pkg.sv"
        evidence["compile_source_bytes"] = dict(evidence["compile_source_bytes"])
        evidence["compile_source_git_blob_bytes"] = dict(
            evidence["compile_source_git_blob_bytes"]
        )
        evidence["compile_source_bytes"][abi] = b"line one\r\nline two\r\n"
        evidence["compile_source_git_blob_bytes"][abi] = b"line one\nline two\n"
        with self.assertRaisesRegex(
            ShellPortError,
            r"line-ending-only=\['fpga/rtl/generated/zhao_abi_pkg.sv'\] "
            r"\(still rejected\)",
        ):
            self.bind(evidence["receipt"], evidence)

    def test_receipt_v3_validates_cleanliness_and_pool_parity(self) -> None:
        receipt = parse_receipt(self.fixture("shell_fit_receipt_v3.json"))
        self.assertIs(receipt["rtlCleanAtHead"], True)
        self.assertEqual(receipt["evidenceMode"], "production")
        self.assertIs(receipt["compileSourcePoolParity"], True)
        validate_receipt(receipt)

    def test_dirty_receipt_is_rejected_before_every_other_field(self) -> None:
        with self.assertRaisesRegex(
            ShellPortError, "^receipt rtlCleanAtHead is not true$"
        ):
            validate_receipt({"rtlCleanAtHead": False})
        with self.assertRaisesRegex(
            ShellPortError, "^receipt rtlCleanAtHead is not true$"
        ):
            parse_receipt('{"rtlCleanAtHead": false}')

    def test_receipt_binds_to_manifest_and_raw_reports(self) -> None:
        evidence = self.evidence()
        validate_receipt(evidence["receipt"])
        self.bind(evidence["receipt"], evidence)

    def test_every_receipt_evidence_mismatch_fires(self) -> None:
        evidence = self.evidence()
        controls = {
            "commit": (
                lambda row: row.__setitem__("sourceCommit", "f" * 40),
                "sourceCommit mismatch",
            ),
            "gate": (
                lambda row: row.__setitem__("characterization", "labelled_or_dirty_fit"),
                "wrong characterization gate",
            ),
            "mode": (
                lambda row: row.__setitem__("evidenceMode", "test-only"),
                "evidenceMode.*does not match invocation mode",
            ),
            "tool": (
                lambda row: row["tool"].__setitem__("version", "99.0"),
                "tool mismatch",
            ),
            "device": (
                lambda row: row.__setitem__("device", "wrong-device"),
                "device mismatch",
            ),
            "resources": (
                lambda row: row["resources"].__setitem__("alms", 1),
                "resource mismatch",
            ),
            "boundary": (
                lambda row: row["boundary"]["ports"][0].__setitem__("name", "forged_clk"),
                "receipt boundary does not match",
            ),
            "generated hashes": (
                lambda row: row["generatedArtifacts"].__setitem__(
                    "rtlSha256", "0" * 64
                ),
                "generated artifact hashes do not match committed bytes",
            ),
            "source hashes": (
                lambda row: row["sourceHashes"].__setitem__("policy", "0" * 64),
                "receipt sourceHashes do not match independently hashed raw inputs",
            ),
            "source identity": (
                lambda row: row["sourceArtifacts"]["packet"].__setitem__(
                    "path", "wrong-packet.bin"
                ),
                "source artifact identities/hashes do not match raw inputs",
            ),
            "source pool": (
                lambda row: row["compileSourcePool"].append("hidden.sv"),
                "compileSourcePool does not match",
            ),
            "selected SDC": (
                lambda row: row.__setitem__("selectedSdc", "wrong.sdc"),
                "selectedSdc does not match",
            ),
            "configuration": (
                lambda row: row["configuration"]["effectiveSettings"].__setitem__(
                    "DEVICE", "wrong-device"
                ),
                "configuration does not match effective QPF/QSF/SDC closure",
            ),
            "execution": (
                lambda row: row["execution"].__setitem__("processors", 99),
                "execution parameters do not match this invocation",
            ),
            "connectivity": (
                lambda row: row["connectivity"]["ports"][0].__setitem__(
                    "postMapCount", 0
                ),
                "connectivity does not match the post-map port-bit witness",
            ),
            "timing": (
                lambda row: row["timing"].__setitem__("timingPassed", False),
                "timing does not match parsed timing evidence",
            ),
            "stage": (
                lambda row: row["stages"].__setitem__("postMap", "failed"),
                "stage mismatch",
            ),
            "STA hash": (
                lambda row: row["evidenceArtifacts"]["sta"].__setitem__(
                    "sha256", "0" * 64
                ),
                "evidence artifact identities/hashes do not match raw reports",
            ),
            "profile": (
                lambda row: row.__setitem__("trafficProfile", "wrong-profile"),
                "trafficProfile mismatch",
            ),
            "entities": (
                lambda row: row["entities"].pop(),
                "entities do not preserve every fitted hierarchy row",
            ),
            "map entities": (
                lambda row: row["mapEntities"].pop(),
                "mapEntities do not preserve every mapped hierarchy row",
            ),
            "remainder": (
                lambda row: row["remainderAttribution"].__setitem__(
                    "method", "subtraction"
                ),
                "remainder attribution mismatch",
            ),
            "limitations": (
                lambda row: row.__setitem__("limitations", []),
                "limitations do not match bound characterization limitations",
            ),
        }
        for name, (mutate, message) in controls.items():
            with self.subTest(name=name):
                receipt = copy.deepcopy(evidence["receipt"])
                mutate(receipt)
                with self.assertRaisesRegex(ShellPortError, message):
                    self.bind(receipt, evidence)

    def test_each_raw_source_byte_mismatch_fires(self) -> None:
        base = self.evidence()
        for name in base["source_bytes"]:
            with self.subTest(name=name):
                evidence = dict(base)
                evidence["source_bytes"] = dict(base["source_bytes"])
                evidence["source_bytes"][name] += b"\nmutation"
                with self.assertRaisesRegex(
                    ShellPortError,
                    "Git blobs|raw inputs|raw packet identity|manifest provenance",
                ):
                    self.bind(base["receipt"], evidence)

    def test_sta_raw_hash_mismatch_fires(self) -> None:
        base = self.evidence()
        evidence = dict(base)
        evidence["evidence_bytes"] = dict(base["evidence_bytes"])
        evidence["evidence_bytes"]["sta"] += b"\nInfo: co-moving clean prose.\n"
        with self.assertRaisesRegex(
            ShellPortError, "evidence artifact identities/hashes do not match raw reports"
        ):
            self.bind(base["receipt"], evidence)

    def test_reports_cli_rejects_dirty_non_special_pool_member_against_real_git_commit(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            source_commit = self.make_clean_cli_repository(temporary_repo)

            abi_relative = "fpga/rtl/generated/zhao_abi_pkg.sv"
            abi = temporary_repo / abi_relative
            abi.write_bytes(abi.read_bytes() + b"\nlocal dirty ABI mutation\n")

            # Reproduce the rejected attack exactly: an uncommitted attribute and
            # repository-local smudge command make path-aware cat-file output the
            # dirty worktree bytes even though the commit's raw blob is unchanged.
            filter_script = temporary_repo / "attack_filter.py"
            filter_script.write_text(
                "from pathlib import Path\n"
                "import sys\n"
                "sys.stdin.buffer.read()\n"
                f"sys.stdout.buffer.write(Path({str(abi)!r}).read_bytes())\n",
                encoding="utf-8",
            )
            (temporary_repo / ".gitattributes").write_text(
                f"{abi_relative} filter=shellfit-attack\n", encoding="utf-8"
            )
            filter_command = (
                f'"{Path(sys.executable).as_posix()}" "{filter_script.name}"'
            )
            for key, value in (
                ("filter.shellfit-attack.clean", "cat"),
                ("filter.shellfit-attack.smudge", filter_command),
                ("filter.shellfit-attack.required", "true"),
            ):
                completed = subprocess.run(
                    ["git", "-C", str(temporary_repo), "config", key, value],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    text=True, encoding="utf-8", errors="replace",
                    check=False,
                )
                self.assertEqual(completed.returncode, 0, completed.stdout)

            raw_blob = subprocess.run(
                [
                    "git",
                    "-C",
                    str(temporary_repo),
                    "--no-replace-objects",
                    "show",
                    f"{source_commit}:{abi_relative}",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            ).stdout
            filtered_blob = subprocess.run(
                [
                    "git",
                    "-C",
                    str(temporary_repo),
                    "cat-file",
                    "--filters",
                    f"--path={abi_relative}",
                    f"{source_commit}:{abi_relative}",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=True,
            ).stdout
            self.assertNotEqual(raw_blob, abi.read_bytes())
            self.assertEqual(filtered_blob, abi.read_bytes())
            self.assertEqual(
                read_git_blobs_at_commit(
                    temporary_repo,
                    source_commit=source_commit,
                    source_paths={"abi": abi_relative},
                )["abi"],
                raw_blob,
            )

            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                result = reports_main(self.reports_argv(temporary_repo))
        self.assertEqual(result, 1, output.getvalue())
        self.assertIn(
            "captured git evidence differs from direct repository state",
            output.getvalue(),
        )
        self.assertIn("gitStatus", output.getvalue())

    def test_reports_cli_rejects_unknown_index_tag(self) -> None:
        self.assert_injected_index_listing_result(
            b"Z unrelated-tracked.txt\x00",
            expected_result=1,
            expected_message="non-clean or unknown tag 0x5a",
        )

    def test_reports_cli_rejects_truncated_index_record(self) -> None:
        self.assert_injected_index_listing_result(
            b"H unrelated-tracked.txt",
            expected_result=1,
            expected_message="truncated non-NUL record",
        )

    def test_reports_cli_rejects_empty_index_path(self) -> None:
        self.assert_injected_index_listing_result(
            b"H \x00",
            expected_result=1,
            expected_message="record has an empty path",
        )

    def test_reports_cli_rejects_malformed_index_separator(self) -> None:
        self.assert_injected_index_listing_result(
            b"H\tunrelated-tracked.txt\x00",
            expected_result=1,
            expected_message="exact tag/space separator",
        )

    def test_reports_cli_accepts_clean_ordinary_index_record(self) -> None:
        self.assert_injected_index_listing_result(
            b"H unrelated-tracked.txt\x00",
            expected_result=0,
            expected_message="receipt=raw-bound",
        )

    def test_reports_cli_accepts_leading_space_index_path(self) -> None:
        self.assert_injected_index_listing_result(
            b"H  leading-space.txt\x00",
            expected_result=0,
            expected_message="receipt=raw-bound",
        )

    def test_reports_cli_accepts_unicode_index_path(self) -> None:
        self.assert_injected_index_listing_result(
            b"H unicode-\xe7\x8c\xab.txt\x00",
            expected_result=0,
            expected_message="receipt=raw-bound",
        )

    def test_reports_cli_rejects_assume_unchanged_hidden_tracked_dirt(
        self,
    ) -> None:
        self.assert_hidden_tracked_edit_is_rejected(
            index_option="--assume-unchanged", expected_tag=b"h"
        )

    def test_reports_cli_rejects_skip_worktree_hidden_tracked_dirt(self) -> None:
        self.assert_hidden_tracked_edit_is_rejected(
            index_option="--skip-worktree", expected_tag=b"S"
        )

    def test_reports_cli_rejects_stale_clean_capture_for_unrelated_tracked_dirt(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            self.make_clean_cli_repository(
                temporary_repo, add_unrelated_tracked_file=True
            )
            # This file is outside every named source, report, and compile-pool
            # path. Before live evidence verification the stale clean capture
            # made the complete CLI return success.
            (temporary_repo / "unrelated-tracked.txt").write_text(
                "modified after clean capture\n", encoding="utf-8"
            )
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                result = reports_main(self.reports_argv(temporary_repo))
        self.assertEqual(result, 1, output.getvalue())
        self.assertIn(
            "captured git evidence differs from direct repository state",
            output.getvalue(),
        )
        self.assertIn("gitStatus", output.getvalue())
        self.assertIn("gitWorktreeDiff", output.getvalue())

    def test_receipt_builder_exactly_reproduces_bound_fixture(self) -> None:
        evidence = self.evidence()
        self.assertEqual(self.build(evidence), evidence["receipt"])

    def test_manifest_text_provenance_is_checkout_line_ending_independent(self) -> None:
        evidence = self.evidence()

        def with_crlf(data: bytes) -> bytes:
            canonical = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
            return canonical.replace(b"\n", b"\r\n")

        evidence["source_bytes"] = dict(evidence["source_bytes"])
        evidence["git_blob_bytes"] = dict(evidence["git_blob_bytes"])
        for name in ("shell", "package", "policy", "generator", "parser"):
            converted = with_crlf(evidence["source_bytes"][name])
            evidence["source_bytes"][name] = converted
            evidence["git_blob_bytes"][name] = converted

        package_path = "fpga/rtl/common/zhao_pkg.sv"
        converted_package = evidence["source_bytes"]["package"]
        evidence["compile_source_bytes"] = dict(evidence["compile_source_bytes"])
        evidence["compile_source_git_blob_bytes"] = dict(
            evidence["compile_source_git_blob_bytes"]
        )
        evidence["compile_source_bytes"][package_path] = converted_package
        evidence["compile_source_git_blob_bytes"][package_path] = converted_package

        receipt = self.build(evidence)
        self.bind(receipt, evidence)

    def test_manifest_missing_text_hash_mode_fires_receipt_binding(self) -> None:
        evidence = self.evidence()
        evidence["manifest"] = dict(evidence["manifest"])
        evidence["manifest"].pop("source_hash_canonicalization")
        with self.assertRaisesRegex(
            ShellPortError, "does not declare utf8-lf-v1 source hash canonicalization"
        ):
            self.bind(self.build(evidence), evidence)

    def test_reports_cli_emits_then_binds_schema_three_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            self.make_clean_cli_repository(temporary_repo)
            expected = json.loads(
                (temporary_repo / "tests/tools/fixtures/shell_fit_receipt_v3.json").read_bytes()
            )
            emitted = temporary_repo / "reports/synthesis/emitted-shell-fit.json"
            argv = self.reports_argv(temporary_repo)
            receipt_at = argv.index("--receipt")
            argv[receipt_at] = "--emit-receipt"
            argv[receipt_at + 1] = str(emitted)
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                result = reports_main(argv)
            self.assertEqual(result, 0, output.getvalue())
            self.assertEqual(json.loads(emitted.read_bytes()), expected)
            self.assertEqual(list(emitted.parent.glob(f".{emitted.name}.*.tmp")), [])

    def test_reports_cli_does_not_emit_a_receipt_for_invalid_raw_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            self.make_clean_cli_repository(temporary_repo)
            bad_summary = temporary_repo / "reports/bad-shell-fit.summary"
            bad_summary.parent.mkdir(parents=True, exist_ok=True)
            bad_summary.write_text(
                self.fixture("shell_fit_summary_one_virtual_pin.txt"), encoding="utf-8"
            )
            emitted = temporary_repo / "reports/synthesis/must-not-exist.json"
            argv = self.reports_argv(temporary_repo)
            argv[argv.index("--summary") + 1] = str(bad_summary)
            receipt_at = argv.index("--receipt")
            argv[receipt_at] = "--emit-receipt"
            argv[receipt_at + 1] = str(emitted)
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                result = reports_main(argv)
            self.assertEqual(result, 1, output.getvalue())
            self.assertFalse(emitted.exists())
            self.assertIn("Total virtual pins is 1", output.getvalue())

    def test_timing_metric_path_report_mismatch_fires(self) -> None:
        evidence = self.evidence()
        paths = {
            name: evidence["evidence_bytes"][f"{name}Paths"].decode("utf-8")
            for name in ("setup", "hold", "recovery", "removal")
        }
        paths["hold"] = paths["hold"].replace("0.256", "0.999")
        with self.assertRaisesRegex(ShellPortError, "hold metric/report worst slack mismatch"):
            timing_evidence(
                metrics_text=evidence["evidence_bytes"]["timingMetrics"].decode("utf-8"),
                path_reports=paths,
                unconstrained_text=evidence["evidence_bytes"]["unconstrainedPaths"].decode("utf-8"),
                transfers_text=evidence["evidence_bytes"]["clockTransfers"].decode("utf-8"),
                critical_warnings=[],
            )

    def test_setup_and_hold_nothing_to_report_fail_closed(self) -> None:
        evidence = self.evidence()
        metrics = evidence["evidence_bytes"]["timingMetrics"].decode("utf-8")
        base_paths = {
            name: evidence["evidence_bytes"][f"{name}Paths"].decode("utf-8")
            for name in ("setup", "hold", "recovery", "removal")
        }
        for analysis in ("setup", "hold"):
            with self.subTest(analysis=analysis):
                paths = dict(base_paths)
                paths[analysis] = "Nothing to report.\n"
                with self.assertRaisesRegex(
                    ShellPortError,
                    f"raw {analysis} path report has no paths for a mapped, clocked shell",
                ):
                    timing_evidence(
                        metrics_text=metrics,
                        path_reports=paths,
                        unconstrained_text=evidence["evidence_bytes"][
                            "unconstrainedPaths"
                        ].decode("utf-8"),
                        transfers_text=evidence["evidence_bytes"][
                            "clockTransfers"
                        ].decode("utf-8"),
                        critical_warnings=[],
                    )

    def test_self_clock_transfers_require_positive_numeric_paths(self) -> None:
        evidence = self.evidence()
        clean = evidence["evidence_bytes"]["clockTransfers"].decode("utf-8")
        controls = (
            ("false cut", "; gpu_clk    ; gpu_clk   ; 50798906", "; gpu_clk    ; gpu_clk   ; false path"),
            ("zero", "; vid_clk    ; vid_clk   ; 89738", "; vid_clk    ; vid_clk   ; 0"),
            ("nonsense", "; audio_clk  ; audio_clk ; 5166", "; audio_clk  ; audio_clk ; N/A"),
        )
        for label, old, new in controls:
            with self.subTest(label=label), self.assertRaisesRegex(
                ShellPortError, "self-clock transfer"
            ):
                timing_evidence(
                    metrics_text=evidence["evidence_bytes"]["timingMetrics"].decode("utf-8"),
                    path_reports={
                        name: evidence["evidence_bytes"][f"{name}Paths"].decode("utf-8")
                        for name in ("setup", "hold", "recovery", "removal")
                    },
                    unconstrained_text=evidence["evidence_bytes"][
                        "unconstrainedPaths"
                    ].decode("utf-8"),
                    transfers_text=clean.replace(old, new),
                    critical_warnings=[],
                )

    def test_sourced_clock_transfer_relaxation_fires(self) -> None:
        evidence = self.evidence()
        transfers = evidence["evidence_bytes"]["clockTransfers"].decode("utf-8")
        transfers = transfers.replace(
            "; gpu_clk    ; vid_clk   ; 20         ;",
            "; gpu_clk    ; vid_clk   ; false path ;",
        )
        with self.assertRaisesRegex(ShellPortError, "GPU/video transfer"):
            timing_evidence(
                metrics_text=evidence["evidence_bytes"]["timingMetrics"].decode("utf-8"),
                path_reports={
                    name: evidence["evidence_bytes"][f"{name}Paths"].decode("utf-8")
                    for name in ("setup", "hold", "recovery", "removal")
                },
                unconstrained_text=evidence["evidence_bytes"]["unconstrainedPaths"].decode("utf-8"),
                transfers_text=transfers,
                critical_warnings=[],
            )

    def test_reports_subprocess_missing_post_map_bit_fires_without_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_clean_cli_repository(repo)
            relative = "tests/tools/fixtures/shell_fit_post_map_connectivity.tsv"
            data = (repo / relative).read_bytes().replace(
                b"port\tfit_epoch_o[2]\toutput\t1\n", b""
            )
            self.commit_fixture_change(repo, relative, data)
            completed, emitted = self.run_reports_emit_subprocess(repo)
            exists = emitted.exists()
        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertFalse(exists)
        self.assertIn("post-map wrapper port/bit witness", completed.stdout)

    def test_post_map_zero_and_duplicate_port_counts_fire(self) -> None:
        clean = self.fixture("shell_fit_post_map_connectivity.tsv")
        map_report = self.fixture("shell_fit_map_clean.rpt")
        for label, mutated in (
            ("zero", clean.replace("port\tgpu_clk\tinput\t1", "port\tgpu_clk\tinput\t0")),
            ("duplicate", clean.replace("port\tgpu_clk\tinput\t1",
                                        "port\tgpu_clk\tinput\t2")),
        ):
            with self.subTest(label=label), self.assertRaisesRegex(
                ShellPortError, "missing or duplicated"
            ):
                validate_post_map_connectivity(
                    parse_post_map_connectivity(mutated), map_report
                )

    def test_reports_subprocess_disconnected_mapped_port_fires_without_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_clean_cli_repository(repo)
            relative = "tests/tools/fixtures/shell_fit_map_clean.rpt"
            data = (repo / relative).read_bytes().replace(
                b"; Port   ; Type ; Severity ; Details ;\n+--------+------+----------+---------+\n+--------+------+----------+---------+",
                b"; Port   ; Type ; Severity ; Details ;\n+--------+------+----------+---------+\n; gpu_clk ; Input ; Warning ; dangling mapped input ;\n+--------+------+----------+---------+",
            )
            self.commit_fixture_change(repo, relative, data)
            completed, emitted = self.run_reports_emit_subprocess(repo)
            exists = emitted.exists()
        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertFalse(exists)
        self.assertIn("disconnected/dangling ports", completed.stdout)

    def test_reports_subprocess_negative_timing_retains_clean_resources_and_fails_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_clean_cli_repository(repo)
            metrics_rel = "tests/tools/fixtures/shell_fit_timing_metrics_clean.tsv"
            metrics = (repo / metrics_rel).read_bytes().replace(
                b"analysis\tsetup\t0.321\t0", b"analysis\tsetup\t-0.100\t1"
            )
            self.commit_fixture_change(repo, metrics_rel, metrics)
            setup_rel = "tests/tools/fixtures/shell_fit_setup_paths_clean.rpt"
            setup = (repo / setup_rel).read_bytes().replace(
                b"Found 200 setup paths (0 violated). Worst case slack is 0.321",
                b"Found 200 setup paths (1 violated). Worst case slack is -0.100",
            )
            self.commit_fixture_change(repo, setup_rel, setup)
            completed, emitted = self.run_reports_emit_subprocess(repo)
            receipt = json.loads(emitted.read_bytes()) if emitted.exists() else None
        self.assertEqual(completed.returncode, 2, completed.stdout)
        self.assertNotIn("gate=PASS", completed.stdout)
        self.assertIn("gate=FAIL", completed.stdout)
        self.assertIsNotNone(receipt)
        self.assertEqual(receipt["gate"]["status"], "fail")
        self.assertEqual(receipt["stages"]["map"], "successful")
        self.assertEqual(receipt["stages"]["fit"], "successful")
        self.assertEqual(receipt["resources"]["dspBlocks"], 87)

    def test_reports_subprocess_unconstrained_paths_fire_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_clean_cli_repository(repo)
            relative = "tests/tools/fixtures/shell_fit_unconstrained_clean.rpt"
            data = (repo / relative).read_bytes().replace(
                b"; Unconstrained Output Port Paths  ; 0          ; 0",
                b"; Unconstrained Output Port Paths  ; 1          ; 0",
            )
            self.commit_fixture_change(repo, relative, data)
            completed, emitted = self.run_reports_emit_subprocess(repo)
            receipt = json.loads(emitted.read_bytes()) if emitted.exists() else None
        self.assertEqual(completed.returncode, 2, completed.stdout)
        self.assertEqual(receipt["gate"]["status"], "fail")
        self.assertTrue(any("unconstrained" in item for item in receipt["gate"]["failures"]))

    def test_reports_subprocess_critical_warning_in_stage_stderr_fires_gate(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_clean_cli_repository(repo)
            relative = "tests/tools/fixtures/shell_fit_fit.stderr.log"
            self.commit_fixture_change(
                repo,
                relative,
                b"Critical Warning (99999): positive-control stage warning\n",
            )
            completed, emitted = self.run_reports_emit_subprocess(repo)
            receipt = json.loads(emitted.read_bytes()) if emitted.exists() else None
        self.assertEqual(completed.returncode, 2, completed.stdout)
        self.assertEqual(receipt["gate"]["status"], "fail")
        self.assertEqual(len(receipt["timing"]["criticalWarnings"]), 1)
        self.assertEqual(receipt["timing"]["criticalWarnings"][0]["artifact"], "fitStderr")

    def test_reports_subprocess_missing_stage_completion_fires_without_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_clean_cli_repository(repo)
            relative = "tests/tools/fixtures/shell_fit_map.stdout.log"
            self.commit_fixture_change(repo, relative, b"Info: map started but did not finish\n")
            completed, emitted = self.run_reports_emit_subprocess(repo)
            exists = emitted.exists()
        self.assertEqual(completed.returncode, 1, completed.stdout)
        self.assertFalse(exists)
        self.assertIn("mapStdout has no unique successful zero-error completion", completed.stdout)

    def test_receipt_preserves_unmanifested_rows_and_reported_top_self(self) -> None:
        receipt = self.evidence()["receipt"]
        self.assertEqual(sum(row["role"] == "unmanifested" for row in receipt["entities"]), 1)
        self.assertEqual(sum(row["role"] == "unmanifested" for row in receipt["mapEntities"]), 1)
        remainder = receipt["remainderAttribution"]
        self.assertIs(remainder["calculatedBySubtraction"], False)
        self.assertEqual(remainder["reportedFitterTopSelf"]["almsNeededSelf"], "20.0")
        self.assertEqual(remainder["reportedMapTopSelf"]["combinationalAlutsSelf"], 40)

    def test_text_subprocess_invalid_utf8_preserves_diagnostics_and_rc(self) -> None:
        completed = subprocess.run(
            [
                sys.executable,
                "-c",
                "import os; os.write(1, b'diagnostic-before-invalid-byte: \\x81\\n'); raise SystemExit(7)",
            ],
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 7)
        self.assertIsNotNone(completed.stdout)
        self.assertIn("diagnostic-before-invalid-byte: �", completed.stdout)

    def test_runner_rejects_bare_python_name(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        completed = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(TOOLS / "run_shell_fit.ps1"),
                "-PreflightOnly",
                "-PythonExe",
                "python",
            ],
            text=True, encoding="utf-8", errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("must be an absolute path", completed.stdout)

    def test_runner_rejects_explicitly_requested_missing_python(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            missing = Path(temporary) / "missing-python.exe"
            self.assertFalse(missing.exists())
            completed = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(TOOLS / "run_shell_fit.ps1"),
                    "-PreflightOnly",
                    "-PythonExe",
                    str(missing),
                ],
                text=True,
                encoding="utf-8",
                errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("Compatible Python 3.12 executable not found", completed.stdout)

    def test_runner_rejects_windowsapps_python_alias_stub(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            alias = Path(temporary) / "Microsoft" / "WindowsApps" / "python.exe"
            alias.parent.mkdir(parents=True)
            alias.write_bytes(b"Microsoft Store app-execution alias positive control\n")
            completed = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(TOOLS / "run_shell_fit.ps1"),
                    "-PreflightOnly",
                    "-PythonExe",
                    str(alias),
                ],
                text=True, encoding="utf-8", errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("WindowsApps/app-execution alias", completed.stdout)

    def test_runner_rejects_post_resolution_windowsapps_python_alias(self) -> None:
        observation = (
            "    $actual = [IO.Path]::GetFullPath((Resolve-Path "
            "-LiteralPath $requestedFull).Path)"
        )
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    $actual = Join-Path (Split-Path -Parent $actual) "
                "'WindowsApps\\python.exe'",
            ),),
            "resolves to a forbidden WindowsApps/app-execution alias",
        )

    def test_runner_rejects_copied_valid_python_at_wrong_path(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            copied = Path(temporary) / "python.exe"
            shutil.copy2(sys.executable, copied)
            completed = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(TOOLS / "run_shell_fit.ps1"),
                    "-PreflightOnly",
                    "-PythonExe",
                    str(copied),
                ],
                text=True, encoding="utf-8", errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("must name the pinned CPython installation", completed.stdout)

    def test_runner_rejects_missing_pinned_canonical_python(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    Assert-PinnedPythonFile $canonical 'python.exe' $CanonicalPythonExeSize $CanonicalPythonExeSha256",
                "    Assert-PinnedPythonFile ($canonical + '.missing-control') 'python.exe' $CanonicalPythonExeSize $CanonicalPythonExeSha256",
            ),),
            "Pinned CPython python.exe not found",
        )

    def test_runner_rejects_missing_pinned_python312_dll(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    Assert-PinnedPythonFile (Join-Path $installRoot 'python312.dll') 'python312.dll' $CanonicalPython312DllSize $CanonicalPython312DllSha256",
                "    Assert-PinnedPythonFile (Join-Path $installRoot 'python312.dll.missing-control') 'python312.dll' $CanonicalPython312DllSize $CanonicalPython312DllSha256",
            ),),
            "Pinned CPython python312.dll not found",
        )

    def test_runner_rejects_missing_pinned_python3_dll(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    Assert-PinnedPythonFile (Join-Path $installRoot 'python3.dll') 'python3.dll' $CanonicalPython3DllSize $CanonicalPython3DllSha256",
                "    Assert-PinnedPythonFile (Join-Path $installRoot 'python3.dll.missing-control') 'python3.dll' $CanonicalPython3DllSize $CanonicalPython3DllSha256",
            ),),
            "Pinned CPython python3.dll not found",
        )

    def test_runner_rejects_pinned_python_resolution_mismatch(self) -> None:
        observation = "    $expected = [IO.Path]::GetFullPath($Path)"
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation + " + '.wrong-location-control'",
            ),),
            "resolves outside its canonical location",
        )

    def test_runner_rejects_localappdata_relocation(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            fake_local = Path(temporary)
            relocated = (
                fake_local / "Programs" / "Python" / "Python312" / "python.exe"
            )
            relocated.parent.mkdir(parents=True)
            shutil.copy2(sys.executable, relocated)
            env = dict(os.environ)
            env["LOCALAPPDATA"] = str(fake_local)
            completed = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(TOOLS / "run_shell_fit.ps1"),
                    "-PreflightOnly",
                    "-PythonExe",
                    str(relocated),
                ],
                env=env,
                text=True, encoding="utf-8", errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("must name the pinned CPython installation", completed.stdout)

    def test_runner_rejects_python_exe_digest_mismatch_before_probe(self) -> None:
        observation = (
            "    $actualSha256 = (Get-FileHash -LiteralPath $resolved "
            "-Algorithm SHA256).Hash.ToLowerInvariant()"
        )
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    if ($Label -ceq 'python.exe') { $actualSha256 = '"
                + "0" * 64
                + "' }",
            ),),
            "python.exe SHA-256 mismatch",
        )

    def test_runner_rejects_python_exe_size_mismatch_before_probe(self) -> None:
        observation = "    $actualSize = (Get-Item -LiteralPath $resolved).Length"
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    if ($Label -ceq 'python.exe') { $actualSize += 1 }",
            ),),
            "python.exe size mismatch",
        )

    def test_runner_rejects_python312_dll_size_mismatch_before_probe(self) -> None:
        observation = "    $actualSize = (Get-Item -LiteralPath $resolved).Length"
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    if ($Label -ceq 'python312.dll') { $actualSize += 1 }",
            ),),
            "python312.dll size mismatch",
        )

    def test_runner_rejects_python3_dll_size_mismatch_before_probe(self) -> None:
        observation = "    $actualSize = (Get-Item -LiteralPath $resolved).Length"
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    if ($Label -ceq 'python3.dll') { $actualSize += 1 }",
            ),),
            "python3.dll size mismatch",
        )

    def test_runner_rejects_swapped_python_dll_digest_before_probe(self) -> None:
        observation = (
            "    $actualSha256 = (Get-FileHash -LiteralPath $resolved "
            "-Algorithm SHA256).Hash.ToLowerInvariant()"
        )
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    if ($Label -ceq 'python312.dll') { "
                "$actualSha256 = 'fb975a606e7fbf74f64260e3f60c3490b4f74a183c0926fd6ed1ac4c52ac7b1c' }",
            ),),
            "python312.dll SHA-256 mismatch",
        )

    def test_runner_rejects_python3_dll_digest_mismatch_before_probe(self) -> None:
        observation = (
            "    $actualSha256 = (Get-FileHash -LiteralPath $resolved "
            "-Algorithm SHA256).Hash.ToLowerInvariant()"
        )
        self.assert_runner_observation_rejected(
            ((
                observation,
                observation
                + "\n    if ($Label -ceq 'python3.dll') { $actualSha256 = '"
                + "0" * 64
                + "' }",
            ),),
            "python3.dll SHA-256 mismatch",
        )

    def test_runner_rejects_non_valid_python_signature_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $signatureStatus = $signature.Status.ToString()",
                "    $signatureStatus = 'HashMismatch'",
            ),),
            "Authenticode status is 'HashMismatch', not Valid",
        )

    def test_runner_rejects_missing_python_timestamp_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $timestamp = $signature.TimeStamperCertificate",
                "    $timestamp = $null",
            ),),
            "has no Authenticode timestamp certificate",
        )

    def test_runner_rejects_python_signer_subject_mismatch_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $signerSubject = $signer.Subject",
                "    $signerSubject = 'CN=Not Python Software Foundation'",
            ),),
            "Authenticode signer mismatch",
        )

    def test_runner_rejects_python_signer_thumbprint_mismatch_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $signerThumbprint = ($signer.Thumbprint -replace '\\s', '').ToUpperInvariant()",
                "    $signerThumbprint = '0000000000000000000000000000000000000000'",
            ),),
            "Authenticode signer mismatch",
        )

    def test_runner_rejects_python_timestamp_subject_mismatch_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $timestampSubject = $timestamp.Subject",
                "    $timestampSubject = 'CN=Wrong Timestamp Authority'",
            ),),
            "Authenticode timestamp mismatch",
        )

    def test_runner_rejects_python_timestamp_issuer_mismatch_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $timestampIssuer = $timestamp.Issuer",
                "    $timestampIssuer = 'CN=Wrong Timestamp Issuer'",
            ),),
            "Authenticode timestamp mismatch",
        )

    def test_runner_rejects_python_timestamp_thumbprint_mismatch_before_probe(self) -> None:
        self.assert_runner_observation_rejected(
            ((
                "    $timestampThumbprint = ($timestamp.Thumbprint -replace '\\s', '').ToUpperInvariant()",
                "    $timestampThumbprint = '0000000000000000000000000000000000000000'",
            ),),
            "Authenticode timestamp mismatch",
        )

    def test_runner_rejects_non_runnable_python_executable(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            runner = self.make_probe_only_runner(root)
            candidate = root / "python.exe"
            candidate.write_bytes(b"not a Windows executable\n")
            completed = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(runner),
                    "-PreflightOnly",
                    "-PythonExe",
                    str(candidate),
                ],
                text=True, encoding="utf-8", errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("could not be executed as a real Python interpreter", completed.stdout)

    def test_runner_rejects_wrong_python_version_probe(self) -> None:
        invocation = "    $probe = Invoke-PythonIdentityProbe $actual $probeScript $nonce"
        self.assert_runner_observation_rejected(
            ((
                invocation,
                invocation
                + "\n    $reported = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($actual))"
                + "\n    $probe.Stdout = \"ZHAO_SHELL_FIT_PYTHON_V1|$nonce|cpython|3|11|9|$reported`r`n\"",
            ),),
            "must report CPython 3.12",
        )

    def test_runner_rejects_fake_python_that_spoofs_probe(self) -> None:
        invocation = "    $probe = Invoke-PythonIdentityProbe $actual $probeScript $nonce"
        self.assert_runner_observation_rejected(
            ((
                invocation,
                invocation
                + "\n    $probe.Stdout = $probe.Stdout.Replace($nonce, "
                "'00000000000000000000000000000000')",
            ),),
            "probe output did not authenticate the requested interpreter",
        )

    def test_real_runner_rejects_dirty_source_cone_before_quartus(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_runner_repository(repo)
            source = repo / "fpga/rtl/memory/zhao_sdram_params_pkg.sv"
            source.write_text(source.read_text(encoding="utf-8") + "\n// substantive dirt\n",
                              encoding="utf-8")
            completed = subprocess.run(
                [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(repo / "tools/quartus/run_shell_fit.ps1"), "-PythonExe",
                 sys.executable, "-QuartusBin",
                 str(repo / "absent-quartus")],
                text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("Whole tracked tree is dirty at shell-fit invocation", completed.stdout)
        self.assertNotIn("Required Quartus executable", completed.stdout)

    def test_real_runner_rejects_any_dirty_launcher_before_quartus(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        for label, mutation in (
            ("line endings", lambda data: data.replace(b"\n", b"\r\n")),
            ("substantive", lambda data: data + b"\n# substantive change\n"),
        ):
            with self.subTest(label=label), tempfile.TemporaryDirectory() as temporary:
                repo = Path(temporary)
                self.make_runner_repository(repo)
                launcher = repo / "tools/quartus/run_shell_fit.ps1"
                launcher.write_bytes(mutation(launcher.read_bytes()))
                completed = subprocess.run(
                    [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                     str(launcher), "-PythonExe", sys.executable, "-QuartusBin", str(repo / "absent-quartus")],
                    text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    check=False,
                )
                self.assertNotEqual(completed.returncode, 0, completed.stdout)
                self.assertIn("Whole tracked tree is dirty at shell-fit invocation", completed.stdout)
                self.assertNotIn("Required Quartus executable", completed.stdout)

    def test_real_runner_rejects_unrelated_tracked_dirt_before_snapshot(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_runner_repository(repo)
            (repo / "unrelated.txt").write_text(
                "dirty before characterization starts\n", encoding="utf-8"
            )
            completed = subprocess.run(
                [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(repo / "tools/quartus/run_shell_fit.ps1"), "-PythonExe",
                 sys.executable, "-QuartusBin",
                 str(repo / "absent-quartus")],
                text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False,
            )
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("Whole tracked tree is dirty at shell-fit invocation", completed.stdout)
        self.assertIn("unrelated.txt", completed.stdout)
        self.assertNotIn("Required Quartus executable", completed.stdout)

    def test_real_runner_machine_wide_mutex_fires(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_runner_repository(repo)
            ready = repo / "mutex-ready"
            marker_literal = str(ready).replace("'", "''")
            holder_script = (
                "$m=[Threading.Mutex]::new($false,'Global\\ZhaoShellFitQuartusCharacterization');"
                "$null=$m.WaitOne();[IO.File]::WriteAllText('" + marker_literal + "','ready');"
                "Start-Sleep -Seconds 30"
            )
            holder = subprocess.Popen(
                [powershell, "-NoProfile", "-Command", holder_script],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
            )
            try:
                for _ in range(200):
                    if ready.exists():
                        break
                    if holder.poll() is not None:
                        self.fail("mutex holder exited before acquiring the mutex")
                    import time
                    time.sleep(0.05)
                self.assertTrue(ready.exists(), "mutex holder did not become ready")
                completed = subprocess.run(
                    [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                     str(repo / "tools/quartus/run_shell_fit.ps1"), "-PythonExe",
                 sys.executable, "-QuartusBin",
                     str(repo / "absent-quartus")],
                    text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    check=False,
                )
            finally:
                holder.terminate()
                holder.communicate(timeout=10)
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("machine-wide shell-fit characterization", completed.stdout)

    def test_production_runner_rejects_fake_quartus_without_publishing(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_runner_repository(repo)
            fake_bin = self.make_fake_quartus_bin(repo)
            map_count = repo / "map-count"
            env = dict(os.environ)
            env["ZHAO_TEST_MAP_COUNT"] = str(map_count)
            completed = subprocess.run(
                [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(repo / "tools/quartus/run_shell_fit.ps1"), "-PythonExe",
                 sys.executable, "-QuartusBin",
                 str(fake_bin)],
                env=env, text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False,
            )
            synthesis_exists = (repo / "reports/synthesis/zhao_shell_fit.json").exists()
            timing_exists = (repo / "reports/timing/zhao_shell_fit.json").exists()
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("requires canonical Quartus bin", completed.stdout)
        self.assertNotIn("PASS shell_fit_top_clean_characterization", completed.stdout)
        self.assertFalse(synthesis_exists)
        self.assertFalse(timing_exists)
        self.assertFalse(map_count.exists())

    def test_production_runner_rejects_wrong_binary_identity_at_canonical_path(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_runner_repository(repo)
            fake_bin = self.make_fake_quartus_bin(repo)
            runner = repo / "tools/quartus/run_shell_fit.ps1"
            runner_text = runner.read_text(encoding="utf-8")
            runner_text = runner_text.replace(
                "$CanonicalQuartusBin = 'C:\\intelFPGA_lite\\17.0\\quartus\\bin64'",
                "$CanonicalQuartusBin = '"
                + str(fake_bin).replace("'", "''")
                + "'",
            )
            self.assertNotEqual(runner_text, runner.read_text(encoding="utf-8"))
            runner.write_text(runner_text, encoding="utf-8")
            for arguments in (
                ("add", "--", "tools/quartus/run_shell_fit.ps1"),
                ("commit", "-m", "test-only canonical path mutation"),
            ):
                committed = subprocess.run(
                    ["git", "-C", str(repo), *arguments], text=True, encoding="utf-8", errors="replace",
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
                )
                self.assertEqual(committed.returncode, 0, committed.stdout)
            completed = subprocess.run(
                [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(runner), "-PythonExe", sys.executable, "-QuartusBin", str(fake_bin)],
                text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False,
            )
            synthesis_exists = (repo / "reports/synthesis/zhao_shell_fit.json").exists()
            timing_exists = (repo / "reports/timing/zhao_shell_fit.json").exists()
        self.assertNotEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("binary identity mismatch", completed.stdout)
        self.assertNotIn("PASS shell_fit_top_clean_characterization", completed.stdout)
        self.assertFalse(synthesis_exists)
        self.assertFalse(timing_exists)

    def test_real_runner_freezes_commit_and_binds_processors_end_to_end(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            source_commit = self.make_runner_repository(repo)
            fake_bin = self.make_fake_quartus_bin(repo)
            marker = repo / "map-started"
            release = repo / "map-continue"
            map_count_path = repo / "map-count"
            env = dict(os.environ)
            env["ZHAO_TEST_MAP_STARTED"] = str(marker)
            env["ZHAO_TEST_MAP_CONTINUE"] = str(release)
            env["ZHAO_TEST_MAP_COUNT"] = str(map_count_path)
            workspace = None
            process = subprocess.Popen(
                [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(repo / "tools/quartus/run_shell_fit.ps1"), "-PythonExe",
                 sys.executable, "-QuartusBin",
                 str(fake_bin), "-TestOnlyFakeQuartus", "-KeepWorkspace",
                 "-Processors", "3"],
                env=env, text=True, encoding="utf-8", errors="replace", stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            )
            try:
                import time
                for _ in range(600):
                    if marker.exists():
                        break
                    if process.poll() is not None:
                        output = process.communicate()[0]
                        self.fail("runner exited before map barrier:\n" + output)
                    time.sleep(0.05)
                self.assertTrue(marker.exists(), "fake map stage did not start")
                (repo / "unrelated.txt").write_text("HEAD moved after map began\n",
                                                     encoding="utf-8")
                for arguments in (("add", "--", "unrelated.txt"),
                                  ("commit", "-m", "move unrelated HEAD")):
                    completed = subprocess.run(
                        ["git", "-C", str(repo), *arguments], text=True, encoding="utf-8", errors="replace",
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False,
                    )
                    self.assertEqual(completed.returncode, 0, completed.stdout)
                moved_head = subprocess.run(
                    ["git", "-C", str(repo), "rev-parse", "HEAD"], check=True,
                    stdout=subprocess.PIPE, text=True, encoding="utf-8", errors="replace",
                ).stdout.strip()
                self.assertNotEqual(moved_head, source_commit)
                release.write_text("continue\n", encoding="ascii")
                output = process.communicate(timeout=120)[0]
                receipt_line = next(
                    line for line in output.splitlines()
                    if line.startswith("TEST_ONLY_RECEIPT ")
                )
                test_receipt = Path(receipt_line.partition(" ")[2])
                workspace_line = next(
                    line for line in output.splitlines() if line.startswith("WORKSPACE ")
                )
                workspace = Path(workspace_line.partition(" ")[2])
                synthesis = json.loads(test_receipt.read_bytes())
                production_ledgers = (
                    (repo / "reports/synthesis/zhao_shell_fit.json").exists(),
                    (repo / "reports/timing/zhao_shell_fit.json").exists(),
                )
                map_count = map_count_path.read_text(encoding="ascii").splitlines()
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate()
                if workspace is not None:
                    shutil.rmtree(workspace, ignore_errors=True)
        self.assertEqual(process.returncode, 0, output)
        self.assertIn("TEST-ONLY RESULT accepted", output)
        self.assertNotIn("PASS shell_fit_top_clean_characterization", output)
        self.assertEqual(production_ledgers, (False, False))
        self.assertEqual(map_count, ["map"])
        self.assertEqual(synthesis["sourceCommit"], source_commit)
        self.assertEqual(synthesis["evidenceMode"], "test-only")
        self.assertEqual(synthesis["execution"], {"processors": 3})
        self.assertEqual(set(synthesis["stages"]), {"map", "postMap", "fit", "timequest"})
        self.assertEqual(synthesis["remainderAttribution"]["unmanifestedFitterRows"], 1)
        self.assertEqual(synthesis["remainderAttribution"]["unmanifestedMapRows"], 1)

    def test_fake_runner_retains_failed_timing_only_as_test_receipt(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        workspace = None
        with tempfile.TemporaryDirectory() as temporary:
            repo = Path(temporary)
            self.make_runner_repository(repo)
            fake_bin = self.make_fake_quartus_bin(repo)
            env = dict(os.environ)
            env["ZHAO_TEST_FAIL_TIMING"] = "1"
            completed = subprocess.run(
                [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                 str(repo / "tools/quartus/run_shell_fit.ps1"), "-PythonExe",
                 sys.executable, "-QuartusBin",
                 str(fake_bin), "-TestOnlyFakeQuartus", "-KeepWorkspace",
                 "-Processors", "2"],
                env=env,
                text=True, encoding="utf-8", errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
                timeout=120,
            )
            stdout_text = completed.stdout or ""
            stderr_text = completed.stderr or ""
            diagnostics = stdout_text + stderr_text
            receipt_line = next(
                line for line in diagnostics.splitlines()
                if line.startswith("TEST_ONLY_RECEIPT ")
            )
            workspace_line = next(
                line for line in diagnostics.splitlines()
                if line.startswith("WORKSPACE ")
            )
            workspace = Path(workspace_line.partition(" ")[2])
            synthesis = json.loads(
                Path(receipt_line.partition(" ")[2]).read_bytes()
            )
            production_ledgers = (
                (repo / "reports/synthesis/zhao_shell_fit.json").exists(),
                (repo / "reports/timing/zhao_shell_fit.json").exists(),
            )
        if workspace is not None:
            shutil.rmtree(workspace, ignore_errors=True)
        self.assertEqual(completed.returncode, 2, diagnostics)
        self.assertIn("TEST-ONLY RESULT rejected", diagnostics)
        self.assertNotIn("PASS shell_fit_top_clean_characterization", diagnostics)
        self.assertEqual(production_ledgers, (False, False))
        self.assertEqual(synthesis["evidenceMode"], "test-only")
        self.assertEqual(synthesis["gate"]["status"], "fail")
        self.assertEqual(synthesis["stages"]["fit"], "successful")
        self.assertEqual(synthesis["resources"]["dspBlocks"], 87)

    def test_runner_accepts_real_explicit_python_and_never_requires_quartus(self) -> None:
        powershell = shutil.which("powershell.exe") or shutil.which("pwsh")
        if powershell is None:
            self.skipTest("PowerShell is unavailable")
        completed = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(TOOLS / "run_shell_fit.ps1"),
                "-PreflightOnly",
                "-PythonExe",
                sys.executable,
                "-QuartusBin",
                str(REPO / "definitely-absent-quartus"),
            ],
            text=True, encoding="utf-8", errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout)
        self.assertIn("Quartus not invoked", completed.stdout)
        self.assertIn("sources=56", completed.stdout)

    def test_private_index_archive_git_capture_is_real_and_dirty_sensitive(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source_repo = root / "source-repo"
            source_repo.mkdir()
            commit = self.make_clean_cli_repository(source_repo)
            archive_tar = root / "source.tar"
            archive = root / "archive"
            archive.mkdir()
            completed = subprocess.run(
                [
                    "git",
                    "-C",
                    str(source_repo),
                    "--no-replace-objects",
                    "archive",
                    "--format=tar",
                    f"--output={archive_tar}",
                    commit,
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            with tarfile.open(archive_tar) as bundle:
                bundle.extractall(archive, filter="data")

            env = dict(os.environ)
            env["GIT_DIR"] = str(source_repo / ".git")
            env["GIT_WORK_TREE"] = str(archive)
            env["GIT_INDEX_FILE"] = str(root / "private-index")
            completed = subprocess.run(
                ["git", "-C", str(archive), "--no-replace-objects", "read-tree", commit],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)

            capture = archive / "tools/quartus/capture_shell_fit_git.py"
            clean_dir = archive / "reports/private-index-clean"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(capture),
                    "--repo-root",
                    str(archive),
                    "--output-dir",
                    str(clean_dir),
                ],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            self.assertEqual(list(clean_dir.glob(".*.tmp")), [])
            clean = {
                "gitHead": (clean_dir / "git-head.txt").read_bytes(),
                "gitStatus": (clean_dir / "git-status.txt").read_bytes(),
                "gitWorktreeDiff": (clean_dir / "git-worktree.diff").read_bytes(),
                "gitStagedDiff": (clean_dir / "git-staged.diff").read_bytes(),
                "gitIndexFlags": (clean_dir / "git-index-flags.bin").read_bytes(),
            }
            parsed = parse_git_evidence(
                head=clean["gitHead"],
                status=clean["gitStatus"],
                worktree_diff=clean["gitWorktreeDiff"],
                staged_diff=clean["gitStagedDiff"],
                index_flags=clean["gitIndexFlags"],
            )
            self.assertEqual(parsed.source_commit, commit)

            tracked = archive / "tools/quartus/shell_ports.py"
            tracked.write_bytes(tracked.read_bytes() + b"\n# archive mutation\n")
            dirty_dir = archive / "reports/private-index-dirty"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(capture),
                    "--repo-root",
                    str(archive),
                    "--output-dir",
                    str(dirty_dir),
                ],
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            with self.assertRaisesRegex(ShellPortError, "externally captured.*dirty"):
                parse_git_evidence(
                    head=(dirty_dir / "git-head.txt").read_bytes(),
                    status=(dirty_dir / "git-status.txt").read_bytes(),
                    worktree_diff=(dirty_dir / "git-worktree.diff").read_bytes(),
                    staged_diff=(dirty_dir / "git-staged.diff").read_bytes(),
                    index_flags=(dirty_dir / "git-index-flags.bin").read_bytes(),
                )

    def test_reports_cli_accepts_fully_bound_fixtures(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            temporary_repo = Path(temporary)
            self.make_clean_cli_repository(
                temporary_repo, add_unrelated_tracked_file=True
            )
            output = io.StringIO()
            with redirect_stdout(output), redirect_stderr(output):
                result = reports_main(self.reports_argv(temporary_repo))
        self.assertEqual(result, 0, output.getvalue())
        self.assertIn(
            "clocks=3 map-aluts=11234 receipt=raw-bound", output.getvalue()
        )


class DspCensusShellReceiptSubprocessTests(unittest.TestCase):
    def run_loader(
        self,
        shell_path: Path,
        timing_path: Path | None = None,
        fit_path: Path | None = None,
        map_path: Path | None = None,
    ) -> dict[str, object]:
        if timing_path is None:
            timing_path = shell_path.with_name(shell_path.stem + "-timing.json")
            if shell_path.exists():
                timing_path.write_bytes(shell_path.read_bytes())
        program = (
            "import json,sys;"
            "sys.path.insert(0,sys.argv[1]);"
            "import dsp_census as d;"
            "ev=d.load_evidence(sys.argv[2],sys.argv[3],sys.argv[4],sys.argv[5]);"
            "chosen,why,alts=d.select(ev['zhao_shell_top']);"
            "print(json.dumps({'root': 'zhao_shell_top' in ev,"
            "'priced': chosen is not None,"
            "'dsp': None if chosen is None else chosen.dsp,"
            "'alm': None if chosen is None else chosen.alm,"
            "'policyFailed': False if chosen is None else chosen.policy_failed,"
            "'candidateCount': len(ev['zhao_shell_top']),"
            "'alternates': len(alts),"
            "'why': why}))"
        )
        missing_fit = fit_path or shell_path.parent / "absent-fit.json"
        missing_map = map_path or shell_path.parent / "absent-map.json"
        completed = subprocess.run(
            [sys.executable, "-c", program, str(REPO / "tools/budget"),
             str(missing_fit), str(missing_map), str(shell_path), str(timing_path)],
            cwd=REPO,
            text=True, encoding="utf-8", errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout)
        return json.loads(completed.stdout)

    def test_absent_dirty_malformed_duplicate_and_incomplete_receipts_keep_unknown_root(self) -> None:
        valid = json.loads((FIXTURES / "shell_fit_receipt_v3.json").read_bytes())
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            controls: dict[str, bytes | None] = {
                "absent": None,
                "malformed": b"{not-json",
                "dirty": json.dumps({**valid, "rtlCleanAtHead": False}).encode(),
                "test-only": json.dumps({**valid, "evidenceMode": "test-only"}).encode(),
                "duplicate": json.dumps({
                    **valid, "entities": valid["entities"] + [
                        copy.deepcopy(next(row for row in valid["entities"]
                                           if row["role"] == "shell"))
                    ]
                }).encode(),
                "incomplete": json.dumps({
                    **valid, "stages": {**valid["stages"], "fit": "failed"}
                }).encode(),
            }
            results = {}
            for name, data in controls.items():
                path = root / f"{name}.json"
                if data is not None:
                    path.write_bytes(data)
                results[name] = self.run_loader(path)
        for name, result in results.items():
            with self.subTest(name=name):
                self.assertIs(result["root"], True)
                self.assertIs(result["priced"], False)
                self.assertIsNone(result["dsp"])
                self.assertIsNone(result["alm"])

    def test_generic_shell_rows_never_substitute_or_compete_with_canonical_pair(self) -> None:
        valid = (FIXTURES / "shell_fit_receipt_v3.json").read_bytes()
        generic_fit = {
            "blocks": [{
                "module": "zhao_shell_top",
                "dspBlocks": 1,
                "alms": 2,
                "ramBlocks": 3,
                "registers": 4,
                "status": "ok",
                "sourceCommit": "f" * 40,
                "rtlCleanAtHead": True,
            }]
        }
        generic_map = {
            "blocks": [{
                "module": "zhao_shell_top@plausible-newer",
                "dspBlocks": 2,
                "registers": 5,
                "status": "ok",
                "sourceCommit": "f" * 40,
                "rtlCleanAtHead": True,
            }]
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fit = root / "fit.json"
            mapped = root / "map.json"
            fit.write_text(json.dumps(generic_fit), encoding="utf-8")
            mapped.write_text(json.dumps(generic_map), encoding="utf-8")
            synthesis = root / "shell.json"
            timing = root / "shell-timing.json"

            synthesis.write_bytes(b"{malformed")
            timing.write_bytes(b"{malformed")
            invalid = self.run_loader(synthesis, timing, fit, mapped)

            synthesis.write_bytes(valid)
            timing.write_bytes(valid)
            accepted = self.run_loader(synthesis, timing, fit, mapped)

        self.assertIs(invalid["priced"], False)
        self.assertEqual(invalid["candidateCount"], 1)
        self.assertIsNone(invalid["dsp"])
        self.assertIs(accepted["priced"], True)
        self.assertEqual(accepted["candidateCount"], 1)
        self.assertEqual(accepted["dsp"], 84)

    def test_interrupted_ledger_pair_is_unknown_until_both_halves_match(self) -> None:
        old_bytes = (FIXTURES / "shell_fit_receipt_v3.json").read_bytes()
        new_receipt = json.loads(old_bytes)
        new_receipt["sourceCommit"] = "f" * 40
        new_bytes = (json.dumps(new_receipt, sort_keys=True) + "\n").encode("utf-8")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            synthesis = root / "shell.json"
            timing = root / "shell-timing.json"
            synthesis.write_bytes(new_bytes)
            timing.write_bytes(old_bytes)
            with self.assertRaisesRegex(
                ShellPortError, "mixed content-addressed generations"
            ):
                load_published_receipt_pair(synthesis, timing)
            interrupted = self.run_loader(synthesis, timing)

            timing.write_bytes(new_bytes)
            completed = self.run_loader(synthesis, timing)

        self.assertIs(interrupted["priced"], False)
        self.assertEqual(interrupted["candidateCount"], 1)
        self.assertIs(completed["priced"], True)
        self.assertEqual(completed["dsp"], 84)

    def test_clean_completed_gate_failed_receipt_keeps_resource_row(self) -> None:
        receipt = json.loads((FIXTURES / "shell_fit_receipt_v3.json").read_bytes())
        failures = ["setup: failing=1, worstSlackNs=-0.100"]
        receipt["timing"]["timingPassed"] = False
        receipt["timing"]["gateFailures"] = failures
        receipt["gate"]["status"] = "fail"
        receipt["gate"]["failures"] = failures
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "gate-failed.json"
            path.write_text(json.dumps(receipt), encoding="utf-8")
            result = self.run_loader(path)
        self.assertIs(result["root"], True)
        self.assertIs(result["priced"], True)
        self.assertEqual(result["dsp"], 84)
        self.assertEqual(result["alm"], 7421.6)
        self.assertIs(result["policyFailed"], True)


if __name__ == "__main__":
    unittest.main()
