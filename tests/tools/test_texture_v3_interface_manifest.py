#!/usr/bin/env python3
"""Private Python gates for the Texture-V3 schema-v1 interface manifest."""

from __future__ import annotations

import copy
from dataclasses import replace
import hashlib
import json
import os
import re
from pathlib import Path
import platform
import sys
import tempfile
import unittest
from unittest import mock


REPO = Path(__file__).resolve().parents[2]
TOOLS = REPO / "tools" / "rtl"
FIXTURE_PATH = "tests/rtl/fixtures/zhao_texture_interface_schema_fixture.sv"
PACKAGE_PATH = "fpga/rtl/common/zhao_render_texture_pkg.sv"
FIXTURE_CLOSURE = [PACKAGE_PATH, FIXTURE_PATH]
FIXTURE = REPO / Path(*FIXTURE_PATH.split("/"))
QUIET_CONTROLS = REPO / "tests" / "tools" / "fixtures" / "texture_v3_quiet_omission_mutants.json"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import check_texture_v3_interface_manifest as checker
import gen_texture_v3_interface_manifest as generator
import texture_v3_interface_parser as interface


EXPECTED_PACKET_B_CLOSURE = (
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
    "fpga/rtl/texture/zhao_texture_sheetmod.sv",
    "fpga/rtl/texture/zhao_texture_island_v3_top.sv",
)
EXPECTED_PRODUCTION_PARAMETER_INVENTORY = (
    ("MIGRATION_SHADOWS", "bit_vector", "1'h1"),
    ("DEPTH", "unsigned_integer", "16"),
    ("CTXW", "unsigned_integer", "64"),
    ("RCTXW", "unsigned_integer", "160"),
    ("AUXCTXW", "unsigned_integer", "224"),
    ("BINDW", "unsigned_integer", "8"),
    ("LODW", "unsigned_integer", "8"),
    ("GENW", "unsigned_integer", "8"),
    ("LANES", "unsigned_integer", "4"),
    ("SRCW", "unsigned_integer", "18"),
    ("DATAW", "unsigned_integer", "64"),
    ("TOKW", "unsigned_integer", "18"),
    ("AUX_TOKW", "unsigned_integer", "14"),
    ("PAL_SLOTS", "unsigned_integer", "4"),
    ("PAL_ENTRIES", "unsigned_integer", "256"),
    ("BILERP_DSP2", "bit_vector", "1'h0"),
)
EXPECTED_PRODUCTION_PORT_NAMES = (
    "clk", "rst_n", "frag_valid_i", "frag_ready_o", "frag_invw24_i",
    "frag_u_over_w_i", "frag_v_over_w_i", "frag_sample_count_i",
    "frag_binding_i", "frag_lod_i", "frag_recipe_i", "frag_weight_i",
    "frag_ctx_i", "frag_retire_ctx_i", "frag_aux_ctx_i", "frag_aux_i",
    "frag_base_rgb_i", "frag_base_a_i", "frag_class_i", "frag_pal_slot_i",
    "frag_pal_gen_i", "frame_fault_clear_valid_i",
    "frame_fault_clear_ready_o", "frame_fault_o", "lifetime_structural_fault_o",
    "cfg_valid_i",
    "cfg_ready_o", "cfg_op_i", "cfg_page_generation_i", "cfg_selector_i",
    "cfg_row_i", "cfg_crc32_i", "cfg_rsp_valid_o", "cfg_rsp_ready_i",
    "cfg_rsp_op_o", "cfg_rsp_status_o", "cfg_rsp_page_generation_o",
    "active_page_generation_o", "fill_req_valid_o", "fill_req_ready_i",
    "fill_req_addr_o", "fill_data_valid_i", "fill_data_i", "fill_refused_i",
    "pal_load_valid_i", "pal_load_ready_o", "pal_load_op_i",
    "pal_load_slot_i", "pal_load_gen_i", "pal_load_idx_i",
    "pal_load_rgb565_i", "pal_load_crc_ok_i", "sheet_req_valid_o",
    "sheet_req_ready_i", "sheet_req_op_o", "sheet_req_handle_o",
    "sheet_req_texel_o", "sheet_req_src_id_o", "pg_valid_i", "pg_ready_o",
    "pg_op_i", "pg_status_i", "pg_tag_i", "pg_strength_i", "pg_src_id_i",
    "out_valid_o", "out_ready_i", "out_rgb_o", "out_a_o",
    "out_texel_idx_o", "out_status_o", "out_tag_o", "out_retire_ctx_o",
    "out_refused_o", "quiet_o", "err_fragrob_wq_overflow_o",
    "err_fragrob_id_error_o", "err_aux_degenerate_o", "err_rcp_q_o",
    "cnt_reorder_held_o", "cnt_live_peak_o", "cnt_fragments_o",
    "cnt_cache_hits_o", "cnt_cache_misses_o", "cnt_palette_lookups_o",
    "cnt_bilerp_jobs_o", "cnt_mosaic_samples_o", "cnt_texture_samples_o",
    "cnt_aux_accepted_o",
    "cnt_combine_refused_o", "cnt_combine_phases_o", "cnt_rcp_completed_o",
    "cnt_persp_fragments_o", "cnt_dispatch_accepted_o",
    "cnt_plan_accepted_o", "cnt_fragrob_id_errors_o", "shadow_present_o",
    "meta_shadow_mismatch_o", "meta_shadow_reads_o", "meta_align_err_o",
    "meta_align_chk_o", "meta_bil_err_o", "meta_bil_chk_o",
    "meta_near_err_o", "meta_near_chk_o", "meta_bil_first_q_o",
    "meta_bil_first_t_o", "meta_bil_first_tok_o", "meta_genmis_o",
    "cnt_combine_jobs_o", "cnt_palette_stale_o", "cnt_palette_cold_o",
    "err_rsp_dropped_o", "err_bil_chan_o", "cnt_near_refused_o",
    "err_unknown_class_o", "err_class_invalid_o", "err_palette_unusable_o",
    "err_class_mismatch_o", "err_plan_mode_o",
)


