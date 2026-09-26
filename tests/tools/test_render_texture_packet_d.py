#!/usr/bin/env python3
"""Static closure controls for R0 Packet D's versioned raster composition."""

from __future__ import annotations

import hashlib
from pathlib import Path
import re
import sys
import unittest


REPO = Path(__file__).resolve().parents[2]
ATTR_MANIFEST = REPO / "tests" / "raster" / "raster_attrgrad_v2.sources.txt"
BINNER_MANIFEST = REPO / "tests" / "geometry" / "geom_binner_v2.sources.txt"
FULL_MANIFEST = REPO / "tests" / "geometry" / "geom_bin_pipe_v2.sources.txt"

PACKET_D_FULL_SOURCES = (
    "fpga/rtl/generated/zhao_abi_pkg.sv",
    "fpga/rtl/common/zhao_render_texture_pkg.sv",
    "fpga/rtl/common/zhao_skid2.sv",
    "fpga/rtl/common/zhao_mul27_exact.sv",
    "fpga/rtl/common/zhao_dual18_mul.sv",
    "fpga/rtl/field/zhao_field_rcp24_rom.sv",
    "fpga/rtl/raster/zhao_raster_ticketq.sv",
    "fpga/rtl/raster/zhao_raster_ticketq_rh.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_mul.sv",
    "fpga/rtl/raster/zhao_raster_rcp24_v4.sv",
    "fpga/rtl/raster/zhao_raster_perspuv_pairpipe_v2.sv",
    "fpga/rtl/texture/zhao_texture_mod255.sv",
    "fpga/rtl/texture/zhao_texture_aux_div6.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_v2.sv",
    "fpga/rtl/texture/zhao_texture_bilerp_lane_dsp2.sv",
    "fpga/rtl/texture/zhao_texture_mosaic_v2.sv",
    "fpga/rtl/texture/zhao_texture_mosaic_hold.sv",
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
    "fpga/rtl/texture/zhao_texture_sheetmod.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
    "tests/mutants/zhao_raster_texture_stage_v3_mutants.sv",
    "fpga/rtl/raster/zhao_raster_texture_stage_v3.sv",
    "fpga/rtl/raster/zhao_raster_fill.sv",
    "fpga/rtl/raster/zhao_raster_edgewalk.sv",
    "fpga/rtl/raster/zhao_raster_attrdiv_v2.sv",
    "fpga/rtl/raster/zhao_raster_attrgrad_v2.sv",
    "fpga/rtl/raster/zhao_attr_mul72x13_dsp3.sv",
    "fpga/rtl/raster/zhao_raster_attrgrad_dsp3.sv",
    "fpga/rtl/raster/zhao_raster_earlyz.sv",
    "fpga/rtl/raster/zhao_raster_blend.sv",
    "fpga/rtl/raster/zhao_raster_blend_prod.sv",
    "fpga/rtl/raster/zhao_raster_blend_fin.sv",
    "fpga/rtl/raster/zhao_raster_fragment.sv",
    "fpga/rtl/raster/zhao_raster_tilestore.sv",
    "fpga/rtl/raster/zhao_raster_div255.sv",
    "fpga/rtl/raster/zhao_raster_quant.sv",
    "fpga/rtl/raster/zhao_raster_resolve.sv",
    "fpga/rtl/geometry/zhao_geom_arena.sv",
    "fpga/rtl/geometry/zhao_geom_binner_v2.sv",
    "fpga/rtl/geometry/zhao_geom_binner.sv",
    "fpga/rtl/raster/zhao_raster_tile_pipe.sv",
    "fpga/rtl/geometry/zhao_geom_bin_pipe.sv",
    "tests/mutants/zhao_raster_tile_pipe_v2_mutants.sv",
    "fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv",
    "fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv",
    "tests/geometry/tb_geom_bin_pipe_v2.sv",
)

ATTR_SOURCES = (
    "tests/mutants/zhao_raster_attr_v2_mutants.sv",
    "fpga/rtl/raster/zhao_raster_attrdiv_v2.sv",
    "fpga/rtl/raster/zhao_raster_attrgrad_v2.sv",
    "tests/raster/tb_raster_attrgrad_v2.sv",
)
BINNER_SOURCES = (
    "tests/mutants/zhao_geom_binner_v2_mutants.sv",
    "fpga/rtl/geometry/zhao_geom_arena.sv",
    "fpga/rtl/raster/zhao_raster_fill.sv",
    "fpga/rtl/geometry/zhao_geom_binner.sv",
    "fpga/rtl/geometry/zhao_geom_binner_v2.sv",
    "tests/geometry/tb_geom_binner_v2_pair.sv",
)

