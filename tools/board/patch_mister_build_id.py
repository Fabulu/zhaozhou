#!/usr/bin/env python3
"""Make a copied pinned MiSTer build_id.tcl projectless."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

UPSTREAM_SHA256 = "148dc6a8124d36ac2898a45d085960cc05178a76d7297fde8877925fc0a74e88"
PATCHED_SHA256 = "e9a3daa3d507075abf214fefa115505a7669a14898800b728492f8a4393272c6"

PROJECT_OPEN_ORIGINAL = b'''set project_name [lindex $quartus(args) 1]\r\nset revision [lindex $quartus(args) 2]\r\n\r\nif {[project_exists $project_name]} {\r\n    if {[string equal "" $revision]} {\r\n        project_open $project_name -revision [get_current_revision $project_name]\r\n    } else {\r\n        project_open $project_name -revision $revision\r\n    }\r\n} else {\r\n    post_message -type error "Project $project_name does not exist"\r\n    exit\r\n}\r\n\r\nset device  [get_global_assignment -name DEVICE]\r\nset outpath [get_global_assignment -name PROJECT_OUTPUT_DIRECTORY]\r\n\r\nif [is_project_open] {\r\n    project_close\r\n}\r\n\r\ngenerateBuildID_Verilog\r\ngenerateCDF $revision $device $outpath\r\n'''

PROJECTLESS_SAFE = b'''if {[llength $quartus(args)] != 3} {\r\n    post_message -type error "Expected revision, device, and output-directory arguments"\r\n    exit 1\r\n}\r\n\r\nset revision [lindex $quartus(args) 0]\r\nset device   [lindex $quartus(args) 1]\r\nset outpath  [lindex $quartus(args) 2]\r\n\r\ngenerateBuildID_Verilog\r\ngenerateCDF $revision $device $outpath\r\n'''


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def patch_bytes(data: bytes) -> bytes:
    actual = sha256(data)
    if actual != UPSTREAM_SHA256:
        raise ValueError(
            "build_id input is not the pinned upstream blob: "
            f"expected {UPSTREAM_SHA256}, got {actual}"
        )
    count = data.count(PROJECT_OPEN_ORIGINAL)
    if count != 1:
        raise ValueError(
            f"build_id project-open anchor count is {count}, expected exactly 1"
        )
    patched = data.replace(PROJECT_OPEN_ORIGINAL, PROJECTLESS_SAFE, 1)
    actual_patched = sha256(patched)
    if actual_patched != PATCHED_SHA256:
        raise ValueError(
            "patched build_id digest drifted: "
            f"expected {PATCHED_SHA256}, got {actual_patched}"
        )
    return patched


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path, help="copied build-workspace build_id.tcl")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    try:
        before = args.path.read_bytes()
        after = patch_bytes(before)
        args.path.write_bytes(after)
    except (OSError, ValueError) as exc:
        print(f"SUPERSTATION_BUILD_ID_PATCH_FAIL: {exc}", file=sys.stderr)
        return 1
    result = {
        "status": "ok",
        "path": str(args.path),
        "upstreamSha256": sha256(before),
        "patchedSha256": sha256(after),
        "patch": "projectless-explicit-revision-device-output",
    }
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(f"SuperStation projectless build-ID patch applied: {result['patchedSha256']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
