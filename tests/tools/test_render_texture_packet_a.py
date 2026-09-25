#!/usr/bin/env python3
"""Executable Packet-A gates for render/texture packed types and ownership HOLDs."""

from __future__ import annotations

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "quartus"
PACKAGE = REPO / "fpga" / "rtl" / "common" / "zhao_render_texture_pkg.sv"
# The fragment state word's schema is its OWN package: `zhao_render_texture_pkg`
# is inside the texture island's frozen interface manifest, where a single added
# comment moves three pinned hashes and a packed struct moves the duplicate-name
# marker count. See that file's header.
FRAG_PACKAGE = REPO / "fpga" / "rtl" / "common" / "zhao_fragment_state_pkg.sv"
FIXTURE = Path(__file__).resolve().parent / "fixtures" / "zhao_render_texture_layout_top.sv"
MUTANT = REPO / "tests" / "mutants" / "zhao_render_texture_wrong_layout_mutant.sv"
OBSERVATION_SUCCESSOR_MANIFEST = (
    REPO / "tests" / "raster" /
    "packetb_observation_successors.sources.txt"
)
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
PACKET_B_SOURCES = (
    "fpga/rtl/common/zhao_render_texture_pkg.sv",
    "fpga/rtl/field/zhao_field_rcp24_rom.sv",
    "fpga/rtl/raster/zhao_raster_ticketq.sv",
    "fpga/rtl/raster/zhao_raster_ticketq_rh.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_mul.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_v4.sv",
    "fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_mod255.sv",
    "fpga/rtl/texture/zhao_texture_aux_div6.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv",
    "fpga/rtl/texture/zhao_texture_mosaic_v2.sv",
    "fpga/rtl/texture/zhao_texture_palette_res_v2.sv",
    "fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv",
    "fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_v3bank.sv",
    "fpga/rtl/texture/zhao_texture_v3rq.sv",
    "fpga/rtl/texture/zhao_texture_v3own.sv",
    "fpga/rtl/texture/zhao_texture_metajoin_v2.sv",
    "fpga/rtl/texture/zhao_texture_uv_join_v2.sv",
    "fpga/rtl/texture/zhao_texture_early_desc_v2.sv",
    "fpga/rtl/texture/zhao_texture_frag_expand_v2.sv",
    "fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv",
    "fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv",
    "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_material_combine_v3.sv",
    "fpga/rtl/texture/zhao_texture_sheetmod.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
)
PACKET_B_PRODUCTION_FORBIDDEN_SOURCES = (
    "fpga/rtl/raster/zhao_raster_perspuv_svc.sv",
    "fpga/rtl/texture/zhao_texture_bilerp.sv",
    "fpga/rtl/texture/zhao_texture_tmu_pipe.sv",
    "fpga/rtl/texture/zhao_texture_combine.sv",
    "fpga/rtl/texture/zhao_texture_material_combine_v1.sv",
)
AUX_ASSERTION_CONTROLS = (
    ("credit_bound", 1, "CREDIT_BOUND",
     "zhao_texture_aux_pipe_v2_assertion_control_mutant",
     "zhao_texture_aux_pipe_v2_assertion_control_mutant.sv"),
    ("offer_bound", 2, "OFFER_BOUND",
     "zhao_texture_aux_pipe_v2_offer_bound_mutant",
     "zhao_texture_aux_pipe_v2_offer_bound_mutant.sv"),
    ("issued_bound", 3, "ISSUED_BOUND",
     "zhao_texture_aux_pipe_v2_issued_bound_mutant",
     "zhao_texture_aux_pipe_v2_issued_bound_mutant.sv"),
    ("return_bound", 4, "RETURN_BOUND",
     "zhao_texture_aux_pipe_v2_return_bound_mutant",
     "zhao_texture_aux_pipe_v2_return_bound_mutant.sv"),
    ("fixed_producer_room", 5, "FIXED_PRODUCER_ROOM",
     "zhao_texture_aux_pipe_v2_fixed_room_mutant",
     "zhao_texture_aux_pipe_v2_fixed_room_mutant.sv"),
    ("owed_response_room", 6, "OWED_RESPONSE_ROOM",
     "zhao_texture_aux_pipe_v2_owed_room_mutant",
     "zhao_texture_aux_pipe_v2_owed_room_mutant.sv"),
    ("issue_before_local_return", 7, "ISSUE_BEFORE_LOCAL_RETURN",
     "zhao_texture_aux_pipe_v2_issue_order_mutant",
     "zhao_texture_aux_pipe_v2_issue_order_mutant.sv"),
)
TOP_MUTANT_CONTROLS = (
    ("dispatch_index_route", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_INDEX_ROUTE_MUTANT",
     "ZHAO_PACKET_B_MUTANT_DISPATCH_INDEX_ROUTE",
     "packet-b dispatcher-index-route mutant FIRED"),
    ("aux_as_sample2", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_AUX_AS_SAMPLE2",
     "ZHAO_PACKET_B_MUTANT_AUX_AS_SAMPLE2",
     "packet-b AUX-as-sample2 mutant FIRED"),
    ("retire_context_truncation", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_RETIRE_TRUNCATION",
     "ZHAO_PACKET_B_MUTANT_RETIRE_CONTEXT_TRUNCATION",
     "packet-b retire-context-truncation mutant FIRED"),
    ("frame_clear_over_fault", "texture_island_v3_packet_b_fullctx.cpp",
     "PACKET_B_EXPECT_CLEAR_OVER_FAULT",
     "ZHAO_PACKET_B_MUTANT_FRAME_CLEAR_OVER_FAULT",
     "packet-b clear-over-fault mutant FIRED"),
    ("owner_mask_generation", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_OWNER_MASK_LIFETIME",
     "ZHAO_PACKET_B_MUTANT_OWNER_MASK_GENERATION",
     "packet-b owner-mask-generation mutant FIRED"),
    ("cache_sidx3", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_CACHE_SIDX3_LIFETIME",
     "ZHAO_PACKET_B_MUTANT_CACHE_SIDX3",
     "packet-b cache-sidx3 mutant FIRED"),
    ("bilerp_identity", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_BILERP_IDENTITY_REFUSAL",
     "ZHAO_PACKET_B_MUTANT_BILERP_IDENTITY",
     "packet-b bilerp-identity mutant FIRED"),
    ("shadow_one_sided", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_SHADOW_CORRUPTION",
     "ZHAO_PACKET_B_MUTANT_SHADOW_ONE_SIDED",
     "packet-b one-sided-shadow mutant FIRED"),
    ("rsp_drop_observation", "texture_island_v3_packet_b_directed.cpp",
     "PACKET_B_EXPECT_RSP_DROP_OBSERVATION",
     "ZHAO_PACKET_B_MUTANT_RSP_DROP_OBSERVATION",
     "packet-b response-drop detector mutant FIRED"),
)
QUIET_CONTROL_FIXTURE = (
    REPO / "tests" / "tools" / "fixtures" /
    "texture_v3_quiet_omission_mutants.json"
)


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


def active_cmake_text(text: str) -> str:
    """Mask CMake line/bracket comments while preserving line structure."""
    bracket = re.compile(r"#\[(=*)\[.*?\]\1\]", re.DOTALL)
    text = bracket.sub(
        lambda match: "".join("\n" if char == "\n" else " "
                              for char in match.group(0)),
        text,
    )
    active: list[str] = []
    for line in text.splitlines(keepends=True):
        quoted = False
        escaped = False
        comment_at = None
        for index, char in enumerate(line):
            if escaped:
                escaped = False
                continue
            if char == "\\" and quoted:
                escaped = True
                continue
            if char == '"':
                quoted = not quoted
            elif char == "#" and not quoted:
                comment_at = index
                break
        if comment_at is None:
            active.append(line)
        else:
            suffix = "\n" if line.endswith("\n") else ""
            active.append(line[:comment_at] + suffix)
    return "".join(active)


