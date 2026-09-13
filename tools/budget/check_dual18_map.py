#!/usr/bin/env python3
"""Validate the Cyclone V dual-18 MapOnly discriminator reports.

The gate reads the three Quartus-17 resource tables named by the architecture,
then binds them through an orchestration-supplied external invocation anchor and
the top manifest to exact source, QSF, metadata, preparation, raw report, and
.map.summary evidence.  The ordinary Quartus-17 .map.rpt does not expose per-result mapped nets, so a correct one-block resource
shape remains an explicit mapped-route HOLD rather than being upgraded from
source wiring.  The two-primitive variant is a positive control: that mode exits
successfully only after proving that the ordinary one-DSP resource detector
rejects its genuine two-DSP shape.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import re
import stat
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

REPO = Path(__file__).resolve().parents[2]
DEVICE = "5CSEBA6U23I7"
VENDOR_MACRO = "ZHAO_DUAL18_CYCLONEV=1"
TOP_SOURCE = "tests/rtl/dual18_physical_pack_discriminator.sv"
WRAPPER_SOURCE = "fpga/rtl/common/zhao_dual18_mul.sv"
MUTANT_SOURCE = "tests/mutants/dual18_two_primitives_mutant.sv"

RESOURCE_TABLE = "Analysis & Synthesis Resource Usage Summary"
ENTITY_TABLE = "Analysis & Synthesis Resource Utilization by Entity"
DSP_TABLE = "Analysis & Synthesis DSP Block Usage Summary"
SUMMARY_TABLE = "Analysis & Synthesis Summary"
SETTINGS_TABLE = "Analysis & Synthesis Settings"
INDEPENDENT_MODE = "Two Independent 18x18"
DSP_TOTAL = "Total number of DSP blocks"
MAPPED_ROUTE_HOLD = (
    "Quartus-17 Analysis & Synthesis .map.rpt has no per-result mapped-net "
    "routing artifact; distinct mapped resulta/resultb routes remain unproven"
)
INVOCATION_ANCHOR_FILE = "dual18_invocation_anchor.json"

VENDOR_EVIDENCE_SPECS = [
    {
        "kind": "atom-declaration",
        "path": r"C:\intelFPGA_lite\17.0\quartus\eda\sim_lib\cyclonev_atoms.v",
        "requiredPatterns": [
            r"(?m)^module\s+cyclonev_mac\s*\(",
            r'(?m)^parameter\s+operation_mode\s*=\s*"[^"]+"\s*;',
            r"(?m)^parameter\s+signed_max\s*=.*$",
            r"(?m)^parameter\s+signed_may\s*=.*$",
            r"(?m)^parameter\s+signed_mbx\s*=.*$",
            r"(?m)^parameter\s+signed_mby\s*=.*$",
            r"(?m)^output\s*\[result_a_width-1\s*:\s*0\]\s*resulta\s*;",
            r"(?m)^output\s*\[result_b_width-1\s*:\s*0\]\s*resultb\s*;",
        ],
    },
    {
        "kind": "xml-metadata",
        "path": r"C:\intelFPGA_lite\17.0\quartus\libraries\megafunctions\xml_info\cyclonev_mac_info.xml",
        "requiredPatterns": [
            r'(?m)^.*<PARAMETER NAME="OPERATION_MODE"[^\n]*m18x18_full[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="RESULT_A_WIDTH"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="RESULT_B_WIDTH"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MAX"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MAY"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MBX"[^\n]*$',
            r'(?m)^.*<PARAMETER NAME="SIGNED_MBY"[^\n]*$',
            r'(?m)^.*<PORT NAME="ax"[^\n]*$',
            r'(?m)^.*<PORT NAME="ay"[^\n]*$',
            r'(?m)^.*<PORT NAME="bx"[^\n]*$',
            r'(?m)^.*<PORT NAME="by"[^\n]*$',
            r'(?m)^.*<PORT NAME="resulta"[^\n]*$',
            r'(?m)^.*<PORT NAME="resultb"[^\n]*$',
        ],
    },
]

VARIANTS = {
    "inferred": {
        "top": "dual18_inferred_pair",
        "sources": [TOP_SOURCE],
        "macros": [],
    },
    "explicit": {
        "top": "dual18_explicit_pair",
        "sources": [WRAPPER_SOURCE, TOP_SOURCE],
        "macros": [VENDOR_MACRO],
    },
    "s32x18": {
        "top": "dual18_s32x18_exact",
        "sources": [WRAPPER_SOURCE, TOP_SOURCE],
        "macros": [VENDOR_MACRO],
    },
    "two-primitives-mutant": {
        "top": "dual18_two_primitives_mutant",
        "sources": [WRAPPER_SOURCE, MUTANT_SOURCE],
        "macros": [VENDOR_MACRO],
    },
}


class GateError(RuntimeError):
    """A named piece of the discriminator evidence is absent or contradictory."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


_REPARSE_POINT = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x0400)


def lexical_absolute(path: str | Path) -> Path:
    """Return an absolute lexical path without following filesystem indirection."""
    return Path(os.path.abspath(os.fspath(path)))


def canonical_supplied_path(value: str, role: str) -> Path:
    """Require orchestration to name one absolute, normalized lexical path."""
    path = Path(value)
    canonical = lexical_absolute(path)
    if not path.is_absolute() or path != canonical:
        raise GateError("%s is not an absolute canonical lexical path" % role)
    return canonical


def path_components(path: Path) -> list[Path]:
    """List an absolute path root-first, including its final component."""
    absolute = lexical_absolute(path)
    components = [absolute]
    while components[-1].parent != components[-1]:
        components.append(components[-1].parent)
    components.reverse()
    return components


def require_direct_path(path: str | Path, role: str, kind: str) -> Path:
    """Reject symlink, junction, or other reparse indirection in any component.

    Content hashes cannot establish location if an attacker can redirect a
    lexically canonical evidence path into the replaceable candidate tree.  Use
    lstat and Windows' reparse attribute rather than Path.resolve()/is_file(),
    both of which follow the indirection that this trust-boundary check forbids.
    """
    absolute = lexical_absolute(path)
    final_info = None
    for component in path_components(absolute):
        try:
            info = component.lstat()
        except FileNotFoundError as exc:
            if component == absolute:
                raise GateError("%s is missing" % role) from exc
            raise GateError("%s path component is missing: %s" % (role, component)) from exc
        except OSError as exc:
            raise GateError("cannot inspect %s path component %s: %s" %
                            (role, component, exc)) from exc
        is_junction = False
        junction_probe = getattr(component, "is_junction", None)
        if junction_probe is not None:
            try:
                is_junction = bool(junction_probe())
            except OSError as exc:
                raise GateError("cannot inspect %s junction status at %s: %s" %
                                (role, component, exc)) from exc
        attributes = getattr(info, "st_file_attributes", 0)
        if stat.S_ISLNK(info.st_mode) or is_junction or (attributes & _REPARSE_POINT):
            raise GateError(
                "%s uses symlink, junction, or reparse-point indirection at %s"
                % (role, component)
            )
        final_info = info
    assert final_info is not None
    if kind == "file" and not stat.S_ISREG(final_info.st_mode):
        raise GateError("%s is not a regular non-reparse file" % role)
    if kind == "directory" and not stat.S_ISDIR(final_info.st_mode):
        raise GateError("%s is not a direct non-reparse directory" % role)
    return absolute


