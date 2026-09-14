#!/usr/bin/env python3
"""Direct executable controls for TEXJOIN accounting retirement."""

from __future__ import annotations

import contextlib
import hashlib
import io
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "quartus"
BUDGET_TOOLS = REPO / "tools" / "budget"
DESIGN_TOOLS = REPO / "tools" / "design"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
MUTANTS = REPO / "tests" / "mutants"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
if str(BUDGET_TOOLS) not in sys.path:
    sys.path.insert(0, str(BUDGET_TOOLS))
if str(DESIGN_TOOLS) not in sys.path:
    sys.path.insert(0, str(DESIGN_TOOLS))

import check_counters
import check_ownership_roles as ownership
import check_prod_manifest as manifest
import dsp_census
import gen_prod_top as generator


ROLE = "raster_texture_fragment_lifecycle"
EXPECTED_PROVIDERS = {
    "zhao_texture_v3own",
    "zhao_raster_texjoin_v2",
    "zhao_texture_fragrob",
}

PRE_PACKET_B_TOMBSTONES = {"zhao_raster_texjoin_v2"}
PACKET_B_RETIRED_ROOTS = {
    "zhao_texture_tmu_pipe",
    "zhao_texture_material_combine_v1",
}
EXPECTED_RETIRED_CENSUS_SLOTS = [
    "zhao_raster_texjoin_v2",
    "zhao_texture_material_combine_v1",
    "zhao_texture_tmu_pipe",
]
EXPECTED_PRODUCTION_ROOTS_SHA256 = (
    "a4ddaa66c56a4b1cc33c986e96cb1e18cae0352d1d647f2b00b4659f2a92faed"
)
PACKET_B_NAMED_OLD_ORACLES = {
    "zhao_texture_tmu_pipe",
    "zhao_texture_material_combine_v1",
    "zhao_raster_rcp24_v3",
    "zhao_raster_perspuv_pairpipe",
    "zhao_texture_metajoin",
    "zhao_texture_uv_join",
    "zhao_texture_early_desc",
    "zhao_texture_frag_expand",
    "zhao_texture_tmu_plan",
    "zhao_texture_cache_pipe",
    "zhao_texture_rsp_dispatch",
    "zhao_texture_mosaic",
    "zhao_texture_bilerp_lane",
    "zhao_texture_palette_res",
    "zhao_texture_aux_pipe",
    "zhao_texture_material_combine_v2",
}
# Removing the old TMU root also retires its non-standalone bilerp child. The
# production checker exposes it only after the parent stops contributing `inside`.
PACKET_B_TRANSITIVE_OLD_CHILDREN = {"zhao_texture_bilerp"}
PACKET_B_OLD_ORACLES = (
    PACKET_B_NAMED_OLD_ORACLES | PACKET_B_TRANSITIVE_OLD_CHILDREN
)
EXPECTED_PACKET_B_V3_CLOSURE = {
    "zhao_field_rcp24_rom",
    "zhao_raster_perspuv_pairpipe_v2",
    "zhao_raster_rcp24_mul",
    "zhao_raster_rcp24_v4",
    "zhao_raster_ticketq",
    "zhao_raster_ticketq_rh",
    "zhao_texture_aux_div6",
    "zhao_texture_aux_pipe_v2",
    "zhao_texture_bilerp_lane_v2",
    "zhao_texture_binding_resolver_v2",
    "zhao_texture_cache_pipe_v2",
    "zhao_texture_early_desc_v2",
    "zhao_texture_frag_expand_v2",
    "zhao_texture_material_combine_v3",
    "zhao_texture_metajoin_v2",
    "zhao_texture_mod255",
    "zhao_texture_mosaic_v2",
    "zhao_texture_palette_res_v2",
    "zhao_texture_rsp_dispatch_v2",
    "zhao_texture_tmu_plan_v2",
    "zhao_texture_uv_join_v2",
    "zhao_texture_v3bank",
    "zhao_texture_v3own",
    "zhao_texture_v3rq",
}

V3_ROOT = "zhao_texture_island_v3_top"
SECTION_MARKER = re.compile(r"(?m)^  // ---- (zhao_\w+) ----\n")
TAIL_MARKER = "  // One pin, with every output salted by a distinct private source bit,"
_CANONICAL_TOP_CACHE = {}


def static_module_graph():
    """Load the checker's repository-relative graph from any test cwd."""
    previous_cwd = Path.cwd()
    try:
        os.chdir(REPO)
        return manifest.module_edges()
    finally:
        os.chdir(previous_cwd)


def check_fit_sources_from_repo(decl, edges):
    """Run the repository-relative checker and always restore the caller cwd."""
    previous_cwd = Path.cwd()
    try:
        os.chdir(REPO)
        return manifest.check_fit_sources(decl, edges)
    finally:
        os.chdir(previous_cwd)


