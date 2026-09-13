#!/usr/bin/env python3
"""Render the manifest-driven shell-fit smoke monitor.

The committed monitor is simulation-only and has no timing controls. CMake's
established ``verilate()`` integration compiles it with the full shell source
pool; a C++ driver supplies the three clocks and the frozen-domain controls.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
from typing import Mapping, Sequence

from shell_ports import (
    ModuleDeclaration,
    ShellPolicy,
    ShellPortError,
    discover_type_widths,
    flatten_port_elements,
    load_policy_text,
    parse_module_declaration,
    policy_by_name,
    validate_policy,
)


SCRIPT = Path(__file__).resolve()
DEFAULT_REPO = SCRIPT.parents[2]
DEFAULT_MANIFEST = Path("fpga/rtl/generated/zhao_shell_fit_top.manifest.json")
DEFAULT_RTL = Path("fpga/rtl/generated/zhao_shell_fit_top.sv")
DEFAULT_SHELL = Path("fpga/rtl/common/zhao_shell_top.sv")
DEFAULT_PACKAGE = Path("fpga/rtl/common/zhao_pkg.sv")
DEFAULT_POLICY = Path("design/shell_fit_ports.yml")
DEFAULT_GENERATOR = Path("tools/quartus/gen_shell_fit_top.py")
DEFAULT_PARSER = Path("tools/quartus/shell_ports.py")
DEFAULT_OUTPUT = Path("tests/shell/generated/zhao_shell_fit_smoke_tb.sv")
DOMAINS = ("gpu", "video", "audio")
DEAD_REASON_COUNT = 8


def _hex_literal(width: int, value: int) -> str:
    return f"{width}'h{value:0{(width + 3) // 4}x}"


def _load_manifest(text: str) -> Mapping[str, object]:
    try:
        payload = json.loads(text)
    except json.JSONDecodeError as exc:
        raise ShellPortError(f"shell-fit manifest is invalid JSON: {exc}") from exc
    if not isinstance(payload, dict):
        raise ShellPortError("shell-fit manifest root must be an object")
    return payload


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _read_utf8_exact(path: Path) -> str:
    """Decode without universal-newline or locale-dependent transformations."""
    try:
        return path.read_bytes().decode("utf-8")
    except UnicodeDecodeError as exc:
        raise ShellPortError(f"{path} is not UTF-8: {exc}") from exc


def _require_manifest_freshness(
    *,
    repo: Path,
    manifest: Mapping[str, object],
    declaration: ModuleDeclaration,
    policy: ShellPolicy,
) -> None:
    hashes = manifest.get("hashes")
    packet_rom = manifest.get("packet_rom")
    if not isinstance(hashes, dict) or not isinstance(packet_rom, dict):
        raise ShellPortError("manifest is missing hashes or packet_rom provenance")
    packet_raw = packet_rom.get("path")
    if not isinstance(packet_raw, str) or not packet_raw:
        raise ShellPortError("manifest packet_rom.path is absent")
    packet_path = Path(packet_raw)
    if not packet_path.is_absolute():
        packet_path = repo / packet_path
    expected = {
        "shell_declaration": declaration.declaration_sha256,
        "shell_file": _sha256(repo / DEFAULT_SHELL),
        "package_file": _sha256(repo / DEFAULT_PACKAGE),
        "policy": _sha256(repo / DEFAULT_POLICY),
        "generator": _sha256(repo / DEFAULT_GENERATOR),
        "parser": _sha256(repo / DEFAULT_PARSER),
        "packet": _sha256(packet_path),
    }
    stale = [
        f"{name}: manifest={hashes.get(name)!r}, current={value!r}"
        for name, value in expected.items()
        if hashes.get(name) != value
    ]
    rtl_hash = _sha256(repo / DEFAULT_RTL)
    if manifest.get("generated_rtl_sha256") != rtl_hash:
        stale.append(
            "generated_rtl_sha256: "
            f"manifest={manifest.get('generated_rtl_sha256')!r}, current={rtl_hash!r}"
        )
    if manifest.get("traffic_profile") != policy.traffic_profile:
        stale.append(
            "traffic_profile: "
            f"manifest={manifest.get('traffic_profile')!r}, current={policy.traffic_profile!r}"
        )
    if stale:
        raise ShellPortError("stale mixed-generation manifest: " + "; ".join(stale))


def _activity_layout(
    declaration: ModuleDeclaration, policy: ShellPolicy
) -> tuple[list[tuple[object, object, int]], int]:
    rows = policy_by_name(policy)
    layout: list[tuple[object, object, int]] = []
    cursor = 0
    for port in declaration.ports:
        row = rows[port.name]
        if port.direction == "input" and row.driver != "top_port":
            layout.append((port, row, cursor))
            cursor += port.bit_width
    return layout, cursor


def _output_activity_layout(
    declaration: ModuleDeclaration,
    policy: ShellPolicy,
    manifest: Mapping[str, object],
) -> list[tuple[object, object, int]]:
    rows = policy_by_name(policy)
    manifest_rows = manifest.get("ports")
    if not isinstance(manifest_rows, list):
        raise ShellPortError("manifest ports must be an array")
    manifest_by_name = {
        row.get("name"): row for row in manifest_rows if isinstance(row, dict)
    }
    layout: list[tuple[object, object, int]] = []
    for port in declaration.ports:
        if port.direction != "output":
            continue
        row = rows[port.name]
        manifest_row = manifest_by_name.get(port.name)
        if not isinstance(manifest_row, dict):
            raise ShellPortError(f"manifest is missing output row {port.name!r}")
        offset = manifest_row.get("signature_offset")
        if not isinstance(offset, int):
            raise ShellPortError(
                f"manifest output {port.name!r} has invalid signature_offset {offset!r}"
            )
        layout.append((port, row, offset))
    return layout


def render_monitor(
    *,
    manifest: Mapping[str, object],
    declaration: ModuleDeclaration,
    policy: ShellPolicy,
) -> bytes:
    accounting = manifest.get("accounting")
    misr = manifest.get("misr")
    if not isinstance(accounting, dict) or not isinstance(misr, dict):
        raise ShellPortError("manifest is missing accounting or misr object")
    domain_bits = accounting.get("domain_output_bits")
    seeds = misr.get("activity_lfsr_seeds")
    if not isinstance(domain_bits, dict) or not isinstance(seeds, dict):
        raise ShellPortError("manifest is missing domain bits or activity LFSR seeds")
    layout, input_bits = _activity_layout(declaration, policy)
    output_layout = _output_activity_layout(declaration, policy, manifest)
    if input_bits != accounting.get("non_clock_reset_input_bits"):
        raise ShellPortError(
            f"activity layout has {input_bits} bits, manifest has "
            f"{accounting.get('non_clock_reset_input_bits')!r}"
        )

    lines = [
        "// Generated by tools/quartus/shell_fit_smoke.py --write.",
        "// Simulation-only manifest-driven non-vacuity monitor.",
        "module zhao_shell_fit_smoke_tb (",
        "  input  logic       gpu_clk,",
        "  input  logic       vid_clk,",
        "  input  logic       audio_clk,",
        "  input  logic       rst_n,",
        "  input  logic       check_i,",
        "  input  logic [1:0] control_domain_i, // 3 means baseline",
        "  input  logic [3:0] control_arm_i,    // 0 means no detector fault",
        "  input  logic [3:0] protocol_fault_i, // 0 means no protocol fault",
        "  output logic       baseline_failed_o,",
        "  output logic [2:0] dead_domains_o,",
        f"  output logic [{DEAD_REASON_COUNT * len(DOMAINS) - 1}:0] dead_reasons_o,",
        "  output logic       control_group_failure_o,",
        "  output logic [11:0] protocol_failures_o,",
        "  output logic       protocol_failure_o",
        ");",
        "  logic [2:0] fit_signature;",
        "  logic [2:0] fit_epoch;",
        "  zhao_shell_fit_top dut (",
        "    .gpu_clk(gpu_clk),",
        "    .vid_clk(vid_clk),",
        "    .audio_clk(audio_clk),",
        "    .rst_n(rst_n),",
        "    .fit_signature_o(fit_signature),",
        "    .fit_epoch_o(fit_epoch)",
        "  );",
        "  // Drive the wrapper's simulation-only, safe-zero internal fault hook.",
        "  always_comb dut.u_stimulus.protocol_fault_i = protocol_fault_i;",
        "",
    ]
    clock_names = {"gpu": "gpu_clk", "video": "vid_clk", "audio": "audio_clk"}
    for index, domain in enumerate(DOMAINS):
        bits = int(domain_bits[domain])
        seed = int(str(seeds[domain]), 16)
        lines.extend(
            [
                f"  logic [{bits - 1}:0] observed_{domain}_payload;",
                f"  logic observed_{domain}_signature;",
                f"  logic observed_{domain}_epoch;",
                f"  logic [{bits - 1}:0] zero_{domain}_payload;",
                f"  logic zero_{domain}_signature;",
                f"  logic zero_{domain}_epoch;",
                f"  logic {domain}_fault_selected_c;",
                f"  logic [63:0] {domain}_activity_observed_c;",
                f"  logic [31:0] {domain}_wraps_observed_c;",
            ]
        )
        if domain == "gpu":
            lines.extend(
                [
                    f"  logic [{bits - 1}:0] observed_gpu_capture_unused;",
                    f"  logic [{bits - 1}:0] zero_gpu_capture_unused;",
                ]
            )
        observed_connections = [
            f".clk({clock_names[domain]})",
            ".rst_n(rst_n)",
            f".payload_i(observed_{domain}_payload)",
        ]
        zero_connections = [
            f".clk({clock_names[domain]})",
            ".rst_n(rst_n)",
            f".payload_i(zero_{domain}_payload)",
        ]
        if domain == "gpu":
            observed_connections.append(".capture_o(observed_gpu_capture_unused)")
            zero_connections.append(".capture_o(zero_gpu_capture_unused)")
        observed_connections.extend(
            [
                f".signature_o(observed_{domain}_signature)",
                f".epoch_o(observed_{domain}_epoch)",
            ]
        )
        zero_connections.extend(
            [
                f".signature_o(zero_{domain}_signature)",
                f".epoch_o(zero_{domain}_epoch)",
            ]
        )
        lines.extend(
            [
                f"  assign {domain}_fault_selected_c = (control_domain_i == 2'd{index});",
                f"  assign {domain}_activity_observed_c =",
                f"      ({domain}_fault_selected_c && control_arm_i == 4'd2)",
                f"        ? 64'h{seed:016x} : u_observed_{domain}.activity_lfsr_q;",
                f"  assign {domain}_wraps_observed_c =",
                f"      ({domain}_fault_selected_c && control_arm_i == 4'd3)",
                f"        ? 32'd0 : u_observed_{domain}.chunk_wraps_q;",
                f"  assign observed_{domain}_payload =",
                f"      (control_domain_i == 2'd{index} && control_arm_i == 4'd9)",
                f"        ? '0 : dut.{domain}_payload_c;",
                f"  zhao_shell_fit_{domain}_sink u_observed_{domain} (",
                "    " + ",\n    ".join(observed_connections),
                "  );",
                f"  assign zero_{domain}_payload = '0;",
                f"  zhao_shell_fit_{domain}_sink u_zero_{domain} (",
                "    " + ",\n    ".join(zero_connections),
                "  );",
                "",
            ]
        )

    lines.extend(
        [
            f"  logic [{input_bits - 1}:0] input_payload_c;",
            f"  logic [{input_bits - 1}:0] input_previous_q;",
            f"  logic [{input_bits - 1}:0] input_changed_q;",
            "  logic input_sample_started_q;",
            "  always_comb begin",
            "    input_payload_c = '0;",
        ]
    )
    for port, _row, offset in layout:
        for suffix, element_offset, element_width in flatten_port_elements(port):
            lines.append(
                f"    input_payload_c[{offset + element_offset} +: {element_width}] = "
                f"dut.shell_{port.name}{suffix};"
            )
    lines.extend(
        [
            "  end",
            "  always_ff @(posedge gpu_clk or negedge rst_n) begin",
            "    if (!rst_n) begin",
            "      input_previous_q <= '0;",
            "      input_changed_q <= '0;",
            "      input_sample_started_q <= 1'b0;",
            "    end else if (dut.u_stimulus.run_c) begin",
            "      if (input_sample_started_q)",
            "        input_changed_q <= input_changed_q | (input_payload_c ^ input_previous_q);",
            "      input_previous_q <= input_payload_c;",
            "      input_sample_started_q <= 1'b1;",
            "    end",
            "  end",
            "",
        ]
    )

    for index, domain in enumerate(DOMAINS):
        bits = int(domain_bits[domain])
        lines.extend(
            [
                f"  logic [31:0] {domain}_edges_q;",
                f"  logic [31:0] {domain}_epoch_toggles_q;",
                f"  logic {domain}_epoch_previous_q;",
                f"  logic {domain}_misr_nonzero_seen_q;",
                f"  logic {domain}_snapshot_nonzero_seen_q;",
                f"  logic {domain}_signature_one_seen_q;",
                f"  logic {domain}_zero_signature_nonzero_q;",
                f"  logic [{bits - 1}:0] {domain}_capture_previous_q;",
                f"  logic [{bits - 1}:0] {domain}_capture_changed_q;",
                f"  logic {domain}_capture_started_q;",
                f"  always_ff @(posedge {clock_names[domain]} or negedge rst_n) begin",
                "    if (!rst_n) begin",
                f"      {domain}_edges_q <= '0;",
                f"      {domain}_epoch_toggles_q <= '0;",
                f"      {domain}_epoch_previous_q <= 1'b0;",
                f"      {domain}_misr_nonzero_seen_q <= 1'b0;",
                f"      {domain}_snapshot_nonzero_seen_q <= 1'b0;",
                f"      {domain}_signature_one_seen_q <= 1'b0;",
                f"      {domain}_zero_signature_nonzero_q <= 1'b0;",
                f"      {domain}_capture_previous_q <= '0;",
                f"      {domain}_capture_changed_q <= '0;",
                f"      {domain}_capture_started_q <= 1'b0;",
                "    end else begin",
                f"      if (!({domain}_fault_selected_c && control_arm_i == 4'd1))",
                f"        {domain}_edges_q <= {domain}_edges_q + 32'd1;",
                f"      if (observed_{domain}_epoch != {domain}_epoch_previous_q &&",
                f"          !({domain}_fault_selected_c && control_arm_i == 4'd6))",
                f"        {domain}_epoch_toggles_q <= {domain}_epoch_toggles_q + 32'd1;",
                f"      {domain}_epoch_previous_q <= observed_{domain}_epoch;",
                f"      if (u_observed_{domain}.misr_q != 0 &&",
                f"          !({domain}_fault_selected_c && control_arm_i == 4'd4))",
                f"        {domain}_misr_nonzero_seen_q <= 1'b1;",
                f"      if (u_observed_{domain}.snapshot_q != 0 &&",
                f"          !({domain}_fault_selected_c && control_arm_i == 4'd5))",
                f"        {domain}_snapshot_nonzero_seen_q <= 1'b1;",
                f"      if (observed_{domain}_signature &&",
                f"          !({domain}_fault_selected_c && control_arm_i == 4'd8))",
                f"        {domain}_signature_one_seen_q <= 1'b1;",
                f"      if (zero_{domain}_signature) {domain}_zero_signature_nonzero_q <= 1'b1;",
                f"      if ({domain}_capture_started_q &&",
                f"          !({domain}_fault_selected_c && control_arm_i == 4'd7))",
                f"        {domain}_capture_changed_q <= {domain}_capture_changed_q | "
                f"(u_observed_{domain}.capture_q ^ {domain}_capture_previous_q);",
                f"      {domain}_capture_previous_q <= u_observed_{domain}.capture_q;",
                f"      {domain}_capture_started_q <= 1'b1;",
                "    end",
                "  end",
                "",
            ]
        )

    lines.extend(
        [
            "  logic input_failure_c;",
            "  logic output_failure_c;",
            "  logic event_failure_c;",
            "  logic zero_reference_failure_c;",
            "  logic sink_mismatch_c;",
            "  always_comb begin",
            "    dead_domains_o = '0;",
            "    dead_reasons_o = '0;",
            "    control_group_failure_o = 1'b0;",
        ]
    )
    for index, domain in enumerate(DOMAINS):
        seed = int(str(seeds[domain]), 16)
        base = index * DEAD_REASON_COUNT
        reasons = (
            f"({domain}_edges_q == 0)",
            f"({domain}_activity_observed_c == 64'h{seed:016x})",
            f"({domain}_wraps_observed_c == 0)",
            f"(!{domain}_misr_nonzero_seen_q)",
            f"(!{domain}_snapshot_nonzero_seen_q)",
            f"({domain}_epoch_toggles_q == 0)",
            f"({domain}_capture_changed_q == '0)",
            f"(!{domain}_signature_one_seen_q)",
        )
        for reason_index, expression in enumerate(reasons):
            lines.append(f"    dead_reasons_o[{base + reason_index}] = {expression};")
        lines.append(
            f"    dead_domains_o[{index}] = |dead_reasons_o[{base} +: {DEAD_REASON_COUNT}];"
        )
    lines.extend(["    input_failure_c = 1'b0;"])
    for port, row, offset in layout:
        mask = int(row.dynamic_mask or 0)
        lines.extend(
            [
                f"    if (input_changed_q[{offset} +: {port.bit_width}] != "
                f"{_hex_literal(port.bit_width, mask)})",
                "      input_failure_c = 1'b1;",
            ]
        )
    lines.append("    output_failure_c = 1'b0;")
    domain_indexes = {domain: index for index, domain in enumerate(DOMAINS)}
    for port, row, offset in output_layout:
        mask = int(row.dynamic_mask or 0)
        domain = row.domain
        domain_index = domain_indexes[domain]
        mask_enabled = "1'b1" if mask != 0 else "1'b0"
        failed = (
            f"({domain}_capture_changed_q[{offset} +: {port.bit_width}] != "
            f"{_hex_literal(port.bit_width, mask)})"
        )
        lines.extend(
            [
                f"    if ({failed}) output_failure_c = 1'b1;",
                f"    if (control_domain_i == 2'd{domain_index} && "
                f"control_arm_i == 4'd9 && {mask_enabled}) begin",
                f"      if ({failed}) control_group_failure_o = 1'b1;",
                "    end",
            ]
        )
    lines.extend(
        [
            "    sink_mismatch_c = 1'b0;",
        ]
    )
    for index, domain in enumerate(DOMAINS):
        lines.extend(
            [
                f"    if (!(control_domain_i == 2'd{index} && control_arm_i == 4'd9) &&",
                f"        (observed_{domain}_signature != fit_signature[{index}] ||",
                f"         observed_{domain}_epoch != fit_epoch[{index}]))",
                "      sink_mismatch_c = 1'b1;",
            ]
        )
    event_checks = (
        ("dut.u_stimulus.ring_transactions_q == 0", "ring transaction"),
        ("dut.u_stimulus.ring_done_posts_q == 0", "FRAME_RING DONE post"),
        ("dut.u_stimulus.ring_done_dwell_q == 0", "FRAME_RING DONE dwell"),
        ("dut.u_stimulus.ring_done_frees_q == 0", "FRAME_RING DONE-to-FREE"),
        ("dut.u_stimulus.hps_read_completions_q == 0", "HPS read completion"),
        ("dut.u_stimulus.hps_first_beat_timing_witnesses_q == 0", "exact HPS first-beat timing"),
        ("dut.u_stimulus.hps_first_beat_timing_errors_q != 0", "clean HPS first-beat timing"),
        ("dut.u_stimulus.blit_successes_q == 0", "successful debug blit"),
        ("dut.u_stimulus.blit_failures_q != 0", "failure-free debug blit"),
        ("dut.u_stimulus.pad_directed_phases_q != 4'hf", "all directed pad phases"),
        ("dut.u_stimulus.audio_accepts_q == 0", "audio acceptance"),
        ("dut.u_stimulus.counter_windows_q == 0", "complete counter window"),
        ("dut.u_stimulus.counter_selector_seen_q != 40'hffffffffff", "counter selectors 0..39"),
        ("dut.u_stimulus.render_accepts_q < 2", "both render transactions"),
        ("dut.u_stimulus.render_directed_accepts_q == 0", "directed render acceptance"),
        ("dut.u_stimulus.render_entropy_accepts_q == 0", "entropy-width render acceptance"),
        ("dut.u_stimulus.render_entropy_completions_q == 0", "entropy-width render completion"),
        ("dut.u_stimulus.render_entropy_backpressure_witnesses_q == 0", "entropy delayed-ready stability witness"),
        ("dut.u_stimulus.render_backpressure_stability_errors_q != 0", "stable render offer under backpressure"),
        ("dut.u_stimulus.render_accept_class_errors_q != 0", "accepted render mode/data classification"),
        (
            "dut.u_stimulus.render_entropy_accepted_payload_q != dut.u_stimulus.render_entropy_offer_snapshot_q",
            "accepted render payload matching the stalled offer",
        ),
        (
            "dut.u_stimulus.render_directed_accepted_coefficients_q == "
            "dut.u_stimulus.render_entropy_accepted_coefficients_q",
            "distinct accepted directed/entropy coefficients",
        ),
        (
            "dut.u_stimulus.render_directed_accepted_vertices_q == "
            "dut.u_stimulus.render_entropy_accepted_vertices_q",
            "distinct accepted directed/entropy vertices",
        ),
        (
            "dut.u_stimulus.render_directed_accepted_bounds_q == "
            "dut.u_stimulus.render_entropy_accepted_bounds_q",
            "distinct accepted directed/entropy bounds",
        ),
        (
            "dut.u_stimulus.render_directed_accepted_identity_q == "
            "dut.u_stimulus.render_entropy_accepted_identity_q",
            "distinct accepted directed/entropy top-left/source identity",
        ),
        (
            "dut.u_stimulus.render_entropy_accepted_mode_q != 32'ha5e0_0068 || "
            "dut.u_stimulus.render_entropy_accepted_fill_q == 64'ha5a5_a5a5_a5a5_a5a5 || "
            "dut.u_stimulus.render_entropy_accepted_clear_q == 64'h5a5a_5a5a_5a5a_5a5a",
            "accepted entropy mode/data distinct from directed values",
        ),
        ("dut.u_stimulus.render_drains_q == 0", "completed two-job render profile"),
        ("dut.u_stimulus.guard_accepts_q == 0", "geometry guard accept"),
        ("dut.u_stimulus.guard_rejects_q == 0", "geometry guard reject"),
        ("dut.u_stimulus.guard_last_beats_q == 0", "geometry final beat"),
        ("dut.u_stimulus.guard_exact_frames_q == 0", "exact eight-beat geometry frame"),
        ("dut.u_stimulus.guard_verdict_timeouts_q != 0", "clean geometry verdict timing"),
        ("dut.u_stimulus.guard_early_last_errors_q != 0", "no early geometry last"),
        ("dut.u_stimulus.guard_late_last_errors_q != 0", "no late geometry last"),
        ("dut.u_stimulus.guard_extra_beat_errors_q != 0", "no extra geometry beat"),
        (
            "dut.u_stimulus.guard_verdict_extra_witnesses_q != 0",
            "no injected response-pending geometry beat",
        ),
        (
            "dut.u_stimulus.guard_post_denial_extra_witnesses_q != 0",
            "no injected post-denial geometry beat",
        ),
        (
            "dut.u_stimulus.guard_preownership_fault_windows_q != 3'b000",
            "no injected pre-ownership geometry beat",
        ),
        ("dut.u_stimulus.sdr_read_responses_q == 0", "SDR read response"),
        ("dut.u_stimulus.sdr_write_transactions_q == 0", "SDR write transaction"),
        ("dut.u_stimulus.sdr_write_observations_q == 0", "SDR write observation hash"),
        ("dut.shell_hps_err_count_o != 0", "clean HPS error counter"),
        ("dut.shell_shell_err_wfifo_o != 0", "clean shell write-FIFO tripwire"),
        ("dut.shell_shell_err_route_o != 0", "clean shell route tripwire"),
        ("dut.shell_shell_err_cdc_o != 0", "clean shell CDC tripwire"),
        ("dut.shell_shell_err_framer_o != 0", "clean shell framer tripwire"),
        ("dut.shell_render_stream_error_o != 0", "clean render stream tripwire"),
        ("dut.shell_render_fatal_o != 0", "clean render fatal tripwire"),
        ("dut.shell_render_overflow_o != 0", "clean render overflow tripwire"),
        ("dut.shell_render_fragment_error_o != 0", "clean render fragment tripwire"),
    )
    protocol_checks = (
        ("dut.u_stimulus.ring_sequence_errors_q != 0", "FRAME_RING sequence error"),
        ("dut.u_stimulus.hps_write_watchdog_faults_q != 0", "HPS write watchdog"),
        ("dut.u_stimulus.counter_sequence_errors_q != 0", "counter sequence error"),
        ("dut.u_stimulus.render_timeouts_q != 0", "render retirement timeout"),
        ("dut.u_stimulus.guard_beat_timeouts_q != 0", "geometry beat timeout"),
        (
            "dut.u_stimulus.sdr_write_phase_errors_q != 0 && "
            "dut.u_stimulus.sdr_write_transactions_q == 0 && "
            "dut.u_stimulus.sdr_write_observations_q == 0",
            "SDR write command/data observation failure",
        ),
        ("dut.u_stimulus.guard_verdict_timeouts_q != 0", "geometry verdict timeout"),
        ("dut.u_stimulus.guard_early_last_errors_q != 0", "geometry early last"),
        ("dut.u_stimulus.guard_late_last_errors_q != 0", "geometry late last"),
        (
            "dut.u_stimulus.guard_extra_beat_errors_q != 0 && "
            "(protocol_fault_i != 4'd13 || "
            "dut.u_stimulus.guard_verdict_extra_witnesses_q != 0) && "
            "(protocol_fault_i != 4'd14 || "
            "dut.u_stimulus.guard_post_denial_extra_witnesses_q != 0) && "
            "(protocol_fault_i != 4'd15 || "
            "dut.u_stimulus.guard_preownership_fault_windows_q == 3'b111)",
            "geometry delayed extra beat",
        ),
        ("dut.u_stimulus.hps_first_beat_timing_errors_q != 0", "HPS first-beat timing"),
        (
            "dut.u_stimulus.render_backpressure_stability_errors_q != 0",
            "render offer changed during delayed ready",
        ),
    )
    lines.extend(
        [
            "    event_failure_c =",
            "        " + " ||\n        ".join(f"({check})" for check, _ in event_checks) + ";",
            "    protocol_failures_o = '0;",
        ]
    )
    for index, (check, _label) in enumerate(protocol_checks):
        lines.append(f"    protocol_failures_o[{index}] = ({check});")
    lines.extend(
        [
            "    protocol_failure_o = |protocol_failures_o;",
            "    zero_reference_failure_c =",
        ]
    )
    zero_terms: list[str] = []
    for domain in DOMAINS:
        zero_terms.extend(
            [
                f"(u_zero_{domain}.chunk_wraps_q == 0)",
                f"(u_zero_{domain}.snapshots_q == 0)",
                f"(u_zero_{domain}.misr_q != 32'h0)",
                f"(u_zero_{domain}.snapshot_q != 32'h0)",
                f"{domain}_zero_signature_nonzero_q",
            ]
        )
    lines.extend(
        [
            "        " + " ||\n        ".join(zero_terms) + ";",
            "    baseline_failed_o = |dead_domains_o || input_failure_c ||",
            "                        output_failure_c || event_failure_c ||",
            "                        protocol_failure_o || zero_reference_failure_c ||",
            "                        sink_mismatch_c;",
            "  end",
            "",
            "  always @(posedge check_i) begin",
            "    if (control_domain_i == 2'd3 && control_arm_i == 4'd0 &&",
            "        protocol_fault_i == 4'd0) begin",
            '      if (dead_domains_o != 0) $display("SHELL_FIT_SMOKE_FAIL dead-domains=%b reasons=%h", dead_domains_o, dead_reasons_o);',
        ]
    )
    for port, row, offset in layout:
        mask = int(row.dynamic_mask or 0)
        lines.extend(
            [
                f"      if (input_changed_q[{offset} +: {port.bit_width}] != "
                f"{_hex_literal(port.bit_width, mask)})",
                f'        $display("SHELL_FIT_SMOKE_FAIL input {port.name} observed=%h expected=%h", '
                f"input_changed_q[{offset} +: {port.bit_width}], "
                f"{_hex_literal(port.bit_width, mask)});",
            ]
        )
    for port, row, offset in output_layout:
        mask = int(row.dynamic_mask or 0)
        domain = row.domain
        lines.extend(
            [
                f"      if ({domain}_capture_changed_q[{offset} +: {port.bit_width}] != "
                f"{_hex_literal(port.bit_width, mask)})",
                f'        $display("SHELL_FIT_SMOKE_FAIL output-group {port.name} observed=%h expected=%h", '
                f"{domain}_capture_changed_q[{offset} +: {port.bit_width}], "
                f"{_hex_literal(port.bit_width, mask)});",
            ]
        )
    for check, label in event_checks:
        lines.append(f'      if ({check}) $display("SHELL_FIT_SMOKE_FAIL no {label}");')
    for check, label in protocol_checks:
        lines.append(f'      if ({check}) $display("SHELL_FIT_SMOKE_FAIL {label}");')
    lines.extend(
        [
            "      if (dut.u_stimulus.render_drains_q == 0 ||",
            "          dut.u_stimulus.render_entropy_completions_q == 0 ||",
            "          dut.u_stimulus.render_entropy_backpressure_witnesses_q == 0)",
            '        $display("SHELL_FIT_SMOKE_DIAG render state=%0d accepts=%0d directed=%0d entropy=%0d stalls=%0d entropy_stalls=%0d stable_errors=%0d class_errors=%0d mode=%h fill=%h clear=%h issued=%0d retired=%0d drained=%0d fatal=%0d pixels=%0d bursts=%0d",',
            "                 dut.u_stimulus.render_state_q, dut.u_stimulus.render_accepts_q,",
            "                 dut.u_stimulus.render_directed_accepts_q, dut.u_stimulus.render_entropy_accepts_q,",
            "                 dut.u_stimulus.render_backpressure_witnesses_q,",
            "                 dut.u_stimulus.render_entropy_backpressure_witnesses_q,",
            "                 dut.u_stimulus.render_backpressure_stability_errors_q,",
            "                 dut.u_stimulus.render_accept_class_errors_q,",
            "                 dut.u_stimulus.render_entropy_accepted_mode_q,",
            "                 dut.u_stimulus.render_entropy_accepted_fill_q,",
            "                 dut.u_stimulus.render_entropy_accepted_clear_q,",
            "                 dut.shell_render_issued_words_o, dut.shell_render_retired_words_o,",
            "                 dut.shell_render_drained_o, dut.shell_render_fatal_o,",
            "                 dut.shell_render_pixels_o, dut.shell_render_bursts_o);",
            "      if (dut.u_stimulus.sdr_write_transactions_q == 0)",
            '        $display("SHELL_FIT_SMOKE_DIAG command dma_status=%h blit_done=%0d blit_status=%h hps_reads=%0d",',
            "                 dut.shell_dma_status_o, dut.shell_blit_done_o, dut.shell_blit_status_o,",
            "                 dut.u_stimulus.hps_read_completions_q);",
        ]
    )
    for domain in DOMAINS:
        lines.extend(
            [
                f"      if (u_zero_{domain}.chunk_wraps_q == 0 ||",
                f"          u_zero_{domain}.snapshots_q == 0 ||",
                f"          u_zero_{domain}.misr_q != 0 ||",
                f"          u_zero_{domain}.snapshot_q != 0 ||",
                f"          {domain}_zero_signature_nonzero_q)",
                f'        $display("SHELL_FIT_SMOKE_FAIL all-zero {domain} reference changed");',
            ]
        )
    lines.extend(["    end else if (control_arm_i == 4'd9) begin"])
    for port, row, offset in output_layout:
        mask = int(row.dynamic_mask or 0)
        if mask == 0:
            continue
        domain = row.domain
        domain_index = domain_indexes[domain]
        lines.extend(
            [
                f"      if (control_domain_i == 2'd{domain_index} &&",
                f"          ({domain}_capture_changed_q[{offset} +: {port.bit_width}] != "
                f"{_hex_literal(port.bit_width, mask)}))",
                f'        $display("SHELL_FIT_SMOKE_CONTROL_DEAD_GROUP domain={domain} group={port.name}");',
            ]
        )
    lines.extend(
        [
            "    end else if (protocol_fault_i != 4'd0) begin",
            '      $display("SHELL_FIT_SMOKE_CONTROL_PROTOCOL fault=%0d detected=%b",',
            "               protocol_fault_i, protocol_failures_o);",
            "    end",
            "  end",
            "endmodule",
            "",
        ]
    )
    return "\n".join(lines).encode("utf-8")


def render_repo(repo: Path) -> bytes:
    shell_text = _read_utf8_exact(repo / DEFAULT_SHELL)
    package_text = _read_utf8_exact(repo / DEFAULT_PACKAGE)
    policy_text = _read_utf8_exact(repo / DEFAULT_POLICY)
    declaration = parse_module_declaration(
        shell_text,
        "zhao_shell_top",
        type_widths=discover_type_widths(package_text),
    )
    policy = load_policy_text(policy_text)
    validate_policy(declaration, policy)
    manifest = _load_manifest(_read_utf8_exact(repo / DEFAULT_MANIFEST))
    _require_manifest_freshness(
        repo=repo,
        manifest=manifest,
        declaration=declaration,
        policy=policy,
    )
    return render_monitor(manifest=manifest, declaration=declaration, policy=policy)


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
        temporary.unlink(missing_ok=True)
        raise


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--check", action="store_true")
    action.add_argument("--write", action="store_true")
    parser.add_argument("--repo-root", type=Path, default=DEFAULT_REPO)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo = args.repo_root.resolve()
    output = args.output if args.output.is_absolute() else repo / args.output
    try:
        expected = render_repo(repo)
        if args.check:
            if not output.is_file():
                raise ShellPortError(f"missing generated smoke monitor: {output}")
            if output.read_bytes() != expected:
                raise ShellPortError(f"stale generated smoke monitor: {output}")
            print("shell-fit-smoke: generated monitor is current")
        else:
            _atomic_write(output, expected)
            print(f"shell-fit-smoke: wrote {output}")
    except (OSError, ShellPortError) as exc:
        print(f"shell-fit-smoke: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
