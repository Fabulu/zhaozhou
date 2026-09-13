#!/usr/bin/env python3
"""No-Quartus controls for the dual18 fresh-map/CDB route gate.

All TSVs and process outputs in this test are explicitly synthetic mechanics
fixtures.  A fake executor exercises ordering and provenance without starting
Quartus.  Synthetic fixtures are never accepted as physical evidence by the
public synthetic API.
"""

from __future__ import annotations

import copy
import datetime
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
GENERATOR = REPO / "tools" / "budget" / "gen_calib.py"
CHECKER_PATH = REPO / "tools" / "budget" / "check_dual18_atom_routes.py"
_spec = importlib.util.spec_from_file_location("check_dual18_atom_routes", CHECKER_PATH)
assert _spec and _spec.loader
routes = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = routes
_spec.loader.exec_module(routes)


def port_row(
    node: str,
    direction: str,
    port_id: str,
    port_type: str,
    index: int,
    name: str = "",
) -> str:
    return "\t".join(
        ["port", node, direction, port_id, port_type, str(index), name, "0"]
    )


def edge_rows(source_node: str, source_port: str, destination_node: str, destination_port: str) -> list[str]:
    stem = "edge\t%s\t%s\t%s\t%s\t" % (
        source_node, source_port, destination_node, destination_port
    )
    return [stem + "fanin", stem + "fanout"]


def synthetic_graph(
    kind: str = "correct",
    *,
    artifact_class: str = "synthetic-nonphysical-parser-fixture",
    synthetic: bool = True,
    capture_id: str = "a" * 64,
    project: str = "C:/synthetic/nonphysical",
    revision: str = "dual18_synthetic_nonphysical",
    quartus_version: str = "SYNTHETIC - NOT QUARTUS",
    generated: int | None = None,
) -> str:
    now = int(time.time()) if generated is None else generated
    lines = [
        "schema\tdual18-atom-route-tsv\t2",
        "meta\tartifact_class\t" + artifact_class,
        "meta\tsynthetic\t" + ("true" if synthetic else "false"),
        "meta\tcapture_id\t" + capture_id,
        "meta\tproject\t" + project,
        "meta\trevision\t" + revision,
        "meta\tnetlist_type\tmap",
        "meta\tquartus_version\t" + quartus_version,
        "meta\tgenerated_unix_seconds\t%d" % now,
        "meta\toptional_vo_status\tunavailable",
        "meta\tconnectivity_complete\t1",
        "node\tatom0\tMAC_MULT\t0\tu_dual18",
    ]
    if kind == "two-atom":
        lines.append("node\tatom1\tMAC_MULT\t0\tu_dual18_second")

    for family, width in routes.INPUT_FAMILIES.items():
        for bit in range(width):
            pin = "%s_pin_%d" % (family.lower(), bit)
            signal = "%s_i[%d]" % (family.lower(), bit)
            atom_port = "%s_%d" % (family.lower(), bit)
            node_type = (
                "LUT" if kind == "boundary-node-not-pin" and family == "AX" and bit == 0
                else "PIN"
            )
            port_type = (
                "DATAOUT" if kind == "boundary-port-not-padio" and family == "AX" and bit == 0
                else "PADIO"
            )
            lines.append("node\t%s\t%s\t0\t%s~input" % (pin, node_type, signal))
            lines.append(port_row(pin, "oport", "pad", port_type, 0, signal))
            lines.append(port_row("atom0", "iport", atom_port, family, bit))
            if kind == "control-crossing" and family == "AX" and bit == 0:
                lines += [
                    "node\tctrl_ff\tDFF\t0\toperand_control_ff",
                    port_row("ctrl_ff", "iport", "clk", "CLK", 0),
                    port_row("ctrl_ff", "oport", "q", "Q", 0),
                ]
                lines += edge_rows(pin, "pad", "ctrl_ff", "clk")
                lines += edge_rows("ctrl_ff", "q", "atom0", atom_port)
            else:
                lines += edge_rows(pin, "pad", "atom0", atom_port)

    omitted = ("RESULTB", 35) if kind == "missing-port" else None
    for family, width in routes.OUTPUT_FAMILIES.items():
        for bit in range(width):
            if omitted == (family, bit):
                continue
            atom_port = "%s_%d" % (family.lower(), bit)
            lines.append(
                port_row(
                    "atom0", "oport", atom_port, family, bit,
                    "%s_net_%d" % (family, bit),
                )
            )

    for logical_family, width in routes.OUTPUT_FAMILIES.items():
        for bit in range(width):
            pin = "%s_pin_%d" % (logical_family.lower(), bit)
            signal = "%s_o[%d]" % (logical_family.lower(), bit)
            lines.append("node\t%s\tPIN\t0\t%s~output" % (pin, signal))
            lines.append(port_row(pin, "iport", "pad", "PADIO", 0))
            if omitted == (logical_family, bit):
                continue
            source_family = logical_family
            if kind == "swapped":
                source_family = "RESULTB" if logical_family == "RESULTA" else "RESULTA"
            elif kind == "collapsed":
                source_family = "RESULTA"
            source_port = "%s_%d" % (source_family.lower(), bit)
            if kind in ("sequential", "multi-hop") and logical_family == "RESULTA" and bit == 0:
                middle = "result_ff" if kind == "sequential" else "result_lut"
                middle_type = "DFF" if kind == "sequential" else "LUT"
                lines += [
                    "node\t%s\t%s\t0\tresult_middle" % (middle, middle_type),
                    port_row(middle, "iport", "d", "D", 0),
                    port_row(middle, "oport", "q", "Q", 0),
                ]
                lines += edge_rows("atom0", source_port, middle, "d")
                lines += edge_rows(middle, "q", pin, "pad")
            else:
                rows = edge_rows("atom0", source_port, pin, "pad")
                if kind == "one-sided" and logical_family == "RESULTA" and bit == 0:
                    rows = rows[:1]
                lines += rows

    # A genuine collapse control retains RESULTB with an independent top sink;
    # it does not depend on preserve applied to a fanout-free wire.
    if kind == "collapsed":
        for bit in range(routes.OUTPUT_FAMILIES["RESULTB"]):
            pin = "wrong_resultb_pin_%d" % bit
            signal = "resultb_wrong_sink_o[%d]" % bit
            lines.append("node\t%s\tPIN\t0\t%s~output" % (pin, signal))
            lines.append(port_row(pin, "iport", "pad", "PADIO", 0))
            lines += edge_rows("atom0", "resultb_%d" % bit, pin, "pad")

    if kind == "hidden":
        lines.append("unavailable\tencrypted_node\tatom0\tu_dual18")
    return "\n".join(lines) + "\n"


