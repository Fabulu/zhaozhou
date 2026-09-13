#!/usr/bin/env python3
"""Direct positive/negative tests for check_dual18_map.py.

The reduced fixtures retain genuine Quartus Prime Lite 17.0.2 semicolon-table,
hierarchy, status, and .map.summary formats.  Every accepted fixture is written
to the exact freshly prepared output path and content-hashed as raw evidence.
"""

from __future__ import annotations

import copy
import datetime
import hashlib
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Callable

REPO = Path(__file__).resolve().parents[2]
GENERATOR = REPO / "tools" / "budget" / "gen_calib.py"
PARSER_PATH = REPO / "tools" / "budget" / "check_dual18_map.py"

_spec = importlib.util.spec_from_file_location("check_dual18_map", PARSER_PATH)
assert _spec and _spec.loader
check_map = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = check_map
_spec.loader.exec_module(check_map)


def quartus17_fixture(
    top: str,
    revision: str,
    status: str,
    total: int,
    wrappers: int,
    fixed: int = 2,
) -> str:
    lines = [
        "+-------------------------------------------------------------------------------+",
        "; Analysis & Synthesis Summary                                                  ;",
        "+---------------------------------+---------------------------------------------+",
        "; Analysis & Synthesis Status     ; %-45s ;" % status,
        "; Quartus Prime Version           ; 17.0.2 Build 602 07/19/2017 SJ Lite Edition ;",
        "; Revision Name                   ; %-45s ;" % revision,
        "; Top-level Entity Name           ; %-45s ;" % top,
        "; Family                          ; Cyclone V                                   ;",
        "; Total DSP Blocks                ; %-45d ;" % total,
        "+---------------------------------+---------------------------------------------+",
        "",
        "+---------------------------------------------------------------------------------------------------------------------------+",
        "; Analysis & Synthesis Settings                                                                                             ;",
        "+---------------------------------------------------------------------------------+--------------------+--------------------+",
        "; Option                                                                          ; Setting            ; Default Value      ;",
        "+---------------------------------------------------------------------------------+--------------------+--------------------+",
        "; Device                                                                          ; 5CSEBA6U23I7       ;                    ;",
        "; Top-level entity name                                                           ; %-18s ;                    ;" % top,
        "; Family name                                                                     ; Cyclone V          ; Cyclone V          ;",
        "+---------------------------------------------------------------------------------+--------------------+--------------------+",
        "",
        "+-----------------------------------------------------+",
        "; Analysis & Synthesis Resource Usage Summary         ;",
        "+---------------------------------------------+-------+",
        "; Resource                                    ; Usage ;",
        "+---------------------------------------------+-------+",
        "; Estimate of Logic utilization (ALMs needed) ; 37    ;",
        ";                                             ;       ;",
        "; Combinational ALUT usage for logic          ; 61    ;",
        "; Dedicated logic registers                   ; 178   ;",
        "; Virtual pins                                ; 181   ;",
        "; Total DSP Blocks                            ; %d     ;" % total,
        "+---------------------------------------------+-------+",
        "",
        "+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+",
        "; Analysis & Synthesis Resource Utilization by Entity                                                                                                                                                                                                                                                                                                                       ;",
        "+--------------------------------+---------------------+---------------------------+-------------------+------------+------+--------------+----------------------------------------------+-----------------+--------------+",
        "; Compilation Hierarchy Node     ; Combinational ALUTs ; Dedicated Logic Registers ; Block Memory Bits ; DSP Blocks ; Pins ; Virtual Pins ; Full Hierarchy Name                          ; Entity Name     ; Library Name ;",
        "+--------------------------------+---------------------+---------------------------+-------------------+------------+------+--------------+----------------------------------------------+-----------------+--------------+",
        "; |%-29s ; 61 (4)              ; 178 (12)                  ; 0                 ; %d          ; 0    ; 181          ; |%-44s ; %-15s ; work         ;"
        % (top, total, top, top),
    ]
    for index in range(wrappers):
        instance = "u_dual18" if wrappers == 1 else "u_lane_%s" % ("a" if index == 0 else "b")
        lines.append(
            ";    |zhao_dual18_mul:%-10s ; 0 (0)               ; 0 (0)                     ; 0                 ; 1          ; 0    ; 0            ; |%s|zhao_dual18_mul:%-18s ; zhao_dual18_mul ; work         ;"
            % (instance, top, instance)
        )
    lines += [
        "+--------------------------------+---------------------+---------------------------+-------------------+------------+------+--------------+----------------------------------------------+-----------------+--------------+",
        "",
        "+-------------------------------------------------+",
        "; Analysis & Synthesis DSP Block Usage Summary    ;",
        "+-----------------------------------+-------------+",
        "; Statistic                         ; Number Used ;",
        "+-----------------------------------+-------------+",
        "; Two Independent 18x18             ; %d           ;" % total,
        "; Total number of DSP blocks        ; %d           ;" % total,
        ";                                   ;             ;",
        "; Fixed Point Unsigned Multiplier   ; %d           ;" % fixed,
        "+-----------------------------------+-------------+",
        "",
    ]
    return "\r\n".join(lines)