def parse_cmake_packet_b_sources(text: str) -> tuple[str, ...]:
    """Parse the one exact active Packet-B CMake list; never search comments."""
    text = active_cmake_text(text)
    lines = text.splitlines()
    headers = [
        index for index, line in enumerate(lines)
        if line == "set(ZHAO_TEXTURE_V3_PACKET_B_SOURCES"
    ]
    if len(headers) != 1:
        raise AssertionError(
            "expected exactly one exact ZHAO_TEXTURE_V3_PACKET_B_SOURCES set, "
            f"found {len(headers)}"
        )

    source_re = re.compile(
        r"^  \$\{CMAKE_SOURCE_DIR\}/(fpga/[A-Za-z0-9_./-]+\.sv)$"
    )
    result: list[str] = []
    for line in lines[headers[0] + 1:]:
        if line == ")":
            return tuple(result)
        match = source_re.fullmatch(line)
        if match is None:
            raise AssertionError(
                "non-source token inside exact Packet-B CMake list: " + repr(line)
            )
        result.append(match.group(1))
    raise AssertionError("unterminated ZHAO_TEXTURE_V3_PACKET_B_SOURCES set")


def parse_fit_target_sources(text: str, top: str) -> tuple[str, ...]:
    """Parse one exact strict-subset fit target and its flat source list."""
    lines = text.splitlines()
    header = f"  - top: {top}"
    headers = [index for index, line in enumerate(lines) if line == header]
    if len(headers) != 1:
        raise AssertionError(
            f"expected exactly one exact fit target {top!r}, found {len(headers)}"
        )

    index = headers[0] + 1
    while index < len(lines) and (not lines[index].strip() or
                                  lines[index].lstrip().startswith("#")):
        index += 1
    if index >= len(lines) or lines[index] != "    sources:":
        raise AssertionError(f"fit target {top!r} has no exact sources block")
    index += 1

    source_re = re.compile(r"^      - (fpga/[A-Za-z0-9_./-]+\.sv)$")
    result: list[str] = []
    while index < len(lines):
        line = lines[index]
        if line.startswith("  - top:") or line == "    rules:":
            break
        if not line.strip() or line.lstrip().startswith("#"):
            index += 1
            continue
        match = source_re.fullmatch(line)
        if match is None:
            raise AssertionError(
                f"non-source token inside fit target {top!r}: {line!r}"
            )
        result.append(match.group(1))
        index += 1
    return tuple(result)


def require_exact_packet_b_sources(actual: tuple[str, ...], origin: str) -> None:
    if len(actual) != len(set(actual)):
        raise AssertionError(f"{origin} Packet-B list contains a duplicate")
    if actual != PACKET_B_SOURCES:
        raise AssertionError(
            f"{origin} Packet-B list is not the exact ordered 26-entry closure"
        )


def require_production_packet_b_subsequence(actual: tuple[str, ...]) -> None:
    width = len(PACKET_B_SOURCES)
    starts = [
        index for index in range(len(actual) - width + 1)
        if actual[index:index + width] == PACKET_B_SOURCES
    ]
    if len(starts) != 1:
        raise AssertionError(
            "production does not contain exactly one contiguous Packet-B closure"
        )
    duplicates = [path for path in PACKET_B_SOURCES if actual.count(path) != 1]
    if duplicates:
        raise AssertionError(
            "production duplicates or omits selected Packet-B sources: " +
            ", ".join(duplicates)
        )
    forbidden = sorted(set(actual) & set(PACKET_B_PRODUCTION_FORBIDDEN_SOURCES))
    if forbidden:
        raise AssertionError(
            "production fit compiles superseded Packet-B oracle sources: " +
            ", ".join(forbidden)
        )


def validate_packet_b_registration_texts(cmake_text: str, fit_text: str) -> None:
    cmake_sources = parse_cmake_packet_b_sources(cmake_text)
    selected_sources = parse_fit_target_sources(
        fit_text, "zhao_texture_island_v3_top"
    )
    production_sources = parse_fit_target_sources(fit_text, "zhao_prod_top")
    require_exact_packet_b_sources(cmake_sources, "CMake")
    require_exact_packet_b_sources(selected_sources, "selected V3 fit")
    require_production_packet_b_subsequence(production_sources)


def parse_aux_assertion_controls(text: str) -> tuple[tuple[str, int, str, str, str], ...]:
    text = active_cmake_text(text)
    pattern = re.compile(
        r"(?m)^zhao_aux_v2_assertion_control\(\n"
        r"  ([a-z0-9_]+) ([1-7]) ([A-Z0-9_]+)\n"
        r"  (zhao_texture_aux_pipe_v2_[a-z0-9_]+)\n"
        r"  (zhao_texture_aux_pipe_v2_[a-z0-9_]+\.sv)\)$"
    )
    rows = tuple(
        (name, int(control), label, top, source)
        for name, control, label, top, source in pattern.findall(text)
    )
    exact_call_count = text.count("\nzhao_aux_v2_assertion_control(\n")
    if len(rows) != exact_call_count:
        raise AssertionError("an AUX assertion-control call escaped the exact parser")
    return rows


def validate_aux_assertion_control_map(text: str) -> None:
    text = active_cmake_text(text)
    rows = parse_aux_assertion_controls(text)
    if rows != AUX_ASSERTION_CONTROLS:
        raise AssertionError("AUX assertion-control mapping differs from the canonical map")
    for column, name in ((0, "name"), (1, "control"), (2, "label"),
                         (3, "top"), (4, "source")):
        values = [row[column] for row in rows]
        if len(values) != len(set(values)):
            raise AssertionError(f"AUX assertion-control {name} mapping is not unique")

    labels = tuple(row[2] for row in rows)
    label_match = re.search(
        r"set\(ZHAO_AUX_V2_ASSERT_FIRE_LABELS\n"
        r"((?:  [A-Z0-9_]+\n)+)\)",
        text,
    )
    if label_match is None:
        raise AssertionError("missing exact AUX assertion label authority")
    listed_labels = tuple(
        line.strip() for line in label_match.group(1).splitlines()
    )
    if listed_labels != labels:
        raise AssertionError("AUX assertion label authority is reordered or incomplete")

    for required in (
        'list(REMOVE_ITEM wrong_fire_labels "${FIRE_LABEL}")',
        'list(JOIN wrong_fire_labels "|" wrong_fire_label_pattern)',
        'PASS_REGULAR_EXPRESSION "ZHAO_AUX_V2_ASSERT_FIRE\\\\[${FIRE_LABEL}\\\\]"',
        'ZHAO_AUX_V2_ASSERT_FIRE\\\\[(${wrong_fire_label_pattern})\\\\]',
    ):
        if required not in text:
            raise AssertionError(
                "AUX assertion selected/wrong-label gate is missing: " + required
            )


def cmake_packet_b_fixture(
        sources: tuple[str, ...],
        variable: str = "ZHAO_TEXTURE_V3_PACKET_B_SOURCES") -> str:
    rows = "".join(f"  ${{CMAKE_SOURCE_DIR}}/{path}\n" for path in sources)
    return f"set({variable}\n{rows})\n"


def fit_packet_b_fixture(
        selected: tuple[str, ...],
        production: tuple[str, ...],
        selected_top: str = "zhao_texture_island_v3_top") -> str:
    selected_rows = "".join(f"      - {path}\n" for path in selected)
    production_rows = "".join(f"      - {path}\n" for path in production)
    return (
        "targets:\n"
        f"  - top: {selected_top}\n"
        "    sources:\n"
        f"{selected_rows}"
        "  - top: zhao_prod_top\n"
        "    sources:\n"
        f"{production_rows}"
    )


def parse_observation_successor_manifest(
        text: str, repo_root: Path | None = None) -> tuple[str, ...]:
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    body = normalized[:-1] if normalized.endswith("\n") else normalized
    if not body:
        raise AssertionError("observation successor manifest is empty")
    lines = body.split("\n")
    if any(not line or line.strip() != line for line in lines):
        raise AssertionError("observation successor manifest contains a blank record")

    path_re = re.compile(r"^(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv$")
    for path in lines:
        if "\\" in path or path.startswith("/") or re.match(r"^[A-Za-z]:", path):
            raise AssertionError("observation successor manifest path is not relative")
        if path_re.fullmatch(path) is None:
            raise AssertionError("observation successor manifest path is malformed")
        if any(part in {".", ".."} for part in path.split("/")):
            raise AssertionError("observation successor manifest path escapes its root")
    if len(lines) != len(set(lines)):
        raise AssertionError("observation successor manifest contains a duplicate")
    if repo_root is not None:
        missing = [path for path in lines if not (repo_root / path).is_file()]
        if missing:
            raise AssertionError(
                "observation successor manifest path is missing: " + missing[0]
            )
    return tuple(lines)


