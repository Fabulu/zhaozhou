#!/usr/bin/env python3
"""Generate the ten-pin, protocol-aware zhao_shell_fit_top instrument.

Only ``--write`` mutates files, and each artifact is replaced atomically from a
same-directory temporary file.  ``--check`` is strictly read-only.  Importing
this module, requesting ``--help``, or supplying malformed arguments performs no
writes.
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from typing import Mapping, Sequence

from shell_ports import (
    ModuleDeclaration,
    PolicyPort,
    Port,
    ShellPolicy,
    ShellPortError,
    assert_exact_port_sets,
    discover_type_signedness,
    discover_type_widths,
    domain_output_bits,
    flatten_port_elements,
    load_policy_text,
    parse_module_declaration,
    policy_by_name,
    validate_policy,
)


SCRIPT = Path(__file__).resolve()
PARSER_SCRIPT = SCRIPT.with_name("shell_ports.py")
DEFAULT_REPO = SCRIPT.parents[2]
DEFAULT_SHELL = Path("fpga/rtl/common/zhao_shell_top.sv")
DEFAULT_PACKAGE = Path("fpga/rtl/common/zhao_pkg.sv")
DEFAULT_POLICY = Path("design/shell_fit_ports.yml")
DEFAULT_PACKET = Path("tests/tools/fixtures/shell_fit_frame_blit.bin")
DEFAULT_RTL = Path("fpga/rtl/generated/zhao_shell_fit_top.sv")
DEFAULT_MANIFEST = Path("fpga/rtl/generated/zhao_shell_fit_top.manifest.json")
GENERATOR_SCHEMA = 1
TRAFFIC_PROFILE = "shell-fit-legal-ish-v1"
HANDLER_INPUT_PORTS: Mapping[str, tuple[str, ...]] = {
    "top_port": ("gpu_clk", "vid_clk", "audio_clk", "rst_n"),
    "frame_ring": ("hps_state_i", "hps_byte_len_i", "ring_wr_ready_i"),
    "hps_responder": (
        "hps_req_grant_i",
        "hps_rd_valid_i",
        "hps_rd_data_i",
        "hps_rd_last_i",
    ),
    "pads": (
        "pad_present_i",
        "pad_buttons_i",
        "pad_lx_i",
        "pad_ly_i",
        "pad_rx_i",
        "pad_ry_i",
    ),
    "audio_producer": ("aud_wr_valid_i", "aud_wr_l_i", "aud_wr_r_i"),
    "counter_consumer": ("cnt_snap_ready_i",),
    "render_producer": (
        "render_frame_begin_i",
        "render_frame_end_i",
        "render_grid_w_i",
        "render_grid_h_i",
        "render_tri_valid_i",
        "render_kx0_i",
        "render_ky0_i",
        "render_kc0_i",
        "render_kx1_i",
        "render_ky1_i",
        "render_kc1_i",
        "render_kx2_i",
        "render_ky2_i",
        "render_kc2_i",
        "render_tl_i",
        "render_ax_i",
        "render_ay_i",
        "render_bx_i",
        "render_by_i",
        "render_cx_i",
        "render_cy_i",
        "render_min_x_i",
        "render_max_x_i",
        "render_min_y_i",
        "render_max_y_i",
        "render_src_id_i",
        "render_fill_word_i",
        "render_clear_word_i",
        "render_state_i",
        "render_src_a_i",
        "render_texel_rgb_i",
        "render_texel_a_i",
        "render_texel_idx_i",
        "render_fb_base_i",
        "render_fb_stride_i",
        "fb_writer_i",
    ),
    "geometry_guard": ("geom_guard_req_i",),
    "sdr_phy_responder": ("phy_dq_i",),
}


@dataclass(frozen=True)
class RenderedArtifacts:
    rtl: bytes
    manifest: bytes


def _hash(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _read_utf8(path: Path) -> tuple[bytes, str]:
    data = path.read_bytes()
    try:
        return data, data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"{path} is not UTF-8: {exc}") from exc


def _dimensions(port: Port) -> str:
    return "".join(f" {dimension.text}" for dimension in port.unpacked_dimensions)


def _signal_declaration(port: Port, name: str, *, attribute: str | None = None) -> str:
    prefix = f"(* {attribute} *) " if attribute else ""
    return f"{prefix}{port.signal_type_text} {name}{_dimensions(port)};"


def _module_port(port: Port, direction: str, name: str, *, preserve: bool = False) -> str:
    attribute = "(* preserve *) " if preserve else ""
    return f"  {attribute}{direction} var {port.signal_type_text} {name}{_dimensions(port)}"


def _format_mask(mask: int | None, width: int) -> str | None:
    if mask is None:
        return None
    return f"0x{mask:0{(width + 3) // 4}x}"


def validate_handler_ownership(
    declaration: ModuleDeclaration, policy: ShellPolicy
) -> None:
    """Bind every input taxonomy row to the handler that actually drives it."""
    declared_inputs = {port.name for port in declaration.ports if port.direction == "input"}
    expected_inputs = {
        name for names in HANDLER_INPUT_PORTS.values() for name in names
    }
    if declared_inputs != expected_inputs:
        missing = sorted(declared_inputs - expected_inputs)
        extra = sorted(expected_inputs - declared_inputs)
        raise ShellPortError(
            f"generator handler inventory differs from shell inputs: missing={missing}, extra={extra}"
        )
    actual: dict[str, list[str]] = {driver: [] for driver in HANDLER_INPUT_PORTS}
    for row in policy.ports:
        if row.direction == "input" and row.driver in actual:
            actual[row.driver].append(row.name)
    errors: list[str] = []
    for driver, expected in HANDLER_INPUT_PORTS.items():
        observed = tuple(actual[driver])
        if observed != expected:
            missing = [name for name in expected if name not in set(observed)]
            extra = [name for name in observed if name not in set(expected)]
            errors.append(
                f"driver {driver!r} ownership differs: missing={missing}, extra={extra}, "
                f"expected_order={list(expected)}, observed_order={list(observed)}"
            )
    if errors:
        raise ShellPortError("; ".join(errors))


def _output_offsets(
    declaration: ModuleDeclaration, policy: ShellPolicy
) -> tuple[dict[str, int], dict[str, int]]:
    rows = policy_by_name(policy)
    running = {"gpu": 0, "video": 0, "audio": 0}
    offsets: dict[str, int] = {}
    for port in declaration.ports:
        if port.direction != "output":
            continue
        domain = rows[port.name].domain
        offsets[port.name] = running[domain]
        running[domain] += port.bit_width
    return offsets, running


def _capture_expression(port: Port, domain: str, offset: int) -> str:
    raw = f"{domain}_capture_bus[{offset} +: {port.bit_width}]"
    base = re.sub(r"\s*\[[^\]]+\]", "", port.signal_type_text).strip()
    base = re.sub(r"^(?:signed|unsigned)\s+", "", base)
    if base not in {"logic", "bit", "reg", "wire"}:
        return f"{base}'({raw})"
    return raw


def _render_packet_function(packet: bytes) -> str:
    rows = [
        "  function automatic logic [7:0] packet_byte(input logic [31:0] address);",
        "    begin",
        "      case (address)",
    ]
    for index, value in enumerate(packet):
        rows.append(f"        32'h{0x1000 + index:08x}: packet_byte = 8'h{value:02x};")
    rows.extend(
        [
            "        default: packet_byte = address[7:0] ^ address[15:8] ^ address[23:16] ^ address[31:24] ^ 8'h5a;",
            "      endcase",
            "    end",
            "  endfunction",
        ]
    )
    return "\n".join(rows)


def _render_sink(domain: str, capture_bits: int, seed: int, *, expose_capture: bool) -> str:
    chunks = (capture_bits + 31) // 32
    index_width = max(1, (chunks - 1).bit_length())
    lines = [
        f"module zhao_shell_fit_{domain}_sink (",
        "  input  logic clk,",
        "  input  logic rst_n,",
        f"  input  logic [{capture_bits - 1}:0] payload_i,",
    ]
    if expose_capture:
        lines.append(f"  output logic [{capture_bits - 1}:0] capture_o,")
    lines.extend(
        [
            "  output logic signature_o,",
            "  output logic epoch_o",
            ");",
            "",
            "  // Immediate native-domain endpoint for every shell output bit.",
            f"  (* preserve *) logic [{capture_bits - 1}:0] capture_q;",
            "  always_ff @(posedge clk) capture_q <= payload_i;",
        ]
    )
    if expose_capture:
        lines.append("  assign capture_o = capture_q;")
    lines.extend(
        [
        "",
        "  logic [9:0] release_q;",
        "  logic run_c;",
        "  always_ff @(posedge clk or negedge rst_n) begin",
        "    if (!rst_n) release_q <= '0;",
        "    else release_q <= {release_q[8:0], 1'b1};",
        "  end",
        "  assign run_c = release_q[9]; // 2 sync edges + 8 warm-up edges",
        "",
        "  (* preserve *) logic [63:0] activity_lfsr_q;",
        f"  logic [{index_width - 1}:0] chunk_index_q, selected_index_q;",
        "  logic selected_valid_q;",
        "  logic [31:0] selected_c, selected_q;",
        "  (* preserve *) logic [31:0] misr_q;",
        "  (* preserve *) logic [31:0] snapshot_q;",
        "  logic [4:0] serial_index_q;",
        "  logic serializer_busy_q;",
        "  logic [31:0] chunk_wraps_q;",
        "  logic [31:0] snapshots_q;",
        "  logic [31:0] misr_next_c;",
        "",
        "  always_comb begin",
        "    selected_c = 32'h0;",
        "    case (chunk_index_q)",
        ]
    )
    for chunk in range(chunks):
        offset = chunk * 32
        width = min(32, capture_bits - offset)
        if width == 32:
            lines.append(
                f"      {index_width}'d{chunk}: selected_c = capture_q[{offset} +: 32];"
            )
        else:
            lines.extend(
                [
                    f"      {index_width}'d{chunk}: begin",
                    f"        selected_c[{width - 1}:0] = capture_q[{offset} +: {width}];",
                    "      end",
                ]
            )
    lines.extend(
        [
            "      default: selected_c = 32'h0;",
            "    endcase",
            "  end",
            "",
            "  assign misr_next_c = {misr_q[30:0], 1'b0}",
            "                     ^ (misr_q[31] ? 32'h0040_0007 : 32'h0)",
            "                     ^ selected_q;",
            "",
            "  always_ff @(posedge clk or negedge rst_n) begin",
            "    if (!rst_n) begin",
            f"      activity_lfsr_q <= 64'h{seed:016x};",
            "      chunk_index_q <= '0;",
            "      selected_index_q <= '0;",
            "      selected_valid_q <= 1'b0;",
            "      selected_q <= '0;",
            "      misr_q <= '0;",
            "      snapshot_q <= '0;",
            "      serial_index_q <= '0;",
            "      serializer_busy_q <= 1'b0;",
            "      signature_o <= 1'b0;",
            "      epoch_o <= 1'b0;",
            "      chunk_wraps_q <= '0;",
            "      snapshots_q <= '0;",
            "    end else if (!run_c) begin",
            f"      activity_lfsr_q <= 64'h{seed:016x};",
            "      chunk_index_q <= '0;",
            "      selected_index_q <= '0;",
            "      selected_valid_q <= 1'b0;",
            "      selected_q <= '0;",
            "      misr_q <= '0;",
            "      snapshot_q <= '0;",
            "      serial_index_q <= '0;",
            "      serializer_busy_q <= 1'b0;",
            "      signature_o <= 1'b0;",
            "      epoch_o <= 1'b0;",
            "      chunk_wraps_q <= '0;",
            "      snapshots_q <= '0;",
            "    end else begin",
            "      activity_lfsr_q <= {activity_lfsr_q[62:0],",
            "                          ^(activity_lfsr_q & 64'hd800_0000_0000_0000)};",
            "      selected_q <= selected_c;",
            "      selected_index_q <= chunk_index_q;",
            "      selected_valid_q <= 1'b1;",
            f"      if (chunk_index_q == {index_width}'d{chunks - 1}) begin",
            "        chunk_index_q <= '0;",
            "      end else begin",
            f"        chunk_index_q <= chunk_index_q + {index_width}'d1;",
            "      end",
            "",
            "      if (selected_valid_q) begin",
            "        misr_q <= misr_next_c;",
            f"        if (selected_index_q == {index_width}'d{chunks - 1}) begin",
            "          chunk_wraps_q <= chunk_wraps_q + 32'd1;",
            "          if (!serializer_busy_q) begin",
            "            snapshot_q <= misr_next_c;",
            "            signature_o <= misr_next_c[0];",
            "            epoch_o <= ~epoch_o;",
            "            serial_index_q <= 5'd1;",
            "            serializer_busy_q <= 1'b1;",
            "            snapshots_q <= snapshots_q + 32'd1;",
            "          end",
            "        end",
            "      end",
            "",
            "      if (serializer_busy_q) begin",
            "        signature_o <= snapshot_q[serial_index_q];",
            "        if (serial_index_q == 5'd31) begin",
            "          serializer_busy_q <= 1'b0;",
            "          serial_index_q <= '0;",
            "        end else begin",
            "          serial_index_q <= serial_index_q + 5'd1;",
            "        end",
            "      end",
            "    end",
            "  end",
            "endmodule",
        ]
    )
    return "\n".join(lines)


def _signed_decimal(width: int, value: int, label: str) -> str:
    if not -(1 << (width - 1)) <= value < (1 << (width - 1)):
        raise ShellPortError(f"{label}={value} does not fit signed {width} bits")
    return f"-{width}'sd{-value}" if value < 0 else f"{width}'sd{value}"


def _render_triangle_values(
    *,
    a: tuple[int, int],
    b: tuple[int, int],
    c: tuple[int, int],
    source_id: int,
) -> dict[str, str]:
    """Derive one legal setup packet exactly from S12.8 triangle vertices."""
    vertices = (a, b, c)
    for index, (x, y) in enumerate(vertices):
        _signed_decimal(21, x, f"vertex {index} x")
        _signed_decimal(21, y, f"vertex {index} y")
    area2 = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
    if area2 <= 0:
        raise ShellPortError(f"renderer triangle area is not positive: {area2}")
    edges = ((b, c), (c, a), (a, b))
    values: dict[str, str] = {}
    top_left = 0
    for index, ((px, py), (qx, qy)) in enumerate(edges):
        kx = py - qy
        ky = qx - px
        kc = px * qy - qx * py
        values[f"render_kx{index}_i"] = _signed_decimal(23, kx, f"edge {index} kx")
        values[f"render_ky{index}_i"] = _signed_decimal(23, ky, f"edge {index} ky")
        values[f"render_kc{index}_i"] = _signed_decimal(48, kc, f"edge {index} kc")
        if (py == qy and px < qx) or py < qy:
            top_left |= 1 << index
    values["render_tl_i"] = f"3'b{top_left:03b}"
    for prefix, (x, y) in zip(("a", "b", "c"), vertices):
        values[f"render_{prefix}x_i"] = _signed_decimal(21, x, f"vertex {prefix} x")
        values[f"render_{prefix}y_i"] = _signed_decimal(21, y, f"vertex {prefix} y")
    min_x = max(0, min(63, min(x for x, _ in vertices) >> 8))
    max_x = max(0, min(63, max(x for x, _ in vertices) >> 8))
    min_y = max(0, min(63, min(y for _, y in vertices) >> 8))
    max_y = max(0, min(63, max(y for _, y in vertices) >> 8))
    for name, value in (
        ("render_min_x_i", min_x),
        ("render_max_x_i", max_x),
        ("render_min_y_i", min_y),
        ("render_max_y_i", max_y),
    ):
        values[name] = _signed_decimal(12, value, name)
    if not 0 <= source_id <= 0xFFFF:
        raise ShellPortError(f"renderer source id does not fit 16 bits: {source_id}")
    values["render_src_id_i"] = f"16'h{source_id:04x}"
    return values


def _render_stimulus(
    declaration: ModuleDeclaration,
    policy: ShellPolicy,
    packet: bytes,
    feedback_ports: Sequence[str],
) -> str:
    rows = policy_by_name(policy)
    by_name = {port.name: port for port in declaration.ports}
    driven = [
        port
        for port in declaration.ports
        if port.direction == "input" and rows[port.name].driver != "top_port"
    ]
    render_payload = [
        port
        for port in driven
        if rows[port.name].driver == "render_producer"
        and port.name
        not in {
            "render_frame_begin_i",
            "render_frame_end_i",
            "render_grid_w_i",
            "render_grid_h_i",
            "render_tri_valid_i",
            "render_fill_word_i",
            "render_clear_word_i",
            "render_state_i",
            "render_src_a_i",
            "render_texel_rgb_i",
            "render_texel_a_i",
            "render_texel_idx_i",
            "render_fb_base_i",
            "render_fb_stride_i",
            "fb_writer_i",
        }
    ]
    render_offer_payload_bits = 264 + sum(port.bit_width for port in render_payload)
    render_offer_fields = [
        "render_grid_w_i",
        "render_grid_h_i",
        "render_fill_word_i",
        "render_clear_word_i",
        "render_state_i",
        "render_src_a_i",
        "render_texel_rgb_i",
        "render_texel_a_i",
        "render_texel_idx_i",
        "render_fb_base_i",
        "render_fb_stride_i",
        "fb_writer_i",
        *(port.name for port in render_payload),
    ]
    render_triangle_groups = {
        "coefficients": (
            "render_kx0_i", "render_ky0_i", "render_kc0_i",
            "render_kx1_i", "render_ky1_i", "render_kc1_i",
            "render_kx2_i", "render_ky2_i", "render_kc2_i",
        ),
        "vertices": (
            "render_ax_i", "render_ay_i", "render_bx_i",
            "render_by_i", "render_cx_i", "render_cy_i",
        ),
        "bounds": (
            "render_min_x_i", "render_max_x_i",
            "render_min_y_i", "render_max_y_i",
        ),
        "identity": ("render_tl_i", "render_src_id_i"),
    }
    grouped_triangle_names = tuple(
        name for fields in render_triangle_groups.values() for name in fields
    )
    if len(grouped_triangle_names) != len(set(grouped_triangle_names)) or set(
        grouped_triangle_names
    ) != {port.name for port in render_payload}:
        raise ShellPortError("renderer triangle group accounting is not exact")
    render_triangle_group_bits = {
        group: sum(by_name[name].bit_width for name in fields)
        for group, fields in render_triangle_groups.items()
    }
    port_lines = [
        "  input logic gpu_clk",
        "  input logic rst_n",
    ]
    for name in feedback_ports:
        port = by_name[name]
        feedback_name = (
            f"{name}_native_i"
            if name == "render_tri_ready_o"
            else f"{name}_captured_i"
        )
        port_lines.append(_module_port(port, "input", feedback_name).strip())
    for port in driven:
        port_lines.append(_module_port(port, "output", port.name, preserve=True).strip())
    lines = [
        "module zhao_shell_fit_stimulus",
        "  import zhao_pkg::*, zhao_abi_pkg::*;",
        "(",
        ",\n".join(f"  {line}" if not line.startswith("  ") else line for line in port_lines),
        ");",
        "  localparam logic [2:0] HPS_IDLE = 3'd0;",
        "  localparam logic [2:0] HPS_GRANT_WAIT = 3'd1;",
        "  localparam logic [2:0] HPS_READ_LATENCY = 3'd2;",
        "  localparam logic [2:0] HPS_READ_BEATS = 3'd3;",
        "  localparam logic [2:0] HPS_WRITE_DATA = 3'd4;",
        "  localparam logic [2:0] RENDER_WAIT_INIT = 3'd0;",
        "  localparam logic [2:0] RENDER_BEGIN = 3'd1;",
        "  localparam logic [2:0] RENDER_OFFER = 3'd2;",
        "  localparam logic [2:0] RENDER_END = 3'd3;",
        "  localparam logic [2:0] RENDER_DRAIN = 3'd4;",
        "",
        "  logic [9:0] release_q;",
        "  logic run_c;",
        "  (* preserve *) logic [63:0] lfsr_q;",
        "  logic [63:0] cycle_q;",
        "  logic [2047:0] entropy_c;",
        "  logic [2:0] hps_state_q;",
        "  logic [2:0] render_state_q;",
        "  logic [31:0] hps_addr_q;",
        "  logic [6:0] hps_len_q;",
        "  logic hps_write_q;",
        "  logic [6:0] hps_wait_q;",
        "  logic [6:0] hps_beat_q;",
        "  logic [7:0] hps_first_beat_age_q;",
        "  logic hps_first_beat_pending_q;",
        "  logic hps_timing_fault_hold_q;",
        "  logic blit_source_read_q;",
        "  logic render_enable_q;",
        "  logic ring_ready_capture_q;",
        "  logic [7:0] ring_delay_q [0:2];",
        "  logic [7:0] audio_pause_q;",
        "  logic [1:0] pad_phase_q;",
        "  logic cnt_ready_capture_q;",
        "  logic [15:0] counter_next_id_q;",
        "  logic [6:0] counter_entries_q;",
        "  logic render_width_cover_q;",
        "  logic render_job_entropy_q;",
        "  logic render_profile_done_q;",
        "  logic render_offer_stalled_q;",
        "  logic render_offer_mutated_q;",
        f"  logic [{render_offer_payload_bits - 1}:0] render_offer_payload_c;",
        f"  logic [{render_offer_payload_bits - 1}:0] render_offer_snapshot_q;",
        f"  logic [{render_offer_payload_bits - 1}:0] render_entropy_offer_snapshot_q /* verilator public_flat_rd */;",
        f"  logic [{render_offer_payload_bits - 1}:0] render_accepted_payload_q /* verilator public_flat_rd */;",
        f"  logic [{render_offer_payload_bits - 1}:0] render_entropy_accepted_payload_q /* verilator public_flat_rd */;",
        *(
            f"  logic [{render_triangle_group_bits[group] - 1}:0] render_triangle_{group}_c;"
            for group in render_triangle_groups
        ),
        *(
            f"  logic [{render_triangle_group_bits[group] - 1}:0] render_directed_accepted_{group}_q /* verilator public_flat_rd */;"
            for group in render_triangle_groups
        ),
        *(
            f"  logic [{render_triangle_group_bits[group] - 1}:0] render_entropy_accepted_{group}_q /* verilator public_flat_rd */;"
            for group in render_triangle_groups
        ),
        "  logic [15:0] render_wait_q;",
        "  logic [31:0] render_issued_start_q;",
        "  logic [31:0] render_retired_start_q;",
        "  logic render_work_seen_q;",
        "  logic render_retirement_seen_q;",
        "  logic guard_negative_q;",
        "  logic guard_response_pending_q;",
        "  logic [15:0] guard_verdict_wait_q;",
        "  logic guard_beat_pending_q;",
        "  logic [3:0] guard_beat_count_q;",
        "  logic guard_frame_owned_q;",
        "  logic guard_denial_watch_q;",
        "  logic guard_preownership_watch_q;",
        "  logic [3:0] guard_extra_fault_delay_q;",
        "  logic [3:0] guard_verdict_extra_fault_delay_q;",
        "  logic [3:0] guard_denial_extra_fault_delay_q;",
        "  logic [15:0] guard_beat_wait_q;",
        "  logic [7:0] guard_gap_q;",
        "  logic [3:0] sdr_read_wait_q;",
        "  logic [3:0] sdr_read_beat_q;",
        "  logic [28:0] sdr_row_q;",
        "  logic sdr_write_pending_q;",
        "  logic sdr_write_active_q;",
        "  logic [14:0] sdr_write_command_q;",
        "  // Safe synthesis default; the generated smoke monitor alone overrides this",
        "  // internal simulation hook hierarchically to fire each protocol detector.",
        "  /* verilator lint_off MULTIDRIVEN */",
        "  logic [3:0] protocol_fault_i = 4'd0;",
        "  /* verilator lint_on MULTIDRIVEN */",
        "  logic [1:0] ring_state_checked_c;",
        "  logic hps_req_write_checked_c;",
        "  logic [15:0] counter_id_checked_c;",
        "  logic guard_verdict_ok_checked_c;",
        "  logic guard_verdict_violation_checked_c;",
        "  logic guard_beat_valid_checked_c;",
        "  logic guard_beat_last_checked_c;",
        "  logic sdr_dq_oe_checked_c;",
        "",
        "  // Positive progress witnesses used only by the deterministic smoke test.",
        "  logic [31:0] ring_transactions_q;",
        "  logic [31:0] ring_done_posts_q;",
        "  logic [31:0] ring_done_frees_q;",
        "  logic [31:0] ring_done_dwell_q;",
        "  logic [31:0] ring_sequence_errors_q;",
        "  logic [31:0] hps_read_completions_q;",
        "  logic [31:0] hps_write_watchdog_faults_q;",
        "  logic [31:0] hps_first_beat_timing_witnesses_q;",
        "  logic [31:0] hps_first_beat_timing_errors_q;",
        "  logic [31:0] blit_successes_q;",
        "  logic [31:0] blit_failures_q;",
        "  logic [3:0] pad_directed_phases_q /* verilator public_flat_rd */;",
        "  logic [31:0] audio_accepts_q;",
        "  logic [31:0] counter_accepts_q;",
        "  logic [31:0] counter_windows_q;",
        "  logic [31:0] counter_sequence_errors_q;",
        "  logic [39:0] counter_selector_seen_q /* verilator public_flat_rd */;",
        "  logic [31:0] render_accepts_q;",
        "  logic [31:0] render_directed_accepts_q;",
        "  logic [31:0] render_entropy_accepts_q;",
        "  logic [31:0] render_entropy_completions_q;",
        "  logic [31:0] render_backpressure_witnesses_q;",
        "  logic [31:0] render_entropy_backpressure_witnesses_q;",
        "  logic [31:0] render_backpressure_stability_errors_q;",
        "  logic [31:0] render_accept_class_errors_q;",
        "  logic [31:0] render_entropy_accepted_mode_q /* verilator public_flat_rd */;",
        "  logic [63:0] render_entropy_accepted_fill_q /* verilator public_flat_rd */;",
        "  logic [63:0] render_entropy_accepted_clear_q /* verilator public_flat_rd */;",
        "  logic [31:0] render_drains_q;",
        "  logic [31:0] render_timeouts_q;",
        "  logic [31:0] guard_accepts_q;",
        "  logic [31:0] guard_rejects_q;",
        "  logic [31:0] guard_last_beats_q;",
        "  logic [31:0] guard_exact_frames_q;",
        "  logic [31:0] guard_beat_timeouts_q;",
        "  logic [31:0] guard_verdict_timeouts_q;",
        "  logic [31:0] guard_early_last_errors_q;",
        "  logic [31:0] guard_late_last_errors_q;",
        "  logic [31:0] guard_extra_beat_errors_q;",
        "  logic [31:0] guard_verdict_extra_witnesses_q /* verilator public_flat_rd */;",
        "  logic [31:0] guard_post_denial_extra_witnesses_q /* verilator public_flat_rd */;",
        "  logic [2:0] guard_preownership_fault_windows_q /* verilator public_flat_rd */;",
        "  logic [31:0] sdr_read_responses_q;",
        "  logic [31:0] sdr_write_transactions_q;",
        "  logic [31:0] sdr_write_observations_q;",
        "  logic [31:0] sdr_write_phase_errors_q;",
        "",
        "  always_ff @(posedge gpu_clk or negedge rst_n) begin",
        "    if (!rst_n) release_q <= '0;",
        "    else release_q <= {release_q[8:0], 1'b1};",
        "  end",
        "  assign run_c = release_q[9]; // 2 sync edges + 8 warm-up edges",
        "  assign entropy_c = {32{lfsr_q ^ cycle_q}};",
        "  // Fault controls perturb observed protocol operands; the unchanged detector",
        "  // arms below must then report the deliberately malformed transaction.",
        "  assign ring_state_checked_c = (protocol_fault_i == 4'd1)",
        "                                  ? 2'd2 : ring_wr_state_o_captured_i;",
        "  assign hps_req_write_checked_c = hps_req_write_o_captured_i ||",
        "                                     (protocol_fault_i == 4'd2);",
        "  assign counter_id_checked_c = cnt_snap_id_o_captured_i ^",
        "                                ((protocol_fault_i == 4'd3) ? 16'd1 : 16'd0);",
        "  assign guard_verdict_ok_checked_c = geom_guard_rsp_o_captured_i.ok &&",
        "                                         (protocol_fault_i != 4'd7);",
        "  assign guard_verdict_violation_checked_c = geom_guard_rsp_o_captured_i.violation &&",
        "                                                (protocol_fault_i != 4'd7);",
        "  assign guard_beat_valid_checked_c =",
        "      (protocol_fault_i == 4'd5) ? 1'b0 :",
        "      (geom_beat_valid_o_captured_i ||",
        "       ((protocol_fault_i == 4'd10) && guard_frame_owned_q &&",
        "        !guard_beat_pending_q && (guard_extra_fault_delay_q == 4'd1)) ||",
        "       ((protocol_fault_i == 4'd13) && guard_frame_owned_q &&",
        "        guard_response_pending_q &&",
        "        (guard_verdict_extra_fault_delay_q == 4'd1)) ||",
        "       ((protocol_fault_i == 4'd14) && guard_denial_watch_q &&",
        "        !guard_response_pending_q && !guard_beat_pending_q &&",
        "        (guard_denial_extra_fault_delay_q == 4'd1)) ||",
        "       ((protocol_fault_i == 4'd15) && guard_preownership_watch_q &&",
        "        !guard_beat_pending_q &&",
        "        ((!guard_response_pending_q && !geom_guard_req_i.valid &&",
        "          !guard_preownership_fault_windows_q[0]) ||",
        "         (!guard_response_pending_q && geom_guard_req_i.valid &&",
        "          !guard_preownership_fault_windows_q[1]) ||",
        "         (guard_response_pending_q &&",
        "          !guard_verdict_ok_checked_c &&",
        "          !guard_verdict_violation_checked_c &&",
        "          !guard_preownership_fault_windows_q[2]))));",
        "  assign guard_beat_last_checked_c =",
        "      ((protocol_fault_i == 4'd8) && geom_beat_valid_o_captured_i &&",
        "       (guard_beat_count_q == 4'd0)) ? 1'b1 :",
        "      ((protocol_fault_i == 4'd9) && geom_beat_valid_o_captured_i &&",
        "       (guard_beat_count_q == 4'd7)) ? 1'b0 :",
        "      geom_beat_last_o_captured_i;",
        "  assign sdr_dq_oe_checked_c = phy_dq_oe_o_captured_i;",
        "  assign render_offer_payload_c = {" + ", ".join(render_offer_fields) + "};",
        *(
            "  assign render_triangle_" + group + "_c = {" + ", ".join(fields) + "};"
            for group, fields in render_triangle_groups.items()
        ),
        "",
        _render_packet_function(packet),
        "",
        "  function automatic logic [63:0] packet_word(",
        "      input logic [31:0] address);",
        "    integer byte_index;",
        "    begin",
        "      for (byte_index = 0; byte_index < 8; byte_index = byte_index + 1)",
        "        packet_word[byte_index*8 +: 8] = packet_byte(address + byte_index);",
        "    end",
        "  endfunction",
        "",
        "  integer slot;",
        "  integer pad;",
        "  always_ff @(posedge gpu_clk or negedge rst_n) begin",
        "    if (!rst_n) begin",
        "      lfsr_q <= 64'h9e37_79b9_7f4a_7c15;",
        "      cycle_q <= '0;",
        "      hps_state_q <= HPS_IDLE;",
        "      render_state_q <= RENDER_WAIT_INIT;",
        "      hps_addr_q <= '0;",
        "      hps_len_q <= '0;",
        "      hps_write_q <= 1'b0;",
        "      hps_wait_q <= '0;",
        "      hps_beat_q <= '0;",
        "      hps_first_beat_age_q <= '0;",
        "      hps_first_beat_pending_q <= 1'b0;",
        "      hps_timing_fault_hold_q <= 1'b0;",
        "      blit_source_read_q <= 1'b0;",
        "      render_enable_q <= 1'b0;",
        "      ring_ready_capture_q <= 1'b0;",
        "      audio_pause_q <= '0;",
        "      pad_phase_q <= '0;",
        "      cnt_ready_capture_q <= 1'b0;",
        "      counter_next_id_q <= '0;",
        "      counter_entries_q <= '0;",
        "      render_width_cover_q <= 1'b0;",
        "      render_job_entropy_q <= 1'b0;",
        "      render_profile_done_q <= 1'b0;",
        "      render_offer_stalled_q <= 1'b0;",
        "      render_offer_mutated_q <= 1'b0;",
        "      render_offer_snapshot_q <= '0;",
        "      render_entropy_offer_snapshot_q <= '0;",
        "      render_accepted_payload_q <= '0;",
        "      render_entropy_accepted_payload_q <= '0;",
        *(
            f"      render_directed_accepted_{group}_q <= '0;"
            for group in render_triangle_groups
        ),
        *(
            f"      render_entropy_accepted_{group}_q <= '0;"
            for group in render_triangle_groups
        ),
        "      render_wait_q <= '0;",
        "      render_issued_start_q <= '0;",
        "      render_retired_start_q <= '0;",
        "      render_work_seen_q <= 1'b0;",
        "      render_retirement_seen_q <= 1'b0;",
        "      guard_negative_q <= 1'b0;",
        "      guard_response_pending_q <= 1'b0;",
        "      guard_verdict_wait_q <= '0;",
        "      guard_beat_pending_q <= 1'b0;",
        "      guard_beat_count_q <= '0;",
        "      guard_frame_owned_q <= 1'b0;",
        "      guard_denial_watch_q <= 1'b0;",
        "      guard_preownership_watch_q <= 1'b1;",
        "      guard_extra_fault_delay_q <= '0;",
        "      guard_verdict_extra_fault_delay_q <= '0;",
        "      guard_denial_extra_fault_delay_q <= '0;",
        "      guard_beat_wait_q <= '0;",
        "      guard_gap_q <= '0;",
        "      sdr_read_wait_q <= '0;",
        "      sdr_read_beat_q <= '0;",
        "      sdr_row_q <= '0;",
        "      sdr_write_pending_q <= 1'b0;",
        "      sdr_write_active_q <= 1'b0;",
        "      sdr_write_command_q <= '0;",
        "      ring_transactions_q <= '0;",
        "      ring_done_posts_q <= '0;",
        "      ring_done_frees_q <= '0;",
        "      ring_done_dwell_q <= '0;",
        "      ring_sequence_errors_q <= '0;",
        "      hps_read_completions_q <= '0;",
        "      hps_write_watchdog_faults_q <= '0;",
        "      hps_first_beat_timing_witnesses_q <= '0;",
        "      hps_first_beat_timing_errors_q <= '0;",
        "      blit_successes_q <= '0;",
        "      blit_failures_q <= '0;",
        "      pad_directed_phases_q <= '0;",
        "      audio_accepts_q <= '0;",
        "      counter_accepts_q <= '0;",
        "      counter_windows_q <= '0;",
        "      counter_sequence_errors_q <= '0;",
        "      counter_selector_seen_q <= '0;",
        "      render_accepts_q <= '0;",
        "      render_directed_accepts_q <= '0;",
        "      render_entropy_accepts_q <= '0;",
        "      render_entropy_completions_q <= '0;",
        "      render_backpressure_witnesses_q <= '0;",
        "      render_entropy_backpressure_witnesses_q <= '0;",
        "      render_backpressure_stability_errors_q <= '0;",
        "      render_accept_class_errors_q <= '0;",
        "      render_entropy_accepted_mode_q <= '0;",
        "      render_entropy_accepted_fill_q <= '0;",
        "      render_entropy_accepted_clear_q <= '0;",
        "      render_drains_q <= '0;",
        "      render_timeouts_q <= '0;",
        "      guard_accepts_q <= '0;",
        "      guard_rejects_q <= '0;",
        "      guard_last_beats_q <= '0;",
        "      guard_exact_frames_q <= '0;",
        "      guard_beat_timeouts_q <= '0;",
        "      guard_verdict_timeouts_q <= '0;",
        "      guard_early_last_errors_q <= '0;",
        "      guard_late_last_errors_q <= '0;",
        "      guard_extra_beat_errors_q <= '0;",
        "      guard_verdict_extra_witnesses_q <= '0;",
        "      guard_post_denial_extra_witnesses_q <= '0;",
        "      guard_preownership_fault_windows_q <= '0;",
        "      sdr_read_responses_q <= '0;",
        "      sdr_write_transactions_q <= '0;",
        "      sdr_write_observations_q <= '0;",
        "      sdr_write_phase_errors_q <= '0;",
    ]
    # Every pseudo-input is explicitly reset.  Unpacked elements are named one
    # by one so no tool-specific whole-array assignment is required.
    for port in driven:
        for suffix, _offset, _width in flatten_port_elements(port):
            lines.append(f"      {port.name}{suffix} <= '0;")
    lines.extend(
        [
            "      for (slot = 0; slot < 3; slot = slot + 1) ring_delay_q[slot] <= 8'(slot * 17);",
            "    end else if (run_c) begin",
            "      lfsr_q <= {lfsr_q[62:0], ^(lfsr_q & 64'hd800_0000_0000_0000)};",
            "      cycle_q <= cycle_q + 64'd1;",
            "      ring_ready_capture_q <= ring_wr_ready_i;",
            "      cnt_ready_capture_q <= cnt_snap_ready_i;",
            "",
            "      // Defaults for one-cycle protocol pulses and don't-care payload motion.",
            "      hps_req_grant_i <= 1'b0;",
            "      hps_rd_valid_i <= 1'b0;",
            "      hps_rd_last_i <= 1'b0;",
            "      hps_rd_data_i <= entropy_c[127 +: 64];",
            "      render_frame_begin_i <= 1'b0;",
            "      render_frame_end_i <= 1'b0;",
            "      cnt_snap_ready_i <= (cycle_q[3:0] != 4'hf);",
            "      ring_wr_ready_i <= (cycle_q[2:0] != 3'h7);",
            "",
            "      // The first accepted blit-source request proves that BeginFrame installed",
            "      // a live shared framebuffer lease. Its registered response is held while",
            "      // the one-shot renderer owns and retires work; completion hands the same",
            "      // lease to blit before any source data can launch a blit write.",
            "",
            "      // Three coherent FRAME_RING slots. FPGA-posted DONE remains HPS-visible",
            "      // until the scheduler's later DONE -> FREE write is accepted.",
            "      for (slot = 0; slot < 3; slot = slot + 1) begin",
            "        if (hps_state_i[slot] == 2'd3)",
            "          ring_done_dwell_q <= ring_done_dwell_q + 32'd1;",
            "        if (ring_delay_q[slot] != 0) begin",
            "          ring_delay_q[slot] <= ring_delay_q[slot] - 8'd1;",
            "        end else if (hps_state_i[slot] == 2'd0) begin",
            "          hps_state_i[slot] <= 2'd1;",
            f"          hps_byte_len_i[slot] <= 32'd{len(packet)};",
            "        end else if (hps_state_i[slot] == 2'd1) begin",
            "          hps_state_i[slot] <= 2'd2;",
            "        end",
            "      end",
            "      if (ring_wr_valid_o_captured_i && ring_ready_capture_q) begin",
            "        ring_transactions_q <= ring_transactions_q + 32'd1;",
            "        if (ring_state_checked_c == 2'd3) begin",
            "          hps_state_i[ring_wr_slot_o_captured_i] <= 2'd3;",
            "          ring_done_posts_q <= ring_done_posts_q + 32'd1;",
            "        end else if (ring_state_checked_c == 2'd0) begin",
            "          if (hps_state_i[ring_wr_slot_o_captured_i] == 2'd3)",
            "            ring_done_frees_q <= ring_done_frees_q + 32'd1;",
            "          else",
            "            ring_sequence_errors_q <= ring_sequence_errors_q + 32'd1;",
            "          hps_state_i[ring_wr_slot_o_captured_i] <= 2'd0;",
            "          hps_byte_len_i[ring_wr_slot_o_captured_i] <= 32'd0;",
            "          ring_delay_q[ring_wr_slot_o_captured_i] <= 8'd23;",
            "        end else begin",
            "          ring_sequence_errors_q <= ring_sequence_errors_q + 32'd1;",
            "        end",
            "      end",
            "      if (blit_done_o_captured_i) begin",
            "        if (blit_status_o_captured_i == 8'd0)",
            "          blit_successes_q <= blit_successes_q + 32'd1;",
            "        else",
            "          blit_failures_q <= blit_failures_q + 32'd1;",
            "      end",
            "",
            "      // Registered HPS responder: 1..4 grant cycles and exactly the established",
            "      // 16 idle edges followed by first data on the next edge. The first-beat",
            "      // age witness is independent of the response state transition.",
            "      case (hps_state_q)",
            "        HPS_IDLE: if (hps_req_valid_o_captured_i) begin",
            "          hps_addr_q <= hps_req_addr_o_captured_i;",
            "          hps_len_q <= hps_req_len_o_captured_i;",
            "          hps_write_q <= hps_req_write_checked_c;",
            "          blit_source_read_q <= !hps_req_write_checked_c &&",
            "                                hps_req_addr_o_captured_i[31:20] == 12'h002;",
            "          if (!hps_req_write_checked_c &&",
            "              hps_req_addr_o_captured_i[31:20] == 12'h002 &&",
            "              !render_profile_done_q && render_timeouts_q == 0) begin",
            "            render_enable_q <= 1'b1;",
            "            fb_writer_i <= 1'b1;",
            "          end",
            "          hps_wait_q <= {5'd0, cycle_q[1:0]} + 7'd1;",
            "          hps_beat_q <= '0;",
            "          hps_state_q <= HPS_GRANT_WAIT;",
            "        end",
            "        HPS_GRANT_WAIT: if (hps_wait_q != 0) begin",
            "          hps_wait_q <= hps_wait_q - 7'd1;",
            "        end else begin",
            "          hps_req_grant_i <= 1'b1;",
            "          hps_wait_q <= hps_write_q ? 7'd127 : 7'd16;",
            "          hps_first_beat_age_q <= '0;",
            "          hps_first_beat_pending_q <= !hps_write_q && !blit_source_read_q;",
            "          hps_timing_fault_hold_q <= 1'b0;",
            "          hps_state_q <= hps_write_q ? HPS_WRITE_DATA : HPS_READ_LATENCY;",
            "        end",
            "        HPS_READ_LATENCY: if (hps_wait_q != 0) begin",
            "          hps_wait_q <= hps_wait_q - 7'd1;",
            "          if (hps_first_beat_pending_q)",
            "            hps_first_beat_age_q <= hps_first_beat_age_q + 8'd1;",
            "        end else if ((protocol_fault_i == 4'd11) &&",
            "                     hps_first_beat_pending_q && !hps_timing_fault_hold_q) begin",
            "          hps_timing_fault_hold_q <= 1'b1;",
            "          hps_first_beat_age_q <= hps_first_beat_age_q + 8'd1;",
            "        end else if (blit_source_read_q && !render_profile_done_q &&",
            "                     render_timeouts_q == 0) begin",
            "          hps_state_q <= HPS_READ_BEATS;",
            "        end else begin",
            "          hps_rd_valid_i <= 1'b1;",
            "          hps_rd_data_i <= packet_word(hps_addr_q);",
            "          hps_rd_last_i <= (7'd8 >= hps_len_q);",
            "          if (hps_first_beat_pending_q) begin",
            "            if (hps_first_beat_age_q == 8'd16)",
            "              hps_first_beat_timing_witnesses_q <= hps_first_beat_timing_witnesses_q + 32'd1;",
            "            else",
            "              hps_first_beat_timing_errors_q <= hps_first_beat_timing_errors_q + 32'd1;",
            "            hps_first_beat_pending_q <= 1'b0;",
            "          end",
            "          if (7'd8 >= hps_len_q) begin",
            "            blit_source_read_q <= 1'b0;",
            "            hps_read_completions_q <= hps_read_completions_q + 32'd1;",
            "            hps_state_q <= HPS_IDLE;",
            "          end else begin",
            "            hps_beat_q <= 7'd1;",
            "            hps_state_q <= HPS_READ_BEATS;",
            "          end",
            "        end",
            "        HPS_READ_BEATS: begin",
            "          if (blit_source_read_q && !render_profile_done_q &&",
            "              render_timeouts_q == 0) begin",
            "            // Keep registered response controls idle through both render jobs.",
            "            hps_rd_valid_i <= 1'b0;",
            "            hps_rd_last_i <= 1'b0;",
            "          end else begin",
            "            hps_rd_valid_i <= 1'b1;",
            "            hps_rd_data_i <= packet_word(hps_addr_q + {22'd0, hps_beat_q, 3'b000});",
            "            hps_rd_last_i <= ((hps_beat_q + 7'd1) * 8 >= hps_len_q);",
            "            if ((hps_beat_q + 7'd1) * 8 >= hps_len_q) begin",
            "              blit_source_read_q <= 1'b0;",
            "              hps_read_completions_q <= hps_read_completions_q + 32'd1;",
            "              hps_state_q <= HPS_IDLE;",
            "            end else begin",
            "              hps_beat_q <= hps_beat_q + 7'd1;",
            "            end",
            "          end",
            "        end",
            "        HPS_WRITE_DATA: begin",
            "          if (hps_wr_valid_o_captured_i && hps_wr_last_o_captured_i) begin",
            "            hps_wait_q <= '0;",
            "            hps_state_q <= HPS_IDLE;",
            "          end else if (hps_wait_q != 0) begin",
            "            hps_wait_q <= hps_wait_q - 7'd1;",
            "          end else begin",
            "            hps_write_watchdog_faults_q <= hps_write_watchdog_faults_q + 32'd1;",
            "            hps_state_q <= HPS_IDLE;",
            "          end",
            "        end",
            "        default: hps_state_q <= HPS_IDLE;",
            "      endcase",
            "",
            "      // Directed pad phases cover center, signed INT_MIN/INT_MAX, then entropy.",
            "      if (cycle_q[5:0] == 6'd0) begin",
            "        pad_directed_phases_q[pad_phase_q] <= 1'b1;",
            "        case (pad_phase_q)",
            "          2'd0: begin",
            "            pad_present_i <= 4'h0;",
            "            for (pad = 0; pad < 4; pad = pad + 1) begin",
            "              pad_buttons_i[pad] <= 32'h0000_0000;",
            "              pad_lx_i[pad] <= 16'h0000;",
            "              pad_ly_i[pad] <= 16'h0000;",
            "              pad_rx_i[pad] <= 16'h0000;",
            "              pad_ry_i[pad] <= 16'h0000;",
            "            end",
            "          end",
            "          2'd1: begin",
            "            pad_present_i <= 4'hf;",
            "            for (pad = 0; pad < 4; pad = pad + 1) begin",
            "              pad_buttons_i[pad] <= 32'h5555_5555;",
            "              pad_lx_i[pad] <= 16'h8000;",
            "              pad_ly_i[pad] <= 16'h8000;",
            "              pad_rx_i[pad] <= 16'h8000;",
            "              pad_ry_i[pad] <= 16'h8000;",
            "            end",
            "          end",
            "          2'd2: begin",
            "            pad_present_i <= 4'hf;",
            "            for (pad = 0; pad < 4; pad = pad + 1) begin",
            "              pad_buttons_i[pad] <= 32'haaaa_aaaa;",
            "              pad_lx_i[pad] <= 16'h7fff;",
            "              pad_ly_i[pad] <= 16'h7fff;",
            "              pad_rx_i[pad] <= 16'h7fff;",
            "              pad_ry_i[pad] <= 16'h7fff;",
            "            end",
            "          end",
            "          default: begin",
            "            pad_present_i <= cycle_q[9:6];",
            "            for (pad = 0; pad < 4; pad = pad + 1) begin",
            "              pad_buttons_i[pad] <= entropy_c[256 + pad*32 +: 32];",
            "              pad_lx_i[pad] <= entropy_c[384 + pad*16 +: 16];",
            "              pad_ly_i[pad] <= entropy_c[448 + pad*16 +: 16];",
            "              pad_rx_i[pad] <= entropy_c[512 + pad*16 +: 16];",
            "              pad_ry_i[pad] <= entropy_c[576 + pad*16 +: 16];",
            "            end",
            "          end",
            "        endcase",
            "        pad_phase_q <= pad_phase_q + 2'd1;",
            "      end",
            "",
            "      // Ready/valid stereo producer; samples remain stable during backpressure.",
            "      if (aud_wr_valid_i && aud_wr_ready_o_captured_i) begin",
            "        audio_accepts_q <= audio_accepts_q + 32'd1;",
            "        if (audio_accepts_q[2:0] == 3'h7) begin",
            "          aud_wr_valid_i <= 1'b0;",
            "          audio_pause_q <= 8'd255;",
            "        end else begin",
            "          aud_wr_l_i <= entropy_c[640 +: 16];",
            "          aud_wr_r_i <= entropy_c[672 +: 16];",
            "        end",
            "      end else if (!aud_wr_valid_i) begin",
            "        if (audio_pause_q != 0) audio_pause_q <= audio_pause_q - 8'd1;",
            "        else begin",
            "          aud_wr_valid_i <= 1'b1;",
            "          aud_wr_l_i <= entropy_c[704 +: 16];",
            "          aud_wr_r_i <= entropy_c[736 +: 16];",
            "        end",
            "      end",
            "      // The captured read-window beat is paired with the ready level from",
            "      // the same shell edge. Require one exact ascending 0..39 window.",
            "      if (cnt_snap_valid_o_captured_i && cnt_ready_capture_q) begin",
            "        counter_accepts_q <= counter_accepts_q + 32'd1;",
            "        if (counter_id_checked_c < 16'd40) begin",
            "          counter_selector_seen_q[counter_id_checked_c[5:0]] <= 1'b1;",
            "          if (counter_id_checked_c != counter_next_id_q)",
            "            counter_sequence_errors_q <= counter_sequence_errors_q + 32'd1;",
            "          if (counter_id_checked_c == 16'd39) begin",
            "            if (counter_next_id_q == 16'd39 && counter_entries_q == 7'd39)",
            "              counter_windows_q <= counter_windows_q + 32'd1;",
            "            else",
            "              counter_sequence_errors_q <= counter_sequence_errors_q + 32'd1;",
            "            counter_next_id_q <= '0;",
            "            counter_entries_q <= '0;",
            "          end else begin",
            "            counter_next_id_q <= counter_id_checked_c + 16'd1;",
            "            counter_entries_q <= counter_entries_q + 7'd1;",
            "          end",
            "        end else begin",
            "          counter_sequence_errors_q <= counter_sequence_errors_q + 32'd1;",
            "        end",
            "      end",
            "",
            "      // Two lease-protected jobs are mandatory: a legal directed retirement,",
            "      // then a distinct entropy-width transaction held through clear backpressure.",
            "      case (render_state_q)",
            "        RENDER_WAIT_INIT: if (init_done_o_captured_i && render_enable_q) begin",
            "          render_issued_start_q <= render_issued_words_o_captured_i;",
            "          render_retired_start_q <= render_retired_words_o_captured_i;",
            "          render_work_seen_q <= 1'b0;",
            "          render_retirement_seen_q <= 1'b0;",
            "          render_job_entropy_q <= render_width_cover_q;",
            "          render_width_cover_q <= ~render_width_cover_q;",
            "          render_grid_w_i <= 6'd4;",
            "          render_grid_h_i <= 6'd4;",
            "          render_fill_word_i <= render_width_cover_q ? entropy_c[896 +: 64] : 64'ha5a5_a5a5_a5a5_a5a5;",
            "          render_clear_word_i <= render_width_cover_q ? entropy_c[960 +: 64] : 64'h5a5a_5a5a_5a5a_5a5a;",
            "          render_state_i <= render_width_cover_q ? 32'ha5e0_0068 : 32'h0000_0000;",
            "          render_src_a_i <= render_width_cover_q ? entropy_c[1056 +: 8] : 8'hff;",
            "          render_texel_rgb_i <= render_width_cover_q ? entropy_c[1088 +: 24] : 24'hff00ff;",
            "          render_texel_a_i <= render_width_cover_q ? entropy_c[1120 +: 8] : 8'hff;",
            "          render_texel_idx_i <= entropy_c[1152 +: 8];",
            "          render_fb_base_i <= 27'd0;",
            "          render_fb_stride_i <= 16'd128;",
        ]
    )
    directed_values = _render_triangle_values(
        a=(1024, 1024),
        b=(15360, 2048),
        c=(2048, 15360),
        source_id=0x2A2A,
    )
    # A second legal, positive-area guard-band triangle. Its scissored box covers
    # the 64x64 frame while every coefficient, vertex, bound, top-left vector,
    # and source identifier differs from the directed transaction.
    entropy_values = _render_triangle_values(
        a=(-1024, 15872),
        b=(-1792, -1024),
        c=(16128, 512),
        source_id=0xD5E7,
    )
    matching_triangle_fields = sorted(
        name
        for name in grouped_triangle_names
        if directed_values[name] == entropy_values[name]
    )
    if matching_triangle_fields:
        raise ShellPortError(
            "directed and entropy renderer triangle fields are not all distinct: "
            + ", ".join(matching_triangle_fields)
        )
    directed_triangle_match = " && ".join(
        f"(render_triangle_{group}_c == {{{', '.join(directed_values[name] for name in fields)}}})"
        for group, fields in render_triangle_groups.items()
    )
    entropy_triangle_match = " && ".join(
        f"(render_triangle_{group}_c == {{{', '.join(entropy_values[name] for name in fields)}}})"
        for group, fields in render_triangle_groups.items()
    )
    for port in render_payload:
        if port.name not in directed_values or port.name not in entropy_values:
            raise ShellPortError(
                f"renderer triangle field {port.name!r} lacks an exact two-job value"
            )
        lines.append(
            f"            {port.name} <= render_width_cover_q ? "
            f"{entropy_values[port.name]} : {directed_values[port.name]};"
        )
    lines.extend(
        [
            "          render_frame_begin_i <= 1'b1;",
            "          render_offer_stalled_q <= 1'b0;",
            "          render_offer_mutated_q <= 1'b0;",
            "          render_wait_q <= '0;",
            "          render_state_q <= RENDER_BEGIN;",
            "        end",
            "        RENDER_BEGIN: begin",
            "          // Offer the installed mode and payload immediately after frame_begin.",
            "          // The binner's frame-clear phase supplies genuine ready backpressure.",
            "          render_tri_valid_i <= 1'b1;",
            "          render_state_q <= RENDER_OFFER;",
            "        end",
            "        RENDER_OFFER: begin",
            "          if (render_tri_valid_i && !render_tri_ready_o_native_i) begin",
            "            if (!render_offer_stalled_q) begin",
            "              render_offer_snapshot_q <= render_offer_payload_c;",
            "              if (render_job_entropy_q)",
            "                render_entropy_offer_snapshot_q <= render_offer_payload_c;",
            "              render_offer_stalled_q <= 1'b1;",
            "            end else if (render_offer_payload_c != render_offer_snapshot_q) begin",
            "              render_backpressure_stability_errors_q <=",
            "                  render_backpressure_stability_errors_q + 32'd1;",
            "            end",
            "            // Positive control 12 changes a real offered field after the snapshot.",
            "            if ((protocol_fault_i == 4'd12) && render_offer_stalled_q &&",
            "                !render_offer_mutated_q) begin",
            "              render_fill_word_i <= render_fill_word_i ^ 64'h1;",
            "              render_offer_mutated_q <= 1'b1;",
            "            end",
            "          end",
            "          if (render_tri_valid_i && render_tri_ready_o_native_i) begin",
            "            render_tri_valid_i <= 1'b0;",
            "            render_accepts_q <= render_accepts_q + 32'd1;",
            "            render_accepted_payload_q <= render_offer_payload_c;",
            "            if (render_offer_stalled_q) begin",
            "              if (render_offer_payload_c == render_offer_snapshot_q) begin",
            "                render_backpressure_witnesses_q <= render_backpressure_witnesses_q + 32'd1;",
            "                if (render_job_entropy_q)",
            "                  render_entropy_backpressure_witnesses_q <=",
            "                      render_entropy_backpressure_witnesses_q + 32'd1;",
            "              end else",
            "                render_backpressure_stability_errors_q <=",
            "                    render_backpressure_stability_errors_q + 32'd1;",
            "            end",
            "            if (render_job_entropy_q) begin",
            "              // Classify using values sampled on the actual acceptance edge,",
            "              // independently from the label that launched the transaction.",
            "              render_entropy_accepted_payload_q <= render_offer_payload_c;",
            "              render_entropy_accepted_mode_q <= render_state_i;",
            "              render_entropy_accepted_fill_q <= render_fill_word_i;",
            "              render_entropy_accepted_clear_q <= render_clear_word_i;",
            *(
                f"              render_entropy_accepted_{group}_q <= render_triangle_{group}_c;"
                for group in render_triangle_groups
            ),
            "              if ((render_state_i == 32'ha5e0_0068) &&",
            "                  (render_fill_word_i != 64'ha5a5_a5a5_a5a5_a5a5) &&",
            "                  (render_clear_word_i != 64'h5a5a_5a5a_5a5a_5a5a) &&",
            f"                  ({entropy_triangle_match}))",
            "                render_entropy_accepts_q <= render_entropy_accepts_q + 32'd1;",
            "              else",
            "                render_accept_class_errors_q <= render_accept_class_errors_q + 32'd1;",
            "            end else begin",
            *(
                f"              render_directed_accepted_{group}_q <= render_triangle_{group}_c;"
                for group in render_triangle_groups
            ),
            "              if ((render_state_i == 32'h0000_0000) &&",
            "                  (render_fill_word_i == 64'ha5a5_a5a5_a5a5_a5a5) &&",
            "                  (render_clear_word_i == 64'h5a5a_5a5a_5a5a_5a5a) &&",
            f"                  ({directed_triangle_match}))",
            "                render_directed_accepts_q <= render_directed_accepts_q + 32'd1;",
            "              else",
            "                render_accept_class_errors_q <= render_accept_class_errors_q + 32'd1;",
            "            end",
            "            render_frame_end_i <= 1'b1;",
            "            render_state_q <= RENDER_END;",
            "          end",
            "        end",
            "        RENDER_END: begin",
            "          render_wait_q <= '0;",
            "          render_state_q <= RENDER_DRAIN;",
            "        end",
            "        RENDER_DRAIN: begin",
            "          render_wait_q <= render_wait_q + 16'd1;",
            "          if (render_issued_words_o_captured_i > render_issued_start_q)",
            "            render_work_seen_q <= 1'b1;",
            "          if (render_retired_words_o_captured_i > render_retired_start_q)",
            "            render_retirement_seen_q <= 1'b1;",
            "          if ((protocol_fault_i != 4'd4) && !render_job_entropy_q &&",
            "              (render_work_seen_q ||",
            "               render_issued_words_o_captured_i > render_issued_start_q) &&",
            "              (render_retirement_seen_q ||",
            "               render_retired_words_o_captured_i > render_retired_start_q) &&",
            "              render_drained_o_captured_i &&",
            "              render_issued_words_o_captured_i == render_retired_words_o_captured_i) begin",
            "            render_state_q <= RENDER_WAIT_INIT;",
            "          end else if ((protocol_fault_i != 4'd4) && render_job_entropy_q &&",
            "                       (render_work_seen_q ||",
            "                        render_issued_words_o_captured_i > render_issued_start_q) &&",
            "                       (render_retirement_seen_q ||",
            "                        render_retired_words_o_captured_i > render_retired_start_q) &&",
            "                       render_drained_o_captured_i &&",
            "                       render_issued_words_o_captured_i == render_retired_words_o_captured_i) begin",
            "            render_entropy_completions_q <= render_entropy_completions_q + 32'd1;",
            "            render_drains_q <= render_drains_q + 32'd1;",
            "            render_profile_done_q <= 1'b1;",
            "            render_enable_q <= 1'b0;",
            "            fb_writer_i <= 1'b0;",
            "            render_state_q <= RENDER_WAIT_INIT;",
            "          end else if (&render_wait_q ||",
            "                       ((protocol_fault_i == 4'd4) && &render_wait_q[7:0])) begin",
            "            render_timeouts_q <= render_timeouts_q + 32'd1;",
            "            render_enable_q <= 1'b0;",
            "            fb_writer_i <= 1'b0;",
            "            render_state_q <= RENDER_WAIT_INIT;",
            "          end",
            "        end",
            "        default: render_state_q <= RENDER_WAIT_INIT;",
            "      endcase",
            "",
            "      // A legal geometry read owns exactly eight accepted beats. Before the",
            "      // first legal verdict, reset-active quarantine owns every unsolicited",
            "      // beat through initial idle, request wait, and response pending. Completed",
            "      // ownership persists through the next verdict; a denied verdict then",
            "      // starts quarantine through idle/request/pending states until a later",
            "      // legal verdict begins new ownership.",
            "      if (guard_frame_owned_q && !guard_beat_pending_q &&",
            "          (guard_extra_fault_delay_q != 0))",
            "        guard_extra_fault_delay_q <= guard_extra_fault_delay_q - 4'd1;",
            "      if (guard_frame_owned_q && guard_response_pending_q &&",
            "          (guard_verdict_extra_fault_delay_q != 0))",
            "        guard_verdict_extra_fault_delay_q <=",
            "            guard_verdict_extra_fault_delay_q - 4'd1;",
            "      if (guard_denial_watch_q && !guard_response_pending_q &&",
            "          !guard_beat_pending_q && (guard_denial_extra_fault_delay_q != 0))",
            "        guard_denial_extra_fault_delay_q <=",
            "            guard_denial_extra_fault_delay_q - 4'd1;",
            "      if (guard_beat_pending_q) begin",
            "        if (guard_beat_valid_checked_c) begin",
            "          guard_beat_wait_q <= '0;",
            "          if (guard_beat_count_q < 4'd7) begin",
            "            if (guard_beat_last_checked_c) begin",
            "              guard_early_last_errors_q <= guard_early_last_errors_q + 32'd1;",
            "              guard_beat_pending_q <= 1'b0;",
            "              guard_frame_owned_q <= 1'b0;",
            "              guard_gap_q <= 8'd11;",
            "            end else begin",
            "              guard_beat_count_q <= guard_beat_count_q + 4'd1;",
            "            end",
            "          end else if (guard_beat_count_q == 4'd7) begin",
            "            guard_beat_pending_q <= 1'b0;",
            "            if (guard_beat_last_checked_c) begin",
            "              guard_last_beats_q <= guard_last_beats_q + 32'd1;",
            "              guard_exact_frames_q <= guard_exact_frames_q + 32'd1;",
            "              guard_gap_q <= 8'd11;",
            "              guard_extra_fault_delay_q <= 4'd5;",
            "            end else begin",
            "              guard_late_last_errors_q <= guard_late_last_errors_q + 32'd1;",
            "              guard_frame_owned_q <= 1'b0;",
            "              guard_gap_q <= 8'd11;",
            "            end",
            "          end else begin",
            "            guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;",
            "            guard_beat_pending_q <= 1'b0;",
            "            guard_frame_owned_q <= 1'b0;",
            "            guard_gap_q <= 8'd11;",
            "          end",
            "        end else if (&guard_beat_wait_q ||",
            "                     ((protocol_fault_i == 4'd5) && &guard_beat_wait_q[7:0])) begin",
            "          guard_beat_pending_q <= 1'b0;",
            "          guard_frame_owned_q <= 1'b0;",
            "          guard_beat_timeouts_q <= guard_beat_timeouts_q + 32'd1;",
            "          guard_gap_q <= 8'd11;",
            "        end else begin",
            "          guard_beat_wait_q <= guard_beat_wait_q + 16'd1;",
            "        end",
            "      end else if (guard_response_pending_q) begin",
            "        // A legal verdict starts new ownership before its optional first beat",
            "        // is classified. Otherwise old-frame, pre-ownership, or post-denial",
            "        // quarantine owns every unsolicited beat through verdict resolution.",
            "        if (guard_verdict_ok_checked_c) begin",
            "          guard_accepts_q <= guard_accepts_q + 32'd1;",
            "          guard_response_pending_q <= 1'b0;",
            "          guard_verdict_extra_fault_delay_q <= '0;",
            "          guard_denial_watch_q <= 1'b0;",
            "          guard_preownership_watch_q <= 1'b0;",
            "          guard_denial_extra_fault_delay_q <= '0;",
            "          guard_verdict_wait_q <= '0;",
            "          guard_beat_wait_q <= '0;",
            "          if (guard_beat_valid_checked_c) begin",
            "            if (guard_beat_last_checked_c) begin",
            "              guard_early_last_errors_q <= guard_early_last_errors_q + 32'd1;",
            "              guard_beat_pending_q <= 1'b0;",
            "              guard_frame_owned_q <= 1'b0;",
            "              guard_gap_q <= 8'd11;",
            "            end else begin",
            "              guard_beat_pending_q <= 1'b1;",
            "              guard_frame_owned_q <= 1'b1;",
            "              guard_beat_count_q <= 4'd1;",
            "            end",
            "          end else begin",
            "            guard_beat_pending_q <= 1'b1;",
            "            guard_frame_owned_q <= 1'b1;",
            "            guard_beat_count_q <= '0;",
            "          end",
            "        end else begin",
            "          if ((guard_frame_owned_q || guard_denial_watch_q ||",
            "               (guard_preownership_watch_q &&",
            "                ((protocol_fault_i == 4'd0) ||",
            "                 (protocol_fault_i == 4'd15))) ||",
            "               guard_verdict_violation_checked_c) &&",
            "              guard_beat_valid_checked_c) begin",
            "            guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;",
            "            if ((protocol_fault_i == 4'd13) && guard_frame_owned_q &&",
            "                (guard_verdict_extra_fault_delay_q == 4'd1))",
            "              guard_verdict_extra_witnesses_q <=",
            "                  guard_verdict_extra_witnesses_q + 32'd1;",
            "            if ((protocol_fault_i == 4'd15) &&",
            "                guard_preownership_watch_q &&",
            "                !guard_verdict_violation_checked_c &&",
            "                !guard_preownership_fault_windows_q[2])",
            "              guard_preownership_fault_windows_q[2] <= 1'b1;",
            "            guard_verdict_extra_fault_delay_q <= '0;",
            "          end",
            "          if (guard_verdict_violation_checked_c) begin",
            "            guard_rejects_q <= guard_rejects_q + 32'd1;",
            "            guard_response_pending_q <= 1'b0;",
            "            guard_frame_owned_q <= 1'b0;",
            "            if ((protocol_fault_i == 4'd0) ||",
            "                (protocol_fault_i == 4'd14)) begin",
            "              guard_denial_watch_q <= 1'b1;",
            "              guard_denial_extra_fault_delay_q <= 4'd1;",
            "            end else begin",
            "              // Keep unrelated positive controls single-arm; fault hooks are",
            "              // simulation-only and synthesize at the normal zero value.",
            "              guard_denial_watch_q <= 1'b0;",
            "              guard_denial_extra_fault_delay_q <= '0;",
            "            end",
            "            guard_verdict_extra_fault_delay_q <= '0;",
            "            guard_verdict_wait_q <= '0;",
            "            guard_gap_q <= 8'd11;",
            "          end else if (&guard_verdict_wait_q ||",
            "                       ((protocol_fault_i == 4'd7) && &guard_verdict_wait_q[7:0])) begin",
            "            guard_response_pending_q <= 1'b0;",
            "            guard_frame_owned_q <= 1'b0;",
            "            guard_verdict_extra_fault_delay_q <= '0;",
            "            guard_verdict_timeouts_q <= guard_verdict_timeouts_q + 32'd1;",
            "            guard_gap_q <= 8'd11;",
            "          end else begin",
            "            guard_verdict_wait_q <= guard_verdict_wait_q + 16'd1;",
            "          end",
            "        end",
            "      end else begin",
            "        if ((guard_frame_owned_q || guard_denial_watch_q ||",
            "             (guard_preownership_watch_q &&",
            "              ((protocol_fault_i == 4'd0) ||",
            "               (protocol_fault_i == 4'd15)))) &&",
            "            guard_beat_valid_checked_c) begin",
            "          guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;",
            "          if ((protocol_fault_i == 4'd14) && guard_denial_watch_q &&",
            "              (guard_denial_extra_fault_delay_q == 4'd1))",
            "            guard_post_denial_extra_witnesses_q <=",
            "                guard_post_denial_extra_witnesses_q + 32'd1;",
            "          if ((protocol_fault_i == 4'd15) &&",
            "              guard_preownership_watch_q) begin",
            "            if (geom_guard_req_i.valid &&",
            "                !guard_preownership_fault_windows_q[1])",
            "              guard_preownership_fault_windows_q[1] <= 1'b1;",
            "            else if (!geom_guard_req_i.valid &&",
            "                     !guard_preownership_fault_windows_q[0])",
            "              guard_preownership_fault_windows_q[0] <= 1'b1;",
            "          end",
            "          guard_denial_extra_fault_delay_q <= '0;",
            "          guard_gap_q <= 8'd11;",
            "        end",
            "        if (geom_guard_req_i.valid && geom_guard_rsp_o_captured_i.ready) begin",
            "          geom_guard_req_i.valid <= 1'b0;",
            "          guard_response_pending_q <= 1'b1;",
            "          guard_extra_fault_delay_q <= '0;",
            "          guard_verdict_extra_fault_delay_q <=",
            "              guard_frame_owned_q ? 4'd1 : 4'd0;",
            "          guard_verdict_wait_q <= '0;",
            "        end else if (!geom_guard_req_i.valid) begin",
            "          if (guard_gap_q != 0) guard_gap_q <= guard_gap_q - 8'd1;",
            "          else begin",
            "            geom_guard_req_i.valid <= 1'b1;",
            "            guard_negative_q <= ~guard_negative_q;",
            "            if (!guard_negative_q) begin",
            "              geom_guard_req_i.write <= 1'b0;",
            "              geom_guard_req_i.client <= ZHAO_CLIENT_ENGINE1;",
            "              geom_guard_req_i.addr <= 27'h6a00000;",
            "              geom_guard_req_i.len <= 7'd64;",
            "              geom_guard_req_i.be <= 64'hffff_ffff_ffff_ffff;",
            "            end else begin",
            "              geom_guard_req_i.write <= lfsr_q[0];",
            "              geom_guard_req_i.client <= zhao_client_e'(lfsr_q[3:1]);",
            "              geom_guard_req_i.addr <= {1'b0, lfsr_q[25:6], 6'b0};",
            "              geom_guard_req_i.len <= {1'b0, lfsr_q[5:0]};",
            "              geom_guard_req_i.be <= lfsr_q;",
            "            end",
            "          end",
            "        end",
            "      end",
            "",
            "      // Compact SDR responder. Captured READ at R schedules first DQ at R+3.",
            "      if (sdr_read_beat_q != 0) begin",
            "        phy_dq_i <= sdr_row_q[15:0] ^ {3'b0, sdr_row_q[28:16]} ^ {12'h0, sdr_read_beat_q};",
            "        sdr_read_beat_q <= sdr_read_beat_q - 4'd1;",
            "      end else if (sdr_read_wait_q != 0) begin",
            "        sdr_read_wait_q <= sdr_read_wait_q - 4'd1;",
            "        if (sdr_read_wait_q == 4'd1) begin",
            "          phy_dq_i <= sdr_row_q[15:0] ^ {3'b0, sdr_row_q[28:16]};",
            "          sdr_read_beat_q <= 4'd7;",
            "          sdr_read_responses_q <= sdr_read_responses_q + 32'd1;",
            "        end",
            "      end else begin",
            "        phy_dq_i <= entropy_c[1888 +: 16];",
            "        if (!phy_cs_n_o_captured_i && phy_ras_n_o_captured_i &&",
            "            !phy_cas_n_o_captured_i && phy_we_n_o_captured_i) begin",
            "          sdr_row_q <= {phy_ba_o_captured_i, phy_a_o_captured_i, cycle_q[13:0]};",
            "          sdr_read_wait_q <= 4'd1;",
            "        end",
            "      end",
            "      // WRITE command and DQ data are distinct SDR phases. Hold the decoded",
            "      // command identity until output-enable proves the delayed data phase.",
            "      if (sdr_dq_oe_checked_c && !sdr_write_active_q)",
            "        sdr_write_phase_errors_q <= sdr_write_phase_errors_q + 32'd1;",
            "      if (!phy_cs_n_o_captured_i && phy_ras_n_o_captured_i &&",
            "          !phy_cas_n_o_captured_i && !phy_we_n_o_captured_i &&",
            "          protocol_fault_i != 4'd6) begin",
            "        sdr_write_pending_q <= 1'b1;",
            "        sdr_write_active_q <= 1'b1;",
            "        sdr_write_command_q <= {phy_ba_o_captured_i, phy_a_o_captured_i};",
            "      end",
            "      if (sdr_write_active_q && phy_dq_oe_o_captured_i) begin",
            "        if (sdr_write_pending_q) begin",
            "          sdr_write_transactions_q <= sdr_write_transactions_q + 32'd1;",
            "          sdr_write_pending_q <= 1'b0;",
            "        end",
            "        sdr_write_observations_q <= {sdr_write_observations_q[30:0],",
            "                                     sdr_write_observations_q[31]}",
            "                                  ^ {17'd0, sdr_write_command_q}",
            "                                  ^ {14'd0, phy_dqm_o_captured_i,",
            "                                     phy_dq_o_captured_i};",
            "      end else if (sdr_write_active_q && !sdr_write_pending_q) begin",
            "        sdr_write_active_q <= 1'b0;",
            "      end",
            "    end",
            "  end",
            "endmodule",
        ]
    )
    return "\n".join(lines)


def _render_top(
    declaration: ModuleDeclaration,
    policy: ShellPolicy,
    packet: bytes,
    packet_path: str,
    hashes: Mapping[str, str],
) -> tuple[str, dict[str, object]]:
    rows = policy_by_name(policy)
    offsets, domain_totals = _output_offsets(declaration, policy)
    feedback_ports = [
        "ring_wr_valid_o",
        "ring_wr_slot_o",
        "ring_wr_state_o",
        "hps_req_valid_o",
        "hps_req_write_o",
        "hps_req_addr_o",
        "hps_req_len_o",
        "hps_wr_valid_o",
        "hps_wr_last_o",
        "blit_done_o",
        "blit_status_o",
        "aud_wr_ready_o",
        "cnt_snap_valid_o",
        "cnt_snap_id_o",
        "init_done_o",
        "render_tri_ready_o",
        "render_drained_o",
        "render_issued_words_o",
        "render_retired_words_o",
        "geom_guard_rsp_o",
        "geom_beat_valid_o",
        "geom_beat_last_o",
        "phy_cs_n_o",
        "phy_ras_n_o",
        "phy_cas_n_o",
        "phy_we_n_o",
        "phy_a_o",
        "phy_ba_o",
        "phy_dq_o",
        "phy_dq_oe_o",
        "phy_dqm_o",
    ]
    output_names = {port.name for port in declaration.ports if port.direction == "output"}
    unknown_feedback = [name for name in feedback_ports if name not in output_names]
    if unknown_feedback:
        raise ShellPortError(f"generator feedback ports absent from shell: {unknown_feedback}")

    lines = [
        "// GENERATED FILE -- DO NOT EDIT.",
        "// Generator: tools/quartus/gen_shell_fit_top.py",
        f"// shell-declaration-sha256: {hashes['shell_declaration']}",
        f"// policy-sha256: {hashes['policy']}",
        f"// generator-sha256: {hashes['generator']}",
        f"// parser-sha256: {hashes['parser']}",
        f"// packet-rom-sha256: {hashes['packet']}",
        "// Traffic is deterministic legal-ish characterization stimulus, not an HPS/SDRAM model.",
        "",
        "module zhao_shell_fit_top",
        "  import zhao_pkg::*, zhao_abi_pkg::*;",
        "(",
        "  input  logic       gpu_clk,",
        "  input  logic       vid_clk,",
        "  input  logic       audio_clk,",
        "  input  logic       rst_n,",
        "  output logic [2:0] fit_signature_o,",
        "  output logic [2:0] fit_epoch_o",
        ");",
        "",
        "  // Exact shell boundary aliases. Inputs are registered in u_stimulus;",
        "  // outputs feed native-domain capture banks for signatures and accounting.",
        "  // Renderer ready additionally feeds its same-edge ready/valid decision directly.",
    ]
    for port in declaration.ports:
        if port.name in {"gpu_clk", "vid_clk", "audio_clk", "rst_n"}:
            continue
        lines.append("  " + _signal_declaration(port, f"shell_{port.name}", attribute='keep = "true"'))
    lines.append("")
    for domain in ("gpu", "video", "audio"):
        width = domain_totals[domain]
        lines.append(f"  logic [{width - 1}:0] {domain}_payload_c;")
        if domain == "gpu":
            lines.extend(
                [
                    "  /* verilator lint_off UNUSEDSIGNAL */",
                    f"  logic [{width - 1}:0] {domain}_capture_bus;",
                    "  /* verilator lint_on UNUSEDSIGNAL */",
                ]
            )
        lines.extend(
            [
                f"  always_comb begin",
                f"    {domain}_payload_c = '0;",
            ]
        )
        for port in declaration.ports:
            if port.direction != "output" or rows[port.name].domain != domain:
                continue
            base = offsets[port.name]
            for suffix, element_offset, width_element in flatten_port_elements(port):
                lines.append(
                    f"    {domain}_payload_c[{base + element_offset} +: {width_element}] "
                    f"= shell_{port.name}{suffix};"
                )
        lines.extend(["  end", ""])

    lines.extend(["  zhao_shell_fit_stimulus u_stimulus ("])
    stimulus_connections = [".gpu_clk(gpu_clk)", ".rst_n(rst_n)"]
    for name in feedback_ports:
        port = next(port for port in declaration.ports if port.name == name)
        if name == "render_tri_ready_o":
            feedback_name = f"{name}_native_i"
            expression = f"shell_{name}"
        else:
            feedback_name = f"{name}_captured_i"
            domain = rows[name].domain
            expression = _capture_expression(port, domain, offsets[name])
        stimulus_connections.append(f".{feedback_name}({expression})")
    for port in declaration.ports:
        if port.direction == "input" and rows[port.name].driver != "top_port":
            stimulus_connections.append(f".{port.name}(shell_{port.name})")
    for index, connection in enumerate(stimulus_connections):
        comma = "," if index + 1 < len(stimulus_connections) else ""
        lines.append(f"    {connection}{comma}")
    lines.extend(["  );", ""])

    shell_connections: list[str] = []
    for port in declaration.ports:
        signal = port.name if port.name in {"gpu_clk", "vid_clk", "audio_clk", "rst_n"} else f"shell_{port.name}"
        shell_connections.append(f".{port.name}({signal})")
    lines.append("  zhao_shell_top u_shell (")
    for index, connection in enumerate(shell_connections):
        lines.append(f"    {connection}{',' if index + 1 < len(shell_connections) else ''}")
    lines.extend(["  );", ""])

    seeds = {"gpu": 0xD1B54A32D192ED03, "video": 0x94D049BB133111EB, "audio": 0xBF58476D1CE4E5B9}
    clock_names = {"gpu": "gpu_clk", "video": "vid_clk", "audio": "audio_clk"}
    for index, domain in enumerate(("gpu", "video", "audio")):
        sink_connections = [
            f".clk({clock_names[domain]})",
            ".rst_n(rst_n)",
            f".payload_i({domain}_payload_c)",
        ]
        if domain == "gpu":
            sink_connections.append(f".capture_o({domain}_capture_bus)")
        sink_connections.extend(
            [
                f".signature_o(fit_signature_o[{index}])",
                f".epoch_o(fit_epoch_o[{index}])",
            ]
        )
        lines.append(f"  zhao_shell_fit_{domain}_sink u_{domain}_sink (")
        for connection_index, connection in enumerate(sink_connections):
            comma = "," if connection_index + 1 < len(sink_connections) else ""
            lines.append(f"    {connection}{comma}")
        lines.extend(["  );", ""])
    lines.append("endmodule")
    lines.append("")
    lines.append("/* verilator lint_off DECLFILENAME */")
    lines.append(_render_stimulus(declaration, policy, packet, feedback_ports))
    lines.append("")
    for domain in ("gpu", "video", "audio"):
        lines.append(
            _render_sink(
                domain,
                domain_totals[domain],
                seeds[domain],
                expose_capture=(domain == "gpu"),
            )
        )
        lines.append("")
    lines.append("/* verilator lint_on DECLFILENAME */")

    manifest_rows: list[dict[str, object]] = []
    for port in declaration.ports:
        row = rows[port.name]
        generated_signal = (
            port.name if row.driver == "top_port" else f"shell_{port.name}"
        )
        manifest_rows.append(
            {
                "ordinal": port.ordinal,
                "name": port.name,
                "direction": port.direction,
                "type_text": port.type_text,
                "signal_type_text": port.signal_type_text,
                "packed_dimensions": [asdict(dimension) for dimension in port.packed_dimensions],
                "unpacked_dimensions": [asdict(dimension) for dimension in port.unpacked_dimensions],
                "signed": port.signed,
                "element_width": port.element_width,
                "bit_width": port.bit_width,
                "domain": row.domain,
                "driver": row.driver,
                "sink": row.sink,
                "dynamic_mask": _format_mask(row.dynamic_mask, port.bit_width),
                "constant_reason": row.constant_reason,
                "signature_offset": offsets.get(port.name),
                "generated_signal": generated_signal,
                "connection": f".{port.name}({generated_signal})",
                "registered_driver": (
                    f"u_stimulus.{port.name}"
                    if port.direction == "input" and row.driver != "top_port"
                    else None
                ),
                "capture_register": (
                    None
                    if port.direction != "output"
                    else f"u_{row.domain}_sink.capture_q[{offsets[port.name]} +: {port.bit_width}]"
                ),
            }
        )
    manifest: dict[str, object] = {
        "schema_version": GENERATOR_SCHEMA,
        "module": "zhao_shell_fit_top",
        "shell_module": declaration.module_name,
        "shell_instance": "u_shell",
        "traffic_profile": policy.traffic_profile,
        "hierarchy": {
            "top": {"module": "zhao_shell_fit_top", "instance": None},
            "required_children": [
                {"module": "zhao_shell_top", "instance": "u_shell", "role": "shell"},
                {
                    "module": "zhao_shell_fit_stimulus",
                    "instance": "u_stimulus",
                    "role": "stimulus",
                },
                {
                    "module": "zhao_shell_fit_gpu_sink",
                    "instance": "u_gpu_sink",
                    "role": "gpu_sink",
                },
                {
                    "module": "zhao_shell_fit_video_sink",
                    "instance": "u_video_sink",
                    "role": "video_sink",
                },
                {
                    "module": "zhao_shell_fit_audio_sink",
                    "instance": "u_audio_sink",
                    "role": "audio_sink",
                },
            ],
            "remainder_attribution": "reported top row only; never inferred by subtraction",
        },
        "hashes": dict(hashes),
        "external_ports": [
            {"name": "gpu_clk", "direction": "input", "bits": 1, "domain_index": 0,
             "element_width": 1, "packed_dimensions": [], "unpacked_dimensions": [], "signed": False},
            {"name": "vid_clk", "direction": "input", "bits": 1, "domain_index": 1,
             "element_width": 1, "packed_dimensions": [], "unpacked_dimensions": [], "signed": False},
            {"name": "audio_clk", "direction": "input", "bits": 1, "domain_index": 2,
             "element_width": 1, "packed_dimensions": [], "unpacked_dimensions": [], "signed": False},
            {"name": "rst_n", "direction": "input", "bits": 1, "domain_index": None,
             "element_width": 1, "packed_dimensions": [], "unpacked_dimensions": [], "signed": False},
            {"name": "fit_signature_o", "direction": "output", "bits": 3, "domain_index": None,
             "element_width": 3,
             "packed_dimensions": [{"text": "[2:0]", "left": 2, "right": 0}],
             "unpacked_dimensions": [], "signed": False},
            {"name": "fit_epoch_o", "direction": "output", "bits": 3, "domain_index": None,
             "element_width": 3,
             "packed_dimensions": [{"text": "[2:0]", "left": 2, "right": 0}],
             "unpacked_dimensions": [], "signed": False},
        ],
        "accounting": {
            "port_count": len(declaration.ports),
            "input_ports": sum(port.direction == "input" for port in declaration.ports),
            "output_ports": sum(port.direction == "output" for port in declaration.ports),
            "input_bits": declaration.input_bits,
            "output_bits": declaration.output_bits,
            "total_bits": declaration.total_bits,
            "non_clock_reset_input_bits": declaration.input_bits - 4,
            "domain_output_bits": domain_totals,
            "domain_chunks": {
                domain: (bits + 31) // 32 for domain, bits in domain_totals.items()
            },
        },
        "misr": {
            "width": 32,
            "polynomial": "0x00400007",
            "initial": "0x00000000",
            "chunk_order": "declaration, declared unpacked index, packed LSB first",
            "serializer": "snapshot LSB first; epoch toggles with snapshot bit 0",
            "activity_lfsr_seeds": {
                domain: f"0x{seed:016x}" for domain, seed in seeds.items()
            },
            "cross_domain_payload_reduction": False,
        },
        "packet_rom": {
            "path": packet_path.replace("\\", "/"),
            "bytes": len(packet),
            "description": "committed minimal directed frame packet",
        },
        "limitations": [
            "Deterministic legal-ish characterization traffic, not a functional HPS model.",
            "Command-aware compact SDR response, not an SDRAM storage array.",
            "Directed render and guard transactions prove activity, not rendered correctness.",
            "Packet A has not run Quartus and makes no fit, pin, timing, or area claim.",
        ],
        "ports": manifest_rows,
    }
    return "\n".join(lines).rstrip() + "\n", manifest


def render_artifacts(
    *,
    shell_bytes: bytes,
    package_bytes: bytes,
    policy_bytes: bytes,
    packet_bytes: bytes,
    packet_path: str,
    generator_bytes: bytes,
    parser_bytes: bytes,
) -> RenderedArtifacts:
    try:
        shell_text = shell_bytes.decode("utf-8")
        package_text = package_bytes.decode("utf-8")
        policy_text = policy_bytes.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"input is not UTF-8: {exc}") from exc
    type_widths = discover_type_widths(package_text)
    type_signedness = discover_type_signedness(package_text)
    declaration = parse_module_declaration(
        shell_text,
        "zhao_shell_top",
        type_widths=type_widths,
        type_signedness=type_signedness,
    )
    policy = load_policy_text(policy_text)
    validate_policy(declaration, policy)
    validate_handler_ownership(declaration, policy)
    if policy.traffic_profile != TRAFFIC_PROFILE:
        raise ShellPortError(
            f"unsupported traffic profile {policy.traffic_profile!r}, expected {TRAFFIC_PROFILE!r}"
        )
    hashes = {
        "shell_declaration": declaration.declaration_sha256,
        "shell_file": _hash(shell_bytes),
        "package_file": _hash(package_bytes),
        "policy": _hash(policy_bytes),
        "generator": _hash(generator_bytes),
        "parser": _hash(parser_bytes),
        "packet": _hash(packet_bytes),
    }
    rtl_text, manifest = _render_top(
        declaration, policy, packet_bytes, packet_path, hashes
    )
    rtl = rtl_text.encode("utf-8")
    manifest["generated_rtl_sha256"] = _hash(rtl)
    manifest_bytes = (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")

    parser_names = [port.name for port in declaration.ports]
    policy_names = [row.name for row in policy.ports]
    connection_names = [row["name"] for row in manifest["ports"]]
    sink_names = [
        row["name"]
        for row in manifest["ports"]
        if row["direction"] == "output" and row["capture_register"] is not None
    ]
    assert_exact_port_sets(
        {
            "parser": parser_names,
            "policy": policy_names,
            "manifest-connections": connection_names,
        }
    )
    expected_outputs = [port.name for port in declaration.ports if port.direction == "output"]
    assert_exact_port_sets({"parser-outputs": expected_outputs, "manifest-sinks": sink_names})
    return RenderedArtifacts(rtl=rtl, manifest=manifest_bytes)


def _atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(descriptor, "wb") as handle:
            handle.write(data)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    except BaseException:
        try:
            temporary.unlink(missing_ok=True)
        finally:
            raise


def check_artifacts(expected: Mapping[Path, bytes]) -> list[str]:
    stale: list[str] = []
    for path, data in expected.items():
        try:
            actual = path.read_bytes()
        except FileNotFoundError:
            stale.append(f"missing generated artifact: {path}")
            continue
        if actual != data:
            stale.append(f"stale generated artifact: {path}")
    return stale


def _resolve(repo: Path, path: Path) -> Path:
    return path if path.is_absolute() else repo / path


def _provenance_path(repo: Path, path: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(repo.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--check", action="store_true", help="compare committed bytes; never write")
    action.add_argument("--write", action="store_true", help="atomically replace generated artifacts")
    parser.add_argument("--repo-root", type=Path, default=DEFAULT_REPO)
    parser.add_argument("--shell", type=Path, default=DEFAULT_SHELL)
    parser.add_argument("--package", type=Path, default=DEFAULT_PACKAGE)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY)
    parser.add_argument("--packet", type=Path, default=DEFAULT_PACKET)
    parser.add_argument("--rtl-out", type=Path, default=DEFAULT_RTL)
    parser.add_argument("--manifest-out", type=Path, default=DEFAULT_MANIFEST)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo = args.repo_root.resolve()
    shell = _resolve(repo, args.shell)
    package = _resolve(repo, args.package)
    policy = _resolve(repo, args.policy)
    packet = _resolve(repo, args.packet)
    rtl_out = _resolve(repo, args.rtl_out)
    manifest_out = _resolve(repo, args.manifest_out)
    try:
        artifacts = render_artifacts(
            shell_bytes=shell.read_bytes(),
            package_bytes=package.read_bytes(),
            policy_bytes=policy.read_bytes(),
            packet_bytes=packet.read_bytes(),
            packet_path=_provenance_path(repo, packet),
            generator_bytes=SCRIPT.read_bytes(),
            parser_bytes=PARSER_SCRIPT.read_bytes(),
        )
        expected = {rtl_out: artifacts.rtl, manifest_out: artifacts.manifest}
        if args.check:
            stale = check_artifacts(expected)
            if stale:
                for diagnostic in stale:
                    print(f"shell-fit-generate: {diagnostic}", file=sys.stderr)
                return 1
            print("shell-fit-generate: generated RTL and manifest are current")
            return 0
        for path, data in expected.items():
            _atomic_write(path, data)
        print(f"shell-fit-generate: wrote {rtl_out} and {manifest_out}")
        return 0
    except (OSError, ShellPortError) as exc:
        print(f"shell-fit-generate: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
