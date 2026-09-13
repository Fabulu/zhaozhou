#!/usr/bin/env python3
"""Read-only shell-fit QSF and CMake source-pool preflight.

The parser intentionally handles assignment syntax rather than sourcing Tcl. The
shell-fit project uses literal file assignments, so variables, globs, and Tcl
control flow are rejected instead of being guessed.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import shlex
import sys
from typing import Sequence

from shell_ports import ShellPortError


@dataclass(frozen=True)
class QsfModel:
    top: str
    sources: tuple[str, ...]
    constraint_files: tuple[str, ...]
    virtual_pin_targets: tuple[str, ...]
    wildcard_targets: tuple[str, ...]


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
    wildcard_targets: list[str] = []
    for line_number, raw_line in enumerate(text.splitlines(), 1):
        _reject_unsupported_tcl_syntax(raw_line, line_number)
        try:
            tokens = shlex.split(raw_line, comments=True, posix=True)
        except ValueError as exc:
            raise ShellPortError(f"malformed QSF line {line_number}: {exc}") from exc
        if not tokens:
            continue
        allowed_commands = {
            "set_global_assignment",
            "set_instance_assignment",
            "set_location_assignment",
        }
        if tokens[0] not in allowed_commands:
            raise ShellPortError(
                f"unsupported active QSF command on line {line_number}: {tokens[0]!r}"
            )
        target = ""
        if "-to" in tokens:
            target_at = tokens.index("-to")
            if target_at + 1 >= len(tokens):
                raise ShellPortError(f"QSF line {line_number} has -to without a target")
            target = tokens[target_at + 1]
            if any(marker in target for marker in ("*", "?", "[", "]")):
                wildcard_targets.append(target)
        if "-name" not in tokens:
            continue
        name_at = tokens.index("-name")
        if name_at + 1 >= len(tokens):
            raise ShellPortError(f"QSF line {line_number} has -name without a value")
        name = tokens[name_at + 1].upper()
        assignment_value = tokens[name_at + 2] if name_at + 2 < len(tokens) else ""
        if name == "TOP_LEVEL_ENTITY":
            tops.append(assignment_value)
        elif name in {"SYSTEMVERILOG_FILE", "VERILOG_FILE"}:
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
        elif name == "VIRTUAL_PIN":
            virtual_targets.append(target or "<global>")
    if len(tops) != 1:
        raise ShellPortError(f"expected one QSF TOP_LEVEL_ENTITY, found {len(tops)}")
    return QsfModel(
        top=tops[0],
        sources=tuple(sources),
        constraint_files=tuple(constraint_files),
        virtual_pin_targets=tuple(virtual_targets),
        wildcard_targets=tuple(wildcard_targets),
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
    if model.virtual_pin_targets:
        errors.append(
            "QSF contains VIRTUAL_PIN assignments to "
            + ", ".join(repr(target) for target in model.virtual_pin_targets)
        )
    if model.wildcard_targets:
        errors.append(
            "QSF contains wildcard assignment targets "
            + ", ".join(repr(target) for target in model.wildcard_targets)
        )
    if errors:
        raise ShellPortError("; ".join(errors))


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--qsf", type=Path, required=True)
    parser.add_argument("--sdc", type=Path, required=True)
    parser.add_argument("--variable", default="ZHAO_SHELL_RTL")
    parser.add_argument("--wrapper", default="fpga/rtl/generated/zhao_shell_fit_top.sv")
    parser.add_argument("--top", default="zhao_shell_fit_top")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo = args.repo_root.resolve()
    cmake = args.cmake if args.cmake.is_absolute() else repo / args.cmake
    qsf = args.qsf if args.qsf.is_absolute() else repo / args.qsf
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
    except (OSError, ShellPortError) as exc:
        print(f"shell-fit-qsf: {exc}", file=sys.stderr)
        return 1
    print(
        f"shell-fit-qsf: top={model.top} sources={len(model.sources)} "
        "virtual-pins=none wildcard-targets=none"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