def quartus17_fixture(top: str, revision: str, status: str, total: int, wrappers: int) -> str:
    lines = [
        "; Analysis & Synthesis Summary ;",
        "; Analysis & Synthesis Status ; %s ;" % status,
        "; Quartus Prime Version ; 17.0.2 Build 602 07/19/2017 SJ Lite Edition ;",
        "; Revision Name ; %s ;" % revision,
        "; Top-level Entity Name ; %s ;" % top,
        "; Family ; Cyclone V ;",
        "; Total DSP Blocks ; %d ;" % total,
        "",
        "; Analysis & Synthesis Settings ;",
        "; Option ; Setting ; Default Value ;",
        "; Device ; 5CSEBA6U23I7 ; ;",
        "; Top-level entity name ; %s ; ;" % top,
        "; Family name ; Cyclone V ; Cyclone V ;",
        "",
        "; Analysis & Synthesis Resource Usage Summary ;",
        "; Resource ; Usage ;",
        "; Estimate of Logic utilization (ALMs needed) ; 37 ;",
        "; Combinational ALUT usage for logic ; 61 ;",
        "; Dedicated logic registers ; 178 ;",
        "; Virtual pins ; 181 ;",
        "; Total DSP Blocks ; %d ;" % total,
        "",
        "; Analysis & Synthesis Resource Utilization by Entity ;",
        "; Compilation Hierarchy Node ; Combinational ALUTs ; Dedicated Logic Registers ; Block Memory Bits ; DSP Blocks ; Pins ; Virtual Pins ; Full Hierarchy Name ; Entity Name ; Library Name ;",
        "; |%s ; 61 (4) ; 178 (12) ; 0 ; %d ; 0 ; 181 ; |%s ; %s ; work ;" % (top, total, top, top),
    ]
    for index in range(wrappers):
        instance = "u_dual18" if wrappers == 1 else "u_lane_%s" % ("a" if index == 0 else "b")
        lines.append(
            "; |zhao_dual18_mul:%s ; 0 (0) ; 0 (0) ; 0 ; 1 ; 0 ; 0 ; |%s|zhao_dual18_mul:%s ; zhao_dual18_mul ; work ;"
            % (instance, top, instance)
        )
    lines += [
        "",
        "; Analysis & Synthesis DSP Block Usage Summary ;",
        "; Statistic ; Number Used ;",
        "; Two Independent 18x18 ; %d ;" % total,
        "; Total number of DSP blocks ; %d ;" % total,
        "; Fixed Point Unsigned Multiplier ; 2 ;",
        "",
    ]
    return "\r\n".join(lines)


