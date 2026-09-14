from __future__ import annotations

import copy
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
RUNNER_PATH = REPO / "tools" / "board" / "run_superstation_quartus.py"
SPEC = importlib.util.spec_from_file_location("run_superstation_quartus", RUNNER_PATH)
assert SPEC is not None and SPEC.loader is not None
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)
PATCH_PATH = REPO / "tools" / "board" / "patch_mister_build_id.py"
PATCH_SPEC = importlib.util.spec_from_file_location("patch_mister_build_id_for_runner", PATCH_PATH)
assert PATCH_SPEC is not None and PATCH_SPEC.loader is not None
PATCH = importlib.util.module_from_spec(PATCH_SPEC)
PATCH_SPEC.loader.exec_module(PATCH)


class SuperStationQuartusRunnerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.quartus = root / "quartus"
        self.build = root / "build"
        self.quartus.mkdir()
        (self.build / "sys").mkdir(parents=True)
        for name in (
            "quartus_sh.exe",
            "quartus_map.exe",
            "quartus_fit.exe",
            "quartus_asm.exe",
            "quartus_sta.exe",
        ):
            (self.quartus / name).write_bytes(b"fixture\n")
        patched_build_id = PATCH.patch_bytes(
            (REPO / "fpga" / "sys" / "build_id.tcl").read_bytes()
        )
        (self.build / "sys" / "build_id.tcl").write_bytes(patched_build_id)
        self.qsf = self.build / "ZhaozhouSpecs.qsf"
        self.qsf.write_text(
            'set_global_assignment -name LAST_QUARTUS_VERSION "17.0.2 Standard Edition"\n',
            encoding="utf-8",
        )
        self.output = self.build / "output_files"
        self.output.mkdir()
        for suffix in RUNNER.STAGE_OUTPUT_SUFFIXES:
            (self.output / f"ZhaozhouSpecs.{suffix}").write_bytes(
                f"artifact:{suffix}\n".encode()
            )

    def completed(
        self, command: list[str], return_code: int = 0
    ) -> subprocess.CompletedProcess[bytes]:
        stdout = b""
        if command[-1:] == ["--version"]:
            stdout = (
                b"Quartus Prime Shell\n"
                b"Version 17.0.2 Build 602 07/19/2017 SJ Lite Edition\n"
            )
        return subprocess.CompletedProcess(command, return_code, stdout=stdout, stderr=b"")

    def clean_runner(
        self, command: list[str], *, cwd: Path, check: bool, **kwargs: object
    ) -> subprocess.CompletedProcess[bytes]:
        self.assertEqual(cwd, self.build)
        self.assertFalse(check)
        return self.completed(command)

    def create_receipt(self) -> Path:
        return RUNNER.run_compile(
            self.quartus,
            self.build,
            "ZhaozhouSpecs",
            runner=self.clean_runner,
        )

    def test_real_compile_stage_receives_no_write_flags(self) -> None:
        commands = RUNNER.stage_commands(self.quartus, self.build, "ZhaozhouSpecs")
        self.assertEqual(
            commands[1],
            [
                str(self.quartus / "quartus_map.exe"),
                "--read_settings_files=on",
                "--write_settings_files=off",
                "ZhaozhouSpecs",
                "-c",
                "ZhaozhouSpecs",
            ],
        )
        for command in commands[1:4]:
            self.assertEqual(
                command[1:3],
                ["--read_settings_files=on", "--write_settings_files=off"],
            )

    def test_build_id_command_uses_only_bound_explicit_inputs(self) -> None:
        commands = RUNNER.stage_commands(self.quartus, self.build, "ZhaozhouSpecs")
        self.assertEqual(
            commands[0],
            [
                str(self.quartus / "quartus_sh.exe"),
                "-t",
                str(self.build / "sys" / "build_id.tcl"),
                "ZhaozhouSpecs",
                "5CSEBA6U23I7",
                "output_files",
            ],
        )
        script = (self.build / "sys" / "build_id.tcl").read_bytes()
        self.assertNotIn(b"project_open", script)
        self.assertNotIn(b"project_close", script)
        self.assertNotIn(b"get_global_assignment", script)

    def test_build_entries_use_guarded_runner_not_flow(self) -> None:
        for name in (
            "build_superstation_bringup.ps1",
            "build_superstation_specs.ps1",
        ):
            text = (REPO / "tools" / "board" / name).read_text(encoding="utf-8")
            self.assertIn("run_superstation_quartus.py", text)
            self.assertIn("patch_mister_build_id.py", text)
            self.assertIn("SuperStation projectless build-ID patch failed", text)
            self.assertNotIn("--flow compile", text)

    def test_actual_projectless_build_id_invocation_preserves_qsf(self) -> None:
        quartus_bin = Path(r"C:\intelFPGA_lite\17.0\quartus\bin64")
        quartus_sh = quartus_bin / "quartus_sh.exe"
        self.assertTrue(quartus_sh.is_file(), f"required board tool is missing: {quartus_sh}")
        before = self.qsf.read_bytes()
        command = RUNNER.stage_commands(quartus_bin, self.build, "ZhaozhouSpecs")[0]
        completed = subprocess.run(
            command,
            cwd=self.build,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stdout.decode("cp1252"))
        self.assertEqual(self.qsf.read_bytes(), before)
        self.assertTrue((self.build / "build_id.v").is_file())
        self.assertTrue((self.build / "jtag.cdf").is_file())

    def test_clean_stage_sequence_preserves_qsf(self) -> None:
        calls: list[list[str]] = []
        original = self.qsf.read_bytes()

        def clean_runner(
            command: list[str], *, cwd: Path, check: bool, **kwargs: object
        ) -> subprocess.CompletedProcess[bytes]:
            self.assertEqual(cwd, self.build)
            self.assertFalse(check)
            if command[-1:] != ["--version"]:
                calls.append(command)
            return self.completed(command)

        receipt = RUNNER.run_compile(
            self.quartus,
            self.build,
            "ZhaozhouSpecs",
            runner=clean_runner,
        )
        self.assertEqual(calls, RUNNER.stage_commands(self.quartus, self.build, "ZhaozhouSpecs"))
        self.assertEqual(self.qsf.read_bytes(), original)
        self.assertEqual(receipt, self.output / "ZhaozhouSpecs.stage-receipt.json")
        receipt_data = json.loads(receipt.read_text(encoding="utf-8"))
        self.assertEqual(receipt.read_bytes(), RUNNER.receipt_bytes(receipt_data))
        self.assertFalse(receipt.with_name(receipt.name + ".tmp").exists())
        self.assertEqual(
            RUNNER.verify_stage_receipt_file(
                self.quartus, self.build, "ZhaozhouSpecs"
            ),
            [],
        )

    def test_missing_stage_receipt_is_rejected(self) -> None:
        errors = RUNNER.verify_stage_receipt_file(
            self.quartus, self.build, "ZhaozhouSpecs"
        )
        self.assertTrue(any("receipt is missing" in error for error in errors))

    def test_noncanonical_receipt_encoding_is_rejected(self) -> None:
        path = self.create_receipt()
        data = json.loads(path.read_text(encoding="utf-8"))
        path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        errors = RUNNER.verify_stage_receipt_file(
            self.quartus, self.build, "ZhaozhouSpecs"
        )
        self.assertIn("stage sequence receipt encoding is not canonical", errors)

    def test_each_stage_failure_prevents_receipt(self) -> None:
        commands = RUNNER.stage_commands(self.quartus, self.build, "ZhaozhouSpecs")
        for failed_index, failed_command in enumerate(commands):
            with self.subTest(stage=RUNNER.STAGE_NAMES[failed_index]):
                receipt = self.output / "ZhaozhouSpecs.stage-receipt.json"
                receipt.unlink(missing_ok=True)

                def failing_runner(
                    command: list[str], *, cwd: Path, check: bool, **kwargs: object
                ) -> subprocess.CompletedProcess[bytes]:
                    if command[-1:] == ["--version"]:
                        return self.completed(command)
                    return self.completed(command, 1 if command == failed_command else 0)

                with self.assertRaisesRegex(ValueError, "failed with exit code 1"):
                    RUNNER.run_compile(
                        self.quartus,
                        self.build,
                        "ZhaozhouSpecs",
                        runner=failing_runner,
                    )
                self.assertFalse(receipt.exists())

    def test_forged_stage_receipt_fields_are_rejected(self) -> None:
        path = self.create_receipt()
        valid = json.loads(path.read_text(encoding="utf-8"))
        mutants: list[tuple[str, dict, str]] = []

        rc_mutant = copy.deepcopy(valid)
        rc_mutant["stages"][2]["returnCode"] = 1
        mutants.append(("RC", rc_mutant, "command/RC mismatch"))

        command_mutant = copy.deepcopy(valid)
        command_mutant["stages"][1]["command"][1] = "--read_settings_files=off"
        mutants.append(("command", command_mutant, "command/RC mismatch"))

        qsf_mutant = copy.deepcopy(valid)
        qsf_mutant["qsf"]["afterEach"]["fit"]["sha256"] = "0" * 64
        mutants.append(("QSF hash", qsf_mutant, "QSF stage records mismatch"))

        artifact_mutant = copy.deepcopy(valid)
        artifact_mutant["artifacts"]["ZhaozhouSpecs.rbf"]["sha256"] = "0" * 64
        mutants.append(("artifact hash", artifact_mutant, "artifact mismatch"))

        for label, mutant, expected_error in mutants:
            with self.subTest(field=label):
                mutant["receiptSha256"] = RUNNER.receipt_digest(mutant)
                errors = RUNNER.verify_stage_receipt_data(
                    mutant, self.quartus, self.build, "ZhaozhouSpecs"
                )
                self.assertTrue(any(expected_error in error for error in errors), errors)

    def test_unexpected_done_or_extra_artifact_prevents_receipt(self) -> None:
        for name in ("ZhaozhouSpecs.done", "ZhaozhouSpecs.unexpected"):
            with self.subTest(name=name):
                extra = self.output / name
                extra.write_bytes(b"extra\n")
                with self.assertRaisesRegex(
                    ValueError, "direct-stage output record set mismatch"
                ):
                    self.create_receipt()
                self.assertFalse(
                    (self.output / "ZhaozhouSpecs.stage-receipt.json").exists()
                )
                extra.unlink()

    def test_quartus_qsf_mutation_is_rejected(self) -> None:
        calls = 0

        def mutating_runner(
            command: list[str], *, cwd: Path, check: bool, **kwargs: object
        ) -> subprocess.CompletedProcess[bytes]:
            nonlocal calls
            if command[-1:] != ["--version"]:
                calls += 1
            if calls == 1 and command[-1:] != ["--version"]:
                text = self.qsf.read_text(encoding="utf-8").replace(
                    "17.0.2 Standard Edition", "17.0.2 Lite Edition"
                )
                text += (
                    'set_global_assignment -name RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND '
                    '"AS INPUT TRI-STATED"\n'
                )
                self.qsf.write_text(text, encoding="utf-8")
            return self.completed(command)

        with self.assertRaisesRegex(
            ValueError,
            r"Quartus mutated manifest-bound ZhaozhouSpecs\.qsf while running quartus_sh\.exe",
        ):
            RUNNER.run_compile(
                self.quartus,
                self.build,
                "ZhaozhouSpecs",
                runner=mutating_runner,
            )
        self.assertEqual(calls, 1)
        self.assertIn(
            "RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND",
            self.qsf.read_text(encoding="utf-8"),
        )


if __name__ == "__main__":
    unittest.main()
