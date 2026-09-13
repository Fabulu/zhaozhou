#!/usr/bin/env python3
"""Executable Packet-A gates for render/texture packed types and ownership HOLDs."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "quartus"
PACKAGE = REPO / "fpga" / "rtl" / "common" / "zhao_render_texture_pkg.sv"
FIXTURE = Path(__file__).resolve().parent / "fixtures" / "zhao_render_texture_layout_top.sv"
MUTANT = REPO / "tests" / "mutants" / "zhao_render_texture_wrong_layout_mutant.sv"
ROLE = "raster_texture_fragment_lifecycle"
EXPECTED_PROVIDERS = {
    "zhao_texture_v3own",
    "zhao_raster_texjoin_v2",
    "zhao_texture_fragrob",
}
EXPECTED_NEW_EXCLUSIONS = {
    "zhao_dual18_mul": "not-yet-adopted",
    "zhao_shell_fit_audio_sink": "probe",
    "zhao_shell_fit_gpu_sink": "probe",
    "zhao_shell_fit_stimulus": "probe",
    "zhao_shell_fit_top": "probe",
    "zhao_shell_fit_video_sink": "probe",
}


def stripped_path_environment() -> dict[str, str]:
    environment = os.environ.copy()
    environment["PATH"] = ""
    environment["VERILATOR_ROOT"] = str(REPO / "deliberately-wrong-vroot")
    environment.pop("ZHAO_WINLIBS_BIN", None)
    return environment


if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import check_ownership_roles as ownership
import check_prod_manifest as prod_manifest


def run_verilator(top: str, sources: list[Path], mdir: Path,
                  *extra: str,
                  base_environment: dict[str, str] | None = None,
                  ) -> subprocess.CompletedProcess[str]:
    verilator = ownership.find_verilator(str(REPO))
    if not verilator:
        raise AssertionError("the pinned Verilator executable is unavailable")
    verilator = Path(verilator).resolve()
    command = [
        str(verilator), *extra,
        "-Wall", "-Wno-DECLFILENAME", "-Wno-UNUSEDSIGNAL",
        "--Mdir", str(mdir), "--top-module", top,
        *map(str, sources),
    ]
    return subprocess.run(
        command,
        cwd=REPO,
        env=ownership.verilator_environment(
            str(verilator), str(REPO), base_environment
        ),
        capture_output=True,
        text=True,
        errors="replace",
    )


def find_native_gxx() -> Path:
    # tools/env/zhao-env.ps1's pinned Windows compiler.  Never fall back to a
    # caller-PATH compiler: mixing an unrelated ABI would make this gate lie.
    candidate = REPO.parents[1] / "dsstuff" / "mingw64" / "bin" / "g++.exe"
    if candidate.is_file():
        return candidate
    raise AssertionError("native g++ from tools/env/zhao-env.ps1 is unavailable")


def compile_generated_model(mdir: Path, top: str) -> Path:
    verilator = Path(ownership.find_verilator(str(REPO)) or "")
    verilator_root = ownership._verilator_root(str(verilator))
    if not verilator_root:
        raise AssertionError("Verilator runtime include directory is unavailable")
    include = Path(verilator_root) / "include"
    gxx = find_native_gxx()
    prefix = "V" + top
    model_sources = sorted(mdir.glob(prefix + "*.cpp"))
    if not model_sources:
        raise AssertionError("Verilator emitted no model C++ sources")

    main = mdir / "packet_a_main.cpp"
    main.write_text(
        '#include "' + prefix + '.h"\n'
        '#include "verilated.h"\n'
        'double sc_time_stamp() { return 0.0; }\n'
        'int main(int argc, char** argv) {\n'
        '  VerilatedContext context;\n'
        '  context.commandArgs(argc, argv);\n'
        '  ' + prefix + ' top{&context};\n'
        '  top.eval();\n'
        '  top.final();\n'
        '  return 0;\n'
        '}\n',
        encoding="utf-8",
    )
    executable = mdir / (prefix + (".exe" if os.name == "nt" else ""))
    command = [
        str(gxx), "-std=gnu++17", "-O0",
        "-I" + str(mdir), "-I" + str(include),
        "-I" + str(include / "vltstd"),
        str(main), *map(str, model_sources),
        str(include / "verilated.cpp"),
        str(include / "verilated_threads.cpp"),
        "-o", str(executable), "-pthread",
    ]
    environment = os.environ.copy()
    environment["PATH"] = str(gxx.parent) + os.pathsep + environment.get("PATH", "")
    compiled = subprocess.run(
        command, cwd=REPO, env=environment,
        capture_output=True, text=True, errors="replace",
    )
    if compiled.returncode != 0:
        raise AssertionError(
            "generated Packet-A model did not compile:\n" +
            compiled.stdout + compiled.stderr
        )
    return executable


def run_generated_model(executable: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    environment = os.environ.copy()
    environment["PATH"] = (
        str(find_native_gxx().parent) + os.pathsep + environment.get("PATH", "")
    )
    return subprocess.run(
        [str(executable), *arguments],
        cwd=REPO,
        env=environment,
        capture_output=True,
        text=True,
        errors="replace",
    )


def production_source_segment() -> str:
    text = (REPO / "design" / "fit_targets.yml").read_text(encoding="utf-8")
    start = text.index("- top: zhao_prod_top")
    segment = text[start:]
    next_target = segment.find("\n  - top:")
    return segment if next_target < 0 else segment[:next_target]


class RenderTextureLayoutTests(unittest.TestCase):
    def test_native_compiler_does_not_fall_back_to_caller_path(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-packet-a-fake-gxx-") as temporary:
            fake_gxx = Path(temporary) / "g++.exe"
            fake_gxx.write_bytes(b"caller controlled")
            with mock.patch.object(Path, "is_file", return_value=False), mock.patch.dict(
                    os.environ, {"PATH": temporary}, clear=False):
                with self.assertRaisesRegex(
                        AssertionError,
                        "native g\\+\\+ from tools/env/zhao-env.ps1 is unavailable"):
                    find_native_gxx()

    def test_exact_layout_roundtrips_and_all_runtime_detectors_fire(self) -> None:
        top = "zhao_render_texture_layout_top"
        with tempfile.TemporaryDirectory(prefix="zhao-packet-a-layout-") as temporary:
            mdir = Path(temporary)
            # The canonical ownership-tool environment must load Verilator even
            # when the invoking process contributes no executable search path.
            generated = run_verilator(
                top, [PACKAGE, FIXTURE], mdir, "--cc",
                base_environment=stripped_path_environment(),
            )
            self.assertEqual(
                generated.returncode, 0,
                generated.stdout + generated.stderr,
            )
            executable = compile_generated_model(mdir, top)

            normal = run_generated_model(executable)
            field_span_runs = [
                run_generated_model(executable, "+FIELD_SPAN_CONTROL=%d" % control)
                for control in range(1, 46)
            ]
            roundtrip_runs = [
                run_generated_model(executable, "+ROUNDTRIP_CONTROL=%d" % control)
                for control in range(1, 7)
            ]

        diagnostic = normal.stdout + normal.stderr
        self.assertEqual(normal.returncode, 0, diagnostic)
        self.assertIn("ZHAO_RENDER_TEXTURE_LAYOUT_GUARD_OK field_spans=45", diagnostic)
        self.assertIn("ZHAO_RENDER_TEXTURE_LAYOUT_ROUNDTRIP_OK controls=6", diagnostic)

        for control, ran in enumerate(field_span_runs, 1):
            with self.subTest(field_span_control=control):
                diagnostic = ran.stdout + ran.stderr
                self.assertNotEqual(ran.returncode, 0, diagnostic)
                self.assertIn(
                    "ZHAO_RENDER_TEXTURE_FIELD_SPAN_FIRE[%d]" % control,
                    diagnostic,
                )
                self.assertNotIn("FIELD_SPAN_ESCAPED", diagnostic)

        roundtrip_names = (
            "continuation", "aux", "earlyz_payload",
            "pretexture", "retirement", "result",
        )
        for control, (name, ran) in enumerate(
                zip(roundtrip_names, roundtrip_runs), 1):
            with self.subTest(roundtrip_control=control, packet=name):
                diagnostic = ran.stdout + ran.stderr
                self.assertNotEqual(ran.returncode, 0, diagnostic)
                self.assertIn(
                    "ZHAO_RENDER_TEXTURE_ROUNDTRIP_FIRE[%d]: %s" %
                    (control, name),
                    diagnostic,
                )
                self.assertNotIn("ROUNDTRIP_ESCAPED", diagnostic)

    def test_committed_wrong_layout_executes_unique_aux_contract_fatal(self) -> None:
        top = "zhao_render_texture_wrong_layout_mutant"
        with tempfile.TemporaryDirectory(prefix="zhao-packet-a-mutant-") as temporary:
            mdir = Path(temporary)
            generated = run_verilator(top, [PACKAGE, MUTANT], mdir, "--cc")
            self.assertEqual(
                generated.returncode, 0,
                generated.stdout + generated.stderr,
            )
            executable = compile_generated_model(mdir, top)
            result = run_generated_model(executable)
        diagnostic = result.stdout + result.stderr
        self.assertNotEqual(result.returncode, 0, diagnostic)
        self.assertTrue(diagnostic.strip(), "AUX mutant failed without a diagnostic")
        self.assertIn(
            "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[16]: AUX_LAYOUT",
            diagnostic,
        )

    def test_every_static_contract_detector_executes_its_unique_fatal(self) -> None:
        controls = {
            1: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[1]: WIDTH_CONTRACT",
            2: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[2]: EARLYZ_OFFSET_CONTRACT",
            3: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[3]: CONTINUATION_OFFSET_CONTRACT",
            4: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[4]: AUX_OFFSET_CONTRACT",
            5: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[5]: TEXREQ_OFFSET_CONTRACT",
            6: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[6]: EZPAY_OFFSET_CONTRACT",
            7: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[7]: PRETEX_OFFSET_CONTRACT",
            8: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[8]: RETIRE_OFFSET_CONTRACT",
            9: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[9]: RESULT_OFFSET_CONTRACT",
            10: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[10]: CONTINUATION_LAYOUT",
            11: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[11]: TEXREQ_LAYOUT",
            12: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[12]: EZPAY_LAYOUT",
            13: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[13]: PRETEX_LAYOUT",
            14: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[14]: RETIRE_LAYOUT",
            15: "ZHAO_RENDER_TEXTURE_CONTRACT_FIRE[15]: RESULT_LAYOUT",
        }
        top = "zhao_render_texture_elab_control_top"

        # A failing process is evidence only after the same generated root has
        # run successfully with its real/default contract.
        with tempfile.TemporaryDirectory(
                prefix="zhao-packet-a-contract-baseline-") as temporary:
            mdir = Path(temporary)
            generated = run_verilator(top, [PACKAGE, FIXTURE], mdir, "--cc")
            self.assertEqual(
                generated.returncode, 0,
                generated.stdout + generated.stderr,
            )
            baseline = run_generated_model(
                compile_generated_model(mdir, top)
            )
        baseline_diagnostic = baseline.stdout + baseline.stderr
        self.assertEqual(baseline.returncode, 0, baseline_diagnostic)
        self.assertIn(
            "ZHAO_RENDER_TEXTURE_LAYOUT_GUARD_OK field_spans=45",
            baseline_diagnostic,
        )

        for control, expected_fatal in controls.items():
            with self.subTest(static_contract_control=control), tempfile.TemporaryDirectory(
                    prefix="zhao-packet-a-contract-control-") as temporary:
                mdir = Path(temporary)
                generated = run_verilator(
                    top, [PACKAGE, FIXTURE], mdir,
                    "--cc", "-GCONTROL=%d" % control,
                )
                self.assertEqual(
                    generated.returncode, 0,
                    generated.stdout + generated.stderr,
                )
                result = run_generated_model(
                    compile_generated_model(mdir, top)
                )
                diagnostic = result.stdout + result.stderr
                self.assertNotEqual(result.returncode, 0, diagnostic)
                self.assertTrue(
                    diagnostic.strip(),
                    "contract control %d failed without a diagnostic" % control,
                )
                self.assertIn(expected_fatal, diagnostic)

    def test_ratified_type_and_offset_vocabulary_is_present(self) -> None:
        text = PACKAGE.read_text(encoding="utf-8")
        for required in (
            "zhao_raster_continuation_v2_t",
            "zhao_aux_surface_ctx_v2_t",
            "zhao_raster_pretex_v2_t",
            "zhao_raster_earlyz_payload_v2_t",
            "zhao_raster_retire_ctx_v2_t",
            "zhao_texture_result_v2_t",
            "AUX_WX_LO", "AUX_WZ_LO", "AUX_SHEET_HANDLE_LO",
            "AUX_ENV_X0_LO", "AUX_ENV_X1_LO", "AUX_ENV_Z0_LO",
            "AUX_ENV_Z1_LO", "PRETEX_IN_TILE_ADDR_LO",
            "PRETEX_U_OVER_W_LO", "RETIRE_RASTER_SEQUENCE_LO",
            "TEXTURE_RESULT_SOURCE_REFUSED_BIT",
        ):
            self.assertIn(required, text)
        self.assertRegex(text, r"(?m)^\s*logic signed \[31:0\] wz;")
        self.assertRegex(text, r"(?m)^\s*logic signed \[31:0\] wx;")


class PacketAOwnershipAndClosureTests(unittest.TestCase):
    def test_existing_registry_remains_exactly_three_providers(self) -> None:
        roles = ownership.read_roles(REPO / "design" / "prod_manifest.yml")
        self.assertEqual(set(roles[ROLE]["providers"]), EXPECTED_PROVIDERS)
        self.assertEqual(
            ownership.PINNED_PROVIDER_REGISTRY[ROLE],
            frozenset(EXPECTED_PROVIDERS),
        )

    def test_characterization_modules_are_explicitly_excluded_not_selected(self) -> None:
        tops, excluded = prod_manifest.read_manifest(
            REPO / "design" / "prod_manifest.yml"
        )
        for module, expected_reason in EXPECTED_NEW_EXCLUSIONS.items():
            with self.subTest(module=module):
                self.assertNotIn(module, tops)
                self.assertIn(module, excluded)
                reason, detail = excluded[module]
                self.assertEqual(reason, expected_reason)
                self.assertTrue(detail)
        self.assertIn("selected production consumer", excluded["zhao_dual18_mul"][1])
        for module in EXPECTED_NEW_EXCLUSIONS:
            if module.startswith("zhao_shell_fit_"):
                self.assertIn("never production", excluded[module][1])

    def test_current_shell_is_fail_closed_not_a_connected_owner_claim(self) -> None:
        roles = ownership.read_roles(REPO / "design" / "prod_manifest.yml")
        self.assertEqual(roles[ROLE]["scope"], "selected_subsystem")
        self.assertEqual(roles[ROLE]["root"], "zhao_texture_island_v3_top")

        errors, observations = ownership.run_check(
            REPO / "design" / "prod_manifest.yml",
            REPO / "fpga" / "rtl",
            root_overrides={ROLE: "zhao_shell_top"},
        )
        observed = {
            role: reachable
            for role, _scope, _root, reachable in observations
        }
        self.assertIn(
            ROLE, observed,
            "shell ownership census produced no explicit observation: %r" % errors,
        )
        self.assertEqual(observed[ROLE], [])
        self.assertTrue(
            any("has 0 elaborated lifecycle-owner instances" in error
                for error in errors),
            errors,
        )
        # There is no semantic connected-shell root to register in Packet A.
        declarations, _edges, _extra = ownership.module_edges(REPO / "fpga" / "rtl")
        self.assertNotIn("zhao_shell_top_v2", declarations)

    def test_probe_is_excluded_and_production_selection_stays_unchanged(self) -> None:
        tops, excluded = prod_manifest.read_manifest(
            REPO / "design" / "prod_manifest.yml"
        )
        self.assertNotIn("zhao_render_texture_layout_guard", tops)
        self.assertEqual(excluded["zhao_render_texture_layout_guard"][0], "probe")

        generated_top = (
            REPO / "fpga" / "rtl" / "prod" / "zhao_prod_top.sv"
        ).read_text(encoding="utf-8")
        self.assertNotIn("zhao_render_texture_pkg", generated_top)
        self.assertNotIn("zhao_render_texture_layout_guard", generated_top)
        self.assertNotIn("zhao_raster_texjoin_v2", generated_top)
        self.assertEqual(prod_manifest.check_top_fresh(), [])

        source_segment = production_source_segment()
        self.assertNotIn("zhao_render_texture_pkg.sv", source_segment)
        self.assertNotIn("zhao_render_texture_layout_guard", source_segment)
        self.assertNotIn("zhao_raster_texjoin_v2.sv", source_segment)

        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            declarations, edges = prod_manifest.module_edges()
            closure_errors = prod_manifest.check_fit_sources(declarations, edges)
        finally:
            os.chdir(previous_cwd)
        self.assertEqual(closure_errors, [])


if __name__ == "__main__":
    unittest.main()
