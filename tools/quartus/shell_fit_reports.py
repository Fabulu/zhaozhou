#!/usr/bin/env python3
"""Parse and bind shell-fit receipt, fitter, clock, message, and hierarchy evidence.

This module reads committed artifacts and uses read-only Git object/status queries. It
does not invoke Quartus and does not infer wrapper cost by subtracting hierarchy
totals.
"""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Mapping, Sequence
import uuid

from shell_fit_qsf import (
    REQUIRED_QPF_SETTINGS,
    REQUIRED_QSF_SETTINGS,
    QpfModel,
    QsfModel,
    SdcClosureEntry,
    parse_cmake_source_pool,
    parse_qpf,
    parse_qsf,
    read_sdc_closure,
    validate_qsf,
    validate_sdc_closure,
)
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
class TimingAnalysis:
    name: str
    worst_slack_ns: Decimal | None
    failing_endpoint_count: int
    reported_path_count: int
    reported_violated_count: int


@dataclass(frozen=True)
class PostMapPort:
    name: str
    direction: str
    mapped_endpoint_count: int


SOURCE_EVIDENCE_NAMES = (
    "shell",
    "package",
    "policy",
    "generator",
    "parser",
    "packet",
    "cmake",
    "qsf",
    "sdc",
    "qsfParser",
    "evidenceParser",
    "gitCapture",
    "runner",
    "reportScript",
    "postMapScript",
    "project",
)

EVIDENCE_ARTIFACT_NAMES = (
    "summary",
    "sta",
    "clocks",
    "hierarchy",
    "mapSummary",
    "mapReport",
    "timingMetrics",
    "clockTransfers",
    "unconstrainedPaths",
    "setupPaths",
    "holdPaths",
    "recoveryPaths",
    "removalPaths",
    "postMapConnectivity",
    "mapStdout",
    "mapStderr",
    "postMapStdout",
    "postMapStderr",
    "fitStdout",
    "fitStderr",
    "timequestStdout",
    "timequestStderr",
    "gitHead",
    "gitStatus",
    "gitWorktreeDiff",
    "gitStagedDiff",
    "gitIndexFlags",
)

CHARACTERIZATION_LIMITATIONS = (
    "5CSEBA6U23I7 is a provisional capacity/timing target, not frozen board truth.",
    "The ten fitter pins are an unassigned characterization boundary, not a package or board pinout.",
    "No framework top, board I/O delays, PLLs, or physical clocks are included.",
    "gpu_clk and vid_clk remain timing-related; only audio_clk is declared asynchronous.",
    "The result does not characterize a physical SDRAM interface or fabricated hardware.",
)


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
    if summary.device != "5CSEBA6U23I7":
        errors.append(f"fitter device is {summary.device!r}, expected '5CSEBA6U23I7'")
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
    stripped = line.strip()
    if (
        not stripped.startswith(";")
        or not stripped.endswith(";")
        or stripped.startswith(";;")
        or stripped.endswith(";;")
    ):
        raise ShellPortError(
            "Quartus table row must use exactly one leading and trailing ';' delimiter"
        )
    return [cell.strip() for cell in stripped[1:-1].split(";")]


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


def parse_post_map_connectivity(text: str) -> tuple[str, tuple[PostMapPort, ...]]:
    rows = list(csv.reader(io.StringIO(text), delimiter="\t"))
    expected_header = ["record", "name", "direction", "mapped_endpoint_count"]
    if not rows or rows[0] != expected_header:
        raise ShellPortError("post-map connectivity witness header is absent")
    ports: list[PostMapPort] = []
    for line_number, row in enumerate(rows[1:], 2):
        if len(row) != 4:
            raise ShellPortError(
                f"malformed post-map connectivity row {line_number}: {row!r}"
            )
        if row[0] != "port":
            raise ShellPortError(
                f"unknown post-map connectivity record on row {line_number}: {row!r}"
            )
        if not re.fullmatch(r"[0-9]+", row[3]):
            raise ShellPortError(
                f"post-map endpoint count is invalid on row {line_number}: {row!r}"
            )
        ports.append(
            PostMapPort(
                name=row[1],
                direction=row[2],
                mapped_endpoint_count=int(row[3]),
            )
        )
    if not ports:
        raise ShellPortError("post-map connectivity witness has no port rows")
    ports.sort(key=lambda port: port.name)
    return "zhao_shell_fit_top", tuple(ports)


def validate_post_map_connectivity(
    witness: tuple[str, Sequence[PostMapPort]], map_report_text: str
) -> None:
    _top, ports = witness
    expected = (
        ("audio_clk", "input"),
        ("fit_epoch_o[0]", "output"),
        ("fit_epoch_o[1]", "output"),
        ("fit_epoch_o[2]", "output"),
        ("fit_signature_o[0]", "output"),
        ("fit_signature_o[1]", "output"),
        ("fit_signature_o[2]", "output"),
        ("gpu_clk", "input"),
        ("rst_n", "input"),
        ("vid_clk", "input"),
    )
    actual = tuple((port.name, port.direction) for port in ports)
    duplicates = sorted({item for item in actual if actual.count(item) > 1})
    if duplicates or actual != expected:
        raise ShellPortError(
            "post-map wrapper port/bit witness is invalid: "
            f"duplicates={duplicates!r}, actual={actual!r}, expected={expected!r}"
        )
    disconnected = [
        (port.name, port.mapped_endpoint_count)
        for port in ports
        if port.mapped_endpoint_count <= 0
    ]
    if disconnected:
        raise ShellPortError(
            "post-map wrapper ports have no mapped fanin/fanout endpoints: "
            f"{disconnected!r}"
        )
    boundary_counts = re.findall(
        r"^;\s*boundary_port\s*;\s*([0-9][0-9,]*)\s*;\s*$",
        map_report_text,
        flags=re.M,
    )
    if boundary_counts != ["10"]:
        raise ShellPortError(
            f"mapped boundary_port count is {boundary_counts!r}, expected exactly ['10']"
        )