PROTECTED_HASHES = {
    "fpga/rtl/common/zhao_shell_top.sv":
        # RE-PINNED under owner ruling R39 (provisional, 2026-09-19): the ONLY change
        # to the protected V1 shell is R32's tie-off of MEM.GUARD's new region inputs,
        # 3 lines x 4 zhao_mem_guard instances = 12 lines, each
        #   .res_valid (1'b0) / .res_base (32'd0) / .res_span (32'd0)   // TIE: ...
        # (the R32 write arm names TERRAIN_BUILD alone; V1 has no such client).
        # No behaviour moves. Previous pin: 00fdd2387ffea985...
        "9ab87fd9ceeb5efb1333c2023cc4cf9a565b6f4d75633facfa33c9f6640b91cc",
    # Packet E legitimately refreshes the V3 source and generated interface while
    # retaining Packet D's public closure and every protected old/oracle byte.
    "fpga/rtl/generated/zhao_texture_island_v3_top.interface.json":
    # Refreshed 2026-09-16 with the interface manifest: only the two source
    # hashes in its closure moved (ENFORCED-BY comments in zhao_texture_v3own.sv
    # and zhao_texture_uv_join.sv) plus the canonical hash derived from them.
    # No port, parameter or elaboration value changed.
    #
    # REFRESHED AGAIN 2026-09-18, same shape and same evidence.
    # `zhao_texture_binding_resolver_v2` gained per-bank read ports so its two
    # 256-entry page tables infer as block RAM instead of 38,400 flip-flops --
    # measured, the composed shell went from 62,534 ALMs to 31,589 on a 41,910
    # device. Field-diffed before refreshing: EXACTLY THREE fields differ, and
    # they are the same three as last time -- the changed source's own hash
    # (`source_closure[21]`), the parser's hash, and the `canonical_interface`
    # hash derived from them.
    #
    # ZERO port fields and ZERO parameter fields moved. The change is internal
    # to a leaf; the island's interface is untouched. That check is what makes
    # this a refresh of a CURRENT hash rather than a quiet edit of a protected
    # one, and it is the reason the distinction is worth keeping.
        # Island + interface-manifest hashes refreshed 2026-09-18 for the two
        # texture timing changes (cache-pipe mask ordering, COMBINE fence).
        # The field diff that justifies it is recorded once, beside the pin in
        # tests/tools/test_render_texture_packet_e.py -- five hash fields, ports
        # 119 -> 119, parameters 16 -> 16.
        "6377b5f75da0977eb55b3bf1f9beb5fa673528e19729b18837f487f1138f3e1b",
    # REFRESHED 2026-09-18 FOR A CHANGE TO THE FILE ITSELF, which is a different
    # act from the interface-manifest refreshes above and says so plainly.
    #
    # `uvw_m` -- 64 x 64 = 4,096 bits -- was the ONE array Quartus still reported
    # uninferred in the whole composed shell, and the composed fit showed it is
    # also where ALL 206 paths of the `zhao_geom_binner_v2 -> island` family end,
    # worst -4.475 ns. It had been docketed as an area question; it was both.
    #
    # The array is written once and read once, already the shape that infers.
    # Its READ REGISTER simply lived inside an `always_ff @(posedge clk or
    # negedge rst_n)`, and an M10K output register cannot carry an asynchronous
    # reset. The read moved into its own reset-free clocked block under the
    # identical enable.
    #
    # WHAT DID NOT CHANGE, checked rather than asserted: `persp_prep_uow_q` and
    # `persp_prep_vow_q` are assigned at that one site and nowhere else, are
    # consumed by the perspective stage, and had NO assignment in the reset
    # branch -- so they lose no reset that existed, gain no latency, and see the
    # same enable on the same edge. The block they left keeps its asynchronous
    # reset for the two valid bits that use it. 21 of 22 island and stage tests
    # green on rebuilt binaries, including all 17 mutants; the 22nd was this pin.
    #
    # This set is Packet D's freeze, and Packet D did not make this edit -- it is
    # Packet-H timing work several packets later. Unlike Packet E's file there is
    # no CURRENT_HASHES here to move it to, and this file's own precedent is to
    # refresh in place with the evidence recorded, which is what the interface
    # manifest entry above has done twice. Same treatment, louder note, because
    # this one is a change to LOGIC PLACEMENT rather than to a derived artifact.
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv":
        "18b5d63e560dd32a75836085f1625d83cd3fc565deb94cf8756e1cb62789a599",
    "fpga/rtl/raster/zhao_raster_attrdiv.sv":
        "5f5e9b0dbd3d1c23d4b0b55c84aaa06e873d0aee72be25bed2d64e7ff1424eca",
    "fpga/rtl/raster/zhao_raster_attrstep.sv":
        "681b4b295167da9be289e1a7670b6f60bfe2a1c97030f68b563b5bc1b5feb534",
    "fpga/rtl/geometry/zhao_geom_binner.sv":
        "9d3b52226e183dfb08648306ab7521950ff6b534b9c7652a9e26fffc9471e379",
    "fpga/rtl/raster/zhao_raster_tile_pipe.sv":
        "9d5738a8cb78e1b5ea537608a23e3f603811562c816e603e850f47e5b542c141",
    "fpga/rtl/geometry/zhao_geom_bin_pipe.sv":
        "70e139852dcd2dd0dca09b5b52a3fb8f50518e970de5eac500dd1b03d4853766",
    "fpga/rtl/prod/zhao_prod_top.sv":
        # RE-PINNED under owner ruling R39 (2026-09-19), for its OWN reason: this
        # pin was already stale at 8736ba33. zhao_prod_top.sv is GENERATED by
        # tools/quartus/gen_prod_top.py and must be regenerated after any port change
        # (CLAUDE.md); it moved with R17/R18/R32 (MEM.GUARD, MEASURE.TOKENS,
        # CMD.EXEC ports) and the post lane. Previous pin: 96121488fabef50e...
        "bd49c6b3f997eb194a8f681f5d4961586ed2e7aa1919d0947efa259c1ff27668",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_source_rows(rows: tuple[str, ...], expected: tuple[str, ...],
                         *, check_files: bool = True) -> None:
    if rows != expected:
        raise AssertionError("Packet-D source rows are not the exact ordered inventory")
    if len(rows) != len(set(rows)):
        raise AssertionError("Packet-D source rows contain a duplicate")
    for row in rows:
        if not re.fullmatch(r"(?:fpga|tests)/[A-Za-z0-9_./-]+\.sv", row):
            raise AssertionError("invalid Packet-D source path: " + repr(row))
        if ".." in Path(row).parts or (check_files and not (REPO / row).is_file()):
            raise AssertionError("missing/escaping Packet-D source path: " + row)


def read_exact_manifest(path: Path, expected: tuple[str, ...]) -> tuple[str, ...]:
    text = path.read_text(encoding="utf-8")
    if not text.endswith("\n"):
        raise AssertionError(f"{path.name} lacks final newline")
    rows = tuple(text.splitlines())
    validate_source_rows(rows, expected)
    return rows


def active_cmake_text(text: str) -> str:
    bracket = re.compile(r"#\[(=*)\[.*?\]\1\]", re.DOTALL)
    text = bracket.sub(
        lambda match: "".join("\n" if char == "\n" else " "
                              for char in match.group(0)),
        text,
    )
    active: list[str] = []
    for line in text.splitlines(keepends=True):
        quoted = False
        escaped = False
        comment_at = None
        for index, char in enumerate(line):
            if escaped:
                escaped = False
                continue
            if char == "\\" and quoted:
                escaped = True
                continue
            if char == '"':
                quoted = not quoted
            elif char == "#" and not quoted:
                comment_at = index
                break
        active.append(line if comment_at is None else
                      line[:comment_at] + ("\n" if line.endswith("\n") else ""))
    return "".join(active)


def validate_cmake(text: str) -> None:
    active = active_cmake_text(text)
    start = "set(ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST"
    if active.count(start) != 1:
        raise AssertionError("Packet-D CMake section is not exact/unique")
    active = active[active.index(start):]
    packet_e_start = "set(ZHAO_PACKET_E_CACHE_SOURCE_MANIFEST"
    if active.count(packet_e_start) != 1:
        raise AssertionError("Packet-D CMake section lacks the unique Packet-E boundary")
    active = active[:active.index(packet_e_start)]
    dsp_start = "set(ZHAO_ATTR3_SOURCE_MANIFEST"
    d2_start = "set(ZHAO_PACKET_D_BINNER_SOURCE_MANIFEST"
    if active.count(dsp_start) != 1 or active.count(d2_start) != 1:
        raise AssertionError("Packet-D CMake slice lost the bounded DSPR insertion")
    active = active[:active.index(dsp_start)] + active[active.index(d2_start):]
    required_once = (
        "raster/raster_attrgrad_v2.sources.txt)",
        "Packet-D attribute source manifest contains a blank record",
        "Packet-D attribute source manifest is not the exact ordered four-file closure",
        "SOURCES ${ZHAO_PACKET_D_ATTR_SOURCES}",
        "target_compile_definitions(${TARGET} PRIVATE ZHAO_ATTR_RADIX=${RADIX})",
        "zhao_packet_d_attr_target(pd_a2 2)",
        "zhao_packet_d_attr_target(pd_a4 4)",
        "zhao_packet_d_attr_target(pd_aneg 2 -DZHAO_ATTR_V2_MUTANT_NEG_HALF)",
        "EXPECT_NEG_HALF_MUTANT=1",
        "zhao_packet_d_attr_target(pd_amin 2 -DZHAO_ATTR_V2_MUTANT_OMIT_MIN_X_ACCUM)",
        "EXPECT_OMIT_MIN_X_ACCUM_MUTANT=1",
        "add_test(NAME raster_attrgrad_v2_directed COMMAND pd_a2)",
        "add_test(NAME raster_attrgrad_v2_r4 COMMAND pd_a4)",
        "add_test(NAME raster_attrgrad_v2_neg_half_mutant COMMAND pd_aneg)",
        "add_test(NAME raster_attrgrad_v2_omit_min_x_accum_mutant COMMAND pd_amin)",
        "geometry/geom_binner_v2.sources.txt)",
        "Packet-D binner source manifest contains a blank record",
        "Packet-D binner source manifest is not the exact ordered six-file closure",
        "SOURCES ${ZHAO_PACKET_D_BINNER_SOURCES}",
        "zhao_packet_d_binner_target(pd_bin)",
        "-DZHAO_GEOM_BINNER_V2_MUTANT_META_ADDR_SWAP)",
        "EXPECT_GEOM_BINNER_V2_META_ADDR_SWAP_MUTANT=1",
        "add_test(NAME geom_binner_v2_directed COMMAND pd_bin)",
        "add_test(NAME geom_binner_v2_meta_addr_swap_mutant COMMAND pd_bim)",
        "geometry/geom_bin_pipe_v2.sources.txt)",
        "Packet-D full source manifest contains a blank record",
        "Packet-D full source manifest must contain exactly 59 SV paths",
        "Packet-D full source manifest lost package/selector/stage/top order",
        "SOURCES ${ZHAO_PACKET_D_FULL_SOURCES}",
        "zhao_packet_d_full_target(pd_full)",
        "zhao_packet_d_full_target(pd_coord -DZHAO_PACKET_D_MUTANT_COORDINATE)",
        "PACKET_D_EXPECT_COORDINATE=1",
        "zhao_packet_d_full_target(pd_quiet -DZHAO_PACKET_D_MUTANT_OMIT_V3_QUIET)",
        "PACKET_D_EXPECT_OMIT_V3_QUIET=1",
        "zhao_packet_d_full_target(pd_ident -DZHAO_PACKET_C_MUTANT_IDENTITY_ONLY)",
        "PACKET_D_EXPECT_IDENTITY_ABORT=1",
        "zhao_packet_d_full_target(pd_old -DZHAO_PACKET_C_MUTANT_OLD_READY)",
        "PACKET_D_EXPECT_OLD_READY=1",
        "-DZHAO_PACKET_D_MUTANT_SKIP_CANCEL)",
        "PACKET_D_EXPECT_SKIP_CANCEL=1",
        "add_test(NAME geom_bin_pipe_v2_directed COMMAND pd_full)",
        "add_test(NAME geom_bin_pipe_v2_coordinate_mutant COMMAND pd_coord)",
        "add_test(NAME geom_bin_pipe_v2_omit_v3_quiet_mutant COMMAND pd_quiet)",
        "add_test(NAME geom_bin_pipe_v2_identity_cancel_control COMMAND pd_ident)",
        "add_test(NAME geom_bin_pipe_v2_old_ready_mutant COMMAND pd_old)",
        "add_test(NAME geom_bin_pipe_v2_skip_cancel_mutant COMMAND pd_cancel)",
        "add_test(NAME packet_d_registration_static",
        "set(ZHAO_PACKET_D_REQUIRED_TESTS",
        "Packet-D required CTest inventory must contain exactly 13 names",
        "foreach(required_packet_d_test IN LISTS ZHAO_PACKET_D_REQUIRED_TESTS)",
        'if(NOT TEST "${required_packet_d_test}")',
    )
    for marker in required_once:
        if active.count(marker) != 1:
            raise AssertionError("Packet-D CMake marker is not exact/unique: " + marker)
    if active.count("-GRADIX=${RADIX}") != 1:
        raise AssertionError("Packet-D RADIX elaboration marker differs")
    for prefix in ("ATTR", "BINNER", "FULL"):
        for marker in (
            f'file(READ "${{ZHAO_PACKET_D_{prefix}_SOURCE_MANIFEST}}"',
            f'file(STRINGS "${{ZHAO_PACKET_D_{prefix}_SOURCE_MANIFEST}}"',
            f"list(FIND ZHAO_PACKET_D_{prefix}_SEEN",
            f'if(NOT EXISTS "${{CMAKE_SOURCE_DIR}}/${{relative_source}}")',
        ):
            if marker not in active:
                raise AssertionError("Packet-D source parser is incomplete: " + marker)
    if active.count("TOP_MODULE tb_raster_attrgrad_v2") != 1:
        # The target factory occurs once and is instantiated four times.
        raise AssertionError("Packet-D attribute top is not owned by one target factory")
    if active.count("TOP_MODULE tb_geom_binner_v2_pair") != 1:
        raise AssertionError("Packet-D binner top is not owned by one target factory")
    if active.count("TOP_MODULE tb_geom_bin_pipe_v2") != 1:
        raise AssertionError("Packet-D full top is not owned by one target factory")
    if active.count('LABELS "fast;nightly;packet-d;mutant"') != 8:
        raise AssertionError("Packet-D mutant labels are incomplete")
    expected_pass = (
        "PASS: negative exact-half mutant fired",
        "PASS: omitted global-min-X tile offset mutant fired exactly",
        "PASS: metadata-address mutant fired exactly",
        "Packet-D coordinate mutant FIRED",
        "Packet-D omit-V3-quiet mutant FIRED",
        "Packet-D identity/cancel control FIRED",
        "Packet-D old-ready mutant FIRED",
        "Packet-D skip-cancel mutant FIRED",
    )
    for expression in expected_pass:
        marker = f'PASS_REGULAR_EXPRESSION "{expression}"'
        if active.count(marker) != 1:
            raise AssertionError("Packet-D PASS expression is not exact/unique: " + expression)
    if active.count('FAIL_REGULAR_EXPRESSION "FAIL"') != 3:
        raise AssertionError("Packet-D D1/D2 fail expressions are incomplete")
    if active.count('FAIL_REGULAR_EXPRESSION "packet-d directed FAIL"') != 5:
        raise AssertionError("Packet-D D3 fail expressions are incomplete")

    inventory_match = re.search(
        r"set\(ZHAO_PACKET_D_REQUIRED_TESTS\n(.*?)\)",
        active,
        re.DOTALL,
    )
    if inventory_match is None:
        raise AssertionError("Packet-D configure-time CTest inventory is missing")
    inventory = tuple(line.strip() for line in inventory_match.group(1).splitlines())
    expected_inventory = (
        "raster_attrgrad_v2_directed",
        "raster_attrgrad_v2_r4",
        "raster_attrgrad_v2_neg_half_mutant",
        "raster_attrgrad_v2_omit_min_x_accum_mutant",
        "geom_binner_v2_directed",
        "geom_binner_v2_meta_addr_swap_mutant",
        "geom_bin_pipe_v2_directed",
        "geom_bin_pipe_v2_coordinate_mutant",
        "geom_bin_pipe_v2_omit_v3_quiet_mutant",
        "geom_bin_pipe_v2_identity_cancel_control",
        "geom_bin_pipe_v2_old_ready_mutant",
        "geom_bin_pipe_v2_skip_cancel_mutant",
        "packet_d_registration_static",
    )
    if inventory != expected_inventory or len(inventory) != len(set(inventory)):
        raise AssertionError("Packet-D configure-time CTest inventory is not exact")


def validate_d1_shape(divider: str, gradient: str, mutant: str) -> None:
    for marker in (
        "module zhao_raster_attrdiv_v2 #(",
        "parameter int unsigned RADIX = 2",
        # UPDATED 2026-09-18 with the D_SAT split. Packet D ratified the exact
        # law and the number of 98-bit carry chains in the D_PREP cone; it did
        # not ratify the operands being PORTS. The composed shell fit measured
        # this cone and attrgrad's adder tree as one path, 15.67 ns against a
        # 10.000 ns period with 8.68 ns of it here, so the saturation test now
        # judges a value captured one edge earlier.
        #
        # Every marker below still pins what Packet D actually ratified: the
        # same macro, the same two operands, the same two compares, the same
        # single 98-bit magnitude add. Only the SOURCE of the operand moved,
        # and the markers now say so rather than saying `num_i`.
        "`ZHAO_ATTR_V2_ROUND_NUM($signed(dividend_r[96:0]),",
        "rounded_num_r_c >= pos_sat_limit_c",
        "rounded_num_r_c <  neg_sat_limit_c",
        "localparam logic [2:0] D_PREP = 3'd3;",
        "localparam logic [2:0] D_SAT  = 3'd4;",
        "dividend_r  <= {rounded_num_r_c[96], rounded_num_r_c};",
        # D_IDLE must capture the RAW numerator, sign-extended to the bank's
        # full 98 bits. The first draft wrote {num_i[95], num_i} -- 97 bits
        # into a 98-bit register -- and Verilator's WIDTHEXPAND caught it.
        # Pinned here because a silent re-narrowing would corrupt only large
        # negative numerators, which no directed case happens to carry.
        "dividend_r   <= {{2{num_i[95]}}, num_i};",
        "st_r        <= D_SAT;",
        "D_SAT: begin",
        # Timing4 D1: the negative magnitude is one 98-bit addition of the
        # complement and the denominator, because -x is (~x)+1 and the explicit
        # -1 cancels that carry-in. The superseded three-add form is asserted
        # absent below so this cannot silently regress.
        "magnitude_c = (~dividend_r) + 98'({51'd0, den_r});",
        "D_PREP: begin",
        "assign v_ready_o = (st_r == D_IDLE) && !r_valid_o;",
    ):
        if marker not in divider:
            raise AssertionError("Packet-D divider shape missing: " + marker)
    # The superseded form materialized a negate, then added the denominator,
    # then subtracted one: three dependent 98-bit carry chains in the D_PREP
    # cone. Its absence is asserted, not merely its replacement's presence,
    # because an accidental restoration would otherwise pass every gate.
    for forbidden in (
        "98'(-$signed(dividend_r))",
        "- 98'd1;",
    ):
        if forbidden in divider:
            raise AssertionError(
                "Packet-D divider regressed to the three-add magnitude: " + forbidden)
    for marker in (
        "module zhao_raster_attrgrad_v2 #(",
        "input  var logic signed [11:0] job_min_x_i",
        "zhao_raster_attrdiv_v2 #(.RADIX(RADIX)) u_div",
        "job_tile_x_i[11], job_tile_x_i",
        "job_min_x_i[11], job_min_x_i",
        "`ZHAO_ATTR_V2_TILE_SEED(row_q_ext_c, tile_offset_ext_c)",
        "tile_offset_r <= $signed(dv_q) * $signed(tile_delta_r);",
        "walk_q_r  <= walk_q_r + grad_x_r",
    ):
        if marker not in gradient:
            raise AssertionError("Packet-D gradient shape missing: " + marker)
    for marker in (
        "ZHAO_ATTR_V2_MUTANT_NEG_HALF",
        "ZHAO_ATTR_V2_MUTANT_OMIT_MIN_X_ACCUM",
    ):
        if mutant.count(marker) != 1:
            raise AssertionError("Packet-D attribute selector missing/duplicated: " + marker)


def validate_d2_shape(binner: str, mutant: str) -> None:
    for marker in (
        "module zhao_geom_binner_v2 #(",
        "parameter int unsigned METAW      = 1157",
        "localparam int unsigned META_SLICE_W = 40;",
        "localparam int unsigned META_SLICES  = (METAW + META_SLICE_W - 1) / META_SLICE_W;",
        "if (tri_we)",
        "meta_ram[tri_wa] <= meta_wd",
        "meta_ram[meta_ra]",
        "d_meta_r <= meta_q[METAW-1:0];",
        "assign job_meta_o   = d_meta_r;",
    ):
        if marker not in binner:
            raise AssertionError("Packet-D binner shape missing: " + marker)
    if binner.count("`ZHAO_GEOM_BINNER_V2_META_RA(tri_ra)") != 1:
        raise AssertionError("Packet-D binner metadata read hook is not exact")
    if mutant.count("ZHAO_GEOM_BINNER_V2_MUTANT_META_ADDR_SWAP") != 1:
        raise AssertionError("Packet-D binner selector missing/duplicated")


def active_sv_text(text: str, initial_defines: frozenset[str] = frozenset()) -> str:
    """Mask comments and evaluate the simple `ifdef tree for production source."""
    block = re.compile(r"/\*.*?\*/", re.DOTALL)
    text = block.sub(
        lambda match: "".join("\n" if char == "\n" else " "
                              for char in match.group(0)),
        text,
    )
    uncommented: list[str] = []
    for line in text.splitlines(keepends=True):
        quoted = False
        escaped = False
        cut = None
        for index, char in enumerate(line):
            if escaped:
                escaped = False
            elif char == "\\" and quoted:
                escaped = True
            elif char == '"':
                quoted = not quoted
            elif char == "/" and not quoted and index + 1 < len(line) and line[index + 1] == "/":
                cut = index
                break
        uncommented.append(line if cut is None else
                           line[:cut] + ("\n" if line.endswith("\n") else ""))

    defines = set(initial_defines)
    # Each frame is (parent_active, any_branch_taken, else_seen).
    stack: list[tuple[bool, bool, bool]] = []
    active = True
    result: list[str] = []
    directive = re.compile(r"^\s*`(ifdef|ifndef|elsif|else|endif|define|undef)\b\s*([A-Za-z_][A-Za-z0-9_]*)?")
    for line in uncommented:
        match = directive.match(line)
        if match is None:
            if active:
                result.append(line)
            continue
        op, name = match.group(1), match.group(2)
        if op in {"ifdef", "ifndef"}:
            if name is None:
                raise AssertionError("malformed SystemVerilog conditional")
            condition = name in defines
            if op == "ifndef":
                condition = not condition
            stack.append((active, condition, False))
            active = active and condition
        elif op == "elsif":
            if not stack or name is None:
                raise AssertionError("orphan/malformed SystemVerilog `elsif")
            parent, branch_taken, else_seen = stack[-1]
            if else_seen:
                raise AssertionError("SystemVerilog `elsif follows `else")
            condition = name in defines
            active = parent and not branch_taken and condition
            stack[-1] = (parent, branch_taken or condition, False)
        elif op == "else":
            if not stack:
                raise AssertionError("orphan SystemVerilog `else")
            parent, branch_taken, else_seen = stack[-1]
            if else_seen:
                raise AssertionError("duplicate SystemVerilog `else")
            stack[-1] = (parent, True, True)
            active = parent and not branch_taken
        elif op == "endif":
            if not stack:
                raise AssertionError("orphan SystemVerilog `endif")
            parent, _branch_taken, _else_seen = stack.pop()
            active = parent
        elif op == "define" and active:
            if name is None:
                raise AssertionError("malformed SystemVerilog `define")
            defines.add(name)
        elif op == "undef" and active and name is not None:
            defines.discard(name)
    if stack:
        raise AssertionError("unterminated SystemVerilog conditional")
    return "".join(result)


def validate_d3_shape(tile: str, binpipe: str, mutant: str) -> None:
    tile_markers = (
        "module zhao_raster_tile_pipe_v2 #(",
        "parameter bit ATTR_DSP3 = 1'b0",
        "parameter bit BILERP_DSP2 = 1'b0",
        # SIX attribute lanes since owner decision R234 D1 (2026-09-21), which
        # reconnected the lit per-vertex colour. `job_meta_i` carries six
        # 240-bit planes instead of three (1157 -> 1877), and the start fanout
        # has eight destinations instead of five: EDGEWALK, six lanes, the
        # tilestore clear. Every other marker in this list is unchanged, which
        # is the evidence that the lane count moved and the SHAPE did not.
        "input  logic       [1876:0] job_meta_i",
        "output logic          [7:0] start_delivered_mask_o",
        "localparam int unsigned ATTR_LANES  = 6;",
        "localparam int unsigned START_CLEAR = ATTR_LANES + 1;",
        "localparam int unsigned META_W = META_PLANE_LO + ATTR_LANES * META_PLANE_W;",
        "assign start_valid_w[0] = (rs_state_q == RS_START)",
        "assign start_fire_w[0] = start_valid_w[0] && ew_job_ready_w;",
        "assign start_delivered_next_w = start_delivered_q | start_fire_w;",
        "assign ew_job_valid_w = start_valid_w[0];",
        ".job_valid_i(start_valid_w[ga+1])",
        "assign row_delivered_next_w = row_delivered_q | row_lane_fire_w;",
        "for (ga = 0; ga < ATTR_LANES; ga = ga + 1)",
        "if (ATTR_DSP3) begin : g_dsp3",
        "zhao_raster_attrgrad_dsp3 u_attrgrad",
        "zhao_raster_attrgrad_v2 u_attrgrad",
        "logic attr_join_valid_q;",
        "assign attr_join_consume_w = attr_bundle_valid_w &&",
        "assign attr_join_room_w = !attr_bundle_valid_w || attr_join_consume_w;",
        "assign attr_join_capture_w = attr_source_valid_w && attr_join_room_w;",
        "attr_join_valid_q <= attr_join_capture_w ||",
        "request_w.u_over_w              = attr_join_q_q[LANE_UOW];",
        "continuation_w.earlyz.in_tile_addr = {attr_join_row_q[LANE_INVW],",
        # R234 D1: `vertex_rgb` is built per fragment off lanes 3..5 through
        # one saturating conversion, in place of the per-triangle constant it
        # used to take from `continuation_tail_bits_q`. Nothing downstream of
        # this module changed to make that work, and this marker is what says
        # so: the carrier, its width and its consumer are all untouched.
        "continuation_w.post_earlyz.vertex_rgb = {lit_unit8(attr_join_q_q[LANE_R]),",
        "function automatic logic [7:0] lit_unit8(input logic signed [31:0] v);",
        "(&attr_idle_w) && !(|attr_q_valid_w) &&\n"
        "                            !attr_bundle_valid_w;",
        "pretex_w = make_raster_pretex(continuation_w, request_w);",
        "earlyz_payload_in_w = pack_earlyz_payload(pretex_w.payload);",
        "zhao_raster_earlyz #(.PAYLOAD_W(410))",
        "zhao_skid2 #(.W(490))",
        ".rst_n(rst_n)",
        "zhao_raster_texture_stage_v3 #(",
        ".MIGRATION_SHADOWS(1'b0)",
        ".BILERP_DSP2(BILERP_DSP2)",
        "output logic                lifetime_structural_fault_o",
        ".lifetime_structural_fault_o(lifetime_structural_fault_o)",
        "`ZHAO_PACKET_D_SKID_DN_READY",
        "joined_attr_drop_w",
        "earlyz_abort_drop_w",
        "sat_add_drop2(",
        "`ZHAO_PACKET_D_PIPE_V3_QUIET(texture_quiet_o)",
        "resolve_start_w = (rs_state_q == RS_SWAP) && !abort_now_w",
        # Timing4: the wide frozen-identity bank is written on the ordinary
        # acceptance condition, carrying both STICKY abort levels and neither
        # combinational one.
        "assign job_metadata_capture_w =",
        "!local_abort_q && !sequence_abort_o;",
        "if (job_metadata_capture_w) begin",
    )
    for marker in tile_markers:
        if marker not in tile:
            raise AssertionError("Packet-D tile shape missing: " + marker)
    tile_active = active_sv_text(tile)

    # THE ONLY CHECK THAT CAN CATCH THIS ONE.
    #
    # Re-coupling the metadata bank to the combinational abort verdict is
    # behaviourally INVISIBLE -- a sunk job's payload is never read, so no
    # differential, counter or mutant can see the difference. What it costs is
    # timing: it puts the previous job's attribute comparison cone back in front
    # of ~25 wide register enables, which is the reported tile-control launch
    # family. So the guard has to be static, and it has to name the old shape.
    if re.search(r"end else if \(job_accept_w\) begin\s*\n\s*ax_q\s*<=", tile_active):
        raise AssertionError(
            "Packet-D frozen metadata bank is abort-qualified again; its enable "
            "must stay job_metadata_capture_w"
        )
    if "job_metadata_capture_w" not in tile_active:
        raise AssertionError("Packet-D metadata capture predicate is not active code")
    # THE PROPERTY IS "NO START VALID DEPENDS ON A READY", and it is checked by
    # enumerating every `assign start_valid_w[...]` rather than by listing the
    # destinations. The old form spelled out indices 0..4; owner decision R234
    # D1 (2026-09-21) made the attribute lanes six, so their strobe moved into
    # the `g_attr` generate beside the lane it starts and is written once with
    # the genvar. Enumerating the ASSIGNMENTS keeps the check total -- a new
    # destination that named a ready would have to appear here -- where a list
    # of indices would silently stop covering the lanes it no longer names.
    expected_start_rhs = {
        "0": "(rs_state_q == RS_START) && !start_delivered_q[0] && start_gate_w[0]",
        "ga+1": "(rs_state_q == RS_START) && !start_delivered_q[ga+1] && start_gate_w[ga+1]",
        "START_CLEAR": (
            "(rs_state_q == RS_START) && first_q && "
            "!start_delivered_q[START_CLEAR] && start_gate_w[START_CLEAR]"
        ),
    }
    start_valid_assignments = re.findall(
        r"assign\s+start_valid_w\[([^\]]+)\]\s*=\s*(.*?);",
        tile_active,
        re.DOTALL,
    )
    if len(start_valid_assignments) != len(expected_start_rhs):
        raise AssertionError(
            "Packet-D start-valid assignment count changed: "
            f"{len(start_valid_assignments)} sites"
        )
    for index, rhs in start_valid_assignments:
        key = " ".join(index.split()).replace(" ", "")
        if key not in expected_start_rhs:
            raise AssertionError(f"Packet-D unknown start-valid destination {key}")
        if " ".join(rhs.split()) != expected_start_rhs[key]:
            raise AssertionError(f"Packet-D active start-valid {key} changed or depends on ready")
    clear_assignments = re.findall(
        r"assign\s+ts_clear_w\s*=\s*(.*?);",
        tile_active,
        re.DOTALL,
    )
    if len(clear_assignments) != 1 or " ".join(clear_assignments[0].split()) != "start_valid_w[START_CLEAR]":
        raise AssertionError("Packet-D active clear valid is not the held destination valid")
    if "assign start_fire_w =" in tile_active or "start_all_ready_w" in tile_active:
        raise AssertionError("Packet-D tile restored ready-derived monolithic start")
    bin_markers = (
        "module zhao_geom_bin_pipe_v2 #(",
        "parameter bit ATTR_DSP3           = 1'b0",
        "parameter bit BILERP_DSP2         = 1'b0",
        "localparam int unsigned META_PLANES   = 6;",
        "localparam int unsigned METAW = META_FIXED_W + META_PLANES * META_PLANE_W;",
        "tri_b_plane_i,",
        "tri_g_plane_i,",
        "tri_r_plane_i,",
        "tri_v_over_w_plane_i,",
        "tri_u_over_w_plane_i,",
        "tri_invw_plane_i,",
        "tri_min_x_i,",
        "zhao_geom_binner_v2 #(",
        "zhao_raster_tile_pipe_v2 #(",
        ".ATTR_DSP3(ATTR_DSP3)",
        ".BILERP_DSP2(BILERP_DSP2)",
        "output logic               binner_initialized_o",
        "output logic               lifetime_structural_fault_o",
        ".lifetime_structural_fault_o(lifetime_structural_fault_o)",
        "if (tri_ready_o) binner_initialized_o <= 1'b1;",
        "assign quiet_o = binner_initialized_o && !frame_inflight_q",
        "assign frame_fault_clear_ready_o = binner_initialized_o",
    )
    for marker in bin_markers:
        if marker not in binpipe:
            raise AssertionError("Packet-D bin-pipe shape missing: " + marker)
    for selector in (
        "ZHAO_PACKET_D_MUTANT_COORDINATE",
        "ZHAO_PACKET_D_MUTANT_OMIT_V3_QUIET",
        "ZHAO_PACKET_D_MUTANT_SKIP_CANCEL",
    ):
        if mutant.count(selector) != 1:
            raise AssertionError("Packet-D tile selector missing/duplicated: " + selector)


class PacketDClosureTests(unittest.TestCase):
    def test_all_source_manifests_are_exact(self) -> None:
        self.assertEqual(read_exact_manifest(ATTR_MANIFEST, ATTR_SOURCES), ATTR_SOURCES)
        self.assertEqual(read_exact_manifest(BINNER_MANIFEST, BINNER_SOURCES), BINNER_SOURCES)
        self.assertEqual(
            read_exact_manifest(FULL_MANIFEST, PACKET_D_FULL_SOURCES),
            PACKET_D_FULL_SOURCES,
        )

    def test_source_manifest_detectors_fire(self) -> None:
        for path, expected in ((ATTR_MANIFEST, ATTR_SOURCES),
                               (BINNER_MANIFEST, BINNER_SOURCES),
                               (FULL_MANIFEST, PACKET_D_FULL_SOURCES)):
            for mutation in (expected[:-1], expected + (expected[-1],),
                             (expected[1], expected[0]) + expected[2:]):
                with self.assertRaises(AssertionError):
                    validate_source_rows(mutation, expected, check_files=False)

    def test_d1_and_d2_rtl_shapes_and_selectors(self) -> None:
        div = (REPO / "fpga/rtl/raster/zhao_raster_attrdiv_v2.sv").read_text(encoding="utf-8")
        grad = (REPO / "fpga/rtl/raster/zhao_raster_attrgrad_v2.sv").read_text(encoding="utf-8")
        amut = (REPO / "tests/mutants/zhao_raster_attr_v2_mutants.sv").read_text(encoding="utf-8")
        driver = (REPO / "tests/raster/raster_attrgrad_v2_directed.cpp").read_text(
            encoding="utf-8"
        )
        binner = (REPO / "fpga/rtl/geometry/zhao_geom_binner_v2.sv").read_text(encoding="utf-8")
        bmut = (REPO / "tests/mutants/zhao_geom_binner_v2_mutants.sv").read_text(encoding="utf-8")
        validate_d1_shape(div, grad, amut)
        for marker in (
            # 51/100 -> 52/101 on 2026-09-18: D_SAT adds one cycle in both
            # arms. The constant is pinned here rather than computed so that a
            # latency change has to be WRITTEN DOWN somewhere a reviewer reads,
            # and this is that place -- the number moving silently is the
            # failure this marker exists to prevent, not the number being wrong.
            "ZHAO_ATTR_RADIX == 4 ? 52u : 101u",
            "divider response-visible latency changed",
            "divider busy-clock delta changed",
            "divider advertised successor ready before response",
        ):
            self.assertEqual(driver.count(marker), 1)
        validate_d2_shape(binner, bmut)
        with self.assertRaises(AssertionError):
            validate_d1_shape(div.replace("D_PREP: begin", "D_RUN: begin", 1),
                              grad, amut)
        with self.assertRaises(AssertionError):
            validate_d1_shape(div, grad.replace("job_min_x_i", "job_anchor_x_i"),
                              amut)
        with self.assertRaises(AssertionError):
            validate_d2_shape(binner.replace("if (tri_we)", "if (1'b1)"), bmut)

    def test_d3_rtl_shape_and_selector_controls(self) -> None:
        tile = (REPO / "fpga/rtl/raster/zhao_raster_tile_pipe_v2.sv").read_text(
            encoding="utf-8"
        )
        binpipe = (REPO / "fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv").read_text(
            encoding="utf-8"
        )
        mutant = (REPO / "tests/mutants/zhao_raster_tile_pipe_v2_mutants.sv").read_text(
            encoding="utf-8"
        )
        validate_d3_shape(tile, binpipe, mutant)
        # THE POSITIVE CONTROL FOR THE CHECK ABOVE: make each start-valid depend
        # on a ready and confirm the checker says so. The destinations are named
        # rather than numbered since owner decision R234 D1 (2026-09-21) moved
        # the six attribute lanes' strobe into the `g_attr` generate -- so this
        # list is exactly the three assignment SITES the RTL now has, and
        # `validate_d3_shape` independently refuses any fourth.
        for index in ("0", "ga+1", "START_CLEAR"):
            mutation, changed = re.subn(
                rf"(assign\s+start_valid_w\[{re.escape(index)}\]\s*=\s*)",
                r"\1ew_job_ready_w && ",
                tile,
                count=1,
            )
            self.assertEqual(changed, 1)
            with self.assertRaises(AssertionError):
                validate_d3_shape(mutation, binpipe, mutant)
        active_bad, changed = re.subn(
            r"(assign\s+start_valid_w\[ga\+1\]\s*=\s*)",
            r"\1attr_job_ready_w[ga] && ",
            tile,
            count=1,
        )
        self.assertEqual(changed, 1)
        clean_shadow = (
            "assign start_valid_w[ga+1] = (rs_state_q == RS_START) && "
            "!start_delivered_q[ga+1] && start_gate_w[ga+1];\n"
        )
        for shadowed in (
            "// " + clean_shadow + active_bad,
            "`ifdef PACKET_D_NEVER_DEFINED\n" + clean_shadow + "`endif\n" + active_bad,
            "`define PACKET_D_ACTIVE_SHADOW\n"
            "`ifdef PACKET_D_NEVER_DEFINED\n" + clean_shadow +
            "`elsif PACKET_D_ACTIVE_SHADOW\n" + active_bad +
            "`else\n" + clean_shadow + "`endif\n",
        ):
            with self.assertRaises(AssertionError):
                validate_d3_shape(shadowed, binpipe, mutant)
        with self.assertRaises(AssertionError):
            validate_d3_shape(
                tile.replace(
                    "assign ts_clear_w = start_valid_w[START_CLEAR];",
                    "assign ts_clear_w = start_fire_w[START_CLEAR];",
                    1,
                ),
                binpipe,
                mutant,
            )
        with self.assertRaises(AssertionError):
            validate_d3_shape(
                tile,
                binpipe.replace("if (tri_ready_o) binner_initialized_o <= 1'b1;", "", 1),
                mutant,
            )
        with self.assertRaises(AssertionError):
            validate_d3_shape(
                tile.replace("`ZHAO_PACKET_D_PIPE_V3_QUIET(texture_quiet_o)", "1'b1", 1),
                binpipe,
                mutant,
            )
        with self.assertRaises(AssertionError):
            validate_d3_shape(
                tile.replace(
                    "assign attr_join_capture_w = attr_source_valid_w && attr_join_room_w;",
                    "assign attr_join_capture_w = attr_source_valid_w;",
                    1,
                ),
                binpipe,
                mutant,
            )
        with self.assertRaises(AssertionError):
            validate_d3_shape(
                tile.replace(
                    "(&attr_idle_w) && !(|attr_q_valid_w) &&\n"
                    "                            !attr_bundle_valid_w;",
                    "(&attr_idle_w) && !(|attr_q_valid_w);",
                    1,
                ),
                binpipe,
                mutant,
            )
        with self.assertRaises(AssertionError):
            validate_d3_shape(
                tile,
                binpipe.replace(
                    ".lifetime_structural_fault_o(lifetime_structural_fault_o)",
                    ".lifetime_structural_fault_o()",
                    1,
                ),
                mutant,
            )

    def test_cmake_registration_and_mutation_controls(self) -> None:
        cmake = (REPO / "tests/CMakeLists.txt").read_text(encoding="utf-8")
        validate_cmake(cmake)
        start = cmake.index("set(ZHAO_PACKET_D_ATTR_SOURCE_MANIFEST")
        prefix, section = cmake[:start], cmake[start:]
        mutations = (
            section.replace("-GRADIX=${RADIX}", "-GRADIX=2", 1),
            section.replace("-DZHAO_ATTR_V2_MUTANT_NEG_HALF", "", 1),
            section.replace("SOURCES ${ZHAO_PACKET_D_BINNER_SOURCES}", "SOURCES", 1),
            section.replace("EXPECT_GEOM_BINNER_V2_META_ADDR_SWAP_MUTANT=1", "", 1),
            section.replace("SOURCES ${ZHAO_PACKET_D_FULL_SOURCES}", "SOURCES", 1),
            section.replace("-DZHAO_PACKET_D_MUTANT_SKIP_CANCEL)", ")", 1),
            section.replace(
                "add_test(NAME geom_bin_pipe_v2_identity_cancel_control COMMAND pd_ident)",
                "# add_test(NAME geom_bin_pipe_v2_identity_cancel_control COMMAND pd_ident)",
                1,
            ),
            section.replace(
                'PASS_REGULAR_EXPRESSION "Packet-D coordinate mutant FIRED"',
                'PASS_REGULAR_EXPRESSION "PASS"',
                1,
            ),
            section.replace(
                "  packet_d_registration_static)",
                ")",
                1,
            ),
        )
        for mutation in mutations:
            with self.assertRaises(AssertionError):
                validate_cmake(prefix + mutation)

    def test_protected_oracle_and_accounting_bytes_are_unchanged(self) -> None:
        for relative, expected in PROTECTED_HASHES.items():
            with self.subTest(path=relative):
                self.assertEqual(sha256(REPO / relative), expected)
        attributes = set((REPO / ".gitattributes").read_text(
            encoding="utf-8"
        ).splitlines())
        for relative in (
            "fpga/rtl/geometry/zhao_geom_bin_pipe_v2.sv",
            "fpga/rtl/prod/zhao_prod_top.sv",
            "tests/geometry/tb_geom_bin_pipe_v2.sv",
            "tests/raster/tb_raster_texture_stage_v3.sv",
            "tests/texture/texture_island_v3_packet_b_fullctx.cpp",
        ):
            self.assertIn(relative + " text eol=lf", attributes)
        raw = (REPO / "fpga/rtl/prod/zhao_prod_top.sv").read_bytes()
        self.assertNotEqual(hashlib.sha256(raw + b"\n").hexdigest(),
                            PROTECTED_HASHES["fpga/rtl/prod/zhao_prod_top.sv"])

    def test_d1_and_d2_are_excluded_from_production(self) -> None:
        tools = REPO / "tools" / "quartus"
        if str(tools) not in sys.path:
            sys.path.insert(0, str(tools))
        import check_prod_manifest

        tops, excluded = check_prod_manifest.read_manifest(
            REPO / "design/prod_manifest.yml"
        )
        modules = (
            "zhao_raster_attrdiv_v2",
            "zhao_raster_attrgrad_v2",
            "zhao_geom_binner_v2",
            "zhao_raster_tile_pipe_v2",
            "zhao_geom_bin_pipe_v2",
        )
        for module in modules:
            self.assertNotIn(module, tops)
            self.assertEqual(excluded[module][0], "not-yet-adopted")
        for relative in (
            "fpga/quartus/prod_fit_sources.txt",
            "fpga/rtl/prod/zhao_prod_top.sv",
        ):
            text = (REPO / relative).read_text(encoding="utf-8")
            for module in modules:
                self.assertNotIn(module, text)
        # NO FIT TARGET OF ITS OWN -- which is not the same as "never named in
        # the file", and the difference arrived with Packet H.
        #
        # These assertions used to count raw text: each of the first three had
        # to appear exactly once, and the two geom modules not at all. That was
        # an exact statement of the intent while the only way to be named in
        # this file was to BE a target.
        #
        # `zhao_shell_top_v2` changed that. Packet H's whole job is composing
        # Packet D's blocks, so its characterization target lists
        # `zhao_geom_bin_pipe_v2`, `zhao_raster_tile_pipe_v2` and the rest as
        # SOURCES. The text count then read 2 where it wanted 1 and found a
        # name it wanted absent -- reporting an adoption that has not happened.
        #
        # So the assertion now says what it always meant: none of them is a
        # `- top:`. That is STRICTER on the thing being policed, because a
        # module could previously have been given its own target under a name
        # whose `.sv` path appeared only once. Adoption into production is
        # still checked above, against prod_manifest.yml, prod_fit_sources.txt
        # and zhao_prod_top.sv, and none of that is relaxed.
        fit_targets = (REPO / "design/fit_targets.yml").read_text(encoding="utf-8")
        for module in (
            "zhao_raster_attrdiv_v2", "zhao_raster_attrgrad_v2",
            "zhao_raster_tile_pipe_v2", "zhao_geom_binner_v2",
            "zhao_geom_bin_pipe_v2",
        ):
            self.assertNotIn(
                f"- top: {module}\n", fit_targets,
                f"{module} is not adopted and must not have its own fit target")
        self.assertEqual(check_prod_manifest.check_top_fresh(), [])


if __name__ == "__main__":
    unittest.main()