def legacy_roots_manifest_text():
    """Reconstruct only the pre-Packet-B root/tombstone selection in memory."""
    text = (REPO / "design" / "prod_manifest.yml").read_text(encoding="utf-8")
    for module in sorted(PACKET_B_RETIRED_ROOTS):
        tombstone = "  - %s\n" % module
        if text.count(tombstone) != 1:
            raise AssertionError("expected one Packet B tombstone for " + module)
        text = text.replace(tombstone, "", 1)
        disposition = re.compile(
            r"(?m)^  - " + re.escape(module) + r":.*\n"
        )
        text, removed = disposition.subn("", text, count=1)
        if removed != 1:
            raise AssertionError(
                "expected one Packet B excluded disposition for " + module
            )
    selected = "  - %s\n" % V3_ROOT
    if text.count(selected) != 1:
        raise AssertionError("expected one selected V3 accounting root")
    restored = "".join(
        "  - %s\n" % module for module in sorted(PACKET_B_RETIRED_ROOTS)
    )
    return text.replace(selected, selected + restored, 1)


def canonical_generated_top(legacy_roots=False):
    """Generate canonical text in a temporary directory without touching RTL."""
    cache_key = "legacy" if legacy_roots else "current"
    if cache_key in _CANONICAL_TOP_CACHE:
        return _CANONICAL_TOP_CACHE[cache_key]

    with tempfile.TemporaryDirectory(prefix="packet-b-canonical-top-") as temporary:
        temporary_path = Path(temporary)
        output = temporary_path / "zhao_prod_top.sv"
        manifest_path = REPO / "design" / "prod_manifest.yml"
        if legacy_roots:
            manifest_path = temporary_path / "prod_manifest_legacy_roots.yml"
            manifest_path.write_text(legacy_roots_manifest_text(), encoding="utf-8")

        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            with contextlib.redirect_stdout(io.StringIO()):
                status = generator.main(
                    manifest_path=manifest_path,
                    out_path=output,
                    check=False,
                )
        finally:
            os.chdir(previous_cwd)
        if status != 0:
            raise AssertionError(
                "canonical production-top generation returned RC %d" % status
            )
        text = output.read_text(encoding="utf-8").replace("\r\n", "\n")

    _CANONICAL_TOP_CACHE[cache_key] = text
    return text


def split_generated_top(text):
    """Split generated bytes into header, ordered module sections and final fold."""
    text = text.replace("\r\n", "\n")
    tail_start = text.find(TAIL_MARKER)
    if tail_start < 0:
        raise AssertionError("generated top has no final fold marker")
    matches = [match for match in SECTION_MARKER.finditer(text)
               if match.start() < tail_start]
    if not matches:
        raise AssertionError("generated top has no module sections")

    order = []
    sections = {}
    for index, match in enumerate(matches):
        name = match.group(1)
        if name in sections:
            raise AssertionError("duplicate generated section " + name)
        end = matches[index + 1].start() if index + 1 < len(matches) else tail_start
        order.append(name)
        sections[name] = text[match.start():end]
    return text[:matches[0].start()], order, sections, text[tail_start:]


