#!/usr/bin/env python3
"""Parse and bind shell-fit receipt, fitter, clock, message, and hierarchy evidence.

This module reads committed artifacts and uses read-only Git object/status queries. It
does not invoke Quartus and does not infer wrapper cost by subtracting hierarchy
totals.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Mapping, Sequence

from shell_fit_qsf import parse_cmake_source_pool, parse_qsf, validate_qsf
from shell_ports import (
    ShellPortError,
    discover_type_signedness,
    discover_type_widths,
    parse_module_declaration,
)


@dataclass(frozen=True)
class FitSummary:
    status: str
    tool_name: str
    tool_version: str
    device: str
    alms: int
    registers: int
    memory_bits: int
    ram_blocks: Decimal
    dsp_blocks: Decimal
    real_pins: int
    virtual_pins: int

    @property
    def m10ks(self) -> Decimal:
        """Compatibility name; Quartus reports this resource as Total RAM Blocks."""
        return self.ram_blocks


@dataclass(frozen=True)
class MapSummary:
    status: str
    tool_version: str
    top_entity: str
    combinational_aluts: int
    registers: int


@dataclass(frozen=True)
class MapHierarchyRow:
    node: str
    combinational_aluts: int
    combinational_aluts_self: int
    registers: int
    registers_self: int
    memory_bits: int
    dsp_blocks: Decimal
    pins: int
    virtual_pins: int
    full_hierarchy_name: str
    entity_name: str
    library_name: str


@dataclass(frozen=True)
class GitEvidence:
    source_commit: str
    clean: bool


@dataclass(frozen=True)
class ClockConstraint:
    name: str
    clock_type: str
    period_ns: Decimal
    targets: tuple[str, ...]


@dataclass(frozen=True)
class HierarchyRow:
    node: str
    alms_needed: Decimal
    alms_needed_self: Decimal
    final_placement_alms: Decimal
    final_placement_alms_self: Decimal
    dense_recoverable_alms: Decimal
    dense_recoverable_alms_self: Decimal
    unavailable_alms: Decimal
    unavailable_alms_self: Decimal
    memory_alms: Decimal
    memory_alms_self: Decimal
    combinational_aluts: int
    combinational_aluts_self: int
    registers: int
    registers_self: int
    io_registers: int
    io_registers_self: int
    memory_bits: int
    ram_blocks: Decimal
    dsp_blocks: Decimal
    pins: int
    virtual_pins: int
    full_hierarchy_name: str
    entity_name: str
    library_name: str

    @property
    def m10ks(self) -> Decimal:
        return self.ram_blocks


def _first_number(text: str, *, decimal: bool = False) -> int | Decimal:
    match = re.search(r"[-+]?\d[\d,]*(?:\.\d+)?", text)
    if not match:
        raise ShellPortError(f"field has no numeric value: {text!r}")
    token = match.group(0).replace(",", "")
    try:
        return Decimal(token) if decimal else int(Decimal(token))
    except (InvalidOperation, ValueError) as exc:
        raise ShellPortError(f"invalid numeric value {token!r}") from exc


def _number_pair(
    text: str, *, decimal: bool = False
) -> tuple[int, int] | tuple[Decimal, Decimal]:
    tokens = re.findall(r"[-+]?\d[\d,]*(?:\.\d+)?", text)
    if len(tokens) != 2:
        raise ShellPortError(f"expected total and parenthesized self value, got {text!r}")
    try:
        if decimal:
            return tuple(Decimal(token.replace(",", "")) for token in tokens)  # type: ignore[return-value]
        return tuple(int(Decimal(token.replace(",", ""))) for token in tokens)  # type: ignore[return-value]
    except (InvalidOperation, ValueError) as exc:
        raise ShellPortError(f"invalid total/self numeric pair {text!r}") from exc


def _report_field(text: str, name: str) -> str:
    escaped = re.escape(name)
    patterns = (
        re.compile(r"^\s*;\s*" + escaped + r"\s*;\s*([^;]+?)\s*;\s*$", re.M),
        re.compile(r"^\s*" + escaped + r"\s*:\s*(.*?)\s*$", re.M),
    )
    values: list[str] = []
    for pattern in patterns:
        values.extend(match.group(1).strip() for match in pattern.finditer(text))
    if len(values) != 1:
        raise ShellPortError(f"expected one {name!r} field, found {len(values)}")
    return values[0]


def _quartus_version(text: str) -> str:
    return re.sub(r"^Version\s+", "", text.strip(), count=1)


def parse_fit_summary(text: str) -> FitSummary:
    return FitSummary(
        status=_report_field(text, "Fitter Status"),
        tool_name="Quartus Prime",
        tool_version=_quartus_version(_report_field(text, "Quartus Prime Version")),
        device=_report_field(text, "Device"),
        alms=int(_first_number(_report_field(text, "Logic utilization (in ALMs)"))),
        registers=int(_first_number(_report_field(text, "Total registers"))),
        memory_bits=int(_first_number(_report_field(text, "Total block memory bits"))),
        ram_blocks=Decimal(
            _first_number(_report_field(text, "Total RAM Blocks"), decimal=True)
        ),
        dsp_blocks=Decimal(
            _first_number(_report_field(text, "Total DSP Blocks"), decimal=True)
        ),
        real_pins=int(_first_number(_report_field(text, "Total pins"))),
        virtual_pins=int(_first_number(_report_field(text, "Total virtual pins"))),
    )


def validate_fit_summary(summary: FitSummary, *, expected_real_pins: int = 10) -> None:
    errors: list[str] = []
    if not summary.status.lower().startswith("successful"):
        errors.append(f"fitter status is not successful: {summary.status!r}")
    if not re.match(r"^17\.0\.2(?:\s|$)", summary.tool_version):
        errors.append(
            f"Quartus Prime Version is not 17.0.2 evidence: {summary.tool_version!r}"
        )
    if summary.virtual_pins != 0:
        errors.append(f"Total virtual pins is {summary.virtual_pins}, expected 0")
    if summary.real_pins != expected_real_pins:
        errors.append(
            f"Total pins is {summary.real_pins}, expected {expected_real_pins} real pins"
        )
    if errors:
        raise ShellPortError("; ".join(errors))


def _normalized_report_text(text: str) -> str:
    """Collapse tool line wrapping so warnings cannot evade per-line matching."""
    return " ".join(text.split())


def scan_virtual_clock_warnings(artifacts: Mapping[str, str]) -> None:
    for name, text in artifacts.items():
        normalized = _normalized_report_text(text)
        match = re.search(
            r"clock port\s+is\s+fed\s+by\s+virtual pin\b.*?"
            r"(?:ripple clock|timing analysis)",
            normalized,
            flags=re.I,
        )
        if match:
            raise ShellPortError(
                f"bound Quartus artifact {name!r} contains forbidden "
                f"virtual-clock warning: {match.group(0)}"
            )


def validate_report_messages(text: str) -> None:
    """Require a real raw Quartus report, not a one-line cleanliness claim."""
    if not text.strip():
        raise ShellPortError("Quartus messages artifact is empty")
    required_markers = (
        "TimeQuest Timing Analyzer report for",
        "Quartus Prime Version",
    )
    missing = [marker for marker in required_markers if marker not in text]
    if missing:
        raise ShellPortError(
            "messages artifact is not recognizable raw Quartus output; "
            f"missing {missing}"
        )
    if not re.search(
        r"^Info:\s+Quartus Prime TimeQuest Timing Analyzer was successful\.\s+"
        r"0 errors,\s+\d+ warnings\s*$",
        text,
        flags=re.M,
    ):
        raise ShellPortError("raw TimeQuest messages contain no unique successful completion")
    scan_virtual_clock_warnings({"messages": text})


def parse_map_summary(summary_text: str, *, map_report_text: str) -> MapSummary:
    return MapSummary(
        status=_report_field(summary_text, "Analysis & Synthesis Status"),
        tool_version=_quartus_version(
            _report_field(summary_text, "Quartus Prime Version")
        ),
        top_entity=_report_field(summary_text, "Top-level Entity Name"),
        combinational_aluts=int(
            _first_number(
                _report_field(map_report_text, "Combinational ALUT usage for logic")
            )
        ),
        registers=int(_first_number(_report_field(summary_text, "Total registers"))),
    )


def validate_map_summary(
    summary: MapSummary, *, expected_top: str = "zhao_shell_fit_top"
) -> None:
    errors: list[str] = []
    if not summary.status.lower().startswith("successful"):
        errors.append(f"Analysis & Synthesis status is not successful: {summary.status!r}")
    if not re.match(r"^17\.0\.2(?:\s|$)", summary.tool_version):
        errors.append(f"map Quartus version is not 17.0.2: {summary.tool_version!r}")
    if summary.top_entity != expected_top:
        errors.append(
            f"map top entity is {summary.top_entity!r}, expected {expected_top!r}"
        )
    if summary.combinational_aluts <= 0 or summary.registers <= 0:
        errors.append("map ALUT/register evidence must both be nonzero")
    if errors:
        raise ShellPortError("; ".join(errors))


def parse_git_evidence(
    *,
    head: bytes,
    status: bytes,
    worktree_diff: bytes,
    staged_diff: bytes,
    index_flags: bytes,
) -> GitEvidence:
    try:
        head_text = head.decode("utf-8").strip()
        status_text = status.decode("utf-8")
        worktree_text = worktree_diff.decode("utf-8")
        staged_text = staged_diff.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"git evidence is not UTF-8: {exc}") from exc
    if not re.fullmatch(r"[0-9a-f]{40}", head_text):
        raise ShellPortError(f"git HEAD evidence is invalid: {head_text!r}")
    dirty = [
        name
        for name, value in (
            ("status", status_text.encode("utf-8")),
            ("worktree diff", worktree_text.encode("utf-8")),
            ("staged diff", staged_text.encode("utf-8")),
            ("index concealment flags", index_flags),
        )
        if value.strip(b"\x00\t\r\n ")
    ]
    if dirty:
        raise ShellPortError(
            "externally captured git/stage evidence is dirty: " + ", ".join(dirty)
        )
    return GitEvidence(source_commit=head_text, clean=True)


def _run_git(repo: Path, *arguments: str) -> bytes:
    try:
        completed = subprocess.run(
            ["git", "-C", str(repo), *arguments],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except OSError as exc:
        raise ShellPortError(f"cannot execute git for provenance: {exc}") from exc
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise ShellPortError(
            f"git {' '.join(arguments)!r} failed with {completed.returncode}: {detail}"
        )
    return completed.stdout


def read_git_blobs_at_commit(
    repo: Path, *, source_commit: str, source_paths: Mapping[str, str]
) -> Mapping[str, bytes]:
    """Read raw blob objects from an exact commit tree without checkout filters."""
    resolved = _run_git(
        repo,
        "--no-replace-objects",
        "rev-parse",
        "--verify",
        f"{source_commit}^{{commit}}",
    ).decode("ascii", errors="strict").strip()
    if resolved != source_commit:
        raise ShellPortError(
            f"sourceCommit resolves to {resolved!r}, expected exact {source_commit!r}"
        )
    blobs: dict[str, bytes] = {}
    for name, path in source_paths.items():
        candidate = Path(path)
        if candidate.is_absolute() or ".." in candidate.parts or "\\" in path:
            raise ShellPortError(f"invalid repository blob path {path!r}")
        listing = _run_git(
            repo,
            "--no-replace-objects",
            "ls-tree",
            "--full-tree",
            "-z",
            source_commit,
            "--",
            path,
        )
        entries = [entry for entry in listing.split(b"\0") if entry]
        if len(entries) != 1:
            raise ShellPortError(
                f"commit tree path {path!r} resolved to {len(entries)} entries, expected 1"
            )
        try:
            metadata, tree_path = entries[0].split(b"\t", 1)
            _mode, object_type, object_id = metadata.split()
        except ValueError as exc:
            raise ShellPortError(f"malformed ls-tree entry for {path!r}") from exc
        try:
            expected_tree_path = path.encode("utf-8")
        except UnicodeEncodeError as exc:
            raise ShellPortError(f"repository blob path is not UTF-8: {path!r}") from exc
        if tree_path != expected_tree_path:
            raise ShellPortError(
                f"commit tree returned path {tree_path!r}, expected {expected_tree_path!r}"
            )
        if object_type != b"blob" or not re.fullmatch(rb"[0-9a-f]{40}", object_id):
            raise ShellPortError(
                f"commit tree path {path!r} is not an exact SHA-1 blob: "
                f"type={object_type!r}, object={object_id!r}"
            )
        blobs[name] = _run_git(
            repo,
            "--no-replace-objects",
            "cat-file",
            "blob",
            object_id.decode("ascii"),
        )
    return blobs


def read_compile_source_pool(
    repo: Path, source_paths: Sequence[str]
) -> Mapping[str, bytes]:
    """Read every recomputed compile-pool member from the live worktree."""
    resolved_repo = repo.resolve()
    sources: dict[str, bytes] = {}
    for path in source_paths:
        candidate = Path(path)
        if candidate.is_absolute() or ".." in candidate.parts or "\\" in path:
            raise ShellPortError(f"invalid compile source path {path!r}")
        if path in sources:
            raise ShellPortError(f"duplicate compile source path {path!r}")
        resolved = (resolved_repo / candidate).resolve()
        try:
            resolved.relative_to(resolved_repo)
        except ValueError as exc:
            raise ShellPortError(f"compile source path escapes repository: {path!r}") from exc
        sources[path] = resolved.read_bytes()
    return sources


def parse_git_index_flags(raw: bytes) -> bytes:
    """Validate ``git ls-files -v -z`` and retain only forbidden concealment rows."""
    if not raw:
        return b""
    if not raw.endswith(b"\x00"):
        raise ShellPortError("git ls-files -v output has a truncated non-NUL record")

    concealed: list[bytes] = []
    for record in raw[:-1].split(b"\x00"):
        if len(record) < 2:
            raise ShellPortError("git ls-files -v returned a malformed empty record")
        if record[1:2] != b" ":
            raise ShellPortError(
                "git ls-files -v record does not use the exact tag/space separator"
            )
        path = record[2:]
        if not path:
            raise ShellPortError("git ls-files -v record has an empty path")

        tag = record[0:1]
        if tag == b"H":
            continue
        assume_unchanged = b"a" <= tag <= b"z"
        skip_worktree = tag in (b"S", b"s")
        if assume_unchanged or skip_worktree:
            concealed.append(record)
            continue
        raise ShellPortError(
            "git ls-files -v returned a non-clean or unknown tag "
            f"0x{tag.hex()}"
        )
    return b"" if not concealed else b"\x00".join(concealed) + b"\x00"


def read_git_index_concealment(repo: Path) -> bytes:
    """Read and validate tracked-path index flags without stage/mode ambiguity."""
    raw = _run_git(
        repo,
        "--no-replace-objects",
        "ls-files",
        "--cached",
        "-v",
        "-z",
    )
    return parse_git_index_flags(raw)


def read_direct_git_evidence(repo: Path) -> Mapping[str, bytes]:
    """Capture current whole-tree tracked Git state using fixed, filter-free commands."""
    return {
        "gitHead": _run_git(
            repo,
            "--no-replace-objects",
            "rev-parse",
            "--verify",
            "HEAD^{commit}",
        ),
        # Ignore unrelated untracked outputs, but never an unrelated tracked edit.
        # Expected untracked measurement sources still fail raw commit-tree lookup.
        "gitStatus": _run_git(
            repo,
            "--no-replace-objects",
            "status",
            "--short",
            "--untracked-files=no",
        ),
        "gitWorktreeDiff": _run_git(
            repo,
            "--no-replace-objects",
            "diff",
            "--no-ext-diff",
            "--no-textconv",
            "--binary",
            "--",
        ),
        "gitStagedDiff": _run_git(
            repo,
            "--no-replace-objects",
            "diff",
            "--cached",
            "--no-ext-diff",
            "--no-textconv",
            "--binary",
            "--",
        ),
        "gitIndexFlags": read_git_index_concealment(repo),
    }


def verify_captured_git_evidence(
    captured: Mapping[str, bytes], live: Mapping[str, bytes]
) -> None:
    names = set(captured) | set(live)
    mismatches = sorted(name for name in names if captured.get(name) != live.get(name))
    if mismatches:
        raise ShellPortError(
            "captured git evidence differs from direct repository state: "
            + ", ".join(mismatches)
        )


def _table_cells(line: str) -> list[str]:
    return [cell.strip() for cell in line.strip().strip(";").split(";")]


def parse_clock_constraints(text: str) -> tuple[ClockConstraint, ...]:
    lines = text.splitlines()
    header_at = None
    headers: list[str] = []
    required_headers = {"Clock Name", "Type", "Period", "Frequency", "Targets"}
    for index, line in enumerate(lines):
        cells = _table_cells(line) if line.lstrip().startswith(";") else []
        if required_headers.issubset(cells):
            header_at = index
            headers = cells
            break
    if header_at is None:
        raise ShellPortError("genuine TimeQuest report_clocks table is absent")
    positions = {
        name: headers.index(name) for name in ("Clock Name", "Type", "Period", "Targets")
    }
    clocks: list[ClockConstraint] = []
    for line in lines[header_at + 1 :]:
        if not line.lstrip().startswith(";"):
            if clocks and line.strip():
                break
            continue
        cells = _table_cells(line)
        if len(cells) != len(headers):
            continue
        name = cells[positions["Clock Name"]]
        if not name or name == "Clock Name":
            continue
        target_text = cells[positions["Targets"]].strip()
        if not (target_text.startswith("{") and target_text.endswith("}")):
            raise ShellPortError(
                f"clock {name!r} has non-literal Targets cell {target_text!r}"
            )
        targets = tuple(target_text[1:-1].split())
        if not targets or any(any(marker in target for marker in "*?[]") for target in targets):
            raise ShellPortError(
                f"clock {name!r} has invalid or wildcard target set {targets!r}"
            )
        clocks.append(
            ClockConstraint(
                name=name,
                clock_type=cells[positions["Type"]],
                period_ns=Decimal(
                    _first_number(cells[positions["Period"]], decimal=True)
                ),
                targets=targets,
            )
        )
    if not clocks:
        raise ShellPortError("genuine TimeQuest report_clocks table has no clock rows")
    return tuple(clocks)


def parse_timequest_status(text: str) -> str:
    if "TimeQuest Timing Analyzer report for" not in text:
        raise ShellPortError("raw STA report identity is absent")
    version = _quartus_version(_report_field(text, "Quartus Prime Version"))
    if not re.match(r"^17\.0\.2(?:\s|$)", version):
        raise ShellPortError(f"STA Quartus version is not 17.0.2: {version!r}")
    successes = re.findall(
        r"^Info:\s+Quartus Prime TimeQuest Timing Analyzer was successful\.\s+"
        r"0 errors,\s+\d+ warnings\s*$",
        text,
        flags=re.M,
    )
    if len(successes) != 1:
        raise ShellPortError(
            f"expected one genuine TimeQuest success message, found {len(successes)}"
        )
    if re.search(r"^Error(?:\s*\(\d+\))?\s*:", text, flags=re.M):
        raise ShellPortError("raw STA report contains an Error message")
    return "successful"


def validate_clock_constraints(
    clocks: Sequence[ClockConstraint],
    expected: Mapping[str, Decimal] | None = None,
) -> None:
    expected = expected or {
        "gpu_clk": Decimal("10.000"),
        "vid_clk": Decimal("20.000"),
        "audio_clk": Decimal("40.000"),
    }
    names = [clock.name for clock in clocks]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    missing = sorted(set(expected) - set(names))
    extra = sorted(set(names) - set(expected))
    errors: list[str] = []
    if duplicates:
        errors.append(f"duplicate constrained clocks {duplicates}")
    if missing or extra:
        errors.append(f"constrained clock set mismatch: missing={missing}, extra={extra}")
    by_name = {clock.name: clock for clock in clocks}
    for name, period in expected.items():
        clock = by_name.get(name)
        if clock is None:
            continue
        if clock.clock_type != "Base":
            errors.append(
                f"clock {name!r} type is {clock.clock_type!r}, expected 'Base'"
            )
        if clock.period_ns != period:
            errors.append(
                f"clock {name!r} period is {clock.period_ns} ns, expected {period} ns"
            )
        if clock.targets != (name,):
            errors.append(
                f"clock {name!r} targets are {clock.targets!r}, expected {(name,)!r}"
            )
    if errors:
        raise ShellPortError("; ".join(errors))


def parse_map_hierarchy(text: str) -> tuple[MapHierarchyRow, ...]:
    lines = text.splitlines()
    header_at = None
    headers: list[str] = []
    required = {
        "Compilation Hierarchy Node",
        "Combinational ALUTs",
        "Dedicated Logic Registers",
        "Block Memory Bits",
        "DSP Blocks",
        "Pins",
        "Virtual Pins",
        "Full Hierarchy Name",
        "Entity Name",
        "Library Name",
    }
    for index, line in enumerate(lines):
        cells = _table_cells(line) if line.lstrip().startswith(";") else []
        if required.issubset(cells):
            header_at = index
            headers = cells
            break
    if header_at is None:
        raise ShellPortError(
            "Analysis & Synthesis Resource Utilization by Entity table is absent"
        )
    positions = {name: headers.index(name) for name in required}
    rows: list[MapHierarchyRow] = []
    for line in lines[header_at + 1 :]:
        if not line.lstrip().startswith(";"):
            if rows and line.strip():
                break
            continue
        cells = _table_cells(line)
        if len(cells) != len(headers):
            continue
        node = cells[positions["Compilation Hierarchy Node"]]
        if not node or node == "Compilation Hierarchy Node":
            continue
        combinational = _number_pair(cells[positions["Combinational ALUTs"]])
        registers = _number_pair(cells[positions["Dedicated Logic Registers"]])
        rows.append(
            MapHierarchyRow(
                node=node,
                combinational_aluts=combinational[0],
                combinational_aluts_self=combinational[1],
                registers=registers[0],
                registers_self=registers[1],
                memory_bits=int(
                    _first_number(cells[positions["Block Memory Bits"]])
                ),
                dsp_blocks=Decimal(
                    _first_number(cells[positions["DSP Blocks"]], decimal=True)
                ),
                pins=int(_first_number(cells[positions["Pins"]])),
                virtual_pins=int(_first_number(cells[positions["Virtual Pins"]])),
                full_hierarchy_name=cells[positions["Full Hierarchy Name"]],
                entity_name=cells[positions["Entity Name"]],
                library_name=cells[positions["Library Name"]],
            )
        )
    if not rows:
        raise ShellPortError("map hierarchy table has no entity rows")
    return tuple(rows)


def require_exact_map_hierarchy_row(
    rows: Sequence[MapHierarchyRow],
    *,
    top_module: str,
    module: str,
    instance: str | None,
) -> MapHierarchyRow:
    node = f"|{module}" if instance is None else f"|{module}:{instance}|"
    full = f"|{module}" if instance is None else f"|{top_module}|{module}:{instance}"
    matches = [
        row
        for row in rows
        if row.node == node
        and row.full_hierarchy_name == full
        and row.entity_name == module
        and row.library_name == "work"
    ]
    identity = module if instance is None else f"{module}:{instance}"
    if len(matches) != 1:
        raise ShellPortError(
            f"expected one exact map hierarchy row for {identity!r}, found {len(matches)}"
        )
    return matches[0]


def require_shell_map_hierarchy(
    rows: Sequence[MapHierarchyRow],
    *,
    instance: str = "zhao_shell_top:u_shell",
    top_module: str = "zhao_shell_fit_top",
) -> MapHierarchyRow:
    if ":" not in instance:
        raise ShellPortError(f"shell map identity lacks instance name: {instance!r}")
    module, instance_name = instance.rsplit(":", 1)
    row = require_exact_map_hierarchy_row(
        rows,
        top_module=top_module,
        module=module,
        instance=instance_name,
    )
    missing: list[str] = []
    if row.combinational_aluts <= 0:
        missing.append("logic")
    if row.registers <= 0:
        missing.append("registers")
    if row.memory_bits <= 0:
        missing.append("RAM")
    if row.dsp_blocks <= 0:
        missing.append("DSP")
    if missing:
        raise ShellPortError(
            f"map hierarchy row {instance!r} has no nonzero " + ", ".join(missing)
        )
    return row


def validate_map_hierarchy(
    summary: MapSummary, rows: Sequence[MapHierarchyRow]
) -> None:
    top = require_exact_map_hierarchy_row(
        rows,
        top_module=summary.top_entity,
        module=summary.top_entity,
        instance=None,
    )
    errors: list[str] = []
    if top.combinational_aluts != summary.combinational_aluts:
        errors.append(
            "map top combinational ALUT total does not match "
            "Combinational ALUT usage for logic"
        )
    if top.registers != summary.registers:
        errors.append("map top register total does not match Total registers")
    if errors:
        raise ShellPortError("; ".join(errors))


def parse_fitter_hierarchy(text: str) -> tuple[HierarchyRow, ...]:
    lines = text.splitlines()
    header_at = None
    headers: list[str] = []
    for index, line in enumerate(lines):
        cells = _table_cells(line) if line.lstrip().startswith(";") else []
        if "Compilation Hierarchy Node" in cells and "ALMs needed [=A-B+C]" in cells:
            header_at = index
            headers = cells
            break
    if header_at is None:
        raise ShellPortError("Fitter Resource Utilization by Entity table is absent")
    required = {
        "Compilation Hierarchy Node",
        "ALMs needed [=A-B+C]",
        "[A] ALMs used in final placement",
        "[B] Estimate of ALMs recoverable by dense packing",
        "[C] Estimate of ALMs unavailable",
        "ALMs used for memory",
        "Combinational ALUTs",
        "Dedicated Logic Registers",
        "I/O Registers",
        "Block Memory Bits",
        "M10Ks",
        "DSP Blocks",
        "Pins",
        "Virtual Pins",
        "Full Hierarchy Name",
        "Entity Name",
        "Library Name",
    }
    missing_headers = sorted(required - set(headers))
    if missing_headers:
        raise ShellPortError(f"fitter hierarchy headers are missing {missing_headers}")
    positions = {name: headers.index(name) for name in required}
    rows: list[HierarchyRow] = []
    for line in lines[header_at + 1 :]:
        if not line.lstrip().startswith(";"):
            if rows and line.strip():
                break
            continue
        cells = _table_cells(line)
        if len(cells) != len(headers):
            continue
        node = cells[positions["Compilation Hierarchy Node"]]
        if not node or node == "Compilation Hierarchy Node":
            continue
        alms_needed = _number_pair(
            cells[positions["ALMs needed [=A-B+C]"]], decimal=True
        )
        final_placement = _number_pair(
            cells[positions["[A] ALMs used in final placement"]], decimal=True
        )
        dense_recoverable = _number_pair(
            cells[positions["[B] Estimate of ALMs recoverable by dense packing"]],
            decimal=True,
        )
        unavailable = _number_pair(
            cells[positions["[C] Estimate of ALMs unavailable"]], decimal=True
        )
        memory_alms = _number_pair(
            cells[positions["ALMs used for memory"]], decimal=True
        )
        combinational = _number_pair(cells[positions["Combinational ALUTs"]])
        registers = _number_pair(cells[positions["Dedicated Logic Registers"]])
        io_registers = _number_pair(cells[positions["I/O Registers"]])
        rows.append(
            HierarchyRow(
                node=node,
                alms_needed=alms_needed[0],
                alms_needed_self=alms_needed[1],
                final_placement_alms=final_placement[0],
                final_placement_alms_self=final_placement[1],
                dense_recoverable_alms=dense_recoverable[0],
                dense_recoverable_alms_self=dense_recoverable[1],
                unavailable_alms=unavailable[0],
                unavailable_alms_self=unavailable[1],
                memory_alms=memory_alms[0],
                memory_alms_self=memory_alms[1],
                combinational_aluts=combinational[0],
                combinational_aluts_self=combinational[1],
                registers=registers[0],
                registers_self=registers[1],
                io_registers=io_registers[0],
                io_registers_self=io_registers[1],
                memory_bits=int(_first_number(cells[positions["Block Memory Bits"]])),
                ram_blocks=Decimal(_first_number(cells[positions["M10Ks"]], decimal=True)),
                dsp_blocks=Decimal(
                    _first_number(cells[positions["DSP Blocks"]], decimal=True)
                ),
                pins=int(_first_number(cells[positions["Pins"]])),
                virtual_pins=int(_first_number(cells[positions["Virtual Pins"]])),
                full_hierarchy_name=cells[positions["Full Hierarchy Name"]],
                entity_name=cells[positions["Entity Name"]],
                library_name=cells[positions["Library Name"]],
            )
        )
    if not rows:
        raise ShellPortError("fitter hierarchy table has no entity rows")
    return tuple(rows)


def require_exact_hierarchy_row(
    rows: Sequence[HierarchyRow],
    *,
    top_module: str,
    module: str,
    instance: str | None,
) -> HierarchyRow:
    node = f"|{module}" if instance is None else f"|{module}:{instance}"
    full = node if instance is None else f"|{top_module}{node}"
    matches = [
        row
        for row in rows
        if row.node == node
        and row.full_hierarchy_name == full
        and row.entity_name == module
    ]
    identity = module if instance is None else f"{module}:{instance}"
    if len(matches) != 1:
        raise ShellPortError(
            f"expected one exact fitted hierarchy row for {identity!r}, found {len(matches)}"
        )
    return matches[0]


def require_shell_hierarchy(
    rows: Sequence[HierarchyRow],
    *,
    instance: str = "zhao_shell_top:u_shell",
    top_module: str = "zhao_shell_fit_top",
) -> HierarchyRow:
    if ":" not in instance:
        raise ShellPortError(f"shell hierarchy identity lacks instance name: {instance!r}")
    module, instance_name = instance.rsplit(":", 1)
    row = require_exact_hierarchy_row(
        rows,
        top_module=top_module,
        module=module,
        instance=instance_name,
    )
    missing: list[str] = []
    if row.alms_needed <= 0 or row.combinational_aluts <= 0:
        missing.append("logic")
    if row.registers <= 0:
        missing.append("registers")
    if row.memory_bits <= 0 and row.m10ks <= 0:
        missing.append("RAM")
    if row.dsp_blocks <= 0:
        missing.append("DSP")
    if missing:
        raise ShellPortError(
            f"fitted hierarchy row {instance!r} has no nonzero " + ", ".join(missing)
        )
    return row


def parse_receipt(text: str) -> Mapping[str, object]:
    try:
        payload = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ShellPortError(f"shell-fit receipt is not valid JSON: {exc}") from exc
    if not isinstance(payload, dict):
        raise ShellPortError("shell-fit receipt root must be an object")
    # Cleanliness is inspected before schema completeness or any flattering fit
    # field: a dirty receipt cannot describe an exact committed specimen.
    if payload.get("rtlCleanAtHead") is not True:
        raise ShellPortError("receipt rtlCleanAtHead is not true")
    required = {
        "schemaVersion",
        "sourceCommit",
        "rtlCleanAtHead",
        "compileSourcePoolParity",
        "compileSourcePool",
        "selectedSdc",
        "generatedArtifacts",
        "sourceHashes",
        "sourceArtifacts",
        "evidenceArtifacts",
        "tool",
        "device",
        "stages",
        "resources",
        "entities",
        "mapEntities",
        "remainderAttribution",
        "trafficProfile",
        "limitations",
    }
    missing = sorted(required - set(payload))
    if missing:
        raise ShellPortError(f"shell-fit receipt is missing keys {missing}")
    if not isinstance(payload["rtlCleanAtHead"], bool):
        raise ShellPortError("receipt rtlCleanAtHead must be boolean")
    if not isinstance(payload["compileSourcePoolParity"], bool):
        raise ShellPortError("receipt compileSourcePoolParity must be boolean")
    if not isinstance(payload["compileSourcePool"], list) or not all(
        isinstance(item, str) for item in payload["compileSourcePool"]
    ):
        raise ShellPortError("receipt compileSourcePool must be an array of paths")
    if not isinstance(payload["selectedSdc"], str) or not payload["selectedSdc"]:
        raise ShellPortError("receipt selectedSdc must be a non-empty path")
    return payload


def validate_receipt(payload: Mapping[str, object]) -> None:
    # Provenance cleanliness is the first acceptance question. A dirty receipt
    # cannot be rehabilitated by matching stages, resources, or pool metadata.
    if payload.get("rtlCleanAtHead") is not True:
        raise ShellPortError("receipt rtlCleanAtHead is not true")
    errors: list[str] = []
    if payload.get("schemaVersion") != 3:
        errors.append(f"receipt schemaVersion {payload.get('schemaVersion')!r} != 3")
    if payload.get("compileSourcePoolParity") is not True:
        errors.append("receipt compileSourcePoolParity is not true")
    source_commit = payload.get("sourceCommit")
    if not isinstance(source_commit, str) or not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        errors.append(f"receipt sourceCommit is invalid: {source_commit!r}")
    tool = payload.get("tool")
    if not isinstance(tool, dict) or not all(
        isinstance(tool.get(key), str) and tool.get(key) for key in ("name", "version")
    ):
        errors.append("receipt tool must contain non-empty name and version")
    if not isinstance(payload.get("device"), str) or not payload.get("device"):
        errors.append("receipt device must be a non-empty string")
    resources = payload.get("resources")
    resource_keys = (
        "alms",
        "registers",
        "memoryBits",
        "ramBlocks",
        "dspBlocks",
        "mapCombinationalAluts",
        "mapRegisters",
        "realPins",
        "virtualPins",
    )
    if not isinstance(resources, dict):
        errors.append("receipt resources is not an object")
    else:
        missing_resources = [key for key in resource_keys if key not in resources]
        if missing_resources:
            errors.append(f"receipt resources is missing {missing_resources}")
        if resources.get("virtualPins") != 0:
            errors.append(
                f"receipt virtualPins is {resources.get('virtualPins')!r}, expected 0"
            )
        if resources.get("realPins") != 10:
            errors.append(f"receipt realPins is {resources.get('realPins')!r}, expected 10")
    stages = payload.get("stages")
    if not isinstance(stages, dict):
        errors.append("receipt stages is not an object")
    else:
        for stage in ("analysis", "map", "fit", "timequest"):
            if stages.get(stage) != "successful":
                errors.append(f"receipt stage {stage!r} is {stages.get(stage)!r}")
    for key in (
        "generatedArtifacts",
        "sourceHashes",
        "sourceArtifacts",
        "evidenceArtifacts",
        "remainderAttribution",
    ):
        if not isinstance(payload.get(key), dict):
            errors.append(f"receipt {key} is not an object")
    if not isinstance(payload.get("entities"), list) or not payload.get("entities"):
        errors.append("receipt entities must be a non-empty array")
    if not isinstance(payload.get("mapEntities"), list) or not payload.get("mapEntities"):
        errors.append("receipt mapEntities must be a non-empty array")
    if not isinstance(payload.get("limitations"), list):
        errors.append("receipt limitations must be an array")
    if errors:
        raise ShellPortError("; ".join(errors))


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _line_ending_only_difference(left: bytes, right: bytes) -> bool:
    """Classify, but never forgive, a byte mismatch caused only by line endings."""
    if left == right:
        return False

    def lf_bytes(data: bytes) -> bytes:
        return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")

    return lf_bytes(left) == lf_bytes(right)


def _decimal_equal(left: object, right: Decimal) -> bool:
    try:
        return Decimal(str(left)) == right
    except InvalidOperation:
        return False


def _map_hierarchy_evidence(row: MapHierarchyRow) -> dict[str, object]:
    return {
        "fullHierarchyName": row.full_hierarchy_name,
        "entityName": row.entity_name,
        "libraryName": row.library_name,
        "combinationalAluts": row.combinational_aluts,
        "combinationalAlutsSelf": row.combinational_aluts_self,
        "registers": row.registers,
        "registersSelf": row.registers_self,
        "memoryBits": row.memory_bits,
        "dspBlocks": str(row.dsp_blocks),
        "pins": row.pins,
        "virtualPins": row.virtual_pins,
    }


def _hierarchy_evidence(row: HierarchyRow) -> dict[str, object]:
    return {
        "fullHierarchyName": row.full_hierarchy_name,
        "entityName": row.entity_name,
        "libraryName": row.library_name,
        "almsNeeded": str(row.alms_needed),
        "almsNeededSelf": str(row.alms_needed_self),
        "finalPlacementAlms": str(row.final_placement_alms),
        "finalPlacementAlmsSelf": str(row.final_placement_alms_self),
        "denseRecoverableAlms": str(row.dense_recoverable_alms),
        "denseRecoverableAlmsSelf": str(row.dense_recoverable_alms_self),
        "unavailableAlms": str(row.unavailable_alms),
        "unavailableAlmsSelf": str(row.unavailable_alms_self),
        "memoryAlms": str(row.memory_alms),
        "memoryAlmsSelf": str(row.memory_alms_self),
        "combinationalAluts": row.combinational_aluts,
        "combinationalAlutsSelf": row.combinational_aluts_self,
        "registers": row.registers,
        "registersSelf": row.registers_self,
        "ioRegisters": row.io_registers,
        "ioRegistersSelf": row.io_registers_self,
        "memoryBits": row.memory_bits,
        "ramBlocks": str(row.ram_blocks),
        "dspBlocks": str(row.dsp_blocks),
        "pins": row.pins,
        "virtualPins": row.virtual_pins,
    }


def bind_receipt_to_evidence(
    payload: Mapping[str, object],
    *,
    manifest: Mapping[str, object],
    manifest_bytes: bytes,
    rtl_bytes: bytes,
    summary: FitSummary,
    map_summary: MapSummary,
    map_hierarchy_rows: Sequence[MapHierarchyRow],
    timequest_status: str,
    hierarchy_rows: Sequence[HierarchyRow],
    git_evidence: GitEvidence,
    git_blob_bytes: Mapping[str, bytes],
    compile_source_pool: Sequence[str],
    compile_source_bytes: Mapping[str, bytes],
    compile_source_git_blob_bytes: Mapping[str, bytes],
    selected_sdc_path: str,
    source_bytes: Mapping[str, bytes],
    source_paths: Mapping[str, str],
    evidence_bytes: Mapping[str, bytes],
    evidence_paths: Mapping[str, str],
) -> None:
    errors: list[str] = []
    required_sources = (
        "shell",
        "package",
        "policy",
        "generator",
        "parser",
        "packet",
        "cmake",
        "qsf",
        "sdc",
    )
    required_evidence = (
        "summary",
        "sta",
        "clocks",
        "hierarchy",
        "mapSummary",
        "mapReport",
        "gitHead",
        "gitStatus",
        "gitWorktreeDiff",
        "gitStagedDiff",
        "gitIndexFlags",
    )
    missing_sources = [
        name for name in required_sources if name not in source_bytes or name not in source_paths
    ]
    missing_evidence = [
        name
        for name in required_evidence
        if name not in evidence_bytes or name not in evidence_paths
    ]
    if missing_sources or missing_evidence:
        raise ShellPortError(
            f"raw evidence inputs are incomplete: sources={missing_sources}, "
            f"evidence={missing_evidence}"
        )

    if payload.get("sourceCommit") != git_evidence.source_commit:
        errors.append(
            f"sourceCommit mismatch: receipt={payload.get('sourceCommit')!r}, "
            f"git={git_evidence.source_commit!r}"
        )
    if payload.get("rtlCleanAtHead") is not git_evidence.clean:
        errors.append("rtlCleanAtHead does not match direct git repository evidence")
    if payload.get("compileSourcePoolParity") is not True:
        errors.append("compileSourcePoolParity does not match derived CMake/QSF equality")
    if payload.get("compileSourcePool") != list(compile_source_pool):
        errors.append("compileSourcePool does not match the hard-derived ordered QSF pool")
    expected_pool_paths = set(compile_source_pool)
    duplicate_pool_paths = sorted(
        path for path in expected_pool_paths if compile_source_pool.count(path) != 1
    )
    missing_live_pool = sorted(expected_pool_paths - set(compile_source_bytes))
    missing_git_pool = sorted(expected_pool_paths - set(compile_source_git_blob_bytes))
    extra_live_pool = sorted(set(compile_source_bytes) - expected_pool_paths)
    extra_git_pool = sorted(set(compile_source_git_blob_bytes) - expected_pool_paths)
    changed_pool = sorted(
        path
        for path in expected_pool_paths
        if path in compile_source_bytes
        and path in compile_source_git_blob_bytes
        and compile_source_bytes[path] != compile_source_git_blob_bytes[path]
    )
    pool_line_ending_only = [
        path
        for path in changed_pool
        if _line_ending_only_difference(
            compile_source_bytes[path], compile_source_git_blob_bytes[path]
        )
    ]
    if (
        duplicate_pool_paths
        or missing_live_pool
        or missing_git_pool
        or extra_live_pool
        or extra_git_pool
        or changed_pool
    ):
        errors.append(
            "compile source pool specimen bytes differ from Git blobs at sourceCommit: "
            f"duplicates={duplicate_pool_paths}, missing-live={missing_live_pool}, "
            f"missing-git={missing_git_pool}, extra-live={extra_live_pool}, "
            f"extra-git={extra_git_pool}, changed={changed_pool}, "
            f"line-ending-only={pool_line_ending_only} (still rejected)"
        )
    if payload.get("selectedSdc") != selected_sdc_path:
        errors.append("selectedSdc does not match the exact SDC selected by the QSF")
    expected_git_bytes = {
        **source_bytes,
        "generatedRtl": rtl_bytes,
        "manifest": manifest_bytes,
    }
    missing_git_blobs = sorted(set(expected_git_bytes) - set(git_blob_bytes))
    changed_git_blobs = sorted(
        name
        for name, data in expected_git_bytes.items()
        if name in git_blob_bytes and git_blob_bytes[name] != data
    )
    extra_git_blobs = sorted(set(git_blob_bytes) - set(expected_git_bytes))
    git_line_ending_only = [
        name
        for name in changed_git_blobs
        if _line_ending_only_difference(expected_git_bytes[name], git_blob_bytes[name])
    ]
    if missing_git_blobs or changed_git_blobs or extra_git_blobs:
        errors.append(
            "raw specimen bytes differ from Git blobs at sourceCommit: "
            f"missing={missing_git_blobs}, changed={changed_git_blobs}, "
            f"extra={extra_git_blobs}, line-ending-only={git_line_ending_only} "
            "(still rejected)"
        )
    expected_tool = {"name": summary.tool_name, "version": summary.tool_version}
    if payload.get("tool") != expected_tool:
        errors.append(f"tool mismatch: receipt={payload.get('tool')!r}, report={expected_tool!r}")
    if map_summary.tool_version != summary.tool_version:
        errors.append("map and fitter reports were produced by different tool versions")
    if payload.get("device") != summary.device:
        errors.append(
            f"device mismatch: receipt={payload.get('device')!r}, report={summary.device!r}"
        )
    expected_stages = {
        "analysis": "successful" if map_summary.status.lower().startswith("successful") else "failed",
        "map": "successful" if map_summary.status.lower().startswith("successful") else "failed",
        "fit": "successful" if summary.status.lower().startswith("successful") else "failed",
        "timequest": "successful"
        if timequest_status.lower().startswith("successful")
        else "failed",
    }
    if payload.get("stages") != expected_stages:
        errors.append(
            f"stage mismatch: receipt={payload.get('stages')!r}, reports={expected_stages!r}"
        )
    expected_resources = {
        "alms": summary.alms,
        "registers": summary.registers,
        "memoryBits": summary.memory_bits,
        "ramBlocks": int(summary.ram_blocks),
        "dspBlocks": int(summary.dsp_blocks),
        "mapCombinationalAluts": map_summary.combinational_aluts,
        "mapRegisters": map_summary.registers,
        "realPins": summary.real_pins,
        "virtualPins": summary.virtual_pins,
    }
    if payload.get("resources") != expected_resources:
        errors.append(
            f"resource mismatch: receipt={payload.get('resources')!r}, "
            f"reports={expected_resources!r}"
        )
    expected_artifacts = {
        "rtlSha256": _sha256(rtl_bytes),
        "manifestSha256": _sha256(manifest_bytes),
    }
    if payload.get("generatedArtifacts") != expected_artifacts:
        errors.append("generated artifact hashes do not match committed bytes")

    try:
        shell_text = source_bytes["shell"].decode("utf-8")
        package_text = source_bytes["package"].decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"raw shell/package input is not UTF-8: {exc}") from exc
    declaration = parse_module_declaration(
        shell_text,
        "zhao_shell_top",
        type_widths=discover_type_widths(package_text),
        type_signedness=discover_type_signedness(package_text),
    )
    expected_source_hashes = {
        "shellDeclaration": declaration.declaration_sha256,
        "shellFile": _sha256(source_bytes["shell"]),
        "packageFile": _sha256(source_bytes["package"]),
        "policy": _sha256(source_bytes["policy"]),
        "generator": _sha256(source_bytes["generator"]),
        "parser": _sha256(source_bytes["parser"]),
        "packet": _sha256(source_bytes["packet"]),
        "cmake": _sha256(source_bytes["cmake"]),
        "qsf": _sha256(source_bytes["qsf"]),
        "sdc": _sha256(source_bytes["sdc"]),
    }
    if payload.get("sourceHashes") != expected_source_hashes:
        errors.append("receipt sourceHashes do not match independently hashed raw inputs")
    expected_source_artifacts = {
        name: {"path": source_paths[name], "sha256": _sha256(source_bytes[name])}
        for name in required_sources
    }
    if payload.get("sourceArtifacts") != expected_source_artifacts:
        errors.append("receipt source artifact identities/hashes do not match raw inputs")
    expected_evidence_artifacts = {
        name: {"path": evidence_paths[name], "sha256": _sha256(evidence_bytes[name])}
        for name in required_evidence
    }
    if payload.get("evidenceArtifacts") != expected_evidence_artifacts:
        errors.append("receipt evidence artifact identities/hashes do not match raw reports")

    manifest_hashes = manifest.get("hashes")
    if not isinstance(manifest_hashes, dict):
        errors.append("manifest hashes object is absent")
        manifest_hashes = {}
    manifest_expected = {
        "shell_declaration": expected_source_hashes["shellDeclaration"],
        "shell_file": expected_source_hashes["shellFile"],
        "package_file": expected_source_hashes["packageFile"],
        "policy": expected_source_hashes["policy"],
        "generator": expected_source_hashes["generator"],
        "parser": expected_source_hashes["parser"],
        "packet": expected_source_hashes["packet"],
    }
    if manifest_hashes != manifest_expected:
        errors.append("manifest provenance does not match independently hashed raw inputs")
    packet_rom = manifest.get("packet_rom")
    if not isinstance(packet_rom, dict) or packet_rom.get("path") != source_paths["packet"]:
        errors.append("manifest packet path does not match the supplied raw packet identity")
    if payload.get("trafficProfile") != manifest.get("traffic_profile"):
        errors.append(
            f"trafficProfile mismatch: receipt={payload.get('trafficProfile')!r}, "
            f"manifest={manifest.get('traffic_profile')!r}"
        )
    if payload.get("limitations") != manifest.get("limitations"):
        errors.append("receipt limitations do not match manifest limitations")

    hierarchy = manifest.get("hierarchy")
    if not isinstance(hierarchy, dict):
        errors.append("manifest hierarchy object is absent")
        hierarchy = {}
    top_spec = hierarchy.get("top")
    children = hierarchy.get("required_children")
    specs: list[Mapping[str, object]] = []
    if isinstance(top_spec, dict):
        specs.append({**top_spec, "role": "top"})
    else:
        errors.append("manifest hierarchy top is absent")
    if isinstance(children, list) and all(isinstance(item, dict) for item in children):
        specs.extend(children)
    else:
        errors.append("manifest required hierarchy children are absent")
    expected_entities: list[dict[str, object]] = []
    expected_map_entities: list[dict[str, object]] = []
    top_module = str(top_spec.get("module")) if isinstance(top_spec, dict) else ""
    top_row: HierarchyRow | None = None
    for spec in specs:
        module = spec.get("module")
        instance = spec.get("instance")
        role = spec.get("role")
        if not isinstance(module, str) or not isinstance(role, str):
            errors.append(f"invalid manifest hierarchy spec {spec!r}")
            continue
        if instance is not None and not isinstance(instance, str):
            errors.append(f"invalid manifest hierarchy instance {instance!r}")
            continue
        try:
            row = require_exact_hierarchy_row(
                hierarchy_rows,
                top_module=top_module,
                module=module,
                instance=instance,
            )
        except ShellPortError as exc:
            errors.append(str(exc))
            continue
        if role == "top":
            top_row = row
        expected_entities.append(
            {
                "role": role,
                "module": module,
                "instance": instance,
                **_hierarchy_evidence(row),
            }
        )
        try:
            map_row = require_exact_map_hierarchy_row(
                map_hierarchy_rows,
                top_module=top_module,
                module=module,
                instance=instance,
            )
        except ShellPortError as exc:
            errors.append(str(exc))
            continue
        expected_map_entities.append(
            {
                "role": role,
                "module": module,
                "instance": instance,
                **_map_hierarchy_evidence(map_row),
            }
        )
    if payload.get("entities") != expected_entities:
        errors.append("receipt entities do not preserve complete required hierarchy rows")
    if payload.get("mapEntities") != expected_map_entities:
        errors.append("receipt mapEntities do not preserve complete required map rows")
    remainder = payload.get("remainderAttribution")
    expected_remainder = {
        "method": hierarchy.get("remainder_attribution"),
        "reportedTopFullHierarchyName": (
            top_row.full_hierarchy_name if top_row is not None else None
        ),
        "calculatedBySubtraction": False,
    }
    if remainder != expected_remainder:
        errors.append(
            f"remainder attribution mismatch: receipt={remainder!r}, "
            f"expected={expected_remainder!r}"
        )
    if errors:
        raise ShellPortError("; ".join(errors))


def _load_json_object(path: Path, label: str) -> tuple[bytes, Mapping[str, object]]:
    data = path.read_bytes()
    try:
        payload = json.loads(data)
    except json.JSONDecodeError as exc:
        raise ShellPortError(f"{label} is not valid JSON: {exc}") from exc
    if not isinstance(payload, dict):
        raise ShellPortError(f"{label} root must be an object")
    return data, payload


def _repo_identity(path: Path, repo: Path) -> str:
    try:
        return path.resolve().relative_to(repo.resolve()).as_posix()
    except ValueError as exc:
        raise ShellPortError(f"evidence path escapes repository: {path}") from exc


def _utf8(data: bytes, label: str) -> str:
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"{label} is not UTF-8: {exc}") from exc


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--sta", type=Path, required=True)
    parser.add_argument("--clocks", type=Path, required=True)
    parser.add_argument("--hierarchy", type=Path, required=True)
    parser.add_argument("--map-summary", type=Path, required=True)
    parser.add_argument("--map-report", type=Path, required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--rtl", type=Path, required=True)
    parser.add_argument("--shell", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--generator", type=Path, required=True)
    parser.add_argument("--parser", dest="port_parser", type=Path, required=True)
    parser.add_argument("--packet", type=Path, required=True)
    parser.add_argument("--cmake", type=Path, required=True)
    parser.add_argument("--cmake-variable", default="ZHAO_SHELL_RTL")
    parser.add_argument("--qsf", type=Path, required=True)
    parser.add_argument("--sdc", type=Path, required=True)
    parser.add_argument("--git-head", type=Path, required=True)
    parser.add_argument("--git-status", type=Path, required=True)
    parser.add_argument("--git-worktree-diff", type=Path, required=True)
    parser.add_argument("--git-staged-diff", type=Path, required=True)
    parser.add_argument("--git-index-flags", type=Path, required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        repo = args.repo_root.resolve()
        source_path_objects = {
            "shell": args.shell,
            "package": args.package,
            "policy": args.policy,
            "generator": args.generator,
            "parser": args.port_parser,
            "packet": args.packet,
            "cmake": args.cmake,
            "qsf": args.qsf,
            "sdc": args.sdc,
        }
        evidence_path_objects = {
            "summary": args.summary,
            "sta": args.sta,
            "clocks": args.clocks,
            "hierarchy": args.hierarchy,
            "mapSummary": args.map_summary,
            "mapReport": args.map_report,
            "gitHead": args.git_head,
            "gitStatus": args.git_status,
            "gitWorktreeDiff": args.git_worktree_diff,
            "gitStagedDiff": args.git_staged_diff,
            "gitIndexFlags": args.git_index_flags,
        }
        source_bytes = {name: path.read_bytes() for name, path in source_path_objects.items()}
        evidence_bytes = {
            name: path.read_bytes() for name, path in evidence_path_objects.items()
        }
        source_paths = {
            name: _repo_identity(path, repo) for name, path in source_path_objects.items()
        }
        evidence_paths = {
            name: _repo_identity(path, repo) for name, path in evidence_path_objects.items()
        }

        # Provenance is the first substantive verdict: do not interpret flattering
        # fit/resource evidence until the supplied clean capture matches the repo.
        git_evidence = parse_git_evidence(
            head=evidence_bytes["gitHead"],
            status=evidence_bytes["gitStatus"],
            worktree_diff=evidence_bytes["gitWorktreeDiff"],
            staged_diff=evidence_bytes["gitStagedDiff"],
            index_flags=evidence_bytes["gitIndexFlags"],
        )
        verify_captured_git_evidence(
            {
                "gitHead": evidence_bytes["gitHead"],
                "gitStatus": evidence_bytes["gitStatus"],
                "gitWorktreeDiff": evidence_bytes["gitWorktreeDiff"],
                "gitStagedDiff": evidence_bytes["gitStagedDiff"],
                "gitIndexFlags": evidence_bytes["gitIndexFlags"],
            },
            read_direct_git_evidence(repo),
        )

        summary = parse_fit_summary(_utf8(evidence_bytes["summary"], "fit summary"))
        validate_fit_summary(summary)
        sta_text = _utf8(evidence_bytes["sta"], "raw STA report")
        validate_report_messages(sta_text)
        timequest_status = parse_timequest_status(sta_text)
        clocks_text = _utf8(evidence_bytes["clocks"], "report_clocks output")
        validate_clock_constraints(parse_clock_constraints(clocks_text))
        hierarchy_rows = parse_fitter_hierarchy(
            _utf8(evidence_bytes["hierarchy"], "fitter hierarchy report")
        )
        shell = require_shell_hierarchy(hierarchy_rows)
        map_summary_text = _utf8(evidence_bytes["mapSummary"], "map summary")
        map_report_text = _utf8(evidence_bytes["mapReport"], "raw map report")
        map_summary = parse_map_summary(
            map_summary_text, map_report_text=map_report_text
        )
        validate_map_summary(map_summary)
        map_hierarchy_rows = parse_map_hierarchy(map_report_text)
        validate_map_hierarchy(map_summary, map_hierarchy_rows)
        require_shell_map_hierarchy(map_hierarchy_rows)
        scan_virtual_clock_warnings(
            {
                name: _utf8(evidence_bytes[name], f"bound Quartus artifact {name}")
                for name in (
                    "summary",
                    "sta",
                    "clocks",
                    "hierarchy",
                    "mapSummary",
                    "mapReport",
                )
            }
        )
        receipt = parse_receipt(_utf8(args.receipt.read_bytes(), "receipt"))
        validate_receipt(receipt)
        manifest_bytes, manifest = _load_json_object(args.manifest, "shell-fit manifest")
        hierarchy_spec = manifest.get("hierarchy")
        if not isinstance(hierarchy_spec, dict) or not isinstance(
            hierarchy_spec.get("top"), dict
        ):
            raise ShellPortError("manifest top hierarchy identity is absent")
        expected_top = hierarchy_spec["top"].get("module")
        if not isinstance(expected_top, str) or not expected_top:
            raise ShellPortError("manifest top module identity is invalid")
        cmake_pool = parse_cmake_source_pool(
            _utf8(source_bytes["cmake"], "CMake source pool"),
            variable=args.cmake_variable,
            repo=repo,
            cmake_dir=args.cmake.parent,
        )
        qsf_model = parse_qsf(
            _utf8(source_bytes["qsf"], "QSF"), repo=repo, qsf_dir=args.qsf.parent
        )
        compile_source_pool = (*cmake_pool, _repo_identity(args.rtl, repo))
        selected_sdc_path = source_paths["sdc"]
        validate_qsf(
            qsf_model,
            expected_top=expected_top,
            expected_sources=compile_source_pool,
            expected_constraints=(selected_sdc_path,),
        )
        compile_source_bytes = read_compile_source_pool(repo, compile_source_pool)
        compile_source_git_blob_bytes = read_git_blobs_at_commit(
            repo,
            source_commit=git_evidence.source_commit,
            source_paths={path: path for path in compile_source_pool},
        )
        git_paths = {
            **source_paths,
            "generatedRtl": _repo_identity(args.rtl, repo),
            "manifest": _repo_identity(args.manifest, repo),
        }
        git_blob_bytes = read_git_blobs_at_commit(
            repo,
            source_commit=git_evidence.source_commit,
            source_paths=git_paths,
        )
        bind_receipt_to_evidence(
            receipt,
            manifest=manifest,
            manifest_bytes=manifest_bytes,
            rtl_bytes=args.rtl.read_bytes(),
            summary=summary,
            map_summary=map_summary,
            map_hierarchy_rows=map_hierarchy_rows,
            timequest_status=timequest_status,
            hierarchy_rows=hierarchy_rows,
            git_evidence=git_evidence,
            git_blob_bytes=git_blob_bytes,
            compile_source_pool=compile_source_pool,
            compile_source_bytes=compile_source_bytes,
            compile_source_git_blob_bytes=compile_source_git_blob_bytes,
            selected_sdc_path=selected_sdc_path,
            source_bytes=source_bytes,
            source_paths=source_paths,
            evidence_bytes=evidence_bytes,
            evidence_paths=evidence_paths,
        )
    except (OSError, ShellPortError) as exc:
        print(f"shell-fit-reports: {exc}", file=sys.stderr)
        return 1
    print(
        "shell-fit-reports: "
        f"alms={summary.alms} real-pins={summary.real_pins} virtual-pins=0 "
        f"u-shell-alms={shell.alms_needed} clocks=3 map-aluts={map_summary.combinational_aluts} "
        "receipt=raw-bound"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