def semicolon_cells(line: str) -> list[str] | None:
    stripped = line.strip()
    if not (stripped.startswith(";") and stripped.endswith(";")):
        return None
    return [cell.strip() for cell in stripped[1:-1].split(";")]


def extract_table(text: str, title: str) -> list[list[str]]:
    """Return all semicolon rows in one genuine Quartus text table."""
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    starts = []
    for index, line in enumerate(lines):
        cells = semicolon_cells(line)
        if cells and len(cells) == 1 and cells[0] == title:
            starts.append(index)
    if len(starts) != 1:
        raise GateError("expected exactly one %r table, found %d" % (title, len(starts)))

    rows: list[list[str]] = []
    saw_row = False
    for line in lines[starts[0] + 1 :]:
        if not line.strip() and saw_row:
            break
        cells = semicolon_cells(line)
        if cells is not None:
            rows.append(cells)
            saw_row = True
    if not rows:
        raise GateError("%r table has no semicolon rows" % title)
    return rows


def parse_int(cell: str, context: str) -> int:
    match = re.fullmatch(r"\s*([0-9][0-9,]*)(?:\s*\([^)]*\))?\s*", cell)
    if not match:
        raise GateError("%s is not a Quartus integer cell: %r" % (context, cell))
    return int(match.group(1).replace(",", ""))


def unique_labeled_value(rows: Iterable[list[str]], label: str, context: str) -> int:
    matches = [row for row in rows if len(row) >= 2 and row[0] == label]
    if len(matches) != 1:
        raise GateError("%s: expected one %r row, found %d" % (context, label, len(matches)))
    return parse_int(matches[0][1], "%s/%s" % (context, label))


def unique_labeled_text(rows: Iterable[list[str]], label: str, context: str) -> str:
    matches = [row for row in rows if len(row) >= 2 and row[0] == label]
    if len(matches) != 1:
        raise GateError("%s: expected one %r row, found %d" % (context, label, len(matches)))
    return matches[0][1]


def summary_field(text: str, label: str) -> str:
    matches = re.findall(r"(?m)^" + re.escape(label) + r"\s*:\s*([^\r\n]+)\s*$", text)
    if len(matches) != 1:
        raise GateError("map summary expected one %r field, found %d" % (label, len(matches)))
    return matches[0].strip()


def entity_rows(rows: list[list[str]]) -> tuple[list[str], list[dict[str, str]]]:
    headers = [row for row in rows if "Compilation Hierarchy Node" in row and "DSP Blocks" in row]
    if len(headers) != 1:
        raise GateError("entity table needs exactly one recognized header row")
    header = headers[0]
    parsed = []
    for row in rows:
        if row is header or len(row) != len(header):
            continue
        if not row or not row[0] or row[0] == "Compilation Hierarchy Node":
            continue
        parsed.append(dict(zip(header, row)))
    if not parsed:
        raise GateError("entity table contains no entity data rows")
    return header, parsed


def strip_sv_comments_and_attributes(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r"\(\*.*?\*\)", "", text, flags=re.DOTALL)
    return text


def module_body(text: str, module: str) -> str:
    clean = strip_sv_comments_and_attributes(text)
    match = re.search(
        r"\bmodule\s+" + re.escape(module) + r"\b(?P<body>.*?)\bendmodule\b",
        clean,
        flags=re.DOTALL,
    )
    if not match:
        raise GateError("source inspection cannot find module %s" % module)
    return match.group("body")


def expected_qsf_text(spec: dict, absolute_sources: list[str]) -> str:
    lines = [
        "# GENERATED by tools/budget/gen_calib.py -- dual18 MapOnly discriminator.",
        "# Device and tool premise: Quartus Prime Lite 17.0.2 / Cyclone V.",
        'set_global_assignment -name FAMILY "Cyclone V"',
        "set_global_assignment -name DEVICE %s" % DEVICE,
        "set_global_assignment -name TOP_LEVEL_ENTITY %s" % spec["top"],
        "set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files",
        "set_global_assignment -name NUM_PARALLEL_PROCESSORS 4",
        "set_global_assignment -name SEED 1",
        'set_global_assignment -name OPTIMIZATION_MODE "BALANCED"',
    ]
    for source in absolute_sources:
        lines.append('set_global_assignment -name SYSTEMVERILOG_FILE "%s"' % source)
    for macro in spec["macros"]:
        lines.append('set_global_assignment -name VERILOG_MACRO "%s"' % macro)
    lines.append("set_instance_assignment -name VIRTUAL_PIN ON -to *")
    return "\n".join(lines) + "\n"


def check_exact_qsf(qsf: str, spec: dict, absolute_sources: list[str]) -> None:
    """Reject every compile/configuration mechanism not emitted by the generator.

    Exact bytes are intentional here.  Looking only for SYSTEMVERILOG_FILE would
    allow a coherent receipt to hide VERILOG_FILE, VHDL_FILE, QIP_FILE, IP_FILE,
    EDIF_FILE, or another Quartus source mechanism beside the witnessed sources.
    """
    expected = expected_qsf_text(spec, absolute_sources)
    if qsf != expected:
        actual_assignments = re.findall(
            r"(?m)^set_(?:global|instance)_assignment\s+-name\s+(\S+)", qsf
        )
        expected_assignments = re.findall(
            r"(?m)^set_(?:global|instance)_assignment\s+-name\s+(\S+)", expected
        )
        raise GateError(
            "QSF is not the exact generated source/settings set; unaccounted "
            "compile mechanisms are forbidden (expected assignments=%r, got=%r)"
            % (expected_assignments, actual_assignments)
        )


