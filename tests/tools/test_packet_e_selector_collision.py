#!/usr/bin/env python3
"""Run one exact Packet-E dual-selector positive control with a hard timeout."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "rtl"))
import texture_v3_interface_parser as interface

CACHE_SOURCES = (
    "tests/mutants/zhao_texture_cache_pipe_v2_packet_e_mutants.sv",
    "fpga/rtl/texture/zhao_texture_cache_pipe_v2.sv",
)
TOP_SOURCES = (
    "fpga/rtl/common/zhao_render_texture_pkg.sv",
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
    "tests/mutants/zhao_texture_island_v3_packet_b_mutants.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
)
MUX_SOURCES = (
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_pkg.sv",
    "fpga/rtl/memory/zhao_mem_guard.sv",
    "tests/mutants/zhao_render_asset_mux_mutants.sv",
    "fpga/rtl/memory/zhao_render_asset_mux.sv",
    "tests/memory/tb_render_asset_mux.sv",
)

PROFILES = {
    "cache-sv": {
        "kind": "sv",
        "top": "zhao_texture_cache_pipe_v2",
        "defines": (
            "ZHAO_PACKET_E_MUTANT_DENIAL_REPLAYS",
            "ZHAO_PACKET_E_MUTANT_PREPAID_DOUBLE_RESV",
        ),
        "sources": CACHE_SOURCES,
        "diagnostic": "ZHAO_PACKET_E_MUTANT_SELECTOR_COLLISION___05FDEFINE_EXACTLY_ONE_SELECTOR",
    },
    "cache-cpp": {
        "kind": "cpp",
        "defines": (
            "EXPECT_PACKET_E_DENIAL_REPLAY_MUTANT=1",
            "EXPECT_PACKET_E_PREPAID_DOUBLE_RESV_MUTANT=1",
        ),
        "source": "tests/texture/texture_cache_pipe_v2_directed.cpp",
        "diagnostic": "PACKET_E_DRIVER_SELECTOR_COLLISION: define exactly one inverse branch",
    },
    "top-sv": {
        "kind": "sv",
        "top": "zhao_texture_island_v3_top",
        "defines": (
            "ZHAO_PACKET_E_MUTANT_PRE_E_FILL_LIFETIME",
            "ZHAO_PACKET_E_MUTANT_RELABEL_REFUSAL_ERR",
        ),
        "sources": TOP_SOURCES,
        "diagnostic": "ZHAO_PACKET_E_TOP_MUTANT_SELECTOR_COLLISION___05FDEFINE_EXACTLY_ONE_SELECTOR",
    },
    "top-cpp": {
        "kind": "cpp",
        "defines": (
            "PACKET_E_EXPECT_PRE_E_FILL_LIFETIME=1",
            "PACKET_E_EXPECT_RELABEL_REFUSAL_ERR=1",
        ),
        "source": "tests/texture/texture_island_v3_packet_b_directed.cpp",
        "diagnostic": "PACKET_E_EXPECT_SELECTOR_COLLISION__DEFINE_EXACTLY_ONE",
    },
    "mux-sv": {
        "kind": "sv",
        "top": "tb_render_asset_mux",
        "defines": (
            "ZHAO_RENDER_ASSET_MUTANT_HOLD_GUARD_VALID",
            "ZHAO_RENDER_ASSET_MUTANT_DRIFT_SUBOWNER",
        ),
        "sources": MUX_SOURCES,
        "diagnostic": "ZHAO_RENDER_ASSET_MUX_MUTANT_SELECTOR_COLLISION___05FDEFINE_EXACTLY_ONE_SELECTOR",
    },
    "mux-cpp": {
        "kind": "cpp",
        "defines": (
            "EXPECT_RENDER_ASSET_HOLD_GUARD_VALID_MUTANT=1",
            "EXPECT_RENDER_ASSET_DRIFT_SUBOWNER_MUTANT=1",
        ),
        "source": "tests/memory/render_asset_mux_directed.cpp",
        "diagnostic": "RENDER_ASSET_MUX_DRIVER_SELECTOR_COLLISION: define exactly one inverse branch",
    },
}


def run(profile: str, verilator: Path, cxx: Path) -> int:
    spec = PROFILES[profile]
    try:
        if spec["kind"] == "sv":
            with tempfile.TemporaryDirectory(prefix=f"packet-e-{profile}-") as temporary:
                command = [
                    str(verilator), "--lint-only", "--Mdir", temporary,
                    "--top-module", str(spec["top"]),
                    *(f"-D{define}" for define in spec["defines"]),
                    *(str(REPO / source) for source in spec["sources"]),
                ]
                completed = subprocess.run(
                    command, cwd=REPO,
                    env=interface.verilator_environment(verilator, REPO),
                    capture_output=True, text=True, errors="replace", timeout=120,
                    check=False,
                )
        else:
            command = [
                str(cxx), "-E", "-x", "c++",
                *(f"-D{define}" for define in spec["defines"]),
                str(REPO / str(spec["source"])),
            ]
            completed = subprocess.run(
                command, cwd=REPO, capture_output=True, text=True,
                errors="replace", timeout=30, check=False,
            )
    except subprocess.TimeoutExpired:
        print(f"FAIL: Packet-E selector control {profile} timed out", file=sys.stderr)
        return 1

    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0:
        print(f"FAIL: Packet-E selector control {profile} unexpectedly compiled", file=sys.stderr)
        return 1
    if str(spec["diagnostic"]) not in diagnostic:
        print(
            f"FAIL: Packet-E selector control {profile} missed exact diagnostic; "
            f"rc={completed.returncode}\n{diagnostic[-4000:]}",
            file=sys.stderr,
        )
        return 1
    print(f"PACKET_E_SELECTOR_COLLISION[{profile}] FIRED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=tuple(PROFILES), required=True)
    parser.add_argument("--verilator", type=Path, required=True)
    parser.add_argument("--cxx", type=Path, required=True)
    args = parser.parse_args()
    if not args.verilator.is_file() or not args.cxx.is_file():
        parser.error("--verilator and --cxx must name existing executables")
    return run(args.profile, args.verilator.resolve(), args.cxx.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