def parse_timing_metrics(
    metrics_text: str, path_reports: Mapping[str, str]
) -> tuple[dict[str, Decimal], tuple[TimingAnalysis, ...]]:
    rows = list(csv.DictReader(io.StringIO(metrics_text), delimiter="\t"))
    if not rows or set(rows[0]) != {"record", "name", "value", "count"}:
        raise ShellPortError("timing_metrics.tsv header or rows are absent")
    clocks: dict[str, Decimal] = {}
    metric_analyses: dict[str, tuple[Decimal | None, int]] = {}
    for row in rows:
        if row["record"] == "clock":
            if row["name"] in clocks:
                raise ShellPortError(f"duplicate timing metric clock {row['name']!r}")
            clocks[row["name"]] = Decimal(row["value"])
        elif row["record"] == "analysis":
            if row["name"] in metric_analyses:
                raise ShellPortError(f"duplicate timing metric analysis {row['name']!r}")
            value = None if row["value"] == "NA" else Decimal(row["value"])
            metric_analyses[row["name"]] = (value, int(row["count"]))
        else:
            raise ShellPortError(f"unknown timing metric record {row!r}")
    expected_clocks = {
        "gpu_clk": Decimal("10.000"),
        "vid_clk": Decimal("20.000"),
        "audio_clk": Decimal("40.000"),
    }
    if clocks != expected_clocks:
        raise ShellPortError(f"timing metric clocks are {clocks!r}, expected {expected_clocks!r}")
    expected_names = ("setup", "hold", "recovery", "removal")
    if tuple(metric_analyses) != expected_names:
        raise ShellPortError(
            f"timing metric analyses are {tuple(metric_analyses)!r}, expected {expected_names!r}"
        )
    analyses: list[TimingAnalysis] = []
    for name in expected_names:
        report = path_reports.get(name)
        if report is None:
            raise ShellPortError(f"raw {name} path report is absent")
        value, failing = metric_analyses[name]
        if "Nothing to report." in report:
            if name in {"setup", "hold"}:
                raise ShellPortError(
                    f"raw {name} path report has no paths for a mapped, clocked shell"
                )
            path_count = 0
            violated = 0
            reported_slack = None
        else:
            match = re.search(
                rf"Report Timing:\s*Found\s+(\d+)\s+{name}\s+paths\s+\((\d+)\s+violated\)\.\s+"
                r"Worst case slack is\s+([-+]?\d+(?:\.\d+)?)",
                report,
                flags=re.I,
            )
            if match is None:
                raise ShellPortError(f"raw {name} path report identity is absent")
            path_count = int(match.group(1))
            violated = int(match.group(2))
            reported_slack = Decimal(match.group(3))
            if name in {"setup", "hold"} and path_count <= 0:
                raise ShellPortError(
                    f"raw {name} path report has no paths for a mapped, clocked shell"
                )
        if reported_slack != value:
            raise ShellPortError(
                f"{name} metric/report worst slack mismatch: {value!r} != {reported_slack!r}"
            )
        if (failing == 0) != (violated == 0):
            raise ShellPortError(
                f"{name} metric/report violation polarity mismatch: {failing} vs {violated}"
            )
        analyses.append(
            TimingAnalysis(
                name=name,
                worst_slack_ns=value,
                failing_endpoint_count=failing,
                reported_path_count=path_count,
                reported_violated_count=violated,
            )
        )
    return clocks, tuple(analyses)


def parse_unconstrained_summary(text: str) -> dict[str, dict[str, int]]:
    expected = (
        "Illegal Clocks",
        "Unconstrained Clocks",
        "Unconstrained Input Ports",
        "Unconstrained Input Port Paths",
        "Unconstrained Output Ports",
        "Unconstrained Output Port Paths",
    )
    result: dict[str, dict[str, int]] = {}
    for name in expected:
        matches = re.findall(
            rf"^;\s*{re.escape(name)}\s*;\s*([0-9][0-9,]*)\s*;\s*([0-9][0-9,]*)\s*;",
            text,
            flags=re.M,
        )
        if len(matches) != 1:
            raise ShellPortError(f"unconstrained-path summary row {name!r} is absent or ambiguous")
        result[name] = {
            "setup": int(matches[0][0].replace(",", "")),
            "hold": int(matches[0][1].replace(",", "")),
        }
    return result


def parse_clock_transfers(text: str) -> list[dict[str, str]]:
    transfers: list[dict[str, str]] = []
    section_name: str | None = None
    for line in text.splitlines():
        if "Setup Transfers" in line:
            section_name = "setup"
            continue
        if "Hold Transfers" in line:
            section_name = "hold"
            continue
        if section_name is None or not line.lstrip().startswith(";"):
            continue
        cells = _table_cells(line)
        if len(cells) != 6 or cells[0] in {"From Clock", ""}:
            continue
        if cells[0].startswith("-"):
            continue
        transfers.append(
            {"analysis": section_name, "from": cells[0], "to": cells[1], "rrPaths": cells[2]}
        )
    for analysis in ("setup", "hold"):
        rows = [row for row in transfers if row["analysis"] == analysis]
        pairs = {(row["from"], row["to"]): row["rrPaths"] for row in rows}
        if len(pairs) != len(rows):
            raise ShellPortError(f"{analysis} clock-transfer matrix contains duplicate pairs")
        required = {
            ("audio_clk", "audio_clk"),
            ("gpu_clk", "gpu_clk"),
            ("vid_clk", "vid_clk"),
            ("gpu_clk", "vid_clk"),
            ("vid_clk", "gpu_clk"),
            ("gpu_clk", "audio_clk"),
            ("audio_clk", "gpu_clk"),
        }
        if set(pairs) != required:
            raise ShellPortError(
                f"{analysis} clock-transfer matrix is incomplete or contains extra pairs"
            )
        for pair in (("audio_clk", "audio_clk"), ("gpu_clk", "gpu_clk"), ("vid_clk", "vid_clk")):
            value = pairs[pair]
            if not re.fullmatch(r"[0-9][0-9,]*", value):
                raise ShellPortError(
                    f"self-clock transfer {pair!r} is not a positive numeric path count: {value!r}"
                )
            if int(value.replace(",", "")) <= 0:
                raise ShellPortError(
                    f"self-clock transfer {pair!r} has no mapped paths: {value!r}"
                )
        for pair in (("gpu_clk", "audio_clk"), ("audio_clk", "gpu_clk")):
            if pairs[pair].lower() != "false path":
                raise ShellPortError(f"audio asynchronous transfer {pair!r} is not a false path")
        for pair in (("gpu_clk", "vid_clk"), ("vid_clk", "gpu_clk")):
            value = pairs[pair]
            if not re.fullmatch(r"[0-9][0-9,]*", value) or int(value.replace(",", "")) <= 0:
                raise ShellPortError(f"GPU/video transfer {pair!r} was cut or disappeared")
    return transfers