def _matched_vendor_record(spec: dict, path: Path) -> dict:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise GateError("cannot read canonical Quartus-17 %s: %s" % (spec["kind"], exc)) from exc
    search_text = text
    if spec["kind"] == "atom-declaration":
        scope = re.search(
            r"(?ms)^module\s+cyclonev_mac\s*\(.*?^endmodule\s*//cyclonev_mac\s*$",
            text,
        )
        if not scope:
            raise GateError("canonical atom declaration has no complete cyclonev_mac module")
        search_text = scope.group(0)
    excerpts = []
    for pattern in spec["requiredPatterns"]:
        match = re.search(pattern, search_text)
        if not match:
            raise GateError(
                "canonical Quartus-17 %s misses required pattern %r"
                % (spec["kind"], pattern)
            )
        excerpts.append(match.group(0).strip())
    return {
        "kind": spec["kind"],
        "path": spec["path"],
        "available": True,
        "sha256": sha256_file(path),
        "matchedExcerpts": excerpts,
    }


def validate_vendor_evidence(
    config: dict, specs: list[dict] | None = None
) -> tuple[list[str], list[dict]]:
    """Recompute canonical metadata evidence or return an explicit host HOLD."""
    if specs is None:
        specs = VENDOR_EVIDENCE_SPECS
    evidence = config.get("vendorInterfaceEvidence")
    if not isinstance(evidence, list) or len(evidence) != len(specs):
        raise GateError("effective config lacks the canonical Quartus-17 evidence records")

    holds: list[str] = []
    normalized: list[dict] = []
    for row, spec in zip(evidence, specs):
        if not isinstance(row, dict):
            raise GateError("Quartus-17 evidence record is not an object")
        if row.get("kind") != spec["kind"] or row.get("path") != spec["path"]:
            raise GateError(
                "Quartus-17 evidence must use canonical %s path %s"
                % (spec["kind"], spec["path"])
            )
        canonical = Path(spec["path"])
        if canonical.is_file():
            expected = _matched_vendor_record(spec, canonical)
            if row != expected:
                raise GateError(
                    "canonical Quartus-17 %s hash/pattern excerpts differ from the receipt"
                    % spec["kind"]
                )
            normalized.append(expected)
            continue

        if row.get("available") is False:
            expected = {
                "kind": spec["kind"],
                "path": spec["path"],
                "available": False,
                "requiredPatterns": list(spec["requiredPatterns"]),
            }
            if row != expected:
                raise GateError(
                    "unavailable canonical Quartus-17 %s receipt is malformed" % spec["kind"]
                )
            normalized.append(expected)
        elif row.get("available") is True:
            # A receipt prepared on an installed host remains content-bound, but
            # this host cannot independently revalidate it and therefore cannot
            # turn it into a pass.
            if (
                not re.fullmatch(r"[0-9a-f]{64}", str(row.get("sha256", "")))
                or not isinstance(row.get("matchedExcerpts"), list)
                or len(row["matchedExcerpts"]) != len(spec["requiredPatterns"])
                or not all(isinstance(value, str) and value for value in row["matchedExcerpts"])
            ):
                raise GateError(
                    "unrevalidated canonical Quartus-17 %s receipt is malformed" % spec["kind"]
                )
            normalized.append(dict(row))
        else:
            raise GateError("Quartus-17 %s availability is malformed" % spec["kind"])
        holds.append(
            "canonical installed Quartus-17 %s unavailable on parser host" % spec["kind"]
        )
    return holds, normalized


