#!/usr/bin/env python3
"""Independent Verilator-JSON census for the generated shell-fit instrument.

Unlike :mod:`shell_ports`, this module never reads SystemVerilog declarations.
It consumes Verilator's elaborated AST, follows dtype references, and compares
primary-I/O names, directions, unpacked shapes, and elaborated ``$bits`` totals.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import Iterable, Mapping, Sequence

from shell_ports import ModuleDeclaration, ShellPortError


@dataclass(frozen=True)
class ElaboratedPort:
    ordinal: int
    name: str
    direction: str
    bit_width: int
    element_width: int
    unpacked_ranges: tuple[tuple[int, int], ...]
    signed: bool
    dtype_kind: str
    packed_ranges: tuple[tuple[int, int], ...] = ()


_SKIP_CHILDREN = {
    "type",
    "name",
    "addr",
    "loc",
    "dtypep",
    "varp",
    "taskp",
    "refDTypep",
    "typedefp",
    "classOrPackagep",
    "varScopep",
    "sensIfacep",
}


def _children(node: Mapping[str, object]) -> Iterable[Mapping[str, object]]:
    for key, value in node.items():
        if key in _SKIP_CHILDREN:
            continue
        if isinstance(value, list):
            for child in value:
                if isinstance(child, dict):
                    yield child


def _walk(node: Mapping[str, object]) -> Iterable[Mapping[str, object]]:
    yield node
    for child in _children(node):
        yield from _walk(child)


def _range(text: object) -> tuple[int, int] | None:
    if not isinstance(text, str):
        return None
    match = re.fullmatch(r"\[?(-?\d+):(-?\d+)\]?", text.strip())
    if not match:
        return None
    return int(match.group(1)), int(match.group(2))


class _Types:
    def __init__(self, root: Mapping[str, object]):
        self.by_addr: dict[str, Mapping[str, object]] = {}
        for node in _walk(root):
            address = node.get("addr")
            kind = node.get("type")
            if isinstance(address, str) and isinstance(kind, str) and kind.endswith("DTYPE"):
                self.by_addr[address] = node
        self.cache: dict[
            str,
            tuple[
                int,
                int,
                tuple[tuple[int, int], ...],
                tuple[tuple[int, int], ...],
                bool,
                str,
            ],
        ] = {}

    def info(
        self, address: object, depth: int = 0
    ) -> tuple[
        int,
        int,
        tuple[tuple[int, int], ...],
        tuple[tuple[int, int], ...],
        bool,
        str,
    ]:
        """Return total/element widths, packed/unpacked ranges, signedness, kind."""
        if not isinstance(address, str) or depth > 32:
            raise ShellPortError(f"unresolved Verilator dtype reference {address!r}")
        if address in self.cache:
            return self.cache[address]
        node = self.by_addr.get(address)
        if node is None:
            raise ShellPortError(f"Verilator dtype address {address!r} is absent")
        kind = str(node.get("type"))
        signed = bool(node.get("signed"))
        if kind == "BASICDTYPE":
            declared = _range(node.get("range"))
            width = abs(declared[0] - declared[1]) + 1 if declared else 1
            result = (width, width, (declared,) if declared else (), (), signed, kind)
        elif kind in {"REFDTYPE", "MEMBERDTYPE", "ENUMDTYPE", "TYPEDEFDTYPE"}:
            target = node.get("refDTypep") or node.get("dtypep")
            total, element, packed, unpacked, target_signed, target_kind = self.info(
                target, depth + 1
            )
            # A named typedef is an opaque declaration-level type. Its intrinsic
            # bit layout contributes width/signedness, but not explicit dimensions.
            surface_packed = () if kind in {"REFDTYPE", "TYPEDEFDTYPE"} else packed
            result = (
                total,
                element,
                surface_packed,
                unpacked,
                signed or target_signed,
                target_kind,
            )
        elif kind == "STRUCTDTYPE":
            total = 0
            members = node.get("membersp")
            if not isinstance(members, list) or not members:
                raise ShellPortError(f"packed struct dtype {address!r} has no members")
            for member in members:
                if not isinstance(member, dict):
                    continue
                member_ref = member.get("refDTypep") or member.get("dtypep")
                member_total, _element, _packed, unpacked, _signed, _kind = self.info(
                    member_ref, depth + 1
                )
                if unpacked:
                    raise ShellPortError("unpacked member encountered in packed port struct")
                total += member_total
            result = (total, total, (), (), signed, kind)
        elif kind == "UNPACKARRAYDTYPE":
            declared = _range(node.get("declRange"))
            if declared is None:
                raise ShellPortError(
                    f"unresolved Verilator unpacked range {node.get('declRange')!r}"
                )
            target = node.get("refDTypep") or node.get("dtypep")
            sub_total, sub_element, packed, sub_ranges, sub_signed, sub_kind = self.info(
                target, depth + 1
            )
            count = abs(declared[0] - declared[1]) + 1
            result = (
                count * sub_total,
                sub_element,
                packed,
                (declared,) + sub_ranges,
                signed or sub_signed,
                sub_kind,
            )
        elif kind == "PACKARRAYDTYPE":
            declared = _range(node.get("declRange"))
            if declared is None:
                raise ShellPortError(
                    f"unresolved Verilator packed range {node.get('declRange')!r}"
                )
            target = node.get("refDTypep") or node.get("dtypep")
            sub_total, _sub_element, sub_packed, sub_ranges, sub_signed, sub_kind = self.info(
                target, depth + 1
            )
            count = abs(declared[0] - declared[1]) + 1
            total = count * sub_total
            result = (
                total,
                total,
                (declared,) + sub_packed,
                sub_ranges,
                signed or sub_signed,
                sub_kind,
            )
        else:
            raise ShellPortError(f"unsupported Verilator dtype kind {kind!r}")
        self.cache[address] = result
        return result


def parse_elaborated_ports(
    root: Mapping[str, object], module_name: str
) -> tuple[ElaboratedPort, ...]:
    modules = [
        node
        for node in _walk(root)
        if node.get("type") == "MODULE" and node.get("name") == module_name
    ]
    if len(modules) != 1:
        raise ShellPortError(
            f"expected one elaborated MODULE {module_name!r}, found {len(modules)}"
        )
    types = _Types(root)
    ports: list[ElaboratedPort] = []
    for node in _walk(modules[0]):
        if node.get("type") != "VAR" or not node.get("isPrimaryIO"):
            continue
        name = node.get("name")
        direction_raw = node.get("direction")
        if not isinstance(name, str) or direction_raw not in {"INPUT", "OUTPUT", "INOUT"}:
            raise ShellPortError(f"malformed elaborated primary I/O node {node!r}")
        total, element, packed, unpacked, signed, kind = types.info(node.get("dtypep"))
        ports.append(
            ElaboratedPort(
                ordinal=len(ports),
                name=name,
                direction=str(direction_raw).lower(),
                bit_width=total,
                element_width=element,
                unpacked_ranges=unpacked,
                signed=signed,
                dtype_kind=kind,
                packed_ranges=packed,
            )
        )
    if not ports:
        raise ShellPortError(f"elaborated module {module_name!r} has no primary I/O")
    return tuple(ports)


def compare_declaration_to_elaboration(
    declaration: ModuleDeclaration, elaborated: Sequence[ElaboratedPort]
) -> None:
    errors: list[str] = []
    declared_names = [port.name for port in declaration.ports]
    elaborated_names = [port.name for port in elaborated]
    missing = [name for name in declared_names if name not in set(elaborated_names)]
    extra = [name for name in elaborated_names if name not in set(declared_names)]
    if missing:
        errors.append("elaboration is missing declared ports: " + ", ".join(missing))
    if extra:
        errors.append("elaboration has undeclared ports: " + ", ".join(extra))
    if not missing and not extra and declared_names != elaborated_names:
        errors.append("elaborated primary-I/O order differs from declaration order")
    by_name = {port.name: port for port in elaborated}
    for port in declaration.ports:
        other = by_name.get(port.name)
        if other is None:
            continue
        if other.direction != port.direction:
            errors.append(
                f"port {port.name!r} direction: declaration={port.direction}, "
                f"elaboration={other.direction}"
            )
        if other.bit_width != port.bit_width:
            errors.append(
                f"port {port.name!r} $bits: declaration={port.bit_width}, "
                f"elaboration={other.bit_width}"
            )
        if other.element_width != port.element_width:
            errors.append(
                f"port {port.name!r} element width: declaration={port.element_width}, "
                f"elaboration={other.element_width}"
            )
        if other.signed != port.signed:
            errors.append(
                f"port {port.name!r} signedness: declaration={port.signed}, "
                f"elaboration={other.signed}"
            )
        expected_packed = tuple((dim.left, dim.right) for dim in port.packed_dimensions)
        if other.packed_ranges != expected_packed:
            errors.append(
                f"port {port.name!r} packed shape: declaration={expected_packed}, "
                f"elaboration={other.packed_ranges}"
            )
        expected_ranges = tuple((dim.left, dim.right) for dim in port.unpacked_dimensions)
        if other.unpacked_ranges != expected_ranges:
            errors.append(
                f"port {port.name!r} unpacked shape: declaration={expected_ranges}, "
                f"elaboration={other.unpacked_ranges}"
            )
    if errors:
        raise ShellPortError("; ".join(errors))


def load_tree(path: Path) -> Mapping[str, object]:
    with path.open(encoding="utf-8") as handle:
        root = json.load(handle)
    if not isinstance(root, dict):
        raise ShellPortError(f"Verilator tree {path} does not contain an object root")
    return root


def elaborate(
    *,
    repo: Path,
    top: str,
    sources: Sequence[str],
    out_dir: Path,
    verilator: str,
) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    command = [
        verilator,
        "--json-only",
        "--bbox-unsup",
        "-Wno-fatal",
        "--top-module",
        top,
        "--Mdir",
        str(out_dir),
        *sources,
    ]
    environment = os.environ.copy()
    completed = subprocess.run(
        command,
        cwd=repo,
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    tree = out_dir / f"V{top}.tree.json"
    if completed.returncode or not tree.is_file():
        raise ShellPortError(
            f"Verilator elaboration failed ({completed.returncode}):\n{completed.stdout[-6000:]}"
        )
    return tree


EXPECTED_SHELL_CENSUS = {
    "port_count": 154,
    "input_ports": 59,
    "output_ports": 95,
    "input_bits": 1457,
    "output_bits": 1929,
    "total_bits": 3386,
}
EXPECTED_WRAPPER_CENSUS = {
    "port_count": 6,
    "input_ports": 4,
    "output_ports": 2,
    "input_bits": 4,
    "output_bits": 6,
    "total_bits": 10,
}


def port_census(ports: Sequence[ElaboratedPort]) -> dict[str, int]:
    input_ports = [port for port in ports if port.direction == "input"]
    output_ports = [port for port in ports if port.direction == "output"]
    inout_ports = [port for port in ports if port.direction == "inout"]
    if inout_ports:
        raise ShellPortError(
            "elaboration contains unsupported inout ports: "
            + ", ".join(port.name for port in inout_ports)
        )
    return {
        "port_count": len(ports),
        "input_ports": len(input_ports),
        "output_ports": len(output_ports),
        "input_bits": sum(port.bit_width for port in input_ports),
        "output_bits": sum(port.bit_width for port in output_ports),
        "total_bits": sum(port.bit_width for port in ports),
    }


def _manifest_rows(payload: Mapping[str, object], key: str) -> list[Mapping[str, object]]:
    raw = payload.get(key)
    if not isinstance(raw, list) or not raw:
        raise ShellPortError(f"manifest {key!r} must be a non-empty array")
    if any(not isinstance(row, dict) for row in raw):
        raise ShellPortError(f"manifest {key!r} contains a non-object row")
    return raw


def compare_manifest_to_elaboration(
    *,
    elaborated: Sequence[ElaboratedPort],
    rows: Sequence[Mapping[str, object]],
    width_key: str,
    label: str,
) -> None:
    errors: list[str] = []
    expected_names = [row.get("name") for row in rows]
    if any(not isinstance(name, str) for name in expected_names):
        raise ShellPortError(f"{label} manifest contains an invalid port name")
    if len(set(expected_names)) != len(expected_names):
        raise ShellPortError(f"{label} manifest contains duplicate port names")
    actual_names = [port.name for port in elaborated]
    missing = [name for name in expected_names if name not in set(actual_names)]
    extra = [name for name in actual_names if name not in set(expected_names)]
    if missing:
        errors.append(f"elaboration is missing manifest ports: {missing}")
    if extra:
        errors.append(f"elaboration has ports absent from manifest: {extra}")
    if not missing and not extra and expected_names != actual_names:
        errors.append("elaborated primary-I/O order differs from manifest order")

    by_name = {port.name: port for port in elaborated}
    for ordinal, row in enumerate(rows):
        name = row.get("name")
        if not isinstance(name, str) or name not in by_name:
            continue
        port = by_name[name]
        direction = row.get("direction")
        width = row.get(width_key)
        if direction != port.direction:
            errors.append(
                f"port {name!r} direction: manifest={direction!r}, "
                f"elaboration={port.direction!r}"
            )
        if not isinstance(width, int) or isinstance(width, bool):
            errors.append(f"port {name!r} has invalid manifest {width_key}={width!r}")
        elif width != port.bit_width:
            errors.append(
                f"port {name!r} $bits: manifest={width}, elaboration={port.bit_width}"
            )
        manifest_ordinal = row.get("ordinal")
        if manifest_ordinal is not None and manifest_ordinal != ordinal:
            errors.append(
                f"port {name!r} ordinal: manifest={manifest_ordinal!r}, row={ordinal}"
            )
        if "element_width" in row and row.get("element_width") != port.element_width:
            errors.append(
                f"port {name!r} element width: manifest={row.get('element_width')!r}, "
                f"elaboration={port.element_width}"
            )
        if row.get("signed") is not port.signed:
            errors.append(
                f"port {name!r} signedness: manifest={row.get('signed')!r}, "
                f"elaboration={port.signed}"
            )
        dimensions = row.get("packed_dimensions")
        if not isinstance(dimensions, list) or any(
            not isinstance(dimension, dict) for dimension in dimensions
        ):
            errors.append(f"port {name!r} has malformed packed dimensions")
        else:
            expected_packed = tuple(
                (dimension.get("left"), dimension.get("right"))
                for dimension in dimensions
            )
            if expected_packed != port.packed_ranges:
                errors.append(
                    f"port {name!r} packed shape: manifest={expected_packed}, "
                    f"elaboration={port.packed_ranges}"
                )
        if "unpacked_dimensions" in row:
            dimensions = row.get("unpacked_dimensions")
            if not isinstance(dimensions, list) or any(
                not isinstance(dimension, dict) for dimension in dimensions
            ):
                errors.append(f"port {name!r} has malformed unpacked dimensions")
            else:
                expected_ranges = tuple(
                    (dimension.get("left"), dimension.get("right"))
                    for dimension in dimensions
                )
                if expected_ranges != port.unpacked_ranges:
                    errors.append(
                        f"port {name!r} unpacked shape: manifest={expected_ranges}, "
                        f"elaboration={port.unpacked_ranges}"
                    )
    if errors:
        raise ShellPortError(f"{label}: " + "; ".join(errors))


def _require_census(
    *, actual: Mapping[str, int], expected: Mapping[str, object], label: str
) -> None:
    differences = [
        f"{key}: expected={expected.get(key)!r}, elaborated={actual[key]}"
        for key in actual
        if expected.get(key) != actual[key]
    ]
    if differences:
        raise ShellPortError(f"{label} census differs: " + "; ".join(differences))


def check_shell_fit(
    *,
    repo: Path,
    sources: Sequence[str],
    wrapper: Path,
    manifest_path: Path,
    work_dir: Path,
    verilator: str,
) -> tuple[dict[str, int], dict[str, int]]:
    try:
        manifest = load_tree(manifest_path)
    except json.JSONDecodeError as exc:
        raise ShellPortError(f"invalid shell-fit manifest JSON: {exc}") from exc
    accounting = manifest.get("accounting")
    if not isinstance(accounting, dict):
        raise ShellPortError("manifest accounting must be an object")
    for key, expected in EXPECTED_SHELL_CENSUS.items():
        if accounting.get(key) != expected:
            raise ShellPortError(
                f"manifest accounting {key}: expected={expected}, "
                f"actual={accounting.get(key)!r}"
            )

    shell_tree = elaborate(
        repo=repo,
        top="zhao_shell_top",
        sources=sources,
        out_dir=work_dir / "shell",
        verilator=verilator,
    )
    shell_ports = parse_elaborated_ports(load_tree(shell_tree), "zhao_shell_top")
    shell_census = port_census(shell_ports)
    _require_census(
        actual=shell_census,
        expected=EXPECTED_SHELL_CENSUS,
        label="zhao_shell_top",
    )
    compare_manifest_to_elaboration(
        elaborated=shell_ports,
        rows=_manifest_rows(manifest, "ports"),
        width_key="bit_width",
        label="zhao_shell_top manifest",
    )

    wrapper_source = str(wrapper)
    if wrapper_source in sources:
        raise ShellPortError("generated wrapper occurs in the shell source pool")
    wrapper_tree = elaborate(
        repo=repo,
        top="zhao_shell_fit_top",
        sources=[*sources, wrapper_source],
        out_dir=work_dir / "wrapper",
        verilator=verilator,
    )
    wrapper_ports = parse_elaborated_ports(load_tree(wrapper_tree), "zhao_shell_fit_top")
    wrapper_census = port_census(wrapper_ports)
    _require_census(
        actual=wrapper_census,
        expected=EXPECTED_WRAPPER_CENSUS,
        label="zhao_shell_fit_top",
    )
    compare_manifest_to_elaboration(
        elaborated=wrapper_ports,
        rows=_manifest_rows(manifest, "external_ports"),
        width_key="bits",
        label="zhao_shell_fit_top external manifest",
    )
    return shell_census, wrapper_census


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--tree", type=Path, help="inspect an existing Verilator .tree.json")
    mode.add_argument(
        "--check-shell-fit",
        action="store_true",
        help="elaborate and verify the real shell and generated fit wrapper",
    )
    parser.add_argument("--module", help="module to inspect with --tree")
    parser.add_argument("--repo-root", type=Path)
    parser.add_argument("--sources", nargs="+")
    parser.add_argument("--wrapper", type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--work-dir", type=Path)
    parser.add_argument("--verilator")
    return parser


def _resolved(path: Path, repo: Path) -> Path:
    return path.resolve() if path.is_absolute() else (repo / path).resolve()


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        if args.tree is not None:
            if not args.module:
                raise ShellPortError("--module is required with --tree")
            ports = parse_elaborated_ports(load_tree(args.tree), args.module)
            payload = {
                "module": args.module,
                **port_census(ports),
                "ports": [port.__dict__ for port in ports],
            }
            print(json.dumps(payload, indent=2, sort_keys=True))
            return 0

        missing = [
            option
            for option, value in (
                ("--repo-root", args.repo_root),
                ("--sources", args.sources),
                ("--wrapper", args.wrapper),
                ("--manifest", args.manifest),
                ("--work-dir", args.work_dir),
                ("--verilator", args.verilator),
            )
            if not value
        ]
        if missing:
            raise ShellPortError(
                "--check-shell-fit requires " + ", ".join(missing)
            )
        repo = args.repo_root.resolve()
        shell_census, wrapper_census = check_shell_fit(
            repo=repo,
            sources=args.sources,
            wrapper=_resolved(args.wrapper, repo),
            manifest_path=_resolved(args.manifest, repo),
            work_dir=_resolved(args.work_dir, repo),
            verilator=args.verilator,
        )
        print(
            "shell-elab-census: "
            f"shell={shell_census['port_count']} ports/{shell_census['total_bits']} bits; "
            f"wrapper={wrapper_census['port_count']} ports/{wrapper_census['total_bits']} bits"
        )
    except (OSError, json.JSONDecodeError, ShellPortError) as exc:
        print(f"shell-elab-census: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
