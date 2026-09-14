#!/usr/bin/env python3
"""Generate Packet-F's fixed real-pin/MISR G8A raster/texture wrapper."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile


REPO = Path(__file__).resolve().parents[2]
TEMPLATE = REPO / "tools/quartus/templates/zhao_raster_texture_v3_fit_top.sv.in"
WRAPPER = REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv"
MANIFEST = REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.manifest.json"
WRAPPER_REL = "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.sv"

SOURCE_CLOSURE = (
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_render_texture_pkg.sv",
    "fpga/rtl/common/zhao_skid2.sv",
    "fpga/rtl/field/zhao_field_rcp24_rom.sv",
    "fpga/rtl/raster/zhao_raster_ticketq.sv",
    "fpga/rtl/raster/zhao_raster_ticketq_rh.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_mul.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_v4.sv",
    "fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_mod255.sv",
    "fpga/rtl/texture/zhao_texture_aux_div6.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv",
    "fpga/rtl/texture/zhao_texture_mosaic_v2.sv",
    "fpga/rtl/texture/zhao_texture_palette_res_v2.sv",
    "fpga/rtl/texture/zhao_texture_tmu_plan_v2.sv",
    "fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_v3bank.sv",
    "fpga/rtl/texture/zhao_texture_v3rq.sv",
    "fpga/rtl/texture/zhao_texture_v3own.sv",
    "fpga/rtl/texture/zhao_texture_metajoin_v2.sv",
    "fpga/rtl/texture/zhao_texture_uv_join_v2.sv",
    "fpga/rtl/texture/zhao_texture_early_desc_v2.sv",
    "fpga/rtl/texture/zhao_texture_frag_expand_v2.sv",
    "fpga/rtl/texture/zhao_texture_binding_resolver_v2.sv",
    "fpga/rtl/texture/zhao_texture_rsp_dispatch_v2.sv",
    "fpga/rtl/texture/zhao_texture_aux_pipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_material_combine_v3.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
    "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv",
    "fpga/rtl/raster/zhao_raster_fill.sv",
    "fpga/rtl/raster/zhao_raster_edgewalk.sv",
    "fpga/rtl/raster/zhao_raster_attrdiv_v2.sv",
    "fpga/rtl/raster/zhao_raster_attrgrad_v2.sv",
    "fpga/rtl/raster/zhao_raster_earlyz.sv",
    "fpga/rtl/raster/zhao_raster_blend_prod.sv",
    "fpga/rtl/raster/zhao_raster_blend_fin.sv",
    "fpga/rtl/raster/zhao_raster_fragment.sv",
    "fpga/rtl/raster/zhao_raster_tilestore.sv",
    "fpga/rtl/raster/zhao_raster_div255.sv",
    "fpga/rtl/raster/zhao_raster_quant.sv",
    "fpga/rtl/raster/zhao_raster_resolve.sv",
    "fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv",
    WRAPPER_REL,
)


def sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def canonical_json(payload: object) -> bytes:
    return (json.dumps(payload, indent=2, sort_keys=True) + "\n").encode("utf-8")


def read_inputs() -> dict[str, bytes]:
    paths = [Path(__file__).resolve(), TEMPLATE]
    paths.extend(REPO / path for path in SOURCE_CLOSURE[:-1])
    snapshots: dict[str, bytes] = {}
    for path in paths:
        if not path.is_file():
            raise RuntimeError(f"required G8A generator input is missing: {path}")
        snapshots[str(path)] = path.read_bytes()
    return snapshots


def build_outputs(snapshots: dict[str, bytes]) -> tuple[bytes, bytes]:
    generator_path = str(Path(__file__).resolve())
    template_path = str(TEMPLATE)
    generator_hash = sha256(snapshots[generator_path])
    template_hash = sha256(snapshots[template_path])
    header = (
        "// GENERATED FILE -- DO NOT EDIT.\n"
        "// Generator: tools/quartus/gen_raster_texture_v3_fit_top.py\n"
        f"// generator-sha256: {generator_hash}\n"
        f"// template-sha256: {template_hash}\n"
        "// manifest: fpga/rtl/generated/zhao_raster_texture_v3_fit_top.manifest.json\n"
        "// Product witness: u_tile.u_texture_stage explicitly sets MIGRATION_SHADOWS=1'b0.\n"
        "// Characterization traffic is legal and deterministic; this is not a shell or board top.\n\n"
    ).encode("utf-8")
    wrapper = header + snapshots[template_path]
    if not wrapper.endswith(b"\n"):
        wrapper += b"\n"
    wrapper_hash = sha256(wrapper)

    closure = []
    for ordinal, relative in enumerate(SOURCE_CLOSURE):
        digest = wrapper_hash if relative == WRAPPER_REL else sha256(
            snapshots[str(REPO / relative)]
        )
        closure.append({"ordinal": ordinal, "path": relative, "sha256": digest})

    payload = {
        "external_ports": [
            {"bit_width": 1, "direction": "input", "name": "clk", "ordinal": 0},
            {"bit_width": 1, "direction": "input", "name": "rst_n", "ordinal": 1},
            {"bit_width": 8, "direction": "output", "name": "fit_signature_o", "ordinal": 2},
            {"bit_width": 8, "direction": "output", "name": "fit_epoch_o", "ordinal": 3},
        ],
        "hashes": {
            "generated_rtl_sha256": wrapper_hash,
            "generator_sha256": generator_hash,
            "template_sha256": template_hash,
        },
        "hierarchy": {
            "owner": {
                "instance": "u_tile.u_texture_stage.u_texture_v3.u_own",
                "module": "zhao_texture_v3own",
                "required_count": 1,
            },
            "required": [
                {"instance": "u_tile", "module": "zhao_raster_tile_pipe_v2"},
                {"instance": "u_tile.u_texture_stage", "module": "zhao_raster_texture_stage_v3"},
                {"instance": "u_tile.u_texture_stage.u_texture_v3", "module": "zhao_texture_island_v3_top"},
            ],
            "texjoin_required_count": 0,
        },
        "limitations": [
            "Subsystem-only 16x16 characterization; not production terrain throughput.",
            "Random fitter-assigned physical top pins are capacity/timing stimulus, not board pin assignments.",
            "No shell, lease, CDC, framebuffer publication, or physical-device claim.",
        ],
        "module": "zhao_raster_texture_v3_fit_top",
        "product_profile": {
            "parameter": "MIGRATION_SHADOWS",
            "stage_instance": "u_tile.u_texture_stage",
            "value": "1'b0",
            "v3_instance": "u_tile.u_texture_stage.u_texture_v3",
        },
        "schema_id": "zhao.g8a.fit_top",
        "schema_version": 1,
        "source_closure": closure,
        "traffic_profile": {
            "binding_crc32": "0xc60b5076",
            "binding_generation": 1,
            "binding_selector": 1,
            "cache_fill_halfwords": 8,
            "framebuffer_backpressure": "deterministic-lfsr",
            "material": "one-sample-clut4-index5-green",
            "tile": "one 16x16 half-tile triangle per quiet interval",
        },
    }
    return wrapper, canonical_json(payload)


def write_atomic(path: Path, raw: bytes) -> None:
    if not path.parent.is_dir():
        raise RuntimeError(f"output parent does not exist: {path.parent}")
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=path.name + ".", suffix=".tmp",
            dir=path.parent, delete=False,
        ) as stream:
            temporary = stream.name
            stream.write(raw)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass


def verify_live_inputs(snapshots: dict[str, bytes]) -> None:
    for name, expected in snapshots.items():
        path = Path(name)
        if not path.is_file() or path.read_bytes() != expected:
            raise RuntimeError(f"G8A generator input changed during generation: {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        snapshots = read_inputs()
        wrapper, manifest = build_outputs(snapshots)
        verify_live_inputs(snapshots)
        if args.check:
            mismatches = []
            for path, expected in ((WRAPPER, wrapper), (MANIFEST, manifest)):
                if not path.is_file() or path.read_bytes() != expected:
                    mismatches.append(str(path.relative_to(REPO)))
            if mismatches:
                print("STALE: " + ", ".join(mismatches))
                return 1
            print(
                "fresh: G8A wrapper/manifest match generator "
                f"({len(SOURCE_CLOSURE)} sources)"
            )
            return 0
        write_atomic(WRAPPER, wrapper)
        write_atomic(MANIFEST, manifest)
        print(
            f"generated {WRAPPER.relative_to(REPO)} and {MANIFEST.relative_to(REPO)} "
            f"({len(SOURCE_CLOSURE)} sources)"
        )
        return 0
    except (OSError, RuntimeError) as exc:
        print(f"G8A generation failed: {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
