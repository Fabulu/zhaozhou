#!/usr/bin/env python3
"""Fresh-map and validate genuine Quartus-CDB dual-18 atom routes.

The physical path is one checker-owned transaction: lock and snapshot every
manifest-bound input, create an unpredictable fresh workspace, run canonical
quartus_map, lock/hash its exact database, then run canonical quartus_cdb.  No
caller may supply map/CDB artifacts.  Every parsed artifact is parsed from the
same immutable bytes that are hashed in the receipt.

Quartus 17 exposes net fanin/fanout adjacency but not internal data dependency
arcs through arbitrary atoms.  This checker never invents those arcs.  A route
that crosses an atom without an API-provided edge is HOLD, especially across
register/control/clock/reset ports.
"""

from __future__ import annotations

import argparse
import base64
import collections
import ctypes
import datetime
import hashlib
import importlib.util
import json
import os
import re
import secrets
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import BinaryIO, Callable, Iterable

REPO = Path(__file__).resolve().parents[2]
MAP_CHECKER = REPO / "tools" / "budget" / "check_dual18_map.py"
_spec = importlib.util.spec_from_file_location("dual18_map_contract", MAP_CHECKER)
if _spec is None or _spec.loader is None:
    raise RuntimeError("cannot load check_dual18_map.py")
map_check = importlib.util.module_from_spec(_spec)
sys.modules[_spec.name] = map_check
_spec.loader.exec_module(map_check)

GATE = "dual18_postmap_lane_route_witness"
ARTIFACT_CLASS = "genuine-quartus-cdb-post-map"
TOOL_VERSION = "17.0.2 Build 602"
CDB_PATH = Path(r"C:\intelFPGA_lite\17.0\quartus\bin64\quartus_cdb.exe")
MAP_PATH = Path(r"C:\intelFPGA_lite\17.0\quartus\bin64\quartus_map.exe")
CAPTURE_SCRIPT = REPO / "tools" / "quartus" / "capture_dual18_atom_routes.tcl"
INPUT_FAMILIES = {"AX": 18, "AY": 18, "BX": 18, "BY": 18}
OUTPUT_FAMILIES = {"RESULTA": 36, "RESULTB": 36}
OWNER_ATOM_TYPE = "MAC"
ROUTE_VARIANTS = {
    "explicit": "dual18_explicit_pair",
    "lane-collapse-mutant": "dual18_lane_collapse_mutant",
    "lane-swap-mutant": "dual18_lane_swap_mutant",
    "two-primitives-mutant": "dual18_two_primitives_mutant",
}
CONTROL_VARIANTS = {
    "lane-collapse-mutant": "lane-collapse",
    "lane-swap-mutant": "lane-swap",
    "two-primitives-mutant": "two-atom",
}
ENCRYPTED_MODEL_HOLD = (
    "encrypted vendor-model differential simulation remains HOLD; mapped routes "
    "do not prove arithmetic semantics"
)
QUARTUS_HARD_PATH_LIMIT = 260
QUARTUS_PATH_MARGIN = 40
QUARTUS_INTERNAL_PATH_LIMIT = QUARTUS_HARD_PATH_LIMIT - QUARTUS_PATH_MARGIN
RUNTIME_LEAF_PREFIX = "d18_"
RUNTIME_LEAF_DIGEST_BYTES = 6
RUNTIME_CREATE_ATTEMPTS = 32
RUNTIME_HASH_DOMAIN = "dual18-map-cdb-workspace-v1"
RUNTIME_PARENT_NAME = "d18_runs"
COMPILED_PARTITION_DIR = "incremental_db/compiled_partitions"
# Longest retained Quartus-17 compiled-partition artifact in the local smoke
# workspace; longer than the ordinary .root_partition.map.hdb.
COMPILED_PARTITION_SUFFIX = ".root_partition.map.hbdb.hb_info"


class GateError(RuntimeError):
    """Contradictory, forged, replayed, or structurally wrong evidence."""


class DetectorFired(GateError):
    """A route detector rejected a complete graph (positive-control signal)."""

    def __init__(self, message: str, evidence: dict | None = None):
        super().__init__(message)
        self.evidence = evidence


class HoldError(RuntimeError):
    """Evidence is unavailable/hidden, so the physical claim is undecidable."""


def _canonical(path: str | Path) -> Path:
    return map_check.lexical_absolute(path)


def _same_path(left: str | Path, right: str | Path) -> bool:
    return os.path.normcase(os.path.normpath(os.fspath(left))) == os.path.normcase(
        os.path.normpath(os.fspath(right))
    )


def _runtime_path_policy() -> dict:
    return {
        "schemaVersion": 1,
        "freshRunTokenBytes": 32,
        "workspaceLeafPrefix": RUNTIME_LEAF_PREFIX,
        "workspaceLeafHash": "sha256",
        "workspaceLeafHashDomain": RUNTIME_HASH_DOMAIN,
        "workspaceLeafHashInputs": ["captureId", "freshRunToken"],
        "workspaceLeafDigestBytes": RUNTIME_LEAF_DIGEST_BYTES,
        "exclusiveCreateMaxAttempts": RUNTIME_CREATE_ATTEMPTS,
        "anchoredWorkspaceParentName": RUNTIME_PARENT_NAME,
        "quartusHardPathLimit": QUARTUS_HARD_PATH_LIMIT,
        "quartusHardPathMargin": QUARTUS_PATH_MARGIN,
        "quartusInternalPathLimit": QUARTUS_INTERNAL_PATH_LIMIT,
        "longestInternalPathKind": "compiled-partition-artifact",
        "compiledPartitionArtifactSuffix": COMPILED_PARTITION_SUFFIX,
        "longestInternalPathTemplate": (
            COMPILED_PARTITION_DIR + "/{revision}" + COMPILED_PARTITION_SUFFIX
        ),
    }


def _workspace_leaf(capture_id: str, run_token: str) -> str:
    """Bind a compact path leaf to the full token and immutable capture ID."""
    if not re.fullmatch(r"[0-9a-f]{64}", capture_id):
        raise GateError("runtime capture ID is not a lowercase SHA-256")
    if not re.fullmatch(r"[A-Za-z0-9_-]{43}", run_token):
        raise GateError("fresh runtime token is not a 32-byte unpadded base64url value")
    try:
        token_bytes = base64.urlsafe_b64decode(run_token + "=")
    except (ValueError, TypeError) as exc:
        raise GateError("fresh runtime token is malformed base64url") from exc
    if (
        len(token_bytes) != 32
        or base64.urlsafe_b64encode(token_bytes).decode("ascii").rstrip("=")
        != run_token
    ):
        raise GateError("fresh runtime token does not retain exactly 32 bytes")
    payload = (
        RUNTIME_HASH_DOMAIN.encode("ascii")
        + b"\0"
        + capture_id.encode("ascii")
        + b"\0"
        + run_token.encode("ascii")
    )
    digest = hashlib.sha256(payload).digest()[:RUNTIME_LEAF_DIGEST_BYTES]
    encoded = base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")
    leaf = RUNTIME_LEAF_PREFIX + encoded
    if not re.fullmatch(r"d18_[A-Za-z0-9_-]{8}", leaf):
        raise GateError("derived runtime workspace leaf is malformed")
    return leaf


