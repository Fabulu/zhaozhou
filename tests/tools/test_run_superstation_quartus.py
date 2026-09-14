from __future__ import annotations

import importlib.util
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
            command: list[str], *, cwd: Path, check: bool
        ) -> subprocess.CompletedProcess[bytes]:
            self.assertEqual(cwd, self.build)
            self.assertFalse(check)
            calls.append(command)
            return subprocess.CompletedProcess(command, 0)

        RUNNER.run_compile(
            self.quartus,
            self.build,
            "ZhaozhouSpecs",
            runner=clean_runner,
        )
        self.assertEqual(calls, RUNNER.stage_commands(self.quartus, self.build, "ZhaozhouSpecs"))
        self.assertEqual(self.qsf.read_bytes(), original)

    def test_quartus_qsf_mutation_is_rejected(self) -> None:
        calls = 0

        def mutating_runner(
            command: list[str], *, cwd: Path, check: bool
        ) -> subprocess.CompletedProcess[bytes]:
            nonlocal calls
            calls += 1
            if calls == 1:
                text = self.qsf.read_text(encoding="utf-8").replace(
                    "17.0.2 Standard Edition", "17.0.2 Lite Edition"
                )
                text += (
                    'set_global_assignment -name RESERVE_ALL_UNUSED_PINS_NO_OUTPUT_GND '
                    '"AS INPUT TRI-STATED"\n'
                )
                self.qsf.write_text(text, encoding="utf-8")
            return subprocess.CompletedProcess(command, 0)

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