def generated_sections():
    text = (REPO / "fpga" / "rtl" / "prod" / "zhao_prod_top.sv").read_text(
        encoding="utf-8"
    )
    return text, re.findall(r"^  // ---- (zhao_\w+) ----$", text, re.MULTILINE)


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
        reachable = observation_map(observations)[ROLE]
        self.assertEqual(reachable, ["zhao_texture_v3own"])
        self.assertNotIn("zhao_raster_texjoin_v2", reachable)
        self.assertNotIn("zhao_texture_fragrob", reachable)

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
    def test_only_packet_b_duplicate_roots_retired_and_ordinals_stay_stable(self) -> None:
        tops, excluded = manifest.read_manifest(REPO / "design" / "prod_manifest.yml")
        retired = manifest.read_list_section(
            "retired_census_slots", REPO / "design" / "prod_manifest.yml"
        )
        checked, current_sections = generated_sections()
        _legacy_prefix, pre_packet_b_sections, _legacy_sections, _legacy_tail = \
            split_generated_top(canonical_generated_top(legacy_roots=True))

        self.assertEqual(tops[:2], ["zhao_shell_top", "zhao_cmd_decoder"])
        self.assertEqual(len(tops), 63)
        self.assertEqual(
            hashlib.sha256(("\n".join(tops) + "\n").encode("utf-8")).hexdigest(),
            EXPECTED_PRODUCTION_ROOTS_SHA256,
        )
        self.assertEqual(tops.count("zhao_texture_island_v3_top"), 1)
        self.assertEqual(current_sections, sorted(tops))
        self.assertEqual(
            set(pre_packet_b_sections) - set(tops), PACKET_B_RETIRED_ROOTS
        )
        self.assertEqual(set(tops) - set(pre_packet_b_sections), set())
        self.assertEqual(checked, canonical_generated_top())
        self.assertEqual(retired, EXPECTED_RETIRED_CENSUS_SLOTS)
        for module in EXPECTED_RETIRED_CENSUS_SLOTS:
            self.assertNotIn(module, tops)
            self.assertEqual(excluded[module][0], "superseded")

        before = dict(generator.census_slots(
            pre_packet_b_sections, PRE_PACKET_B_TOMBSTONES
        ))
        after = dict(generator.census_slots(tops, retired))
        self.assertEqual(len(after), 63)
        self.assertEqual(max(after), 65)
        self.assertEqual(
            {ordinal: module for ordinal, module in before.items()
             if module in tops},
            after,
        )
        self.assertEqual(set(before.values()) - set(after.values()),
                         PACKET_B_RETIRED_ROOTS)
        self.assertEqual(
            {ordinal for ordinal, module in before.items()
             if module in PACKET_B_RETIRED_ROOTS},
            {62, 63},
        )
        self.assertEqual(
            set(range(max(after) + 1)) - set(after),
            {46, 62, 63},
        )
        self.assertEqual(after[45], "zhao_raster_fog")
        self.assertEqual(after[47], "zhao_raster_toon")

    def test_packet_b_counter_maps_bind_selected_v2_v3_ports(self) -> None:
        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            rows = {bid: (names, mapping)
                    for bid, names, mapping in check_counters.blocks()}
            modules = check_counters.modules()
        finally:
            os.chdir(previous_cwd)

        expected = {
            "TEXTURE.AUX": ("zhao_texture_aux_pipe_v2", 12),
            "TEXTURE.COMBINE": ("zhao_texture_material_combine_v3", 15),
        }
        for block_id, (module, count) in expected.items():
            with self.subTest(block=block_id):
                self.assertEqual(check_counters.module_for_block(block_id), module)
                names, mapping = rows[block_id]
                self.assertEqual(len(names), count)
                defaults, mapped, unresolved, absent = check_counters.resolve_block(
                    block_id, names, mapping, modules
                )
                self.assertEqual(defaults, [])
                self.assertEqual(len(mapped), count)
                self.assertEqual(unresolved, [])
                self.assertIsNone(absent)

                damaged = dict(mapping)
                damaged[names[0]] = "impossible_missing_counter_port_o"
                _defaults, _mapped, fired, _absent = check_counters.resolve_block(
                    block_id, names, damaged, modules
                )
                self.assertEqual(len(fired), 1)
                self.assertIn("not a port", fired[0][2])

    def test_packet_b_profile_is_explicitly_unmeasured(self) -> None:
        profiles = dsp_census.load_profiles(REPO / "design" / "prod_manifest.yml")
        self.assertEqual(profiles[V3_ROOT], "packet-b-prod")
        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            evidence = dsp_census.load_evidence()
        finally:
            os.chdir(previous_cwd)
        chosen, reason, _alternates = dsp_census.select(
            evidence.get(V3_ROOT, []), profiles[V3_ROOT]
        )
        self.assertIsNone(chosen)
        self.assertIn("NO usable measurement", reason)
        self.assertIn("@packet-b-prod", reason)

    def test_packet_b_v3_static_closure_is_exact_and_has_no_exclusions(self) -> None:
        decl, edges = static_module_graph()
        closure = manifest.closure(edges, "zhao_texture_island_v3_top")
        tops, excluded = manifest.read_manifest(REPO / "design" / "prod_manifest.yml")

        self.assertEqual(closure, EXPECTED_PACKET_B_V3_CLOSURE)
        self.assertLessEqual(EXPECTED_PACKET_B_V3_CLOSURE, set(decl))
        self.assertEqual(set(excluded) & closure, set())
        self.assertTrue(PACKET_B_OLD_ORACLES.isdisjoint(closure))
        self.assertEqual(tops.count("zhao_texture_island_v3_top"), 1)
        self.assertNotIn("zhao_texture_island_v3_top", excluded)

    def test_static_manifest_and_generated_top_are_closed_and_fresh(self) -> None:
        decl, edges = static_module_graph()
        tops, excluded = manifest.read_manifest(REPO / "design" / "prod_manifest.yml")
        inside = set()
        for top in tops:
            inside.update(manifest.closure(edges, top))

        self.assertEqual(set(tops) & inside, set())
        self.assertEqual(set(excluded) & inside, set())
        self.assertEqual(set(tops) | inside | set(excluded), set(decl))
        self.assertEqual(check_fit_sources_from_repo(decl, edges), [])

        freshness_errors = manifest.check_top_fresh()
        self.assertEqual(freshness_errors, [])

    def test_manifest_dispositions_reject_duplicates_and_overlap(self) -> None:
        controls = {
            "duplicate-top": (
                "top:\n  - zhao_a\n  - zhao_a\nexcluded:\n"
            ),
            "duplicate-excluded": (
                "top:\n  - zhao_a\nexcluded:\n"
                "  - zhao_b: superseded first\n"
                "  - zhao_b: superseded second\n"
            ),
            "top-excluded-overlap": (
                "top:\n  - zhao_a\nexcluded:\n"
                "  - zhao_a: superseded contradiction\n"
            ),
        }
        with tempfile.TemporaryDirectory(prefix="prod-disposition-control-") as temporary:
            path = Path(temporary) / "prod_manifest.yml"
            for name, text in controls.items():
                with self.subTest(control=name):
                    path.write_text(text, encoding="utf-8")
                    with self.assertRaises(manifest.ManifestSchemaError):
                        manifest.read_manifest(path)

    def test_fit_source_gate_fails_on_missing_or_commented_manifest(self) -> None:
        decl = {"zhao_leaf": "fpga/rtl/zhao_leaf.sv"}
        edges = {"zhao_prod_top": {"zhao_leaf"}, "zhao_leaf": set()}
        with tempfile.TemporaryDirectory(prefix="fit-source-controls-") as temporary:
            root = Path(temporary)
            top = root / "zhao_prod_top.sv"
            yml = root / "fit_targets.yml"
            top.write_text("module zhao_prod_top; endmodule\n", encoding="utf-8")

            errors = manifest.check_fit_sources(
                decl, edges, top_path=top, yml=root / "missing.yml")
            self.assertTrue(any("manifest is missing" in error for error in errors), errors)

            yml.write_text(
                "targets:\n  - top: zhao_prod_top\n    sources:\n"
                "      # - fpga/rtl/zhao_leaf.sv\n",
                encoding="utf-8",
            )
            errors = manifest.check_fit_sources(decl, edges, top_path=top, yml=yml)
            self.assertTrue(any("NOT in" in error for error in errors), errors)

            yml.write_text(
                "targets:\n  - top: zhao_prod_top\n    sources:\n"
                "      - fpga/rtl/zhao_leaf.sv\n",
                encoding="utf-8",
            )
            self.assertEqual(
                manifest.check_fit_sources(decl, edges, top_path=top, yml=yml), []
            )

    def test_fit_source_check_inspects_repo_from_foreign_cwd_and_restores_it(self) -> None:
        with tempfile.TemporaryDirectory(prefix="fit-source-foreign-cwd-") as temporary:
            previous_cwd = Path.cwd()
            foreign_cwd = Path(temporary).resolve()
            try:
                os.chdir(foreign_cwd)
                decl, edges = static_module_graph()
                self.assertEqual(Path.cwd().resolve(), foreign_cwd)
                self.assertEqual(check_fit_sources_from_repo(decl, edges), [])
                self.assertEqual(Path.cwd().resolve(), foreign_cwd)

                damaged_decl = dict(decl)
                damaged_decl["zhao_cmd_decoder"] = \
                    "fpga/rtl/command/not_the_selected_decoder.sv"
                errors = check_fit_sources_from_repo(damaged_decl, edges)
                self.assertEqual(Path.cwd().resolve(), foreign_cwd)
                self.assertTrue(any(
                    "zhao_cmd_decoder" in error and "NOT in" in error
                    for error in errors
                ), errors)
            finally:
                os.chdir(previous_cwd)

    def test_every_pre_packet_b_oracle_is_exactly_superseded_and_retained(self) -> None:
        manifest_path = REPO / "design" / "prod_manifest.yml"
        source = manifest_path.read_text(encoding="utf-8")
        tops, excluded = manifest.read_manifest(manifest_path)
        decl, _edges = static_module_graph()
        fit_targets = (REPO / "design" / "fit_targets.yml").read_text(
            encoding="utf-8"
        )
        standalone_fit_tops = set(re.findall(
            r"(?m)^\s*- top:\s*(zhao_\w+)\s*$", fit_targets
        ))

        self.assertTrue(PACKET_B_OLD_ORACLES.isdisjoint(tops))
        self.assertLessEqual(PACKET_B_OLD_ORACLES, set(decl))
        self.assertLessEqual(PACKET_B_NAMED_OLD_ORACLES, standalone_fit_tops)
        for module in PACKET_B_OLD_ORACLES:
            with self.subTest(module=module):
                self.assertEqual(excluded[module][0], "superseded")
                self.assertIn("retained pre-Packet-B", excluded[module][1])
                self.assertEqual(
                    len(re.findall(
                        r"(?m)^\s*-\s*" + re.escape(module) + r"\s*:", source
                    )),
                    1,
                )

    def test_texjoin_and_fragrob_dispositions_are_preserved(self) -> None:
        _tops, excluded = manifest.read_manifest(
            REPO / "design" / "prod_manifest.yml"
        )
        retired = manifest.read_list_section(
            "retired_census_slots", REPO / "design" / "prod_manifest.yml"
        )
        self.assertEqual(excluded["zhao_raster_texjoin_v2"][0], "superseded")
        self.assertIn("behavioural oracle", excluded["zhao_raster_texjoin_v2"][1])
        self.assertEqual(excluded["zhao_texture_fragrob"][0], "not-yet-adopted")
        self.assertIn("retained differential candidate",
                      excluded["zhao_texture_fragrob"][1])
        self.assertEqual(retired[0], "zhao_raster_texjoin_v2")

    def test_generated_top_is_canonical_after_packet_b(self) -> None:
        tops, _excluded = manifest.read_manifest(REPO / "design" / "prod_manifest.yml")
        checked, sections = generated_sections()
        current = canonical_generated_top()
        legacy = canonical_generated_top(legacy_roots=True)
        _legacy_prefix, legacy_order, _legacy_sections, _legacy_tail = \
            split_generated_top(legacy)
        manifest_text = (REPO / "design" / "prod_manifest.yml").read_text(
            encoding="utf-8"
        )

        self.assertEqual(sections, sorted(tops))
        self.assertEqual(set(legacy_order) - set(sections), PACKET_B_RETIRED_ROOTS)
        self.assertEqual(set(sections) - set(legacy_order), set())
        self.assertEqual(len(sections), len(set(sections)))
        self.assertNotIn("zhao_raster_texjoin_v2", sections)
        self.assertNotRegex(checked, r"\bu46_(?:lfsr_q|src|i|fold_q)\b")
        self.assertNotIn("regeneration is intentionally stale", manifest_text)
        self.assertNotIn("pending the coordinator-owned generated", manifest_text)
        self.assertEqual(checked, current)
        self.assertNotEqual(legacy, current)
        self.assertEqual(manifest.check_top_fresh(), [])

    def test_aliased_output_ports_receive_distinct_fold_salts(self) -> None:
        _prefix, _order, sections, _tail = split_generated_top(
            canonical_generated_top()
        )
        section = sections[V3_ROOT]
        salts = {}
        for port in ("frame_fault_clear_ready_o", "quiet_o"):
            match = re.search(
                r"\(\(\(\^u\d+_" + re.escape(port) +
                r"\)\) & u\d+_src\[(\d+)\]\)",
                section,
            )
            self.assertIsNotNone(match, "missing salted fold for " + port)
            salts[port] = int(match.group(1))
        self.assertNotEqual(
            salts["frame_fault_clear_ready_o"], salts["quiet_o"],
            "aliased output ports cancel when folded with the same salt",
        )

    def test_generated_top_comparison_rejects_unrelated_mutations(self) -> None:
        checked, _sections = generated_sections()
        current = canonical_generated_top()
        self.assertEqual(checked, current)

        with tempfile.TemporaryDirectory(
                prefix="packet-b-generated-positive-control-") as temporary:
            damaged_path = Path(temporary) / "zhao_prod_top.sv"

            def require_rejected(text: str, kind: str) -> None:
                damaged_path.write_text(text, encoding="utf-8")
                previous_cwd = Path.cwd()
                try:
                    os.chdir(REPO)
                    with contextlib.redirect_stdout(io.StringIO()):
                        status = generator.main(
                            manifest_path=REPO / "design" / "prod_manifest.yml",
                            out_path=damaged_path,
                            check=True,
                        )
                finally:
                    os.chdir(previous_cwd)
                self.assertEqual(
                    status, 3,
                    "%s mutation escaped the real generator freshness gate" % kind,
                )

            replacements = {
                "port": (
                    ".pkt_valid_i(u00_src[0 +: 1])",
                    ".pkt_valid_MUTANT_i(u00_src[0 +: 1])",
                ),
                "connection": (
                    ".pkt_ready_o(u00_pkt_ready_o)",
                    ".pkt_ready_o(u00_rec_valid_o)",
                ),
                "seed": (
                    "64'h0000000000012345",
                    "64'h0000000000012344",
                ),
                "section": (
                    "// ---- zhao_cmd_decoder ----",
                    "// ---- zhao_cmd_decoder_mutant ----",
                ),
            }
            for kind, (original, mutant) in replacements.items():
                with self.subTest(kind=kind):
                    self.assertIn(original, checked)
                    require_rejected(checked.replace(original, mutant, 1), kind)

            fold_occurrence = checked.rfind("u00_fold_q")
            self.assertGreater(fold_occurrence, 0)
            damaged_fold = (
                checked[:fold_occurrence] + "u01_fold_q" +
                checked[fold_occurrence + len("u00_fold_q"):]
            )
            require_rejected(damaged_fold, "final-fold")

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

    def test_expected_freshness_is_independent_of_process_cwd(self) -> None:
        probe = (
            "import sys; "
            "sys.path.insert(0, %r); "
            "import check_prod_manifest as checker; "
            "errors = checker.check_top_fresh(); "
            "expected = errors == []; "
            "print('FOREIGN_CWD_FRESH=' + ('1' if expected else '0')); "
            "print('\\n'.join(errors)); "
            "raise SystemExit(0 if expected else 1)"
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


class ProductionParameterOverrideTests(unittest.TestCase):
    TOP = "zhao_texture_island_v3_top"
    PARAMETER = "MIGRATION_SHADOWS"

    def test_multiple_module_header_imports_precede_parameters(self) -> None:
        source = (
            "module fixture_multi_import import alpha::*; import beta::*; #(\n"
            "  parameter int unsigned N = 1\n"
            ") (); endmodule\n"
        )
        declarations = manifest.module_parameter_declarations(
            source, "fixture_multi_import"
        )
        self.assertEqual(tuple(declarations), ("N",))
        self.assertEqual(generator.resolve_params(
            source, "fixture_multi_import"), {"N": 1})

    def test_generator_and_validator_share_signed_and_clog2_semantics(self) -> None:
        expressions = (
            ("$clog2(1)", 0),
            ("8'shFF", -1),
            ("4'sb1000", -8),
            ("$clog2(9) + 4'sd1", 5),
        )
        for expression, expected in expressions:
            with self.subTest(expression=expression):
                self.assertEqual(manifest._eval_parameter_expr(expression, {}), expected)
                self.assertEqual(generator.eval_sv(expression, {}), expected)

    @classmethod
    def setUpClass(cls) -> None:
        cls._temporary = tempfile.TemporaryDirectory(
            prefix="prod-parameter-override-"
        )
        cls.output = Path(cls._temporary.name) / "zhao_prod_top.sv"
        cls.default_output = Path(cls._temporary.name) / "zhao_prod_top_defaults.sv"
        cls.parameter_fixture = Path(cls._temporary.name) / \
            "fixture_prod_parameter_types.sv"
        cls.parameter_fixture.write_text(
            "module fixture_prod_parameter_types #(\n"
            "  parameter bit FLAG = 1'b0,\n"
            "  parameter logic signed [3:0] SIGNED_NIBBLE = 4'sd0,\n"
            "  parameter int unsigned UNSIGNED_COUNT = 0,\n"
            "  parameter int unsigned WIDTH = 8,\n"
            "  parameter logic signed [WIDTH-1:0] SIGNED_VECTOR = 8'sd0,\n"
            "  parameter type TYPE_PARAM = logic,\n"
            "  parameter string STRING_PARAM = \"fixture\",\n"
            "  parameter real REAL_PARAM = 0.0\n"
            ") ();\n"
            "endmodule\n",
            encoding="utf-8",
        )
        cls.dependency_fixture = Path(cls._temporary.name) / \
            "fixture_prod_parameter_dependencies.sv"
        cls.dependency_fixture.write_text(
            "module fixture_forward_parameter_width #(\n"
            "  parameter logic [LATER-1:0] EARLY = 1'b0,\n"
            "  parameter int unsigned LATER = 4\n"
            ") ();\n"
            "endmodule\n"
            "module fixture_cyclic_parameter_width #(\n"
            "  parameter logic [RIGHT-1:0] LEFT = 1'b0,\n"
            "  parameter logic [LEFT-1:0] RIGHT = 1'b0\n"
            ") ();\n"
            "endmodule\n",
            encoding="utf-8",
        )
        real_manifest = REPO / "design" / "prod_manifest.yml"
        default_manifest = Path(cls._temporary.name) / "prod_manifest_defaults.yml"
        manifest_text = real_manifest.read_text(encoding="utf-8")
        override_stanza = (
            "production_parameter_overrides:\n"
            "  zhao_texture_island_v3_top:\n"
            "    MIGRATION_SHADOWS: 1'b0\n"
        )
        if manifest_text.count(override_stanza) != 1:
            cls._temporary.cleanup()
            raise AssertionError("expected exactly one production override stanza")
        default_manifest.write_text(
            manifest_text.replace(override_stanza, "", 1), encoding="utf-8"
        )

        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            status = generator.main(
                manifest_path=real_manifest,
                out_path=cls.output,
                check=False,
            )
            default_status = generator.main(
                manifest_path=default_manifest,
                out_path=cls.default_output,
                check=False,
            )
        finally:
            os.chdir(previous_cwd)
        if status != 0 or default_status != 0:
            cls._temporary.cleanup()
            raise AssertionError(
                "temporary production-top generation returned RC %d/%d"
                % (status, default_status)
            )
        cls.generated = cls.output.read_text(encoding="utf-8")
        cls.default_generated = cls.default_output.read_text(encoding="utf-8")

    @classmethod
    def tearDownClass(cls) -> None:
        cls._temporary.cleanup()

    def write_manifest(self, override_section: str) -> Path:
        path = Path(self._temporary.name) / (
            "case-%d.yml" % len(list(Path(self._temporary.name).glob("case-*.yml")))
        )
        path.write_text(
            "top:\n"
            "  - zhao_texture_island_v3_top\n"
            + override_section,
            encoding="utf-8",
        )
        return path

    def assert_shipping_override_is_emitted(self, text: str) -> None:
        self.assertRegex(
            text,
            r"(?m)^  zhao_texture_island_v3_top #\(\n"
            r"      \.MIGRATION_SHADOWS\(1'b0\)\n"
            r"  \) u\d+_i \($",
        )
        self.assertEqual(text.count(".MIGRATION_SHADOWS(1'b0)"), 1)

    def validate(self, path: Path):
        overrides = manifest.read_parameter_overrides(path)
        manifest.validate_parameter_overrides(
            overrides,
            [self.TOP],
            {self.TOP: REPO / "fpga" / "rtl" / "texture" /
             "zhao_texture_island_v3_top.sv"},
            repo_root=REPO,
        )
        return overrides

    def validate_fixture_overrides(self, parameters):
        return manifest.validate_parameter_overrides(
            {"fixture_prod_parameter_types": parameters},
            ["fixture_prod_parameter_types"],
            {"fixture_prod_parameter_types": self.parameter_fixture},
            repo_root=REPO,
        )

    def validate_dependency_overrides(self, module_name, parameters):
        return manifest.validate_parameter_overrides(
            {module_name: parameters},
            [module_name],
            {module_name: self.dependency_fixture},
            repo_root=REPO,
        )

    def test_manifest_declares_exact_selected_production_override(self) -> None:
        path = REPO / "design" / "prod_manifest.yml"
        overrides = manifest.read_parameter_overrides(path)
        self.assertEqual(
            overrides,
            {self.TOP: {self.PARAMETER: "1'b0"}},
        )
        tops, _excluded = manifest.read_manifest(path)
        self.assertEqual(
            manifest.validate_parameter_overrides(
                overrides,
                tops,
                {self.TOP: REPO / "fpga" / "rtl" / "texture" /
                 "zhao_texture_island_v3_top.sv"},
                repo_root=REPO,
            ),
            {self.TOP: {self.PARAMETER: 0}},
        )

    def test_parameter_declaration_metadata_is_explicit(self) -> None:
        source = self.parameter_fixture.read_text(encoding="utf-8")
        declarations = manifest.module_parameter_declarations(
            source, "fixture_prod_parameter_types"
        )
        self.assertEqual(
            {key: declarations["FLAG"][key]
             for key in ("kind", "type", "integral", "signed", "width")},
            {"kind": "value", "type": "bit", "integral": True,
             "signed": False, "width": 1},
        )
        self.assertEqual(declarations["SIGNED_NIBBLE"]["width"], 4)
        self.assertTrue(declarations["SIGNED_NIBBLE"]["signed"])
        self.assertEqual(declarations["SIGNED_VECTOR"]["width"], 8)
        self.assertTrue(declarations["SIGNED_VECTOR"]["signed"])
        self.assertEqual(declarations["UNSIGNED_COUNT"]["width"], 32)
        self.assertFalse(declarations["UNSIGNED_COUNT"]["signed"])
        self.assertEqual(declarations["TYPE_PARAM"]["kind"], "type")
        self.assertFalse(declarations["STRING_PARAM"]["integral"])
        self.assertFalse(declarations["REAL_PARAM"]["integral"])

    def test_representable_values_return_exact_coerced_integers(self) -> None:
        self.assertEqual(
            self.validate_fixture_overrides({
                "FLAG": "2'b01",
                "SIGNED_NIBBLE": "4'b0111",
                "UNSIGNED_COUNT": "32'hFFFFFFFF",
            }),
            {"fixture_prod_parameter_types": {
                "FLAG": 1,
                "SIGNED_NIBBLE": 7,
                "UNSIGNED_COUNT": 0xFFFFFFFF,
            }},
        )

    def test_bit_parameter_rejects_unrepresentable_value(self) -> None:
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, r"1-bit unsigned"):
            self.validate_fixture_overrides({"FLAG": "2'b10"})

    def test_signed_parameter_rejects_reinterpreted_value(self) -> None:
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, r"4-bit signed"):
            self.validate_fixture_overrides({"SIGNED_NIBBLE": "4'b1000"})

    def test_int_unsigned_parameter_rejects_33_bit_value(self) -> None:
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, r"32-bit unsigned"):
            self.validate_fixture_overrides({
                "UNSIGNED_COUNT": "33'h100000000",
            })

    def test_preceding_width_override_controls_dependent_signed_width(self) -> None:
        source = self.parameter_fixture.read_text(encoding="utf-8")
        effective = manifest.module_parameter_declarations(
            source,
            "fixture_prod_parameter_types",
            effective_values={"WIDTH": 4},
        )
        self.assertEqual(effective["SIGNED_VECTOR"]["width"], 4)
        self.assertTrue(effective["SIGNED_VECTOR"]["signed"])

        # Deliberately put the dependent key first: declaration order still
        # validates WIDTH=4 before SIGNED_VECTOR. 127 then coerces to 4'b1111,
        # which is -1 as the declared signed value and must be refused.
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError,
                r"4-bit signed integral parameter.*produce -1"):
            self.validate_fixture_overrides({
                "SIGNED_VECTOR": "8'b01111111",
                "WIDTH": "32'd4",
            })

    def test_forward_width_dependency_fails_closed(self) -> None:
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, r"packed width is unresolved"):
            self.validate_dependency_overrides(
                "fixture_forward_parameter_width", {"EARLY": "1'b0"}
            )

    def test_cyclic_width_dependency_fails_closed(self) -> None:
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, r"packed width is unresolved"):
            self.validate_dependency_overrides(
                "fixture_cyclic_parameter_width", {"LEFT": "1'b0"}
            )

    def test_type_parameter_rejects_integral_literal(self) -> None:
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, r"is a type parameter"):
            self.validate_fixture_overrides({"TYPE_PARAM": "1'b0"})

    def test_string_and_real_parameters_reject_integral_literals(self) -> None:
        for parameter in ("STRING_PARAM", "REAL_PARAM"):
            with self.subTest(parameter=parameter):
                with self.assertRaisesRegex(
                        manifest.ManifestSchemaError,
                        r"not a supported integral scalar"):
                    self.validate_fixture_overrides({parameter: "1'b0"})

    def test_temporary_generated_top_emits_named_override_before_instance(self) -> None:
        self.assert_shipping_override_is_emitted(self.generated)

    def test_override_block_is_the_only_generated_delta(self) -> None:
        normalized, replacements = re.subn(
            r"  zhao_texture_island_v3_top #\(\n"
            r"      \.MIGRATION_SHADOWS\(1'b0\)\n"
            r"  \) (u\d+)_i \(",
            r"  zhao_texture_island_v3_top \1_i (",
            self.generated,
        )
        self.assertEqual(replacements, 1)
        self.assertEqual(normalized, self.default_generated)
        self.assertNotIn(".MIGRATION_SHADOWS(", self.default_generated)

    def test_checker_sees_parameterized_and_default_instances(self) -> None:
        tops, _excluded = manifest.read_manifest(
            REPO / "design" / "prod_manifest.yml"
        )
        self.assertEqual(
            manifest.generated_top_direct_modules(self.generated), set(tops)
        )
        self.assertIn(
            self.TOP, manifest.generated_top_direct_modules(self.generated)
        )
        self.assertIn(
            "zhao_geom_skin",
            manifest.generated_top_direct_modules(self.generated),
        )

    def test_override_omission_control_fires_real_generator_check(self) -> None:
        damaged, replacements = re.subn(
            r"  zhao_texture_island_v3_top #\(\n"
            r"      \.MIGRATION_SHADOWS\(1'b0\)\n"
            r"  \) (u\d+)_i \(",
            r"  zhao_texture_island_v3_top \1_i (",
            self.generated,
        )
        self.assertEqual(replacements, 1)
        damaged_output = Path(self._temporary.name) / \
            "zhao_prod_top_override_omitted.sv"
        damaged_output.write_text(damaged, encoding="utf-8", newline="\n")

        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            status = generator.main(
                manifest_path=REPO / "design" / "prod_manifest.yml",
                out_path=damaged_output,
                check=True,
            )
        finally:
            os.chdir(previous_cwd)
        self.assertEqual(status, 3)

    def test_no_override_retains_legacy_instance_bytes(self) -> None:
        self.assertEqual(
            generator.instance_declaration_lines(
                "fixture_default_top", "u07", [".a_i(u07_src[0 +: 1])"]
            ),
            [
                "  fixture_default_top u07_i (",
                "      .a_i(u07_src[0 +: 1])",
                "  );",
            ],
        )
        self.assertRegex(
            self.generated,
            r"(?m)^  zhao_geom_skin u\d+_i \($",
        )
        self.assertNotRegex(
            self.generated,
            r"(?m)^  zhao_geom_skin #\($",
        )

    def test_override_changes_parameter_resolution_before_dependent_defaults(self) -> None:
        source = (
            "module fixture_params #(parameter int A = 4, "
            "parameter int B = A + 1) (); endmodule\n"
        )
        self.assertEqual(
            generator.resolve_params(source, "fixture_params", {"A": 8}),
            {"A": 8, "B": 9},
        )

    def test_generator_refuses_invalid_override_without_writing(self) -> None:
        cases = {
            "malformed": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: false\n"
            ),
            "unknown_top": (
                "production_parameter_overrides:\n"
                "  not_a_selected_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
            ),
            "unknown_parameter": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "    NOT_A_REAL_PROD_PARAMETER: 1'b0\n"
            ),
        }
        declaration = REPO / "fpga" / "rtl" / "texture" / \
            "zhao_texture_island_v3_top.sv"
        original_build = generator.build
        try:
            generator.build = lambda: ({self.TOP: declaration}, {})
            for kind, section in cases.items():
                with self.subTest(kind=kind):
                    case_manifest = self.write_manifest(section)
                    output = Path(self._temporary.name) / (kind + "-refused.sv")
                    self.assertEqual(
                        generator.main(
                            manifest_path=case_manifest,
                            out_path=output,
                            check=False,
                        ),
                        2,
                    )
                    self.assertFalse(output.exists())
        finally:
            generator.build = original_build

    def test_unknown_override_top_fails_closed(self) -> None:
        path = self.write_manifest(
            "production_parameter_overrides:\n"
            "  not_a_selected_top:\n"
            "    MIGRATION_SHADOWS: 1'b0\n"
        )
        overrides = manifest.read_parameter_overrides(path)
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, "not present in top"):
            manifest.validate_parameter_overrides(
                overrides,
                [self.TOP],
                {self.TOP: REPO / "fpga" / "rtl" / "texture" /
                 "zhao_texture_island_v3_top.sv"},
                repo_root=REPO,
            )

    def test_unknown_override_parameter_fails_closed(self) -> None:
        path = self.write_manifest(
            "production_parameter_overrides:\n"
            "  zhao_texture_island_v3_top:\n"
            "    NOT_A_REAL_PROD_PARAMETER: 1'b0\n"
        )
        with self.assertRaisesRegex(
                manifest.ManifestSchemaError, "is not declared"):
            self.validate(path)

    def test_duplicate_override_top_and_parameter_fail_closed(self) -> None:
        cases = {
            "top": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
                "  zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
            ),
            "parameter": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
            ),
        }
        for kind, section in cases.items():
            with self.subTest(kind=kind):
                with self.assertRaisesRegex(
                        manifest.ManifestSchemaError, "duplicate"):
                    manifest.read_parameter_overrides(
                        self.write_manifest(section)
                    )

    def test_every_malformed_override_shape_fails_closed(self) -> None:
        cases = {
            "empty_section": "production_parameter_overrides:\n",
            "section_scalar": "production_parameter_overrides: enabled\n",
            "list_top": (
                "production_parameter_overrides:\n"
                "  - zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
            ),
            "wrong_parameter_indent": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "   MIGRATION_SHADOWS: 1'b0\n"
            ),
            "empty_top": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
            ),
            "duplicate_section": (
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
                "production_parameter_overrides:\n"
                "  zhao_texture_island_v3_top:\n"
                "    MIGRATION_SHADOWS: 1'b0\n"
            ),
        }
        for kind, section in cases.items():
            with self.subTest(kind=kind):
                with self.assertRaises(manifest.ManifestSchemaError):
                    manifest.read_parameter_overrides(
                        self.write_manifest(section)
                    )

    def test_every_noncanonical_or_unsafe_value_fails_closed(self) -> None:
        values = (
            "0", "false", '"1\'b0"', "1'B0", "1'bx", "01'b0",
            "1'b00", "4'h0a", "4'd07", "2'b100", "1'b0;",
        )
        for value in values:
            with self.subTest(value=value):
                path = self.write_manifest(
                    "production_parameter_overrides:\n"
                    "  zhao_texture_island_v3_top:\n"
                    "    MIGRATION_SHADOWS: %s\n" % value
                )
                with self.assertRaises(manifest.ManifestSchemaError):
                    manifest.read_parameter_overrides(path)


if __name__ == "__main__":
    unittest.main()