def _quartus_path_preflight(workspace_parent: Path, revision: str) -> dict:
    """Compute and enforce the longest expected compiled-partition file path."""
    if not isinstance(revision, str) or not re.fullmatch(r"[A-Za-z0-9_]+", revision):
        raise GateError("Quartus path preflight revision is unsafe")
    parent = _canonical(workspace_parent)
    representative_leaf = RUNTIME_LEAF_PREFIX + (
        "X" * (RUNTIME_LEAF_DIGEST_BYTES * 4 // 3)
    )
    relative = "%s/%s%s" % (
        COMPILED_PARTITION_DIR, revision, COMPILED_PARTITION_SUFFIX
    )
    expected = _canonical(parent / representative_leaf / Path(relative)).as_posix()
    record = {
        "schemaVersion": 1,
        "limit": QUARTUS_INTERNAL_PATH_LIMIT,
        "quartusHardPathLimit": QUARTUS_HARD_PATH_LIMIT,
        "quartusHardPathMargin": QUARTUS_PATH_MARGIN,
        "workspaceLeafLength": len(representative_leaf),
        "longestInternalRelativePath": relative,
        "longestExpectedPath": expected,
        "longestExpectedPathLength": len(expected),
    }
    if record["longestExpectedPathLength"] > QUARTUS_INTERNAL_PATH_LIMIT:
        raise GateError(
            "Quartus internal path preflight exceeds %d characters: %s (%d)"
            % (
                QUARTUS_INTERNAL_PATH_LIMIT,
                expected,
                record["longestExpectedPathLength"],
            )
        )
    if QUARTUS_INTERNAL_PATH_LIMIT != QUARTUS_HARD_PATH_LIMIT - QUARTUS_PATH_MARGIN:
        raise GateError("Quartus internal path preflight lost its 40-character margin")
    return record


def _validate_manifest_path_preflights(manifest: dict, manifest_path: Path) -> None:
    """Recompute every generated variant's path bound from its anchored parent."""
    rows = manifest["revisions"]
    expected_variants = []
    for row in rows:
        base = row.get("baseRevision")
        revision = row.get("revision")
        if not isinstance(base, str) or not isinstance(revision, str):
            raise GateError("manifest path preflight row identity is malformed")
        expected = _quartus_path_preflight(
            _canonical(manifest_path.parent.parent / RUNTIME_PARENT_NAME), revision
        )
        if row.get("quartusInternalPathPreflight") != expected:
            raise GateError(
                "manifest %s Quartus internal path preflight is stale" % base
            )
        expected_variants.append(
            {"baseRevision": base, "revision": revision, **expected}
        )
    worst = max(
        expected_variants, key=lambda row: row["longestExpectedPathLength"]
    )
    expected_summary = {
        "schemaVersion": 1,
        "limit": QUARTUS_INTERNAL_PATH_LIMIT,
        "quartusHardPathLimit": QUARTUS_HARD_PATH_LIMIT,
        "quartusHardPathMargin": QUARTUS_PATH_MARGIN,
        "variants": expected_variants,
        "worstVariant": worst["baseRevision"],
        "worstExpectedPathLength": worst["longestExpectedPathLength"],
    }
    if manifest.get("quartusInternalPathPreflight") != expected_summary:
        raise GateError("manifest-wide Quartus internal path preflight is stale")


def _signature(info: os.stat_result) -> tuple[int, int, int, int, int]:
    return (
        int(info.st_dev),
        int(info.st_ino),
        int(info.st_size),
        int(info.st_mtime_ns),
        int(getattr(info, "st_file_attributes", 0)),
    )


def _open_read_locked(path: Path) -> BinaryIO:
    """Open a read handle that denies write/delete sharing on Windows."""
    if os.name != "nt":
        return path.open("rb")
    import msvcrt

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    create_file = kernel32.CreateFileW
    create_file.argtypes = [
        ctypes.c_wchar_p,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.c_void_p,
    ]
    create_file.restype = ctypes.c_void_p
    handle = create_file(
        str(path),
        0x80000000,  # GENERIC_READ
        0x00000001,  # FILE_SHARE_READ only: deny write and delete
        None,
        3,  # OPEN_EXISTING
        0x08000000,  # FILE_FLAG_SEQUENTIAL_SCAN
        None,
    )
    invalid = ctypes.c_void_p(-1).value
    if handle in (None, invalid):
        error = ctypes.get_last_error()
        raise OSError(error, "CreateFileW read lock failed", str(path))
    try:
        fd = msvcrt.open_osfhandle(int(handle), os.O_RDONLY | os.O_BINARY)
    except BaseException:
        kernel32.CloseHandle(ctypes.c_void_p(handle))
        raise
    return os.fdopen(fd, "rb", closefd=True)


@dataclass
class ArtifactSnapshot:
    """One immutable read plus a retained identity/anti-mutation handle."""

    path: Path
    role: str
    data: bytes
    sha256: str
    signature: tuple[int, int, int, int, int]
    handle: BinaryIO | None = field(repr=False, default=None)

    @classmethod
    def capture(
        cls, path: str | Path, role: str, *, missing_is_hold: bool = False
    ) -> "ArtifactSnapshot":
        canonical = _canonical(path)
        try:
            map_check.require_direct_path(canonical, role, "file")
            handle = _open_read_locked(canonical)
        except map_check.GateError as exc:
            if missing_is_hold and "missing" in str(exc).lower():
                raise HoldError("%s is unavailable: %s" % (role, exc)) from exc
            raise GateError("cannot lock %s: %s" % (role, exc)) from exc
        except FileNotFoundError as exc:
            if missing_is_hold:
                raise HoldError("%s is unavailable: %s" % (role, exc)) from exc
            raise GateError("cannot lock %s: %s" % (role, exc)) from exc
        except OSError as exc:
            raise GateError("cannot lock %s: %s" % (role, exc)) from exc
        try:
            before = os.fstat(handle.fileno())
            chunks: list[bytes] = []
            while True:
                chunk = handle.read(1024 * 1024)
                if not chunk:
                    break
                chunks.append(chunk)
            after = os.fstat(handle.fileno())
            if _signature(before) != _signature(after):
                raise GateError("%s changed during its only read" % role)
            path_info = canonical.lstat()
            if _signature(path_info) != _signature(after):
                raise GateError("%s path changed while it was opened" % role)
            data = b"".join(chunks)
            return cls(
                canonical,
                role,
                data,
                hashlib.sha256(data).hexdigest(),
                _signature(after),
                handle,
            )
        except BaseException:
            handle.close()
            raise

    def verify_identity(self) -> None:
        if self.handle is None:
            raise GateError("%s anti-mutation handle was closed early" % self.role)
        try:
            handle_signature = _signature(os.fstat(self.handle.fileno()))
            path_signature = _signature(self.path.lstat())
        except OSError as exc:
            raise GateError("%s disappeared after snapshot: %s" % (self.role, exc)) from exc
        if handle_signature != self.signature or path_signature != self.signature:
            raise GateError("%s was swapped or mutated after its immutable snapshot" % self.role)

    def text(self, encoding: str = "utf-8") -> str:
        try:
            return self.data.decode(encoding, errors="strict")
        except UnicodeError as exc:
            raise GateError("%s is not strict %s" % (self.role, encoding)) from exc

    def json(self) -> dict:
        try:
            value = json.loads(self.text())
        except json.JSONDecodeError as exc:
            raise GateError("%s is malformed JSON: %s" % (self.role, exc)) from exc
        if not isinstance(value, dict):
            raise GateError("%s JSON root is not an object" % self.role)
        return value

    def record(self) -> dict:
        return {
            "path": str(self.path),
            "sha256": self.sha256,
            "size": len(self.data),
            "modifiedAtUnixNs": self.signature[3],
        }

    def close(self) -> None:
        if self.handle is not None:
            self.handle.close()
            self.handle = None


@dataclass
class BoundInputs:
    config: dict
    anchor: dict
    manifest: dict
    preparation: dict
    invocation: dict
    snapshots: dict[str, ArtifactSnapshot]
    sources: list[ArtifactSnapshot]
    vendor: list[ArtifactSnapshot]
    manifest_row: dict

    def all_snapshots(self) -> list[ArtifactSnapshot]:
        return list(self.snapshots.values()) + self.sources + self.vendor

    def verify_identities(self) -> None:
        for snapshot in self.all_snapshots():
            snapshot.verify_identity()

    def close(self) -> None:
        for snapshot in reversed(self.all_snapshots()):
            snapshot.close()


def _json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _decode_json(snapshot: ArtifactSnapshot) -> dict:
    return snapshot.json()


def _require_hash(value: object, role: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise GateError("%s is not a lowercase SHA-256" % role)
    return value


def _validate_source_shapes_from_snapshots(
    config: dict,
    variant: str,
    source_rows: list[dict],
    source_snapshots: list[ArtifactSnapshot],
) -> None:
    """Validate control/candidate intent from the already-hashed source bytes."""
    source_text = {
        row["path"]: snapshot.text()
        for row, snapshot in zip(source_rows, source_snapshots)
    }
    wrapper = source_text[map_check.WRAPPER_SOURCE]
    vendor_match = re.search(
        r"`ifdef\s+ZHAO_DUAL18_CYCLONEV(?P<body>.*?)"
        r"`elsif\s+ZHAO_DUAL18_BEHAVIORAL",
        wrapper,
        flags=re.DOTALL,
    )
    if not vendor_match:
        raise GateError("wrapper has no preprocessor-exclusive Cyclone V branch")
    vendor_body = map_check.strip_sv_comments_and_attributes(vendor_match.group("body"))
    if len(re.findall(r"\bcyclonev_mac\s*#\s*\(", vendor_body)) != 1:
        raise GateError("wrapper vendor branch must contain exactly one cyclonev_mac")
    required_settings = (
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
        r"\.resulta\s*\(\s*resulta_o\s*\)",
        r"\.resultb\s*\(\s*resultb_o\s*\)",
        r'\.ax_clock\s*\(\s*"none"\s*\)',
        r'\.ay_scan_in_clock\s*\(\s*"none"\s*\)',
        r'\.bx_clock\s*\(\s*"none"\s*\)',
        r'\.by_clock\s*\(\s*"none"\s*\)',
        r'\.output_clock\s*\(\s*"none"\s*\)',
        r"\.clk\s*\(\s*3'b000\s*\)",
        r"\.ena\s*\(\s*3'b111\s*\)",
        r"\.aclr\s*\(\s*2'b00\s*\)",
    )
    if not all(re.search(pattern, vendor_body) for pattern in required_settings):
        raise GateError("cyclonev_mac candidate settings or distinct ports are incomplete")
    if "*" in re.sub(r"<<<|>>>|<<|>>", "", vendor_body):
        raise GateError("Cyclone V backend contains arithmetic outside cyclonev_mac")

    source_path = source_rows[-1]["path"]
    top_body = map_check.module_body(source_text[source_path], config["top"])
    expected_instances = 2 if variant == "two-primitives-mutant" else 1
    instances = len(re.findall(r"\bzhao_dual18_mul\s*#\s*\(", top_body))
    if instances != expected_instances:
        raise GateError(
            "%s source has %d wrapper instances, expected %d"
            % (config["top"], instances, expected_instances)
        )
    if "*" in re.sub(r"<<<|>>>|<<|>>", "", top_body):
        raise GateError("route calibration top contains a helper multiplication")
    if variant == "explicit":
        required = (
            r"\.resulta_o\s*\(\s*resulta_o\s*\)",
            r"\.resultb_o\s*\(\s*resultb_o\s*\)",
        )
        if not all(re.search(pattern, top_body) for pattern in required):
            raise GateError("explicit source does not preserve two distinct live result routes")
    elif variant == "lane-collapse-mutant":
        if len(re.findall(r"\bassign\s+result[ab]_o\s*=\s*prod_a_c\s*;", top_body)) != 2:
            raise GateError("lane-collapse control no longer collapses both logical outputs")
        if not re.search(r"\.resultb_o\s*\(\s*prod_b_wrong_sink_c\s*\)", top_body):
            raise GateError("lane-collapse control no longer retains physical RESULTB")
        if not re.search(
            r"\bassign\s+resultb_wrong_sink_o\s*=\s*prod_b_wrong_sink_c\s*;",
            top_body,
        ):
            raise GateError("lane-collapse control lacks an independently observable wrong sink")
        if re.search(r"\(\*\s*preserve\s*\*\).*prod_b_wrong_sink_c", top_body):
            raise GateError("lane-collapse control relies on preserve for RESULTB")
    elif variant == "lane-swap-mutant":
        required = (
            r"\bassign\s+resulta_o\s*=\s*prod_b_c\s*;",
            r"\bassign\s+resultb_o\s*=\s*prod_a_c\s*;",
        )
        if not all(re.search(pattern, top_body) for pattern in required):
            raise GateError("lane-swap control no longer crosses logical result routes")


def _acquire_tool(
    contract: dict, field_name: str, canonical_path: Path, role: str
) -> ArtifactSnapshot:
    row = contract.get(field_name)
    row_path = Path(row.get("path", "")) if isinstance(row, dict) else Path()
    if (
        not isinstance(row, dict)
        or not row_path.is_absolute()
        or row_path != _canonical(row_path)
        or row_path != _canonical(canonical_path)
    ):
        raise GateError("%s path is not canonical" % role)
    if row.get("expectedVersion") != TOOL_VERSION:
        raise GateError("%s version contract is not %s" % (role, TOOL_VERSION))
    if row.get("available") is not True:
        raise HoldError("%s was unavailable when calibration was generated" % role)
    snapshot = ArtifactSnapshot.capture(canonical_path, role, missing_is_hold=True)
    if snapshot.sha256 != row.get("sha256"):
        snapshot.close()
        raise GateError("%s hash differs from the content-bound contract" % role)
    return snapshot


def acquire_bound_inputs(
    config_path: Path,
    anchor_path: Path,
    anchor_sha256: str,
    invocation_nonce: str,
    manifest_sha256: str,
    variant: str,
) -> BoundInputs:
    """Lock/read each trust-chain artifact exactly once, then validate bytes."""
    snapshots: dict[str, ArtifactSnapshot] = {}
    sources: list[ArtifactSnapshot] = []
    vendor: list[ArtifactSnapshot] = []
    try:
        supplied_config = Path(config_path)
        supplied_anchor = Path(anchor_path)
        config_path = _canonical(supplied_config)
        anchor_path = _canonical(supplied_anchor)
        if not supplied_config.is_absolute() or supplied_config != config_path:
            raise GateError("effective config path is not absolute canonical lexical form")
        if not supplied_anchor.is_absolute() or supplied_anchor != anchor_path:
            raise GateError("invocation anchor path is not absolute canonical lexical form")
        snapshots["config"] = ArtifactSnapshot.capture(config_path, "effective config")
        snapshots["anchor"] = ArtifactSnapshot.capture(anchor_path, "external invocation anchor")
        config = _decode_json(snapshots["config"])
        anchor = _decode_json(snapshots["anchor"])

        if snapshots["anchor"].sha256 != _require_hash(anchor_sha256, "supplied anchor hash"):
            raise GateError("invocation anchor hash differs from orchestration")
        _require_hash(invocation_nonce, "supplied invocation nonce")
        _require_hash(manifest_sha256, "supplied manifest hash")
        if anchor.get("invocationNonce") != invocation_nonce:
            raise GateError("invocation anchor nonce differs from orchestration")
        if anchor.get("manifestSha256") != manifest_sha256:
            raise GateError("invocation anchor manifest hash differs from orchestration")
        candidate_tree = config_path.parent.parent
        expected_anchor = candidate_tree.parent / map_check.INVOCATION_ANCHOR_FILE
        if anchor_path != _canonical(expected_anchor):
            raise GateError("invocation anchor is not at the canonical external location")
        if (
            anchor.get("schemaVersion") != 1
            or anchor.get("gate") != "dual18_physical_pack_discriminator"
            or anchor.get("trustBoundary")
            != "orchestration-supplied-outside-candidate-tree"
            or anchor.get("manifestPath") != "dual18/dual18_manifest.json"
        ):
            raise GateError("invocation anchor identity is malformed")
        manifest_path = _canonical(anchor_path.parent / anchor["manifestPath"])
        snapshots["manifest"] = ArtifactSnapshot.capture(manifest_path, "anchored manifest")
        if snapshots["manifest"].sha256 != manifest_sha256:
            raise GateError("candidate manifest hash differs from external anchor")
        manifest = _decode_json(snapshots["manifest"])
        if (
            manifest.get("schemaVersion") != 1
            or manifest.get("gate") != "dual18_physical_pack_discriminator"
            or manifest.get("stage") != "map-only"
            or manifest.get("device") != map_check.DEVICE
        ):
            raise GateError("dual18 manifest identity is malformed")
        expected_bases = {row["top"] for row in map_check.VARIANTS.values()}
        rows = manifest.get("revisions")
        if (
            not isinstance(rows, list)
            or len(rows) != len(expected_bases)
            or {row.get("baseRevision") for row in rows if isinstance(row, dict)}
            != expected_bases
        ):
            raise GateError("manifest does not contain the exact six revisions")
        _validate_manifest_path_preflights(
            manifest, snapshots["manifest"].path
        )
        matches = [row for row in rows if row.get("baseRevision") == config.get("baseRevision")]
        if len(matches) != 1:
            raise GateError("manifest has no unique effective-config row")
        manifest_row = matches[0]
        revision_dir = config_path.parent
        expected_revision = config.get("revision")
        if not isinstance(expected_revision, str):
            raise GateError("effective revision is missing")
        if config.get("qpfFile") != expected_revision + ".qpf":
            raise GateError("effective QPF filename is not revision-derived")
        if config.get("qsfFile") != expected_revision + ".qsf":
            raise GateError("effective QSF filename is not revision-derived")
        expected_config = _canonical(revision_dir / "effective_config.json")
        if config_path != expected_config:
            raise GateError("effective config path is not canonical")
        expected_manifest_paths = {
            "projectFile": revision_dir / config["qpfFile"],
            "settingsFile": revision_dir / config["qsfFile"],
            "runPreparation": revision_dir / "run_preparation.json",
            "effectiveConfig": expected_config,
            "routeInvocation": revision_dir / "atom_route_invocation.json",
        }
        for field_name, expected_path in expected_manifest_paths.items():
            try:
                actual_path = map_check._resolve_manifest_reference(manifest_row.get(field_name))
            except (TypeError, map_check.GateError) as exc:
                raise GateError("manifest %s path is malformed" % field_name) from exc
            if actual_path != _canonical(expected_path):
                raise GateError("manifest %s path is not canonical" % field_name)
        if snapshots["config"].sha256 != manifest_row.get("effectiveConfigSha256"):
            raise GateError("effective config bytes differ from manifest")
        for field_name in ("revision", "contentWitness", "top"):
            if config.get(field_name) != manifest_row.get(field_name):
                raise GateError("effective config %s differs from manifest" % field_name)
        if config.get("sourceSetSha256") != manifest_row.get("sourceSetSha256"):
            raise GateError("effective source-set hash differs from manifest")
        if config.get("qsfSha256") != manifest_row.get("settingsFileSha256"):
            raise GateError("effective QSF hash differs from manifest")
        if manifest.get("routeCaptureContract") != config.get("routeCaptureContract"):
            raise GateError("manifest and effective route contracts differ")

        expected_base = ROUTE_VARIANTS[variant]
        spec = map_check.VARIANTS[variant]
        if config.get("baseRevision") != expected_base or config.get("top") != spec["top"]:
            raise GateError("variant/config top mismatch")
        if config.get("stage") != "map-only" or config.get("device") != map_check.DEVICE:
            raise GateError("effective config map/device identity is malformed")
        if config.get("macros") != spec["macros"]:
            raise GateError("effective macro set differs from variant contract")
        if variant in CONTROL_VARIANTS:
            if config.get("controlKind") != CONTROL_VARIANTS[variant] or not re.search(
                r"\bmust reject\b", str(config.get("positiveControl", "")), re.IGNORECASE
            ):
                raise GateError("positive-control intent is missing from effective config")

        path_specs = {
            "preparation": (
                revision_dir / "run_preparation.json",
                manifest_row.get("runPreparationSha256"),
            ),
            "qpf": (revision_dir / config.get("qpfFile", ""), manifest_row.get("projectFileSha256")),
            "qsf": (revision_dir / config.get("qsfFile", ""), manifest_row.get("settingsFileSha256")),
            "invocation": (
                revision_dir / "atom_route_invocation.json",
                manifest_row.get("routeInvocationSha256"),
            ),
        }
        for role, (path, expected_hash) in path_specs.items():
            snapshots[role] = ArtifactSnapshot.capture(path, role)
            if snapshots[role].sha256 != expected_hash:
                raise GateError("%s bytes differ from manifest" % role)
        preparation = _decode_json(snapshots["preparation"])
        invocation = _decode_json(snapshots["invocation"])
        preparation_ref = config.get("runPreparation")
        if (
            not isinstance(preparation_ref, dict)
            or preparation_ref.get("file") != "run_preparation.json"
            or preparation_ref.get("sha256") != snapshots["preparation"].sha256
            or preparation_ref.get("sha256") != manifest_row.get("runPreparationSha256")
        ):
            raise GateError("run-preparation reference differs from immutable bytes")
        if (
            preparation.get("schemaVersion") != 1
            or preparation.get("gate") != "dual18_physical_pack_discriminator"
            or preparation.get("baseRevision") != config.get("baseRevision")
            or preparation.get("revision") != config.get("revision")
            or preparation.get("contentWitness") != config.get("contentWitness")
            or preparation.get("outputDirectory") != "output_files"
            or preparation.get("outputDirectoryWasEmpty") is not True
            or preparation.get("checkerMustRunFreshMap") is not True
            or preparation.get("quartusInternalPathPreflight")
            != manifest_row.get("quartusInternalPathPreflight")
        ):
            raise GateError("run-preparation identity is malformed")

        source_rows = config.get("sources")
        if not isinstance(source_rows, list) or [row.get("path") for row in source_rows] != spec["sources"]:
            raise GateError("effective source list differs from variant contract")
        source_set = hashlib.sha256()
        absolute_sources: list[str] = []
        witnessed_sources: list[dict] = []
        for row in source_rows:
            expected_path = _canonical(REPO / row["path"])
            if not _same_path(row.get("absolutePath", ""), expected_path):
                raise GateError("source %s is not at its canonical path" % row["path"])
            snapshot = ArtifactSnapshot.capture(expected_path, "source %s" % row["path"])
            sources.append(snapshot)
            if snapshot.sha256 != row.get("sha256"):
                raise GateError("source %s hash differs from config" % row["path"])
            source_set.update(row["path"].encode("utf-8"))
            source_set.update(b"\0")
            source_set.update(snapshot.sha256.encode("ascii"))
            source_set.update(b"\n")
            absolute_sources.append(expected_path.as_posix())
            witnessed_sources.append({"path": row["path"], "sha256": snapshot.sha256})
        if source_set.hexdigest() != config.get("sourceSetSha256"):
            raise GateError("source-set hash differs from immutable source bytes")
        expected_qsf = map_check.expected_qsf_text(spec, absolute_sources).encode("ascii")
        if snapshots["qsf"].data != expected_qsf:
            raise GateError("QSF is not the exact generated source/settings set")
        expected_qpf = (
            'QUARTUS_VERSION = "17.0"\nPROJECT_REVISION = "%s"\n' % config["revision"]
        ).encode("ascii")
        if snapshots["qpf"].data != expected_qpf:
            raise GateError("QPF does not carry the exact content-addressed revision")
        _validate_source_shapes_from_snapshots(config, variant, source_rows, sources)

        contract = config.get("routeCaptureContract")
        if not isinstance(contract, dict):
            raise GateError("route capture contract is missing")
        script_row = contract.get("captureScript")
        script_bound_path = Path(script_row.get("absolutePath", "")) if isinstance(script_row, dict) else Path()
        if (
            not isinstance(script_row, dict)
            or script_row.get("path") != "tools/quartus/capture_dual18_atom_routes.tcl"
            or not script_bound_path.is_absolute()
            or script_bound_path != _canonical(script_bound_path)
            or script_bound_path != _canonical(CAPTURE_SCRIPT)
        ):
            raise GateError("capture script path is not canonical")
        snapshots["script"] = ArtifactSnapshot.capture(CAPTURE_SCRIPT, "capture Tcl script")
        if snapshots["script"].sha256 != script_row.get("sha256"):
            raise GateError("capture Tcl script hash differs from content witness")
        snapshots["quartusMap"] = _acquire_tool(contract, "quartusMap", MAP_PATH, "quartus_map")
        snapshots["quartusCdb"] = _acquire_tool(contract, "quartusCdb", CDB_PATH, "quartus_cdb")

        evidence_rows = config.get("vendorInterfaceEvidence")
        if not isinstance(evidence_rows, list) or len(evidence_rows) != len(map_check.VENDOR_EVIDENCE_SPECS):
            raise GateError("canonical vendor evidence records are missing")
        normalized_vendor: list[dict] = []
        for evidence_row, evidence_spec in zip(evidence_rows, map_check.VENDOR_EVIDENCE_SPECS):
            if (
                evidence_row.get("kind") != evidence_spec["kind"]
                or evidence_row.get("path") != evidence_spec["path"]
            ):
                raise GateError("vendor evidence path/kind is not canonical")
            if evidence_row.get("available") is not True:
                raise HoldError("canonical vendor evidence is unavailable")
            snapshot = ArtifactSnapshot.capture(evidence_spec["path"], "vendor %s" % evidence_spec["kind"])
            vendor.append(snapshot)
            if snapshot.sha256 != evidence_row.get("sha256"):
                raise GateError("vendor %s hash differs from config" % evidence_spec["kind"])
            text = snapshot.text()
            search_text = text
            if evidence_spec["kind"] == "atom-declaration":
                scope = re.search(
                    r"(?ms)^module\s+cyclonev_mac\s*\(.*?^endmodule\s*//cyclonev_mac\s*$",
                    text,
                )
                if not scope:
                    raise GateError("canonical cyclonev_mac declaration is unavailable")
                search_text = scope.group(0)
            excerpts = []
            for pattern in evidence_spec["requiredPatterns"]:
                match = re.search(pattern, search_text)
                if not match:
                    raise GateError("vendor evidence misses required pattern %r" % pattern)
                excerpts.append(match.group(0).strip())
            normalized = {
                "kind": evidence_spec["kind"],
                "path": evidence_spec["path"],
                "available": True,
                "sha256": snapshot.sha256,
                "matchedExcerpts": excerpts,
            }
            if normalized != evidence_row:
                raise GateError("vendor evidence excerpts differ from immutable bytes")
            normalized_vendor.append(normalized)

        witness_inputs = {
            "schemaVersion": 1,
            "gate": "dual18_physical_pack_discriminator",
            "tool": {"name": "Quartus Prime Lite", "version": "17.0.2", "build": 602},
            "device": map_check.DEVICE,
            "baseRevision": config["baseRevision"],
            "top": config["top"],
            "sources": witnessed_sources,
            "macros": list(config["macros"]),
            "sourceSetSha256": source_set.hexdigest(),
            "qsfSha256": snapshots["qsf"].sha256,
            "vendorInterfaceEvidence": normalized_vendor,
            "routeCaptureContract": contract,
        }
        if config.get("witnessInputs") != witness_inputs:
            raise GateError("content-witness inputs differ from immutable artifacts")
        witness = hashlib.sha256(
            json.dumps(witness_inputs, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")
        ).hexdigest()
        if config.get("contentWitness") != witness:
            raise GateError("effective content witness is stale")
        if config.get("revision") != "%s_%s" % (config["baseRevision"], witness[:16]):
            raise GateError("content-addressed revision is stale")

        route_ref = config.get("routeCapture")
        if preparation.get("freshRuntimeWorkspacePrefix") != (
            RUNTIME_PARENT_NAME + "/" + invocation.get("freshWorkspacePrefix", "")
        ):
            raise GateError("run-preparation fresh workspace prefix is stale")
        if (
            not isinstance(route_ref, dict)
            or route_ref.get("invocationFile") != "atom_route_invocation.json"
            or route_ref.get("invocationSha256") != snapshots["invocation"].sha256
            or route_ref.get("invocationSha256") != manifest_row.get("routeInvocationSha256")
            or route_ref.get("captureId") != invocation.get("captureId")
            or route_ref.get("captureId") != manifest_row.get("routeCaptureId")
            or route_ref.get("freshWorkspacePrefix")
            != RUNTIME_PARENT_NAME + "/" + invocation.get("freshWorkspacePrefix", "")
            or route_ref.get("quartusInternalPathPreflight")
            != manifest_row.get("quartusInternalPathPreflight")
            or route_ref.get("checkerRunsMap") is not True
        ):
            raise GateError("route invocation reference is stale")
        _validate_contract_and_invocation(
            config, contract, invocation, revision_dir, snapshots["invocation"]
        )
        _validate_anchor_times(anchor, snapshots["anchor"], preparation)
        return BoundInputs(
            config,
            anchor,
            manifest,
            preparation,
            invocation,
            snapshots,
            sources,
            vendor,
            manifest_row,
        )
    except BaseException:
        for snapshot in reversed(list(snapshots.values()) + sources + vendor):
            snapshot.close()
        raise


def _validate_anchor_times(anchor: dict, anchor_snapshot: ArtifactSnapshot, preparation: dict) -> None:
    created_ns = anchor.get("createdAtUnixNs")
    prepared_ns = preparation.get("preparedAtUnixNs")
    if not isinstance(created_ns, int) or not isinstance(prepared_ns, int):
        raise GateError("anchor/preparation timestamps are missing")
    if created_ns < prepared_ns:
        raise GateError("invocation anchor is older than preparation")
    if created_ns > time.time_ns() + 5_000_000_000:
        raise GateError("invocation anchor timestamp is in the future")
    if anchor_snapshot.signature[3] < created_ns:
        raise GateError("invocation anchor file predates claimed creation")


def _validate_contract_and_invocation(
    config: dict,
    contract: dict,
    invocation: dict,
    revision_dir: Path,
    invocation_snapshot: ArtifactSnapshot,
) -> None:
    route_reference = config.get("routeCapture")
    if (
        not isinstance(route_reference, dict)
        or route_reference.get("invocationSha256") != invocation_snapshot.sha256
    ):
        raise GateError("route invocation bytes differ from effective config")
    expected_contract_keys = {
        "schemaVersion", "gate", "artifactClass", "syntheticFixturesPhysical",
        "netlistType", "quartusMap", "quartusCdb", "captureScript",
        "runtimePathPolicy", "mapArgumentOrder", "tclArgumentOrder",
        "databaseDirectories",
        "atomAdjacency", "internalAtomArcs", "requiredAtomType",
        "inputPortFamilies", "outputPortFamilies",
    }
    if set(contract) != expected_contract_keys or (
        contract["schemaVersion"] != 3
        or contract["gate"] != GATE
        or contract["artifactClass"] != ARTIFACT_CLASS
        or contract["syntheticFixturesPhysical"] is not False
        or contract["netlistType"] != "map"
        or contract["runtimePathPolicy"] != _runtime_path_policy()
        or contract["mapArgumentOrder"] != ["project"]
        or contract["tclArgumentOrder"]
        != ["project", "revision", "atomTsv", "optionalAtomVo", "captureId"]
        or contract["databaseDirectories"] != ["db"]
        or contract["atomAdjacency"] != "exact-cdb-fanin-and-fanout-only"
        or contract["internalAtomArcs"] != "unavailable-hold"
        or contract["requiredAtomType"] != OWNER_ATOM_TYPE
        or contract["inputPortFamilies"] != INPUT_FAMILIES
        or contract["outputPortFamilies"] != OUTPUT_FAMILIES
    ):
        raise GateError("route-capture contract schema/identity is malformed")

    expected_invocation_keys = {
        "schemaVersion", "gate", "artifactClass", "synthetic", "contentWitness",
        "captureId", "workspaceParent", "freshWorkspacePrefix", "freshRunTokenBytes",
        "runtimePathPolicy", "quartusInternalPathPreflight",
        "projectFileName", "settingsFileName", "quartusMap", "quartusCdb",
        "captureScript", "mapCommandTemplate", "cdbCommandTemplate", "runtimeOutputs",
    }
    if set(invocation) != expected_invocation_keys or (
        invocation["schemaVersion"] != 3
        or invocation["gate"] != GATE
        or invocation["artifactClass"] != ARTIFACT_CLASS
        or invocation["synthetic"] is not False
        or invocation["contentWitness"] != config["contentWitness"]
        or not re.fullmatch(r"[0-9a-f]{64}", str(invocation["captureId"]))
        or invocation["freshRunTokenBytes"] != 32
        or invocation["runtimePathPolicy"] != _runtime_path_policy()
        or invocation["runtimePathPolicy"] != contract["runtimePathPolicy"]
        or invocation["quartusMap"] != contract["quartusMap"]
        or invocation["quartusCdb"] != contract["quartusCdb"]
        or invocation["captureScript"] != contract["captureScript"]
    ):
        raise GateError("route invocation schema/identity is malformed")
    expected_parent = _canonical(
        revision_dir.parent.parent / RUNTIME_PARENT_NAME
    )
    supplied_parent = Path(invocation["workspaceParent"])
    expected_preflight = _quartus_path_preflight(
        expected_parent, config["revision"]
    )
    expected_prefix = RUNTIME_LEAF_PREFIX
    if (
        not supplied_parent.is_absolute()
        or supplied_parent != _canonical(supplied_parent)
        or supplied_parent != expected_parent
    ):
        raise GateError("runtime workspace parent is not anchor-derived canonical path")
    if invocation["freshWorkspacePrefix"] != expected_prefix:
        raise GateError("runtime workspace prefix is not canonical")
    if (
        invocation["quartusInternalPathPreflight"] != expected_preflight
        or route_reference.get("quartusInternalPathPreflight") != expected_preflight
    ):
        raise GateError("runtime Quartus internal path preflight is stale")
    if invocation["projectFileName"] != config["qpfFile"] or invocation["settingsFileName"] != config["qsfFile"]:
        raise GateError("runtime project/settings filenames differ from config")
    if invocation["mapCommandTemplate"] != [contract["quartusMap"]["path"], "{project}"]:
        raise GateError("quartus_map command template is forged")
    expected_cdb = [
        contract["quartusCdb"]["path"], "-t", contract["captureScript"]["absolutePath"],
        "{project}", config["revision"], "{atomTsv}", "{optionalAtomVo}", invocation["captureId"],
    ]
    if invocation["cdbCommandTemplate"] != expected_cdb:
        raise GateError("quartus_cdb Tcl argument template is forged")
    outputs = invocation["runtimeOutputs"]
    expected_outputs = {
        "mapLog": "%s.quartus_map.log" % config["revision"],
        "mapReport": "output_files/%s.map.rpt" % config["revision"],
        "mapSummary": "output_files/%s.map.summary" % config["revision"],
        "atomTsv": "route_evidence/%s.dual18.atom.tsv" % config["revision"],
        "cdbLog": "route_evidence/%s.quartus_cdb.log" % config["revision"],
        "optionalAtomVo": "route_evidence/%s.post_map.vo" % config["revision"],
        "receipt": "route_evidence/%s.atom_route_receipt.json" % config["revision"],
    }
    if outputs != expected_outputs:
        raise GateError("runtime output paths are not exact generated names")


@dataclass(frozen=True)
class Port:
    node: str
    direction: str
    port_id: str
    port_type: str
    literal_index: int
    name: str
    encrypted: bool

    @property
    def ref(self) -> tuple[str, str, str]:
        return (self.node, self.direction, self.port_id)


@dataclass
class AtomGraph:
    metadata: dict[str, str]
    nodes: dict[str, tuple[str, bool, str]]
    ports: dict[tuple[str, str, str], Port]
    edge_evidence: dict[
        tuple[tuple[str, str, str], tuple[str, str, str]], set[str]
    ]
    errors: list[list[str]]
    unavailable: list[list[str]]
    notes: list[list[str]]

    def exact_edges(self) -> tuple[dict, dict]:
        forward: dict[tuple[str, str, str], set[tuple[str, str, str]]] = collections.defaultdict(set)
        reverse: dict[tuple[str, str, str], set[tuple[str, str, str]]] = collections.defaultdict(set)
        for (source, destination), evidence in self.edge_evidence.items():
            if source not in self.ports or destination not in self.ports:
                raise HoldError("CDB edge references an unavailable port")
            if self.ports[source].direction != "oport" or self.ports[destination].direction != "iport":
                raise HoldError("CDB edge direction identity is unavailable")
            if evidence != {"fanin", "fanout"}:
                raise HoldError(
                    "CDB fanin/fanout disagree for edge %r -> %r" % (source, destination)
                )
            forward[source].add(destination)
            reverse[destination].add(source)
        return forward, reverse


def _unescape(value: str) -> str:
    result: list[str] = []
    index = 0
    while index < len(value):
        if value[index] != "\\":
            result.append(value[index])
            index += 1
            continue
        index += 1
        if index == len(value) or value[index] not in "\\trn":
            raise GateError("TSV contains an invalid escape")
        result.append({"\\": "\\", "t": "\t", "r": "\r", "n": "\n"}[value[index]])
        index += 1
    return "".join(result)


def _bool(value: str, context: str) -> bool:
    if value.lower() in ("1", "true"):
        return True
    if value.lower() in ("0", "false"):
        return False
    raise GateError("%s is not boolean: %r" % (context, value))


def parse_atom_tsv_bytes(data: bytes) -> AtomGraph:
    try:
        text = data.decode("utf-8", errors="strict")
    except UnicodeError as exc:
        raise GateError("atom TSV is not strict UTF-8") from exc
    metadata: dict[str, str] = {}
    nodes: dict[str, tuple[str, bool, str]] = {}
    ports: dict[tuple[str, str, str], Port] = {}
    edge_evidence: dict[
        tuple[tuple[str, str, str], tuple[str, str, str]], set[str]
    ] = collections.defaultdict(set)
    errors: list[list[str]] = []
    unavailable: list[list[str]] = []
    notes: list[list[str]] = []
    schema_seen = False
    for line_number, raw_line in enumerate(text.splitlines(), 1):
        if not raw_line:
            continue
        cells = [_unescape(cell) for cell in raw_line.split("\t")]
        kind = cells[0]
        if kind == "schema":
            if cells != ["schema", "dual18-atom-route-tsv", "2"] or schema_seen:
                raise GateError("TSV schema is duplicate or unsupported")
            schema_seen = True
        elif kind == "meta":
            if len(cells) != 3 or cells[1] in metadata:
                raise GateError("TSV line %d has malformed metadata" % line_number)
            metadata[cells[1]] = cells[2]
        elif kind == "node":
            if len(cells) != 5 or cells[1] in nodes:
                raise GateError("TSV line %d has malformed node" % line_number)
            nodes[cells[1]] = (cells[2], _bool(cells[3], "node encryption"), cells[4])
        elif kind == "port":
            if len(cells) != 8 or cells[2] not in ("iport", "oport"):
                raise GateError("TSV line %d has malformed port" % line_number)
            try:
                literal_index = int(cells[5])
            except ValueError as exc:
                raise GateError("TSV port index is not integer") from exc
            port = Port(
                cells[1], cells[2], cells[3], cells[4].upper(), literal_index,
                cells[6], _bool(cells[7], "port encryption"),
            )
            if port.ref in ports:
                raise GateError("TSV has duplicate port identity %r" % (port.ref,))
            ports[port.ref] = port
        elif kind == "edge":
            if len(cells) != 6 or cells[5] not in ("fanin", "fanout"):
                raise GateError("TSV line %d has malformed edge" % line_number)
            source = (cells[1], "oport", cells[2])
            destination = (cells[3], "iport", cells[4])
            edge_evidence[(source, destination)].add(cells[5])
        elif kind == "error":
            errors.append(cells[1:])
        elif kind == "unavailable":
            unavailable.append(cells[1:])
        elif kind == "note":
            notes.append(cells[1:])
        else:
            raise GateError("TSV line %d has unknown record %r" % (line_number, kind))
    if not schema_seen:
        raise GateError("TSV has no schema")
    if not nodes or not ports:
        raise HoldError("atom graph contains no usable connectivity")
    return AtomGraph(metadata, nodes, ports, dict(edge_evidence), errors, unavailable, notes)


def parse_atom_tsv(text: str) -> AtomGraph:
    """Compatibility test API; callers still receive the schema-2 byte parser."""
    return parse_atom_tsv_bytes(text.encode("utf-8"))


def _walk(start: Iterable[tuple[str, str, str]], adjacency: dict) -> set[tuple[str, str, str]]:
    seen = set(start)
    pending = list(start)
    while pending:
        current = pending.pop()
        for neighbor in adjacency.get(current, ()):
            if neighbor not in seen:
                seen.add(neighbor)
                pending.append(neighbor)
    return seen


def _boundary_port(graph: AtomGraph, signal: str, direction: str) -> tuple[str, str, str]:
    if direction == "oport":
        node_type, port_type, node_name = "IO_IBUF", "O", signal + "~input"
    else:
        node_type, port_type, node_name = "IO_OBUF", "I", signal + "~output"
    node_ids = [
        node_id
        for node_id, (actual_type, _, name) in graph.nodes.items()
        if actual_type.upper() == node_type and name == node_name
    ]
    matches = [
        ref for ref, port in graph.ports.items()
        if port.node in node_ids
        and port.direction == direction
        and port.port_type == port_type
    ]
    if len(node_ids) != 1 or len(matches) != 1:
        raise HoldError(
            "top %s/%s boundary connectivity for %s is missing or ambiguous"
            % (node_type, port_type, signal)
        )
    return matches[0]


def _family_ports(
    graph: AtomGraph,
    owner: str,
    direction: str,
    family: str,
    width: int,
    connected: dict[tuple[str, str, str], set[tuple[str, str, str]]],
) -> dict[int, tuple[str, str, str]]:
    matches = [
        port for port in graph.ports.values()
        if port.node == owner
        and port.direction == direction
        and port.port_type == family
        and connected.get(port.ref)
    ]
    by_index: dict[int, tuple[str, str, str]] = {}
    for port in matches:
        if port.literal_index in by_index:
            raise DetectorFired("atom has duplicate %s[%d]" % (family, port.literal_index))
        by_index[port.literal_index] = port.ref
    if set(by_index) != set(range(width)):
        raise DetectorFired(
            "%s cardinality/index mismatch: expected 0:%d, got %r"
            % (family, width - 1, sorted(by_index))
        )
    return by_index


def check_lane_graph(graph: AtomGraph) -> dict:
    if graph.errors or graph.unavailable:
        raise HoldError(
            "CDB reported hidden/ambiguous/unavailable connectivity: errors=%d unavailable=%d"
            % (len(graph.errors), len(graph.unavailable))
        )
    if any(encrypted for _, encrypted, _ in graph.nodes.values()):
        raise HoldError("CDB graph contains encrypted atom nodes")
    if any(port.encrypted for port in graph.ports.values()):
        raise HoldError("CDB graph contains encrypted atom ports")
    if graph.metadata.get("connectivity_complete") != "1":
        raise HoldError("CDB did not attest complete atom connectivity")
    owners = [
        node for node, row in graph.nodes.items()
        if row[0].upper() == OWNER_ATOM_TYPE
    ]
    if len(owners) != 1:
        raise DetectorFired(
            "one-atom gate expected exactly one mapped %s owner, got %d"
            % (OWNER_ATOM_TYPE, len(owners))
        )
    owner = owners[0]
    forward, reverse = graph.exact_edges()
    input_ports = {
        family: _family_ports(graph, owner, "iport", family, width, reverse)
        for family, width in INPUT_FAMILIES.items()
    }
    output_ports = {
        family: _family_ports(graph, owner, "oport", family, width, forward)
        for family, width in OUTPUT_FAMILIES.items()
    }
    top_outputs = {
        family: {
            bit: _boundary_port(graph, "%s_o[%d]" % (family.lower(), bit), "iport")
            for bit in range(width)
        }
        for family, width in OUTPUT_FAMILIES.items()
    }
    all_owner_outputs = {ref for rows in output_ports.values() for ref in rows.values()}

    # Check result controls first.  The renamed collapse/swap mutants route their
    # logical outputs directly, so their wrong nonempty origins must fire even
    # though registered candidate paths later become an honest API-limit HOLD.
    for family, width in OUTPUT_FAMILIES.items():
        for bit in range(width):
            atom_output = output_ports[family][bit]
            if not forward.get(atom_output):
                raise DetectorFired("%s[%d] has no live mapped fanout" % (family, bit))
            logical_output = top_outputs[family][bit]
            origins = _walk([logical_output], reverse) & all_owner_outputs
            if not origins:
                raise HoldError(
                    "%s_o[%d] crosses atom-internal or sequential logic, but Quartus 17 "
                    "exposes no exact internal data-dependency edge" % (family.lower(), bit)
                )
            if origins != {atom_output}:
                raise DetectorFired(
                    "%s_o[%d] result origin mismatch: expected %r, got %r"
                    % (family.lower(), bit, atom_output, sorted(origins))
                )

    top_inputs = {
        family: {
            bit: _boundary_port(graph, "%s_i[%d]" % (family.lower(), bit), "oport")
            for bit in range(width)
        }
        for family, width in INPUT_FAMILIES.items()
    }
    all_top_inputs = {ref for rows in top_inputs.values() for ref in rows.values()}
    for family, width in INPUT_FAMILIES.items():
        for bit in range(width):
            atom_input = input_ports[family][bit]
            origins = _walk([atom_input], reverse) & all_top_inputs
            if not origins:
                raise HoldError(
                    "%s[%d] operand crosses atom-internal or sequential logic without "
                    "an API-provided data-dependency edge" % (family, bit)
                )
            if origins != {top_inputs[family][bit]}:
                raise DetectorFired(
                    "%s[%d] operand origin mismatch: expected %r, got %r"
                    % (family, bit, top_inputs[family][bit], sorted(origins))
                )
    return {
        "owner": owner,
        "adjacency": "exact-cdb-fanin-and-fanout-only",
        "inventedInternalArcs": 0,
        "operandBitsChecked": sum(INPUT_FAMILIES.values()),
        "resultBitsChecked": sum(OUTPUT_FAMILIES.values()),
    }


def evaluate_synthetic(text: str) -> dict:
    """Exercise parser/detectors; synthetic evidence can never physically pass."""
    try:
        mechanics = check_lane_graph(parse_atom_tsv(text))
    except HoldError as exc:
        return {
            "status": "hold", "routeGateStatus": "hold",
            "mechanicalStatus": "unavailable", "synthetic": True,
            "physicalEvidence": False,
            "holds": [str(exc), "synthetic route fixture is nonphysical"],
        }
    except GateError as exc:
        return {
            "status": "hold", "routeGateStatus": "hold",
            "mechanicalStatus": "reject", "synthetic": True,
            "physicalEvidence": False, "detector": str(exc),
            "holds": ["synthetic route fixture is nonphysical"],
        }
    return {
        "status": "hold", "routeGateStatus": "hold", "mechanicalStatus": "pass",
        "synthetic": True, "physicalEvidence": False, "mechanics": mechanics,
        "holds": ["synthetic route fixture is nonphysical and cannot satisfy the CDB gate"],
    }


@dataclass
class DatabaseSnapshot:
    root: Path
    files: list[ArtifactSnapshot]
    digest: str

    @classmethod
    def capture(cls, root: Path, not_before_ns: int = 0) -> "DatabaseSnapshot":
        root = _canonical(root)
        try:
            map_check.require_direct_path(root, "fresh post-map database", "directory")
        except map_check.GateError as exc:
            raise HoldError("fresh post-map database is unavailable: %s" % exc) from exc
        paths: list[Path] = []
        for current, dirs, names in os.walk(root):
            current_path = Path(current)
            for directory in dirs:
                map_check.require_direct_path(
                    current_path / directory, "post-map database directory", "directory"
                )
            for name in names:
                paths.append(_canonical(current_path / name))
        if not paths:
            raise HoldError("fresh quartus_map produced no database files")
        snapshots: list[ArtifactSnapshot] = []
        try:
            for path in sorted(paths, key=lambda item: item.as_posix().lower()):
                snapshot = ArtifactSnapshot.capture(path, "post-map DB %s" % path.name)
                snapshots.append(snapshot)
                if snapshot.signature[3] < not_before_ns:
                    raise GateError(
                        "post-map database file predates checker-owned map invocation: %s"
                        % path.name
                    )
            rows = [
                {
                    "path": snapshot.path.relative_to(root).as_posix(),
                    "sha256": snapshot.sha256,
                    "size": len(snapshot.data),
                }
                for snapshot in snapshots
            ]
            digest = hashlib.sha256(
                json.dumps(rows, sort_keys=True, separators=(",", ":")).encode("ascii")
            ).hexdigest()
            return cls(root, snapshots, digest)
        except BaseException:
            for snapshot in snapshots:
                snapshot.close()
            raise

    def verify_identity(self) -> None:
        current = sorted(
            _canonical(Path(base) / name)
            for base, _, names in os.walk(self.root)
            for name in names
        )
        expected = sorted(snapshot.path for snapshot in self.files)
        if current != expected:
            raise GateError("post-map database file set changed during CDB capture")
        for snapshot in self.files:
            snapshot.verify_identity()

    def record(self) -> dict:
        return {
            "root": str(self.root),
            "sha256": self.digest,
            "files": [snapshot.record() for snapshot in self.files],
        }

    def close(self) -> None:
        for snapshot in reversed(self.files):
            snapshot.close()


ProcessRunner = Callable[[list[str], Path, BinaryIO], int]


def _default_runner(command: list[str], cwd: Path, log: BinaryIO) -> int:
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        stdin=subprocess.DEVNULL,
        stdout=log,
        stderr=subprocess.STDOUT,
        check=False,
    )
    return int(completed.returncode)


def _write_exclusive(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("xb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())


def _snapshot_output(path: Path, role: str, started_ns: int) -> ArtifactSnapshot:
    snapshot = ArtifactSnapshot.capture(path, role, missing_is_hold=True)
    if snapshot.signature[3] < started_ns:
        snapshot.close()
        raise GateError("%s predates its checker-owned invocation" % role)
    return snapshot


def _validate_map_outputs(
    bound: BoundInputs,
    variant: str,
    report: ArtifactSnapshot,
    summary: ArtifactSnapshot,
    started_ns: int,
    ended_ns: int,
) -> dict:
    text = report.text()
    parsed = map_check.parse_report(text, bound.config["top"])
    map_check.check_consistent_totals(parsed)
    summary_table = map_check.extract_table(text, map_check.SUMMARY_TABLE)
    settings_table = map_check.extract_table(text, map_check.SETTINGS_TABLE)
    status = map_check.unique_labeled_text(summary_table, "Analysis & Synthesis Status", map_check.SUMMARY_TABLE)
    version = map_check.unique_labeled_text(summary_table, "Quartus Prime Version", map_check.SUMMARY_TABLE)
    revision = map_check.unique_labeled_text(summary_table, "Revision Name", map_check.SUMMARY_TABLE)
    top = map_check.unique_labeled_text(summary_table, "Top-level Entity Name", map_check.SUMMARY_TABLE)
    device = map_check.unique_labeled_text(settings_table, "Device", map_check.SETTINGS_TABLE)
    if not status.startswith("Successful"):
        raise HoldError("fresh checker-owned quartus_map was not successful")
    if TOOL_VERSION not in version or "Lite Edition" not in version:
        raise GateError("fresh map report tool version is not canonical Quartus 17.0.2")
    if revision != bound.config["revision"] or top != bound.config["top"] or device != map_check.DEVICE:
        raise GateError("fresh map report revision/top/device differs from immutable inputs")
    summary_text = summary.text()
    if map_check.summary_field(summary_text, "Analysis & Synthesis Status") != status:
        raise GateError("fresh map summary status differs from report")
    if map_check.summary_field(summary_text, "Quartus Prime Version") != version:
        raise GateError("fresh map summary version differs from report")
    if map_check.summary_field(summary_text, "Revision Name") != revision:
        raise GateError("fresh map summary revision differs from report")
    if map_check.summary_field(summary_text, "Top-level Entity Name") != top:
        raise GateError("fresh map summary top differs from report")
    if map_check.parse_int(
        map_check.summary_field(summary_text, "Total DSP Blocks"), "fresh map summary DSP"
    ) != parsed.resource_dsp:
        raise GateError("fresh map summary DSP total differs from report")
    if variant == "two-primitives-mutant":
        if parsed.resource_dsp != 2 or parsed.independent_mode != 2:
            raise GateError("two-atom control did not map as two independent DSP owners")
    else:
        map_check.check_one_block(parsed)
    status_match = re.fullmatch(r"Successful\s*-\s*(.+)", status)
    if not status_match:
        raise GateError("fresh map completion timestamp is unavailable")
    try:
        completed = datetime.datetime.strptime(
            status_match.group(1), "%a %b %d %H:%M:%S %Y"
        ).astimezone()
    except ValueError as exc:
        raise GateError("fresh map completion timestamp is malformed") from exc
    if completed.timestamp() < started_ns / 1_000_000_000 - 1:
        raise GateError("map report completion predates checker-owned map invocation")
    if completed.timestamp() > ended_ns / 1_000_000_000 + 5:
        raise GateError("map report completion postdates checker-owned map invocation")
    return {"parsed": parsed.__dict__, "status": status, "version": version}


def _create_runtime_workspace(bound: BoundInputs) -> tuple[str, str, Path]:
    """Exclusively create a short, nonce-bound workspace; retry collisions."""
    invocation = bound.invocation
    if (
        invocation.get("freshRunTokenBytes") != 32
        or invocation.get("freshWorkspacePrefix") != RUNTIME_LEAF_PREFIX
        or invocation.get("runtimePathPolicy") != _runtime_path_policy()
    ):
        raise GateError("runtime workspace policy changed after immutable validation")
    workspace_parent = _canonical(invocation["workspaceParent"])
    expected_preflight = _quartus_path_preflight(
        workspace_parent, bound.config["revision"]
    )
    if invocation["quartusInternalPathPreflight"] != expected_preflight:
        raise GateError("runtime Quartus internal path preflight changed before launch")
    for _ in range(RUNTIME_CREATE_ATTEMPTS):
        run_token = secrets.token_urlsafe(invocation["freshRunTokenBytes"])
        leaf = _workspace_leaf(invocation["captureId"], run_token)
        workspace = _canonical(workspace_parent / leaf)
        if workspace.parent != workspace_parent or workspace.name != leaf:
            raise GateError("derived runtime workspace escaped its anchored parent")
        compiled_partition_artifact = _canonical(
            workspace
            / Path(COMPILED_PARTITION_DIR)
            / (bound.config["revision"] + COMPILED_PARTITION_SUFFIX)
        ).as_posix()
        if len(compiled_partition_artifact) > QUARTUS_INTERNAL_PATH_LIMIT:
            raise GateError(
                "actual Quartus compiled-partition artifact path exceeds %d "
                "characters: %s (%d)"
                % (
                    QUARTUS_INTERNAL_PATH_LIMIT,
                    compiled_partition_artifact,
                    len(compiled_partition_artifact),
                )
            )
        try:
            workspace.mkdir()
        except FileExistsError:
            continue
        try:
            map_check.require_direct_path(
                workspace, "fresh runtime workspace", "directory"
            )
        except map_check.GateError as exc:
            raise GateError("fresh runtime workspace is not direct: %s" % exc) from exc
        return run_token, leaf, workspace
    raise GateError(
        "could not exclusively create a unique short map/CDB workspace after %d attempts"
        % RUNTIME_CREATE_ATTEMPTS
    )


def _runtime_child(workspace: Path, relative_value: object, role: str) -> Path:
    if not isinstance(relative_value, str) or not relative_value:
        raise GateError("%s path is missing" % role)
    relative = Path(relative_value)
    if relative.is_absolute() or any(part in ("", ".", "..") for part in relative.parts):
        raise GateError("%s path is not a safe workspace-relative path" % role)
    child = _canonical(workspace / relative)
    try:
        child.relative_to(workspace)
    except ValueError as exc:
        raise GateError("%s path escaped the runtime workspace" % role) from exc
    return child


def _expand_runtime(
    bound: BoundInputs, workspace: Path
) -> tuple[dict[str, Path], list[str], list[str]]:
    invocation = bound.invocation
    outputs = {
        key: _runtime_child(workspace, value, "runtime output %s" % key)
        for key, value in invocation["runtimeOutputs"].items()
    }
    project = _runtime_child(
        workspace, Path(invocation["projectFileName"]).stem, "runtime project"
    ).as_posix()
    map_command = [
        project if value == "{project}" else value
        for value in invocation["mapCommandTemplate"]
    ]
    replacements = {
        "{project}": project,
        "{atomTsv}": outputs["atomTsv"].as_posix(),
        "{optionalAtomVo}": outputs["optionalAtomVo"].as_posix(),
    }
    cdb_command = [
        replacements.get(value, value) for value in invocation["cdbCommandTemplate"]
    ]
    return outputs, map_command, cdb_command


def _validate_physical_metadata(
    graph: AtomGraph,
    bound: BoundInputs,
    project: str,
    started_ns: int,
    ended_ns: int,
) -> None:
    required = {
        "artifact_class", "synthetic", "capture_id", "project", "revision",
        "netlist_type", "quartus_version", "generated_unix_seconds",
        "optional_vo_status", "connectivity_complete",
    }
    if required - set(graph.metadata):
        raise HoldError("CDB TSV lacks required process metadata")
    if graph.metadata["artifact_class"] != ARTIFACT_CLASS or _bool(
        graph.metadata["synthetic"], "CDB synthetic marker"
    ):
        raise GateError("CDB TSV is not genuine post-map output")
    if graph.metadata["capture_id"] != bound.invocation["captureId"]:
        raise GateError("CDB capture id differs from content-bound invocation")
    if graph.metadata["revision"] != bound.config["revision"]:
        raise GateError("CDB revision differs from fresh map revision")
    if graph.metadata["netlist_type"] != "map":
        raise GateError("CDB did not read the mapped database")
    if TOOL_VERSION not in graph.metadata["quartus_version"]:
        raise GateError("CDB runtime version is not %s" % TOOL_VERSION)
    if not _same_path(graph.metadata["project"], project):
        raise GateError("CDB project differs from fresh checker-owned map project")
    try:
        generated = int(graph.metadata["generated_unix_seconds"])
    except ValueError as exc:
        raise GateError("CDB generation timestamp is malformed") from exc
    if generated < started_ns // 1_000_000_000 - 1 or generated > ended_ns // 1_000_000_000 + 5:
        raise GateError("CDB TSV timestamp is outside this invocation")


def capture_one(
    bound: BoundInputs,
    variant: str,
    *,
    runner: ProcessRunner = _default_runner,
    after_cdb_snapshot_hook: Callable[[dict[str, Path]], None] | None = None,
) -> dict:
    workspace_parent = _canonical(bound.invocation["workspaceParent"])
    map_check.require_direct_path(
        workspace_parent, "runtime workspace parent", "directory"
    )
    run_token, workspace_leaf, workspace = _create_runtime_workspace(bound)
    outputs, map_command, cdb_command = _expand_runtime(bound, workspace)
    _write_exclusive(workspace / bound.config["qpfFile"], bound.snapshots["qpf"].data)
    _write_exclusive(workspace / bound.config["qsfFile"], bound.snapshots["qsf"].data)
    expected_initial = {
        _canonical(workspace / bound.config["qpfFile"]),
        _canonical(workspace / bound.config["qsfFile"]),
    }
    if {_canonical(path) for path in workspace.iterdir()} != expected_initial:
        raise GateError("fresh workspace was populated before checker-owned map launch")

    runtime_qpf = ArtifactSnapshot.capture(
        workspace / bound.config["qpfFile"], "runtime QPF"
    )
    try:
        runtime_qsf = ArtifactSnapshot.capture(
            workspace / bound.config["qsfFile"], "runtime QSF"
        )
    except BaseException:
        runtime_qpf.close()
        raise
    if runtime_qpf.data != bound.snapshots["qpf"].data or runtime_qsf.data != bound.snapshots["qsf"].data:
        runtime_qpf.close()
        runtime_qsf.close()
        raise GateError("runtime QPF/QSF differ from immutable generated bytes")
    output_snapshots: list[ArtifactSnapshot] = [runtime_qpf, runtime_qsf]
    database: DatabaseSnapshot | None = None
    try:
        map_started = time.time_ns()
        bound.verify_identities()  # all content-bound launch inputs stay locked
        runtime_qpf.verify_identity()
        runtime_qsf.verify_identity()
        bound.snapshots["quartusMap"].verify_identity()  # last check before launch
        with outputs["mapLog"].open("xb") as log:
            map_rc = runner(map_command, workspace, log)
        map_ended = time.time_ns()
        bound.verify_identities()
        runtime_qpf.verify_identity()
        runtime_qsf.verify_identity()
        map_log = _snapshot_output(outputs["mapLog"], "fresh quartus_map log", map_started)
        output_snapshots.append(map_log)
        if map_rc != 0:
            raise HoldError("checker-owned canonical quartus_map failed rc=%d" % map_rc)

        report = _snapshot_output(outputs["mapReport"], "fresh map report", map_started)
        summary = _snapshot_output(outputs["mapSummary"], "fresh map summary", map_started)
        output_snapshots += [report, summary]
        map_evidence = _validate_map_outputs(
            bound, variant, report, summary, map_started, map_ended
        )
        database = DatabaseSnapshot.capture(workspace / "db", map_started)
        route_dir = outputs["atomTsv"].parent
        route_dir.mkdir()

        # This is the exact join: CDB/script/input locks and every post-map DB
        # file are checked immediately before CDB starts, then held throughout.
        bound.verify_identities()
        database.verify_identity()
        bound.snapshots["script"].verify_identity()
        bound.snapshots["quartusCdb"].verify_identity()  # last checks before launch
        cdb_started = time.time_ns()
        with outputs["cdbLog"].open("xb") as log:
            cdb_rc = runner(cdb_command, workspace, log)
        cdb_ended = time.time_ns()
        bound.verify_identities()
        database.verify_identity()
        cdb_log = _snapshot_output(outputs["cdbLog"], "fresh quartus_cdb log", cdb_started)
        output_snapshots.append(cdb_log)
        if cdb_rc != 0:
            raise HoldError("canonical quartus_cdb could not expose complete connectivity rc=%d" % cdb_rc)
        tsv = _snapshot_output(outputs["atomTsv"], "fresh atom TSV", cdb_started)
        output_snapshots.append(tsv)
        vo: ArtifactSnapshot | None = None
        try:
            vo = _snapshot_output(
                outputs["optionalAtomVo"], "fresh optional atom VO", cdb_started
            )
        except HoldError as exc:
            if "missing" not in str(exc).lower():
                raise
        if vo is not None:
            output_snapshots.append(vo)

        # Positive TOCTOU control hook: a replacement after the sole read cannot
        # change parsed bytes and is independently rejected by path identity.
        if after_cdb_snapshot_hook is not None:
            after_cdb_snapshot_hook(outputs)
        for snapshot in output_snapshots:
            snapshot.verify_identity()
        database.verify_identity()
        graph = parse_atom_tsv_bytes(tsv.data)
        project = _canonical(workspace / Path(bound.config["qpfFile"]).stem).as_posix()
        _validate_physical_metadata(graph, bound, project, cdb_started, cdb_ended)
        vo_status = graph.metadata["optional_vo_status"]
        if vo_status not in ("captured", "unavailable"):
            raise GateError("CDB optional VO status is malformed")
        if vo_status == "captured" and vo is None:
            raise GateError("CDB claimed an optional VO that is missing")

        actual_compiled_partition_artifact = _canonical(
            workspace
            / Path(COMPILED_PARTITION_DIR)
            / (bound.config["revision"] + COMPILED_PARTITION_SUFFIX)
        ).as_posix()
        evidence = {
            "schemaVersion": 3,
            "gate": GATE,
            "artifactClass": ARTIFACT_CLASS,
            "physicalEvidence": True,
            "synthetic": False,
            "variant": variant,
            "contentWitness": bound.config["contentWitness"],
            "captureId": bound.invocation["captureId"],
            "freshRunToken": run_token,
            "freshRunTokenBytes": bound.invocation["freshRunTokenBytes"],
            "workspaceLeaf": workspace_leaf,
            "workspaceLeafBinding": {
                "leaf": workspace_leaf,
                "hash": "sha256",
                "domain": RUNTIME_HASH_DOMAIN,
                "inputs": {
                    "captureId": bound.invocation["captureId"],
                    "freshRunToken": run_token,
                },
                "digestBytes": RUNTIME_LEAF_DIGEST_BYTES,
            },
            "workspace": str(workspace),
            "quartusInternalPathPreflight": {
                **bound.invocation["quartusInternalPathPreflight"],
                "actualLongestPath": actual_compiled_partition_artifact,
                "actualLongestPathLength": len(actual_compiled_partition_artifact),
            },
            "mapInvocation": {
                "argv": map_command,
                "startedAtUnixNs": map_started,
                "completedAtUnixNs": map_ended,
                "executable": bound.snapshots["quartusMap"].record(),
                "log": map_log.record(),
                "report": report.record(),
                "summary": summary.record(),
                "result": map_evidence,
            },
            "postMapDatabase": database.record(),
            "cdbInvocation": {
                "argv": cdb_command,
                "startedAtUnixNs": cdb_started,
                "completedAtUnixNs": cdb_ended,
                "executable": bound.snapshots["quartusCdb"].record(),
                "script": bound.snapshots["script"].record(),
                "runtimeVersion": graph.metadata["quartus_version"],
                "log": cdb_log.record(),
            },
            "artifacts": {
                "atomTsv": tsv.record(),
                "optionalAtomVo": vo.record() if vo is not None else {
                    "path": str(outputs["optionalAtomVo"]), "available": False,
                },
            },
            "binding": {
                "anchor": bound.snapshots["anchor"].record(),
                "invocationNonce": bound.anchor["invocationNonce"],
                "manifest": bound.snapshots["manifest"].record(),
                "effectiveConfig": bound.snapshots["config"].record(),
                "routeInvocation": bound.snapshots["invocation"].record(),
                "qpf": bound.snapshots["qpf"].record(),
                "qsf": bound.snapshots["qsf"].record(),
                "sources": [snapshot.record() for snapshot in bound.sources],
            },
        }
        try:
            evidence["mechanics"] = check_lane_graph(graph)
            evidence["routeGateStatus"] = "pass"
        except DetectorFired as exc:
            evidence["routeGateStatus"] = "reject"
            evidence["detector"] = str(exc)
            receipt_bytes = _json_bytes(evidence)
            _write_exclusive(outputs["receipt"], receipt_bytes)
            evidence["receipt"] = {
                "path": str(outputs["receipt"]),
                "sha256": hashlib.sha256(receipt_bytes).hexdigest(),
            }
            raise DetectorFired(str(exc), evidence) from exc
        receipt_bytes = _json_bytes(evidence)
        _write_exclusive(outputs["receipt"], receipt_bytes)
        evidence["receipt"] = {
            "path": str(outputs["receipt"]),
            "sha256": hashlib.sha256(receipt_bytes).hexdigest(),
        }
        return evidence
    finally:
        if database is not None:
            database.close()
        for snapshot in reversed(output_snapshots):
            snapshot.close()


def overall_result(route_status: str, **extra: object) -> dict:
    result = {
        "status": "hold",
        "routeGateStatus": route_status,
        "encryptedVendorModelStatus": "hold",
        "productionMigration": "none",
        "productionDspSaving": 0,
        "holds": [ENCRYPTED_MODEL_HOLD],
    }
    result.update(extra)
    return result


def run_variant(bound: BoundInputs, variant: str, *, runner: ProcessRunner = _default_runner) -> dict:
    if variant in CONTROL_VARIANTS:
        try:
            capture_one(bound, variant, runner=runner)
        except DetectorFired as exc:
            return overall_result(
                "hold", variant=variant, physicalEvidence=True,
                positiveControl={
                    "kind": CONTROL_VARIANTS[variant], "status": "pass",
                    "detector": str(exc),
                },
                routeEvidence=exc.evidence,
            )
        except HoldError as exc:
            return overall_result(
                "hold", variant=variant, physicalEvidence=False,
                positiveControl={"kind": CONTROL_VARIANTS[variant], "status": "hold"},
                holds=[ENCRYPTED_MODEL_HOLD, str(exc)],
            )
        raise GateError("%s detector did not fire on genuine CDB evidence" % variant)
    try:
        evidence = capture_one(bound, variant, runner=runner)
    except HoldError as exc:
        return overall_result(
            "hold", variant=variant, physicalEvidence=False,
            holds=[ENCRYPTED_MODEL_HOLD, str(exc)],
        )
    return overall_result("pass", variant=variant, physicalEvidence=True, routeEvidence=evidence)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=sorted(ROUTE_VARIANTS), default="explicit")
    parser.add_argument("--effective-config", type=Path, required=True)
    parser.add_argument("--invocation-anchor", type=Path, required=True)
    parser.add_argument("--invocation-anchor-sha256", required=True)
    parser.add_argument("--invocation-nonce", required=True)
    parser.add_argument("--manifest-sha256", required=True)
    parser.add_argument("--receipt", type=Path, help="optional orchestration result copy")
    args = parser.parse_args(argv)
    bound: BoundInputs | None = None
    try:
        bound = acquire_bound_inputs(
            args.effective_config,
            args.invocation_anchor,
            args.invocation_anchor_sha256,
            args.invocation_nonce,
            args.manifest_sha256,
            args.variant,
        )
        result = run_variant(bound, args.variant)
        if args.receipt:
            result_bytes = _json_bytes(result)
            _write_exclusive(_canonical(args.receipt), result_bytes)
    except HoldError as exc:
        result = overall_result(
            "hold", variant=args.variant, physicalEvidence=False,
            holds=[ENCRYPTED_MODEL_HOLD, str(exc)],
        )
    except (OSError, UnicodeError, GateError, map_check.GateError) as exc:
        print("DUAL18_ATOM_ROUTE_REJECT: %s" % exc, file=sys.stderr)
        return 1
    finally:
        if bound is not None:
            bound.close()
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