CANONICAL_ORACLE_PAYLOAD = {
    "z": [True, False, 7, "雪\n"],
    "hashes": {
        "canonical_interface_sha256": "ignored",
        "module_declaration_sha256": "1" * 64,
        "top_source_sha256": "2" * 64,
    },
    "a": {"é": "\\"},
}
CANONICAL_ORACLE_BYTES = (
    b'{"a":{"\xc3\xa9":"\\\\"},"hashes":{'
    b'"module_declaration_sha256":"1111111111111111111111111111111111111111111111111111111111111111",'
    b'"top_source_sha256":"2222222222222222222222222222222222222222222222222222222222222222"},'
    b'"z":[true,false,7,"\xe9\x9b\xaa\\n"]}'
)
CANONICAL_ORACLE_SHA256 = "8e49af33bdba6e2490310867f1ab4eb95e89ec1c49566a52785efd73b5e2e335"
EXPECTED_DUPLICATE_PACKAGE_SHA256 = "54f7a8399b02634f5cf0fb892fa271ead317406fe8f35caa8cbd80694ff9c162"
EXPECTED_DUPLICATE_MARKER_SHA256 = "28a1106e736abe08b092f060bbb9cefbc323b92b530cfa854bf250fac1cb166a"
EXPECTED_DUPLICATE_MARKER_ROWS = (
    ("/miscsp/0/typesp/50/membersp/0", "(ME)", "in_tile_addr", "e,33:18,33:30"),
    ("/miscsp/0/typesp/50/membersp/1", "(ME)", "invw24", "e,34:18,34:24"),
    ("/miscsp/0/typesp/50/membersp/2", "(ME)", "fragment_state", "e,35:18,35:32"),
    ("/miscsp/0/typesp/50/membersp/3", "(ME)", "source_id", "e,36:18,36:27"),
    ("/miscsp/0/typesp/51/membersp/0", "(EF)", "vertex_rgb", "e,50:18,50:28"),
    ("/miscsp/0/typesp/51/membersp/1", "(EF)", "vertex_alpha", "e,51:18,51:30"),
    ("/miscsp/0/typesp/51/membersp/2", "(EF)", "effect_tag", "e,52:18,52:28"),
    ("/miscsp/0/typesp/51/membersp/3", "(EF)", "stencil_reference", "e,53:18,53:35"),
    ("/miscsp/0/typesp/52/membersp/0", "(MG)", "earlyz", "e,75:42,75:48"),
    ("/miscsp/0/typesp/52/membersp/1", "(MG)", "post_earlyz", "e,76:42,76:53"),
    ("/miscsp/0/typesp/53/membersp/0", "(YH)", "env_z1", "e,104:25,104:31"),
    ("/miscsp/0/typesp/53/membersp/1", "(YH)", "env_z0", "e,105:25,105:31"),
    ("/miscsp/0/typesp/53/membersp/2", "(YH)", "env_x1", "e,106:25,106:31"),
    ("/miscsp/0/typesp/53/membersp/3", "(YH)", "env_x0", "e,107:25,107:31"),
    ("/miscsp/0/typesp/53/membersp/4", "(YH)", "sheet_handle", "e,108:25,108:37"),
    ("/miscsp/0/typesp/53/membersp/5", "(YH)", "wz", "e,109:25,109:27"),
    ("/miscsp/0/typesp/53/membersp/6", "(YH)", "wx", "e,110:25,110:27"),
    ("/miscsp/0/typesp/54/membersp/0", "(EK)", "u_over_w", "e,144:34,144:42"),
    ("/miscsp/0/typesp/54/membersp/1", "(EK)", "v_over_w", "e,145:34,145:42"),
    ("/miscsp/0/typesp/54/membersp/10", "(EK)", "base_alpha", "e,154:34,154:44"),
    ("/miscsp/0/typesp/54/membersp/11", "(EK)", "response_class", "e,155:34,155:48"),
    ("/miscsp/0/typesp/54/membersp/12", "(EK)", "palette_slot", "e,156:34,156:46"),
    ("/miscsp/0/typesp/54/membersp/13", "(EK)", "palette_generation", "e,157:34,157:52"),
    ("/miscsp/0/typesp/54/membersp/2", "(EK)", "sample_count", "e,146:34,146:46"),
    ("/miscsp/0/typesp/54/membersp/3", "(EK)", "base_binding_selector", "e,147:34,147:55"),
    ("/miscsp/0/typesp/54/membersp/4", "(EK)", "lod_q4_4", "e,148:34,148:42"),
    ("/miscsp/0/typesp/54/membersp/5", "(EK)", "material_recipe", "e,149:34,149:49"),
    ("/miscsp/0/typesp/54/membersp/6", "(EK)", "recipe_weight", "e,150:34,150:47"),
    ("/miscsp/0/typesp/54/membersp/7", "(EK)", "aux_required", "e,151:34,151:46"),
    ("/miscsp/0/typesp/54/membersp/8", "(EK)", "aux_surface_ctx", "e,152:34,152:49"),
    ("/miscsp/0/typesp/54/membersp/9", "(EK)", "base_rgb", "e,153:34,153:42"),
    ("/miscsp/0/typesp/55/membersp/0", "(AL)", "raster_continuation", "e,173:40,173:59"),
    ("/miscsp/0/typesp/55/membersp/1", "(AL)", "texture_request", "e,174:40,174:55"),
    ("/miscsp/0/typesp/56/membersp/0", "(CP)", "earlyz", "e,232:38,232:44"),
    ("/miscsp/0/typesp/56/membersp/1", "(CP)", "payload", "e,233:38,233:45"),
    ("/miscsp/0/typesp/57/membersp/0", "(SQ)", "raster_sequence", "e,260:38,260:53"),
    ("/miscsp/0/typesp/57/membersp/1", "(SQ)", "raster_continuation", "e,261:38,261:57"),
    ("/miscsp/0/typesp/58/membersp/0", "(WR)", "status", "e,281:18,281:24"),
    ("/miscsp/0/typesp/58/membersp/1", "(WR)", "sample0_raw_index", "e,282:18,282:35"),
    ("/miscsp/0/typesp/58/membersp/2", "(WR)", "alpha", "e,283:18,283:23"),
    ("/miscsp/0/typesp/58/membersp/3", "(WR)", "rgb", "e,284:18,284:21"),
)
# The oracle's own answer, not the tool's copied across. The remaps below were
# re-derived from (member_name, loc) after the 2026-09-18 resolver work, this
# digest was then computed from the rebuilt rows, and only afterwards compared
# with what the parser pins -- which is the order that keeps the two
# independent. They agreed.
EXPECTED_PRODUCTION_DUPLICATE_MARKER_SHA256 = "0cb6f8812895c285ade5911768134b90d8691f2a7171007d8aa130a05e53640a"
_PRE_DSP_PRODUCTION_DUPLICATE_MARKER_ROWS = (
    ("/miscsp/0/typesp/107/membersp/0", "(WTOB)", "in_tile_addr", "e,33:18,33:30"),
    ("/miscsp/0/typesp/107/membersp/1", "(WTOB)", "invw24", "e,34:18,34:24"),
    ("/miscsp/0/typesp/107/membersp/2", "(WTOB)", "fragment_state", "e,35:18,35:32"),
    ("/miscsp/0/typesp/107/membersp/3", "(WTOB)", "source_id", "e,36:18,36:27"),
    ("/miscsp/0/typesp/108/membersp/0", "(OUOB)", "vertex_rgb", "e,50:18,50:28"),
    ("/miscsp/0/typesp/108/membersp/1", "(OUOB)", "vertex_alpha", "e,51:18,51:30"),
    ("/miscsp/0/typesp/108/membersp/2", "(OUOB)", "effect_tag", "e,52:18,52:28"),
    ("/miscsp/0/typesp/108/membersp/3", "(OUOB)", "stencil_reference", "e,53:18,53:35"),
    ("/miscsp/0/typesp/109/membersp/0", "(WVOB)", "earlyz", "e,75:42,75:48"),
    ("/miscsp/0/typesp/109/membersp/1", "(WVOB)", "post_earlyz", "e,76:42,76:53"),
    ("/miscsp/0/typesp/110/membersp/0", "(GUJ)", "env_z1", "e,104:25,104:31"),
    ("/miscsp/0/typesp/110/membersp/1", "(GUJ)", "env_z0", "e,105:25,105:31"),
    ("/miscsp/0/typesp/110/membersp/2", "(GUJ)", "env_x1", "e,106:25,106:31"),
    ("/miscsp/0/typesp/110/membersp/3", "(GUJ)", "env_x0", "e,107:25,107:31"),
    ("/miscsp/0/typesp/110/membersp/4", "(GUJ)", "sheet_handle", "e,108:25,108:37"),
    ("/miscsp/0/typesp/110/membersp/5", "(GUJ)", "wz", "e,109:25,109:27"),
    ("/miscsp/0/typesp/110/membersp/6", "(GUJ)", "wx", "e,110:25,110:27"),
    ("/miscsp/0/typesp/111/membersp/0", "(NZOB)", "u_over_w", "e,144:34,144:42"),
    ("/miscsp/0/typesp/111/membersp/1", "(NZOB)", "v_over_w", "e,145:34,145:42"),
    ("/miscsp/0/typesp/111/membersp/10", "(NZOB)", "base_alpha", "e,154:34,154:44"),
    ("/miscsp/0/typesp/111/membersp/11", "(NZOB)", "response_class", "e,155:34,155:48"),
    ("/miscsp/0/typesp/111/membersp/12", "(NZOB)", "palette_slot", "e,156:34,156:46"),
    ("/miscsp/0/typesp/111/membersp/13", "(NZOB)", "palette_generation", "e,157:34,157:52"),
    ("/miscsp/0/typesp/111/membersp/2", "(NZOB)", "sample_count", "e,146:34,146:46"),
    ("/miscsp/0/typesp/111/membersp/3", "(NZOB)", "base_binding_selector", "e,147:34,147:55"),
    ("/miscsp/0/typesp/111/membersp/4", "(NZOB)", "lod_q4_4", "e,148:34,148:42"),
    ("/miscsp/0/typesp/111/membersp/5", "(NZOB)", "material_recipe", "e,149:34,149:49"),
    ("/miscsp/0/typesp/111/membersp/6", "(NZOB)", "recipe_weight", "e,150:34,150:47"),
    ("/miscsp/0/typesp/111/membersp/7", "(NZOB)", "aux_required", "e,151:34,151:46"),
    ("/miscsp/0/typesp/111/membersp/8", "(NZOB)", "aux_surface_ctx", "e,152:34,152:49"),
    ("/miscsp/0/typesp/111/membersp/9", "(NZOB)", "base_rgb", "e,153:34,153:42"),
    ("/miscsp/0/typesp/112/membersp/0", "(JAPB)", "raster_continuation", "e,173:40,173:59"),
    ("/miscsp/0/typesp/112/membersp/1", "(JAPB)", "texture_request", "e,174:40,174:55"),
    ("/miscsp/0/typesp/113/membersp/0", "(LEPB)", "earlyz", "e,232:38,232:44"),
    ("/miscsp/0/typesp/113/membersp/1", "(LEPB)", "payload", "e,233:38,233:45"),
    ("/miscsp/0/typesp/114/membersp/0", "(BGPB)", "raster_sequence", "e,260:38,260:53"),
    ("/miscsp/0/typesp/114/membersp/1", "(BGPB)", "raster_continuation", "e,261:38,261:57"),
    ("/miscsp/0/typesp/115/membersp/0", "(FHPB)", "status", "e,281:18,281:24"),
    ("/miscsp/0/typesp/115/membersp/1", "(FHPB)", "sample0_raw_index", "e,282:18,282:35"),
    ("/miscsp/0/typesp/115/membersp/2", "(FHPB)", "alpha", "e,283:18,283:23"),
    ("/miscsp/0/typesp/115/membersp/3", "(FHPB)", "rgb", "e,284:18,284:21"),
    ("/miscsp/0/typesp/255/membersp/0", "(IYV)", "valid", "z,124:18,124:23"),
    ("/miscsp/0/typesp/255/membersp/1", "(IYV)", "palette_generation", "z,125:18,125:36"),
    ("/miscsp/0/typesp/255/membersp/2", "(IYV)", "palette_slot", "z,126:18,126:30"),
    ("/miscsp/0/typesp/255/membersp/3", "(IYV)", "mode", "z,127:18,127:22"),
    ("/miscsp/0/typesp/255/membersp/4", "(IYV)", "base", "z,128:18,128:22"),
    ("/miscsp/0/typesp/267/membersp/0", "(KYV)", "handle", "z,132:22,132:28"),
    ("/miscsp/0/typesp/267/membersp/1", "(KYV)", "page_generation", "z,133:22,133:37"),
    ("/miscsp/0/typesp/267/membersp/10", "(KYV)", "witness_palette_generation", "z,142:22,142:48"),
    ("/miscsp/0/typesp/267/membersp/2", "(KYV)", "selector_overflow", "z,134:22,134:39"),
    ("/miscsp/0/typesp/267/membersp/3", "(KYV)", "force_refuse", "z,135:22,135:34"),
    ("/miscsp/0/typesp/267/membersp/4", "(KYV)", "binding_selector", "z,136:22,136:38"),
    ("/miscsp/0/typesp/267/membersp/5", "(KYV)", "u", "z,137:25,137:26"),
    ("/miscsp/0/typesp/267/membersp/6", "(KYV)", "v", "z,138:25,138:26"),
    ("/miscsp/0/typesp/267/membersp/7", "(KYV)", "lod_q4_4", "z,139:22,139:30"),
    ("/miscsp/0/typesp/267/membersp/8", "(KYV)", "witness_class", "z,140:22,140:35"),
    ("/miscsp/0/typesp/267/membersp/9", "(KYV)", "witness_palette_slot", "z,141:22,141:42"),
    ("/miscsp/0/typesp/276/membersp/0", "(MYV)", "route_token", "z,146:24,146:35"),
    ("/miscsp/0/typesp/276/membersp/1", "(MYV)", "base", "z,147:24,147:28"),
    ("/miscsp/0/typesp/276/membersp/2", "(MYV)", "mode", "z,148:24,148:28"),
    ("/miscsp/0/typesp/276/membersp/3", "(MYV)", "palette_slot", "z,149:24,149:36"),
    ("/miscsp/0/typesp/276/membersp/4", "(MYV)", "palette_generation", "z,150:24,150:42"),
    ("/miscsp/0/typesp/276/membersp/5", "(MYV)", "u", "z,151:25,151:26"),
    ("/miscsp/0/typesp/276/membersp/6", "(MYV)", "v", "z,152:25,152:26"),
    ("/miscsp/0/typesp/276/membersp/7", "(MYV)", "lod_q4_4", "z,153:24,153:32"),
    ("/miscsp/0/typesp/311/membersp/0", "(BOU)", "active_page_generation", "y,85:33,85:55"),
    ("/miscsp/0/typesp/311/membersp/1", "(BOU)", "binding_selector", "y,86:33,86:49"),
    ("/miscsp/0/typesp/311/membersp/10", "(BOU)", "lod_q4_4", "y,95:33,95:41"),
    ("/miscsp/0/typesp/311/membersp/11", "(BOU)", "aux_context", "y,96:33,96:44"),
    ("/miscsp/0/typesp/311/membersp/2", "(BOU)", "mosaic_weight", "y,87:33,87:46"),
    ("/miscsp/0/typesp/311/membersp/3", "(BOU)", "mosaic_material_b", "y,88:33,88:50"),
    ("/miscsp/0/typesp/311/membersp/4", "(BOU)", "mosaic_material_a", "y,89:33,89:50"),
    ("/miscsp/0/typesp/311/membersp/5", "(BOU)", "palette_generation", "y,90:33,90:51"),
    ("/miscsp/0/typesp/311/membersp/6", "(BOU)", "palette_slot", "y,91:33,91:45"),
    ("/miscsp/0/typesp/311/membersp/7", "(BOU)", "sample_count", "y,92:33,92:45"),
    ("/miscsp/0/typesp/311/membersp/8", "(BOU)", "aux_required", "y,93:33,93:45"),
    ("/miscsp/0/typesp/311/membersp/9", "(BOU)", "response_class", "y,94:33,94:47"),
    ("/miscsp/0/typesp/317/membersp/0", "(DOU)", "owner", "y,100:33,100:38"),
    ("/miscsp/0/typesp/317/membersp/1", "(DOU)", "descriptor", "y,101:33,101:43"),
    ("/miscsp/0/typesp/317/membersp/2", "(DOU)", "u", "y,102:33,102:34"),
    ("/miscsp/0/typesp/317/membersp/3", "(DOU)", "v", "y,103:33,103:34"),
    ("/miscsp/0/typesp/317/membersp/4", "(DOU)", "force_refuse", "y,104:33,104:45"),
    ("/miscsp/0/typesp/329/membersp/0", "(FOU)", "handle", "y,108:33,108:39"),
    ("/miscsp/0/typesp/329/membersp/1", "(FOU)", "page_generation", "y,109:33,109:48"),
    ("/miscsp/0/typesp/329/membersp/10", "(FOU)", "witness_palette_generation", "y,118:33,118:59"),
    ("/miscsp/0/typesp/329/membersp/2", "(FOU)", "selector_overflow", "y,110:33,110:50"),
    ("/miscsp/0/typesp/329/membersp/3", "(FOU)", "force_refuse", "y,111:33,111:45"),
    ("/miscsp/0/typesp/329/membersp/4", "(FOU)", "binding_selector", "y,112:33,112:49"),
    ("/miscsp/0/typesp/329/membersp/5", "(FOU)", "u", "y,113:33,113:34"),
    ("/miscsp/0/typesp/329/membersp/6", "(FOU)", "v", "y,114:33,114:34"),
    ("/miscsp/0/typesp/329/membersp/7", "(FOU)", "lod_q4_4", "y,115:33,115:41"),
    ("/miscsp/0/typesp/329/membersp/8", "(FOU)", "witness_class", "y,116:33,116:46"),
    ("/miscsp/0/typesp/329/membersp/9", "(FOU)", "witness_palette_slot", "y,117:33,117:53"),
    ("/miscsp/0/typesp/354/membersp/0", "(XPT)", "active_page_generation", "x,86:36,86:58"),
    ("/miscsp/0/typesp/354/membersp/1", "(XPT)", "binding_selector", "x,87:36,87:52"),
    ("/miscsp/0/typesp/354/membersp/10", "(XPT)", "lod_q4_4", "x,96:36,96:44"),
    ("/miscsp/0/typesp/354/membersp/11", "(XPT)", "aux_context", "x,97:36,97:47"),
    ("/miscsp/0/typesp/354/membersp/2", "(XPT)", "mosaic_weight", "x,88:36,88:49"),
    ("/miscsp/0/typesp/354/membersp/3", "(XPT)", "mosaic_material_b", "x,89:36,89:53"),
    ("/miscsp/0/typesp/354/membersp/4", "(XPT)", "mosaic_material_a", "x,90:36,90:53"),
    ("/miscsp/0/typesp/354/membersp/5", "(XPT)", "palette_generation", "x,91:36,91:54"),
    ("/miscsp/0/typesp/354/membersp/6", "(XPT)", "palette_slot", "x,92:36,92:48"),
    ("/miscsp/0/typesp/354/membersp/7", "(XPT)", "sample_count", "x,93:36,93:48"),
    ("/miscsp/0/typesp/354/membersp/8", "(XPT)", "aux_required", "x,94:36,94:48"),
    ("/miscsp/0/typesp/354/membersp/9", "(XPT)", "response_class", "x,95:36,95:50"),
)


def expected_duplicate_markers() -> tuple[interface.VerilatorDuplicateMarker, ...]:
    return tuple(
        interface.VerilatorDuplicateMarker(
            json_pointer=pointer,
            parent_struct_addr=parent,
            member_name=name,
            loc=loc,
        )
        for pointer, parent, name, loc in EXPECTED_DUPLICATE_MARKER_ROWS
    )