def quartus17_map_summary(
    status: str, revision: str, top: str, total: int
) -> str:
    return "\n".join(
        [
            "Analysis & Synthesis Status : " + status,
            "Quartus Prime Version : 17.0.2 Build 602 07/19/2017 SJ Lite Edition",
            "Revision Name : " + revision,
            "Top-level Entity Name : " + top,
            "Family : Cyclone V",
            "Logic utilization (in ALMs) : N/A",
            "Total DSP Blocks : %d" % total,
            "",
        ]
    )


def create_directory_indirection(link: Path, target: Path, kind: str) -> None:
    """Create a directory symlink or Windows junction for a negative control."""
    if kind == "symlink":
        os.symlink(target, link, target_is_directory=True)
        return
    if kind != "junction":
        raise ValueError("unknown directory indirection kind: %s" % kind)
    if os.name != "nt":
        raise OSError("Windows junctions are unavailable on %s" % os.name)
    completed = subprocess.run(
        ["cmd.exe", "/d", "/c", "mklink", "/J", str(link), str(target)],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    output = completed.stdout.decode(errors="replace")
    if completed.returncode or not link.exists():
        raise OSError("mklink /J failed: %s" % output.strip())


def remove_directory_indirection(link: Path) -> None:
    """Remove only the link/junction, never its target directory."""
    try:
        link.unlink()
    except OSError:
        os.rmdir(link)


class Dual18MapParserTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = tempfile.TemporaryDirectory(prefix="dual18-map-parser-")
        cls.out = Path(cls.temp.name)
        completed = subprocess.run(
            [sys.executable, str(GENERATOR), "--dual18-only", "--outdir", str(cls.out)],
            cwd=REPO,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if completed.returncode:
            raise RuntimeError(completed.stdout)

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def config(self, base_revision: str) -> dict:
        path = self.out / "dual18" / base_revision / "effective_config.json"
        return self.attach_anchor(check_map.load_config(path), self.out)

    @staticmethod
    def anchor_values(out: Path) -> tuple[Path, bytes, dict]:
        # Keep the canonical lexical name: resolving it would hide precisely the
        # symlink/junction attack this suite exercises.
        path = check_map.lexical_absolute(out / "dual18_invocation_anchor.json")
        raw = path.read_bytes()
        return path, raw, json.loads(raw.decode("utf-8"))

    @classmethod
    def attach_anchor(cls, config: dict, out: Path) -> dict:
        path, raw, anchor = cls.anchor_values(out)
        config["_invocationAnchorPath"] = str(path)
        config["_invocationAnchorSha256"] = hashlib.sha256(raw).hexdigest()
        config["_invocationNonce"] = anchor["invocationNonce"]
        config["_expectedManifestSha256"] = anchor["manifestSha256"]
        return config

    def evidence(
        self,
        base_revision: str,
        total: int,
        wrappers: int,
        fixed: int = 2,
        transform: Callable[[str], str] | None = None,
        completed_seconds: int = 2,
        config: dict | None = None,
    ) -> tuple[str, dict]:
        if config is None:
            config = self.config(base_revision)
        config_path = Path(config["_configPath"])
        preparation = json.loads(
            (config_path.parent / config["runPreparation"]["file"]).read_text(encoding="utf-8")
        )
        prepared_local = datetime.datetime.fromisoformat(preparation["preparedAtLocal"])
        anchor = json.loads(Path(config["_invocationAnchorPath"]).read_text(encoding="utf-8"))
        anchor_local = datetime.datetime.fromisoformat(anchor["createdAtLocal"])
        completion_base = max(prepared_local, anchor_local)
        completed = completion_base.replace(microsecond=0) + datetime.timedelta(
            seconds=completed_seconds
        )
        status = "Successful - " + completed.strftime("%a %b %d %H:%M:%S %Y")
        text = quartus17_fixture(
            config["top"], config["revision"], status, total, wrappers, fixed
        )
        if transform is not None:
            text = transform(text)

        report_path = (config_path.parent / preparation["expectedMapReport"]).resolve()
        summary_path = (config_path.parent / preparation["expectedMapSummary"]).resolve()
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_raw = text.encode("utf-8")
        report_path.write_bytes(report_raw)
        summary_path.write_text(
            quartus17_map_summary(status, config["revision"], config["top"], total),
            encoding="utf-8",
            newline="\n",
        )
        # The raw files are newly created after the empty-output receipt.  Pinning
        # mtimes just after it makes the invariant deterministic across filesystems.
        fresh_ns = max(
            preparation["preparedAtUnixNs"], anchor["createdAtUnixNs"]
        ) + 1_000_000_000
        os.utime(report_path, ns=(fresh_ns, fresh_ns))
        os.utime(summary_path, ns=(fresh_ns, fresh_ns))
        config["_reportPath"] = str(report_path)
        config["_reportSha256"] = hashlib.sha256(report_raw).hexdigest()
        return text, config

    def test_correct_explicit_and_wide_reports_pass_all_three_tables(self) -> None:
        for variant, base in (
            ("explicit", "dual18_explicit_pair"),
            ("s32x18", "dual18_s32x18_exact"),
        ):
            with self.subTest(variant=variant):
                text, config = self.evidence(base, total=1, wrappers=1)
                result = check_map.evaluate(text, config, variant)
                self.assertEqual(result["status"], "hold")
                self.assertEqual(result["oneBlockResourceGate"], "pass")
                self.assertTrue(result["distinctSourceOutputRoutes"])
                self.assertEqual(result["mappedOutputRouteEvidence"]["status"], "hold")
                self.assertNotIn("distinctMappedOutputRoutes", result)
                self.assertEqual(result["resource_dsp"], 1)
                self.assertEqual(result["entity_top_dsp"], 1)
                self.assertEqual(result["wrapper_dsp"], 1)
                self.assertEqual(result["independent_mode"], 1)
                self.assertEqual(result["fixed_multiplier_rows"], 2)
                self.assertEqual(result["runEvidence"]["contentWitness"], config["contentWitness"])
                self.assertEqual(result["runEvidence"]["reportSha256"], config["_reportSha256"])

    def test_each_one_block_revision_requires_exactly_two_logical_multipliers(self) -> None:
        for variant, base in (
            ("explicit", "dual18_explicit_pair"),
            ("s32x18", "dual18_s32x18_exact"),
        ):
            for fixed in (0, 1, 2, 3):
                with self.subTest(variant=variant, logical_multipliers=fixed):
                    text, config = self.evidence(base, total=1, wrappers=1, fixed=fixed)
                    if fixed == 2:
                        result = check_map.evaluate(text, config, variant)
                        self.assertEqual(result["oneBlockResourceGate"], "pass")
                        self.assertEqual(result["status"], "hold")
                    else:
                        with self.assertRaisesRegex(
                            check_map.GateError,
                            "exactly two live logical multipliers",
                        ):
                            check_map.evaluate(text, config, variant)

    def test_inferred_two_is_expected_and_one_is_good_news(self) -> None:
        text, config = self.evidence("dual18_inferred_pair", 2, wrappers=0)
        expected = check_map.evaluate(text, config, "inferred")
        text, config = self.evidence("dual18_inferred_pair", 1, wrappers=0, fixed=2)
        good_news = check_map.evaluate(text, config, "inferred")
        self.assertEqual(expected["interpretation"], "expected contrast")
        self.assertEqual(good_news["interpretation"], "one is good news")

    def test_two_primitive_mutant_proves_one_block_detector_fires(self) -> None:
        text, config = self.evidence("dual18_two_primitives_mutant", 2, wrappers=2)
        result = check_map.evaluate(text, config, "two-primitives-mutant")
        self.assertEqual(result["positiveControl"], "one-block detector fired")
        parsed = check_map.parse_report(text, "dual18_two_primitives_mutant")
        with self.assertRaisesRegex(check_map.GateError, "expected total DSP=1"):
            check_map.check_one_block(parsed)

    def test_disagreeing_tables_missing_hierarchy_and_wrong_mode_reject(self) -> None:
        def disagreeing(text: str) -> str:
            return text.replace(
                "; Total number of DSP blocks        ; 1           ;",
                "; Total number of DSP blocks        ; 2           ;",
            )

        text, config = self.evidence(
            "dual18_explicit_pair", 1, wrappers=1, transform=disagreeing
        )
        with self.assertRaisesRegex(check_map.GateError, "DSP totals disagree"):
            check_map.evaluate(text, config, "explicit")

        text, config = self.evidence("dual18_explicit_pair", 1, wrappers=0)
        with self.assertRaisesRegex(check_map.GateError, "one wrapper row"):
            check_map.evaluate(text, config, "explicit")

        def no_independent_mode(text: str) -> str:
            return text.replace(
                "; Two Independent 18x18             ; 1           ;",
                "; Two Independent 18x18             ; 0           ;",
            )

        text, config = self.evidence(
            "dual18_explicit_pair", 1, wrappers=1, transform=no_independent_mode
        )
        with self.assertRaisesRegex(check_map.GateError, "expected one"):
            check_map.evaluate(text, config, "explicit")

    def test_duplicate_table_rejects(self) -> None:
        config = self.config("dual18_explicit_pair")
        status = "Successful - Sun Sep 13 12:00:00 2026"
        text = quartus17_fixture(
            config["top"], config["revision"], status, 1, wrappers=1
        )
        with self.assertRaisesRegex(check_map.GateError, "found 2"):
            check_map.parse_report(text + text, config["top"])

    def test_report_tool_device_and_content_revision_are_bound(self) -> None:
        text, config = self.evidence(
            "dual18_explicit_pair",
            1,
            wrappers=1,
            transform=lambda value: value.replace("5CSEBA6U23I7", "5CSEBA5U23I7"),
        )
        with self.assertRaisesRegex(check_map.GateError, "report device"):
            check_map.evaluate(text, config, "explicit")

        text, config = self.evidence(
            "dual18_explicit_pair",
            1,
            wrappers=1,
            transform=lambda value: value.replace("17.0.2 Build 602", "18.1.0 Build 625"),
        )
        with self.assertRaisesRegex(check_map.GateError, "not from Quartus"):
            check_map.evaluate(text, config, "explicit")

        config = self.config("dual18_explicit_pair")
        witness_revision = config["revision"]
        base_revision = config["baseRevision"]
        text, config = self.evidence(
            "dual18_explicit_pair",
            1,
            wrappers=1,
            transform=lambda value: value.replace(witness_revision, base_revision),
            config=config,
        )
        with self.assertRaisesRegex(check_map.GateError, "report revision"):
            check_map.evaluate(text, config, "explicit")

    def test_stale_report_completion_positive_control_fires(self) -> None:
        text, config = self.evidence(
            "dual18_explicit_pair", 1, wrappers=1, completed_seconds=-2
        )
        with self.assertRaisesRegex(check_map.GateError, "completion predates"):
            check_map.evaluate(text, config, "explicit")

    def test_synthetic_route_claim_cannot_replace_mapped_artifact(self) -> None:
        invented = (
            "\r\n; Dual18 Mapped Route Evidence ;\r\n"
            "; resulta ; mapped_node_a ;\r\n"
            "; resultb ; mapped_node_b ;\r\n"
        )
        text, config = self.evidence(
            "dual18_explicit_pair",
            1,
            wrappers=1,
            transform=lambda value: value + invented,
        )
        result = check_map.evaluate(text, config, "explicit")
        self.assertEqual(result["oneBlockResourceGate"], "pass")
        self.assertEqual(result["mappedOutputRouteEvidence"]["status"], "hold")
        self.assertEqual(result["status"], "hold")
        self.assertNotIn("distinctMappedOutputRoutes", result)

    def test_invocation_anchor_rejects_missing_forged_wrong_location_and_mismatch(self) -> None:
        bare_path = self.out / "dual18" / "dual18_explicit_pair" / "effective_config.json"
        bare = check_map.load_config(bare_path)
        with self.assertRaisesRegex(check_map.GateError, "anchor arguments are required"):
            check_map.check_invocation_anchor(bare)

        anchor_path, anchor_raw, _ = self.anchor_values(self.out)
        wrong_path = self.out / "dual18" / "forged_anchor.json"
        wrong_path.write_bytes(anchor_raw)
        wrong = self.config("dual18_explicit_pair")
        wrong["_invocationAnchorPath"] = str(wrong_path)
        try:
            with self.assertRaisesRegex(check_map.GateError, "canonical location"):
                check_map.check_invocation_anchor(wrong)
        finally:
            wrong_path.unlink()

        lexical_alias = self.config("dual18_explicit_pair")
        lexical_alias["_invocationAnchorPath"] = str(
            self.out / "dual18" / ".." / "dual18_invocation_anchor.json"
        )
        with self.assertRaisesRegex(check_map.GateError, "canonical lexical path"):
            check_map.check_invocation_anchor(lexical_alias)

        mismatch = self.config("dual18_explicit_pair")
        mismatch["_expectedManifestSha256"] = "0" * 64
        with self.assertRaisesRegex(check_map.GateError, "manifest hash differs"):
            check_map.check_invocation_anchor(mismatch)

        nonce_mismatch = self.config("dual18_explicit_pair")
        nonce_mismatch["_invocationNonce"] = "0" * 64
        with self.assertRaisesRegex(check_map.GateError, "nonce differs"):
            check_map.check_invocation_anchor(nonce_mismatch)

        forged_doc = json.loads(anchor_raw.decode("utf-8"))
        forged_doc["invocationNonce"] = "f" * 64
        try:
            anchor_path.write_text(
                json.dumps(forged_doc, indent=2) + "\n", encoding="utf-8", newline="\n"
            )
            forged = self.config("dual18_explicit_pair")
            forged["_invocationAnchorSha256"] = hashlib.sha256(anchor_raw).hexdigest()
            forged["_invocationNonce"] = json.loads(anchor_raw)["invocationNonce"]
            with self.assertRaisesRegex(check_map.GateError, "anchor hash differs"):
                check_map.check_invocation_anchor(forged)
        finally:
            anchor_path.write_bytes(anchor_raw)

        missing = self.config("dual18_explicit_pair")
        missing_path = anchor_path.with_name(anchor_path.name + ".temporarily-missing")
        anchor_path.replace(missing_path)
        try:
            with self.assertRaisesRegex(check_map.GateError, "anchor is missing"):
                check_map.check_invocation_anchor(missing)
        finally:
            missing_path.replace(anchor_path)

    def test_anchor_file_symlink_into_candidate_tree_is_rejected(self) -> None:
        anchor_path, anchor_raw, _ = self.anchor_values(self.out)
        target = self.out / "dual18" / "anchor-symlink-target.json"
        anchor_path.replace(target)
        linked = False
        try:
            try:
                os.symlink(target, anchor_path)
                linked = True
            except OSError as exc:
                target.replace(anchor_path)
                self.skipTest("anchor-file symlink creation unavailable: %s" % exc)
            config_path = (
                self.out / "dual18" / "dual18_explicit_pair" / "effective_config.json"
            )
            attacked = self.attach_anchor(check_map.load_config(config_path), self.out)
            with self.assertRaisesRegex(
                check_map.GateError, "symlink, junction, or reparse-point indirection"
            ):
                check_map.check_invocation_anchor(attacked)
        finally:
            if linked:
                anchor_path.unlink()
            if target.exists():
                target.replace(anchor_path)
        self.assertEqual(anchor_path.read_bytes(), anchor_raw)

    def test_manifest_file_symlink_is_rejected(self) -> None:
        config = self.config("dual18_explicit_pair")
        manifest_path = self.out / "dual18" / "dual18_manifest.json"
        manifest_raw = manifest_path.read_bytes()
        target = self.out / "dual18" / "manifest-symlink-target.json"
        manifest_path.replace(target)
        linked = False
        try:
            try:
                os.symlink(target, manifest_path)
                linked = True
            except OSError as exc:
                target.replace(manifest_path)
                self.skipTest("manifest-file symlink creation unavailable: %s" % exc)
            with self.assertRaisesRegex(
                check_map.GateError, "symlink, junction, or reparse-point indirection"
            ):
                check_map.check_invocation_anchor(config)
        finally:
            if linked:
                manifest_path.unlink()
            if target.exists():
                target.replace(manifest_path)
        self.assertEqual(manifest_path.read_bytes(), manifest_raw)

    def test_intermediate_evidence_directory_indirection_is_rejected(self) -> None:
        tree = self.out / "dual18"
        for kind in ("symlink", "junction"):
            with self.subTest(kind=kind):
                target = self.out / ("dual18-%s-target" % kind)
                tree.replace(target)
                linked = False
                try:
                    try:
                        create_directory_indirection(tree, target, kind)
                        linked = True
                    except OSError as exc:
                        target.replace(tree)
                        raise unittest.SkipTest(
                            "%s creation unavailable: %s" % (kind, exc)
                        )
                    config_path = (
                        tree / "dual18_explicit_pair" / "effective_config.json"
                    )
                    attacked = self.attach_anchor(
                        check_map.load_config(config_path), self.out
                    )
                    with self.assertRaisesRegex(
                        check_map.GateError,
                        "symlink, junction, or reparse-point indirection",
                    ):
                        check_map.check_invocation_anchor(attacked)
                finally:
                    if linked:
                        remove_directory_indirection(tree)
                    if target.exists():
                        target.replace(tree)

    def test_old_anchor_timestamp_rejects_even_with_recomputed_anchor_hash(self) -> None:
        anchor_path, anchor_raw, anchor = self.anchor_values(self.out)
        config = self.config("dual18_explicit_pair")
        preparation = json.loads(
            (Path(config["_configPath"]).parent / "run_preparation.json").read_text(
                encoding="utf-8"
            )
        )
        old_ns = preparation["preparedAtUnixNs"] - 1_000_000_000
        old_local = datetime.datetime.fromtimestamp(old_ns / 1_000_000_000).astimezone()
        anchor["createdAtUnixNs"] = old_ns
        anchor["createdAtLocal"] = old_local.isoformat(timespec="microseconds")
        try:
            anchor_path.write_text(
                json.dumps(anchor, indent=2) + "\n", encoding="utf-8", newline="\n"
            )
            old = self.attach_anchor(check_map.load_config(Path(config["_configPath"])), self.out)
            with self.assertRaisesRegex(check_map.GateError, "older than candidate preparation"):
                check_map.inspect_effective_sources(old, "explicit")
        finally:
            anchor_path.write_bytes(anchor_raw)

    def test_full_chain_rollback_rejected_by_current_external_anchor(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dual18-full-rollback-") as temp:
            workspace = Path(temp)
            out = workspace / "calib"

            def generate() -> None:
                completed = subprocess.run(
                    [sys.executable, str(GENERATOR), "--dual18-only", "--outdir", str(out)],
                    cwd=REPO,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                    check=False,
                )
                self.assertEqual(completed.returncode, 0, completed.stdout)

            generate()
            base = "dual18_explicit_pair"
            old_config = self.attach_anchor(
                check_map.load_config(out / "dual18" / base / "effective_config.json"), out
            )
            old_text, old_config = self.evidence(
                base, total=1, wrappers=1, config=old_config
            )
            old_tree = workspace / "old_dual18"
            shutil.copytree(out / "dual18", old_tree)
            old_revision = old_config["revision"]

            generate()
            anchor_path, current_anchor_raw, current_anchor = self.anchor_values(out)
            new_config = json.loads(
                (out / "dual18" / base / "effective_config.json").read_text(encoding="utf-8")
            )
            self.assertEqual(new_config["revision"], old_revision)
            shutil.rmtree(out / "dual18")
            shutil.copytree(old_tree, out / "dual18")
            self.assertEqual(anchor_path.read_bytes(), current_anchor_raw)

            rolled_back = self.attach_anchor(
                check_map.load_config(out / "dual18" / base / "effective_config.json"), out
            )
            report_path = Path(old_config["_reportPath"])
            rolled_back["_reportPath"] = str(report_path)
            rolled_back["_reportSha256"] = hashlib.sha256(report_path.read_bytes()).hexdigest()
            self.assertEqual(
                rolled_back["_expectedManifestSha256"], current_anchor["manifestSha256"]
            )
            with self.assertRaisesRegex(
                check_map.GateError, "candidate manifest hash differs from independent"
            ):
                check_map.evaluate(old_text, rolled_back, "explicit")
            self.assertEqual(anchor_path.read_bytes(), current_anchor_raw)

    def test_manifest_rejects_restored_preparation_and_comutated_config_hash(self) -> None:
        text, config = self.evidence("dual18_explicit_pair", 1, wrappers=1)
        config_path = Path(config["_configPath"])
        preparation_path = config_path.parent / config["runPreparation"]["file"]
        original_config = config_path.read_bytes()
        original_preparation = preparation_path.read_bytes()
        try:
            disk_config = json.loads(original_config.decode("utf-8"))
            old_preparation = json.loads(original_preparation.decode("utf-8"))
            old_local = datetime.datetime.fromisoformat(old_preparation["preparedAtLocal"])
            old_preparation["preparedAtLocal"] = (
                old_local - datetime.timedelta(days=1)
            ).isoformat(timespec="microseconds")
            old_preparation["preparedAtUnixNs"] -= 24 * 60 * 60 * 1_000_000_000
            preparation_path.write_text(
                json.dumps(old_preparation, indent=2) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            disk_config["runPreparation"]["sha256"] = hashlib.sha256(
                preparation_path.read_bytes()
            ).hexdigest()
            config_path.write_text(
                json.dumps(disk_config, indent=2) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            attacked = self.attach_anchor(check_map.load_config(config_path), self.out)
            attacked["_reportPath"] = config["_reportPath"]
            attacked["_reportSha256"] = config["_reportSha256"]
            with self.assertRaisesRegex(
                check_map.GateError, "manifest/(?:runPreparation|effectiveConfig) hash mismatch"
            ):
                check_map.evaluate(text, attacked, "explicit")
        finally:
            preparation_path.write_bytes(original_preparation)
            config_path.write_bytes(original_config)

    def test_vendor_evidence_is_canonical_recomputed_and_content_bound(self) -> None:
        config = self.config("dual18_explicit_pair")
        self.assertEqual(
            config["witnessInputs"]["vendorInterfaceEvidence"],
            config["vendorInterfaceEvidence"],
        )
        holds, normalized = check_map.validate_vendor_evidence(config)
        self.assertEqual(normalized, config["vendorInterfaceEvidence"])
        if all(Path(spec["path"]).is_file() for spec in check_map.VENDOR_EVIDENCE_SPECS):
            self.assertEqual(holds, [])

        arbitrary = copy.deepcopy(config)
        arbitrary["vendorInterfaceEvidence"][0]["path"] = str(
            self.out / "arbitrary-cyclonev-atoms.v"
        )
        with self.assertRaisesRegex(check_map.GateError, "canonical atom-declaration path"):
            check_map.validate_vendor_evidence(arbitrary)

        forged = copy.deepcopy(config)
        row = forged["vendorInterfaceEvidence"][0]
        if row["available"]:
            row["matchedExcerpts"][0] = "module cyclonev_mac_forged("
        else:
            row["requiredPatterns"][0] = r"module\s+anything"
        with self.assertRaisesRegex(check_map.GateError, "differ|malformed"):
            check_map.validate_vendor_evidence(forged)

    def test_qsf_is_exact_and_all_extra_compile_mechanisms_fire(self) -> None:
        config = self.config("dual18_explicit_pair")
        spec = check_map.VARIANTS["explicit"]
        absolute_sources = [
            Path(row["absolutePath"]).resolve().as_posix() for row in config["sources"]
        ]
        qsf = (Path(config["_configPath"]).parent / config["qsfFile"]).read_text(
            encoding="ascii"
        )
        check_map.check_exact_qsf(qsf, spec, absolute_sources)

        for assignment in (
            "VERILOG_FILE",
            "VHDL_FILE",
            "QIP_FILE",
            "IP_FILE",
            "SIP_FILE",
            "EDIF_FILE",
            "AHDL_FILE",
            "BDF_FILE",
            "QSYS_FILE",
            "SYSTEMVERILOG_FILE",
        ):
            with self.subTest(assignment=assignment):
                extra = qsf + (
                    'set_global_assignment -name %s "C:/unaccounted/source"\n'
                    % assignment
                )
                with self.assertRaisesRegex(check_map.GateError, "exact generated source"):
                    check_map.check_exact_qsf(extra, spec, absolute_sources)

        first_source = (
            'set_global_assignment -name SYSTEMVERILOG_FILE "%s"\n'
            % absolute_sources[0]
        )
        with self.assertRaisesRegex(check_map.GateError, "exact generated source"):
            check_map.check_exact_qsf(qsf.replace(first_source, "", 1), spec, absolute_sources)

    def test_cli_hashes_raw_report_and_writes_content_bound_receipt(self) -> None:
        _, config = self.evidence("dual18_explicit_pair", 1, wrappers=1)
        config_path = Path(config["_configPath"])
        receipt_path = config_path.parent / "validated_test_receipt.json"
        completed = subprocess.run(
            [
                sys.executable,
                str(PARSER_PATH),
                "--variant",
                "explicit",
                "--report",
                config["_reportPath"],
                "--effective-config",
                str(config_path),
                "--invocation-anchor",
                config["_invocationAnchorPath"],
                "--invocation-anchor-sha256",
                config["_invocationAnchorSha256"],
                "--invocation-nonce",
                config["_invocationNonce"],
                "--manifest-sha256",
                config["_expectedManifestSha256"],
                "--receipt",
                str(receipt_path),
            ],
            cwd=REPO,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout)
        receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
        self.assertEqual(receipt["runEvidence"]["reportSha256"], config["_reportSha256"])
        self.assertEqual(receipt["runEvidence"]["contentWitness"], config["contentWitness"])
        self.assertEqual(
            receipt["runEvidence"]["invocationAnchorSha256"],
            config["_invocationAnchorSha256"],
        )
        self.assertEqual(
            receipt["runEvidence"]["invocationNonce"], config["_invocationNonce"]
        )
        manifest_path = config_path.parent.parent / "dual18_manifest.json"
        self.assertEqual(
            receipt["runEvidence"]["manifestSha256"],
            hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        )
        self.assertEqual(
            receipt["runEvidence"]["effectiveConfigSha256"],
            hashlib.sha256(config_path.read_bytes()).hexdigest(),
        )
        self.assertEqual(receipt["status"], "hold")

    def test_absent_quartus_metadata_is_an_honest_hold(self) -> None:
        fake_specs = []
        evidence = []
        for index, spec in enumerate(check_map.VENDOR_EVIDENCE_SPECS):
            fake = copy.deepcopy(spec)
            fake["path"] = str(self.out / "not-installed" / ("vendor-%d" % index))
            fake_specs.append(fake)
            evidence.append(
                {
                    "kind": fake["kind"],
                    "path": fake["path"],
                    "available": False,
                    "requiredPatterns": list(fake["requiredPatterns"]),
                }
            )
        holds, normalized = check_map.validate_vendor_evidence(
            {"vendorInterfaceEvidence": evidence}, specs=fake_specs
        )
        self.assertEqual(len(holds), 2)
        self.assertEqual(normalized, evidence)
        self.assertNotIn("matchedExcerpts", normalized[0])

    def test_effective_macro_receipt_cannot_silently_select_behavioral(self) -> None:
        config = copy.deepcopy(self.config("dual18_explicit_pair"))
        config["macros"] = ["ZHAO_DUAL18_BEHAVIORAL=1"]
        with self.assertRaisesRegex(check_map.GateError, "effective macro set mismatch"):
            check_map.inspect_effective_sources(config, "explicit")


if __name__ == "__main__":
    unittest.main(verbosity=2)