def validate_stage_logs(stage_logs: Mapping[str, str]) -> None:
    required = {
        "mapStdout": r"^Info:\s+Quartus Prime Analysis & Synthesis was successful\.\s+0 errors,",
        "postMapStdout": r"^Info:\s+Quartus Prime TimeQuest Timing Analyzer was successful\.\s+0 errors,",
        "fitStdout": r"^Info:\s+Quartus Prime Fitter was successful\.\s+0 errors,",
        "timequestStdout": r"^Info:\s+Quartus Prime TimeQuest Timing Analyzer was successful\.\s+0 errors,",
    }
    errors: list[str] = []
    for artifact, pattern in required.items():
        text = stage_logs.get(artifact)
        if text is None or re.search(pattern, text, flags=re.M) is None:
            errors.append(f"{artifact} has no unique successful zero-error completion")
    post_map_stdout = stage_logs.get("postMapStdout", "")
    if re.search(r"^Info:\s+Using post quartus_map netlist\s*$", post_map_stdout, re.M) is None:
        errors.append("postMapStdout does not prove the post-map timing netlist was loaded")
    for artifact in required:
        stderr_name = artifact.replace("Stdout", "Stderr")
        if stderr_name not in stage_logs:
            errors.append(f"{stderr_name} is absent")
    for artifact, text in stage_logs.items():
        fatal_lines = [
            line.strip()
            for line in text.splitlines()
            if re.match(r"^\s*(?:Error|Fatal)(?:\s*\(\d+\))?\s*:", line, flags=re.I)
        ]
        if fatal_lines:
            errors.append(f"{artifact} contains error diagnostics: {fatal_lines!r}")
    if errors:
        raise ShellPortError("; ".join(errors))


def collect_critical_warnings(stage_logs: Mapping[str, str]) -> list[dict[str, str]]:
    warnings: list[dict[str, str]] = []
    for artifact, text in stage_logs.items():
        for line in text.splitlines():
            if re.match(r"^\s*Critical Warning(?:\s*\(\d+\))?\s*:", line):
                warnings.append({"artifact": artifact, "message": line.strip()})
    return warnings


def timing_evidence(
    *,
    metrics_text: str,
    path_reports: Mapping[str, str],
    unconstrained_text: str,
    transfers_text: str,
    critical_warnings: Sequence[Mapping[str, str]],
) -> dict[str, object]:
    clocks, analyses = parse_timing_metrics(metrics_text, path_reports)
    unconstrained = parse_unconstrained_summary(unconstrained_text)
    transfers = parse_clock_transfers(transfers_text)
    failures: list[str] = []
    for analysis in analyses:
        if analysis.failing_endpoint_count != 0 or (
            analysis.worst_slack_ns is not None and analysis.worst_slack_ns < 0
        ):
            failures.append(
                f"{analysis.name}: failing={analysis.failing_endpoint_count}, "
                f"worstSlackNs={analysis.worst_slack_ns}"
            )
    unconstrained_total = sum(
        counts[polarity]
        for counts in unconstrained.values()
        for polarity in ("setup", "hold")
    )
    if unconstrained_total:
        failures.append(f"unconstrained path summary total={unconstrained_total}")
    if critical_warnings:
        failures.append(f"critical warnings={len(critical_warnings)}")
    return {
        "clocks": {name: str(period) for name, period in clocks.items()},
        "analyses": [
            {
                "name": analysis.name,
                "worstSlackNs": (
                    None if analysis.worst_slack_ns is None else str(analysis.worst_slack_ns)
                ),
                "failingEndpointCount": analysis.failing_endpoint_count,
                "reportedPathCount": analysis.reported_path_count,
                "reportedViolatedCount": analysis.reported_violated_count,
            }
            for analysis in analyses
        ],
        "unconstrained": unconstrained,
        "clockTransfers": transfers,
        "criticalWarnings": list(critical_warnings),
        "timingPassed": not failures,
        "gateFailures": failures,
    }


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
    for line_number, line in enumerate(lines[header_at + 1 :], header_at + 2):
        if line.lstrip().startswith("+"):
            if rows:
                break
            continue
        if not line.strip():
            continue
        if not line.lstrip().startswith(";"):
            raise ShellPortError(
                f"malformed map hierarchy row {line_number}: missing leading ';' delimiter"
            )
        cells = _table_cells(line)
        if cells == headers:
            continue
        if len(cells) != len(headers):
            raise ShellPortError(
                f"malformed map hierarchy row {line_number}: "
                f"found {len(cells)} cells, expected {len(headers)}"
            )
        node = cells[positions["Compilation Hierarchy Node"]]
        if not node:
            raise ShellPortError(f"malformed map hierarchy row {line_number}: empty node")
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
    for line_number, line in enumerate(lines[header_at + 1 :], header_at + 2):
        if line.lstrip().startswith("+"):
            if rows:
                break
            continue
        if not line.strip():
            continue
        if not line.lstrip().startswith(";"):
            raise ShellPortError(
                f"malformed fitter hierarchy row {line_number}: missing leading ';' delimiter"
            )
        cells = _table_cells(line)
        if cells == headers:
            continue
        if len(cells) != len(headers):
            raise ShellPortError(
                f"malformed fitter hierarchy row {line_number}: "
                f"found {len(cells)} cells, expected {len(headers)}"
            )
        node = cells[positions["Compilation Hierarchy Node"]]
        if not node:
            raise ShellPortError(f"malformed fitter hierarchy row {line_number}: empty node")
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
    if instance is None:
        node = f"|{module}"
        full = node
    else:
        node = f"|{module}:{instance}|"
        full = f"|{top_module}|{module}:{instance}"
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


