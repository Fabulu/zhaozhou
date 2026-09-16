#!/usr/bin/env python3
"""Packet-I G8B generated-wrapper, fit registration, and receipt-law controls.

WHY THIS FILE EXISTS. tests/CMakeLists.txt introduces the G8B block with

    Same four gates as Packet F's, for the same reasons: a directed activity
    witness so the fit cannot characterise an idle box, a synthesis-mode lint, a
    generated-freshness check so the committed wrapper cannot drift from its
    generator, and a static registration check.

and then registers THREE. The fourth -- this one -- was described, justified and
never written, which is the uncashed-cheque shape CLAUDE.md records: the plan is
correct, the prerequisites are built, the final step never happens, and nothing
in the tree is watching for it. Packet F has its static check at
tests/tools/test_raster_texture_v3_fit_top.py; Packet I had none, so the four
places that state G8B's source closure could drift apart in silence and the
architecture's own receipt conditions were enforced by nobody.

WHAT IT CHECKS, and each check has a positive control that fires it:

  1. the manifest's per-file hashes describe the files actually on disk;
  2. FOUR statements of one fact agree -- the generator's SOURCE_CLOSURE, the
     manifest's source_closure, design/fit_targets.yml's G8B sources, and
     tests/CMakeLists.txt's ZHAO_G8B_TERRAIN_SOURCES -- in the same order;
  3. ROWS_PER_PASS and MATW are LITERALS in the wrapper and match the manifest;
  4. the external boundary is exactly the manifest's four registered ports;
  5. the activity witness drives all three legal view masks and refuses to
     characterise a projector that never loaded its matrix;
  6. the fit target is ruled on Fmax and DELIBERATELY unruled on area/DSP;
  7. the four Packet-I ctest names are registered, labelled and inventoried;
  8. THE RECEIPT LAW -- see the docstring of the receipt test. This is the one
     that is not bookkeeping.
"""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import unittest


REPO = Path(__file__).resolve().parents[2]
GENERATOR_PATH = REPO / "tools/quartus/gen_terrain_pipe_rpp3_matw18_fit_top.py"
TEMPLATE = REPO / "tools/quartus/templates/zhao_terrain_pipe_rpp3_matw18_fit_top.sv.in"
WRAPPER = REPO / "fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.sv"
MANIFEST = REPO / "fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.manifest.json"
FIT_TARGETS = REPO / "design/fit_targets.yml"
CMAKELISTS = REPO / "tests/CMakeLists.txt"
BLOCK_FIT = REPO / "reports/synthesis/zhao_block_fit.json"

TOP = "zhao_terrain_pipe_rpp3_matw18_fit_top"

spec = importlib.util.spec_from_file_location("g8b_generator", GENERATOR_PATH)
if spec is None or spec.loader is None:
    raise RuntimeError("could not load G8B generator")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)
EXPECTED_SOURCES = tuple(generator.SOURCE_CLOSURE)

EXPECTED_TESTS = (
    "terrain_pipe_rpp3_matw18_fit_top_directed",
    "lint_terrain_pipe_rpp3_matw18_fit_top",
    "terrain_pipe_rpp3_matw18_generated_freshness",
    "packet_i_g8b_registration_static",
)

# zhao_terrain_pipe's OWN declared defaults, so the test can say which literal is
# load-bearing. ROWS_PER_PASS already defaults to 3; MATW defaults to 32. That
# asymmetry is the whole reason the architecture forbids inheriting either: a fit
# that silently took the defaults would characterise the RIGHT rows-per-pass and
# the WRONG matrix width, and would look exactly like a correct G8B run.
PIPE_DEFAULTS = {"ROWS_PER_PASS": 3, "MATW": 32}