def validate_observation_successor_cmake(
        cmake_text: str, manifest_text: str,
        repo_root: Path | None = None) -> tuple[str, ...]:
    cmake_text = active_cmake_text(cmake_text)
    sources = parse_observation_successor_manifest(manifest_text, repo_root)
    if len(sources) != 9:
        raise AssertionError("observation successor manifest must have nine SV paths")
    if sources[-1] != "tests/raster/tb_packetb_observation_successors.sv":
        raise AssertionError("observation successor manifest test top is not last")

    manifest_binding = (
        "set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST\n"
        "  ${CMAKE_CURRENT_SOURCE_DIR}/raster/"
        "packetb_observation_successors.sources.txt)"
    )
    required = (
        manifest_binding,
        'file(READ "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}"',
        "Packet-B observation source manifest contains a blank record",
        'file(STRINGS "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}"\n'
        "  ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES)",
        "set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES)",
        "foreach(relative_source IN LISTS "
        "ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES)",
        'IS_ABSOLUTE "${relative_source}"',
        "relative_source MATCHES",
        "list(FIND ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SEEN",
        'if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${relative_source}")',
        'list(APPEND ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES\n'
        '    "${CMAKE_SOURCE_DIR}/${relative_source}")',
        "SOURCES ${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES}",
        "TOP_MODULE tb_packetb_observation_successors",
        "add_test(NAME packetb_observation_successors",
    )
    for statement in required:
        if statement not in cmake_text:
            raise AssertionError(
                "observation successor CMake/manifest binding differs: " + statement
            )
    for unique_statement in (
        manifest_binding,
        'file(STRINGS "${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_MANIFEST}"\n'
        "  ZHAO_PACKETB_OBSERVATION_SUCCESSOR_RELATIVE_SOURCES)",
        "SOURCES ${ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES}",
        "TOP_MODULE tb_packetb_observation_successors",
        "add_test(NAME packetb_observation_successors",
    ):
        if cmake_text.count(unique_statement) != 1:
            raise AssertionError(
                "observation successor CMake binding is not unique: " +
                unique_statement
            )
    section_start = cmake_text.index(manifest_binding)
    section_end = cmake_text.index(
        "add_executable(pb_desc", section_start
    )
    standalone = cmake_text[section_start:section_end]
    if "${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv" in standalone:
        raise AssertionError(
            "standalone observation target duplicates a manifest source"
        )
    return sources


def parse_top_mutant_controls(
        text: str) -> tuple[tuple[str, str, str, str, str], ...]:
    text = active_cmake_text(text)
    pattern = re.compile(
        r'(?m)^zhao_packet_b_top_mutant_control\(\n'
        r'  ([a-z0-9_]+) ([a-z0-9_]+\.cpp)\n'
        r'  (PACKET_B_EXPECT_[A-Z0-9_]+)\n'
        r'  (ZHAO_PACKET_B_MUTANT_[A-Z0-9_]+)\n'
        r'  "([^"]+)"\)$'
    )
    rows = tuple(pattern.findall(text))
    if len(rows) != text.count("\nzhao_packet_b_top_mutant_control(\n"):
        raise AssertionError("a Packet-B top-mutant call escaped the exact parser")
    return rows