def quartus17_map_summary(status: str, revision: str, top: str, total: int) -> str:
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


class FakeQuartus:
    """Writes mechanics fixtures but never starts a process."""

    def __init__(self, bound: routes.BoundInputs, kind: str = "correct", map_rc: int = 0):
        self.bound = bound
        self.kind = kind
        self.map_rc = map_rc
        self.calls: list[tuple[list[str], Path]] = []

    def __call__(self, command: list[str], cwd: Path, log) -> int:
        self.calls.append((list(command), cwd))
        if len(self.calls) == 1:
            log.write(b"FAKE TEST EXECUTOR: quartus_map was not launched\n")
            if self.map_rc:
                return self.map_rc
            revision = self.bound.config["revision"]
            total = 2 if self.kind == "two-atom" else 1
            wrappers = 2 if self.kind == "two-atom" else 1
            status = "Successful - " + datetime.datetime.now().astimezone().strftime(
                "%a %b %d %H:%M:%S %Y"
            )
            output = cwd / "output_files"
            output.mkdir()
            (output / (revision + ".map.rpt")).write_text(
                quartus17_fixture(self.bound.config["top"], revision, status, total, wrappers),
                encoding="utf-8",
                newline="",
            )
            (output / (revision + ".map.summary")).write_text(
                quartus17_map_summary(status, revision, self.bound.config["top"], total),
                encoding="utf-8",
                newline="\n",
            )
            database = cwd / "db"
            database.mkdir()
            (database / (revision + ".map.cdb")).write_bytes(b"FAKE-NONPHYSICAL-DB\n")
            return 0
        log.write(b"FAKE TEST EXECUTOR: quartus_cdb was not launched\n")
        atom_tsv = Path(command[5])
        project = command[3]
        atom_tsv.write_text(
            synthetic_graph(
                self.kind,
                artifact_class=routes.ARTIFACT_CLASS,
                synthetic=False,
                capture_id=self.bound.invocation["captureId"],
                project=project,
                revision=self.bound.config["revision"],
                quartus_version="17.0.2 Build 602 TEST FIXTURE - NOT QUARTUS",
            ),
            encoding="utf-8",
            newline="\n",
        )
        return 0


