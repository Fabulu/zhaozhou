#!/usr/bin/env python3
"""Direct executable controls for TEXJOIN accounting retirement."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "quartus"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
MUTANTS = REPO / "tests" / "mutants"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import check_ownership_roles as ownership
import check_prod_manifest as manifest
import gen_prod_top as generator


ROLE = "raster_texture_fragment_lifecycle"
EXPECTED_PROVIDERS = {
    "zhao_texture_v3own",
    "zhao_raster_texjoin_v2",
    "zhao_texture_fragrob",
}


def observation_map(observations):
    return {role: reachable for role, _scope, _root, reachable in observations}


def stripped_path_environment():
    environment = os.environ.copy()
    environment["PATH"] = ""
    environment["VERILATOR_ROOT"] = str(REPO / "deliberately-wrong-vroot")
    environment.pop("ZHAO_WINLIBS_BIN", None)
    return environment


class VerilatorLoaderEnvironmentTests(unittest.TestCase):
    def selected_verilator(self) -> Path:
        selected = ownership.find_verilator(str(REPO))
        self.assertIsNotNone(selected, "pinned verilator_bin is unavailable")
        return Path(selected).resolve()

    def test_builder_loads_selected_verilator_from_stripped_path(self) -> None:
        verilator = self.selected_verilator()
        environment = ownership.verilator_environment(
            str(verilator), str(REPO), stripped_path_environment()
        )
        # Rebuilding an already canonical environment must not duplicate paths.
        environment = ownership.verilator_environment(
            str(verilator), str(REPO), environment
        )
        suite = verilator.parent.parent
        expected_prefixes = [suite / "bin", suite / "lib"]
        winlibs = REPO.parents[1] / "dsstuff" / "mingw64" / "bin"
        if winlibs.is_dir():
            expected_prefixes.append(winlibs)

        path_entries = environment["PATH"].split(os.pathsep)
        self.assertEqual(
            [os.path.normcase(os.path.normpath(entry))
             for entry in path_entries[:len(expected_prefixes)]],
            [os.path.normcase(os.path.normpath(str(entry)))
             for entry in expected_prefixes],
        )
        for prefix in expected_prefixes:
            normalized = os.path.normcase(os.path.normpath(str(prefix)))
            self.assertEqual(
                sum(os.path.normcase(os.path.normpath(entry)) == normalized
                    for entry in path_entries),
                1,
            )
        self.assertEqual(
            Path(environment["VERILATOR_ROOT"]),
            suite / "share" / "verilator",
        )

        result = subprocess.run(
            [str(verilator), "--version"],
            cwd=REPO,
            env=environment,
            capture_output=True,
            text=True,
            errors="replace",
        )
        diagnostic = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, diagnostic)
        self.assertIn("Verilator", diagnostic)

    def test_v3param_ownership_smoke_loads_from_stripped_path(self) -> None:
        verilator = self.selected_verilator()
        with tempfile.TemporaryDirectory(prefix="owner-loader-smoke-") as temporary:
            source = Path(temporary) / "owner_loader_smoke.sv"
            source.write_text(
                "module owner_loader_smoke_leaf; endmodule\n"
                "module owner_loader_smoke_top;\n"
                "  owner_loader_smoke_leaf u_leaf();\n"
                "endmodule\n",
                encoding="utf-8",
            )
            cells = ownership.elaborated_cells(
                "owner_loader_smoke_top",
                [str(source)],
                str(REPO),
                str(verilator),
                base_environment=stripped_path_environment(),
            )
        self.assertIn(("u_leaf", "owner_loader_smoke_leaf"), cells)

    def test_package_only_sources_are_dependency_ordered(self) -> None:
        sources = [Path(source) for source in ownership.ordered_package_sources(
            REPO / "fpga" / "rtl")]
        abi = REPO / "fpga" / "rtl" / "generated" / "zhao_abi_pkg.sv"
        common = REPO / "fpga" / "rtl" / "common" / "zhao_pkg.sv"
        self.assertIn(abi, sources)
        self.assertIn(common, sources)
        self.assertLess(sources.index(abi), sources.index(common))

    def test_builder_fails_closed_when_suite_lib_is_missing(self) -> None:
        with tempfile.TemporaryDirectory(prefix="owner-loader-missing-lib-") as temporary:
            suite = Path(temporary) / "fake-suite"
            suite_bin = suite / "bin"
            suite_bin.mkdir(parents=True)
            fake_verilator = suite_bin / (
                "verilator_bin.exe" if os.name == "nt" else "verilator_bin"
            )
            fake_verilator.write_text("not executed\n", encoding="utf-8")
            with self.assertRaisesRegex(
                    ownership.ElaborationError,
                    r"Verilator loader environment invalid: required suite lib "
                    r"directory is missing"):
                ownership.verilator_environment(
                    str(fake_verilator), str(REPO), stripped_path_environment()
                )


class OwnershipRoleTests(unittest.TestCase):
    def run_owner_check(self, source: Path | None = None,
                        root: str | None = None):
        extras = [source] if source else []
        overrides = {ROLE: root} if root else None
        return ownership.run_check(
            REPO / "design" / "prod_manifest.yml",
            REPO / "fpga" / "rtl",
            extras,
            overrides,
        )

    def test_selected_v3_subsystem_has_exactly_one_elaborated_owner(self) -> None:
        roles = ownership.read_roles(REPO / "design" / "prod_manifest.yml")
        self.assertEqual(roles[ROLE]["scope"], "selected_subsystem")
        self.assertEqual(roles[ROLE]["root"], "zhao_texture_island_v3_top")
        self.assertEqual(roles[ROLE]["provider_identification"],
                         "explicit_registry")

        errors, observations = self.run_owner_check()
        self.assertEqual(errors, [])
        self.assertEqual(observation_map(observations)[ROLE],
                         ["zhao_texture_v3own"])

    def test_provider_registry_is_exactly_pinned_to_all_known_implementations(self) -> None:
        roles = ownership.read_roles(REPO / "design" / "prod_manifest.yml")
        self.assertEqual(set(roles[ROLE]["providers"]), EXPECTED_PROVIDERS)
        self.assertEqual(ownership.PINNED_PROVIDER_REGISTRY[ROLE],
                         frozenset(EXPECTED_PROVIDERS))

        # A manifest edit cannot silently erase one known implementation: the
        # checker owns an independent pinned copy of the semantic registry.
        source = (REPO / "design" / "prod_manifest.yml").read_text(encoding="utf-8")
        damaged = source.replace("      - zhao_texture_fragrob\n", "", 1)
        with tempfile.TemporaryDirectory(prefix="owner-registry-control-") as td:
            path = Path(td) / "prod_manifest.yml"
            path.write_text(damaged, encoding="utf-8")
            with self.assertRaisesRegex(ownership.RoleManifestError,
                                        "must be exactly"):
                ownership.read_roles(path)

    def test_duplicate_owner_control_uses_real_elaboration(self) -> None:
        errors, observations = self.run_owner_check(
            FIXTURES / "texjoin_duplicate_owner_top.sv",
            "texjoin_duplicate_owner_top",
        )
        self.assertTrue(any("2 elaborated lifecycle-owner instances" in error
                            for error in errors), errors)
        reachable = observation_map(observations)[ROLE]
        self.assertEqual(reachable,
                         ["zhao_raster_texjoin_v2", "zhao_texture_v3own"])

    def test_dead_generate_ifdef_and_string_do_not_count_but_macro_instance_does(self) -> None:
        errors, observations = self.run_owner_check(
            FIXTURES / "texjoin_elaboration_semantics_top.sv",
            "texjoin_elaboration_semantics_top",
        )
        self.assertEqual(errors, [])
        # V3OWN is reached only through the macro-expanded island instance.
        # TEXJOIN/FRAGROB occur in source text but not in the elaborated AST.
        self.assertEqual(observation_map(observations)[ROLE],
                         ["zhao_texture_v3own"])

    def test_renamed_annotated_extra_provider_is_rejected(self) -> None:
        errors, observations = self.run_owner_check(
            MUTANTS / "zhao_texture_lifecycle_extra_owner_mutant.sv",
            "texjoin_extra_owner_control_top",
        )
        joined = "\n".join(errors)
        self.assertIn("explicitly annotated provider(s) absent", joined)
        self.assertIn("zhao_texture_lifecycle_extra_owner_mutant", joined)
        self.assertIn("2 elaborated lifecycle-owner instances", joined)
        self.assertEqual(
            observation_map(observations)[ROLE],
            ["zhao_texture_lifecycle_extra_owner_mutant", "zhao_texture_v3own"],
        )


class AccountingRetirementTests(unittest.TestCase):
    def test_retired_ordinal_is_a_non_emitting_tombstone(self) -> None:
        tops, excluded = manifest.read_manifest(REPO / "design" / "prod_manifest.yml")
        retired = manifest.read_list_section(
            "retired_census_slots", REPO / "design" / "prod_manifest.yml"
        )
        self.assertNotIn("zhao_raster_texjoin_v2", tops)
        self.assertEqual(excluded["zhao_raster_texjoin_v2"][0], "superseded")
        self.assertEqual(retired, ["zhao_raster_texjoin_v2"])

        slots = generator.census_slots(tops, retired)
        slot_of = {module: ordinal for ordinal, module in slots}
        self.assertNotIn("zhao_raster_texjoin_v2", slot_of)
        self.assertEqual(slot_of["zhao_raster_fog"], 45)
        self.assertEqual(slot_of["zhao_raster_toon"], 47)
        self.assertNotIn(46, {ordinal for ordinal, _module in slots})

    def test_generated_top_matches_live_tops_and_has_no_texjoin_scaffold(self) -> None:
        tops, _excluded = manifest.read_manifest(REPO / "design" / "prod_manifest.yml")
        text = (REPO / "fpga" / "rtl" / "prod" / "zhao_prod_top.sv").read_text(
            encoding="utf-8"
        )
        sections = re.findall(r"^  // ---- (zhao_\w+) ----$", text, re.MULTILINE)
        self.assertEqual(sorted(sections), sorted(tops))
        self.assertEqual(len(sections), len(set(sections)))
        self.assertNotIn("zhao_raster_texjoin_v2", sections)
        self.assertNotRegex(text, r"\bu46_(?:lfsr_q|src|i|fold_q)\b")
        self.assertEqual(manifest.check_top_fresh(), [])

    def test_every_nonzero_generator_status_invalidates_freshness(self) -> None:
        control = FIXTURES / "gen_prod_top_status_control.py"
        meanings = {
            1: "selected top skipped",
            2: "generator refusal",
            3: "stale or mismatched generated top",
        }
        for status, meaning in meanings.items():
            with self.subTest(status=status):
                errors = manifest.check_top_fresh(
                    [sys.executable, str(control), str(status)])
                self.assertEqual(len(errors), 1)
                self.assertIn("RC %d" % status, errors[0])
                self.assertIn(meaning, errors[0])
                self.assertIn("output:", errors[0])

    def test_generator_freshness_is_independent_of_process_cwd(self) -> None:
        probe = (
            "import sys; "
            "sys.path.insert(0, %r); "
            "import check_prod_manifest as checker; "
            "errors = checker.check_top_fresh(); "
            "print('FOREIGN_CWD_FRESH=' + ('1' if not errors else '0')); "
            "print('\\n'.join(errors)); "
            "raise SystemExit(0 if not errors else 1)"
        ) % str(TOOLS)
        with tempfile.TemporaryDirectory(prefix="texjoin-foreign-cwd-") as td:
            result = subprocess.run(
                [sys.executable, "-c", probe], cwd=td,
                capture_output=True, text=True,
            )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("FOREIGN_CWD_FRESH=1", result.stdout)

    def test_oracle_fit_target_tests_and_historical_evidence_remain(self) -> None:
        self.assertTrue((REPO / "fpga" / "rtl" / "raster" /
                         "zhao_raster_texjoin_v2.sv").is_file())
        fit_targets = (REPO / "design" / "fit_targets.yml").read_text(encoding="utf-8")
        self.assertRegex(fit_targets, r"(?m)^\s*- top: zhao_raster_texjoin_v2\s*$")
        self.assertTrue((REPO / "tests" / "raster" /
                         "raster_texjoin_v2_directed.cpp").is_file())
        self.assertTrue((REPO / "tests" / "texture" /
                         "fragrob_differential.cpp").is_file())
        evidence = (REPO / "reports" / "synthesis" /
                    "zhao_block_fit.json").read_text(encoding="utf-8")
        self.assertIn('"module":  "zhao_raster_texjoin_v2"', evidence)

    def test_existing_response_and_overflow_controls_remain_named(self) -> None:
        response = (REPO / "tests" / "texture" /
                    "texture_v3own_adversarial.cpp").read_text(encoding="utf-8")
        for required in (
            "stale generation", "not-required source", "out-of-range sample_index 3",
            "duplicate and unsolicited FINAL", "simultaneous TMU + AUX identity faults",
        ):
            self.assertIn(required, response)
        self.assertTrue((REPO / "tests" / "mutants" /
                         "zhao_texture_frag_expand_mutant.sv").is_file())
        self.assertTrue((REPO / "tests" / "texture" /
                         "frag_expand_overflow_control.cpp").is_file())
        cmake = (REPO / "tests" / "CMakeLists.txt").read_text(encoding="utf-8")
        for target in (
            "desc_identity_stall_control",
            "desc_identity_slotswap_control",
            "desc_identity_valid_withdraw_control",
            "desc_identity_substream_drop_control",
            "texture_v3own_bubble_control",
            "texture_v3own_no_same_edge_reload_control",
        ):
            self.assertIn(target, cmake)
        self.assertTrue((MUTANTS /
                         "tb_desc_join_expand_valid_withdraw_mutant.sv").is_file())
        self.assertTrue((MUTANTS /
                         "tb_desc_join_expand_substream_drop_mutant.sv").is_file())


if __name__ == "__main__":
    unittest.main()
