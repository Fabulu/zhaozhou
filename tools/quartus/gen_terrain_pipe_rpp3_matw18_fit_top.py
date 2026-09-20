#!/usr/bin/env python3
"""Generate Packet-I's fixed real-pin/MISR G8B terrain wrapper.

Same shape as gen_raster_texture_v3_fit_top.py, deliberately: header plus the
template verbatim, and a manifest carrying the per-file sha256 of the whole
source closure. A generated file nobody regenerates is a stale file with a
reassuring provenance line at the top, so the freshness test re-runs this and
compares bytes.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile


REPO = Path(__file__).resolve().parents[2]
TEMPLATE = REPO / "tools/quartus/templates/zhao_terrain_pipe_rpp3_matw18_fit_top.sv.in"
WRAPPER = REPO / "fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.sv"
MANIFEST = REPO / "fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.manifest.json"
WRAPPER_REL = "fpga/rtl/generated/zhao_terrain_pipe_rpp3_matw18_fit_top.sv"

# The terrain pipe's closure, in dependency order: leaves before the shell.
# This is the same set tests/CMakeLists.txt composes as ZHAO_TERRAIN_PIPE_SV,
# written out here because the fit target names files and the test list names
# CMake variables, and those are two different acts.
SOURCE_CLOSURE = (
    "fpga/rtl/common/zhao_project_core.sv",
    "fpga/rtl/common/zhao_project_service.sv",
    "fpga/rtl/common/zhao_proj_subsystem.sv",
    "fpga/rtl/geometry/zhao_vertex_arena.sv",
    "fpga/rtl/terrain/zhao_terrain_wcache.sv",
    "fpga/rtl/terrain/zhao_terrain_patch_law_pkg.sv",
    "fpga/rtl/terrain/zhao_terrain_tess.sv",
    "fpga/rtl/terrain/zhao_terrain_group_seq.sv",
    "fpga/rtl/terrain/zhao_terrain_pipe.sv",
    WRAPPER_REL,
)

# What the fit exists to pin. The manifest records them so a receipt gate can
# check the ELABORATED values rather than trusting this comment.
FIT_TOP_PARAMETERS = {
    "ROWS_PER_PASS": 3,
    "MATW": 18,
}


def sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def canonical_json(payload: object) -> bytes:
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")


def validate_lf_input(path: Path, raw: bytes) -> None:
    try:
        raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise RuntimeError(f"G8B generator input is not UTF-8: {path}") from exc
    if b"\r" in raw:
        raise RuntimeError(
            f"G8B generator input is not checkout-stable LF text: {path}"
        )


def read_inputs() -> dict[str, bytes]:
    paths = [Path(__file__).resolve(), TEMPLATE]
    paths.extend(REPO / path for path in SOURCE_CLOSURE[:-1])
    snapshots: dict[str, bytes] = {}
    for path in paths:
        if not path.is_file():
            raise RuntimeError(f"required G8B generator input is missing: {path}")
        raw = path.read_bytes()
        validate_lf_input(path, raw)
        snapshots[str(path)] = raw
    return snapshots


def build_outputs(snapshots: dict[str, bytes]) -> tuple[bytes, bytes]:
    generator_path = str(Path(__file__).resolve())
    template_path = str(TEMPLATE)
    generator_hash = sha256(snapshots[generator_path])
    template_hash = sha256(snapshots[template_path])
    header = (
        "// GENERATED FILE -- DO NOT EDIT.\n"
        "// Generator: tools/quartus/gen_terrain_pipe_rpp3_matw18_fit_top.py\n"
        f"// generator-sha256: {generator_hash}\n"
        f"// template-sha256: {template_hash}\n"
        "// manifest: fpga/rtl/generated/"
        "zhao_terrain_pipe_rpp3_matw18_fit_top.manifest.json\n"
        "// Parameter witness: u_terrain_pipe sets ROWS_PER_PASS=3 and MATW=18\n"
        "// as LITERALS. The G8B target must not inherit either from a module\n"
        "// default, a fit-target comment or a runtime convention.\n"
        "// Characterization traffic is legal and deterministic; this is not a\n"
        "// shell or board top.\n\n"
    ).encode("utf-8")
    wrapper = header + snapshots[template_path]
    if not wrapper.endswith(b"\n"):
        wrapper += b"\n"
    wrapper_hash = sha256(wrapper)

    closure = []
    for ordinal, relative in enumerate(SOURCE_CLOSURE):
        if relative == WRAPPER_REL:
            digest = wrapper_hash
        else:
            digest = sha256(snapshots[str(REPO / relative)])
        closure.append({"ordinal": ordinal, "path": relative, "sha256": digest})

    manifest = {
        "schema_id": "zhao.g8b.fit_top",
        "schema_version": 1,
        "module": "zhao_terrain_pipe_rpp3_matw18_fit_top",
        "fit_top_parameters": FIT_TOP_PARAMETERS,
        "external_ports": [
            {"name": "clk", "direction": "input", "bit_width": 1, "ordinal": 0},
            {"name": "rst_n", "direction": "input", "bit_width": 1, "ordinal": 1},
            {"name": "fit_signature_o", "direction": "output",
             "bit_width": 8, "ordinal": 2},
            {"name": "fit_epoch_o", "direction": "output",
             "bit_width": 8, "ordinal": 3},
        ],
        "hashes": {
            "generated_rtl_sha256": wrapper_hash,
            "generator_sha256": generator_hash,
            "template_sha256": template_hash,
        },
        "source_closure": closure,
        "traffic_profile": {
            "view_masks": ["01", "10", "11"],
            "receipt_workload": "11",
            "config_writes": 36,
            "memory_response_latency_clocks": 1,
            "distinct_view_scales": [256, 192],
            "distinct_view_extents": [[320, 240], [256, 200]],
        },
        "limitations": [
            "Subsystem-only terrain characterization; not production world "
            "throughput.",
            "ALM/DSP/M10K/Fmax are reported values for THIS wrapper only; no "
            "saving relative to the ambiguous raw or default-MATW target is "
            "assumed.",
            "A refused product word would mean the projector never loaded its "
            "matrix; the wrapper asserts mat_refused_o == 0 in simulation.",
        ],
    }
    return wrapper, canonical_json(manifest)


def atomic_write(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=str(path.parent))
    try:
        with os.fdopen(handle, "wb") as stream:
            stream.write(payload)
        os.replace(temporary, path)
    except BaseException:
        if os.path.exists(temporary):
            os.unlink(temporary)
        raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check", action="store_true",
        help="verify the committed wrapper/manifest match this generator")
    args = parser.parse_args(argv)

    snapshots = read_inputs()
    wrapper, manifest = build_outputs(snapshots)

    if args.check:
        stale = []
        for path, expected in ((WRAPPER, wrapper), (MANIFEST, manifest)):
            if not path.is_file():
                stale.append(f"{path} is missing")
            elif path.read_bytes() != expected:
                stale.append(f"{path} does not match the generator")
        if stale:
            for line in stale:
                print(f"G8B generation failed: {line}")
            return 1
        print(f"fresh: G8B wrapper/manifest match generator "
              f"({len(SOURCE_CLOSURE)} sources)")
        return 0

    atomic_write(WRAPPER, wrapper)
    atomic_write(MANIFEST, manifest)
    print(f"generated {WRAPPER.relative_to(REPO)} and "
          f"{MANIFEST.relative_to(REPO)} ({len(SOURCE_CLOSURE)} sources)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