class Dual18AtomRouteTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = tempfile.TemporaryDirectory(prefix="dual18-route-no-quartus-")
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
        cls.anchor_path = cls.out / "dual18_invocation_anchor.json"
        cls.anchor_raw = cls.anchor_path.read_bytes()
        cls.anchor = json.loads(cls.anchor_raw.decode("utf-8"))

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def config_path(self, base: str = "dual18_explicit_pair") -> Path:
        return self.out / "dual18" / base / "effective_config.json"

    def invocation_args(self, base: str = "dual18_explicit_pair") -> tuple:
        return (
            self.config_path(base),
            routes._canonical(self.anchor_path),
            hashlib.sha256(self.anchor_raw).hexdigest(),
            self.anchor["invocationNonce"],
            self.anchor["manifestSha256"],
        )

    def acquire(self, variant: str = "explicit", base: str = "dual18_explicit_pair"):
        return routes.acquire_bound_inputs(*self.invocation_args(base), variant)

    def test_correct_fixture_checks_every_bit_but_never_passes_physical_gate(self) -> None:
        result = routes.evaluate_synthetic(synthetic_graph())
        self.assertEqual(result["mechanicalStatus"], "pass")
        self.assertEqual(result["routeGateStatus"], "hold")
        self.assertEqual(result["status"], "hold")
        self.assertFalse(result["physicalEvidence"])
        self.assertEqual(result["mechanics"]["operandBitsChecked"], 72)
        self.assertEqual(result["mechanics"]["resultBitsChecked"], 72)
        self.assertEqual(result["mechanics"]["inventedInternalArcs"], 0)

    def test_forged_genuine_labels_still_cannot_promote_parser_fixture(self) -> None:
        forged = synthetic_graph(artifact_class=routes.ARTIFACT_CLASS, synthetic=False)
        result = routes.evaluate_synthetic(forged)
        self.assertEqual(result["mechanicalStatus"], "pass")
        self.assertEqual(result["routeGateStatus"], "hold")
        self.assertTrue(result["synthetic"])

    def test_lane_collapse_lane_swap_and_two_atom_detectors_fire(self) -> None:
        expected = {
            "collapsed": "origin mismatch",
            "swapped": "origin mismatch",
            "two-atom": "exactly one MAC_MULT",
        }
        for kind, message in expected.items():
            with self.subTest(kind=kind):
                result = routes.evaluate_synthetic(synthetic_graph(kind))
                self.assertEqual(result["mechanicalStatus"], "reject")
                self.assertIn(message, result["detector"])
                self.assertEqual(result["routeGateStatus"], "hold")

    def test_exact_cardinality_detector_fires(self) -> None:
        result = routes.evaluate_synthetic(synthetic_graph("missing-port"))
        self.assertEqual(result["mechanicalStatus"], "reject")
        self.assertIn("cardinality/index mismatch", result["detector"])

    def test_hidden_or_unavailable_connectivity_is_hold_not_pass(self) -> None:
        result = routes.evaluate_synthetic(synthetic_graph("hidden"))
        self.assertEqual(result["mechanicalStatus"], "unavailable")
        self.assertIn("unavailable connectivity", result["holds"][0])

    def test_non_pin_or_non_padio_named_atom_cannot_masquerade_as_boundary(self) -> None:
        for kind in ("boundary-node-not-pin", "boundary-port-not-padio"):
            with self.subTest(kind=kind):
                result = routes.evaluate_synthetic(synthetic_graph(kind))
                self.assertEqual(result["mechanicalStatus"], "unavailable")
                self.assertEqual(result["routeGateStatus"], "hold")
                self.assertIn("PIN/PADIO boundary", result["holds"][0])

    def test_one_sided_fanin_fanout_disagreement_is_hold(self) -> None:
        result = routes.evaluate_synthetic(synthetic_graph("one-sided"))
        self.assertEqual(result["mechanicalStatus"], "unavailable")
        self.assertIn("fanin/fanout disagree", result["holds"][0])

    def test_multi_hop_sequential_and_control_paths_are_hold(self) -> None:
        for kind in ("multi-hop", "sequential", "control-crossing"):
            with self.subTest(kind=kind):
                result = routes.evaluate_synthetic(synthetic_graph(kind))
                self.assertEqual(result["mechanicalStatus"], "unavailable")
                self.assertEqual(result["routeGateStatus"], "hold")
                self.assertRegex(result["holds"][0], "internal|sequential|data-dependency")

    def test_schema_one_and_duplicate_records_reject(self) -> None:
        old = synthetic_graph().replace("dual18-atom-route-tsv\t2", "dual18-atom-route-tsv\t1")
        with self.assertRaisesRegex(routes.GateError, "schema"):
            routes.parse_atom_tsv(old)
        duplicate = synthetic_graph() + "node\tatom0\tMAC_MULT\t0\tdup\n"
        with self.assertRaisesRegex(routes.GateError, "duplicate|malformed"):
            routes.parse_atom_tsv(duplicate)

    def test_generated_contract_binds_map_cdb_script_and_fresh_workspace(self) -> None:
        config = json.loads(self.config_path().read_text(encoding="utf-8"))
        contract = config["routeCaptureContract"]
        invocation_path = self.config_path().parent / config["routeCapture"]["invocationFile"]
        invocation = json.loads(invocation_path.read_text(encoding="utf-8"))
        self.assertEqual(contract["schemaVersion"], 2)
        self.assertEqual(contract["atomAdjacency"], "exact-cdb-fanin-and-fanout-only")
        self.assertEqual(contract["internalAtomArcs"], "unavailable-hold")
        self.assertEqual(Path(contract["quartusMap"]["path"]), routes._canonical(routes.MAP_PATH))
        self.assertEqual(Path(contract["quartusCdb"]["path"]), routes._canonical(routes.CDB_PATH))
        self.assertEqual(invocation["mapCommandTemplate"], [contract["quartusMap"]["path"], "{project}"])
        self.assertEqual(invocation["cdbCommandTemplate"][1:3], ["-t", contract["captureScript"]["absolutePath"]])
        self.assertEqual(invocation["freshRunTokenBytes"], 32)
        self.assertRegex(invocation["freshWorkspacePrefix"], r"^d18_[0-9a-f]{8}_$")
        self.assertNotIn("freshOutputDirectory", invocation)

    def test_all_real_control_sources_and_independent_wrong_sink_are_bound(self) -> None:
        controls = {
            "two-primitives-mutant": "dual18_two_primitives_mutant",
            "lane-collapse-mutant": "dual18_lane_collapse_mutant",
            "lane-swap-mutant": "dual18_lane_swap_mutant",
        }
        for variant, base in controls.items():
            with self.subTest(variant=variant):
                config = json.loads(self.config_path(base).read_text(encoding="utf-8"))
                self.assertEqual(config["controlKind"], routes.CONTROL_VARIANTS[variant])
                self.assertTrue(config["routeCapture"]["checkerRunsMap"])
                bound = self.acquire(variant, base)
                try:
                    self.assertEqual(bound.config["controlKind"], routes.CONTROL_VARIANTS[variant])
                finally:
                    bound.close()
        collapse = (REPO / "tests" / "mutants" / "dual18_lane_collapse_mutant.sv").read_text(encoding="utf-8")
        self.assertIn("resultb_wrong_sink_o", collapse)
        self.assertIn("assign resultb_o            = prod_a_c", collapse)
        self.assertIn("assign resultb_wrong_sink_o = prod_b_wrong_sink_c", collapse)
        self.assertNotRegex(collapse, r"\(\*\s*preserve\s*\*\).*prod_b")

    def test_bound_input_acquisition_rejects_forged_script_hash_and_command(self) -> None:
        bound = self.acquire()
        try:
            forged_contract = copy.deepcopy(bound.config["routeCaptureContract"])
            forged_contract["captureScript"]["sha256"] = "0" * 64
            with self.assertRaisesRegex(routes.GateError, "script hash"):
                snapshot = routes.ArtifactSnapshot.capture(routes.CAPTURE_SCRIPT, "script")
                try:
                    if snapshot.sha256 != forged_contract["captureScript"]["sha256"]:
                        raise routes.GateError("capture Tcl script hash differs from content witness")
                finally:
                    snapshot.close()
            forged_invocation = copy.deepcopy(bound.invocation)
            forged_invocation["mapCommandTemplate"][0] = "C:/forged/quartus_map.exe"
            with self.assertRaisesRegex(routes.GateError, "quartus_map command template is forged"):
                routes._validate_contract_and_invocation(
                    bound.config,
                    bound.config["routeCaptureContract"],
                    forged_invocation,
                    self.config_path().parent,
                    bound.snapshots["invocation"],
                )
        finally:
            bound.close()

    def test_forged_cdb_runtime_version_control_rejects(self) -> None:
        bound = self.acquire()
        try:
            project = "C:/bound/fresh/project"
            now_ns = time.time_ns()
            graph = routes.parse_atom_tsv(
                synthetic_graph(
                    artifact_class=routes.ARTIFACT_CLASS,
                    synthetic=False,
                    capture_id=bound.invocation["captureId"],
                    project=project,
                    revision=bound.config["revision"],
                    quartus_version="99.0 forged",
                )
            )
            with self.assertRaisesRegex(routes.GateError, "runtime version"):
                routes._validate_physical_metadata(
                    graph, bound, project, now_ns - 1_000_000_000, now_ns + 1_000_000_000
                )
        finally:
            bound.close()

    def test_fake_executor_proves_fresh_map_then_database_then_cdb_sequence(self) -> None:
        bound = self.acquire()
        try:
            fake = FakeQuartus(bound)
            evidence = routes.capture_one(bound, "explicit", runner=fake)
            self.assertEqual(len(fake.calls), 2)
            self.assertEqual(fake.calls[0][0][0], routes._canonical(routes.MAP_PATH).as_posix())
            self.assertEqual(fake.calls[1][0][0], routes._canonical(routes.CDB_PATH).as_posix())
            self.assertEqual(fake.calls[0][1], fake.calls[1][1])
            self.assertRegex(evidence["freshRunToken"], r"^[A-Za-z0-9_-]{43}$")
            self.assertEqual(evidence["routeGateStatus"], "pass")
            self.assertEqual(evidence["mechanics"]["inventedInternalArcs"], 0)
            self.assertTrue(evidence["postMapDatabase"]["files"])
            self.assertEqual(evidence["mapInvocation"]["argv"], fake.calls[0][0])
            self.assertEqual(evidence["cdbInvocation"]["argv"], fake.calls[1][0])
        finally:
            bound.close()

    def test_failed_fresh_map_is_overall_hold_and_cdb_never_starts(self) -> None:
        bound = self.acquire()
        try:
            fake = FakeQuartus(bound, map_rc=7)
            result = routes.run_variant(bound, "explicit", runner=fake)
            self.assertEqual(result["status"], "hold")
            self.assertEqual(result["routeGateStatus"], "hold")
            self.assertFalse(result["physicalEvidence"])
            self.assertEqual(len(fake.calls), 1)
            self.assertIn("quartus_map failed", result["holds"][1])
            self.assertEqual(result["productionMigration"], "none")
            self.assertEqual(result["productionDspSaving"], 0)
        finally:
            bound.close()

    def test_stale_output_timestamp_control_rejects(self) -> None:
        stale = self.out / "stale.atom.tsv"
        stale.write_bytes(b"stale\n")
        old_ns = time.time_ns() - 10_000_000_000
        os.utime(stale, ns=(old_ns, old_ns))
        with self.assertRaisesRegex(routes.GateError, "predates"):
            routes._snapshot_output(stale, "stale CDB output", old_ns + 5_000_000_000)

    def test_stale_replayed_database_file_rejects(self) -> None:
        root = self.out / "stale-db-control"
        root.mkdir()
        database_file = root / "old.map.cdb"
        database_file.write_bytes(b"replayed\n")
        old_ns = time.time_ns() - 10_000_000_000
        os.utime(database_file, ns=(old_ns, old_ns))
        with self.assertRaisesRegex(routes.GateError, "database file predates"):
            routes.DatabaseSnapshot.capture(root, old_ns + 5_000_000_000)

    def test_post_snapshot_artifact_swap_is_blocked_or_rejected(self) -> None:
        path = self.out / "toctou-control.tsv"
        replacement = self.out / "toctou-replacement.tsv"
        path.write_bytes(b"captured-original\n")
        snapshot = routes.ArtifactSnapshot.capture(path, "TOCTOU positive control")
        try:
            replacement.write_bytes(b"forged-after-cdb\n")
            try:
                os.replace(replacement, path)
            except PermissionError:
                blocked = True
            else:
                blocked = False
            if blocked:
                self.assertEqual(snapshot.data, b"captured-original\n")
            else:
                with self.assertRaisesRegex(routes.GateError, "swapped or mutated"):
                    snapshot.verify_identity()
                self.assertEqual(snapshot.data, b"captured-original\n")
        finally:
            snapshot.close()
            replacement.unlink(missing_ok=True)
            path.unlink(missing_ok=True)

    def test_pipeline_post_cdb_swap_control_rejects(self) -> None:
        bound = self.acquire()
        try:
            def swap_after_only_read(outputs: dict[str, Path]) -> None:
                replacement = outputs["atomTsv"].with_name("forged-after-cdb.tsv")
                replacement.write_bytes(b"forged-after-cdb\n")
                try:
                    os.replace(replacement, outputs["atomTsv"])
                except PermissionError as exc:
                    raise routes.GateError("post-CDB swap was blocked by read lock") from exc

            with self.assertRaisesRegex(
                routes.GateError, "post-CDB swap|swapped or mutated"
            ):
                routes.capture_one(
                    bound,
                    "explicit",
                    runner=FakeQuartus(bound),
                    after_cdb_snapshot_hook=swap_after_only_read,
                )
        finally:
            bound.close()

    def test_replayed_workspace_name_is_rejected_before_map(self) -> None:
        bound = self.acquire()
        original = routes.secrets.token_urlsafe
        routes.secrets.token_urlsafe = lambda _: "b" * 43
        try:
            workspace = (
                Path(bound.invocation["workspaceParent"])
                / (bound.invocation["freshWorkspacePrefix"] + "b" * 43)
            )
            workspace.mkdir()
            with self.assertRaisesRegex(routes.GateError, "already exists"):
                routes.capture_one(bound, "explicit", runner=FakeQuartus(bound))
        finally:
            routes.secrets.token_urlsafe = original
            bound.close()

    def test_tcl_capture_contract_is_static_exact_and_read_only(self) -> None:
        text = routes.CAPTURE_SCRIPT.read_text(encoding="utf-8")
        self.assertIn("project_open -error_on_incompatible_database", text)
        self.assertIn("read_atom_netlist -type map", text)
        self.assertIn("get_atom_port_info", text)
        self.assertIn("-key $key", text)
        self.assertIn("edge $source_node $source_port $node $port fanin", text)
        self.assertIn("edge $node $port $destination_node $destination_port fanout", text)
        self.assertNotIn("set_atom_node_info", text)
        self.assertNotIn("write_atom_netlist -pdb", text)
        self.assertIn("refusing pre-existing atom-route output", text)


if __name__ == "__main__":
    unittest.main(verbosity=2)
