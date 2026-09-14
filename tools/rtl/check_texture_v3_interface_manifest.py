#!/usr/bin/env python3
"""Check Texture-V3 interface schema, bytes, provenance, and elaboration freshness."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
from typing import Mapping, Sequence

from texture_v3_interface_parser import (
    InterfaceManifestError,
    PRODUCTION_MANIFEST,
    PRODUCTION_SOURCE_CLOSURE,
    PRODUCTION_TOP,
    build_manifest_artifact,
    canonical_json_bytes,
    parse_parameter_arguments,
    validate_manifest_bytes,
)


def _first_difference(expected: object, actual: object, path: str = "root") -> str | None:
    if type(expected) is not type(actual):
        return f"{path}: expected type {type(expected).__name__}, found {type(actual).__name__}"
    if isinstance(expected, dict):
        expected_keys = set(expected)
        actual_keys = set(actual)  # type: ignore[arg-type]
        if expected_keys != actual_keys:
            return (
                f"{path}: keys differ; missing={sorted(expected_keys - actual_keys)}, "
                f"extra={sorted(actual_keys - expected_keys)}"
            )
        for key in sorted(expected):
            difference = _first_difference(
                expected[key], actual[key], f"{path}.{key}"  # type: ignore[index]
            )
            if difference:
                return difference
        return None
    if isinstance(expected, list):
        actual_list = actual  # type: ignore[assignment]
        if len(expected) != len(actual_list):
            return f"{path}: expected {len(expected)} rows, found {len(actual_list)}"
        for index, (expected_item, actual_item) in enumerate(zip(expected, actual_list)):
            difference = _first_difference(
                expected_item, actual_item, f"{path}[{index}]"
            )
            if difference:
                return difference
        return None
    if expected != actual:
        return f"{path}: expected {expected!r}, found {actual!r}"
    return None


def check_manifest(
    *,
    repo_root: Path,
    manifest_path: Path,
    top_module: str,
    source_paths: Sequence[str],
    parameter_overrides: Mapping[str, str] | None = None,
    verilator: str | None = None,
    production: bool | None = None,
) -> dict[str, object]:
    if not manifest_path.is_file():
        raise InterfaceManifestError(f"manifest is missing: {manifest_path}")
    raw = manifest_path.read_bytes()
    actual = validate_manifest_bytes(
        raw,
        expected_top=top_module,
        expected_source_closure=source_paths,
    )
    artifact = build_manifest_artifact(
        repo_root=repo_root,
        top_module=top_module,
        source_paths=source_paths,
        overrides=parameter_overrides,
        verilator=verilator,
        production=production,
    )
    expected = artifact.payload
    difference = _first_difference(expected, actual)
    if difference:
        raise InterfaceManifestError(
            "manifest is stale or disagrees with the independent source/elaboration query: "
            + difference
        )
    expected_raw = canonical_json_bytes(expected)
    if expected_raw != raw:
        # The object equality above should make this unreachable; retaining the
        # byte comparison makes canonical freshness an explicit checker gate.
        raise InterfaceManifestError("manifest object matches but canonical bytes differ")
    artifact.verify_live_unchanged()
    if manifest_path.read_bytes() != raw:
        raise InterfaceManifestError(
            f"manifest changed during source/elaboration check: {manifest_path}"
        )
    return actual


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--top-module", default=PRODUCTION_TOP)
    parser.add_argument(
        "--source",
        action="append",
        default=[],
        help="repo-relative source in exact compile order; repeat for the closure",
    )
    parser.add_argument(
        "-G",
        "--parameter",
        action="append",
        default=[],
        metavar="NAME=VALUE",
    )
    parser.add_argument("--verilator", help="absolute Verilator executable for the query")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo_root = args.repo_root.resolve()
    source_paths = args.source or (
        list(PRODUCTION_SOURCE_CLOSURE)
        if args.top_module == PRODUCTION_TOP
        else []
    )
    if not source_paths:
        print(
            "texture-v3 interface check failed: --source is required for a non-production fixture",
            file=sys.stderr,
        )
        return 1
    manifest_path = args.manifest
    if manifest_path is None:
        if args.top_module != PRODUCTION_TOP:
            print(
                "texture-v3 interface check failed: --manifest is required for a non-production fixture",
                file=sys.stderr,
            )
            return 1
        manifest_path = repo_root / Path(*PRODUCTION_MANIFEST.split("/"))
    elif not manifest_path.is_absolute():
        manifest_path = repo_root / manifest_path
    try:
        overrides = parse_parameter_arguments(args.parameter)
        payload = check_manifest(
            repo_root=repo_root,
            manifest_path=manifest_path,
            top_module=args.top_module,
            source_paths=source_paths,
            parameter_overrides=overrides,
            verilator=args.verilator,
            production=args.top_module == PRODUCTION_TOP,
        )
    except (OSError, InterfaceManifestError) as exc:
        print(f"texture-v3 interface check failed: {exc}", file=sys.stderr)
        return 1
    print(
        "texture-v3 interface check OK: "
        f"{payload['module']['name']} has {len(payload['parameters'])} parameters, "
        f"{len(payload['ports'])} ports, and {len(payload['source_closure'])} ordered sources"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