def load_published_receipt_pair(
    synthesis_path: Path, timing_path: Path
) -> Mapping[str, object]:
    """Load one stable, byte-identical content-addressed ledger generation.

    The publisher replaces the two files separately because no filesystem primitive
    can rename two destinations atomically. Readers therefore treat the receipt
    bytes themselves as the generation: an interrupted old/new pair is UNKNOWN,
    never whichever half happened to be opened first.
    """
    if synthesis_path.resolve() == timing_path.resolve():
        raise ShellPortError("shell-fit ledger pair paths must be distinct")
    stable: tuple[bytes, bytes] | None = None
    for _attempt in range(3):
        try:
            synthesis_before = synthesis_path.read_bytes()
            timing_before = timing_path.read_bytes()
            synthesis_after = synthesis_path.read_bytes()
            timing_after = timing_path.read_bytes()
        except OSError as exc:
            raise ShellPortError(f"shell-fit ledger pair is absent or unreadable: {exc}") from exc
        if synthesis_before == synthesis_after and timing_before == timing_after:
            stable = (synthesis_after, timing_after)
            break
    if stable is None:
        raise ShellPortError("shell-fit ledger pair changed while being read")
    synthesis_bytes, timing_bytes = stable
    if synthesis_bytes != timing_bytes:
        raise ShellPortError(
            "shell-fit ledger pair contains mixed content-addressed generations: "
            f"synthesis={_sha256(synthesis_bytes)}, timing={_sha256(timing_bytes)}"
        )
    payload = parse_receipt(_utf8(synthesis_bytes, "published shell-fit ledger pair"))
    validate_receipt(payload)
    if payload.get("evidenceMode") != "production":
        raise ShellPortError("shell-fit ledger pair is explicitly test-only evidence")
    return payload


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
        "characterization",
        "evidenceMode",
        "sourceCommit",
        "rtlCleanAtHead",
        "compileSourcePoolParity",
        "compileSourcePool",
        "selectedSdc",
        "configuration",
        "execution",
        "generatedArtifacts",
        "sourceHashes",
        "sourceArtifacts",
        "evidenceArtifacts",
        "tool",
        "device",
        "stages",
        "resources",
        "boundary",
        "connectivity",
        "timing",
        "gate",
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
    if payload.get("characterization") != "shell_fit_top_clean_characterization":
        errors.append(
            "receipt characterization is not 'shell_fit_top_clean_characterization'"
        )
    if payload.get("evidenceMode") not in {"production", "test-only"}:
        errors.append(
            f"receipt evidenceMode is invalid: {payload.get('evidenceMode')!r}"
        )
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
    if payload.get("device") != "5CSEBA6U23I7":
        errors.append(
            f"receipt device is {payload.get('device')!r}, expected '5CSEBA6U23I7'"
        )
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
        expected_stages = {"map", "postMap", "fit", "timequest"}
        if set(stages) != expected_stages:
            errors.append(
                f"receipt stage set is {sorted(stages)!r}, expected {sorted(expected_stages)!r}"
            )
        for stage in sorted(expected_stages):
            if stages.get(stage) != "successful":
                errors.append(f"receipt stage {stage!r} is {stages.get(stage)!r}")
    for key in (
        "boundary",
        "connectivity",
        "configuration",
        "execution",
        "gate",
        "timing",
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
    configuration = payload.get("configuration")
    if isinstance(configuration, dict):
        if configuration.get("projectRevision") != "zhao_shell_fit":
            errors.append("receipt configuration has the wrong project revision")
        if configuration.get("qpfQuartusVersion") != "17.0":
            errors.append("receipt configuration has the wrong QPF Quartus version")
        qpf_assignments = configuration.get("qpfAssignments")
        expected_qpf_assignments = [
            {"name": name, "value": value} for name, value in REQUIRED_QPF_SETTINGS.items()
        ]
        if qpf_assignments != expected_qpf_assignments:
            errors.append("receipt configuration does not contain the pinned QPF closure")
        if configuration.get("top") != "zhao_shell_fit_top":
            errors.append("receipt configuration has the wrong top")
        settings = configuration.get("effectiveSettings")
        if settings != REQUIRED_QSF_SETTINGS:
            errors.append("receipt configuration does not contain the pinned QSF settings")
        assignments = configuration.get("globalAssignments")
        if not isinstance(assignments, list) or not assignments:
            errors.append("receipt configuration has no bound ordered QSF assignment closure")
        closure = configuration.get("sdcClosure")
        if not isinstance(closure, list) or not closure:
            errors.append("receipt configuration has no bound SDC closure")
    execution = payload.get("execution")
    if isinstance(execution, dict):
        processors = execution.get("processors")
        if not isinstance(processors, int) or isinstance(processors, bool) or processors < 1:
            errors.append("receipt execution processors must be a positive integer")
    connectivity = payload.get("connectivity")
    if isinstance(connectivity, dict):
        if connectivity.get("passed") is not True:
            errors.append("receipt post-map connectivity did not pass")
        if connectivity.get("mappedBoundaryPortCount") != 10:
            errors.append("receipt mapped boundary-port count is not 10")
        ports = connectivity.get("ports")
        if not isinstance(ports, list) or len(ports) != 10:
            errors.append("receipt does not contain ten post-map port-bit witnesses")
    timing = payload.get("timing")
    gate = payload.get("gate")
    if isinstance(timing, dict) and isinstance(gate, dict):
        failures = timing.get("gateFailures")
        passed = timing.get("timingPassed")
        expected_gate_status = "pass" if passed is True and failures == [] else "fail"
        if gate.get("name") != "shell_fit_top_clean_characterization":
            errors.append("receipt gate has the wrong name")
        if gate.get("status") != expected_gate_status:
            errors.append("receipt gate status contradicts timing evidence")
        if gate.get("failures") != failures:
            errors.append("receipt gate failures contradict timing evidence")
    if errors:
        raise ShellPortError("; ".join(errors))


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _canonical_text_bytes(data: bytes, label: str) -> bytes:
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"{label} is not UTF-8: {exc}") from exc
    return text.replace("\r\n", "\n").replace("\r", "\n").encode("utf-8")


def _canonical_text_sha256(data: bytes, label: str) -> str:
    return _sha256(_canonical_text_bytes(data, label))


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
        "node": row.node,
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
        "node": row.node,
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


def _receipt_limitations(manifest: Mapping[str, object]) -> list[str]:
    limitations = manifest.get("limitations")
    if not isinstance(limitations, list) or not all(isinstance(item, str) for item in limitations):
        raise ShellPortError("manifest limitations must be an array of strings")
    traffic = [item for item in limitations if not item.startswith("Packet A has not run Quartus")]
    return [*traffic, *CHARACTERIZATION_LIMITATIONS]


def _boundary_evidence(rtl_bytes: bytes, package_bytes: bytes) -> dict[str, object]:
    try:
        rtl_text = rtl_bytes.decode("utf-8")
        package_text = package_bytes.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"generated top/package input is not UTF-8: {exc}") from exc
    declaration = parse_module_declaration(
        rtl_text,
        "zhao_shell_fit_top",
        type_widths=discover_type_widths(package_text),
        type_signedness=discover_type_signedness(package_text),
    )
    ports = [
        {
            "ordinal": port.ordinal,
            "name": port.name,
            "direction": port.direction,
            "bitWidth": port.bit_width,
        }
        for port in declaration.ports
    ]
    expected = [
        {"ordinal": 0, "name": "gpu_clk", "direction": "input", "bitWidth": 1},
        {"ordinal": 1, "name": "vid_clk", "direction": "input", "bitWidth": 1},
        {"ordinal": 2, "name": "audio_clk", "direction": "input", "bitWidth": 1},
        {"ordinal": 3, "name": "rst_n", "direction": "input", "bitWidth": 1},
        {
            "ordinal": 4,
            "name": "fit_signature_o",
            "direction": "output",
            "bitWidth": 3,
        },
        {"ordinal": 5, "name": "fit_epoch_o", "direction": "output", "bitWidth": 3},
    ]
    if ports != expected or declaration.total_bits != 10:
        raise ShellPortError(
            "generated fit-top boundary is not the exact six-port/ten-bit contract: "
            f"ports={ports!r}, bits={declaration.total_bits}"
        )
    return {
        "module": declaration.module_name,
        "declarationSha256": declaration.declaration_sha256,
        "portCount": len(ports),
        "inputBits": declaration.input_bits,
        "outputBits": declaration.output_bits,
        "totalBits": declaration.total_bits,
        "ports": ports,
    }