def sha256_bytes(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def load_manifest() -> dict:
    return json.loads(MANIFEST.read_text(encoding="utf-8"))


def fit_target_block(text: str) -> tuple[tuple[str, ...], dict[str, int]]:
    """The G8B entry's source list and rule map, parsed from fit_targets.yml."""
    lines = text.splitlines()
    starts = [index for index, line in enumerate(lines)
              if line.strip() == f"- top: {TOP}"]
    if len(starts) != 1:
        raise AssertionError("G8B fit target is not exact/unique")
    sources: list[str] = []
    rules: dict[str, int] = {}
    in_sources = False
    in_rules = False
    for line in lines[starts[0] + 1:]:
        if re.match(r"^\s{2}- top:", line):
            break
        stripped = line.strip()
        if stripped == "sources:":
            in_sources, in_rules = True, False
        elif stripped == "rules:":
            in_sources, in_rules = False, True
        elif in_sources and re.fullmatch(
                r"- (?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", stripped):
            sources.append(stripped[2:])
        elif in_rules:
            match = re.fullmatch(r"(max_alms|max_dsp|min_fmax_mhz): (\d+)", stripped)
            if match:
                rules[match.group(1)] = int(match.group(2))
    return tuple(sources), rules


def cmake_sources(text: str) -> tuple[str, ...]:
    match = re.search(r"set\(ZHAO_G8B_TERRAIN_SOURCES\n(.*?)\)\n", text, re.DOTALL)
    if match is None:
        raise AssertionError("G8B CMake source list is missing")
    found = re.findall(
        r"\$\{CMAKE_SOURCE_DIR\}/((?:fpga|tests)/[A-Za-z0-9_./-]+\.sv)",
        match.group(1))
    return tuple(found)


def strip_line_comments(text: str) -> str:
    """Drop `//` comments.

    Load-bearing, and it caught itself: the wrapper's HEADER quotes the exact
    instantiation it is about to make -- `zhao_terrain_pipe #(.ROWS_PER_PASS(3),
    .MATW(18))` -- so counting the literal across the whole file finds TWO and a
    check that the parameter is set "exactly once" passes on a file where the
    prose says it and the RTL does not. Prose that agrees with the code is not
    the code.
    """
    return "\n".join(line.split("//", 1)[0] for line in text.splitlines())


def module_port_list(wrapper_text: str) -> tuple[tuple[str, str, int], ...]:
    """(name, direction, bit_width) for the wrapper's external ports, in order."""
    match = re.search(rf"^module {TOP} \(\n(.*?)^\);", wrapper_text,
                      re.DOTALL | re.MULTILINE)
    if match is None:
        raise AssertionError("G8B wrapper module header is missing")
    ports: list[tuple[str, str, int]] = []
    for line in match.group(1).splitlines():
        stripped = line.strip().rstrip(",")
        if not stripped:
            continue
        found = re.search(
            r"(input|output)\s+logic\s*(?:\[\s*(\d+)\s*:\s*(\d+)\s*\]\s*)?([A-Za-z_]\w*)",
            stripped)
        if found is None:
            raise AssertionError(f"unparsed G8B port declaration: {stripped!r}")
        direction = found.group(1)
        if found.group(2) is None:
            width = 1
        else:
            width = int(found.group(2)) - int(found.group(3)) + 1
        ports.append((found.group(4), direction, width))
    return tuple(ports)


def g8b_rows(payload: dict) -> list[dict]:
    return [row for row in payload["blocks"]
            if str(row.get("module", "")).split("@", 1)[0] == TOP]


# ---------------------------------------------------------------------------
# THE RECEIPT LAW, factored out so the positive controls drive the same code the
# gate does rather than a paraphrase of it. Returns a list of violation strings.
# ---------------------------------------------------------------------------
REQUIRED_ROW_FIELDS = ("status", "sourceCommit", "rtlCleanAtHead", "sourceDigest",
                       "fitterSeed", "ioMode", "virtualPins")


def receipt_violations(rows: list[dict]) -> list[str]:
    problems: list[str] = []
    for row in rows:
        name = row.get("module", "<unnamed>")
        for field in REQUIRED_ROW_FIELDS:
            if row.get(field) is None:
                problems.append(f"{name}: required receipt field {field} is absent")
        io_mode = row.get("ioMode")
        virtual_pins = row.get("virtualPins")
        if io_mode not in (None, "physical-top-ports", "virtual-top-ports"):
            problems.append(f"{name}: unknown ioMode {io_mode!r}")
        if (io_mode == "physical-top-ports" and virtual_pins not in (None, 0)):
            problems.append(
                f"{name}: declares physical-top-ports while reporting "
                f"{virtual_pins} virtual pin(s)")
        if (io_mode == "virtual-top-ports" and virtual_pins in (None, 0)):
            problems.append(
                f"{name}: declares virtual-top-ports while reporting "
                f"{virtual_pins} virtual pin(s)")
        status = str(row.get("status", ""))
        if status.startswith("failed") or status.startswith("timeout"):
            continue
        # An ACCEPTING row -- one not stamped failed -- carries the whole of the
        # architecture's G8B receipt condition.
        if row.get("rtlCleanAtHead") is not True:
            problems.append(f"{name}: accepted from a tree that is not clean at HEAD")
        if virtual_pins not in (0,):
            problems.append(
                f"{name}: accepted with {virtual_pins} virtual pin(s); the G8B "
                f"receipt gate requires zero")
        fmax = row.get("fmaxMhz")
        if not isinstance(fmax, (int, float)) or fmax < 100:
            problems.append(f"{name}: accepted at Fmax {fmax}, below the ruled 100 MHz")
    return problems


class G8BFitTopTests(unittest.TestCase):

    # -- 1 ------------------------------------------------------------------
    def test_manifest_hashes_describe_the_files_on_disk(self) -> None:
        manifest = load_manifest()
        closure = manifest["source_closure"]
        self.assertEqual([entry["ordinal"] for entry in closure],
                         list(range(len(closure))),
                         "source_closure ordinals are not 0..n-1 in order")
        for entry in closure:
            path = REPO / entry["path"]
            self.assertTrue(path.is_file(), f"declared G8B source is missing: {path}")
            self.assertEqual(entry["sha256"], sha256_bytes(path.read_bytes()),
                             f"manifest hash does not describe {entry['path']}")
        self.assertEqual(closure[-1]["path"], generator.WRAPPER_REL)
        self.assertEqual(closure[-1]["sha256"],
                         manifest["hashes"]["generated_rtl_sha256"],
                         "the wrapper's closure hash and its own hash disagree")
        self.assertEqual(manifest["hashes"]["generator_sha256"],
                         sha256_bytes(GENERATOR_PATH.read_bytes()))
        self.assertEqual(manifest["hashes"]["template_sha256"],
                         sha256_bytes(TEMPLATE.read_bytes()))

    def test_manifest_hash_detector_fires(self) -> None:
        manifest = load_manifest()
        entry = dict(manifest["source_closure"][0])
        entry["sha256"] = "0" * 64
        path = REPO / entry["path"]
        self.assertNotEqual(entry["sha256"], sha256_bytes(path.read_bytes()),
                            "the hash detector cannot distinguish a wrong digest")

    # -- 2 ------------------------------------------------------------------
    def test_four_statements_of_the_source_closure_agree(self) -> None:
        manifest_paths = tuple(entry["path"]
                               for entry in load_manifest()["source_closure"])
        target_paths, _ = fit_target_block(FIT_TARGETS.read_text(encoding="utf-8"))
        cmake_paths = cmake_sources(CMAKELISTS.read_text(encoding="utf-8"))
        self.assertEqual(manifest_paths, EXPECTED_SOURCES,
                         "manifest closure differs from the generator's")
        self.assertEqual(target_paths, EXPECTED_SOURCES,
                         "design/fit_targets.yml differs from the generator's closure")
        self.assertEqual(cmake_paths, EXPECTED_SOURCES,
                         "ZHAO_G8B_TERRAIN_SOURCES differs from the generator's closure")
        self.assertEqual(len(EXPECTED_SOURCES), 9)

    def test_source_closure_parity_detector_fires(self) -> None:
        text = CMAKELISTS.read_text(encoding="utf-8").replace(
            "  ${CMAKE_SOURCE_DIR}/fpga/rtl/terrain/zhao_terrain_tess.sv\n", "")
        self.assertNotEqual(cmake_sources(text), EXPECTED_SOURCES,
                            "a dropped CMake source does not change the parsed list")
        # Every occurrence, not the first: zhao_terrain_tess.sv is in several
        # fit targets, so dropping only the first mutates somebody else's block
        # and this detector reports "cannot fire" when it simply never aimed at
        # the G8B entry.
        broken = FIT_TARGETS.read_text(encoding="utf-8").replace(
            "      - fpga/rtl/terrain/zhao_terrain_tess.sv\n", "")
        self.assertNotEqual(fit_target_block(broken)[0], EXPECTED_SOURCES,
                            "a dropped fit-target source does not change the parsed list")

    # -- 3 ------------------------------------------------------------------
    def test_parameters_are_literals_and_match_the_manifest(self) -> None:
        text = WRAPPER.read_text(encoding="utf-8")
        squeezed = re.sub(r"[ \t]+", "", strip_line_comments(text))
        for name, value in load_manifest()["fit_top_parameters"].items():
            self.assertEqual(squeezed.count(f".{name}({value})"), 1,
                             f"the wrapper does not set .{name}({value}) exactly once")
            self.assertEqual(
                re.sub(r"[ \t]+", "", text).count(f".{name}({value})"), 2,
                f"the wrapper header no longer quotes .{name}({value}); if the "
                f"prose was removed rather than the RTL this check is now blind")
        self.assertEqual(load_manifest()["fit_top_parameters"],
                         generator.FIT_TOP_PARAMETERS)
        self.assertEqual(generator.FIT_TOP_PARAMETERS["MATW"], 18)
        self.assertEqual(generator.FIT_TOP_PARAMETERS["ROWS_PER_PASS"], 3)
        # WHICH LITERAL IS LOAD-BEARING. Read the pipe's own declared defaults
        # rather than trusting PIPE_DEFAULTS, then assert the asymmetry: MATW
        # must differ from its default (so an inherited value would characterise
        # a different circuit), while ROWS_PER_PASS legitimately agrees with its
        # default (so the literal is provenance, not a behaviour change). Saying
        # "both are explicit, therefore both are checked" would be the flattering
        # reading -- only one of them can be caught by the elaborated value.
        pipe = (REPO / "fpga/rtl/terrain/zhao_terrain_pipe.sv").read_text(
            encoding="utf-8")
        for name, expected_default in PIPE_DEFAULTS.items():
            found = re.search(
                rf"parameter\s+int\s+unsigned\s+{name}\s*=\s*(\d+)", pipe)
            self.assertIsNotNone(found, f"{name} has no declared default in the pipe")
            self.assertEqual(int(found.group(1)), expected_default,
                             f"{name}'s declared default moved; this test's "
                             f"asymmetry claim needs rechecking")
        self.assertNotEqual(generator.FIT_TOP_PARAMETERS["MATW"],
                            PIPE_DEFAULTS["MATW"],
                            "MATW no longer differs from its default, so an "
                            "inherited MATW would be undetectable in the receipt")

    # -- 4 ------------------------------------------------------------------
    def test_external_boundary_is_exactly_the_manifest_ports(self) -> None:
        text = WRAPPER.read_text(encoding="utf-8")
        declared = module_port_list(text)
        manifest_ports = tuple(
            (entry["name"], entry["direction"], entry["bit_width"])
            for entry in sorted(load_manifest()["external_ports"],
                                key=lambda entry: entry["ordinal"]))
        self.assertEqual(declared, manifest_ports,
                         "the wrapper's port list and the manifest's disagree")
        self.assertEqual(len(declared), 4, "the G8B boundary is not the four ports")
        for name, direction, _ in declared:
            if direction == "output":
                self.assertIn(f"(* useioff = 1 *) output logic [7:0] {name}", text,
                              f"{name} is not declared register-in-IO")

    def test_port_parser_fires_on_a_widened_boundary(self) -> None:
        text = WRAPPER.read_text(encoding="utf-8").replace(
            "output logic [7:0] fit_epoch_o", "output logic [15:0] fit_epoch_o", 1)
        self.assertNotEqual(module_port_list(text)[-1][2], 8,
                            "the port parser cannot see a width change")

    # -- 5 ------------------------------------------------------------------
    def test_activity_witness_covers_every_legal_mask_and_refuses_an_idle_box(self) -> None:
        text = WRAPPER.read_text(encoding="utf-8")
        for mask in ("2'b01", "2'b10", "2'b11"):
            self.assertIn(mask, text, f"the wrapper never drives view mask {mask}")
        self.assertEqual(load_manifest()["traffic_profile"]["view_masks"],
                         ["01", "10", "11"])
        self.assertEqual(load_manifest()["traffic_profile"]["receipt_workload"], "11")
        self.assertIn("assert (mat_refused_w == 32'd0)", text,
                      "nothing stops G8B characterising an unconfigured projector")

    # -- 6 ------------------------------------------------------------------
    def test_fit_target_is_ruled_on_fmax_and_deliberately_unruled_on_area(self) -> None:
        _, rules = fit_target_block(FIT_TARGETS.read_text(encoding="utf-8"))
        self.assertEqual(rules, {"min_fmax_mhz": 100},
                         "the G8B fit target's rules moved. 100 MHz is the "
                         "machine's operating requirement; an area or DSP rule "
                         "here would be a budget invented before the first "
                         "measurement, which the architecture forbids and which "
                         "is how the stale terrain rows in that file came to exist")

    # -- 7 ------------------------------------------------------------------
    def test_packet_i_ctest_inventory_is_registered_and_labelled(self) -> None:
        text = CMAKELISTS.read_text(encoding="utf-8")
        for name in EXPECTED_TESTS:
            self.assertEqual(text.count(f"add_test(NAME {name}"), 1,
                             f"Packet-I ctest is not registered exactly once: {name}")
        match = re.search(r"set\(ZHAO_PACKET_I_REQUIRED_TESTS\n(.*?)\)\n",
                          text, re.DOTALL)
        self.assertIsNotNone(match, "the Packet-I required-test inventory is missing")
        inventory = tuple(line.strip() for line in match.group(1).splitlines()
                          if line.strip())
        self.assertEqual(inventory, EXPECTED_TESTS,
                         "the Packet-I inventory and this test disagree on the four gates")
        labels = re.search(
            r"PROPERTIES LABELS \"fast;nightly;packet-i;g8b\"", text)
        self.assertIsNotNone(labels, "the Packet-I gates are not labelled packet-i;g8b")

    # -- 8 ------------------------------------------------------------------
    def test_committed_g8b_receipt_rows_obey_the_packet_i_receipt_law(self) -> None:
        """The architecture's G8B receipt condition, enforced on the committed rows.

        NOT an assertion that G8B has passed. It has not: every committed row is
        stamped `failed:structure` against the ruled 100 MHz, which is the fit
        completing and the BUDGET RULES refusing it -- a real measurement, not a
        failed one. What this enforces is that the moment a row stops being
        stamped failed, it carries the whole condition the architecture names:
        clean tree, zero virtual pins, 100 MHz. A promoted diagnostic row turns
        this red, which is the only moment the check is worth anything.

        The ioMode/virtualPins consistency clause is not bookkeeping. The G8B
        timing campaign SWITCHED PIN MODES mid-series: @g8b and @g8b-t2 are
        physical-top-ports (0 virtual pins) at 43.94 and 43.54 MHz, and
        @g8b-t12, @g8b-t1b and @g8b-t3bc are virtual-top-ports (18) at 57.87,
        73.59 and 74.17. The reported jump from 43.54 to 57.87 lands on exactly
        that boundary, and a leaf fit's virtual-pin boundary is documented in
        CLAUDE.md as flattering by a few MHz. Requiring the field to be present
        and self-consistent on every row is what lets a reader see the break
        instead of reading five rows as one series.
        """
        rows = g8b_rows(json.loads(BLOCK_FIT.read_text(encoding="utf-8")))
        self.assertGreaterEqual(len(rows), 1, "no G8B row is committed at all")
        problems = receipt_violations(rows)
        self.assertEqual(problems, [], "G8B receipt law violations:\n  " +
                         "\n  ".join(problems))

    def test_receipt_law_detectors_fire(self) -> None:
        base = {"module": f"{TOP}@probe", "status": "ok",
                "sourceCommit": "0" * 40, "rtlCleanAtHead": True,
                "sourceDigest": "a" * 64, "fitterSeed": 1,
                "ioMode": "physical-top-ports", "virtualPins": 0,
                "fmaxMhz": 101.0}
        self.assertEqual(receipt_violations([dict(base)]), [],
                         "a fully compliant row is rejected")
        for field in REQUIRED_ROW_FIELDS:
            row = dict(base)
            row.pop(field)
            self.assertTrue(any(field in problem
                                for problem in receipt_violations([row])),
                            f"a row missing {field} is accepted")
        dirty = dict(base, rtlCleanAtHead=False)
        self.assertTrue(receipt_violations([dirty]), "a dirty accepted row is accepted")
        virtual = dict(base, ioMode="virtual-top-ports", virtualPins=18)
        self.assertTrue(receipt_violations([virtual]),
                        "an accepted row with virtual pins is accepted")
        slow = dict(base, fmaxMhz=74.17)
        self.assertTrue(receipt_violations([slow]),
                        "an accepted row below 100 MHz is accepted")
        mismatched = dict(base, ioMode="virtual-top-ports", virtualPins=0,
                          status="failed:structure")
        self.assertTrue(receipt_violations([mismatched]),
                        "an ioMode that contradicts virtualPins is accepted")
        failed = dict(base, status="failed:structure", fmaxMhz=43.94)
        self.assertEqual(receipt_violations([failed]), [],
                         "a row honestly stamped failed is treated as a claim of "
                         "acceptance; diagnostic rows must stay legal")


if __name__ == "__main__":
    unittest.main()