def check_invocation_anchor(config: dict) -> dict:
    """Validate the orchestration-supplied trust root outside candidate evidence.

    The manifest/config/preparation/report tree can move backwards as one.  The
    caller must therefore retain these four values from generator invocation and
    supply them independently; this checker must never discover them from the
    candidate manifest it is judging.  The anchor hashes the final manifest, but
    the manifest deliberately does not hash the anchor (which would be circular).
    """
    supplied = {
        "path": config.get("_invocationAnchorPath"),
        "sha256": config.get("_invocationAnchorSha256"),
        "nonce": config.get("_invocationNonce"),
        "manifestSha256": config.get("_expectedManifestSha256"),
    }
    if not all(isinstance(value, str) and value for value in supplied.values()):
        raise GateError("orchestration invocation anchor arguments are required")
    for field in ("sha256", "nonce", "manifestSha256"):
        if not re.fullmatch(r"[0-9a-f]{64}", supplied[field]):
            raise GateError("orchestration invocation anchor %s is malformed" % field)

    config_path = lexical_absolute(config["_configPath"])
    candidate_tree = config_path.parent.parent
    expected_anchor = candidate_tree.parent / INVOCATION_ANCHOR_FILE
    anchor_path = canonical_supplied_path(
        supplied["path"], "orchestration invocation anchor path"
    )
    if anchor_path != expected_anchor:
        raise GateError(
            "invocation anchor is not at the canonical location outside dual18 "
            "(lexical comparison)"
        )
    require_direct_path(
        anchor_path, "orchestration invocation anchor", "file"
    )
    anchor_raw = anchor_path.read_bytes()
    if hashlib.sha256(anchor_raw).hexdigest() != supplied["sha256"]:
        raise GateError("invocation anchor hash differs from the orchestration value")
    try:
        anchor = json.loads(anchor_raw.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise GateError("invocation anchor is malformed: %s" % exc) from exc
    expected_keys = {
        "schemaVersion",
        "gate",
        "trustBoundary",
        "createdAtLocal",
        "createdAtUnixNs",
        "invocationNonce",
        "evidenceDirectory",
        "manifestPath",
        "manifestSha256",
    }
    if not isinstance(anchor, dict) or set(anchor) != expected_keys:
        raise GateError("invocation anchor has an unexpected schema")
    if (
        anchor.get("schemaVersion") != 1
        or anchor.get("gate") != "dual18_physical_pack_discriminator"
        or anchor.get("trustBoundary") != "orchestration-supplied-outside-candidate-tree"
        or anchor.get("evidenceDirectory") != "dual18"
        or anchor.get("manifestPath") != "dual18/dual18_manifest.json"
    ):
        raise GateError("invocation anchor identity is malformed")
    if anchor.get("invocationNonce") != supplied["nonce"]:
        raise GateError("invocation anchor nonce differs from orchestration")
    if anchor.get("manifestSha256") != supplied["manifestSha256"]:
        raise GateError("invocation anchor manifest hash differs from orchestration")

    manifest_path = lexical_absolute(anchor_path.parent / anchor["manifestPath"])
    expected_manifest = candidate_tree / "dual18_manifest.json"
    if manifest_path != expected_manifest:
        raise GateError("invocation anchor manifest path is not canonical")
    # Validate the lexical trust boundary before reading any candidate artifact.
    # In particular, candidate_tree itself may be a Windows junction even when
    # every resolved comparison above would point at apparently expected bytes.
    require_direct_path(candidate_tree, "dual18 evidence tree", "directory")
    require_direct_path(config_path, "manifest-bound effective config", "file")
    require_direct_path(manifest_path, "invocation-anchored manifest", "file")
    actual_manifest_sha = sha256_file(manifest_path)
    if actual_manifest_sha != anchor["manifestSha256"]:
        raise GateError("candidate manifest hash differs from independent invocation anchor")

    created_ns = anchor.get("createdAtUnixNs")
    if not isinstance(created_ns, int) or created_ns <= 0:
        raise GateError("invocation anchor timestamp is malformed")
    try:
        created_local = datetime.datetime.fromisoformat(anchor["createdAtLocal"])
    except (TypeError, ValueError) as exc:
        raise GateError("invocation anchor local timestamp is malformed") from exc
    local_ns = int(created_local.timestamp() * 1_000_000_000)
    if abs(local_ns - created_ns) > 1_000_000:
        raise GateError("invocation anchor timestamps disagree")
    if created_ns > time.time_ns() + 5_000_000_000:
        raise GateError("invocation anchor timestamp is in the future")
    if anchor_path.stat().st_mtime_ns < created_ns:
        raise GateError("invocation anchor file predates its claimed creation")

    evidence = {
        "path": str(anchor_path),
        "sha256": supplied["sha256"],
        "invocationNonce": supplied["nonce"],
        "createdAtLocal": anchor["createdAtLocal"],
        "createdAtUnixNs": created_ns,
        "manifestPath": str(manifest_path),
        "manifestSha256": actual_manifest_sha,
    }
    config["_invocationAnchorEvidence"] = evidence
    return evidence


def _resolve_manifest_reference(value: object) -> Path:
    if not isinstance(value, str) or not value:
        raise GateError("dual18 manifest contains an invalid path reference")
    # The generator records repo-relative paths (which can contain ../ when a
    # direct test emits into a temporary directory).  Normalize those segments
    # lexically only; resolving here would erase the indirection under review.
    return lexical_absolute(REPO / value)


def check_manifest_binding(config: dict) -> None:
    """Anchor this config/preparation chain in the generated top manifest."""
    config_path = lexical_absolute(config["_configPath"])
    anchor_evidence = config.get("_invocationAnchorEvidence")
    if not isinstance(anchor_evidence, dict):
        raise GateError("manifest validation requires the supplied invocation anchor")
    manifest_path = lexical_absolute(anchor_evidence["manifestPath"])
    require_direct_path(manifest_path, "invocation-anchored manifest", "file")
    try:
        manifest_raw = manifest_path.read_bytes()
        manifest = json.loads(manifest_raw.decode("utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise GateError("cannot read top dual18 manifest %s: %s" % (manifest_path, exc)) from exc
    if hashlib.sha256(manifest_raw).hexdigest() != anchor_evidence["manifestSha256"]:
        raise GateError("candidate manifest changed after invocation-anchor validation")
    if (
        manifest.get("schemaVersion") != 1
        or manifest.get("gate") != "dual18_physical_pack_discriminator"
        or manifest.get("stage") != "map-only"
        or manifest.get("device") != DEVICE
    ):
        raise GateError("top dual18 manifest identity is malformed")
    rows = manifest.get("revisions")
    expected_bases = {spec["top"] for spec in VARIANTS.values()}
    if (
        not isinstance(rows, list)
        or len(rows) != len(expected_bases)
        or {row.get("baseRevision") for row in rows if isinstance(row, dict)} != expected_bases
    ):
        raise GateError("top dual18 manifest does not contain the exact four revisions")

    for row in rows:
        base = row["baseRevision"]
        revision = row.get("revision")
        witness = row.get("contentWitness")
        if (
            row.get("top") != base
            or not isinstance(witness, str)
            or not re.fullmatch(r"[0-9a-f]{64}", witness)
            or revision != "%s_%s" % (base, witness[:16])
        ):
            raise GateError("top dual18 manifest has a stale revision/content witness row")
        revision_dir = manifest_path.parent / base
        expected_paths = {
            "projectFile": revision_dir / (revision + ".qpf"),
            "settingsFile": revision_dir / (revision + ".qsf"),
            "runPreparation": revision_dir / "run_preparation.json",
            "effectiveConfig": revision_dir / "effective_config.json",
        }
        require_direct_path(revision_dir, "manifest-bound revision directory", "directory")
        for field, expected_path in expected_paths.items():
            if _resolve_manifest_reference(row.get(field)) != lexical_absolute(expected_path):
                raise GateError("top dual18 manifest %s path is not canonical" % field)
            require_direct_path(
                expected_path, "manifest-bound %s" % field, "file"
            )
        for path_field, hash_field in (
            ("projectFile", "projectFileSha256"),
            ("settingsFile", "settingsFileSha256"),
            ("runPreparation", "runPreparationSha256"),
            ("effectiveConfig", "effectiveConfigSha256"),
        ):
            artifact = expected_paths[path_field]
            if sha256_file(artifact) != row.get(hash_field):
                raise GateError("top dual18 manifest/%s hash mismatch" % path_field)
        try:
            row_preparation = json.loads(
                expected_paths["runPreparation"].read_text(encoding="utf-8")
            )
        except (OSError, json.JSONDecodeError) as exc:
            raise GateError("manifest-bound run preparation is malformed: %s" % exc) from exc
        prepared_ns = row_preparation.get("preparedAtUnixNs")
        if not isinstance(prepared_ns, int):
            raise GateError("manifest-bound run preparation timestamp is missing")
        if prepared_ns > anchor_evidence["createdAtUnixNs"]:
            raise GateError("invocation anchor is older than candidate preparation")

    matches = [row for row in rows if row["baseRevision"] == config.get("baseRevision")]
    if len(matches) != 1:
        raise GateError("top dual18 manifest has no unique row for this config")
    row = matches[0]
    if config_path != _resolve_manifest_reference(row["effectiveConfig"]):
        raise GateError("effective config path differs from its top manifest row")
    try:
        disk_config = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise GateError("cannot independently reread effective config: %s" % exc) from exc
    supplied_config = {key: value for key, value in config.items() if not key.startswith("_")}
    if supplied_config != disk_config:
        raise GateError("in-memory effective config differs from its manifest-bound file")
    for field in ("revision", "contentWitness", "top"):
        if config.get(field) != row.get(field):
            raise GateError("effective config %s differs from its top manifest row" % field)
    if config.get("qpfFile") != config["revision"] + ".qpf":
        raise GateError("effective QPF filename is not revision-derived")
    if config.get("qsfFile") != config["revision"] + ".qsf":
        raise GateError("effective QSF filename is not revision-derived")
    if config.get("sourceSetSha256") != row.get("sourceSetSha256"):
        raise GateError("effective source-set hash differs from top manifest")
    if config.get("qsfSha256") != row.get("settingsFileSha256"):
        raise GateError("effective QSF hash differs from top manifest")
    preparation_ref = config.get("runPreparation")
    if not isinstance(preparation_ref, dict) or preparation_ref.get("file") != "run_preparation.json":
        raise GateError("effective preparation filename is not canonical")
    if preparation_ref.get("sha256") != row.get("runPreparationSha256"):
        raise GateError("effective preparation hash differs from top manifest")
    config["_manifestEvidence"] = {
        "path": str(manifest_path),
        "sha256": hashlib.sha256(manifest_raw).hexdigest(),
        "effectiveConfigSha256": row["effectiveConfigSha256"],
    }


def inspect_effective_sources(config: dict, variant: str) -> list[str]:
    spec = VARIANTS[variant]
    holds: list[str] = []
    source_rows = config.get("sources")
    if not isinstance(source_rows, list):
        raise GateError("effective config has no source list")
    paths = [row.get("path") for row in source_rows]
    if paths != spec["sources"]:
        raise GateError("effective source list mismatch: expected %r, got %r" %
                        (spec["sources"], paths))
    if config.get("macros") != spec["macros"]:
        raise GateError("effective macro set mismatch: expected %r, got %r" %
                        (spec["macros"], config.get("macros")))
    if config.get("top") != spec["top"]:
        raise GateError("effective top mismatch: expected %s, got %r" %
                        (spec["top"], config.get("top")))
    if config.get("baseRevision") != spec["top"]:
        raise GateError("effective base revision mismatch: expected %s, got %r" %
                        (spec["top"], config.get("baseRevision")))
    if config.get("device") != DEVICE:
        raise GateError("effective device is not %s" % DEVICE)
    if config.get("stage") != "map-only":
        raise GateError("effective config is not stamped map-only")
    check_invocation_anchor(config)
    check_manifest_binding(config)
    vendor_holds, normalized_vendor_evidence = validate_vendor_evidence(config)
    holds.extend(vendor_holds)

    source_set = hashlib.sha256()
    source_text: dict[str, str] = {}
    absolute_sources: list[str] = []
    witnessed_sources = []
    for row in source_rows:
        path = lexical_absolute(row.get("absolutePath", ""))
        expected_source = lexical_absolute(REPO / row["path"])
        if path != expected_source:
            raise GateError("effective source is not at its canonical lexical path: %s" % row["path"])
        require_direct_path(path, "effective source %s" % row["path"], "file")
        actual_hash = sha256_file(path)
        if row.get("sha256") != actual_hash:
            raise GateError("source hash mismatch for %s" % row["path"])
        source_set.update(row["path"].encode("utf-8"))
        source_set.update(b"\0")
        source_set.update(actual_hash.encode("ascii"))
        source_set.update(b"\n")
        source_text[row["path"]] = path.read_text(encoding="utf-8")
        absolute_sources.append(path.as_posix())
        witnessed_sources.append({"path": row["path"], "sha256": actual_hash})
    source_set_sha256 = source_set.hexdigest()
    if config.get("sourceSetSha256") != source_set_sha256:
        raise GateError("effective source-set digest is stale")

    config_path = lexical_absolute(config["_configPath"])
    qsf_name = config.get("qsfFile")
    if not isinstance(qsf_name, str) or Path(qsf_name).name != qsf_name:
        raise GateError("effective config has no safe QSF filename")
    qsf_path = config_path.parent / qsf_name
    require_direct_path(qsf_path, "effective QSF", "file")
    if sha256_file(qsf_path) != config.get("qsfSha256"):
        raise GateError("QSF is missing or differs from its effective receipt")
    qsf = qsf_path.read_text(encoding="ascii")
    check_exact_qsf(qsf, spec, absolute_sources)

    witness_inputs = {
        "schemaVersion": 1,
        "gate": "dual18_physical_pack_discriminator",
        "tool": {"name": "Quartus Prime Lite", "version": "17.0.2", "build": 602},
        "device": DEVICE,
        "baseRevision": spec["top"],
        "top": spec["top"],
        "sources": witnessed_sources,
        "macros": list(spec["macros"]),
        "sourceSetSha256": source_set_sha256,
        "qsfSha256": sha256_file(qsf_path),
        "vendorInterfaceEvidence": normalized_vendor_evidence,
    }
    if config.get("witnessInputs") != witness_inputs:
        raise GateError("content-witness inputs differ from the effective source/QSF configuration")
    witness_bytes = json.dumps(
        witness_inputs, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("ascii")
    content_witness = hashlib.sha256(witness_bytes).hexdigest()
    if config.get("contentWitness") != content_witness:
        raise GateError("effective content witness is stale")
    expected_revision = "%s_%s" % (spec["top"], content_witness[:16])
    if config.get("revision") != expected_revision:
        raise GateError("content-addressed revision mismatch: expected %s, got %r" %
                        (expected_revision, config.get("revision")))

    qpf_name = config.get("qpfFile")
    if not isinstance(qpf_name, str) or Path(qpf_name).name != qpf_name:
        raise GateError("effective config has no safe QPF filename")
    qpf_path = config_path.parent / qpf_name
    expected_qpf = 'QUARTUS_VERSION = "17.0"\nPROJECT_REVISION = "%s"\n' % expected_revision
    require_direct_path(qpf_path, "effective QPF", "file")
    if qpf_path.read_text(encoding="ascii") != expected_qpf:
        raise GateError("QPF does not carry the exact content-addressed revision")

    if variant == "inferred":
        top_body = module_body(source_text[TOP_SOURCE], spec["top"])
        if re.search(r"\bzhao_dual18_mul\b", top_body):
            raise GateError("inferred contrast unexpectedly instantiates the explicit wrapper")
        return holds

    wrapper = source_text[WRAPPER_SOURCE]
    vendor_match = re.search(
        r"`ifdef\s+ZHAO_DUAL18_CYCLONEV(?P<body>.*?)"
        r"`elsif\s+ZHAO_DUAL18_BEHAVIORAL",
        wrapper,
        flags=re.DOTALL,
    )
    if not vendor_match:
        raise GateError("wrapper has no preprocessor-exclusive Cyclone V branch")
    vendor_body = strip_sv_comments_and_attributes(vendor_match.group("body"))
    atoms = re.findall(r"\bcyclonev_mac\s*#\s*\(", vendor_body)
    if len(atoms) != 1:
        raise GateError("wrapper vendor branch must contain exactly one cyclonev_mac")
    if not re.search(r"\.resulta\s*\(\s*resulta_o\s*\)", vendor_body):
        raise GateError("cyclonev_mac resulta does not reach the distinct lane-A output")
    if not re.search(r"\.resultb\s*\(\s*resultb_o\s*\)", vendor_body):
        raise GateError("cyclonev_mac resultb does not reach the distinct lane-B output")
    required_atom_settings = (
        r"\.ax_width\s*\(\s*18\s*\)",
        r"\.ay_scan_in_width\s*\(\s*18\s*\)",
        r"\.bx_width\s*\(\s*18\s*\)",
        r"\.by_width\s*\(\s*18\s*\)",
        r"\.result_a_width\s*\(\s*36\s*\)",
        r"\.result_b_width\s*\(\s*36\s*\)",
        r'\.operation_mode\s*\(\s*"m18x18_full"\s*\)',
        r'\.operand_source_max\s*\(\s*"input"\s*\)',
        r'\.operand_source_may\s*\(\s*"input"\s*\)',
        r'\.operand_source_mbx\s*\(\s*"input"\s*\)',
        r'\.operand_source_mby\s*\(\s*"input"\s*\)',
        r'\.use_chainadder\s*\(\s*"false"\s*\)',
        r'\.enable_double_accum\s*\(\s*"false"\s*\)',
        r'\.ax_clock\s*\(\s*"none"\s*\)',
        r'\.ay_scan_in_clock\s*\(\s*"none"\s*\)',
        r'\.bx_clock\s*\(\s*"none"\s*\)',
        r'\.by_clock\s*\(\s*"none"\s*\)',
        r'\.output_clock\s*\(\s*"none"\s*\)',
        r"\.clk\s*\(\s*3'b000\s*\)",
        r"\.ena\s*\(\s*3'b111\s*\)",
        r"\.aclr\s*\(\s*2'b00\s*\)",
    )
    missing_settings = [pattern for pattern in required_atom_settings
                        if not re.search(pattern, vendor_body)]
    if missing_settings:
        raise GateError("cyclonev_mac candidate settings are incomplete: %r" % missing_settings)
    vendor_arithmetic = re.sub(r"<<<|>>>|<<|>>", "", vendor_body)
    if "*" in vendor_arithmetic:
        raise GateError("Cyclone V backend contains arithmetic outside cyclonev_mac")

    top_body = module_body(source_text[spec["sources"][-1]], spec["top"])
    wrapper_instances = len(re.findall(r"\bzhao_dual18_mul\s*#\s*\(", top_body))
    expected_instances = 2 if variant == "two-primitives-mutant" else 1
    if wrapper_instances != expected_instances:
        raise GateError("%s source has %d wrapper instances, expected %d" %
                        (spec["top"], wrapper_instances, expected_instances))
    if variant in ("explicit", "s32x18", "two-primitives-mutant"):
        arithmetic_free = re.sub(r"<<<|>>>|<<|>>", "", top_body)
        if "*" in arithmetic_free:
            raise GateError("explicit calibration top contains a helper multiplication")

    if variant == "explicit":
        required_routes = (
            r"\.resulta_o\s*\(\s*prod_a_c\s*\)",
            r"\.resultb_o\s*\(\s*prod_b_c\s*\)",
            r"\bresulta_o\s*<=\s*prod_a_c\s*;",
            r"\bresultb_o\s*<=\s*prod_b_c\s*;",
        )
        if not all(re.search(pattern, top_body) for pattern in required_routes):
            raise GateError("explicit source does not preserve two distinct live result routes")
    elif variant == "s32x18":
        required_routes = (
            r"\.resulta_o\s*\(\s*p_lo_c\s*\)",
            r"\.resultb_o\s*\(\s*p_hi_c\s*\)",
            r"\bproduct_c\s*=\s*p_lo_ext_c\s*\+\s*\(\s*p_hi_ext_c\s*<<<\s*16\s*\)\s*;",
        )
        if not all(re.search(pattern, top_body) for pattern in required_routes):
            raise GateError("s32x18 source does not keep both partial results live in recombination")

    return holds


def check_report_provenance(text: str, config: dict, resource_dsp: int) -> None:
    """Bind raw reports to this content witness and this freshly prepared run."""
    summary = extract_table(text, SUMMARY_TABLE)
    settings = extract_table(text, SETTINGS_TABLE)
    status = unique_labeled_text(summary, "Analysis & Synthesis Status", SUMMARY_TABLE)
    version = unique_labeled_text(summary, "Quartus Prime Version", SUMMARY_TABLE)
    revision = unique_labeled_text(summary, "Revision Name", SUMMARY_TABLE)
    top = unique_labeled_text(summary, "Top-level Entity Name", SUMMARY_TABLE)
    family = unique_labeled_text(summary, "Family", SUMMARY_TABLE)
    summary_dsp = unique_labeled_value(summary, "Total DSP Blocks", SUMMARY_TABLE)
    device = unique_labeled_text(settings, "Device", SETTINGS_TABLE)
    settings_top = unique_labeled_text(settings, "Top-level entity name", SETTINGS_TABLE)

    if not status.startswith("Successful"):
        raise GateError("Analysis & Synthesis status is not Successful: %r" % status)
    if "17.0.2 Build 602" not in version or "Lite Edition" not in version:
        raise GateError("map report is not from Quartus Prime Lite 17.0.2 Build 602")
    if revision != config["revision"]:
        raise GateError("report revision %r differs from effective revision %r" %
                        (revision, config["revision"]))
    if top != config["top"] or settings_top != config["top"]:
        raise GateError("report top does not match effective top %s" % config["top"])
    if family != "Cyclone V":
        raise GateError("report family is not Cyclone V")
    if device != DEVICE:
        raise GateError("report device is not %s" % DEVICE)
    if summary_dsp != resource_dsp:
        raise GateError("summary DSP total disagrees with resource table")

    config_path = lexical_absolute(config["_configPath"])
    preparation_ref = config.get("runPreparation")
    if not isinstance(preparation_ref, dict):
        raise GateError("effective config has no run-preparation receipt")
    preparation_name = preparation_ref.get("file")
    if not isinstance(preparation_name, str) or Path(preparation_name).name != preparation_name:
        raise GateError("run-preparation receipt has no safe filename")
    preparation_path = config_path.parent / preparation_name
    require_direct_path(preparation_path, "run-preparation receipt", "file")
    if sha256_file(preparation_path) != preparation_ref.get("sha256"):
        raise GateError("run-preparation receipt is missing or stale")
    try:
        preparation = json.loads(preparation_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise GateError("run-preparation receipt is malformed: %s" % exc) from exc
    if preparation.get("outputDirectoryWasEmpty") is not True:
        raise GateError("MapOnly output directory was not witnessed empty at preparation")
    if preparation.get("outputDirectory") != "output_files":
        raise GateError("run-preparation output directory is not isolated")
    for field in ("baseRevision", "revision", "contentWitness"):
        if preparation.get(field) != config.get(field):
            raise GateError("run-preparation %s differs from effective config" % field)
    expected_report_rel = "output_files/%s.map.rpt" % config["revision"]
    expected_summary_rel = "output_files/%s.map.summary" % config["revision"]
    if preparation.get("expectedMapReport") != expected_report_rel:
        raise GateError("run-preparation map-report path is not revision-derived")
    if preparation.get("expectedMapSummary") != expected_summary_rel:
        raise GateError("run-preparation map-summary path is not revision-derived")

    report_path_raw = config.get("_reportPath")
    report_sha = config.get("_reportSha256")
    if not isinstance(report_path_raw, str) or not isinstance(report_sha, str):
        raise GateError("raw map report path/hash was not supplied to the gate")
    report_path = canonical_supplied_path(report_path_raw, "raw map report path")
    expected_report = lexical_absolute(
        config_path.parent / preparation.get("expectedMapReport", "")
    )
    if report_path != expected_report:
        raise GateError("map report is not the freshly prepared revision's expected lexical output")
    require_direct_path(report_path, "raw map report", "file")
    if sha256_file(report_path) != report_sha:
        raise GateError("raw map report changed after it was read")

    expected_summary = lexical_absolute(
        config_path.parent / preparation.get("expectedMapSummary", "")
    )
    require_direct_path(expected_summary, "fresh Quartus .map.summary companion", "file")
    prepared_unix_ns = preparation.get("preparedAtUnixNs")
    if not isinstance(prepared_unix_ns, int):
        raise GateError("run-preparation timestamp is missing")
    if report_path.stat().st_mtime_ns < prepared_unix_ns:
        raise GateError("map report predates the fresh empty-output preparation")
    if expected_summary.stat().st_mtime_ns < prepared_unix_ns:
        raise GateError("map summary predates the fresh empty-output preparation")
    anchor_evidence = config.get("_invocationAnchorEvidence")
    if not isinstance(anchor_evidence, dict):
        raise GateError("run provenance has no independently supplied invocation anchor")
    anchor_unix_ns = anchor_evidence["createdAtUnixNs"]
    if report_path.stat().st_mtime_ns < anchor_unix_ns:
        raise GateError("map report predates the invocation anchor")
    if expected_summary.stat().st_mtime_ns < anchor_unix_ns:
        raise GateError("map summary predates the invocation anchor")

    try:
        prepared_local = datetime.datetime.fromisoformat(preparation["preparedAtLocal"])
    except (KeyError, TypeError, ValueError) as exc:
        raise GateError("run-preparation local timestamp is malformed") from exc
    status_match = re.fullmatch(r"Successful\s*-\s*(.+)", status)
    if not status_match:
        raise GateError("successful report has no genuine Quartus completion timestamp")
    try:
        completed_local = datetime.datetime.strptime(
            status_match.group(1), "%a %b %d %H:%M:%S %Y"
        ).replace(tzinfo=prepared_local.tzinfo)
    except ValueError as exc:
        raise GateError("Quartus completion timestamp is malformed") from exc
    if completed_local < prepared_local.replace(microsecond=0):
        raise GateError("map report completion predates this prepared run")
    try:
        anchor_local = datetime.datetime.fromisoformat(anchor_evidence["createdAtLocal"])
    except (KeyError, TypeError, ValueError) as exc:
        raise GateError("invocation anchor local timestamp is malformed") from exc
    if completed_local < anchor_local.replace(microsecond=0):
        raise GateError("map report completion predates the invocation anchor")

    map_summary = expected_summary.read_text(encoding="utf-8")
    if summary_field(map_summary, "Analysis & Synthesis Status") != status:
        raise GateError("map summary status/timestamp differs from raw report")
    if summary_field(map_summary, "Quartus Prime Version") != version:
        raise GateError("map summary tool version differs from raw report")
    if summary_field(map_summary, "Revision Name") != revision:
        raise GateError("map summary revision differs from content witness")
    if summary_field(map_summary, "Top-level Entity Name") != top:
        raise GateError("map summary top differs from raw report")
    if summary_field(map_summary, "Family") != family:
        raise GateError("map summary family differs from raw report")
    map_summary_dsp = parse_int(summary_field(map_summary, "Total DSP Blocks"),
                                "map summary/Total DSP Blocks")
    if map_summary_dsp != resource_dsp:
        raise GateError("map summary DSP total differs from raw report")

    config["_runEvidence"] = {
        "contentWitness": config["contentWitness"],
        "invocationAnchorSha256": anchor_evidence["sha256"],
        "invocationNonce": anchor_evidence["invocationNonce"],
        "manifestSha256": config["_manifestEvidence"]["sha256"],
        "effectiveConfigSha256": config["_manifestEvidence"]["effectiveConfigSha256"],
        "runPreparationSha256": preparation_ref["sha256"],
        "reportSha256": report_sha,
        "mapSummarySha256": sha256_file(expected_summary),
        "preparedAtLocal": preparation["preparedAtLocal"],
        "completedAtLocal": completed_local.isoformat(),
    }


@dataclass(frozen=True)
class ParsedReport:
    resource_dsp: int
    entity_top_dsp: int
    wrapper_rows: int
    wrapper_dsp: int
    independent_mode: int
    dsp_table_total: int
    fixed_multiplier_rows: int


def parse_report(text: str, top: str) -> ParsedReport:
    resources = extract_table(text, RESOURCE_TABLE)
    entities_raw = extract_table(text, ENTITY_TABLE)
    dsp = extract_table(text, DSP_TABLE)

    resource_dsp = unique_labeled_value(resources, "Total DSP Blocks", RESOURCE_TABLE)
    _, entities = entity_rows(entities_raw)
    top_rows = [row for row in entities if row.get("Entity Name") == top]
    if len(top_rows) != 1:
        raise GateError("entity table needs exactly one top row for %s, found %d" %
                        (top, len(top_rows)))
    entity_top_dsp = parse_int(top_rows[0]["DSP Blocks"], "top entity DSP Blocks")
    wrapper = [row for row in entities if row.get("Entity Name") == "zhao_dual18_mul"]
    wrapper_dsp = sum(parse_int(row["DSP Blocks"], "wrapper entity DSP Blocks")
                      for row in wrapper)

    independent_mode = unique_labeled_value(dsp, INDEPENDENT_MODE, DSP_TABLE)
    dsp_table_total = unique_labeled_value(dsp, DSP_TOTAL, DSP_TABLE)
    fixed_multiplier_rows = 0
    for row in dsp:
        if len(row) >= 2 and row[0].startswith("Fixed Point ") and row[0].endswith(" Multiplier"):
            fixed_multiplier_rows += parse_int(row[1], "%s/%s" % (DSP_TABLE, row[0]))

    return ParsedReport(resource_dsp, entity_top_dsp, len(wrapper), wrapper_dsp,
                        independent_mode, dsp_table_total, fixed_multiplier_rows)


def check_consistent_totals(parsed: ParsedReport) -> None:
    if not (parsed.resource_dsp == parsed.entity_top_dsp == parsed.dsp_table_total):
        raise GateError(
            "DSP totals disagree: resource=%d top-entity=%d DSP-table=%d" %
            (parsed.resource_dsp, parsed.entity_top_dsp, parsed.dsp_table_total)
        )


def check_one_block(parsed: ParsedReport) -> None:
    check_consistent_totals(parsed)
    if parsed.resource_dsp != 1:
        raise GateError("one-block gate expected total DSP=1, got %d" % parsed.resource_dsp)
    if parsed.independent_mode != 1:
        raise GateError("one-block gate expected one %r row, got %d" %
                        (INDEPENDENT_MODE, parsed.independent_mode))
    if parsed.wrapper_rows != 1 or parsed.wrapper_dsp != 1:
        raise GateError("one-block gate expected one wrapper row owning one DSP, got rows=%d DSP=%d" %
                        (parsed.wrapper_rows, parsed.wrapper_dsp))
    # Both logical lane multipliers must remain live.  Zero or one is not a
    # harmless reporting omission: it is precisely the lost/merged-lane fault
    # this discriminator exists to reject.  Three means a helper survived.
    if parsed.fixed_multiplier_rows != 2:
        raise GateError("one-block gate expected exactly two live logical multipliers, got %d" %
                        parsed.fixed_multiplier_rows)


def evaluate(text: str, config: dict, variant: str) -> dict:
    holds = inspect_effective_sources(config, variant)
    parsed = parse_report(text, VARIANTS[variant]["top"])
    check_report_provenance(text, config, parsed.resource_dsp)
    status = "hold" if holds else "pass"
    base = {
        "status": status,
        "variant": variant,
        **parsed.__dict__,
        "runEvidence": config["_runEvidence"],
    }
    if holds:
        base["holds"] = holds

    if variant == "inferred":
        check_consistent_totals(parsed)
        if parsed.resource_dsp not in (1, 2):
            raise GateError("inferred contrast must report one or two DSPs, got %d" %
                            parsed.resource_dsp)
        if parsed.wrapper_rows or parsed.wrapper_dsp:
            raise GateError("inferred contrast unexpectedly contains a wrapper hierarchy row")
        if parsed.independent_mode != parsed.resource_dsp:
            raise GateError("inferred mode rows do not reconcile with its physical total")
        base["interpretation"] = (
            "one is good news" if parsed.resource_dsp == 1 else "expected contrast"
        )
        return base

    if variant in ("explicit", "s32x18"):
        check_one_block(parsed)
        # The ordinary Quartus-17 Map report proves the physical/logical counts,
        # while source inspection separately guards intended distinct outputs.
        # It does NOT expose per-result mapped nets, so never upgrade those two
        # facts into a mapped-routing claim.
        holds.append(MAPPED_ROUTE_HOLD)
        base["status"] = "hold"
        base["holds"] = holds
        base["oneBlockResourceGate"] = "pass"
        base["distinctSourceOutputRoutes"] = True
        base["mappedOutputRouteEvidence"] = {
            "status": "hold",
            "artifact": "Quartus-17 Analysis & Synthesis .map.rpt",
            "reason": MAPPED_ROUTE_HOLD,
        }
        return base

    # Positive control: first prove the report itself is the intended two-block
    # mutant, then require the normal detector to reject it.
    check_consistent_totals(parsed)
    if parsed.resource_dsp != 2 or parsed.independent_mode != 2:
        raise GateError("mapping mutant did not produce the required two-DSP/two-mode shape")
    if parsed.wrapper_rows != 2 or parsed.wrapper_dsp != 2:
        raise GateError("mapping mutant needs two wrapper hierarchy rows owning two DSPs")
    detector_fired = False
    try:
        check_one_block(parsed)
    except GateError:
        detector_fired = True
    if not detector_fired:
        raise GateError("one-block detector did not fire on the two-primitive mutant")
    base["positiveControl"] = "one-block detector fired"
    return base


def load_config(path: Path) -> dict:
    try:
        config = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise GateError("cannot read effective config %s: %s" % (path, exc)) from exc
    # Preserve the lexical location.  Path.resolve() here would erase a symlink
    # or junction before the external-anchor boundary gets a chance to reject it.
    config["_configPath"] = str(lexical_absolute(path))
    return config


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=sorted(VARIANTS), required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--effective-config", type=Path, required=True)
    parser.add_argument("--invocation-anchor", type=Path, required=True,
                        help="orchestration-retained anchor outside the dual18 tree")
    parser.add_argument("--invocation-anchor-sha256", required=True,
                        help="anchor SHA-256 retained by orchestration at generation")
    parser.add_argument("--invocation-nonce", required=True,
                        help="unpredictable anchor nonce retained by orchestration")
    parser.add_argument("--manifest-sha256", required=True,
                        help="final manifest SHA-256 retained by orchestration")
    parser.add_argument("--receipt", type=Path,
                        help="write the validated content-bound run result as JSON")
    args = parser.parse_args(argv)

    try:
        report_raw = args.report.read_bytes()
        text = report_raw.decode("utf-8", errors="strict")
        config = load_config(args.effective_config)
        # Keep caller-supplied lexical paths intact.  The checker, not resolve(),
        # decides whether a component crosses a symlink/junction/reparse point.
        config["_invocationAnchorPath"] = str(args.invocation_anchor)
        config["_invocationAnchorSha256"] = args.invocation_anchor_sha256
        config["_invocationNonce"] = args.invocation_nonce
        config["_expectedManifestSha256"] = args.manifest_sha256
        config["_reportPath"] = str(args.report)
        config["_reportSha256"] = hashlib.sha256(report_raw).hexdigest()
        result = evaluate(text, config, args.variant)
        if args.receipt:
            args.receipt.parent.mkdir(parents=True, exist_ok=True)
            args.receipt.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                                    encoding="utf-8", newline="\n")
    except (OSError, UnicodeError, GateError) as exc:
        print("DUAL18_MAP_REJECT: %s" % exc, file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