def _manifest_hierarchy_specs(
    manifest: Mapping[str, object],
) -> tuple[str, list[Mapping[str, object]], Mapping[str, object]]:
    hierarchy = manifest.get("hierarchy")
    if not isinstance(hierarchy, dict):
        raise ShellPortError("manifest hierarchy object is absent")
    top_spec = hierarchy.get("top")
    children = hierarchy.get("required_children")
    if not isinstance(top_spec, dict):
        raise ShellPortError("manifest hierarchy top is absent")
    if not isinstance(children, list) or not all(isinstance(item, dict) for item in children):
        raise ShellPortError("manifest required hierarchy children are absent")
    top_module = top_spec.get("module")
    if not isinstance(top_module, str) or not top_module:
        raise ShellPortError("manifest top module identity is invalid")
    return top_module, [{**top_spec, "role": "top"}, *children], hierarchy


def _hierarchy_self_evidence(row: HierarchyRow) -> dict[str, object]:
    return {
        "fullHierarchyName": row.full_hierarchy_name,
        "entityName": row.entity_name,
        "almsNeededSelf": str(row.alms_needed_self),
        "finalPlacementAlmsSelf": str(row.final_placement_alms_self),
        "denseRecoverableAlmsSelf": str(row.dense_recoverable_alms_self),
        "unavailableAlmsSelf": str(row.unavailable_alms_self),
        "memoryAlmsSelf": str(row.memory_alms_self),
        "combinationalAlutsSelf": row.combinational_aluts_self,
        "registersSelf": row.registers_self,
        "ioRegistersSelf": row.io_registers_self,
    }


def _map_self_evidence(row: MapHierarchyRow) -> dict[str, object]:
    return {
        "fullHierarchyName": row.full_hierarchy_name,
        "entityName": row.entity_name,
        "combinationalAlutsSelf": row.combinational_aluts_self,
        "registersSelf": row.registers_self,
    }


def _classified_hierarchy(
    manifest: Mapping[str, object],
    hierarchy_rows: Sequence[HierarchyRow],
    map_hierarchy_rows: Sequence[MapHierarchyRow],
) -> tuple[list[dict[str, object]], list[dict[str, object]], HierarchyRow, MapHierarchyRow, Mapping[str, object]]:
    top_module, specs, hierarchy = _manifest_hierarchy_specs(manifest)
    fit_specs: dict[tuple[str, str], Mapping[str, object]] = {}
    map_specs: dict[tuple[str, str], Mapping[str, object]] = {}
    top_row: HierarchyRow | None = None
    top_map_row: MapHierarchyRow | None = None
    for spec in specs:
        module = spec.get("module")
        instance = spec.get("instance")
        role = spec.get("role")
        if not isinstance(module, str) or not isinstance(role, str):
            raise ShellPortError(f"invalid manifest hierarchy spec {spec!r}")
        if instance is not None and not isinstance(instance, str):
            raise ShellPortError(f"invalid manifest hierarchy instance {instance!r}")
        row = require_exact_hierarchy_row(
            hierarchy_rows, top_module=top_module, module=module, instance=instance
        )
        map_row = require_exact_map_hierarchy_row(
            map_hierarchy_rows, top_module=top_module, module=module, instance=instance
        )
        fit_specs[(row.full_hierarchy_name, row.entity_name)] = spec
        map_specs[(map_row.full_hierarchy_name, map_row.entity_name)] = spec
        if role == "top":
            top_row = row
            top_map_row = map_row
    if top_row is None or top_map_row is None:
        raise ShellPortError("manifest hierarchy has no exact top role")

    entities: list[dict[str, object]] = []
    for row in hierarchy_rows:
        spec = fit_specs.get((row.full_hierarchy_name, row.entity_name))
        entities.append(
            {
                "role": spec.get("role") if spec is not None else "unmanifested",
                "module": spec.get("module") if spec is not None else row.entity_name,
                "instance": spec.get("instance") if spec is not None else None,
                **_hierarchy_evidence(row),
            }
        )
    map_entities: list[dict[str, object]] = []
    for row in map_hierarchy_rows:
        spec = map_specs.get((row.full_hierarchy_name, row.entity_name))
        map_entities.append(
            {
                "role": spec.get("role") if spec is not None else "unmanifested",
                "module": spec.get("module") if spec is not None else row.entity_name,
                "instance": spec.get("instance") if spec is not None else None,
                **_map_hierarchy_evidence(row),
            }
        )
    return entities, map_entities, top_row, top_map_row, hierarchy


def _configuration_evidence(
    qpf: QpfModel,
    qsf: QsfModel,
    sdc_closure: Sequence[SdcClosureEntry],
) -> dict[str, object]:
    return {
        "projectRevision": qpf.project_revision,
        "qpfQuartusVersion": qpf.quartus_version,
        "qpfAssignments": [
            {"name": name, "value": value} for name, value in qpf.assignments
        ],
        "top": qsf.top,
        "sources": list(qsf.sources),
        "constraints": list(qsf.constraint_files),
        "effectiveSettings": {
            name: [value for key, value in qsf.global_assignments if key == name][0]
            for name in REQUIRED_QSF_SETTINGS
        },
        "globalAssignments": [
            {"name": name, "value": value} for name, value in qsf.global_assignments
        ],
        "sdcClosure": [
            {"path": entry.path, "sha256": _sha256(entry.data)}
            for entry in sdc_closure
        ],
    }


def _connectivity_evidence(
    witness: tuple[str, Sequence[PostMapPort]],
) -> dict[str, object]:
    top, ports = witness
    return {
        "top": top,
        "mappedBoundaryPortCount": 10,
        "ports": [
            {
                "name": port.name,
                "direction": port.direction,
                "mappedEndpointCount": port.mapped_endpoint_count,
            }
            for port in ports
        ],
        "passed": True,
    }


def _execution_evidence(processors: int) -> dict[str, object]:
    if processors < 1:
        raise ShellPortError(f"execution processor count must be positive, got {processors}")
    return {"processors": processors}


def _gate_evidence(timing: Mapping[str, object]) -> dict[str, object]:
    failures = timing.get("gateFailures")
    if not isinstance(failures, list) or not all(isinstance(item, str) for item in failures):
        raise ShellPortError("derived timing gate failures are malformed")
    passed = timing.get("timingPassed") is True and not failures
    return {
        "name": "shell_fit_top_clean_characterization",
        "status": "pass" if passed else "fail",
        "failures": list(failures),
    }