def validate_top_profiles_mutants_and_quiet_accounting(cmake_text: str) -> None:
    cmake_text = active_cmake_text(cmake_text)
    rows = parse_top_mutant_controls(cmake_text)
    if rows != TOP_MUTANT_CONTROLS:
        raise AssertionError("Packet-B top-mutant mapping differs from frozen drivers")
    for column, name in ((0, "name"), (2, "expect macro"),
                         (3, "selector"), (4, "diagnostic")):
        values = [row[column] for row in rows]
        if len(values) != len(set(values)):
            raise AssertionError(f"Packet-B top-mutant {name} is not unique")

    timing_methods = (
        "test_aux_registered_clamp_transport_reaches_all_committed_mutants",
        "test_v3_owner_combine_feedback_cut_keeps_generation_witness",
        "test_v3_owner_retirement_head_is_bounded_and_controlled",
        "test_v3_owner_timing3_notifications_preserve_event_moments",
        "test_v3_owner_timing4_event_boundaries_are_registered",
        "test_material_writeback_cut_reaches_all_committed_mutants",
        "test_rcp_v4_balanced_lzc_direct_oracle_and_mutant_are_exact",
    )
    for method in timing_methods:
        marker = f"PacketAOwnershipAndClosureTests.{method}"
        if cmake_text.count(marker) != 1:
            raise AssertionError("G8A timing source control is not registered exactly: " + method)

    mutant_source = (
        REPO / "tests" / "mutants" /
        "zhao_texture_island_v3_packet_b_mutants.sv"
    ).read_text(encoding="utf-8")
    for _name, driver_name, expect_macro, selector, diagnostic in rows:
        driver = (REPO / "tests" / "texture" / driver_name).read_text(
            encoding="utf-8"
        )
        if f"`ifdef {selector}" not in mutant_source:
            raise AssertionError(f"missing Packet-B top selector {selector}")
        if expect_macro not in driver or diagnostic not in driver:
            raise AssertionError(
                f"Packet-B top driver branch differs for {expect_macro}"
            )

    required_cmake = (
        "PACKET_B_EXPECT_NO_SHADOWS=1",
        "VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=0)",
        "VERILATOR_ARGS --assert -GMIGRATION_SHADOWS=1)",
        "ZHAO_TEXTURE_V3_PACKET_B_MUTANT_SOURCES",
        "zhao_texture_island_v3_packet_b_mutants.sv",
        "PASS_REGULAR_EXPRESSION \"${DIAGNOSTIC}\"",
        "FAIL_REGULAR_EXPRESSION \"FAIL\"",
    )
    for required in required_cmake:
        if required not in cmake_text:
            raise AssertionError("Packet-B top registration is missing: " + required)

    directed = (
        REPO / "tests" / "texture" /
        "texture_island_v3_packet_b_directed.cpp"
    ).read_text(encoding="utf-8")
    for required in (
        "#ifdef PACKET_B_EXPECT_NO_SHADOWS",
        "!h.dut.shadow_present_o",
        "h.dut.shadow_present_o && h.dut.meta_shadow_reads_o != 0",
        "h.dut.meta_shadow_mismatch_o == 0",
    ):
        if required not in directed:
            raise AssertionError("Packet-B shadow profile assertion is missing")

    fullctx_start = cmake_text.index(
        "add_executable(tpb_ctx"
    )
    fullctx_end = cmake_text.index(
        "set(ZHAO_TEXTURE_DESC_EXPAND_BIND_V2_SOURCES", fullctx_start
    )
    fullctx_cmake = cmake_text[fullctx_start:fullctx_end]
    if "--public-flat-rw" in fullctx_cmake or "--vpi" in fullctx_cmake:
        raise AssertionError("full-context target still depends on unstable internals")
    fullctx = (
        REPO / "tests" / "texture" /
        "texture_island_v3_packet_b_fullctx.cpp"
    ).read_text(encoding="utf-8")
    if "___024root" in fullctx or "__pi" in fullctx:
        raise AssertionError("full-context driver names a generated class suffix")
    for required in (
        "Vzhao_texture_island_v3_top__Dpi.h",
        "zhao_texture_packet_b_get_owner_context",
        "zhao_texture_packet_b_set_frame_fault_inject",
    ):
        if required not in fullctx:
            raise AssertionError("full-context stable DPI seam is incomplete")

    payload = json.loads(QUIET_CONTROL_FIXTURE.read_text(encoding="utf-8"))
    controls = payload["controls"]
    identities = [(row["equation"], row["operand"]) for row in controls]
    if len(controls) != 72 or len(identities) != len(set(identities)):
        raise AssertionError("quiet mutation accounting is not 72 unique terms")
    bridges = [row for row in controls if row["operand"] == "data_quiet"]
    named = [row for row in controls if row["operand"] != "data_quiet"]
    if len(bridges) != 1 or len(named) != 71:
        raise AssertionError("quiet accounting is not 71 named plus one bridge")


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
                top, [PACKAGE, FRAG_PACKAGE, FIXTURE], mdir, "--cc",
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
                for control in range(1, 8)
            ]
            # The fourteen fragment-state field-name probes live in the FIXTURE
            # rather than the package, because a packed struct in the package
            # emits one duplicate-name MEMBERDTYPE per member and would make the
            # island's pinned schema fingerprint permanently noisier -- see
            # `tools/rtl/texture_v3_interface_parser.py`'s 2026-09-18 note, which
            # refused the same trade at two markers. They get their own control
            # so each one is SEEN TO FIRE rather than merely present.
            frag_span_runs = [
                run_generated_model(executable, "+FRAG_SPAN_CONTROL=%d" % control)
                for control in range(1, 15)
            ]

        diagnostic = normal.stdout + normal.stderr
        self.assertEqual(normal.returncode, 0, diagnostic)
        self.assertIn("ZHAO_RENDER_TEXTURE_LAYOUT_GUARD_OK field_spans=45", diagnostic)
        self.assertIn("ZHAO_RENDER_TEXTURE_LAYOUT_ROUNDTRIP_OK controls=7 frag_spans=14", diagnostic)

        for control, ran in enumerate(field_span_runs, 1):
            with self.subTest(field_span_control=control):
                diagnostic = ran.stdout + ran.stderr
                self.assertNotEqual(ran.returncode, 0, diagnostic)
                self.assertIn(
                    "ZHAO_RENDER_TEXTURE_FIELD_SPAN_FIRE[%d]" % control,
                    diagnostic,
                )
                self.assertNotIn("FIELD_SPAN_ESCAPED", diagnostic)

        for control, ran in enumerate(frag_span_runs, 1):
            with self.subTest(frag_span_control=control):
                diagnostic = ran.stdout + ran.stderr
                self.assertNotEqual(ran.returncode, 0, diagnostic)
                self.assertIn(
                    "ZHAO_FRAG_STATE_SPAN_FIRE[%d]" % control, diagnostic)
                self.assertNotIn("FRAG_STATE_SPAN_ESCAPED", diagnostic)

        roundtrip_names = (
            "continuation", "aux", "earlyz_payload",
            "pretexture", "retirement", "result", "frag_state",
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
            # The fragment state word, added 2026-09-25 (FRAGSTATE). 16 is the
            # AUX fingerprint, whose control is the committed reversed-wx/wz
            # mutant rather than a parameter, so these take 17 and 18.
            # The fragment state word lives in its OWN package and its own
            # guard, because `zhao_render_texture_pkg` is inside the texture
            # island's frozen interface manifest and had to stay byte-identical.
            # Its fatal therefore carries its own prefix.
            17: "ZHAO_FRAG_STATE_CONTRACT_FIRE[1]: FRAG_STATE_OFFSET_CONTRACT",
        }
        top = "zhao_render_texture_elab_control_top"

        # A failing process is evidence only after the same generated root has
        # run successfully with its real/default contract.
        with tempfile.TemporaryDirectory(
                prefix="zhao-packet-a-contract-baseline-") as temporary:
            mdir = Path(temporary)
            generated = run_verilator(top, [PACKAGE, FRAG_PACKAGE, FIXTURE], mdir, "--cc")
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
                    top, [PACKAGE, FRAG_PACKAGE, FIXTURE], mdir,
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
        # THE SIBLING SHELL NOW EXISTS, and this assertion used to say it did
        # not. That was never really a claim about a file -- it was standing in
        # for "there is no semantic connected-shell root to register in Packet
        # A", which was true only while nothing composed the V3 island into a
        # shell. `zhao_shell_top_v2` does, so the question the line was holding
        # open has an answer and the answer is asserted instead.
        #
        # Rooted at the sibling the census finds EXACTLY ONE elaborated
        # lifecycle owner and no errors, where the historical shell finds none
        # -- which is the whole point of the swap and is checked above.
        declarations, _edges, _extra = ownership.module_edges(REPO / "fpga" / "rtl")
        self.assertIn("zhao_shell_top_v2", declarations)

        sibling_errors, sibling_obs = ownership.run_check(
            REPO / "design" / "prod_manifest.yml",
            REPO / "fpga" / "rtl",
            root_overrides={ROLE: "zhao_shell_top_v2"},
        )
        sibling = {
            role: reachable
            for role, _scope, _root, reachable in sibling_obs
        }
        self.assertEqual(sibling.get(ROLE), ["zhao_texture_v3own"], sibling_errors)
        self.assertEqual(sibling_errors, [])

        # AND BEING A CONNECTED OWNER MUST NOT MAKE IT THE SELECTED ONE. That is
        # the half of this test's name that still matters: fail-closed, NOT a
        # connected-owner claim. The sibling is reachable-as-an-owner and
        # registered `not-yet-adopted`, and those two facts have to be carried
        # together -- reachability is not selection, and a shell that became
        # production by being composed would be exactly the silent promotion
        # this file exists to prevent.
        _tops, excluded_now = prod_manifest.read_manifest(
            REPO / "design" / "prod_manifest.yml"
        )
        self.assertNotIn("zhao_shell_top_v2", _tops)
        self.assertEqual(excluded_now["zhao_shell_top_v2"][0], "not-yet-adopted")

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

        cmake_text = (REPO / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        fit_text = (REPO / "design" / "fit_targets.yml").read_text(
            encoding="utf-8"
        )
        validate_packet_b_registration_texts(cmake_text, fit_text)
        self.assertEqual(
            parse_cmake_packet_b_sources(cmake_text), PACKET_B_SOURCES
        )
        self.assertEqual(
            parse_fit_target_sources(fit_text, "zhao_texture_island_v3_top"),
            PACKET_B_SOURCES,
        )
        require_production_packet_b_subsequence(
            parse_fit_target_sources(fit_text, "zhao_prod_top")
        )

        previous_cwd = Path.cwd()
        try:
            os.chdir(REPO)
            declarations, edges = prod_manifest.module_edges()
            overrides = prod_manifest.read_parameter_overrides()
            prod_manifest.validate_parameter_overrides(overrides, tops, declarations)
            edges, _observations = prod_manifest.apply_parameterized_elaboration(
                declarations, edges, tops, overrides)
            closure_errors = prod_manifest.check_fit_sources(declarations, edges)
        finally:
            os.chdir(previous_cwd)
        self.assertEqual(closure_errors, [])

    def test_packet_b_source_registrations_are_exact_ordered_and_unique(self) -> None:
        cmake_text = (REPO / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        fit_text = (REPO / "design" / "fit_targets.yml").read_text(
            encoding="utf-8"
        )
        validate_packet_b_registration_texts(cmake_text, fit_text)
        self.assertEqual(len(PACKET_B_SOURCES), 27)
        self.assertEqual(len(PACKET_B_SOURCES), len(set(PACKET_B_SOURCES)))

    def test_packet_b_source_list_parsers_fire_on_every_structural_mutation(self) -> None:
        canonical_cmake = cmake_packet_b_fixture(PACKET_B_SOURCES)
        canonical_fit = fit_packet_b_fixture(PACKET_B_SOURCES, PACKET_B_SOURCES)
        validate_packet_b_registration_texts(canonical_cmake, canonical_fit)

        reordered = list(PACKET_B_SOURCES)
        reordered[7], reordered[8] = reordered[8], reordered[7]
        extra = "fpga/rtl/texture/zhao_packet_b_extra_control.sv"
        split = len(PACKET_B_SOURCES) // 2
        controls = (
            (
                "extra",
                cmake_packet_b_fixture(PACKET_B_SOURCES + (extra,)),
                canonical_fit,
            ),
            (
                "missing",
                canonical_cmake,
                fit_packet_b_fixture(PACKET_B_SOURCES[:-1], PACKET_B_SOURCES),
            ),
            (
                "reordered",
                cmake_packet_b_fixture(tuple(reordered)),
                canonical_fit,
            ),
            (
                "duplicate",
                canonical_cmake,
                fit_packet_b_fixture(
                    PACKET_B_SOURCES + (PACKET_B_SOURCES[3],),
                    PACKET_B_SOURCES,
                ),
            ),
            (
                "production-noncontiguous-extra",
                canonical_cmake,
                fit_packet_b_fixture(
                    PACKET_B_SOURCES,
                    PACKET_B_SOURCES[:split] + (extra,) + PACKET_B_SOURCES[split:],
                ),
            ),
            (
                "production-superseded-oracle",
                canonical_cmake,
                fit_packet_b_fixture(
                    PACKET_B_SOURCES,
                    PACKET_B_SOURCES + (PACKET_B_PRODUCTION_FORBIDDEN_SOURCES[0],),
                ),
            ),
            (
                "lookalike-cmake-segment",
                cmake_packet_b_fixture(
                    PACKET_B_SOURCES,
                    "ZHAO_TEXTURE_V3_PACKET_B_SOURCES_LOOKALIKE",
                ),
                canonical_fit,
            ),
            (
                "lookalike-fit-segment",
                canonical_cmake,
                fit_packet_b_fixture(
                    PACKET_B_SOURCES,
                    PACKET_B_SOURCES,
                    "zhao_texture_island_v3_top_lookalike",
                ),
            ),
        )
        for name, cmake_text, fit_text in controls:
            with self.subTest(control=name), self.assertRaises(AssertionError):
                validate_packet_b_registration_texts(cmake_text, fit_text)

    def test_aux_assertion_control_mapping_is_exact_unique_and_exclusive(self) -> None:
        cmake_text = (REPO / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        validate_aux_assertion_control_map(cmake_text)

        labels = tuple(row[2] for row in AUX_ASSERTION_CONTROLS)
        for row in AUX_ASSERTION_CONTROLS:
            selected = row[2]
            wrong = tuple(label for label in labels if label != selected)
            self.assertEqual(len(wrong), 6)
            selected_diagnostic = f"ZHAO_AUX_V2_ASSERT_FIRE[{selected}]"
            wrong_pattern = re.compile(
                r"ZHAO_AUX_V2_ASSERT_FIRE\[(?:" + "|".join(wrong) + r")\]"
            )
            self.assertIsNone(wrong_pattern.search(selected_diagnostic))
            for wrong_label in wrong:
                with self.subTest(selected=selected, wrong=wrong_label):
                    self.assertIsNotNone(wrong_pattern.search(
                        f"ZHAO_AUX_V2_ASSERT_FIRE[{wrong_label}]"
                    ))

        duplicate_label = cmake_text.replace(
            "  offer_bound 2 OFFER_BOUND\n",
            "  offer_bound 2 CREDIT_BOUND\n",
            1,
        )
        with self.assertRaisesRegex(AssertionError, "canonical map|unique"):
            validate_aux_assertion_control_map(duplicate_label)

        missing_wrong_label_gate = cmake_text.replace(
            '  list(REMOVE_ITEM wrong_fire_labels "${FIRE_LABEL}")\n',
            "",
            1,
        )
        with self.assertRaisesRegex(AssertionError, "wrong-label gate"):
            validate_aux_assertion_control_map(missing_wrong_label_gate)

    def test_aux_registered_clamp_transport_reaches_all_committed_mutants(self) -> None:
        paths = (
            "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_credit_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_assertion_control_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_offer_bound_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_issued_bound_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_return_bound_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_fixed_room_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_owed_room_mutant.sv",
            "tests/mutants/zhao_texture_aux_pipe_v2_issue_order_mutant.sv",
        )
        markers = (
            "logic                    div_in_valid_q;",
            ".in_valid_i(div_in_valid_q)",
            "&& !div_in_valid_q",
            "div_in_valid_q              <= 1'b0;",
            "div_in_valid_q <= a0_valid_q;",
            "div_in_tag_q <= side_write_q;",
        )

        def validate(source: str) -> None:
            if ".in_valid_i(a0_valid_q)" in source:
                raise AssertionError("AUX mutant retained pre-cut divider input")
            if not re.search(
                r"job_ready_o\s*=\s*rst_n\s*&&\s*\(credit_q\s*!=\s*CREDIT_W'\(CREDIT\)\)",
                source,
            ):
                raise AssertionError("AUX admission is not reset-gated")
            for marker in markers:
                if source.count(marker) != 1:
                    raise AssertionError("AUX registered-clamp marker differs: " + marker)

        for relative in paths:
            with self.subTest(path=relative):
                validate((REPO / relative).read_text(encoding="utf-8"))
        source = (REPO / paths[0]).read_text(encoding="utf-8")
        with self.assertRaises(AssertionError):
            validate(source.replace("&& !div_in_valid_q", "", 1))
        with self.assertRaisesRegex(AssertionError, "reset-gated"):
            validate(source.replace("rst_n && ", "", 1))

        divider = (
            REPO / "fpga/rtl/texture/zhao_texture_aux_div6.sv"
        ).read_text(encoding="utf-8")
        timing4_mutants = (
            REPO / "tests/mutants/zhao_texture_aux_timing4_mutants.sv"
        ).read_text(encoding="utf-8")
        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        for marker in (
            "a0_degenerate_q <= `ZHAO_AUX_T4_DEGENERATE_CAPTURE(",
            "a0_input_fault_q <= `ZHAO_AUX_T4_INPUT_FAULT_CAPTURE(",
            "if (a0_degenerate_q)",
            "if (a0_input_fault_q)",
        ):
            self.assertEqual(source.count(marker), 1, marker)
        for marker in (
            "difference = {1'b0, r} - {1'b0, shifted};",
            "hit        = `ZHAO_AUX_T4_DIV_HIT(difference);",
        ):
            self.assertEqual(divider.count(marker), 1, marker)
        for marker in (
            "ZHAO_AUX_T4_MUTANT_DEGENERATE_DROP",
            "ZHAO_AUX_T4_MUTANT_INPUT_FAULT_DROP",
            "ZHAO_AUX_T4_MUTANT_BORROW_REVERSE",
            "ZHAO_AUX_TIMING4_MUTANT_SELECTOR_COLLISION",
        ):
            self.assertGreaterEqual(timing4_mutants.count(marker), 1, marker)
        for marker in (
            "pb_aux_t4d degenerate ZHAO_AUX_T4_MUTANT_DEGENERATE_DROP",
            "pb_aux_t4f input_fault ZHAO_AUX_T4_MUTANT_INPUT_FAULT_DROP",
            "add_test(NAME texture_aux_div6_timing4_borrow_control",
            "add_test(NAME texture_aux_timing4_registration_static",
        ):
            self.assertEqual(cmake.count(marker), 1, marker)

    def test_v3_owner_combine_feedback_cut_keeps_generation_witness(self) -> None:
        top = (REPO / "fpga/rtl/texture/zhao_texture_island_v3_top.sv").read_text(
            encoding="utf-8"
        )
        owner = (REPO / "fpga/rtl/texture/zhao_texture_v3own.sv").read_text(
            encoding="utf-8"
        )

        def validate_top(source: str) -> None:
            required = (
                "joined_owner_mask_valid_c && expand_frag_ready_w;",
                "wire owner_combine_validation_ready_c =",
                # 2026-09-18: the fence moved from "index with the selected
                # owner" to "evaluate for every queue entry, then select".
                # Same value; see the comment at the fence. What this test
                # protects is unchanged and is checked below, not here --
                # the WIDE generation table must stay out of the COMBINE ready
                # feedback path. These two markers only pin the shape.
                "owner_combine_fence_ok_c[e] =",
                "!join_validation_pending_q[owner_combine_owner_all_w[e*OWNERW + 8 +: 6]];",
                "(material_read_join_generation_q == material_read_owner_q[7:0])",
                "join_validation_generation_m[owner_combine_owner_w[13:8]]",
                "a_combine_after_join_validation",
            )
            ready = re.search(
                r"wire owner_combine_validation_ready_c\s*=.*?;",
                source,
                re.DOTALL,
            )
            if ready is None or "join_validation_generation_m" in ready.group(0):
                raise AssertionError("generation lookup remains in COMBINE ready feedback")
            # THE FENCE ITSELF IS NOW A SEPARATE BLOCK, so checking only the
            # `wire ... =` above would leave the thing this test exists to
            # forbid free to appear one line higher. Widened 2026-09-18 with
            # the change that moved it -- a gate whose subject moves and whose
            # check does not is the shape this repository keeps writing down.
            # Anchored on the ASSIGNMENT, not on the `for` header. The first
            # attempt matched `always_comb ... for (int unsigned e = 0;` and
            # stopped at the semicolon INSIDE the loop header, so it never
            # covered the fence body and could not have fired -- caught only
            # because the control below was written and watched to fail.
            fence = re.search(
                r"owner_combine_fence_ok_c\[e\]\s*=.*?;",
                source,
                re.DOTALL,
            )
            if fence is None:
                raise AssertionError("per-entry COMBINE fence block not found")
            if "join_validation_generation_m" in fence.group(0):
                raise AssertionError(
                    "generation lookup moved into the per-entry COMBINE fence")
            for marker in required:
                if marker not in source:
                    raise AssertionError("owner generation witness missing: " + marker)

        def validate_owner(source: str) -> None:
            if "cmb_res_q - CNTW'(cmb_fire_c)" in source:
                raise AssertionError("reservation carry chain remains")
            markers = (
                "logic cmb_room_after_fire_c;",
                "((cmb_res_q < CNTW'(CMBQD)) &&",
                "(!cmb_fire_c || (cmb_res_q != CNTW'(0)))) ||",
                "(cmb_fire_c && (cmb_res_q == CNTW'(CMBQD)));",
                "assign cmb_pop_c = sel_v_c && cmb_room_after_fire_c;",
            )
            for marker in markers:
                if marker not in source:
                    raise AssertionError("reservation Boolean cut missing: " + marker)

        validate_top(top)
        validate_owner(owner)
        for reservation in range(128):
            for fire in (0, 1):
                old_room = ((reservation - fire) & 0x7F) < 4
                new_room = (
                    (reservation < 4 and (not fire or reservation != 0))
                    or (bool(fire) and reservation == 4)
                )
                self.assertEqual(
                    old_room,
                    new_room,
                    f"reservation rewrite differs at res={reservation} fire={fire}",
                )
        with self.assertRaisesRegex(AssertionError, "generation witness missing"):
            validate_top(
                top.replace(
                    "(material_read_join_generation_q == material_read_owner_q[7:0])",
                    "1'b1",
                    1,
                )
            )
        # THE FIRE CASE FOR THE READY WIRE. Retargeted 2026-09-18 onto the
        # restructured fence: the control used to reinsert the generation
        # lookup into a line that no longer exists, so it would have stopped
        # firing silently -- a control that cannot reach its own fault is the
        # exact thing this file spends its length warning about.
        with self.assertRaisesRegex(AssertionError, "ready feedback"):
            validate_top(
                top.replace(
                    "owner_combine_fence_ok_c[owner_combine_rp_w];",
                    "owner_combine_fence_ok_c[owner_combine_rp_w] &&\n"
                    "      (join_validation_generation_m[owner_combine_owner_w[13:8]] == "
                    "owner_combine_owner_w[7:0]);",
                    1,
                )
            )

        # AND THE FIRE CASE FOR THE PER-ENTRY BLOCK, which is new and would
        # otherwise be a check nobody has watched fail. The generation lookup
        # is forbidden wherever the fence lives; one control per location,
        # because a single control proves only one of them.
        with self.assertRaisesRegex(AssertionError, "per-entry COMBINE fence"):
            validate_top(
                top.replace(
                    "!join_validation_pending_q[owner_combine_owner_all_w[e*OWNERW + 8 +: 6]];",
                    "!join_validation_pending_q[owner_combine_owner_all_w[e*OWNERW + 8 +: 6]] &&\n"
                    "          (join_validation_generation_m[owner_combine_owner_w[13:8]] == "
                    "owner_combine_owner_w[7:0]);",
                    1,
                )
            )

        with self.assertRaisesRegex(AssertionError, "Boolean cut missing"):
            validate_owner(
                owner.replace(
                    "cmb_res_q == CNTW'(CMBQD)",
                    "cmb_res_q != CNTW'(CMBQD)",
                    1,
                )
            )

    def test_v3_owner_retirement_head_is_bounded_and_controlled(self) -> None:
        production = (
            REPO / "fpga/rtl/texture/zhao_texture_v3own.sv"
        ).read_text(encoding="utf-8")

        def validate(source: str) -> None:
            forbidden = (
                "g2_v_q", "g2_owner_q", "fres_cap_q", "ctx_cap_q",
                "assign out_ctx_o    = oq_ctx_q",
                "if (out_fire_c) oq_rp_q",
            )
            for marker in forbidden:
                if marker in source:
                    raise AssertionError("retirement pre-head structure remains: " + marker)
            required = (
                "assign out_valid_o      = oq_head_v_q;",
                "assign oq_head_room_c   = !oq_head_v_q || out_fire_c;",
                "assign oq_body_load_c   = (oq_occ_c != '0) && oq_head_room_c;",
                "assign oq_bypass_load_c = (oq_occ_c == '0) && g1_v_q && oq_head_room_c;",
                "if (g1_v_q)         oq_wp_q <= oq_wp_q + (OQPW+1)'(1);",
                "if (oq_head_load_c) oq_rp_q <= oq_rp_q + (OQPW+1)'(1);",
                "oq_ctx_q[oq_wp_q[OQPW-1:0]] <= ctx_rd_c;",
                "oq_body_head_ctx_q <= oq_ctx_q[oq_rp_q[OQPW-1:0]];",
                "oq_bypass_head_ctx_q <= ctx_rd_c;",
                "a_out_structure    : assert (out_res_q ==",
                "CNTW'(g0_v_q) + CNTW'(g1_v_q) + CNTW'(oq_occ_c)",
                "&& !oq_head_v_q && (oq_occ_c == '0)",
            )
            for marker in required:
                if source.count(marker) != 1:
                    raise AssertionError("retirement head marker differs: " + marker)

        validate(production)

        def production_body(mutant: str) -> str:
            marker = "// zhao_texture_v3own.sv -- the V3 owner"
            start = mutant.find(marker)
            if start < 0:
                raise AssertionError("retirement mutant lost its production-body boundary")
            return mutant[start:]

        reload_mutant = (
            REPO / "tests/mutants/zhao_texture_v3own_no_same_edge_reload_mutant.sv"
        ).read_text(encoding="utf-8")
        reload_body = production_body(reload_mutant)
        reload_broken = "  assign oq_head_room_c   = !oq_head_v_q;"
        self.assertEqual(reload_body.count(reload_broken), 1)
        reload_restored = reload_body.replace(
            "module zhao_texture_v3own_no_same_edge_reload_mutant #(",
            "module zhao_texture_v3own #(", 1,
        ).replace(
            reload_broken,
            "  assign oq_head_room_c   = !oq_head_v_q || out_fire_c;", 1,
        )
        self.assertEqual(reload_restored, production)

        pointer_mutant = (
            REPO / "tests/mutants/zhao_texture_v3own_bypass_pointer_mutant.sv"
        ).read_text(encoding="utf-8")
        pointer_body = production_body(pointer_mutant)
        pointer_broken = (
            "      if (oq_body_load_c) oq_rp_q <= oq_rp_q + (OQPW+1)'(1);"
        )
        self.assertEqual(pointer_body.count(pointer_broken), 1)
        pointer_restored = pointer_body.replace(
            "module zhao_texture_v3own_bypass_pointer_mutant #(",
            "module zhao_texture_v3own #(", 1,
        ).replace(
            pointer_broken,
            "      if (oq_head_load_c) oq_rp_q <= oq_rp_q + (OQPW+1)'(1);", 1,
        )
        self.assertEqual(pointer_restored, production)

        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        driver = (
            REPO / "tests/texture/texture_v3own_bubble_control.cpp"
        ).read_text(encoding="utf-8")
        assertion_control = (
            REPO / "tests/tools/test_v3own_assertion_control.py"
        ).read_text(encoding="utf-8")
        attributes = (REPO / ".gitattributes").read_text(encoding="utf-8")
        for marker in (
            "add_executable(test_texture_v3own_bypass_pointer_assertion_control",
            "ZHAO_BYPASS_POINTER_ASSERT_CONTROL=1",
            "TOP_MODULE zhao_texture_v3own_bypass_pointer_mutant",
            "zhao_texture_v3own_bypass_pointer_mutant.sv",
            "test_v3own_assertion_control.py",
            "--exe $<TARGET_FILE:test_texture_v3own_bypass_pointer_assertion_control>",
        ):
            self.assertIn(marker, cmake)
        for marker in (
            "#ifdef ZHAO_BYPASS_POINTER_ASSERT_CONTROL",
            "context.fatalOnError(false);",
            "V3OWN_BYPASS_POINTER_ASSERT_CONTROL",
            "fired ? 0 : 2",
        ):
            self.assertIn(marker, driver)
        for marker in (
            'EXPECTED_LABEL = "a_out_structure"',
            'labels != [EXPECTED_LABEL]',
            '"fired=1 emitted=0"',
            '"FAIL:" in diagnostic',
        ):
            self.assertIn(marker, assertion_control)
        self.assertIn("tests/mutants/zhao_texture_v3own*.sv text eol=lf", attributes)
        self.assertIn("tests/tools/test_v3own_assertion_control.py text eol=lf", attributes)
        with self.assertRaisesRegex(AssertionError, "head marker differs"):
            validate(production.replace(
                "if (oq_head_load_c) oq_rp_q", "if (oq_body_load_c) oq_rp_q", 1
            ))

    def test_v3_owner_timing3_notifications_preserve_event_moments(self) -> None:
        source = (
            REPO / "fpga/rtl/texture/zhao_texture_v3own.sv"
        ).read_text(encoding="utf-8")

        def validate(text: str) -> None:
            required = (
                "assign iss_t_capture_ok_c = iss_tmu_valid_i && iss_t_in_rng_c",
                "!iss_t_pending_hit_c &&",
                "assign iss_a_capture_ok_c = iss_aux_valid_i && !iss_a_pending_hit_c &&",
                "iss0t_v_q   <= iss_tmu_valid_i;",
                "iss0a_v_q   <= iss_aux_valid_i;",
                "(iss0t_handle_q == {c0t_slot_q, c0t_sidx_q, c0t_gen_q})",
                "(iss0a_owner_q == {c0a_slot_q, c0a_gen_q})",
                "if (iss0t_v_q && !iss0t_acc_q) d_issue_c",
                "if (iss0a_v_q && !iss0a_acc_q) d_issue_c",
                "cmb_accept_v_q <= cmb_fire_c;",
                "cmb_accept_gen_ok_q <= cmb_gen_ok_c;",
                "if (cmb_accept_v_q && (cmb_accept_slot_q == SLOTW'(i))",
                "assign src_cbi_published_c = cbi_q[src_rd_slot_i] ||",
                "(cmb_accept_gen_q == win_gen_of_slot(src_rd_slot_i))",
                "(cmb_accept_gen_q == c0f_gen_q));",
                "&& !ctxw_v_q && !iss0t_v_q && !iss0a_v_q",
                "&& !kpipe_busy_c && !cmb_accept_v_q",
            )
            for marker in required:
                if text.count(marker) != 1:
                    raise AssertionError("timing3 notification marker differs: " + marker)
            if "if (cmb_fire_c && (cmb_owner_o" in text:
                raise AssertionError("COMBINE head still drives the owner table directly")
            if "iss_tmu_valid_i && !iss_t_ok_c" in text:
                raise AssertionError("raw ISSUE input still drives the error accumulator")

        validate(source)
        mutations = (
            source.replace("!iss_t_pending_hit_c &&", "1'b1 &&", 1),
            source.replace(
                "(iss0t_handle_q == {c0t_slot_q, c0t_sidx_q, c0t_gen_q})",
                "1'b0", 1,
            ),
            source.replace("cmb_accept_v_q <= cmb_fire_c;", "cmb_accept_v_q <= 1'b0;", 1),
            source.replace(
                "(cmb_accept_gen_q == win_gen_of_slot(src_rd_slot_i))",
                "1'b1", 1,
            ),
            source.replace("&& !kpipe_busy_c && !cmb_accept_v_q",
                           "&& !kpipe_busy_c", 1),
        )
        for index, mutation in enumerate(mutations):
            self.assertNotEqual(
                mutation, source,
                f"timing notification mutation {index} matched nothing")
            with self.assertRaises(AssertionError):
                validate(mutation)

    def test_v3_owner_timing4_event_boundaries_are_registered(self) -> None:
        owner = (
            REPO / "fpga/rtl/texture/zhao_texture_v3own.sv"
        ).read_text(encoding="utf-8")
        island = (
            REPO / "fpga/rtl/texture/zhao_texture_island_v3_top.sv"
        ).read_text(encoding="utf-8")
        mutant = (
            REPO / "tests/mutants/zhao_texture_v3own_timing4_event_mutants.sv"
        ).read_text(encoding="utf-8")
        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        control = (
            REPO / "tests/tools/test_v3own_timing4_event_control.py"
        ).read_text(encoding="utf-8")

        owner_markers = (
            "`define ZHAO_V3OWN_ADMISSION_EVENT_OWNER(owner) owner",
            "`define ZHAO_V3OWN_RESERVATION_EVENT_OWNER(owner) owner",
            "ctxw_v_q <= adm_fire_c;",
            "ctxw_owner_q <= `ZHAO_V3OWN_ADMISSION_EVENT_OWNER(",
            "if (ctxw_v_q &&",
            "assign iss_t_admission_hit_c = ctxw_v_q &&",
            "assign iss_a_admission_hit_c = ctxw_v_q &&",
            "if (k0_v_q && k0_gen_ok_q &&",
            "k0_owner_q <= `ZHAO_V3OWN_RESERVATION_EVENT_OWNER(sel_data_c);",
            "a_admission_event_identity : assert",
            "a_reservation_event_identity : assert",
        )
        for marker in owner_markers:
            self.assertEqual(owner.count(marker), 1, marker)
        self.assertNotIn("if (adm_fire_c &&\n          (adm_owner_o", owner)

        island_markers = (
            "assign frag_ready_o = binding_admission_enable_w && !lifetime_admission_block_w",
            "owner_admission_event_valid_q <= own_adm_accept_w;",
            "if (owner_admission_event_valid_q)",
            "join_validation_pending_q[\n            owner_admission_event_owner_q",
        )
        for marker in island_markers:
            self.assertEqual(island.count(marker), 1, marker)

        for marker in (
            "ZHAO_V3OWN_T4_MUTANT_ADMISSION_STALE",
            "ZHAO_V3OWN_T4_MUTANT_RESERVATION_STALE",
            "ZHAO_V3OWN_T4_EVENT_MUTANT_SELECTOR_COLLISION",
            "ZHAO_V3OWN_ADMISSION_EVENT_OWNER",
            "ZHAO_V3OWN_RESERVATION_EVENT_OWNER",
        ):
            self.assertGreaterEqual(mutant.count(marker), 1, marker)
        for marker in (
            "t_v3own_t4a admission_identity ZHAO_V3OWN_T4_MUTANT_ADMISSION_STALE",
            "t_v3own_t4r reservation_identity ZHAO_V3OWN_T4_MUTANT_RESERVATION_STALE",
            "add_test(NAME texture_v3own_timing4_${NAME}_control",
            "add_test(NAME texture_v3own_timing4_event_selector_collision",
            "tests/mutants/zhao_texture_v3own_timing4_event_mutants.sv",
        ):
            self.assertEqual(cmake.count(marker), 1, marker)
        for marker in (
            '"a_admission_event_identity", "a_reservation_event_identity"',
            "set(labels) != {expected}",
            "ZHAO_V3OWN_T4_EVENT_MUTANT_SELECTOR_COLLISION",
        ):
            self.assertIn(marker, control)

        with self.assertRaises(AssertionError):
            mutated = owner.replace("if (ctxw_v_q &&", "if (adm_fire_c &&", 1)
            for marker in owner_markers:
                if mutated.count(marker) != 1:
                    raise AssertionError(marker)

    def test_material_writeback_cut_reaches_all_committed_mutants(self) -> None:
        production = (
            REPO / "fpga/rtl/texture/zhao_texture_material_combine_v3.sv"
        ).read_text(encoding="utf-8")
        mutants = (
            REPO / "tests/mutants/zhao_texture_material_combine_v3_mutants.sv"
        ).read_text(encoding="utf-8")
        marker_counts = {
            "logic r_v, d_v, s_v, o_v, m_v, f_v;": 13,
            "logic wb_v;": 13,
            "s_v <= d_v;": 13,
            "o_v <= s_v;": 12,
            "m_p0 <= o_a0 * o_b0;": 13,
            "m_p1 <= o_a1 * o_b1;": 13,
            "f_v <= m_v;": 13,
            "f_lane0 <= finish_lane(": 12,
            "f_lane1 <= finish_lane(": 12,
            "wb_v <= f_v;": 12,
            "wb_row <= {f_status, f_index, next_scratch};": 12,
            "scr_we = wb_v && !wb_final;": 12,
            "scr_we = wb_v && !wb_final && !wb_drop;": 1,
            "cmp_we = wb_v && wb_final;": 13,
            "&& !wb_v": 13,
            "if (wb_v) begin": 13,
        }
        for marker, mutant_count in marker_counts.items():
            production_count = 0 if marker == (
                "scr_we = wb_v && !wb_final && !wb_drop;") else 1
            self.assertEqual(production.count(marker), production_count, marker)
            self.assertEqual(mutants.count(marker), mutant_count, marker)

        bypass_controls = {
            "o_v <= d_v;": 1,
            "wb_v <= m_v;": 1,
            "wb_row <= {m_status, m_index, next_scratch};": 1,
        }
        for marker, count in bypass_controls.items():
            self.assertNotIn(marker, production)
            self.assertEqual(mutants.count(marker), count, marker)

        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        for marker in (
            "skip_s_capture MATERIAL_V3_MUTANT_SKIP_S",
            "zhao_texture_material_combine_v3_skip_s_capture_mutant",
            "skip_f_finish MATERIAL_V3_MUTANT_SKIP_F",
            "zhao_texture_material_combine_v3_skip_f_finish_mutant",
        ):
            self.assertEqual(cmake.count(marker), 1, marker)
        with self.assertRaises(AssertionError):
            self.assertEqual(
                mutants.replace("o_v <= d_v;", "o_v <= s_v;", 1).count(
                    "o_v <= d_v;"), 1
            )

    def test_rcp_v4_balanced_lzc_direct_oracle_and_mutant_are_exact(self) -> None:
        production = (
            REPO / "fpga/rtl/raster/zhao_raster_rcp24_v4.sv"
        ).read_text(encoding="utf-8")
        mutant = (
            REPO / "tests/mutants/zhao_raster_rcp24_v4_timing_mutants.sv"
        ).read_text(encoding="utf-8")
        driver = (
            REPO / "tests/raster/raster_rcp24_v4_timing_directed.cpp"
        ).read_text(encoding="utf-8")
        manifest = (
            REPO / "tests/raster/rcp24_v4_timing.sources.txt"
        ).read_text(encoding="utf-8").splitlines()
        expected = [
            "fpga/rtl/common/zhao_render_texture_pkg.sv",
            "fpga/rtl/field/zhao_field_rcp24_rom.sv",
            "fpga/rtl/raster/zhao_raster_ticketq.sv",
            "fpga/rtl/raster/zhao_raster_ticketq_rh.sv",
            "fpga/rtl/raster/zhao_raster_rcp24_mul.sv",
            "tests/mutants/zhao_raster_rcp24_v4_timing_mutants.sv",
            "fpga/rtl/raster/zhao_raster_rcp24_v4.sv",
        ]
        self.assertEqual(manifest, expected)
        for marker in (
            "function automatic logic [4:0] leading_zero24",
            "e_in_c = leading_zero24(`ZHAO_RCP_V4_LZC_VALUE(d_i));",
            "a0_e_q      <= e_in_c;",
            "a0_zero_q   <= (d_i == 24'd0);",
            "a1_k_q    <= a0_zero_q ? 6'd0",
        ):
            self.assertEqual(production.count(marker), 1, marker)
        self.assertNotIn("for (int unsigned b = 0; b < 24", production)
        self.assertEqual(mutant.count("ZHAO_RCP_V4_LZC_MUTANT_REVERSE"), 1)
        self.assertIn("value[0], value[1], value[2]", mutant)
        for marker in (
            "zref::rcp_u24",
            "result remains stable under output backpressure",
            "occupancy is accepted minus completed",
            "the committed LZC orientation mutant is detected by the oracle",
        ):
            self.assertIn(marker, driver)
        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        for marker in (
            "RCP24 V4 timing source manifest is not the exact seven-file closure",
            "add_test(NAME raster_rcp24_v4_timing_directed",
            "add_test(NAME raster_rcp24_v4_lzc_orientation_mutant",
            "-DZHAO_RCP_V4_LZC_MUTANT_REVERSE",
        ):
            self.assertEqual(cmake.count(marker), 1, marker)
        with self.assertRaises(AssertionError):
            self.assertIn(
                "a0_e_q      <= e_in_c;",
                production.replace("a0_e_q      <= e_in_c;", "", 1),
            )

    def test_observation_successor_cmake_uses_exact_durable_manifest(self) -> None:
        cmake_text = (REPO / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        manifest_text = OBSERVATION_SUCCESSOR_MANIFEST.read_text(encoding="utf-8")
        sources = validate_observation_successor_cmake(
            cmake_text, manifest_text, REPO
        )
        self.assertEqual(len(sources), 9)
        self.assertEqual(len(sources), len(set(sources)))
        self.assertEqual(
            sources[-1], "tests/raster/tb_packetb_observation_successors.sv"
        )

        replacement = "fpga/rtl/raster/zhao_missing_observation_control.sv"
        controls = (
            ("blank", manifest_text.replace("\n", "\n\n", 1), None),
            ("duplicate", manifest_text + sources[0] + "\n", None),
            ("absolute", "C:/absolute/control.sv\n" + manifest_text, None),
            ("backslash", manifest_text.replace("/", "\\", 1), None),
            ("missing", manifest_text.replace(sources[0], replacement, 1), REPO),
        )
        for name, mutated, root in controls:
            with self.subTest(control=name), self.assertRaises(AssertionError):
                parse_observation_successor_manifest(mutated, root)

        literal_duplicate = cmake_text.replace(
            "set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES)\n",
            "set(ZHAO_PACKETB_OBSERVATION_SUCCESSOR_SOURCES)\n"
            "  ${CMAKE_SOURCE_DIR}/fpga/rtl/raster/zhao_raster_rcp24_v3.sv\n",
            1,
        )
        with self.assertRaisesRegex(AssertionError, "duplicates a manifest source"):
            validate_observation_successor_cmake(
                literal_duplicate, manifest_text, REPO
            )

        registration = (
            "add_test(NAME packetb_observation_successors\n"
            "         COMMAND pb_obs)\n"
            "set_tests_properties(packetb_observation_successors PROPERTIES\n"
            "  LABELS \"fast;nightly;packet-b\" TIMEOUT 1200)"
        )
        self.assertIn(registration, cmake_text)
        commented_registration = cmake_text.replace(
            registration,
            "\n".join("# " + line for line in registration.splitlines()),
            1,
        )
        with self.assertRaisesRegex(AssertionError, "CMake/manifest binding differs"):
            validate_observation_successor_cmake(
                commented_registration, manifest_text, REPO
            )

    def test_top_profiles_mutants_and_quiet_accounting_are_exact(self) -> None:
        cmake_text = (REPO / "tests" / "CMakeLists.txt").read_text(
            encoding="utf-8"
        )
        validate_top_profiles_mutants_and_quiet_accounting(cmake_text)

        duplicate_selector = cmake_text.replace(
            "  ZHAO_PACKET_B_MUTANT_AUX_AS_SAMPLE2\n",
            "  ZHAO_PACKET_B_MUTANT_DISPATCH_INDEX_ROUTE\n",
            1,
        )
        with self.assertRaisesRegex(AssertionError, "mapping|unique"):
            validate_top_profiles_mutants_and_quiet_accounting(duplicate_selector)

        missing_production_witness = cmake_text.replace(
            "  PACKET_B_EXPECT_NO_SHADOWS=1)\n",
            ")\n",
            1,
        )
        with self.assertRaisesRegex(AssertionError, "registration is missing"):
            validate_top_profiles_mutants_and_quiet_accounting(
                missing_production_witness
            )

        missing_timing_control = cmake_text.replace(
            "          PacketAOwnershipAndClosureTests."
            "test_aux_registered_clamp_transport_reaches_all_committed_mutants\n",
            "",
            1,
        )
        with self.assertRaisesRegex(AssertionError, "timing source control"):
            validate_top_profiles_mutants_and_quiet_accounting(missing_timing_control)


if __name__ == "__main__":
    unittest.main()
