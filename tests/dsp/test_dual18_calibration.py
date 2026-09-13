#!/usr/bin/env python3
"""Direct tests for gen_calib.py's four isolated dual18 revisions."""

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


class Dual18CalibrationGenerationTest(unittest.TestCase):
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

    def test_four_revisions_are_content_addressed_fresh_and_isolated(self) -> None:
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
                [row["baseRevision"] for row in manifest["revisions"]],
                [
                    "dual18_inferred_pair",
                    "dual18_explicit_pair",
                    "dual18_s32x18_exact",
                    "dual18_two_primitives_mutant",
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
                self.assertEqual(preparation["revision"], revision)
                self.assertEqual(preparation["contentWitness"], config["contentWitness"])
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

            inferred_dir = out / "dual18" / "dual18_inferred_pair"
            inferred_config = json.loads((inferred_dir / "effective_config.json").read_text())
            inferred_qsf = (inferred_dir / inferred_config["qsfFile"]).read_text(encoding="ascii")
            self.assertNotIn("zhao_dual18_mul.sv", inferred_qsf)
            self.assertNotIn("ZHAO_DUAL18_", inferred_qsf)

            # Regeneration is a fresh run: it removes stale raw output even when
            # unchanged content yields the same deterministic witness/revision.
            stale = inferred_dir / "output_files" / "stale.map.rpt"
            stale.write_text("old evidence\n", encoding="utf-8")
            self.generate(out)
            self.assertFalse(stale.exists())
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


if __name__ == "__main__":
    unittest.main(verbosity=2)