def build_receipt_from_evidence(
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
    compile_source_pool: Sequence[str],
    selected_sdc_path: str,
    source_bytes: Mapping[str, bytes],
    source_paths: Mapping[str, str],
    evidence_bytes: Mapping[str, bytes],
    evidence_paths: Mapping[str, str],
    qpf_model: QpfModel,
    qsf_model: QsfModel,
    sdc_closure: Sequence[SdcClosureEntry],
    post_map_witness: tuple[str, Sequence[PostMapPort]],
    timing: Mapping[str, object],
    processors: int,
    evidence_mode: str = "production",
) -> dict[str, object]:
    if evidence_mode not in {"production", "test-only"}:
        raise ShellPortError(f"invalid receipt evidence mode {evidence_mode!r}")
    missing_sources = [
        name for name in SOURCE_EVIDENCE_NAMES if name not in source_bytes or name not in source_paths
    ]
    missing_evidence = [
        name
        for name in EVIDENCE_ARTIFACT_NAMES
        if name not in evidence_bytes or name not in evidence_paths
    ]
    if missing_sources or missing_evidence:
        raise ShellPortError(
            f"raw evidence inputs are incomplete: sources={missing_sources}, "
            f"evidence={missing_evidence}"
        )

    try:
        shell_text = source_bytes["shell"].decode("utf-8")
        package_text = source_bytes["package"].decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"raw shell/package input is not UTF-8: {exc}") from exc
    shell_declaration = parse_module_declaration(
        shell_text,
        "zhao_shell_top",
        type_widths=discover_type_widths(package_text),
        type_signedness=discover_type_signedness(package_text),
    )
    source_hashes = {
        "shellDeclaration": shell_declaration.declaration_sha256,
        "shellFile": _sha256(source_bytes["shell"]),
        "packageFile": _sha256(source_bytes["package"]),
        **{
            name: _sha256(source_bytes[name])
            for name in SOURCE_EVIDENCE_NAMES
            if name not in {"shell", "package"}
        },
    }

    entities, map_entities, top_row, top_map_row, hierarchy = _classified_hierarchy(
        manifest, hierarchy_rows, map_hierarchy_rows
    )

    return {
        "schemaVersion": 3,
        "characterization": "shell_fit_top_clean_characterization",
        "evidenceMode": evidence_mode,
        "sourceCommit": git_evidence.source_commit,
        "rtlCleanAtHead": git_evidence.clean,
        "compileSourcePoolParity": True,
        "compileSourcePool": list(compile_source_pool),
        "selectedSdc": selected_sdc_path,
        "configuration": _configuration_evidence(qpf_model, qsf_model, sdc_closure),
        "execution": _execution_evidence(processors),
        "generatedArtifacts": {
            "rtlSha256": _sha256(rtl_bytes),
            "manifestSha256": _sha256(manifest_bytes),
        },
        "sourceHashes": source_hashes,
        "sourceArtifacts": {
            name: {"path": source_paths[name], "sha256": _sha256(source_bytes[name])}
            for name in SOURCE_EVIDENCE_NAMES
        },
        "evidenceArtifacts": {
            name: {"path": evidence_paths[name], "sha256": _sha256(evidence_bytes[name])}
            for name in EVIDENCE_ARTIFACT_NAMES
        },
        "tool": {"name": summary.tool_name, "version": summary.tool_version},
        "device": summary.device,
        "stages": {
            "map": (
                "successful" if map_summary.status.lower().startswith("successful") else "failed"
            ),
            "postMap": "successful",
            "fit": "successful" if summary.status.lower().startswith("successful") else "failed",
            "timequest": (
                "successful" if timequest_status.lower().startswith("successful") else "failed"
            ),
        },
        "resources": {
            "alms": summary.alms,
            "registers": summary.registers,
            "memoryBits": summary.memory_bits,
            "ramBlocks": int(summary.ram_blocks),
            "dspBlocks": int(summary.dsp_blocks),
            "mapCombinationalAluts": map_summary.combinational_aluts,
            "mapRegisters": map_summary.registers,
            "realPins": summary.real_pins,
            "virtualPins": summary.virtual_pins,
        },
        "boundary": _boundary_evidence(rtl_bytes, source_bytes["package"]),
        "connectivity": _connectivity_evidence(post_map_witness),
        "timing": dict(timing),
        "gate": _gate_evidence(timing),
        "entities": entities,
        "mapEntities": map_entities,
        "remainderAttribution": {
            "method": hierarchy.get("remainder_attribution"),
            "reportedTopFullHierarchyName": top_row.full_hierarchy_name,
            "calculatedBySubtraction": False,
            "reportedFitterTopSelf": _hierarchy_self_evidence(top_row),
            "reportedMapTopSelf": _map_self_evidence(top_map_row),
            "unmanifestedFitterRows": sum(
                entity["role"] == "unmanifested" for entity in entities
            ),
            "unmanifestedMapRows": sum(
                entity["role"] == "unmanifested" for entity in map_entities
            ),
        },
        "trafficProfile": manifest.get("traffic_profile"),
        "limitations": _receipt_limitations(manifest),
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
    qpf_model: QpfModel,
    qsf_model: QsfModel,
    sdc_closure: Sequence[SdcClosureEntry],
    post_map_witness: tuple[str, Sequence[PostMapPort]],
    timing: Mapping[str, object],
    processors: int,
    evidence_mode: str = "production",
) -> None:
    if evidence_mode not in {"production", "test-only"}:
        raise ShellPortError(f"invalid receipt evidence mode {evidence_mode!r}")
    errors: list[str] = []
    required_sources = SOURCE_EVIDENCE_NAMES
    required_evidence = EVIDENCE_ARTIFACT_NAMES
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
    if payload.get("characterization") != "shell_fit_top_clean_characterization":
        errors.append("receipt names the wrong characterization gate")
    if payload.get("evidenceMode") != evidence_mode:
        errors.append(
            f"receipt evidenceMode {payload.get('evidenceMode')!r} does not match "
            f"invocation mode {evidence_mode!r}"
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
    expected_configuration = _configuration_evidence(qpf_model, qsf_model, sdc_closure)
    if payload.get("configuration") != expected_configuration:
        errors.append("receipt configuration does not match effective QPF/QSF/SDC closure")
    expected_execution = _execution_evidence(processors)
    if payload.get("execution") != expected_execution:
        errors.append("receipt execution parameters do not match this invocation")
    expected_connectivity = _connectivity_evidence(post_map_witness)
    if payload.get("connectivity") != expected_connectivity:
        errors.append("receipt connectivity does not match the post-map port-bit witness")
    if payload.get("timing") != timing:
        errors.append("receipt timing does not match parsed timing evidence")
    expected_gate = _gate_evidence(timing)
    if payload.get("gate") != expected_gate:
        errors.append("receipt gate verdict does not match parsed timing/policy evidence")
    expected_git_bytes = {
        **source_bytes,
        "generatedRtl": rtl_bytes,
        "manifest": manifest_bytes,
        **{f"sdcClosure:{entry.path}": entry.data for entry in sdc_closure},
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
        "map": "successful" if map_summary.status.lower().startswith("successful") else "failed",
        "postMap": "successful",
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
    shell_canonical = _canonical_text_bytes(source_bytes["shell"], "shell input")
    package_canonical = _canonical_text_bytes(source_bytes["package"], "package input")
    declaration = parse_module_declaration(
        shell_canonical.decode("utf-8"),
        "zhao_shell_top",
        type_widths=discover_type_widths(package_canonical.decode("utf-8")),
        type_signedness=discover_type_signedness(package_canonical.decode("utf-8")),
    )
    expected_source_hashes = {
        "shellDeclaration": parse_module_declaration(
            shell_text,
            "zhao_shell_top",
            type_widths=discover_type_widths(package_text),
            type_signedness=discover_type_signedness(package_text),
        ).declaration_sha256,
        "shellFile": _sha256(source_bytes["shell"]),
        "packageFile": _sha256(source_bytes["package"]),
        **{
            name: _sha256(source_bytes[name])
            for name in required_sources
            if name not in {"shell", "package"}
        },
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
    if manifest.get("source_hash_canonicalization") != "utf8-lf-v1":
        errors.append("manifest does not declare utf8-lf-v1 source hash canonicalization")
    manifest_expected = {
        "shell_declaration": declaration.declaration_sha256,
        "shell_file": _canonical_text_sha256(source_bytes["shell"], "shell input"),
        "package_file": _canonical_text_sha256(source_bytes["package"], "package input"),
        "policy": _canonical_text_sha256(source_bytes["policy"], "policy input"),
        "generator": _canonical_text_sha256(source_bytes["generator"], "generator input"),
        "parser": _canonical_text_sha256(source_bytes["parser"], "parser input"),
        "packet": _sha256(source_bytes["packet"]),
    }
    if manifest_hashes != manifest_expected:
        errors.append(
            "manifest provenance does not match canonical UTF-8/LF text and raw packet inputs"
        )
    packet_rom = manifest.get("packet_rom")
    if not isinstance(packet_rom, dict) or packet_rom.get("path") != source_paths["packet"]:
        errors.append("manifest packet path does not match the supplied raw packet identity")
    if payload.get("trafficProfile") != manifest.get("traffic_profile"):
        errors.append(
            f"trafficProfile mismatch: receipt={payload.get('trafficProfile')!r}, "
            f"manifest={manifest.get('traffic_profile')!r}"
        )
    if payload.get("limitations") != _receipt_limitations(manifest):
        errors.append("receipt limitations do not match bound characterization limitations")

    expected_boundary = _boundary_evidence(rtl_bytes, source_bytes["package"])
    if payload.get("boundary") != expected_boundary:
        errors.append("receipt boundary does not match the exact generated six-port/ten-bit top")

    try:
        (
            expected_entities,
            expected_map_entities,
            top_row,
            top_map_row,
            hierarchy,
        ) = _classified_hierarchy(manifest, hierarchy_rows, map_hierarchy_rows)
    except ShellPortError as exc:
        errors.append(str(exc))
        expected_entities = []
        expected_map_entities = []
        top_row = None
        top_map_row = None
        hierarchy = {}
    if payload.get("entities") != expected_entities:
        errors.append("receipt entities do not preserve every fitted hierarchy row")
    if payload.get("mapEntities") != expected_map_entities:
        errors.append("receipt mapEntities do not preserve every mapped hierarchy row")
    remainder = payload.get("remainderAttribution")
    expected_remainder = {
        "method": hierarchy.get("remainder_attribution"),
        "reportedTopFullHierarchyName": (
            top_row.full_hierarchy_name if top_row is not None else None
        ),
        "calculatedBySubtraction": False,
        "reportedFitterTopSelf": (
            _hierarchy_self_evidence(top_row) if top_row is not None else None
        ),
        "reportedMapTopSelf": (
            _map_self_evidence(top_map_row) if top_map_row is not None else None
        ),
        "unmanifestedFitterRows": sum(
            entity["role"] == "unmanifested" for entity in expected_entities
        ),
        "unmanifestedMapRows": sum(
            entity["role"] == "unmanifested" for entity in expected_map_entities
        ),
    }
    if remainder != expected_remainder:
        errors.append(
            f"remainder attribution mismatch: receipt={remainder!r}, "
            f"expected={expected_remainder!r}"
        )
    if errors:
        raise ShellPortError("; ".join(errors))


def _write_json_atomic(path: Path, payload: Mapping[str, object]) -> None:
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


_FITTER_CP1252_DEGREE_C = re.compile(rb"(?<=\d )\xb0C(?= ;)")


def _fitter_report_text(data: bytes) -> str:
    """Decode fitter output while admitting only Quartus's numeric CP1252 degree-C token."""
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        pass

    parts: list[str] = []
    cursor = 0
    for match in _FITTER_CP1252_DEGREE_C.finditer(data):
        try:
            parts.append(data[cursor : match.start()].decode("utf-8"))
        except UnicodeDecodeError as exc:
            offset = cursor + exc.start
            raise ShellPortError(
                "fitter hierarchy report contains unsupported non-UTF-8 byte "
                f"0x{data[offset]:02x} at offset {offset}"
            ) from exc
        parts.append("°C")
        cursor = match.end()
    try:
        parts.append(data[cursor:].decode("utf-8"))
    except UnicodeDecodeError as exc:
        offset = cursor + exc.start
        raise ShellPortError(
            "fitter hierarchy report contains unsupported non-UTF-8 byte "
            f"0x{data[offset]:02x} at offset {offset}"
        ) from exc
    return "".join(parts)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, fromfile_prefix_chars="@")
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--summary", type=Path, required=True)
    parser.add_argument("--sta", type=Path, required=True)
    parser.add_argument("--clocks", type=Path, required=True)
    parser.add_argument("--hierarchy", type=Path, required=True)
    parser.add_argument("--map-summary", type=Path, required=True)
    parser.add_argument("--map-report", type=Path, required=True)
    receipt_group = parser.add_mutually_exclusive_group(required=True)
    receipt_group.add_argument("--receipt", type=Path)
    receipt_group.add_argument("--emit-receipt", type=Path)
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
    parser.add_argument("--qsf-parser", type=Path, required=True)
    parser.add_argument("--evidence-parser", type=Path, required=True)
    parser.add_argument("--git-capture", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--report-script", type=Path, required=True)
    parser.add_argument("--post-map-script", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    parser.add_argument("--timing-metrics", type=Path, required=True)
    parser.add_argument("--clock-transfers", type=Path, required=True)
    parser.add_argument("--unconstrained-paths", type=Path, required=True)
    parser.add_argument("--setup-paths", type=Path, required=True)
    parser.add_argument("--hold-paths", type=Path, required=True)
    parser.add_argument("--recovery-paths", type=Path, required=True)
    parser.add_argument("--removal-paths", type=Path, required=True)
    parser.add_argument("--post-map-connectivity", type=Path, required=True)
    for stage in ("map", "post-map", "fit", "timequest"):
        parser.add_argument(f"--{stage}-stdout", type=Path, required=True)
        parser.add_argument(f"--{stage}-stderr", type=Path, required=True)
    parser.add_argument("--processors", type=int, default=4)
    parser.add_argument(
        "--evidence-mode", choices=("production", "test-only"), default="production"
    )
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
            "qsfParser": args.qsf_parser,
            "evidenceParser": args.evidence_parser,
            "gitCapture": args.git_capture,
            "runner": args.runner,
            "reportScript": args.report_script,
            "postMapScript": args.post_map_script,
            "project": args.project,
        }
        evidence_path_objects = {
            "summary": args.summary,
            "sta": args.sta,
            "clocks": args.clocks,
            "hierarchy": args.hierarchy,
            "mapSummary": args.map_summary,
            "mapReport": args.map_report,
            "timingMetrics": args.timing_metrics,
            "clockTransfers": args.clock_transfers,
            "unconstrainedPaths": args.unconstrained_paths,
            "setupPaths": args.setup_paths,
            "holdPaths": args.hold_paths,
            "recoveryPaths": args.recovery_paths,
            "removalPaths": args.removal_paths,
            "postMapConnectivity": args.post_map_connectivity,
            "mapStdout": args.map_stdout,
            "mapStderr": args.map_stderr,
            "postMapStdout": args.post_map_stdout,
            "postMapStderr": args.post_map_stderr,
            "fitStdout": args.fit_stdout,
            "fitStderr": args.fit_stderr,
            "timequestStdout": args.timequest_stdout,
            "timequestStderr": args.timequest_stderr,
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
        hierarchy_text = _fitter_report_text(evidence_bytes["hierarchy"])
        hierarchy_rows = parse_fitter_hierarchy(hierarchy_text)
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
        post_map_witness = parse_post_map_connectivity(
            _utf8(evidence_bytes["postMapConnectivity"], "post-map connectivity witness")
        )
        validate_post_map_connectivity(post_map_witness, map_report_text)
        stage_logs = {
            name: _utf8(evidence_bytes[name], f"Quartus stage log {name}")
            for name in (
                "mapStdout",
                "mapStderr",
                "postMapStdout",
                "postMapStderr",
                "fitStdout",
                "fitStderr",
                "timequestStdout",
                "timequestStderr",
            )
        }
        validate_stage_logs(stage_logs)
        critical_warnings = collect_critical_warnings(stage_logs)
        timing = timing_evidence(
            metrics_text=_utf8(evidence_bytes["timingMetrics"], "timing metrics"),
            path_reports={
                name: _utf8(evidence_bytes[f"{name}Paths"], f"{name} path report")
                for name in ("setup", "hold", "recovery", "removal")
            },
            unconstrained_text=_utf8(
                evidence_bytes["unconstrainedPaths"], "unconstrained-path report"
            ),
            transfers_text=_utf8(
                evidence_bytes["clockTransfers"], "clock-transfer report"
            ),
            critical_warnings=critical_warnings,
        )
        scan_virtual_clock_warnings(
            {
                name: (
                    hierarchy_text
                    if name == "hierarchy"
                    else _utf8(evidence_bytes[name], f"bound Quartus artifact {name}")
                )
                for name in (
                    "summary",
                    "sta",
                    "clocks",
                    "hierarchy",
                    "mapSummary",
                    "mapReport",
                    "timingMetrics",
                    "clockTransfers",
                    "unconstrainedPaths",
                    "setupPaths",
                    "holdPaths",
                    "recoveryPaths",
                    "removalPaths",
                    "postMapConnectivity",
                    "mapStdout",
                    "mapStderr",
                    "postMapStdout",
                    "postMapStderr",
                    "fitStdout",
                    "fitStderr",
                    "timequestStdout",
                    "timequestStderr",
                )
            }
        )
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
        qpf_model = parse_qpf(_utf8(source_bytes["project"], "QPF"))
        sdc_closure = read_sdc_closure(args.sdc, repo=repo)
        validate_sdc_closure(sdc_closure)
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
            **{f"sdcClosure:{entry.path}": entry.path for entry in sdc_closure},
        }
        git_blob_bytes = read_git_blobs_at_commit(
            repo,
            source_commit=git_evidence.source_commit,
            source_paths=git_paths,
        )
        if args.emit_receipt is not None:
            _repo_identity(args.emit_receipt, repo)
            receipt = build_receipt_from_evidence(
                manifest=manifest,
                manifest_bytes=manifest_bytes,
                rtl_bytes=args.rtl.read_bytes(),
                summary=summary,
                map_summary=map_summary,
                map_hierarchy_rows=map_hierarchy_rows,
                timequest_status=timequest_status,
                hierarchy_rows=hierarchy_rows,
                git_evidence=git_evidence,
                compile_source_pool=compile_source_pool,
                selected_sdc_path=selected_sdc_path,
                source_bytes=source_bytes,
                source_paths=source_paths,
                evidence_bytes=evidence_bytes,
                evidence_paths=evidence_paths,
                qpf_model=qpf_model,
                qsf_model=qsf_model,
                sdc_closure=sdc_closure,
                post_map_witness=post_map_witness,
                timing=timing,
                processors=args.processors,
                evidence_mode=args.evidence_mode,
            )
        else:
            receipt = parse_receipt(_utf8(args.receipt.read_bytes(), "receipt"))
        validate_receipt(receipt)
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
            qpf_model=qpf_model,
            qsf_model=qsf_model,
            sdc_closure=sdc_closure,
            post_map_witness=post_map_witness,
            timing=timing,
            processors=args.processors,
            evidence_mode=args.evidence_mode,
        )
        if args.emit_receipt is not None:
            _write_json_atomic(args.emit_receipt, receipt)
    except (OSError, ShellPortError) as exc:
        print(f"shell-fit-reports: {exc}", file=sys.stderr)
        return 1
    gate = receipt["gate"]
    gate_passed = isinstance(gate, dict) and gate.get("status") == "pass"
    print(
        "shell-fit-reports: "
        f"gate={'PASS' if gate_passed else 'FAIL'} "
        f"alms={summary.alms} real-pins={summary.real_pins} virtual-pins=0 "
        f"u-shell-alms={shell.alms_needed} clocks=3 map-aluts={map_summary.combinational_aluts} "
        "receipt=raw-bound"
    )
    return 0 if gate_passed else 2


if __name__ == "__main__":
    raise SystemExit(main())
