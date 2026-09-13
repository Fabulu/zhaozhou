#!/usr/bin/env python3
"""Direct tests for gen_calib.py's six isolated dual18 revisions."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
GENERATOR = REPO / "tools" / "budget" / "gen_calib.py"
DUAL18_RTL = REPO / "fpga" / "rtl" / "common" / "zhao_dual18_mul.sv"


class Dual18CalibrationGenerationTest(unittest.TestCase):
    def test_unused_preadder_and_coefficient_ports_are_disabled(self) -> None:
        rtl = DUAL18_RTL.read_text(encoding="utf-8")
        self.assertIn(".az_width(0)", rtl)
        self.assertIn(".bz_width(0)", rtl)
        for port in ("az", "bz", "coefsela", "coefselb"):
            self.assertNotIn(f".{port}(", rtl)

    def generate(self, out: Path) -> str:
        completed = subprocess.run(
            [sys.executable, str(GENERATOR), "--dual18-only", "--outdir", str(out)],
            cwd=REPO,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout)
        return completed.stdout

    def test_six_revisions_are_content_addressed_fresh_and_isolated(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dual18-calib-test-") as temp:
            out = Path(temp)
            generation_output = self.generate(out)
            manifest_path = out / "dual18" / "dual18_manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            anchor_path = out / "dual18_invocation_anchor.json"
            first_anchor_raw = anchor_path.read_bytes()
            first_anchor = json.loads(first_anchor_raw.decode("utf-8"))
            self.assertEqual(anchor_path.parent, out)
            self.assertRegex(first_anchor["invocationNonce"], r"[0-9a-f]{64}$")
            self.assertEqual(len(bytes.fromhex(first_anchor["invocationNonce"])), 32)
            self.assertEqual(
                first_anchor["manifestSha256"],
                hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
            )
            self.assertIn("DUAL18_INVOCATION_ANCHOR", generation_output)
            self.assertIn(hashlib.sha256(first_anchor_raw).hexdigest(), generation_output)
            self.assertIn(first_anchor["invocationNonce"], generation_output)
            self.assertIn(first_anchor["manifestSha256"], generation_output)
            self.assertEqual(
                first_anchor["trustBoundary"],
                "orchestration-supplied-outside-candidate-tree",
            )
            self.assertEqual(manifest["device"], "5CSEBA6U23I7")
            self.assertEqual(
                manifest["evidenceBoundary"],
                {
                    "withoutGenuineCurrentCdbArtifacts": "hold",
                    "encryptedVendorModel": "hold",
                    "productionMigration": "none",
                    "productionDspSaving": 0,
                },
            )
            self.assertFalse(manifest["routeCaptureContract"]["syntheticFixturesPhysical"])
            path_policy = manifest["routeCaptureContract"]["runtimePathPolicy"]
            self.assertEqual(path_policy["freshRunTokenBytes"], 32)
            self.assertEqual(path_policy["workspaceLeafPrefix"], "d18_")
            self.assertEqual(path_policy["workspaceLeafDigestBytes"], 6)
            self.assertEqual(path_policy["anchoredWorkspaceParentName"], "d18_runs")
            self.assertEqual(
                path_policy["compiledPartitionArtifactSuffix"],
                ".root_partition.map.hbdb.hb_info",
            )
            self.assertEqual(path_policy["quartusHardPathLimit"], 260)
            self.assertEqual(path_policy["quartusHardPathMargin"], 40)
            self.assertEqual(path_policy["quartusInternalPathLimit"], 220)
            all_preflights = manifest["quartusInternalPathPreflight"]
            self.assertEqual(all_preflights["limit"], 220)
            self.assertEqual(all_preflights["quartusHardPathLimit"], 260)
            self.assertEqual(all_preflights["quartusHardPathMargin"], 40)
            self.assertEqual(len(all_preflights["variants"]), 6)
            self.assertLessEqual(all_preflights["worstExpectedPathLength"], 220)
            self.assertEqual(
                all_preflights["worstVariant"],
                "dual18_two_primitives_mutant",
            )
            self.assertEqual(
                all_preflights["worstExpectedPathLength"],
                max(row["longestExpectedPathLength"] for row in all_preflights["variants"]),
            )
            self.assertEqual(
                [row["baseRevision"] for row in manifest["revisions"]],
                [
                    "dual18_inferred_pair",
                    "dual18_explicit_pair",
                    "dual18_s32x18_exact",
                    "dual18_two_primitives_mutant",
                    "dual18_lane_collapse_mutant",
                    "dual18_lane_swap_mutant",
                ],
            )

            expected = {
                "dual18_inferred_pair": (
                    ["tests/rtl/dual18_physical_pack_discriminator.sv"], []
                ),
                "dual18_explicit_pair": (
                    [
                        "fpga/rtl/common/zhao_dual18_mul.sv",
                        "tests/rtl/dual18_physical_pack_discriminator.sv",
                    ],
                    ["ZHAO_DUAL18_CYCLONEV=1"],
                ),
                "dual18_s32x18_exact": (
                    [
                        "fpga/rtl/common/zhao_dual18_mul.sv",
                        "tests/rtl/dual18_physical_pack_discriminator.sv",
                    ],
                    ["ZHAO_DUAL18_CYCLONEV=1"],
                ),
                "dual18_two_primitives_mutant": (
                    [
                        "fpga/rtl/common/zhao_dual18_mul.sv",
                        "tests/mutants/dual18_two_primitives_mutant.sv",
                    ],
                    ["ZHAO_DUAL18_CYCLONEV=1"],
                ),
                "dual18_lane_collapse_mutant": (
                    [
                        "fpga/rtl/common/zhao_dual18_mul.sv",
                        "tests/mutants/dual18_lane_collapse_mutant.sv",
                    ],
                    ["ZHAO_DUAL18_CYCLONEV=1"],
                ),
                "dual18_lane_swap_mutant": (
                    [
                        "fpga/rtl/common/zhao_dual18_mul.sv",
                        "tests/mutants/dual18_lane_swap_mutant.sv",
                    ],
                    ["ZHAO_DUAL18_CYCLONEV=1"],
                ),
            }
            first_witnesses = {}

            for base_revision, (sources, macros) in expected.items():
                revision_dir = out / "dual18" / base_revision
                config_path = revision_dir / "effective_config.json"
                config = json.loads(config_path.read_text(encoding="utf-8"))
                revision = config["revision"]
                self.assertRegex(revision, re.escape(base_revision) + r"_[0-9a-f]{16}$")
                self.assertEqual(config["baseRevision"], base_revision)
                self.assertEqual(revision, base_revision + "_" + config["contentWitness"][:16])
                first_witnesses[base_revision] = config["contentWitness"]

                qpf = (revision_dir / config["qpfFile"]).read_text(encoding="ascii")
                qsf_path = revision_dir / config["qsfFile"]
                qsf = qsf_path.read_text(encoding="ascii")
                self.assertIn('PROJECT_REVISION = "%s"' % revision, qpf)
                self.assertEqual(config["stage"], "map-only")
                self.assertNotIn("VIRTUAL_PIN", qsf)
                if base_revision == "dual18_explicit_pair":
                    self.assertEqual(config["top"], "dual18_explicit_pair")
                route_contract = config["routeCaptureContract"]
                self.assertEqual(route_contract["gate"], "dual18_postmap_lane_route_witness")
                self.assertFalse(route_contract["syntheticFixturesPhysical"])
                self.assertEqual(route_contract["requiredAtomType"], "MAC")
                self.assertEqual(
                    route_contract["inputPortFamilies"],
                    {"AX": 18, "AY": 18, "BX": 18, "BY": 18},
                )
                self.assertEqual(
                    route_contract["outputPortFamilies"], {"RESULTA": 36, "RESULTB": 36}
                )
                self.assertEqual(route_contract["schemaVersion"], 3)
                self.assertEqual(
                    route_contract["atomAdjacency"],
                    "exact-cdb-fanin-and-fanout-only",
                )
                self.assertEqual(route_contract["internalAtomArcs"], "unavailable-hold")
                self.assertEqual(
                    Path(route_contract["quartusMap"]["path"]),
                    Path(r"C:\intelFPGA_lite\17.0\quartus\bin64\quartus_map.exe"),
                )
                self.assertEqual(
                    Path(route_contract["quartusCdb"]["path"]),
                    Path(r"C:\intelFPGA_lite\17.0\quartus\bin64\quartus_cdb.exe"),
                )
                self.assertEqual(
                    route_contract["mapArgumentOrder"], ["project"]
                )
                self.assertEqual(
                    route_contract["tclArgumentOrder"],
                    ["project", "revision", "atomTsv", "optionalAtomVo", "captureId"],
                )
                capture = config["routeCapture"]
                self.assertRegex(capture["captureId"], r"[0-9a-f]{64}$")
                invocation_path = revision_dir / capture["invocationFile"]
                self.assertEqual(
                    capture["invocationSha256"],
                    hashlib.sha256(invocation_path.read_bytes()).hexdigest(),
                )
                invocation = json.loads(invocation_path.read_text(encoding="utf-8"))
                self.assertFalse(invocation["synthetic"])
                self.assertEqual(invocation["schemaVersion"], 3)
                self.assertEqual(invocation["captureId"], capture["captureId"])
                self.assertTrue(capture["checkerRunsMap"])
                self.assertEqual(invocation["freshRunTokenBytes"], 32)
                self.assertEqual(invocation["freshWorkspacePrefix"], "d18_")
                self.assertEqual(invocation["runtimePathPolicy"], path_policy)
                path_preflight = invocation["quartusInternalPathPreflight"]
                self.assertEqual(
                    path_preflight,
                    capture["quartusInternalPathPreflight"],
                )
                self.assertEqual(path_preflight["limit"], 220)
                self.assertEqual(path_preflight["quartusHardPathLimit"], 260)
                self.assertEqual(path_preflight["quartusHardPathMargin"], 40)
                self.assertEqual(path_preflight["workspaceLeafLength"], 12)
                self.assertLessEqual(path_preflight["longestExpectedPathLength"], 220)
                self.assertEqual(
                    path_preflight["longestExpectedPathLength"],
                    len(path_preflight["longestExpectedPath"]),
                )
                literal_expected_path = (
                    out
                    / "d18_runs"
                    / "d18_XXXXXXXX"
                    / "incremental_db"
                    / "compiled_partitions"
                    / (revision + ".root_partition.map.hbdb.hb_info")
                ).absolute().as_posix()
                self.assertEqual(
                    path_preflight["longestExpectedPath"],
                    literal_expected_path,
                )
                self.assertEqual(
                    path_preflight["longestExpectedPathLength"],
                    len(literal_expected_path),
                )
                self.assertEqual(
                    invocation["workspaceParent"],
                    str((out / "d18_runs").absolute()).replace("\\", "/"),
                )
                self.assertEqual(
                    invocation["mapCommandTemplate"],
                    [route_contract["quartusMap"]["path"], "{project}"],
                )
                self.assertEqual(
                    invocation["cdbCommandTemplate"],
                    [
                        route_contract["quartusCdb"]["path"],
                        "-t",
                        route_contract["captureScript"]["absolutePath"],
                        "{project}",
                        revision,
                        "{atomTsv}",
                        "{optionalAtomVo}",
                        capture["captureId"],
                    ],
                )
                self.assertEqual(
                    invocation["runtimeOutputs"],
                    {
                        "mapLog": "%s.quartus_map.log" % revision,
                        "mapReport": "output_files/%s.map.rpt" % revision,
                        "mapSummary": "output_files/%s.map.summary" % revision,
                        "atomTsv": "route_evidence/%s.dual18.atom.tsv" % revision,
                        "cdbLog": "route_evidence/%s.quartus_cdb.log" % revision,
                        "optionalAtomVo": "route_evidence/%s.post_map.vo" % revision,
                        "receipt": "route_evidence/%s.atom_route_receipt.json" % revision,
                    },
                )
                self.assertNotIn("freshOutputDirectory", invocation)
                self.assertNotIn("command", invocation)
                self.assertEqual(config["device"], "5CSEBA6U23I7")
                self.assertEqual([row["path"] for row in config["sources"]], sources)
                self.assertEqual(config["macros"], macros)
                self.assertEqual(config["qsfSha256"], hashlib.sha256(qsf_path.read_bytes()).hexdigest())
                self.assertNotIn("dual18_s32xu12_projector", qsf)

                digest = hashlib.sha256()
                for row in config["sources"]:
                    source = Path(row["absolutePath"])
                    actual = hashlib.sha256(source.read_bytes()).hexdigest()
                    self.assertEqual(row["sha256"], actual)
                    digest.update(row["path"].encode("utf-8"))
                    digest.update(b"\0")
                    digest.update(actual.encode("ascii"))
                    digest.update(b"\n")
                    self.assertEqual(qsf.count(str(source).replace("\\", "/")), 1)
                self.assertEqual(config["sourceSetSha256"], digest.hexdigest())
                self.assertEqual(
                    config["witnessInputs"]["vendorInterfaceEvidence"],
                    config["vendorInterfaceEvidence"],
                )
                self.assertEqual(qsf.count("SYSTEMVERILOG_FILE"), len(sources))
                self.assertEqual(qsf.count("VERILOG_MACRO"), len(macros))
                for macro in macros:
                    self.assertIn('VERILOG_MACRO "%s"' % macro, qsf)

                preparation_path = revision_dir / config["runPreparation"]["file"]
                self.assertEqual(
                    config["runPreparation"]["sha256"],
                    hashlib.sha256(preparation_path.read_bytes()).hexdigest(),
                )
                preparation = json.loads(preparation_path.read_text(encoding="utf-8"))
                self.assertTrue(preparation["outputDirectoryWasEmpty"])
                self.assertTrue(preparation["checkerMustRunFreshMap"])
                self.assertEqual(
                    preparation["freshRuntimeWorkspacePrefix"],
                    "d18_runs/" + invocation["freshWorkspacePrefix"],
                )
                self.assertEqual(preparation["revision"], revision)
                self.assertEqual(preparation["contentWitness"], config["contentWitness"])
                self.assertEqual(
                    preparation["quartusInternalPathPreflight"],
                    path_preflight,
                )
                output_dir = revision_dir / preparation["outputDirectory"]
                self.assertEqual(list(output_dir.iterdir()), [])

                evidence = {row["kind"]: row for row in config["vendorInterfaceEvidence"]}
                for row in evidence.values():
                    if row["available"]:
                        self.assertTrue(row.get("matchedExcerpts"))
                    else:
                        # Generation on a host without Quartus is an explicit
                        # HOLD receipt, not a KeyError pretending the file exists.
                        self.assertTrue(row.get("requiredPatterns"))
                        self.assertNotIn("matchedExcerpts", row)
                if evidence["atom-declaration"]["available"]:
                    atom = "\n".join(evidence["atom-declaration"]["matchedExcerpts"])
                    self.assertIn('parameter operation_mode = "m18x18_sumof2";', atom)
                    self.assertNotIn("single_port", atom)
                if evidence["xml-metadata"]["available"]:
                    xml = "\n".join(evidence["xml-metadata"]["matchedExcerpts"])
                    self.assertIn("m18x18_full", xml)
                    self.assertIn('PORT NAME="resulta"', xml)
                    self.assertIn('PORT NAME="resultb"', xml)

                manifest_row = next(row for row in manifest["revisions"]
                                    if row["baseRevision"] == base_revision)
                self.assertEqual(manifest_row["revision"], revision)
                self.assertEqual(manifest_row["sourceSetSha256"], config["sourceSetSha256"])
                self.assertEqual(
                    manifest_row["projectFileSha256"],
                    hashlib.sha256((revision_dir / config["qpfFile"]).read_bytes()).hexdigest(),
                )
                self.assertEqual(manifest_row["settingsFileSha256"], config["qsfSha256"])
                self.assertEqual(
                    manifest_row["runPreparationSha256"],
                    config["runPreparation"]["sha256"],
                )
                self.assertEqual(
                    manifest_row["effectiveConfigSha256"],
                    hashlib.sha256(config_path.read_bytes()).hexdigest(),
                )
                self.assertEqual(manifest_row["routeCaptureId"], capture["captureId"])
                self.assertEqual(
                    manifest_row["quartusInternalPathPreflight"],
                    path_preflight,
                )
                self.assertEqual(
                    manifest_row["routeInvocationSha256"],
                    hashlib.sha256(invocation_path.read_bytes()).hexdigest(),
                )

            inferred_dir = out / "dual18" / "dual18_inferred_pair"
            inferred_config = json.loads((inferred_dir / "effective_config.json").read_text())
            inferred_qsf = (inferred_dir / inferred_config["qsfFile"]).read_text(encoding="ascii")
            self.assertNotIn("zhao_dual18_mul.sv", inferred_qsf)
            self.assertNotIn("ZHAO_DUAL18_", inferred_qsf)

            # Regeneration clears the replaceable per-revision output tree while
            # preserving checker-owned raw runtime artifacts under d18_runs.
            stale = inferred_dir / "output_files" / "stale.map.rpt"
            stale.write_text("old evidence\n", encoding="utf-8")
            preserved_runtime = out / "d18_runs" / "d18_preserved" / "raw.map.rpt"
            preserved_runtime.parent.mkdir()
            preserved_runtime.write_text("preserved raw evidence\n", encoding="utf-8")
            self.generate(out)
            self.assertFalse(stale.exists())
            self.assertEqual(
                preserved_runtime.read_text(encoding="utf-8"),
                "preserved raw evidence\n",
            )
            second_anchor_raw = anchor_path.read_bytes()
            second_anchor = json.loads(second_anchor_raw.decode("utf-8"))
            self.assertNotEqual(second_anchor["invocationNonce"], first_anchor["invocationNonce"])
            self.assertNotEqual(
                hashlib.sha256(second_anchor_raw).hexdigest(),
                hashlib.sha256(first_anchor_raw).hexdigest(),
            )
            self.assertEqual(
                second_anchor["manifestSha256"],
                hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
            )
            for base_revision, witness in first_witnesses.items():
                config = json.loads(
                    (out / "dual18" / base_revision / "effective_config.json").read_text()
                )
                self.assertEqual(config["contentWitness"], witness)
                self.assertEqual(list((out / "dual18" / base_revision / "output_files").iterdir()), [])

    def _outdir_for_full_artifact_length(self, parent: Path, target: int) -> Path:
        # Independent literal reconstruction: do not import the generator helper
        # or its constants, or the boundary test could bless the same defect.
        revision = "dual18_two_primitives_mutant_" + ("0" * 16)
        artifact_tail = (
            Path("d18_runs") / "d18_XXXXXXXX" /
            "incremental_db" / "compiled_partitions" /
            (revision + ".root_partition.map.hbdb.hb_info")
        )
        filler_length = target - len(str(parent)) - len(str(artifact_tail)) - 2
        self.assertGreater(filler_length, 0)
        out = parent / ("x" * filler_length)
        complete = out / artifact_tail
        self.assertEqual(len(str(complete.resolve())), target)
        return out

    def test_generation_accepts_220_and_refuses_221_character_artifact(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dual18-path-preflight-") as temp:
            parent = Path(temp)
            accepted_out = self._outdir_for_full_artifact_length(parent, 220)
            accepted = subprocess.run(
                [
                    sys.executable,
                    str(GENERATOR),
                    "--dual18-only",
                    "--outdir",
                    str(accepted_out),
                ],
                cwd=REPO,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(accepted.returncode, 0, accepted.stdout)
            manifest = json.loads(
                (accepted_out / "dual18" / "dual18_manifest.json").read_text(
                    encoding="utf-8")
            )
            self.assertEqual(
                manifest["quartusInternalPathPreflight"]["worstExpectedPathLength"],
                220,
            )

            refused_out = self._outdir_for_full_artifact_length(parent, 221)
            refused = subprocess.run(
                [
                    sys.executable,
                    str(GENERATOR),
                    "--dual18-only",
                    "--outdir",
                    str(refused_out),
                ],
                cwd=REPO,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertNotEqual(refused.returncode, 0, refused.stdout)
            self.assertIn(
                "Quartus internal path preflight exceeds 220 characters",
                refused.stdout,
            )
            self.assertIn("(221)", refused.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