# WHAT MOVES WHEN THE V3 CLOSURE CHANGES, AND WHAT MUST NOT.
#
# The table above is the durable record: for each duplicate, WHICH member of
# WHICH struct it is and WHERE it is written (`file,line:col,line:col` in the
# byte-pinned package). That identity is the property worth guarding, and it is
# unchanged by RTL edits elsewhere in the closure.
#
# Three things in a Verilator tree move without the design's meaning changing:
#
#   * the `typesp/N` container index and the mangled parent address such as
#     `(CUOB)`, which are elaboration ORDER and shift whenever any module in the
#     closure gains or loses a type;
#   * the LINE NUMBERS inside a `loc`, when a struct is pushed down its own file
#     by an edit above it -- Timing4's `rd_logical_raw_o` port moved every
#     early-descriptor row by exactly +10.
#
# What must never move is which member of which struct is duplicated, its column
# span, and the order. Re-deriving the production rows through three explicit
# remaps keeps the oracle hand-written and independent of the parser, and makes
# each shift a visible reviewed edit instead of a silently re-pinned hash.
#
# The per-file offset is deliberately uniform per file and applied to BOTH ends
# of the span: a struct that moved has all of its members move together. If one
# member of a file shifted differently from its neighbours, that is not a
# relocation, and the assertion below refuses it.
#
# If a member NAME, COLUMN or ORDER ever differs, do not extend these tables:
# the set of duplicated members has actually changed and needs a decision.
# RE-DERIVED 2026-09-18 (second time that day) for the binding resolver's
# stored legality bit and pipelined CRC verdict. Verilator reassigns these
# internal address labels whenever declarations in the closure change, so all
# sixteen moved even though every member NAME and LOCATION stayed put and the
# marker count stayed at 105.
#
# Learned by replaying the manifest's own `elaboration.argv` and matching each
# row on (member_name, loc) -- properties of the SOURCE, which did not move --
# then reading back whatever index and address the tree assigned. All 105 rows
# matched, with no ambiguous mapping and none left over. The digest is whatever
# falls out of that; see the warning below about never working the other way.
_DSP_PRODUCTION_PARENT_ADDRS = {
    "(WTOB)": "(DFQB)", "(OUOB)": "(VFQB)", "(WVOB)": "(DHQB)",
    "(GUJ)": "(GYJ)", "(NZOB)": "(UKQB)", "(JAPB)": "(QLQB)",
    "(LEPB)": "(SPQB)", "(BGPB)": "(IRQB)", "(FHPB)": "(MSQB)",
    "(IYV)": "(QIW)", "(KYV)": "(SIW)", "(MYV)": "(UIW)",
    "(BOU)": "(JYU)", "(DOU)": "(LYU)", "(FOU)": "(NYU)",
    "(XPT)": "(AAU)",
}
# EXTENDED 2026-09-18 for the binding banks' move into M10K, then RE-DERIVED
# later the same day for the stored legality bit and the pipelined CRC verdict.
#
# The M10K change moved four containers by +3 (311: 306 -> 309, 317: 312 -> 315,
# 329: 324 -> 327, 354: 349 -> 352) and the rest not at all. The later work
# moves those same four back to 306/312/324/349, and the reason is worth
# recording because it looks like a revert and is not:
#
#   the stored legality bit was first written as a packed struct, which
#   contributes THREE typesp entries -- the struct dtype and its two members --
#   and also took the duplicate-marker count from 105 to 107. It is a packed
#   vector instead, for the reason recorded in the parser's pin, and dropping
#   the struct removes exactly those three entries.
#
# So the indices coincide with an earlier state without the design having
# returned to it. Observed, not assumed: all 105 rows re-matched on
# (member_name, loc) with no ambiguity and none left over.
#
# Derived by replaying the elaboration the manifest itself records -- its
# `elaboration.argv`, parameter overrides included -- and reading the actual
# containers back, not by adjusting numbers until the digest matched. Fitting a
# remap to a target digest would make this oracle agree with the tool by
# construction, which is the one thing an INDEPENDENT oracle must never do.
_DSP_PRODUCTION_TYPESP_REMAP = {
    107: 105, 108: 106, 109: 107, 110: 108, 111: 109, 112: 110,
    113: 111, 114: 112, 115: 113, 255: 249, 267: 261, 276: 270,
    311: 306, 317: 312, 329: 324, 354: 349,
}
# Source-file tag -> line offset. Only `x`, the early-descriptor source, moved.
_DSP_PRODUCTION_LOC_LINE_OFFSETS = {"e": 0, "x": 10, "y": 0, "z": 0}

_LOC_PATTERN = re.compile(r"^([A-Za-z]+),(\d+):(\d+),(\d+):(\d+)$")


def _remap_production_pointer(pointer: str) -> str:
    match = re.fullmatch(r"/miscsp/0/typesp/(\d+)/membersp/(\d+)", pointer)
    if match is None:
        raise AssertionError(f"unexpected duplicate-marker pointer shape: {pointer}")
    container = int(match.group(1))
    if container not in _DSP_PRODUCTION_TYPESP_REMAP:
        raise AssertionError(f"no production remap for typesp/{container}")
    return "/miscsp/0/typesp/%d/membersp/%s" % (
        _DSP_PRODUCTION_TYPESP_REMAP[container], match.group(2))


def _remap_production_loc(loc: str) -> str:
    match = _LOC_PATTERN.fullmatch(loc)
    if match is None:
        raise AssertionError(f"unexpected duplicate-marker loc shape: {loc}")
    tag, start_line, start_col, end_line, end_col = match.groups()
    if tag not in _DSP_PRODUCTION_LOC_LINE_OFFSETS:
        raise AssertionError(f"no production line offset for source tag {tag!r}")
    offset = _DSP_PRODUCTION_LOC_LINE_OFFSETS[tag]
    return "%s,%d:%s,%d:%s" % (
        tag, int(start_line) + offset, start_col, int(end_line) + offset, end_col)


EXPECTED_PRODUCTION_DUPLICATE_MARKER_ROWS = tuple(
    (_remap_production_pointer(pointer),
     _DSP_PRODUCTION_PARENT_ADDRS[parent], name, _remap_production_loc(loc))
    for pointer, parent, name, loc in _PRE_DSP_PRODUCTION_DUPLICATE_MARKER_ROWS
)


def expected_production_duplicate_markers() -> tuple[interface.VerilatorDuplicateMarker, ...]:
    return tuple(
        interface.VerilatorDuplicateMarker(
            json_pointer=pointer,
            parent_struct_addr=parent,
            member_name=name,
            loc=loc,
        )
        for pointer, parent, name, loc in EXPECTED_PRODUCTION_DUPLICATE_MARKER_ROWS
    )


def recanonical(payload: dict[str, object]) -> bytes:
    hashes = payload["hashes"]
    assert isinstance(hashes, dict)
    hashes["canonical_interface_sha256"] = "1" * 64
    hashes["canonical_interface_sha256"] = interface.canonical_interface_sha256(payload)
    return interface.canonical_json_bytes(payload)


def manifest_copy(raw: bytes) -> dict[str, object]:
    return copy.deepcopy(interface.load_manifest_bytes(raw))


def fixture_qualified_constants():
    snapshots = interface.SnapshotSet.capture(REPO, FIXTURE_CLOSURE)
    return interface.resolve_package_integral_constants(
        snapshots, FIXTURE_CLOSURE
    )


def load_quiet_fixture() -> tuple[
    list[dict[str, object]],
    list[dict[str, str]],
    list[dict[str, str]],
]:
    payload = interface.load_quiet_fixture_bytes(QUIET_CONTROLS.read_bytes())
    return (
        copy.deepcopy(payload["aliases"]),
        copy.deepcopy(payload["controls"]),
        copy.deepcopy(payload["expression_leaf_controls"]),
    )


def quiet_equation_source(
    controls: list[dict[str, str]],
    *,
    omit: int | None = None,
    invert: int | None = None,
) -> str:
    equations: list[str] = []
    for equation in ("data_quiet", "quiet_o"):
        terms: list[str] = []
        for ordinal, row in enumerate(controls):
            if row["equation"] != equation or ordinal == omit:
                continue
            polarity = row["polarity"]
            if ordinal == invert:
                polarity = "negative" if polarity == "positive" else "positive"
            terms.append(interface.quiet_term_text(row["operand"], polarity))
        equations.append(f"assign {equation} =\n    " + "\n && ".join(terms) + ";\n")
    return "\n".join(equations)


def full_quiet_source(
    aliases: list[dict[str, object]],
    controls: list[dict[str, str]],
    *,
    omit_declaration: int | None = None,
    mapping_mutant: int | None = None,
    width_mutant: int | None = None,
) -> str:
    declarations: list[str] = []
    for ordinal, row in enumerate(aliases):
        name = str(row["name"])
        width = int(row["width"])
        source = str(row["source_expression"])
        if ordinal == width_mutant:
            width = 2 if width == 1 else 1
        if ordinal != omit_declaration:
            dimension = "" if width == 1 else f"[{width - 1}:0] "
            declarations.append(f"logic {dimension}{name};\n")
        if ordinal == mapping_mutant:
            source = "1'b1" if source != "1'b1" else "1'b0"
        declarations.append(f"assign {name} = {source};\n")
    return "".join(declarations) + quiet_equation_source(controls)


class SourceDeclarationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.raw = FIXTURE.read_bytes()
        cls.snapshots = interface.SnapshotSet.capture(REPO, FIXTURE_CLOSURE)
        cls.qualified_constants = interface.resolve_package_integral_constants(
            cls.snapshots, FIXTURE_CLOSURE
        )
        cls.view = interface.parse_module_source(
            cls.raw,
            source_path=FIXTURE_PATH,
            module_name="zhao_texture_interface_schema_fixture",
            qualified_constants=cls.qualified_constants,
        )

    def test_parameter_order_type_default_and_selected_value(self) -> None:
        self.assertEqual(len(self.view.parameters), 1)
        parameter = self.view.parameters[0]
        self.assertEqual(parameter.ordinal, 0)
        self.assertEqual(parameter.name, "W")
        self.assertEqual(parameter.declared_kind, "parameter")
        self.assertEqual(parameter.declared_type, "int unsigned")
        self.assertEqual(parameter.source_default_expression, "96")
        self.assertEqual(parameter.selected_value.kind, "unsigned_integer")
        self.assertEqual(parameter.selected_value.text, "96")

    def test_source_order_kind_signedness_and_dimensions(self) -> None:
        ports = self.view.ports
        self.assertEqual([port.name for port in ports], [
            "clk_i", "wide_i", "ascending_signed_i", "package_wide_i",
            "counters_o",
        ])
        self.assertEqual([port.ordinal for port in ports], list(range(5)))
        self.assertEqual(ports[0].direction, "input")
        self.assertEqual(ports[0].net_or_var, "net")
        self.assertEqual(ports[0].bit_width, 1)
        self.assertEqual(ports[0].source_expression, "input wire logic clk_i")
        self.assertEqual(ports[1].net_or_var, "variable")
        self.assertEqual(ports[1].bit_width, 96)
        self.assertEqual(
            [(d.left, d.right, d.direction, d.source_expression) for d in ports[1].packed_dimensions],
            [(95, 0, "descending", "[W-1:0]")],
        )
        self.assertTrue(ports[2].signed)
        self.assertEqual(
            [(d.left, d.right, d.direction) for d in ports[2].packed_dimensions],
            [(0, 7, "ascending")],
        )
        self.assertEqual(ports[3].direction, "input")
        self.assertEqual(ports[3].bit_width, 224)
        self.assertEqual(
            ports[3].packed_dimensions[0].source_expression,
            "[zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]",
        )
        self.assertEqual(ports[4].direction, "output")
        self.assertEqual(ports[4].net_or_var, "variable")
        self.assertEqual(ports[4].element_width, 32)
        self.assertEqual(ports[4].unpacked_element_count, 6)
        self.assertEqual(ports[4].bit_width, 192)
        self.assertEqual(
            [(d.left, d.right, d.direction) for d in ports[4].unpacked_dimensions],
            [(2, 1, "descending"), (5, 7, "ascending")],
        )

    def test_module_byte_span_and_lf_normalized_hash_are_exact(self) -> None:
        span = self.raw[
            self.view.declaration_start_byte:self.view.declaration_end_byte_exclusive
        ]
        self.assertTrue(span.startswith(b"module zhao_texture_interface_schema_fixture"))
        self.assertTrue(span.endswith(b");"))
        normalized = span.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
        self.assertEqual(
            self.view.module_declaration_sha256,
            hashlib.sha256(normalized.encode("utf-8")).hexdigest(),
        )

    def test_parameter_override_is_elaboration_canonical(self) -> None:
        view = interface.parse_module_source(
            self.raw,
            source_path=FIXTURE_PATH,
            module_name="zhao_texture_interface_schema_fixture",
            overrides={"W": "80"},
            qualified_constants=self.qualified_constants,
        )
        self.assertEqual(view.parameters[0].selected_value.text, "80")
        self.assertEqual(view.ports[1].bit_width, 80)

    def test_singleton_dimension_is_canonically_descending(self) -> None:
        source = b"module singleton_fixture(input logic [3:3] one_i); endmodule\n"
        view = interface.parse_module_source(
            source,
            source_path="tests/singleton_fixture.sv",
            module_name="singleton_fixture",
        )
        dimension = view.ports[0].packed_dimensions[0]
        self.assertEqual((dimension.left, dimension.right, dimension.direction), (3, 3, "descending"))

    def test_unknown_parameter_override_fails_closed(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "absent"):
            interface.parse_module_source(
                self.raw,
                source_path=FIXTURE_PATH,
                module_name="zhao_texture_interface_schema_fixture",
                overrides={"NOT_W": "80"},
                qualified_constants=self.qualified_constants,
            )

    def test_bit_vector_parameter_width_can_depend_on_an_earlier_parameter(self) -> None:
        source = b"""module dependent_parameter_fixture #(\n    parameter int unsigned W = 5,\n    parameter logic [W-1:0] MASK = 5'h0f\n) (input logic clk_i);\nendmodule\n"""
        view = interface.parse_module_source(
            source,
            source_path="tests/dependent_parameter_fixture.sv",
            module_name="dependent_parameter_fixture",
        )
        self.assertEqual(
            (view.parameters[1].selected_value.kind, view.parameters[1].selected_value.text),
            ("bit_vector", "5'h0f"),
        )

    def test_canonical_parameter_kinds(self) -> None:
        unsigned, _ = interface.canonical_parameter_value("int unsigned", "17", {})
        signed, _ = interface.canonical_parameter_value("int signed", "-17", {})
        vector, _ = interface.canonical_parameter_value("logic [4:0]", "5'h0f", {})
        string, _ = interface.canonical_parameter_value("string", '"café"', {})
        self.assertEqual((unsigned.kind, unsigned.text), ("unsigned_integer", "17"))
        self.assertEqual((signed.kind, signed.text), ("signed_integer", "-17"))
        self.assertEqual((vector.kind, vector.text), ("bit_vector", "5'h0f"))
        self.assertEqual((string.kind, string.text), ("string", "café"))


