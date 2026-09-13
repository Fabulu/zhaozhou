#!/usr/bin/env python3
"""Positive-control tests for shell-fit preflight and evidence parsers."""

from __future__ import annotations

import copy
from contextlib import redirect_stderr, redirect_stdout
from decimal import Decimal
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "quartus"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
ARCHIVED_CLOCKS = REPO / "reports" / "characterization" / "clocks.rpt"
ARCHIVED_STA = REPO / "reports" / "characterization" / "zhao_shell_fit.sta.rpt"
ARCHIVED_MAP_SUMMARY = REPO / "reports" / "synthesis" / "blockpaths" / "zhao_geom_project.map.summary"
ARCHIVED_MAP_REPORT = REPO / "reports" / "synthesis" / "blockpaths" / "zhao_geom_project.map.rpt"
ARCHIVED_WRAPPED_WARNING = (
    REPO / "reports" / "composed" / "wumen-ff932e5-20260822T104634Z" / "run.log"
)
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import shell_fit_reports as reports_module
from shell_fit_qsf import parse_cmake_source_pool, parse_qsf, validate_qsf
from shell_fit_reports import (
    bind_receipt_to_evidence,
    main as reports_main,
    parse_clock_constraints,
    parse_fit_summary,
    parse_fitter_hierarchy,
    parse_git_evidence,
    parse_git_index_flags,
    parse_map_hierarchy,
    parse_map_summary,
    parse_receipt,
    parse_timequest_status,
    read_git_blobs_at_commit,
    read_git_index_concealment,
    require_exact_hierarchy_row,
    require_exact_map_hierarchy_row,
    require_shell_hierarchy,
    require_shell_map_hierarchy,
    scan_virtual_clock_warnings,
    validate_clock_constraints,
    validate_fit_summary,
    validate_map_hierarchy,
    validate_map_summary,
    validate_receipt,
    validate_report_messages,
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
                "--sdc",
                str(FIXTURES / "shell_fit_constraints_clean.sdc"),
                "--variable",
                "SHELL_FIT_FIXTURE_RTL",
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout)


class ReportPreflightTests(unittest.TestCase):
    SOURCE_COMMIT = "0123456789abcdef0123456789abcdef01234567"

    def fixture(self, name: str) -> str:
        return (FIXTURES / name).read_text(encoding="utf-8")

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
                text=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
        source_commit = subprocess.run(
            ["git", "-C", str(repo), "rev-parse", "HEAD"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
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
                text=True,
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
        receipt = parse_receipt(self.fixture("shell_fit_receipt_v3.json"))
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
        }
        evidence_path_objects = {
            "summary": FIXTURES / "shell_fit_summary_zero_virtual_pins.txt",
            "sta": ARCHIVED_STA,
            "clocks": ARCHIVED_CLOCKS,
            "hierarchy": FIXTURES / "shell_fit_hierarchy_with_shell.rpt",
            "mapSummary": FIXTURES / "shell_fit_map_clean.summary",
            "mapReport": FIXTURES / "shell_fit_map_clean.rpt",
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
        git_blob_bytes = {
            **source_bytes,
            "generatedRtl": rtl_bytes,
            "manifest": manifest_bytes,
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
            text=True,
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
                "entities do not preserve complete required hierarchy rows",
            ),
            "map entities": (
                lambda row: row["mapEntities"].pop(),
                "mapEntities do not preserve complete required map rows",
            ),
            "remainder": (
                lambda row: row["remainderAttribution"].__setitem__(
                    "method", "subtraction"
                ),
                "remainder attribution mismatch",
            ),
            "limitations": (
                lambda row: row.__setitem__("limitations", []),
                "limitations do not match manifest limitations",
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
                    text=True,
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


if __name__ == "__main__":
    unittest.main()
