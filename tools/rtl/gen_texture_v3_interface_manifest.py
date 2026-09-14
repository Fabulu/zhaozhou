#!/usr/bin/env python3
"""Generate the closed schema-v1 Texture-V3 interface manifest atomically."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
import tempfile
from typing import Callable, Sequence

from texture_v3_interface_parser import (
    InterfaceManifestError,
    PRODUCTION_MANIFEST,
    PRODUCTION_SOURCE_CLOSURE,
    PRODUCTION_TOP,
    build_manifest_artifact,
    canonical_json_bytes,
    parse_parameter_arguments,
)


def generate_manifest_bytes(
    *,
    repo_root: Path,
    top_module: str,
    source_paths: Sequence[str],
    parameter_overrides: dict[str, str] | None = None,
    verilator: str | None = None,
    production: bool | None = None,
) -> bytes:
    artifact = build_manifest_artifact(
        repo_root=repo_root,
        top_module=top_module,
        source_paths=source_paths,
        overrides=parameter_overrides,
        verilator=verilator,
        production=production,
    )
    raw = canonical_json_bytes(artifact.payload)
    artifact.verify_live_unchanged()
    return raw


def write_atomic(
    path: Path,
    raw: bytes,
    *,
    before_replace: Callable[[], None] | None = None,
) -> None:
    """Replace *path* only after every byte has been produced successfully."""
    parent = path.parent
    if not parent.is_dir():
        raise InterfaceManifestError(
            f"output parent does not exist; refusing a partial generation side effect: {parent}"
        )
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=path.name + ".",
            suffix=".tmp",
            dir=parent,
            delete=False,
        ) as stream:
            temporary_name = stream.name
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        if before_replace is not None:
            before_replace()
        os.replace(temporary_name, path)
        temporary_name = None
    finally:
        if temporary_name is not None:
            try:
                os.unlink(temporary_name)
            except FileNotFoundError:
                pass


def generate_to_path(
    *,
    repo_root: Path,
    output: Path,
    top_module: str,
    source_paths: Sequence[str],
    parameter_overrides: dict[str, str] | None = None,
    verilator: str | None = None,
    production: bool | None = None,
) -> bytes:
    # Deliberately finish source parse, elaboration, closure, quiet, schema, and
    # hash validation before opening anything beside the destination.
    artifact = build_manifest_artifact(
        repo_root=repo_root,
        top_module=top_module,
        source_paths=source_paths,
        overrides=parameter_overrides,
        verilator=verilator,
        production=production,
    )
    raw = canonical_json_bytes(artifact.payload)
    write_atomic(output, raw, before_replace=artifact.verify_live_unchanged)
    return raw


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
    )
    parser.add_argument("--top-module", default=PRODUCTION_TOP)
    parser.add_argument(
        "--source",
        action="append",
        default=[],
        help="repo-relative source in compile order; repeat for the complete closure",
    )
    parser.add_argument(
        "-G",
        "--parameter",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="override a selected parameter value",
    )
    parser.add_argument("--verilator", help="absolute Verilator executable for the query")
    parser.add_argument("--output", type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo_root = args.repo_root.resolve()
    source_paths = args.source or (
        list(PRODUCTION_SOURCE_CLOSURE)
        if args.top_module == PRODUCTION_TOP
        else []
    )
    output = args.output
    if output is None:
        if args.top_module != PRODUCTION_TOP:
            print(
                "texture-v3 interface generation failed: --output is required for a non-production fixture",
                file=sys.stderr,
            )
            return 1
        output = repo_root / Path(*PRODUCTION_MANIFEST.split("/"))
    elif not output.is_absolute():
        output = repo_root / output
    try:
        overrides = parse_parameter_arguments(args.parameter)
        raw = generate_to_path(
            repo_root=repo_root,
            output=output,
            top_module=args.top_module,
            source_paths=source_paths,
            parameter_overrides=overrides,
            verilator=args.verilator,
            production=args.top_module == PRODUCTION_TOP,
        )
    except (OSError, InterfaceManifestError) as exc:
        print(f"texture-v3 interface generation failed: {exc}", file=sys.stderr)
        return 1
    print(
        f"texture-v3 interface manifest generated: {output} ({len(raw)} canonical bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
