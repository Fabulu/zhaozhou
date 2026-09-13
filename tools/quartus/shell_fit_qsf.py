#!/usr/bin/env python3
"""Read-only shell-fit QSF and CMake source-pool preflight.

The parser intentionally handles assignment syntax rather than sourcing Tcl. The
shell-fit project uses literal file assignments, so variables, globs, and Tcl
control flow are rejected instead of being guessed.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import re
import shlex
import sys
from typing import Mapping, Sequence

from shell_ports import ShellPortError


@dataclass(frozen=True)
class QsfModel:
    top: str
    sources: tuple[str, ...]
    constraint_files: tuple[str, ...]
    virtual_pin_targets: tuple[str, ...]
    physical_pin_targets: tuple[str, ...]
    wildcard_targets: tuple[str, ...]
    global_assignments: tuple[tuple[str, str], ...]
    instance_assignments: tuple[tuple[str, str, str], ...]


@dataclass(frozen=True)
class QpfModel:
    project_revision: str
    quartus_version: str
    assignments: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class SdcClosureEntry:
    path: str
    data: bytes


REQUIRED_QPF_SETTINGS = {
    "QUARTUS_VERSION": "17.0",
    "DATE": "00:00:00  August 18, 2026",
    "PROJECT_REVISION": "zhao_shell_fit",
}


REQUIRED_QSF_SETTINGS = {
    "FAMILY": "Cyclone V",
    "DEVICE": "5CSEBA6U23I7",
    "TOP_LEVEL_ENTITY": "zhao_shell_fit_top",
    "PROJECT_OUTPUT_DIRECTORY": "output_files",
    "SEED": "1",
    "OPTIMIZATION_MODE": "HIGH PERFORMANCE EFFORT",
    "OPTIMIZATION_TECHNIQUE": "BALANCED",
    "FITTER_EFFORT": "STANDARD FIT",
    "PLACEMENT_EFFORT_MULTIPLIER": "1.0",
    "PHYSICAL_SYNTHESIS_REGISTER_RETIMING": "OFF",
    "LAST_QUARTUS_VERSION": "17.0.2 Lite Edition",
}


def _balanced_parenthesis(text: str, opening: int) -> int:
    depth = 0
    quoted = False
    escaped = False
    for index in range(opening, len(text)):
        char = text[index]
        if escaped:
            escaped = False
            continue
        if char == "\\":
            escaped = True
            continue
        if char == '"':
            quoted = not quoted
            continue
        if quoted:
            continue
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index
    raise ShellPortError("unterminated CMake set() source pool")


def _repo_path(raw: str, *, base: Path, repo: Path) -> str:
    value = raw.replace("\\", "/")
    prefix = "${PROJECT_SOURCE_DIR}/"
    if value.startswith(prefix):
        value = value[len(prefix) :]
        candidate = repo / value
    else:
        path = Path(value)
        candidate = path if path.is_absolute() else base / path
    try:
        return candidate.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError as exc:
        raise ShellPortError(f"source path escapes repository: {raw!r}") from exc


def _expand_cmake_token(
    token: str,
    *,
    text: str,
    repo: Path,
) -> str:
    scalars: dict[str, str] = {
        "PROJECT_SOURCE_DIR": str(repo),
        "CMAKE_SOURCE_DIR": str(repo),
    }
    scalar_re = re.compile(
        r"\bset\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+([^\s()]+)\s*\)"
    )
    for match in scalar_re.finditer(text):
        scalars[match.group(1)] = match.group(2)
    result = token
    for _ in range(16):
        references = re.findall(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}", result)
        if not references:
            return result
        unknown = [name for name in references if name not in scalars]
        if unknown:
            raise ShellPortError(
                f"unresolved CMake variable(s) {sorted(set(unknown))} in source {token!r}"
            )
        result = re.sub(
            r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}",
            lambda match: scalars[match.group(1)],
            result,
        )
    raise ShellPortError(f"recursive CMake variable expansion in source {token!r}")


def parse_cmake_source_pool(
    text: str,
    *,
    variable: str,
    repo: Path,
    cmake_dir: Path,
) -> tuple[str, ...]:
    matches = list(re.finditer(r"\bset\s*\(\s*" + re.escape(variable) + r"\b", text))
    if len(matches) != 1:
        raise ShellPortError(
            f"expected one CMake set({variable} ...), found {len(matches)}"
        )
    opening = text.find("(", matches[0].start())
    closing = _balanced_parenthesis(text, opening)
    body = text[matches[0].end() : closing]
    body = "\n".join(line.split("#", 1)[0] for line in body.splitlines())
    try:
        tokens = shlex.split(body, posix=True)
    except ValueError as exc:
        raise ShellPortError(f"malformed CMake source pool {variable}: {exc}") from exc
    if not tokens:
        raise ShellPortError(f"CMake source pool {variable} is empty")
    sources = tuple(
        _repo_path(
            _expand_cmake_token(token, text=text, repo=repo),
            base=cmake_dir,
            repo=repo,
        )
        for token in tokens
    )
    duplicates = sorted({source for source in sources if sources.count(source) > 1})
    if duplicates:
        raise ShellPortError(f"CMake source pool has duplicates: {duplicates}")
    return sources


def _reject_unsupported_tcl_syntax(raw_line: str, line_number: int) -> None:
    """Reject Tcl features that could conceal a second/source command."""
    quoted = False
    escaped = False
    for char in raw_line:
        if escaped:
            escaped = False
            continue
        if char == "\\":
            escaped = True
            continue
        if char == '"':
            quoted = not quoted
            continue
        if char == "#" and not quoted:
            break
        if char == ";" and not quoted:
            raise ShellPortError(
                f"unsupported Tcl command boundary on QSF line {line_number}"
            )
        if char in "{}" and not quoted:
            raise ShellPortError(
                f"unsupported Tcl grouping on QSF line {line_number}"
            )
        if char in "$[]":
            raise ShellPortError(
                f"unsupported active Tcl substitution on QSF line {line_number}"
            )
    if escaped:
        raise ShellPortError(
            f"unsupported Tcl line continuation on QSF line {line_number}"
        )
    if quoted:
        raise ShellPortError(f"unterminated quote on QSF line {line_number}")


def _literal_assignment_path(
    value: str, *, line_number: int, label: str, repo: Path, qsf_dir: Path
) -> str:
    if not value:
        raise ShellPortError(f"QSF line {line_number} has no {label} path")
    if any(marker in value for marker in ("$", "[", "]", "*", "?")):
        raise ShellPortError(
            f"QSF {label} line {line_number} is not a literal path: {value!r}"
        )
    return _repo_path(value, base=qsf_dir, repo=repo)


def parse_qsf(text: str, *, repo: Path, qsf_dir: Path) -> QsfModel:
    tops: list[str] = []
    sources: list[str] = []
    constraint_files: list[str] = []
    virtual_targets: list[str] = []
    physical_targets: list[str] = []
    wildcard_targets: list[str] = []
    global_assignments: list[tuple[str, str]] = []
    instance_assignments: list[tuple[str, str, str]] = []
    for line_number, raw_line in enumerate(text.splitlines(), 1):
        _reject_unsupported_tcl_syntax(raw_line, line_number)
        try:
            tokens = shlex.split(raw_line, comments=True, posix=True)
        except ValueError as exc:
            raise ShellPortError(f"malformed QSF line {line_number}: {exc}") from exc
        if not tokens:
            continue
        command = tokens[0]
        if command == "set_global_assignment":
            if len(tokens) != 4 or tokens[1] != "-name":
                raise ShellPortError(
                    "QSF set_global_assignment line "
                    f"{line_number} must have exact shape "
                    "'set_global_assignment -name NAME VALUE'"
                )
            name = tokens[2].upper()
            assignment_value = tokens[3]
            global_assignments.append((name, assignment_value))
            if name == "TOP_LEVEL_ENTITY":
                tops.append(assignment_value)
            elif name == "SYSTEMVERILOG_FILE":
                sources.append(
                    _literal_assignment_path(
                        assignment_value,
                        line_number=line_number,
                        label="RTL source",
                        repo=repo,
                        qsf_dir=qsf_dir,
                    )
                )
            elif name == "SDC_FILE":
                constraint_files.append(
                    _literal_assignment_path(
                        assignment_value,
                        line_number=line_number,
                        label="SDC constraint",
                        repo=repo,
                        qsf_dir=qsf_dir,
                    )
                )
            elif (
                name.endswith("_FILE")
                or name.endswith("_FILES")
                or name in {"SEARCH_PATH", "USER_LIBRARIES", "QIP_FILE"}
            ):
                raise ShellPortError(
                    f"unsupported source-bearing QSF assignment on line {line_number}: {name!r}"
                )
            continue
        if command == "set_instance_assignment":
            if (
                len(tokens) != 6
                or tokens[1] != "-name"
                or tokens[4] != "-to"
                or not tokens[2]
                or not tokens[3]
                or not tokens[5]
            ):
                raise ShellPortError(
                    "QSF set_instance_assignment line "
                    f"{line_number} must have exact shape "
                    "'set_instance_assignment -name NAME VALUE -to LITERAL_TARGET'"
                )
            name = tokens[2].upper()
            value = tokens[3]
            target = tokens[5]
            if any(marker in target for marker in ("*", "?", "[", "]")):
                wildcard_targets.append(target)
            if name == "VIRTUAL_PIN":
                virtual_targets.append(target)
            instance_assignments.append((name, value, target))
            continue
        if command == "set_location_assignment":
            if (
                len(tokens) != 4
                or tokens[2] != "-to"
                or not tokens[1]
                or not tokens[3]
            ):
                raise ShellPortError(
                    "QSF set_location_assignment line "
                    f"{line_number} must have exact shape "
                    "'set_location_assignment LOCATION -to LITERAL_TARGET'"
                )
            target = tokens[3]
            if any(marker in target for marker in ("*", "?", "[", "]")):
                wildcard_targets.append(target)
            physical_targets.append(target)
            continue
        raise ShellPortError(
            f"unsupported active QSF command on line {line_number}: {command!r}"
        )
    if len(tops) != 1:
        raise ShellPortError(f"expected one QSF TOP_LEVEL_ENTITY, found {len(tops)}")
    return QsfModel(
        top=tops[0],
        sources=tuple(sources),
        constraint_files=tuple(constraint_files),
        virtual_pin_targets=tuple(virtual_targets),
        physical_pin_targets=tuple(physical_targets),
        wildcard_targets=tuple(wildcard_targets),
        global_assignments=tuple(global_assignments),
        instance_assignments=tuple(instance_assignments),
    )


def validate_qsf(
    model: QsfModel,
    *,
    expected_top: str,
    expected_sources: Sequence[str],
    expected_constraints: Sequence[str],
) -> None:
    errors: list[str] = []
    if model.top != expected_top:
        errors.append(f"QSF top {model.top!r} != expected {expected_top!r}")
    allowed_global_assignments = set(REQUIRED_QSF_SETTINGS) | {
        "SYSTEMVERILOG_FILE",
        "SDC_FILE",
        "VERILOG_MACRO",
    }
    unsupported_assignments = sorted(
        {name for name, _value in model.global_assignments if name not in allowed_global_assignments}
    )
    if unsupported_assignments:
        errors.append(
            f"QSF contains unbound global assignments {unsupported_assignments}"
        )
    macro_values = [
        value for name, value in model.global_assignments if name == "VERILOG_MACRO"
    ]
    if macro_values != ["QUARTUS_SYNTHESIS=1"]:
        errors.append(
            "QSF VERILOG_MACRO closure is "
            f"{macro_values!r}, expected one 'QUARTUS_SYNTHESIS=1'"
        )
    for name, expected in REQUIRED_QSF_SETTINGS.items():
        values = [value for key, value in model.global_assignments if key == name]
        if values != [expected]:
            errors.append(
                f"QSF effective setting {name} is {values!r}, expected one {expected!r}"
            )
    duplicates = sorted(
        {source for source in model.sources if model.sources.count(source) > 1}
    )
    if duplicates:
        errors.append(f"QSF source pool has duplicates: {duplicates}")
    if tuple(model.sources) != tuple(expected_sources):
        expected_set = set(expected_sources)
        actual_set = set(model.sources)
        missing = [source for source in expected_sources if source not in actual_set]
        extra = [source for source in model.sources if source not in expected_set]
        if missing or extra:
            errors.append(f"QSF source pool mismatch: missing={missing}, extra={extra}")
        elif not duplicates:
            first = next(
                index
                for index, pair in enumerate(zip(expected_sources, model.sources))
                if pair[0] != pair[1]
            )
            errors.append(
                "QSF source order diverges at ordinal "
                f"{first}: expected {expected_sources[first]!r}, got {model.sources[first]!r}"
            )
    constraint_duplicates = sorted(
        {
            constraint
            for constraint in model.constraint_files
            if model.constraint_files.count(constraint) > 1
        }
    )
    if constraint_duplicates:
        errors.append(f"QSF SDC pool has duplicates: {constraint_duplicates}")
    if tuple(model.constraint_files) != tuple(expected_constraints):
        expected_constraint_set = set(expected_constraints)
        actual_constraint_set = set(model.constraint_files)
        missing_constraints = [
            constraint
            for constraint in expected_constraints
            if constraint not in actual_constraint_set
        ]
        extra_constraints = [
            constraint
            for constraint in model.constraint_files
            if constraint not in expected_constraint_set
        ]
        if missing_constraints or extra_constraints:
            errors.append(
                "QSF SDC pool mismatch: "
                f"missing={missing_constraints}, extra={extra_constraints}"
            )
        elif not constraint_duplicates:
            first = next(
                index
                for index, pair in enumerate(
                    zip(expected_constraints, model.constraint_files)
                )
                if pair[0] != pair[1]
            )
            errors.append(
                "QSF SDC order diverges at ordinal "
                f"{first}: expected {expected_constraints[first]!r}, "
                f"got {model.constraint_files[first]!r}"
            )
    if model.instance_assignments:
        errors.append(
            "QSF contains unapproved instance assignments "
            + ", ".join(
                repr({"name": name, "value": value, "target": target})
                for name, value, target in model.instance_assignments
            )
        )
    if model.virtual_pin_targets:
        errors.append(
            "QSF contains VIRTUAL_PIN assignments to "
            + ", ".join(repr(target) for target in model.virtual_pin_targets)
        )
    if model.physical_pin_targets:
        errors.append(
            "QSF contains physical pin assignments to "
            + ", ".join(repr(target) for target in model.physical_pin_targets)
        )
    if model.wildcard_targets:
        errors.append(
            "QSF contains wildcard assignment targets "
            + ", ".join(repr(target) for target in model.wildcard_targets)
        )
    if errors:
        raise ShellPortError("; ".join(errors))


def parse_qpf(text: str) -> QpfModel:
    assignments: dict[str, str] = {}
    for line_number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r'([A-Z_]+)\s*=\s*"([^"]*)"', line)
        if match is None:
            raise ShellPortError(f"unsupported active QPF syntax on line {line_number}")
        key, value = match.groups()
        if key in assignments:
            raise ShellPortError(f"duplicate QPF assignment {key!r}")
        assignments[key] = value
    revision = assignments.get("PROJECT_REVISION")
    version = assignments.get("QUARTUS_VERSION")
    if revision != "zhao_shell_fit":
        raise ShellPortError(
            f"QPF PROJECT_REVISION is {revision!r}, expected 'zhao_shell_fit'"
        )
    if version != "17.0":
        raise ShellPortError(f"QPF QUARTUS_VERSION is {version!r}, expected '17.0'")
    if assignments != REQUIRED_QPF_SETTINGS:
        raise ShellPortError(
            f"QPF assignment closure is {assignments!r}, expected {REQUIRED_QPF_SETTINGS!r}"
        )
    return QpfModel(
        project_revision=revision,
        quartus_version=version,
        assignments=tuple(assignments.items()),
    )


def _sdc_logical_lines(text: str, *, path: str) -> tuple[str, ...]:
    logical: list[str] = []
    pending = ""
    for line_number, raw in enumerate(text.splitlines(), 1):
        stripped = raw.strip()
        if not pending and (not stripped or stripped.startswith("#")):
            continue
        if ";" in stripped:
            raise ShellPortError(f"SDC {path}:{line_number} contains a Tcl command boundary")
        continuation = stripped.endswith("\\")
        fragment = stripped[:-1].rstrip() if continuation else stripped
        pending = f"{pending} {fragment}".strip()
        if not continuation:
            if pending:
                logical.append(" ".join(pending.split()))
            pending = ""
    if pending:
        raise ShellPortError(f"SDC {path} ends with an unterminated continuation")
    return tuple(logical)


def read_sdc_closure(root: Path, *, repo: Path) -> tuple[SdcClosureEntry, ...]:
    repo = repo.resolve()
    root_path = root.resolve()
    try:
        root_relative = root_path.relative_to(repo).as_posix()
    except ValueError as exc:
        raise ShellPortError(f"SDC root escapes repository: {root}") from exc
    entries: list[SdcClosureEntry] = []
    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(relative: str) -> None:
        if relative in visiting:
            raise ShellPortError(f"SDC source cycle reaches {relative!r}")
        if relative in visited:
            raise ShellPortError(f"SDC closure sources {relative!r} more than once")
        path = (repo / relative).resolve()
        try:
            path.relative_to(repo)
        except ValueError as exc:
            raise ShellPortError(f"SDC source escapes repository: {relative!r}") from exc
        data = path.read_bytes()
        try:
            text = data.decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ShellPortError(f"SDC {relative!r} is not UTF-8: {exc}") from exc
        visiting.add(relative)
        entries.append(SdcClosureEntry(path=relative, data=data))
        for command in _sdc_logical_lines(text, path=relative):
            head = command.split(None, 1)[0]
            if head not in {"create_clock", "set_clock_groups", "source"}:
                raise ShellPortError(
                    f"SDC closure contains forbidden or unsupported command {head!r} in {relative!r}"
                )
            if re.search(r"\[\s*(?:source|eval|uplevel)\b", command) or "$" in command:
                raise ShellPortError(f"SDC {relative!r} contains active Tcl indirection")
            if head != "source":
                continue
            try:
                tokens = shlex.split(command, posix=True)
            except ValueError as exc:
                raise ShellPortError(f"malformed SDC source in {relative!r}: {exc}") from exc
            if len(tokens) != 2:
                raise ShellPortError(f"SDC source in {relative!r} must name one literal file")
            child = tokens[1]
            if child.startswith("{") and child.endswith("}"):
                child = child[1:-1]
            if not child or any(marker in child for marker in ("$", "[", "]", "*", "?")):
                raise ShellPortError(f"SDC source in {relative!r} is not a literal path")
            child_relative = _repo_path(child, base=path.parent, repo=repo)
            visit(child_relative)
        visiting.remove(relative)
        visited.add(relative)

    visit(root_relative)
    return tuple(entries)


def validate_sdc_closure(entries: Sequence[SdcClosureEntry]) -> None:
    if not entries:
        raise ShellPortError("SDC closure is empty")
    commands: list[str] = []
    for entry in entries:
        text = entry.data.decode("utf-8")
        commands.extend(
            command
            for command in _sdc_logical_lines(text, path=entry.path)
            if not command.startswith("source ")
        )
    clocks: list[tuple[str, str, str]] = []
    groups: list[str] = []
    for command in commands:
        if command.startswith("create_clock "):
            match = re.fullmatch(
                r"create_clock -name ([A-Za-z_][A-Za-z0-9_]*) -period ([0-9]+(?:\.[0-9]+)?) "
                r"\[get_ports \{([A-Za-z_][A-Za-z0-9_]*)\}\]",
                command,
            )
            if match is None:
                raise ShellPortError(f"SDC create_clock is not in the pinned literal form: {command!r}")
            clocks.append(match.groups())
        elif command.startswith("set_clock_groups "):
            groups.append(command)
    expected_clocks = [
        ("gpu_clk", "10.000", "gpu_clk"),
        ("vid_clk", "20.000", "vid_clk"),
        ("audio_clk", "40.000", "audio_clk"),
    ]
    if clocks != expected_clocks:
        raise ShellPortError(f"SDC effective clocks are {clocks!r}, expected {expected_clocks!r}")
    expected_group = (
        "set_clock_groups -asynchronous -group [get_clocks {audio_clk}] "
        "-group [get_clocks {gpu_clk vid_clk}]"
    )
    if groups != [expected_group]:
        raise ShellPortError(
            "SDC effective clock groups do not preserve the pinned audio-only asynchronous cut"
        )
    if len(commands) != 4:
        raise ShellPortError(
            f"SDC closure contains {len(commands)} effective constraints, expected exactly 4"
        )


def _write_json_atomic(path: Path, payload: Mapping[str, object]) -> None:
    import json
    import os
    import uuid

    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
    try:
        with temporary.open("xb") as stream:
            stream.write((json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8"))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--qsf", type=Path, required=True)
    parser.add_argument("--qpf", type=Path, required=True)
    parser.add_argument("--sdc", type=Path, required=True)
    parser.add_argument("--emit-model", type=Path)
    parser.add_argument("--variable", default="ZHAO_SHELL_RTL")
    parser.add_argument("--wrapper", default="fpga/rtl/generated/zhao_shell_fit_top.sv")
    parser.add_argument("--top", default="zhao_shell_fit_top")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo = args.repo_root.resolve()
    cmake = args.cmake if args.cmake.is_absolute() else repo / args.cmake
    qsf = args.qsf if args.qsf.is_absolute() else repo / args.qsf
    qpf = args.qpf if args.qpf.is_absolute() else repo / args.qpf
    sdc = args.sdc if args.sdc.is_absolute() else repo / args.sdc
    try:
        pool = parse_cmake_source_pool(
            cmake.read_text(encoding="utf-8"),
            variable=args.variable,
            repo=repo,
            cmake_dir=cmake.parent,
        )
        wrapper = _repo_path(args.wrapper, base=repo, repo=repo)
        model = parse_qsf(
            qsf.read_text(encoding="utf-8"), repo=repo, qsf_dir=qsf.parent
        )
        validate_qsf(
            model,
            expected_top=args.top,
            expected_sources=(*pool, wrapper),
            expected_constraints=(_repo_path(str(sdc), base=repo, repo=repo),),
        )
        qpf_model = parse_qpf(qpf.read_text(encoding="utf-8"))
        sdc_closure = read_sdc_closure(sdc, repo=repo)
        validate_sdc_closure(sdc_closure)
        if args.emit_model is not None:
            try:
                emit_path = args.emit_model.resolve()
                emit_path.relative_to(repo)
            except ValueError as exc:
                raise ShellPortError("QSF model output must remain inside the repository") from exc
            _write_json_atomic(
                emit_path,
                {
                    "top": model.top,
                    "projectRevision": qpf_model.project_revision,
                    "quartusVersion": qpf_model.quartus_version,
                    "qpfAssignments": [
                        {"name": name, "value": value}
                        for name, value in qpf_model.assignments
                    ],
                    "sources": list(model.sources),
                    "constraints": [
                        {"path": entry.path, "sha256": hashlib.sha256(entry.data).hexdigest()}
                        for entry in sdc_closure
                    ],
                    "effectiveSettings": {
                        name: [value for key, value in model.global_assignments if key == name][0]
                        for name in REQUIRED_QSF_SETTINGS
                    },
                    "globalAssignments": [
                        {"name": name, "value": value}
                        for name, value in model.global_assignments
                    ],
                },
            )
    except (OSError, ShellPortError) as exc:
        print(f"shell-fit-qsf: {exc}", file=sys.stderr)
        return 1
    print(
        f"shell-fit-qsf: revision={qpf_model.project_revision} top={model.top} "
        f"sources={len(model.sources)} sdc-closure={len(sdc_closure)} "
        "virtual-pins=none physical-pins=none wildcard-targets=none"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