class PackageConstantResolutionTests(unittest.TestCase):
    def test_sized_signed_literals_follow_systemverilog_value_semantics(self) -> None:
        self.assertEqual(interface.eval_sv_integer("8'shFF"), -1)
        self.assertEqual(interface.eval_sv_integer("4'sb1000"), -8)
        self.assertEqual(interface.eval_sv_integer("4'h1F"), 15)
        self.assertEqual(interface._verilator_constant("8'shff"), -1)
        self.assertEqual(interface._verilator_constant("4'sb1000"), -8)
        self.assertEqual(interface._verilator_constant("8'hff"), 255)
        selected, semantic = interface.canonical_parameter_value(
            "logic signed [7:0]", "8'shFF", {}
        )
        self.assertEqual((selected.kind, selected.text, semantic),
                         ("bit_vector", "8'hff", -1))
        selected, semantic = interface.canonical_parameter_value(
            "logic signed [7:0]", "8'hFF", {}
        )
        self.assertEqual((selected.kind, selected.text, semantic),
                         ("bit_vector", "8'hff", -1))
        with self.assertRaisesRegex(interface.InterfaceManifestError,
                                    "unsized signed literal"):
            interface.eval_sv_integer("'shFF")

    def test_real_elaboration_accepts_signed_packed_vector_parameter(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-signed-parameter-") as temporary:
            repo = Path(temporary)
            source = repo / "rtl" / "signed_parameter.sv"
            source.parent.mkdir(parents=True)
            source.write_text(
                "module signed_parameter #(\n"
                "  parameter logic signed [7:0] P = 8'shFF\n"
                ") (input logic bit_i); endmodule\n",
                encoding="utf-8",
            )
            for spec in interface.CUSTOM_TOOL_SPECS.values():
                relative = Path(*spec["path"].split("/"))
                destination = repo / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes((REPO / relative).read_bytes())
            artifact = interface.build_manifest_artifact(
                repo_root=repo,
                top_module="signed_parameter",
                source_paths=["rtl/signed_parameter.sv"],
                production=False,
                verilator=str(interface.find_verilator(REPO)),
            )
            artifact.verify_live_unchanged()
            parameter = artifact.payload["parameters"][0]
            self.assertEqual(parameter["selected_value"],
                             {"kind": "bit_vector", "text": "8'hff"})

    def resolve_fixture(
        self,
        package_sources: list[tuple[str, str]],
        module_text: str,
    ):
        with tempfile.TemporaryDirectory(prefix="zhao-package-constant-") as temporary:
            repo = Path(temporary)
            (repo / "rtl").mkdir(parents=True, exist_ok=True)
            paths: list[str] = []
            for name, text in package_sources:
                path = f"rtl/{name}.sv"
                destination = repo / Path(*path.split("/"))
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_text(text, encoding="utf-8")
                paths.append(path)
            module_path = "rtl/top.sv"
            (repo / "rtl" / "top.sv").write_text(module_text, encoding="utf-8")
            paths.append(module_path)
            snapshots = interface.SnapshotSet.capture(repo, paths)
            return interface.resolve_package_integral_constants(snapshots, paths)

    def test_committed_package_first_fixture_resolves_qualified_width(self) -> None:
        constants = fixture_qualified_constants()
        self.assertEqual(
            constants["zhao_render_texture_pkg::AUX_SURFACE_CTX_W"],
            224,
        )
        view = interface.parse_module_source(
            FIXTURE.read_bytes(),
            source_path=FIXTURE_PATH,
            module_name="zhao_texture_interface_schema_fixture",
            qualified_constants=constants,
        )
        package_port = view.ports[3]
        self.assertEqual(package_port.name, "package_wide_i")
        self.assertEqual(package_port.bit_width, 224)

    def test_renamed_package_constant_control_fires(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "unknown package integral constant"):
            self.resolve_fixture(
                [("p", "package p; localparam int unsigned WIDTH_RENAMED = 8; endpackage\n")],
                "module top(input logic [p::WIDTH-1:0] value_i); endmodule\n",
            )

    def test_unknown_package_control_fires(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "unknown package integral constant"):
            self.resolve_fixture(
                [],
                "module top(input logic [missing_pkg::WIDTH-1:0] value_i); endmodule\n",
            )

    def test_nonintegral_package_constant_fires(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "nonintegral"):
            self.resolve_fixture(
                [("p", 'package p; localparam string WIDTH = "eight"; endpackage\n')],
                "module top(input logic [p::WIDTH-1:0] value_i); endmodule\n",
            )

    def test_cyclic_package_constants_fire(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "cyclic"):
            self.resolve_fixture(
                [(
                    "p",
                    "package p; localparam int A = B; localparam int B = A; endpackage\n",
                )],
                "module top(input logic [p::A-1:0] value_i); endmodule\n",
            )

    def test_ambiguous_package_declarations_fire(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "ambiguous package"):
            self.resolve_fixture(
                [
                    ("p_one", "package p; localparam int WIDTH = 8; endpackage\n"),
                    ("p_two", "package p; localparam int WIDTH = 9; endpackage\n"),
                ],
                "module top(input logic [p::WIDTH-1:0] value_i); endmodule\n",
            )


class CanonicalDigestOracleTests(unittest.TestCase):
    def test_fixed_literal_canonical_bytes_and_digest(self) -> None:
        manual = copy.deepcopy(CANONICAL_ORACLE_PAYLOAD)
        removed = manual["hashes"].pop("canonical_interface_sha256")
        self.assertEqual(removed, "ignored")
        self.assertEqual(
            set(manual["hashes"]),
            {"module_declaration_sha256", "top_source_sha256"},
        )
        self.assertEqual(interface.canonical_json_bytes(manual), CANONICAL_ORACLE_BYTES)
        self.assertEqual(
            hashlib.sha256(CANONICAL_ORACLE_BYTES).hexdigest(),
            CANONICAL_ORACLE_SHA256,
        )
        self.assertEqual(
            interface.canonical_interface_sha256(CANONICAL_ORACLE_PAYLOAD),
            CANONICAL_ORACLE_SHA256,
        )

    def test_only_self_digest_is_omitted(self) -> None:
        changed_self = copy.deepcopy(CANONICAL_ORACLE_PAYLOAD)
        changed_self["hashes"]["canonical_interface_sha256"] = "different self value"
        self.assertEqual(
            interface.canonical_interface_sha256(changed_self),
            CANONICAL_ORACLE_SHA256,
        )
        for sibling in ("module_declaration_sha256", "top_source_sha256"):
            with self.subTest(sibling=sibling):
                changed = copy.deepcopy(CANONICAL_ORACLE_PAYLOAD)
                changed["hashes"][sibling] = "f" * 64
                self.assertNotEqual(
                    interface.canonical_interface_sha256(changed),
                    CANONICAL_ORACLE_SHA256,
                )


class SnapshotConsistencyTests(unittest.TestCase):
    def test_shadow_copy_and_hash_use_the_immutable_first_read(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-snapshot-") as temporary:
            repo = Path(temporary) / "repo"
            shadow = Path(temporary) / "shadow"
            (repo / "rtl").mkdir(parents=True)
            (shadow / "rtl").mkdir(parents=True)
            source = repo / "rtl" / "top.sv"
            source.write_bytes(b"first bytes\n")
            snapshots = interface.SnapshotSet.capture(repo, ["rtl/top.sv"])
            source.write_bytes(b"second bytes\n")
            interface._copy_shadow_sources(shadow, snapshots, ["rtl/top.sv"])
            self.assertEqual((shadow / "rtl" / "top.sv").read_bytes(), b"first bytes\n")
            self.assertEqual(
                snapshots["rtl/top.sv"].sha256,
                hashlib.sha256(b"first bytes\n").hexdigest(),
            )
            with self.assertRaisesRegex(interface.InterfaceManifestError, "changed"):
                snapshots.verify_live_unchanged()

    def test_final_snapshot_recheck_prevents_output_replacement(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-snapshot-race-") as temporary:
            repo = Path(temporary) / "repo"
            (repo / "rtl").mkdir(parents=True)
            (repo / "tools" / "rtl").mkdir(parents=True)
            source_path = "rtl/top.sv"
            source = repo / "rtl" / "top.sv"
            source.write_text("module top(input logic clk_i); endmodule\n", encoding="utf-8")
            for spec in interface.CUSTOM_TOOL_SPECS.values():
                path = repo / Path(*spec["path"].split("/"))
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f"tool {spec['name']}\n", encoding="utf-8")
            parsed = interface.parse_module_source(
                source.read_bytes(), source_path=source_path, module_name="top"
            )
            elaborated = interface.ElaborationView(
                parameters=(),
                ports=(
                    interface.ElaboratedPort(
                        ordinal=0,
                        name="clk_i",
                        direction="input",
                        net_or_var="net",
                        bit_width=1,
                        element_width=1,
                        signed=False,
                        packed_ranges=(),
                        unpacked_ranges=(),
                        dtype_kind="BASICDTYPE",
                    ),
                ),
                module_source_paths=frozenset({source_path}),
            )

            def race_query(**kwargs: object) -> tuple[interface.ElaborationView, str, list[str]]:
                source.write_text(
                    "module top(input logic changed_i); endmodule\n", encoding="utf-8"
                )
                return (
                    elaborated,
                    interface.SUPPORTED_VERILATOR_VERSION,
                    interface.expected_elaboration_argv("top", [], [source_path]),
                )

            output = Path(temporary) / "kept.json"
            output.write_bytes(b"sentinel")
            with mock.patch.object(interface, "run_verilator_query", side_effect=race_query):
                with self.assertRaisesRegex(interface.InterfaceManifestError, "changed"):
                    generator.generate_to_path(
                        repo_root=repo,
                        output=output,
                        top_module="top",
                        source_paths=[source_path],
                        production=False,
                    )
            self.assertEqual(output.read_bytes(), b"sentinel")


class ToolAuthorityTests(unittest.TestCase):
    def load_verilator_text(self, text: str):
        with tempfile.TemporaryDirectory(prefix="zhao-verilator-duplicate-") as temporary:
            path = Path(temporary) / "tree.json"
            path.write_text(text, encoding="utf-8")
            return interface._load_verilator_json(path)

    @staticmethod
    def known_package_duplicate_json() -> str:
        return (
            '{"type":"NETLIST","name":"$root","modulesp":[],'
            '"miscsp":[{"type":"TYPETABLE","typesp":['
            '{"type":"STRUCTDTYPE","addr":"(S)","membersp":['
            '{"type":"MEMBERDTYPE","name":"in_tile_addr",'
            '"name":"in_tile_addr","addr":"(D)","loc":"e,33:18,33:30",'
            '"dtypep":"(B)"}]}]}]}'
        )

    def test_known_package_member_identical_name_shape_is_structurally_valid(self) -> None:
        root = self.load_verilator_text(self.known_package_duplicate_json())
        markers = interface.verilator_duplicate_markers(root)
        self.assertEqual(len(markers), 1)
        self.assertEqual(markers[0].member_name, "in_tile_addr")

    def test_identical_root_name_duplicate_is_rejected(self) -> None:
        raw = (
            '{"type":"NETLIST","name":"$root","name":"$root",'
            '"modulesp":[],"miscsp":[{"type":"TYPETABLE","typesp":[]}]}'
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "known package MEMBERDTYPE"):
            self.load_verilator_text(raw)

    def test_identical_module_name_duplicate_is_rejected(self) -> None:
        raw = (
            '{"type":"NETLIST","name":"$root","modulesp":['
            '{"type":"MODULE","name":"top","name":"top","addr":"(M)",'
            '"loc":"e,1:1,1:1","origName":"top","stmtsp":[]}],'
            '"miscsp":[{"type":"TYPETABLE","typesp":[]}]}'
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "known package MEMBERDTYPE"):
            self.load_verilator_text(raw)

    def test_identical_cell_name_duplicate_is_rejected(self) -> None:
        raw = (
            '{"type":"NETLIST","name":"$root","modulesp":['
            '{"type":"MODULE","name":"top","addr":"(M)",'
            '"loc":"e,1:1,1:1","origName":"top","stmtsp":['
            '{"type":"CELL","name":"u_child","name":"u_child",'
            '"modp":"(C)"}]}],"miscsp":[{"type":"TYPETABLE","typesp":[]}]}'
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "known package MEMBERDTYPE"):
            self.load_verilator_text(raw)

    def test_identical_top_level_package_name_duplicate_is_rejected(self) -> None:
        raw = (
            '{"type":"NETLIST","name":"$root","modulesp":['
            '{"type":"PACKAGE","name":"zhao_render_texture_pkg",'
            '"name":"zhao_render_texture_pkg","addr":"(P)",'
            '"loc":"e,1:1,1:1","origName":"zhao_render_texture_pkg",'
            '"stmtsp":[]}],"miscsp":[{"type":"TYPETABLE","typesp":[]}]}'
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "known package MEMBERDTYPE"):
            self.load_verilator_text(raw)

    def test_more_than_one_identical_name_duplicate_in_member_is_rejected(self) -> None:
        raw = self.known_package_duplicate_json().replace(
            '"name":"in_tile_addr","addr"',
            '"name":"in_tile_addr","name":"in_tile_addr","addr"',
            1,
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "duplicate key"):
            self.load_verilator_text(raw)

    def test_single_generic_package_member_cannot_activate_exception(self) -> None:
        root = self.load_verilator_text(self.known_package_duplicate_json())
        markers = interface.verilator_duplicate_markers(root)
        with self.assertRaisesRegex(interface.InterfaceManifestError, "fingerprint differs"):
            interface.validate_verilator_duplicate_fingerprint(
                markers,
                top_module=interface.SCHEMA_FIXTURE_TOP,
                purpose=interface.SCHEMA_FIXTURE_PURPOSE,
                verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
            )

    def test_independent_full_duplicate_fingerprint_oracle_is_accepted(self) -> None:
        markers = expected_duplicate_markers()
        self.assertEqual(len(markers), 41)
        self.assertEqual(
            interface.verilator_duplicate_marker_sha256(markers),
            EXPECTED_DUPLICATE_MARKER_SHA256,
        )
        self.assertEqual(
            hashlib.sha256((REPO / Path(*PACKAGE_PATH.split("/"))).read_bytes()).hexdigest(),
            EXPECTED_DUPLICATE_PACKAGE_SHA256,
        )
        self.assertEqual(
            interface.SUPPORTED_DUPLICATE_PROFILES,
            {
                (
                    "zhao_texture_interface_schema_fixture",
                    "schema_fixture",
                ): {
                    "count": 41,
                    "sha256": EXPECTED_DUPLICATE_MARKER_SHA256,
                },
                (
                    "zhao_texture_island_v3_top",
                    "production_interface",
                ): {
                    "count": 105,
                    "sha256": EXPECTED_PRODUCTION_DUPLICATE_MARKER_SHA256,
                },
            },
        )
        self.assertEqual(
            interface.SUPPORTED_DUPLICATE_PACKAGE_SHA256,
            EXPECTED_DUPLICATE_PACKAGE_SHA256,
        )
        interface.validate_verilator_duplicate_fingerprint(
            markers,
            top_module=interface.SCHEMA_FIXTURE_TOP,
            purpose=interface.SCHEMA_FIXTURE_PURPOSE,
            verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
            package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
        )

    def test_independent_production_duplicate_fingerprint_oracle_is_accepted(self) -> None:
        markers = expected_production_duplicate_markers()
        self.assertEqual(len(markers), 105)
        self.assertEqual(
            interface.verilator_duplicate_marker_sha256(markers),
            EXPECTED_PRODUCTION_DUPLICATE_MARKER_SHA256,
        )
        interface.validate_verilator_duplicate_fingerprint(
            markers,
            top_module=interface.PRODUCTION_TOP,
            purpose=interface.PRODUCTION_INTERFACE_PURPOSE,
            verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
            package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
        )

    def test_production_fingerprint_missing_extra_zero_and_field_mutants_fire(self) -> None:
        markers = expected_production_duplicate_markers()
        first = markers[0]
        cases = {
            "missing": markers[:-1],
            "extra": markers + (
                replace(first, json_pointer=first.json_pointer + "/extra"),
            ),
            "zero": (),
            "path": (replace(first, json_pointer=first.json_pointer + "x"),) + markers[1:],
            "parent": (replace(first, parent_struct_addr="(WRONG)"),) + markers[1:],
            "name": (replace(first, member_name=first.member_name + "_wrong"),) + markers[1:],
            "loc": (replace(first, loc="e,999:1,999:2"),) + markers[1:],
        }
        for control, mutant in cases.items():
            with self.subTest(control=control):
                with self.assertRaisesRegex(interface.InterfaceManifestError, "fingerprint differs"):
                    interface.validate_verilator_duplicate_fingerprint(
                        mutant,
                        top_module=interface.PRODUCTION_TOP,
                        purpose=interface.PRODUCTION_INTERFACE_PURPOSE,
                        verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                        package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
                    )

    def test_wrong_top_purpose_and_cross_profile_fingerprints_fire(self) -> None:
        schema_markers = expected_duplicate_markers()
        production_markers = expected_production_duplicate_markers()
        unsupported = (
            (schema_markers, interface.PRODUCTION_TOP, interface.PRODUCTION_INTERFACE_PURPOSE),
            (production_markers, interface.SCHEMA_FIXTURE_TOP, interface.SCHEMA_FIXTURE_PURPOSE),
            (schema_markers, "wrong_top", interface.SCHEMA_FIXTURE_PURPOSE),
            (schema_markers, interface.SCHEMA_FIXTURE_TOP, "wrong_purpose"),
            (schema_markers, "third_top", "third_profile"),
        )
        for markers, top_module, purpose in unsupported:
            with self.subTest(top_module=top_module, purpose=purpose):
                with self.assertRaises(interface.InterfaceManifestError):
                    interface.validate_verilator_duplicate_fingerprint(
                        markers,
                        top_module=top_module,
                        purpose=purpose,
                        verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                        package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
                    )

    def test_full_duplicate_fingerprint_missing_extra_and_field_mutants_fire(self) -> None:
        markers = expected_duplicate_markers()
        first = markers[0]
        cases = {
            "missing": markers[:-1],
            "extra": markers + (
                replace(first, json_pointer=first.json_pointer + "/extra"),
            ),
            "path": (replace(first, json_pointer=first.json_pointer + "x"),) + markers[1:],
            "parent": (replace(first, parent_struct_addr="(WRONG)"),) + markers[1:],
            "name": (replace(first, member_name=first.member_name + "_wrong"),) + markers[1:],
            "loc": (replace(first, loc="e,999:1,999:2"),) + markers[1:],
        }
        for control, mutant in cases.items():
            with self.subTest(control=control):
                with self.assertRaisesRegex(interface.InterfaceManifestError, "fingerprint differs"):
                    interface.validate_verilator_duplicate_fingerprint(
                        mutant,
                        top_module=interface.SCHEMA_FIXTURE_TOP,
                        purpose=interface.SCHEMA_FIXTURE_PURPOSE,
                        verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                        package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
                    )

    def test_pinned_package_zero_marker_bypass_is_rejected(self) -> None:
        with self.assertRaisesRegex(interface.InterfaceManifestError, "fingerprint differs"):
            interface.validate_verilator_duplicate_fingerprint(
                (),
                top_module=interface.SCHEMA_FIXTURE_TOP,
                purpose=interface.SCHEMA_FIXTURE_PURPOSE,
                verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
            )

    def test_known_profiles_cannot_take_generic_zero_marker_bypass(self) -> None:
        known_profiles = (
            (interface.SCHEMA_FIXTURE_TOP, interface.SCHEMA_FIXTURE_PURPOSE),
            (interface.PRODUCTION_TOP, interface.PRODUCTION_INTERFACE_PURPOSE),
        )
        for top_module, purpose in known_profiles:
            with self.subTest(top_module=top_module, purpose=purpose):
                with self.assertRaisesRegex(
                        interface.InterfaceManifestError, "package bytes differ"):
                    interface.validate_verilator_duplicate_fingerprint(
                        (),
                        top_module=top_module,
                        purpose=purpose,
                        verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                        package_sha256=None,
                    )

    def test_generic_elaboration_without_pinned_package_allows_zero_markers(self) -> None:
        interface.validate_verilator_duplicate_fingerprint(
            (),
            top_module=None,
            purpose=None,
            verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
            package_sha256=None,
        )

    def test_full_duplicate_fingerprint_version_and_package_byte_mutants_fire(self) -> None:
        markers = expected_duplicate_markers()
        with self.assertRaisesRegex(interface.InterfaceManifestError, "pinned Verilator version"):
            interface.validate_verilator_duplicate_fingerprint(
                markers,
                top_module=interface.SCHEMA_FIXTURE_TOP,
                purpose=interface.SCHEMA_FIXTURE_PURPOSE,
                verilator_version="Verilator 5.052",
                package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
            )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "package bytes differ"):
            interface.validate_verilator_duplicate_fingerprint(
                markers,
                top_module=interface.SCHEMA_FIXTURE_TOP,
                purpose=interface.SCHEMA_FIXTURE_PURPOSE,
                verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                package_sha256="0" * 64,
            )

    def test_explicit_missing_zhao_verilator_fails_without_fallback(self) -> None:
        missing = str(REPO / "deliberately-missing-verilator.exe")
        with mock.patch.dict(os.environ, {"ZHAO_VERILATOR": missing}, clear=False):
            with self.assertRaisesRegex(interface.InterfaceManifestError, "explicit ZHAO_VERILATOR"):
                interface.find_verilator(REPO)

    def test_explicit_invalid_verilator_root_fails_without_fallback(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-invalid-vroot-") as temporary:
            invalid_root = str(Path(temporary) / "missing" / "share" / "verilator")
            with mock.patch.dict(
                os.environ, {"VERILATOR_ROOT": invalid_root}, clear=True
            ):
                with self.assertRaisesRegex(
                    interface.InterfaceManifestError,
                    "explicit VERILATOR_ROOT has no derived Verilator executable",
                ):
                    interface.find_verilator(REPO)

    def test_custom_tool_versions_have_one_role_aware_authority(self) -> None:
        self.assertEqual(
            {role: spec["version"] for role, spec in interface.CUSTOM_TOOL_SPECS.items()},
            {"checker": "1.0.0", "generator": "1.0.0", "parser": "1.0.0"},
        )
        self.assertNotIn(
            "TOOL_VERSION =",
            (TOOLS / "gen_texture_v3_interface_manifest.py").read_text(encoding="utf-8"),
        )
        self.assertNotIn(
            "TOOL_VERSION =",
            (TOOLS / "check_texture_v3_interface_manifest.py").read_text(encoding="utf-8"),
        )

    def test_supported_verilator_version_is_exactly_pinned(self) -> None:
        interface.validate_supported_verilator_version(
            interface.SUPPORTED_VERILATOR_VERSION
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "unsupported Verilator version"):
            interface.validate_supported_verilator_version("Verilator 5.052")

    def test_verilator_tree_duplicate_key_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-verilator-json-duplicate-") as temporary:
            tree = Path(temporary) / "tree.json"
            tree.write_bytes(b'{"type":"NETLIST","type":"OTHER"}')
            with self.assertRaisesRegex(interface.InterfaceManifestError, "duplicate key"):
                interface._load_verilator_json(tree)

    def test_two_separate_qualifying_package_members_are_rejected(self) -> None:
        member_one = (
            '{"type":"MEMBERDTYPE","name":"first","name":"first",'
            '"addr":"(D1)","loc":"e,33:18,33:30","dtypep":"(B)"}'
        )
        member_two = (
            '{"type":"MEMBERDTYPE","name":"second","name":"second",'
            '"addr":"(D2)","loc":"e,34:18,34:24","dtypep":"(B)"}'
        )
        raw = (
            '{"type":"NETLIST","name":"$root","modulesp":[],'
            '"miscsp":[{"type":"TYPETABLE","typesp":['
            '{"type":"STRUCTDTYPE","addr":"(S)","membersp":['
            + member_one + ',' + member_two
            + ']}]}]}'
        )
        root = self.load_verilator_text(raw)
        markers = interface.verilator_duplicate_markers(root)
        self.assertEqual(len(markers), 2)
        with self.assertRaisesRegex(interface.InterfaceManifestError, "fingerprint differs"):
            interface.validate_verilator_duplicate_fingerprint(
                markers,
                top_module=interface.SCHEMA_FIXTURE_TOP,
                purpose=interface.SCHEMA_FIXTURE_PURPOSE,
                verilator_version=interface.SUPPORTED_VERILATOR_VERSION,
                package_sha256=EXPECTED_DUPLICATE_PACKAGE_SHA256,
            )

    def test_supported_verilator_json_schema_is_explicitly_validated(self) -> None:
        root = {
            "type": "NETLIST",
            "name": "$root",
            "modulesp": [
                {
                    "type": "MODULE",
                    "addr": "(TOP)",
                    "loc": "e,1:1,1:1",
                    "origName": "top",
                    "stmtsp": [],
                }
            ],
            "miscsp": [{"type": "TYPETABLE", "typesp": []}],
        }
        meta = {
            "files": {
                "e": {
                    "filename": "rtl/top.sv",
                    "realpath": "C:/shadow/rtl/top.sv",
                    "language": "1800-2023",
                }
            },
            "pointers": {},
            "ptrFieldNames": [],
        }
        interface.validate_verilator_json_schema(root, meta)
        bad_root = copy.deepcopy(root)
        del bad_root["modulesp"]
        with self.assertRaisesRegex(interface.InterfaceManifestError, "modulesp"):
            interface.validate_verilator_json_schema(bad_root, meta)
        bad_meta = copy.deepcopy(meta)
        bad_meta["schema_guess"] = 1
        with self.assertRaisesRegex(interface.InterfaceManifestError, "metadata members"):
            interface.validate_verilator_json_schema(root, bad_meta)


class GeneratedFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temporary = tempfile.TemporaryDirectory(prefix="zhao-interface-test-")
        cls.manifest = Path(cls.temporary.name) / "fixture.interface.json"
        cls.raw = generator.generate_to_path(
            repo_root=REPO,
            output=cls.manifest,
            top_module="zhao_texture_interface_schema_fixture",
            source_paths=FIXTURE_CLOSURE,
            parameter_overrides={},
            production=False,
        )
        cls.payload = interface.validate_manifest_bytes(
            cls.raw,
            expected_top="zhao_texture_interface_schema_fixture",
            expected_source_closure=FIXTURE_CLOSURE,
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary.cleanup()

    def test_generated_bytes_are_compact_canonical_and_have_no_trailing_lf(self) -> None:
        self.assertEqual(self.raw, self.manifest.read_bytes())
        self.assertEqual(self.raw, interface.canonical_json_bytes(self.payload))
        self.assertFalse(self.raw.startswith(b"\xef\xbb\xbf"))
        self.assertFalse(self.raw.endswith(b"\n"))
        self.assertNotIn(b": ", self.raw)

    def test_hash_rules_cover_raw_files_and_lf_declaration_only(self) -> None:
        hashes = self.payload["hashes"]
        closure = self.payload["source_closure"]
        self.assertEqual(hashes["top_source_sha256"], hashlib.sha256(FIXTURE.read_bytes()).hexdigest())
        self.assertEqual(closure[-1]["sha256"], hashes["top_source_sha256"])
        self.assertEqual(
            closure[0]["sha256"],
            hashlib.sha256((REPO / Path(*PACKAGE_PATH.split("/"))).read_bytes()).hexdigest(),
        )
        self.assertEqual(
            hashes["canonical_interface_sha256"],
            interface.canonical_interface_sha256(self.payload),
        )
        source = interface.parse_module_source(
            FIXTURE.read_bytes(),
            source_path=FIXTURE_PATH,
            module_name="zhao_texture_interface_schema_fixture",
            qualified_constants=fixture_qualified_constants(),
        )
        self.assertEqual(hashes["module_declaration_sha256"], source.module_declaration_sha256)

    def test_tool_hashes_versions_and_paths_are_live_data(self) -> None:
        tools = self.payload["tools"]
        for role, spec in interface.CUSTOM_TOOL_SPECS.items():
            with self.subTest(role=role):
                self.assertEqual(tools[role]["name"], spec["name"])
                self.assertEqual(tools[role]["path"], spec["path"])
                self.assertEqual(tools[role]["version"], "1.0.0")
                self.assertEqual(
                    tools[role]["sha256"],
                    hashlib.sha256((REPO / Path(*spec["path"].split("/"))).read_bytes()).hexdigest(),
                )
        self.assertEqual(tools["runtime"], {
            "name": "CPython", "version": platform.python_version()
        })
        self.assertEqual(tools["elaborator"]["name"], "Verilator")
        self.assertEqual(tools["elaborator"]["version_command"], ["verilator", "--version"])
        self.assertTrue(tools["elaborator"]["version"].startswith("Verilator "))

    def test_constituent_tool_hash_is_digest_sensitive(self) -> None:
        payload = manifest_copy(self.raw)
        original = payload["hashes"]["canonical_interface_sha256"]
        payload["tools"]["parser"]["sha256"] = "f" * 64
        self.assertNotEqual(interface.canonical_interface_sha256(payload), original)

    def test_elaboration_argv_and_all_effective_parameters_are_exact(self) -> None:
        self.assertEqual(self.payload["elaboration"], {
            "argv": [
                "verilator", "--json-only", "--top-module",
                "zhao_texture_interface_schema_fixture", "-GW=96",
                PACKAGE_PATH, FIXTURE_PATH,
            ],
            "cwd": ".",
            "parameter_overrides": [
                {"name": "W", "value": {"kind": "unsigned_integer", "text": "96"}}
            ],
            "top_module": "zhao_texture_interface_schema_fixture",
        })

    def test_elaboration_preserves_wide_signed_ascending_and_unpacked_ports(self) -> None:
        ports = {row["name"]: row for row in self.payload["ports"]}
        self.assertEqual(ports["wide_i"]["bit_width"], 96)
        self.assertEqual(ports["package_wide_i"]["bit_width"], 224)
        self.assertEqual(
            ports["package_wide_i"]["packed_dimensions"][0]["source_expression"],
            "[zhao_render_texture_pkg::AUX_SURFACE_CTX_W-1:0]",
        )
        self.assertTrue(ports["ascending_signed_i"]["signed"])
        self.assertEqual(ports["ascending_signed_i"]["packed_dimensions"][0]["direction"], "ascending")
        self.assertEqual(ports["counters_o"]["unpacked_element_count"], 6)
        self.assertEqual(ports["counters_o"]["bit_width"], 192)
        self.assertEqual(
            [(row["left"], row["right"]) for row in ports["counters_o"]["unpacked_dimensions"]],
            [(2, 1), (5, 7)],
        )

    def test_checker_repeats_source_and_elaboration_query_without_writing(self) -> None:
        before = self.manifest.read_bytes()
        checked = checker.check_manifest(
            repo_root=REPO,
            manifest_path=self.manifest,
            top_module="zhao_texture_interface_schema_fixture",
            source_paths=FIXTURE_CLOSURE,
            parameter_overrides={},
            production=False,
        )
        self.assertEqual(checked, self.payload)
        self.assertEqual(self.manifest.read_bytes(), before)

    def test_checker_rejects_manifest_replaced_during_elaboration(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-interface-check-race-") as temporary:
            manifest = Path(temporary) / "manifest.json"
            manifest.write_bytes(self.raw)
            rebuilt = mock.Mock()
            rebuilt.payload = self.payload

            def replace_manifest(**_kwargs):
                manifest.write_bytes(self.raw + b"\n")
                return rebuilt

            with mock.patch.object(
                    checker, "build_manifest_artifact",
                    side_effect=replace_manifest):
                with self.assertRaisesRegex(
                        interface.InterfaceManifestError, "manifest changed"):
                    checker.check_manifest(
                        repo_root=REPO,
                        manifest_path=manifest,
                        top_module="zhao_texture_interface_schema_fixture",
                        source_paths=FIXTURE_CLOSURE,
                        parameter_overrides={},
                        production=False,
                    )
            rebuilt.verify_live_unchanged.assert_called_once_with()

    def test_generator_does_not_touch_existing_output_on_failure(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-interface-no-write-") as temporary:
            output = Path(temporary) / "kept.json"
            sentinel = b"do not replace me"
            output.write_bytes(sentinel)
            with self.assertRaises(interface.InterfaceManifestError):
                generator.generate_to_path(
                    repo_root=REPO,
                    output=output,
                    top_module="missing_top",
                    source_paths=["tests/rtl/fixtures/does_not_exist.sv"],
                    production=False,
                )
            self.assertEqual(output.read_bytes(), sentinel)
            self.assertEqual(list(output.parent.glob(output.name + ".*.tmp")), [])


class ManifestClosedSchemaNegativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.raw = GeneratedFixtureTests.raw
        cls.payload = GeneratedFixtureTests.payload

    def assert_rejected(self, raw: bytes, message: str | None = None) -> None:
        context = self.assertRaises(interface.InterfaceManifestError)
        with context:
            interface.validate_manifest_bytes(
                raw,
                expected_top="zhao_texture_interface_schema_fixture",
                expected_source_closure=FIXTURE_CLOSURE,
            )
        if message is not None:
            self.assertIn(message, str(context.exception))

    def test_duplicate_key_is_rejected_before_object_collapse(self) -> None:
        mutated = self.raw.replace(b'"cwd":"."', b'"cwd":".","cwd":"."', 1)
        self.assert_rejected(mutated, "duplicate")

    def test_singleton_dimension_cannot_claim_ascending(self) -> None:
        payload = manifest_copy(self.raw)
        port = payload["ports"][1]
        dimension = port["packed_dimensions"][0]
        dimension.update({
            "direction": "ascending",
            "left": 0,
            "right": 0,
            "size": 1,
            "source_expression": "[0:0]",
        })
        port["element_width"] = 1
        port["bit_width"] = 1
        self.assert_rejected(recanonical(payload), "singleton")

    def test_trailing_newline_is_rejected(self) -> None:
        self.assert_rejected(self.raw + b"\n", "not canonical")

    def test_invalid_utf8_is_rejected(self) -> None:
        self.assert_rejected(self.raw[:-1] + b"\xff", "valid UTF-8")

    def test_wrong_recorded_elaborator_version_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["tools"]["elaborator"]["version"] = "Verilator 5.052"
        self.assert_rejected(recanonical(payload), "pinned supported Verilator version")

    def test_utf8_bom_is_rejected(self) -> None:
        self.assert_rejected(b"\xef\xbb\xbf" + self.raw, "BOM")

    def test_float_is_rejected(self) -> None:
        mutated = self.raw.replace(b'"schema_version":1', b'"schema_version":1.0', 1)
        self.assert_rejected(mutated, "floating-point")

    def test_null_is_rejected(self) -> None:
        mutated = self.raw.replace(b'"schema_version":1', b'"schema_version":null', 1)
        self.assert_rejected(mutated, "integer")

    def test_non_nfc_string_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["parameters"][0]["source_default_expression"] = "é"
        self.assert_rejected(recanonical(payload), "not NFC")

    def test_extra_root_member_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["timestamp"] = "2026-09-13"
        self.assert_rejected(recanonical(payload), "extra")

    def test_extra_nested_member_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["ports"][0]["helpful_guess"] = False
        self.assert_rejected(recanonical(payload), "extra")

    def test_missing_nested_member_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        del payload["ports"][0]["signed"]
        self.assert_rejected(recanonical(payload), "missing")

    def test_all_zero_prose_digest_sentinel_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["source_closure"][0]["sha256"] = "0" * 64
        payload["hashes"]["top_source_sha256"] = "0" * 64
        self.assert_rejected(recanonical(payload), "sentinel")

    def test_self_hash_retained_algorithm_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["hashes"]["canonical_interface_sha256"] = hashlib.sha256(
            interface.canonical_json_bytes(payload)
        ).hexdigest()
        self.assert_rejected(interface.canonical_json_bytes(payload), "only its own member omitted")

    def test_noncanonical_bit_vector_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        value = {"kind": "bit_vector", "text": "5'hF"}
        payload["parameters"][0]["selected_value"] = value
        payload["elaboration"]["parameter_overrides"][0]["value"] = value
        payload["elaboration"]["argv"][4] = "-GW=5'hF"
        self.assert_rejected(recanonical(payload), "canonical bit vector")

    def test_reordered_closure_is_rejected(self) -> None:
        payload = manifest_copy(self.raw)
        payload["source_closure"][0]["path"] = "tests/rtl/fixtures/other_package.sv"
        payload["elaboration"]["argv"][-2] = "tests/rtl/fixtures/other_package.sv"
        self.assert_rejected(recanonical(payload), "source_closure order differs")


class RootAbiPortTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = interface.parse_module_source(
            b"""module root_abi_fixture (\n    input logic frame_fault_clear_valid_i,\n    output logic frame_fault_clear_ready_o,\n    output logic frame_fault_o\n);\nendmodule\n""",
            source_path="tests/root_abi_fixture.sv",
            module_name="root_abi_fixture",
        )

    def test_three_exact_frame_fault_ports_pass(self) -> None:
        interface.audit_required_root_ports(self.source)

    def test_each_frame_fault_port_name_is_required_independently(self) -> None:
        for ordinal, port in enumerate(self.source.ports):
            with self.subTest(port=port.name):
                ports = list(self.source.ports)
                ports[ordinal] = replace(port, name=port.name + "_wrong")
                with self.assertRaisesRegex(interface.InterfaceManifestError, port.name):
                    interface.audit_required_root_ports(
                        replace(self.source, ports=tuple(ports))
                    )

    def test_each_frame_fault_port_direction_is_checked_independently(self) -> None:
        for ordinal, port in enumerate(self.source.ports):
            with self.subTest(port=port.name):
                ports = list(self.source.ports)
                wrong = "output" if port.direction == "input" else "input"
                ports[ordinal] = replace(port, direction=wrong)
                with self.assertRaisesRegex(interface.InterfaceManifestError, port.name):
                    interface.audit_required_root_ports(
                        replace(self.source, ports=tuple(ports))
                    )

    def test_each_frame_fault_port_width_is_checked_independently(self) -> None:
        dimension = interface.DimensionView("[1:0]", 1, 0)
        for ordinal, port in enumerate(self.source.ports):
            with self.subTest(port=port.name):
                ports = list(self.source.ports)
                ports[ordinal] = replace(port, packed_dimensions=(dimension,))
                with self.assertRaisesRegex(interface.InterfaceManifestError, port.name):
                    interface.audit_required_root_ports(
                        replace(self.source, ports=tuple(ports))
                    )


class SourceElaborationMismatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SourceDeclarationTests.view
        cls.elaborated = interface.ElaborationView(
            parameters=(
                interface.ElaboratedParameter(
                    ordinal=0,
                    name="W",
                    value=96,
                    bit_width=32,
                    signed=False,
                    dtype_kind="BASICDTYPE",
                ),
            ),
            ports=tuple(
                interface.ElaboratedPort(
                    ordinal=port.ordinal,
                    name=port.name,
                    direction=port.direction,
                    net_or_var=port.net_or_var,
                    bit_width=port.bit_width,
                    element_width=port.element_width,
                    signed=port.signed,
                    packed_ranges=tuple((d.left, d.right) for d in port.packed_dimensions),
                    unpacked_ranges=tuple((d.left, d.right) for d in port.unpacked_dimensions),
                    dtype_kind="BASICDTYPE",
                )
                for port in cls.source.ports
            ),
            module_source_paths=frozenset({FIXTURE_PATH}),
        )

    def assert_port_mutation_fires(self, ordinal: int, **changes: object) -> None:
        ports = list(self.elaborated.ports)
        ports[ordinal] = replace(ports[ordinal], **changes)
        mutated = replace(self.elaborated, ports=tuple(ports))
        with self.assertRaisesRegex(interface.InterfaceManifestError, "source/elaboration mismatch"):
            interface.compare_source_and_elaboration(self.source, mutated)

    def test_parameter_dtype_kind_mismatch_fires(self) -> None:
        parameter = replace(self.elaborated.parameters[0], dtype_kind="STRUCTDTYPE")
        mutated = replace(self.elaborated, parameters=(parameter,))
        with self.assertRaisesRegex(interface.InterfaceManifestError, "dtype_kind"):
            interface.compare_source_and_elaboration(self.source, mutated)

    def test_fixed_integer_width_mismatch_fires_independently(self) -> None:
        parameter = replace(self.elaborated.parameters[0], bit_width=31)
        mutated = replace(self.elaborated, parameters=(parameter,))
        with self.assertRaisesRegex(interface.InterfaceManifestError, "fixed integer width"):
            interface.compare_source_and_elaboration(self.source, mutated)

    def test_port_dtype_kind_mismatch_fires(self) -> None:
        self.assert_port_mutation_fires(1, dtype_kind="STRUCTDTYPE")

    def test_wrong_package_constant_value_disagrees_with_elaboration(self) -> None:
        wrong_constants = dict(fixture_qualified_constants())
        wrong_constants["zhao_render_texture_pkg::AUX_SURFACE_CTX_W"] = 223
        wrong_source = interface.parse_module_source(
            FIXTURE.read_bytes(),
            source_path=FIXTURE_PATH,
            module_name="zhao_texture_interface_schema_fixture",
            qualified_constants=wrong_constants,
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "source/elaboration mismatch"):
            interface.compare_source_and_elaboration(wrong_source, self.elaborated)

    def test_wrong_direction_fires(self) -> None:
        self.assert_port_mutation_fires(1, direction="output")

    def test_wrong_net_variable_kind_fires(self) -> None:
        self.assert_port_mutation_fires(0, net_or_var="variable")

    def test_wrong_signedness_fires(self) -> None:
        self.assert_port_mutation_fires(2, signed=False)

    def test_wrong_packed_range_fires(self) -> None:
        self.assert_port_mutation_fires(2, packed_ranges=((7, 0),))

    def test_wrong_unpacked_nonzero_bounds_fire(self) -> None:
        self.assert_port_mutation_fires(4, unpacked_ranges=((1, 0), (0, 2)))

    def test_wrong_total_bits_fires(self) -> None:
        self.assert_port_mutation_fires(4, bit_width=32)

    def test_plain_isparam_localparam_inclusion_fires(self) -> None:
        localparam = interface.ElaboratedParameter(
            ordinal=1,
            name="DEPENDENT_LOCAL_W",
            value=97,
            bit_width=32,
            signed=False,
            dtype_kind="BASICDTYPE",
        )
        mutated = replace(
            self.elaborated,
            parameters=self.elaborated.parameters + (localparam,),
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "parameter names/order differ"):
            interface.compare_source_and_elaboration(self.source, mutated)

    def test_parameter_value_mismatch_fires(self) -> None:
        parameter = replace(self.elaborated.parameters[0], value=95)
        mutated = replace(self.elaborated, parameters=(parameter,))
        with self.assertRaisesRegex(interface.InterfaceManifestError, "parameter 'W' value"):
            interface.compare_source_and_elaboration(self.source, mutated)


class ProductionParameterInventoryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.artifact = interface.build_manifest_artifact(
            repo_root=REPO,
            top_module=interface.PRODUCTION_TOP,
            source_paths=interface.PRODUCTION_SOURCE_CLOSURE,
            production=True,
        )
        cls.artifact.verify_live_unchanged()

    def test_exact_sixteen_production_generic_parameters_match_source(self) -> None:
        parameters = self.artifact.payload["parameters"]
        top_source = self.artifact.snapshots[
            interface.PRODUCTION_SOURCE_CLOSURE[-1]
        ].raw.decode("utf-8")
        for localparam in ("OWNERW", "SMPW", "OWNERS", "MATW", "TUPLEW"):
            self.assertRegex(
                top_source,
                rf"localparam\s+[^;=]*\b{localparam}\b\s*=",
            )
        actual = tuple(
            (
                row["name"],
                row["selected_value"]["kind"],
                row["selected_value"]["text"],
            )
            for row in parameters
        )
        self.assertEqual(len(EXPECTED_PRODUCTION_PARAMETER_INVENTORY), 16)
        self.assertEqual(actual, EXPECTED_PRODUCTION_PARAMETER_INVENTORY)
        self.assertTrue(all(row["declared_kind"] == "parameter" for row in parameters))

    def test_exact_120_production_ports_match_frozen_inventory(self) -> None:
        ports = self.artifact.payload["ports"]
        # 120 since owner ruling R9 added cnt_texture_samples_o (TEXTURE.TMU's counter).
        self.assertEqual(len(EXPECTED_PRODUCTION_PORT_NAMES), 120)
        self.assertEqual(
            tuple(row["name"] for row in ports),
            EXPECTED_PRODUCTION_PORT_NAMES,
        )


class SourceClosureTests(unittest.TestCase):
    def test_packet_b_order_is_independently_pinned(self) -> None:
        self.assertEqual(interface.PRODUCTION_SOURCE_CLOSURE, EXPECTED_PACKET_B_CLOSURE)
        self.assertEqual(len(EXPECTED_PACKET_B_CLOSURE), 26)
        self.assertEqual(len(set(EXPECTED_PACKET_B_CLOSURE)), 26)
        self.assertEqual(EXPECTED_PACKET_B_CLOSURE[0], "fpga/rtl/common/zhao_render_texture_pkg.sv")
        self.assertEqual(EXPECTED_PACKET_B_CLOSURE[-1], "fpga/rtl/texture/zhao_texture_island_v3_top.sv")

    def test_fixture_is_an_exact_package_first_two_source_closure(self) -> None:
        rows = interface.validate_source_closure(
            repo_root=REPO,
            source_paths=FIXTURE_CLOSURE,
            top_module="zhao_texture_interface_schema_fixture",
            elaborated_module_paths=[FIXTURE_PATH],
            production=False,
        )
        self.assertEqual(
            [(row["kind"], row["ordinal"], row["path"]) for row in rows],
            [
                ("systemverilog_package", 0, PACKAGE_PATH),
                ("systemverilog_module", 1, FIXTURE_PATH),
            ],
        )

    def test_repo_path_case_is_not_silently_normalized(self) -> None:
        wrong_case = FIXTURE_PATH.replace("tests/", "Tests/", 1)
        with self.assertRaisesRegex(interface.InterfaceManifestError, "wrong case|missing"):
            interface.validate_source_closure(
                repo_root=REPO,
                source_paths=[wrong_case],
                top_module="zhao_texture_interface_schema_fixture",
                production=False,
            )

    def test_real_elaboration_instance_graph_excludes_separate_unused_module(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-real-closure-") as temporary:
            repo = Path(temporary)
            (repo / "rtl").mkdir()
            (repo / "rtl" / "used.sv").write_text(
                "module used(input logic a); endmodule\n", encoding="utf-8"
            )
            (repo / "rtl" / "unused.sv").write_text(
                "module unused(input logic a); endmodule\n", encoding="utf-8"
            )
            (repo / "rtl" / "top.sv").write_text(
                "module top(input logic a); used u_used(.a(a)); endmodule\n",
                encoding="utf-8",
            )
            paths = ["rtl/used.sv", "rtl/unused.sv", "rtl/top.sv"]
            snapshots = interface.SnapshotSet.capture(repo, paths)
            source = interface.parse_module_source(
                snapshots["rtl/top.sv"].raw,
                source_path="rtl/top.sv",
                module_name="top",
            )
            elaborated, _version, _argv = interface.run_verilator_query(
                repo_root=repo,
                top_module="top",
                parameters=source.parameters,
                source_paths=paths,
                snapshots=snapshots,
                verilator=str(interface.find_verilator(REPO)),
            )
            self.assertEqual(
                elaborated.module_source_paths,
                frozenset({"rtl/used.sv", "rtl/top.sv"}),
            )
            with self.assertRaisesRegex(interface.InterfaceManifestError, "unreachable_extra"):
                interface.validate_source_closure(
                    repo_root=repo,
                    source_paths=paths,
                    top_module="top",
                    elaborated_module_paths=elaborated.module_source_paths,
                    snapshots=snapshots,
                    production=False,
                )
            rows = interface.validate_source_closure(
                repo_root=repo,
                source_paths=["rtl/used.sv", "rtl/top.sv"],
                top_module="top",
                elaborated_module_paths=elaborated.module_source_paths,
                snapshots=snapshots,
                production=False,
            )
            self.assertEqual([row["path"] for row in rows], ["rtl/used.sv", "rtl/top.sv"])

    def test_unreachable_extra_source_fires(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-closure-extra-") as temporary:
            repo = Path(temporary)
            (repo / "rtl").mkdir()
            (repo / "rtl" / "child.sv").write_text(
                "module child(input logic a); endmodule\n", encoding="utf-8"
            )
            (repo / "rtl" / "extra.sv").write_text(
                "module extra(input logic a); endmodule\n", encoding="utf-8"
            )
            (repo / "rtl" / "top.sv").write_text(
                "module top(input logic a); child u_child(.a(a)); endmodule\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(interface.InterfaceManifestError, "unreachable_extra"):
                interface.validate_source_closure(
                    repo_root=repo,
                    source_paths=["rtl/child.sv", "rtl/extra.sv", "rtl/top.sv"],
                    top_module="top",
                    production=False,
                )

    def test_missing_reachable_source_cannot_be_hidden(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-closure-missing-") as temporary:
            repo = Path(temporary)
            (repo / "rtl").mkdir()
            (repo / "rtl" / "top.sv").write_text(
                "module top(input logic a); missing_child u_missing(.a(a)); endmodule\n",
                encoding="utf-8",
            )
            # Verilator, not a text undercount, is the final authority for unknown
            # modules.  The static closure accepts only its known file set; this
            # check proves an elaborated source outside it is still fatal.
            with self.assertRaisesRegex(interface.InterfaceManifestError, "outside closure"):
                interface.validate_source_closure(
                    repo_root=repo,
                    source_paths=["rtl/top.sv"],
                    top_module="top",
                    elaborated_module_paths=["rtl/top.sv", "rtl/missing_child.sv"],
                    production=False,
                )

    def test_package_after_module_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory(prefix="zhao-closure-package-order-") as temporary:
            repo = Path(temporary)
            (repo / "rtl").mkdir()
            (repo / "rtl" / "top.sv").write_text(
                "module top(input logic a); endmodule\n", encoding="utf-8"
            )
            (repo / "rtl" / "pkg.sv").write_text(
                "package pkg; localparam int X = 1; endpackage\n", encoding="utf-8"
            )
            with self.assertRaisesRegex(interface.InterfaceManifestError, "package .* after"):
                interface.validate_source_closure(
                    repo_root=repo,
                    source_paths=["rtl/top.sv", "rtl/pkg.sv"],
                    top_module="top",
                    production=False,
                )

    def test_production_reordering_fires_before_file_contents_can_excuse_it(self) -> None:
        reordered = list(EXPECTED_PACKET_B_CLOSURE)
        reordered[1], reordered[2] = reordered[2], reordered[1]
        with self.assertRaisesRegex(interface.InterfaceManifestError, "pinned contract"):
            interface.validate_source_closure(
                repo_root=REPO,
                source_paths=reordered,
                top_module=interface.PRODUCTION_TOP,
                production=True,
            )


class QuietContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.aliases, cls.controls, cls.expression_leaves = load_quiet_fixture()

    def test_fixture_is_an_independent_complete_unique_control_ledger(self) -> None:
        self.assertEqual(self.controls, list(interface.quiet_expected_controls()))
        identities = [
            (row["equation"], row["operand"]) for row in self.controls
        ]
        self.assertEqual(len(identities), len(set(identities)))
        self.assertEqual(len(self.controls), 72)
        self.assertEqual(
            sum(row["equation"] == "data_quiet" for row in self.controls), 60
        )
        self.assertEqual(
            sum(row["equation"] == "quiet_o" for row in self.controls), 12
        )
        public_new = [
            row for row in self.controls
            if row["equation"] == "quiet_o" and row["operand"] != "data_quiet"
        ]
        self.assertEqual(len(public_new), 11)
        self.assertEqual(60 + len(public_new), 71)

    def test_alias_fixture_is_independent_complete_source_and_width_authority(self) -> None:
        fixture_map = {
            str(row["name"]): (str(row["source_expression"]), int(row["width"]))
            for row in self.aliases
        }
        expected_sources = dict(interface.QUIET_SOURCE_MAP)
        expected_sources.update(interface.QUIET_CONTROL_SOURCE_MAP)
        expected_map = {
            name: (
                source,
                4 if name in {
                    "q_class_rsp_valid", "q_bilerp_req_valid", "q_bilerp_rsp_valid"
                } else 1,
            )
            for name, source in expected_sources.items()
        }
        self.assertEqual(fixture_map, expected_map)
        self.assertEqual(len(self.aliases), len(set(fixture_map)))
        self.assertEqual(
            fixture_map["q_dispatch_req_valid"],
            ("|class_terminal_offer_valid_w[3:0]", 1),
        )
        self.assertEqual(
            fixture_map["q_class_rsp_valid"],
            ("dispatch_pending_w[3:0]", 4),
        )
        self.assertEqual(
            fixture_map["q_tmu_return_valid"],
            ("dispatch_return_valid_w", 1),
        )

    def test_quiet_fixture_rejects_duplicate_keys(self) -> None:
        raw = QUIET_CONTROLS.read_bytes().replace(
            b'"schema_version": 1,',
            b'"schema_version": 1, "schema_version": 1,',
            1,
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "duplicate"):
            interface.load_quiet_fixture_bytes(raw)

    def test_quiet_fixture_schema_version_rejects_boolean_one(self) -> None:
        raw = QUIET_CONTROLS.read_bytes().replace(
            b'"schema_version": 1,', b'"schema_version": true,', 1
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "JSON integer"):
            interface.load_quiet_fixture_bytes(raw)

    def test_five_expression_aliases_and_two_combine_leaves_are_independently_pinned(self) -> None:
        self.assertEqual(
            interface.QUIET_AUTHORIZED_EXPRESSION_ALIASES,
            {
                "q_bilerp_idle": "&bilerp_lane_idle_w[3:0]",
                "q_fill_rsp_valid": "fill_data_valid_i || fill_refused_i",
                "q_dispatch_req_valid": "|class_terminal_offer_valid_w[3:0]",
                "q_palette_cfg_rsp_valid": "1'b0",
                "q_combine_idle": "material_read_idle_w && combine_leaf_idle_w",
            },
        )
        self.assertEqual(
            self.expression_leaves,
            [
                {
                    "alias": "q_combine_idle",
                    "leaf": "material_read_idle_w",
                    "polarity": "positive",
                },
                {
                    "alias": "q_combine_idle",
                    "leaf": "combine_leaf_idle_w",
                    "polarity": "positive",
                },
            ],
        )
        flattened_named = 60 + 11
        flattened_physical = flattened_named + len(self.expression_leaves) - 1
        self.assertEqual(flattened_named, 71)
        self.assertEqual(flattened_physical, 72)

    def test_each_combine_physical_leaf_has_omission_polarity_and_mapping_controls(self) -> None:
        canonical = "assign q_combine_idle = material_read_idle_w && combine_leaf_idle_w;"
        baseline = full_quiet_source(self.aliases, self.controls)
        self.assertIn(canonical, baseline)
        for row in self.expression_leaves:
            leaf = row["leaf"]
            other = next(
                candidate["leaf"]
                for candidate in self.expression_leaves
                if candidate["leaf"] != leaf
            )
            mutants = {
                "omission": f"assign q_combine_idle = {other};",
                "polarity": canonical.replace(leaf, "!" + leaf),
                "mapping": canonical.replace(leaf, leaf + "_wrong"),
            }
            for control, replacement in mutants.items():
                with self.subTest(leaf=leaf, control=control):
                    source = baseline.replace(canonical, replacement, 1)
                    with self.assertRaises(interface.InterfaceManifestError) as context:
                        interface.audit_quiet_contract(source, require_source_map=True)
                    self.assertIn("q_combine_idle source differs", str(context.exception))

    def test_complete_literal_equations_pass(self) -> None:
        interface.audit_quiet_contract(
            quiet_equation_source(self.controls), require_source_map=False
        )

    def test_every_operand_has_an_independent_omission_fire_control(self) -> None:
        for ordinal, row in enumerate(self.controls):
            with self.subTest(ordinal=ordinal, **row):
                source = quiet_equation_source(self.controls, omit=ordinal)
                with self.assertRaises(interface.InterfaceManifestError) as context:
                    interface.audit_quiet_contract(source, require_source_map=False)
                self.assertIn(row["equation"], str(context.exception))
                self.assertIn(row["operand"].split("[")[0], str(context.exception))

    def test_every_operand_has_an_independent_wrong_polarity_fire_control(self) -> None:
        for ordinal, row in enumerate(self.controls):
            with self.subTest(ordinal=ordinal, **row):
                source = quiet_equation_source(self.controls, invert=ordinal)
                with self.assertRaises(interface.InterfaceManifestError) as context:
                    interface.audit_quiet_contract(source, require_source_map=False)
                self.assertIn(row["equation"], str(context.exception))
                self.assertIn(row["operand"].split("[")[0], str(context.exception))

    def test_exact_source_map_and_equations_pass_together(self) -> None:
        interface.audit_quiet_contract(
            full_quiet_source(self.aliases, self.controls), require_source_map=True
        )

    def test_every_alias_mapping_has_an_independent_fire_control(self) -> None:
        for ordinal, row in enumerate(self.aliases):
            with self.subTest(ordinal=ordinal, alias=row["name"]):
                source = full_quiet_source(
                    self.aliases, self.controls, mapping_mutant=ordinal
                )
                with self.assertRaises(interface.InterfaceManifestError) as context:
                    interface.audit_quiet_contract(source, require_source_map=True)
                self.assertIn(str(row["name"]), str(context.exception))

    def test_every_alias_declaration_has_an_independent_omission_control(self) -> None:
        for ordinal, row in enumerate(self.aliases):
            with self.subTest(ordinal=ordinal, alias=row["name"]):
                source = full_quiet_source(
                    self.aliases, self.controls, omit_declaration=ordinal
                )
                with self.assertRaises(interface.InterfaceManifestError) as context:
                    interface.audit_quiet_contract(source, require_source_map=True)
                self.assertIn(str(row["name"]), str(context.exception))
                self.assertIn("declaration differs", str(context.exception))

    def test_every_alias_width_has_an_independent_mutant(self) -> None:
        for ordinal, row in enumerate(self.aliases):
            with self.subTest(ordinal=ordinal, alias=row["name"]):
                source = full_quiet_source(
                    self.aliases, self.controls, width_mutant=ordinal
                )
                with self.assertRaises(interface.InterfaceManifestError) as context:
                    interface.audit_quiet_contract(source, require_source_map=True)
                self.assertIn(str(row["name"]), str(context.exception))
                self.assertIn("declaration differs", str(context.exception))

    def test_actual_production_top_quiet_contract_gate(self) -> None:
        top = REPO / "fpga" / "rtl" / "texture" / "zhao_texture_island_v3_top.sv"
        interface.audit_quiet_contract(
            top.read_text(encoding="utf-8"), require_source_map=True
        )

    def test_wrong_vector_alias_declaration_fires(self) -> None:
        source = full_quiet_source(self.aliases, self.controls).replace(
            "logic [3:0] q_class_rsp_valid;",
            "logic q_class_rsp_valid;",
            1,
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "q_class_rsp_valid declaration differs"):
            interface.audit_quiet_contract(source, require_source_map=True)

    def test_private_hierarchical_source_reference_fires(self) -> None:
        source = full_quiet_source(self.aliases, self.controls).replace(
            "assign q_owner_idle = own_ev_quiet_w;",
            "assign q_owner_idle = u_own.ev_quiet_o;",
            1,
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "hierarchical"):
            interface.audit_quiet_contract(source, require_source_map=True)

    def test_wrong_special_alias_expression_fires(self) -> None:
        source = full_quiet_source(self.aliases, self.controls).replace(
            "assign q_fill_rsp_valid = fill_data_valid_i || fill_refused_i;",
            "assign q_fill_rsp_valid = fill_data_valid_i;",
            1,
        )
        with self.assertRaisesRegex(interface.InterfaceManifestError, "q_fill_rsp_valid source differs"):
            interface.audit_quiet_contract(source, require_source_map=True)

    def test_second_driver_fires(self) -> None:
        source = full_quiet_source(self.aliases, self.controls) + "always_comb q_owner_idle = 1'b0;\n"
        with self.assertRaisesRegex(interface.InterfaceManifestError, "exactly one driver"):
            interface.audit_quiet_contract(source, require_source_map=True)

    def test_vector_controls_use_all_four_declared_bits(self) -> None:
        expected = {
            "q_class_rsp_valid[3:0]",
            "q_bilerp_req_valid[3:0]",
            "q_bilerp_rsp_valid[3:0]",
        }
        actual = {
            row["operand"] for row in self.controls
            if row["operand"] in expected
        }
        self.assertEqual(actual, expected)
        for operand in expected:
            self.assertEqual(interface.quiet_term_text(operand, "negative"), f"!(|{operand})")


def _flatten_suite(suite: unittest.TestSuite):
    for item in suite:
        if isinstance(item, unittest.TestSuite):
            yield from _flatten_suite(item)
        else:
            yield item


class DuplicateFingerprintGroupingTests(unittest.TestCase):
    """The negative control for making the fingerprint address-agnostic.

    canonical_verilator_duplicate_marker_bytes stopped hashing Verilator's
    internal parent label and hashes a group ordinal instead, because that
    label is reassigned wholesale on any elaboration-order change and forced
    three re-pins in one session while nothing about the design moved.

    A change that makes a detector quieter is exactly the change to distrust,
    so these assert it is quieter ONLY about relabelling: regrouping and
    renaming must still move the hash.
    """

    def _marker(self, pointer, addr, name, loc):
        return interface.VerilatorDuplicateMarker(pointer, addr, name, loc)

    def setUp(self):
        self.base = [
            self._marker("/p/0", "(AAA)", "x", "f,1:1,1:2"),
            self._marker("/p/1", "(AAA)", "y", "f,2:1,2:2"),
            self._marker("/p/2", "(BBB)", "x", "f,3:1,3:2"),
        ]

    def test_relabelling_every_parent_does_not_move_the_fingerprint(self):
        relabelled = [
            self._marker(m.json_pointer,
                         {"(AAA)": "(ZZZ)", "(BBB)": "(YYY)"}[
                             m.parent_struct_addr],
                         m.member_name, m.loc)
            for m in self.base
        ]
        self.assertEqual(
            interface.verilator_duplicate_marker_sha256(self.base),
            interface.verilator_duplicate_marker_sha256(relabelled),
            "a pure relabelling must not be reported as a design change")

    def test_moving_a_member_to_another_parent_still_fires(self):
        regrouped = list(self.base)
        regrouped[1] = self._marker("/p/1", "(BBB)", "y", "f,2:1,2:2")
        self.assertNotEqual(
            interface.verilator_duplicate_marker_sha256(self.base),
            interface.verilator_duplicate_marker_sha256(regrouped),
            "a member changing parents is a real change and must fire")

    def test_renaming_a_duplicated_member_still_fires(self):
        renamed = list(self.base)
        renamed[1] = self._marker("/p/1", "(AAA)", "y_renamed", "f,2:1,2:2")
        self.assertNotEqual(
            interface.verilator_duplicate_marker_sha256(self.base),
            interface.verilator_duplicate_marker_sha256(renamed),
            "the set of duplicated members changing must fire")

    def test_splitting_one_parent_into_two_still_fires(self):
        """Two members sharing a parent, then not, is a partition change.

        Written first as "give the last row a brand-new address", which does
        NOT fire -- and correctly so: that row was alone under its parent
        before and is alone under a different name after, which is the same
        partition and therefore the same relabelling this change exists to
        ignore. The invariant is the GROUPING, so the case that must fire is
        two members ceasing to share.
        """
        split = list(self.base)
        split[1] = self._marker("/p/1", "(CCC)", "y", "f,2:1,2:2")
        self.assertNotEqual(
            interface.verilator_duplicate_marker_sha256(self.base),
            interface.verilator_duplicate_marker_sha256(split),
            "two members that shared a parent and now do not must fire")

    def test_renaming_a_lone_parent_is_a_relabelling(self):
        """The boundary case, asserted rather than left implicit."""
        renamed = list(self.base)
        renamed[2] = self._marker("/p/2", "(CCC)", "x", "f,3:1,3:2")
        self.assertEqual(
            interface.verilator_duplicate_marker_sha256(self.base),
            interface.verilator_duplicate_marker_sha256(renamed),
            "a parent with the same members under a new label is the same "
            "partition")


if __name__ == "__main__":
    if "--tool-only" not in sys.argv:
        unittest.main()
    else:
        sys.argv.remove("--tool-only")
        verbosity = 2 if "-v" in sys.argv else 1
        selected = [
            test
            for test in _flatten_suite(
                unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
            )
            if not test.id().endswith(
                "QuietContractTests.test_actual_production_top_quiet_contract_gate"
            )
        ]
        result = unittest.TextTestRunner(verbosity=verbosity).run(
            unittest.TestSuite(selected)
        )
        raise SystemExit(0 if result.wasSuccessful() else 1)
