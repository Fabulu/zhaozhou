#!/usr/bin/env python3
"""Unit and positive-control tests for the shell-fit declaration pipeline."""

from __future__ import annotations

import copy
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


REPO = Path(__file__).resolve().parents[2]
QUARTUS_TOOLS = REPO / "tools" / "quartus"
FIXTURES = Path(__file__).resolve().parent / "fixtures"
GENERATOR = QUARTUS_TOOLS / "gen_shell_fit_top.py"
SMOKE_GENERATOR = QUARTUS_TOOLS / "shell_fit_smoke.py"
for entry in (str(QUARTUS_TOOLS),):
    if entry not in sys.path:
        sys.path.insert(0, entry)

import gen_shell_fit_top as generator
import shell_elab_ports as shell_elab
import shell_fit_smoke as smoke_generator
from shell_ports import (
    ShellPortError,
    assert_exact_port_sets,
    discover_type_widths,
    flatten_port_elements,
    load_policy_text,
    parse_module_declaration,
    validate_policy,
)


class ShellPortParserTests(unittest.TestCase):
    def test_complex_ansi_fixture(self) -> None:
        source = (FIXTURES / "shell_ports_complex.sv").read_text(encoding="utf-8")
        widths = discover_type_widths(source)
        self.assertEqual(widths["fixture_req_t"], 4)
        declaration = parse_module_declaration(
            source, "shell_ports_fixture", type_widths=widths
        )
        self.assertEqual(
            [port.name for port in declaration.ports],
            [
                "signed_i",
                "first_i",
                "second_i",
                "fixture_req_t",
                "request_i",
                "lanes_o",
                "ready_o",
            ],
        )
        self.assertEqual(
            [port.bit_width for port in declaration.ports], [9, 4, 4, 1, 4, 18, 4]
        )
        self.assertEqual(declaration.input_bits, 22)
        self.assertEqual(declaration.output_bits, 22)
        self.assertEqual(declaration.total_bits, 44)
        self.assertEqual(declaration.ports[0].signal_type_text, "logic signed [WIDTH-1:0]")
        self.assertEqual(
            [(dim.left, dim.right) for dim in declaration.ports[5].unpacked_dimensions],
            [(1, 0)],
        )
        self.assertEqual(
            flatten_port_elements(declaration.ports[5]),
            (("[1]", 0, 9), ("[0]", 9, 9)),
        )
        # The identifier happens to equal an imported typedef. It is still the
        # scalar port name because the explicit type before it is logic.
        fake_typedef_token = declaration.ports[3]
        self.assertEqual(fake_typedef_token.name, "fixture_req_t")
        self.assertEqual(fake_typedef_token.type_text, "logic")
        self.assertEqual(fake_typedef_token.bit_width, 1)

    def test_declaration_hash_is_stable_across_fresh_processes(self) -> None:
        shell = REPO / generator.DEFAULT_SHELL
        package = REPO / generator.DEFAULT_PACKAGE
        expected = parse_module_declaration(
            shell.read_bytes().decode("utf-8"),
            "zhao_shell_top",
            type_widths=discover_type_widths(package.read_bytes().decode("utf-8")),
        ).declaration_sha256
        program = (
            "import pathlib,sys; "
            f"sys.path.insert(0, {str(QUARTUS_TOOLS)!r}); "
            "import shell_ports; "
            f"s=pathlib.Path({str(shell)!r}).read_bytes().decode('utf-8'); "
            f"p=pathlib.Path({str(package)!r}).read_bytes().decode('utf-8'); "
            "print(shell_ports.parse_module_declaration(s, 'zhao_shell_top', "
            "type_widths=shell_ports.discover_type_widths(p)).declaration_sha256)"
        )
        observed = []
        for seed in ("1", "2", "random"):
            completed = subprocess.run(
                [sys.executable, "-c", program],
                env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1", "PYTHONHASHSEED": seed},
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            observed.append(completed.stdout.strip())
        self.assertEqual(observed, [expected, expected, expected])

    def test_smoke_utf8_reader_preserves_source_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "source.sv"
            source.write_bytes(b"module exact\r\n(input logic value_i);\r\nendmodule\r\n")
            self.assertEqual(
                smoke_generator._read_utf8_exact(source), source.read_bytes().decode("utf-8")
            )
            self.assertIn("\r\n", smoke_generator._read_utf8_exact(source))

    def test_unknown_packed_type_fires(self) -> None:
        source = "module bad(input invented_typedef payload_i); endmodule\n"
        with self.assertRaisesRegex(
            ShellPortError, "unknown packed type 'invented_typedef' on port 'payload_i'"
        ):
            parse_module_declaration(source, "bad")

    def test_missing_sink_fixture_fires_exact_name(self) -> None:
        fixture = json.loads(
            (FIXTURES / "shell_fit_missing_sink.json").read_text(encoding="utf-8")
        )
        source = (FIXTURES / fixture["source_fixture"]).read_text(encoding="utf-8")
        declaration = parse_module_declaration(
            source,
            "shell_ports_fixture",
            type_widths=discover_type_widths(source),
        )
        outputs = [port.name for port in declaration.ports if port.direction == "output"]
        self.assertEqual(outputs, fixture["declared_outputs"])
        with self.assertRaisesRegex(ShellPortError, re.escape(fixture["expected_diagnostic"])):
            assert_exact_port_sets(
                {
                    "parser-outputs": outputs,
                    "fixture-sinks": fixture["captured_outputs"],
                }
            )


class ShellPolicyTests(unittest.TestCase):
    SOURCE = "module policy_fixture(input logic a_i, output logic b_o); endmodule\n"

    @staticmethod
    def policy_data() -> dict[str, object]:
        return {
            "schema_version": 1,
            "module": "policy_fixture",
            "traffic_profile": "registered_legalish_v1",
            "ports": [
                {
                    "ordinal": 0,
                    "name": "a_i",
                    "direction": "input",
                    "domain": "gpu",
                    "driver": "pads",
                    "dynamic_mask": "0x1",
                },
                {
                    "ordinal": 1,
                    "name": "b_o",
                    "direction": "output",
                    "domain": "gpu",
                    "sink": "gpu_capture",
                    "dynamic_mask": "0x1",
                },
            ],
        }

    def validate(self, data: dict[str, object]) -> None:
        declaration = parse_module_declaration(self.SOURCE, "policy_fixture")
        validate_policy(declaration, load_policy_text(json.dumps(data)))

    def test_valid_exact_policy(self) -> None:
        self.validate(self.policy_data())

    def test_missing_policy_row_fires(self) -> None:
        data = self.policy_data()
        data["ports"] = copy.deepcopy(data["ports"][:-1])
        with self.assertRaisesRegex(ShellPortError, "policy is missing shell ports: b_o"):
            self.validate(data)

    def test_extra_policy_row_fires(self) -> None:
        data = self.policy_data()
        extra = copy.deepcopy(data["ports"][1])
        extra.update({"ordinal": 2, "name": "ghost_o"})
        data["ports"].append(extra)
        with self.assertRaisesRegex(ShellPortError, "policy has unknown shell ports: ghost_o"):
            self.validate(data)

    def test_duplicate_policy_row_fires(self) -> None:
        data = self.policy_data()
        data["ports"].append(copy.deepcopy(data["ports"][1]))
        with self.assertRaisesRegex(ShellPortError, "policy has duplicate shell ports: b_o"):
            self.validate(data)

    def test_invalid_domain_fires(self) -> None:
        data = self.policy_data()
        data["ports"][1]["domain"] = "wildcard"
        with self.assertRaisesRegex(ShellPortError, "unclassified domain 'wildcard'"):
            self.validate(data)

    def test_unknown_policy_key_fires(self) -> None:
        data = self.policy_data()
        data["ports"][0]["default"] = "not allowed"
        with self.assertRaisesRegex(ShellPortError, r"has unknown keys \['default'\]"):
            load_policy_text(json.dumps(data))

    def test_intentional_constant_bits_need_reason(self) -> None:
        data = self.policy_data()
        data["ports"][0]["dynamic_mask"] = 0
        with self.assertRaisesRegex(
            ShellPortError, "intentional constant bits without a reason"
        ):
            self.validate(data)

    def test_output_activity_mask_is_required(self) -> None:
        data = self.policy_data()
        del data["ports"][1]["dynamic_mask"]
        with self.assertRaisesRegex(ShellPortError, "output 'b_o' is missing dynamic_mask"):
            self.validate(data)

    def test_output_constant_bits_need_reason(self) -> None:
        data = self.policy_data()
        data["ports"][1]["dynamic_mask"] = 0
        with self.assertRaisesRegex(
            ShellPortError, "output 'b_o' has intentional constant bits without a reason"
        ):
            self.validate(data)

    def test_output_activity_mask_must_fit_width(self) -> None:
        data = self.policy_data()
        data["ports"][1]["dynamic_mask"] = 2
        with self.assertRaisesRegex(
            ShellPortError, "output 'b_o' dynamic_mask does not fit 1 bits"
        ):
            self.validate(data)


class ShellGeneratorTests(unittest.TestCase):
    def render_repo(self) -> generator.RenderedArtifacts:
        return generator.render_artifacts(
            shell_bytes=(REPO / generator.DEFAULT_SHELL).read_bytes(),
            package_bytes=(REPO / generator.DEFAULT_PACKAGE).read_bytes(),
            policy_bytes=(REPO / generator.DEFAULT_POLICY).read_bytes(),
            packet_bytes=(REPO / generator.DEFAULT_PACKET).read_bytes(),
            packet_path=generator.DEFAULT_PACKET.as_posix(),
            generator_bytes=GENERATOR.read_bytes(),
            parser_bytes=(QUARTUS_TOOLS / "shell_ports.py").read_bytes(),
        )

    def test_render_triangle_values_are_derived_and_positive_area(self) -> None:
        values = generator._render_triangle_values(
            a=(-1024, 15872),
            b=(-1792, -1024),
            c=(16128, 512),
            source_id=0xD5E7,
        )
        self.assertEqual(values["render_kx0_i"], "-23'sd1536")
        self.assertEqual(values["render_ky1_i"], "-23'sd17152")
        self.assertEqual(values["render_kc2_i"], "48'sd29491200")
        self.assertEqual(values["render_tl_i"], "3'b011")
        self.assertEqual(
            [
                values["render_min_x_i"],
                values["render_max_x_i"],
                values["render_min_y_i"],
                values["render_max_y_i"],
            ],
            ["12'sd0", "12'sd63", "12'sd0", "12'sd62"],
        )
        with self.assertRaisesRegex(ShellPortError, "area is not positive"):
            generator._render_triangle_values(
                a=(-1024, 15872),
                b=(16128, 512),
                c=(-1792, -1024),
                source_id=0xD5E7,
            )

    def run_cli(self, *arguments: str, cwd: Path) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        return subprocess.run(
            [sys.executable, str(GENERATOR), *arguments],
            cwd=cwd,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )

    def run_smoke_cli(self, *arguments: str, cwd: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(SMOKE_GENERATOR), *arguments],
            cwd=cwd,
            env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )

    def test_import_has_no_generated_side_effects(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            completed = subprocess.run(
                [
                    sys.executable,
                    "-c",
                    (
                        "import sys; "
                        f"sys.path.insert(0, {str(QUARTUS_TOOLS)!r}); "
                        "import gen_shell_fit_top"
                    ),
                ],
                cwd=work,
                env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"},
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            self.assertEqual(list(work.iterdir()), [])

    def test_help_and_malformed_arguments_never_write(self) -> None:
        for arguments in (("--help",), ("--write", "--check"), ("--bogus",)):
            with self.subTest(arguments=arguments), tempfile.TemporaryDirectory() as temporary:
                work = Path(temporary)
                rtl = work / "generated.sv"
                manifest = work / "generated.json"
                completed = self.run_cli(
                    *arguments,
                    "--rtl-out",
                    str(rtl),
                    "--manifest-out",
                    str(manifest),
                    cwd=work,
                )
                self.assertFalse(rtl.exists(), completed.stdout)
                self.assertFalse(manifest.exists(), completed.stdout)

    def test_check_names_missing_artifacts_without_writing(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            rtl = work / "missing.sv"
            manifest = work / "missing.json"
            completed = self.run_cli(
                "--check",
                "--repo-root",
                str(REPO),
                "--rtl-out",
                str(rtl),
                "--manifest-out",
                str(manifest),
                cwd=work,
            )
            self.assertEqual(completed.returncode, 1, completed.stdout)
            self.assertIn(f"missing generated artifact: {rtl}", completed.stdout)
            self.assertIn(f"missing generated artifact: {manifest}", completed.stdout)
            self.assertFalse(rtl.exists())
            self.assertFalse(manifest.exists())

    def test_check_detects_stale_byte_without_rewriting(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            rtl = work / "stale.sv"
            manifest = work / "stale.json"
            rtl.write_bytes(b"stale-rtl")
            manifest.write_bytes(b"stale-manifest")
            completed = self.run_cli(
                "--check",
                "--repo-root",
                str(REPO),
                "--rtl-out",
                str(rtl),
                "--manifest-out",
                str(manifest),
                cwd=work,
            )
            self.assertEqual(completed.returncode, 1, completed.stdout)
            self.assertIn(f"stale generated artifact: {rtl}", completed.stdout)
            self.assertIn(f"stale generated artifact: {manifest}", completed.stdout)
            self.assertEqual(rtl.read_bytes(), b"stale-rtl")
            self.assertEqual(manifest.read_bytes(), b"stale-manifest")

    def test_write_then_check_and_atomic_replace(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            rtl = work / "generated.sv"
            manifest = work / "generated.json"
            written = self.run_cli(
                "--write",
                "--repo-root",
                str(REPO),
                "--rtl-out",
                str(rtl),
                "--manifest-out",
                str(manifest),
                cwd=work,
            )
            self.assertEqual(written.returncode, 0, written.stdout)
            self.assertTrue(rtl.is_file())
            self.assertTrue(manifest.is_file())
            self.assertEqual([path for path in work.iterdir() if path.name.startswith(".")], [])
            checked = self.run_cli(
                "--check",
                "--repo-root",
                str(REPO),
                "--rtl-out",
                str(rtl),
                "--manifest-out",
                str(manifest),
                cwd=work,
            )
            self.assertEqual(checked.returncode, 0, checked.stdout)

        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "artifact"
            with mock.patch.object(generator.os, "replace", wraps=os.replace) as replace:
                generator._atomic_write(target, b"new bytes")
            replace.assert_called_once()
            self.assertEqual(target.read_bytes(), b"new bytes")

    def test_custom_packet_path_is_recorded_as_supplied(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            packet = work / "alternate-packet.bin"
            rtl = work / "generated.sv"
            manifest_path = work / "generated.json"
            packet.write_bytes(b"alternate packet provenance")
            completed = self.run_cli(
                "--write",
                "--repo-root",
                str(REPO),
                "--packet",
                str(packet),
                "--rtl-out",
                str(rtl),
                "--manifest-out",
                str(manifest_path),
                cwd=work,
            )
            self.assertEqual(completed.returncode, 0, completed.stdout)
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            self.assertEqual(manifest["packet_rom"]["path"], packet.resolve().as_posix())
            self.assertEqual(
                manifest["hashes"]["packet"], hashlib.sha256(packet.read_bytes()).hexdigest()
            )

    def test_policy_driver_taxonomy_must_equal_handler_ownership(self) -> None:
        shell = (REPO / generator.DEFAULT_SHELL).read_bytes().decode("utf-8")
        package = (REPO / generator.DEFAULT_PACKAGE).read_bytes().decode("utf-8")
        declaration = parse_module_declaration(
            shell,
            "zhao_shell_top",
            type_widths=discover_type_widths(package),
        )
        data = json.loads((REPO / generator.DEFAULT_POLICY).read_bytes().decode("utf-8"))
        by_name = {row["name"]: row for row in data["ports"]}
        by_name["hps_state_i"]["driver"] = "hps_responder"
        by_name["hps_req_grant_i"]["driver"] = "frame_ring"
        policy = load_policy_text(json.dumps(data))
        validate_policy(declaration, policy)
        with self.assertRaisesRegex(
            ShellPortError,
            r"driver 'frame_ring' ownership differs:.*hps_state_i.*hps_req_grant_i",
        ):
            generator.validate_handler_ownership(declaration, policy)

    def test_stale_manifest_refuses_smoke_write_without_altering_output(self) -> None:
        copied = (
            generator.DEFAULT_SHELL,
            generator.DEFAULT_PACKAGE,
            generator.DEFAULT_POLICY,
            generator.DEFAULT_PACKET,
            smoke_generator.DEFAULT_GENERATOR,
            smoke_generator.DEFAULT_PARSER,
            smoke_generator.DEFAULT_RTL,
            smoke_generator.DEFAULT_MANIFEST,
        )
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            for relative in copied:
                destination = work / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(REPO / relative, destination)
            with (work / generator.DEFAULT_POLICY).open("ab") as handle:
                handle.write(b"\n")
            output = work / "monitor.sv"
            output.write_bytes(b"sentinel monitor")
            completed = self.run_smoke_cli(
                "--write",
                "--repo-root",
                str(work),
                "--output",
                str(output),
                cwd=work,
            )
            self.assertEqual(completed.returncode, 1, completed.stdout)
            self.assertIn("stale mixed-generation manifest: policy:", completed.stdout)
            self.assertEqual(output.read_bytes(), b"sentinel monitor")

    def test_smoke_activity_masks_are_exact_not_subset_checks(self) -> None:
        monitor = smoke_generator.render_repo(REPO).decode("utf-8")
        self.assertNotRegex(
            monitor,
            r"(?:input_changed_q|(?:gpu|video|audio)_capture_changed_q)\[[^\n]+\]\s*&",
        )
        exact_checks = re.findall(
            r"(?:input_changed_q|(?:gpu|video|audio)_capture_changed_q)\[[^\n]+\]\s*!=",
            monitor,
        )
        # Every pseudo-input and output is checked in both the failure cone and
        # its diagnostic cone; this rejects both missing and unexpected bits.
        self.assertGreaterEqual(len(exact_checks), 2 * (55 + 95))
        self.assertIn(
            "protocol_fault_i != 4'd13 || dut.u_stimulus.guard_verdict_extra_witnesses_q != 0",
            " ".join(monitor.split()),
        )
        self.assertIn(
            "protocol_fault_i != 4'd14 || dut.u_stimulus.guard_post_denial_extra_witnesses_q != 0",
            " ".join(monitor.split()),
        )
        self.assertIn(
            "protocol_fault_i != 4'd15 || dut.u_stimulus.guard_preownership_fault_windows_q == 3'b111",
            " ".join(monitor.split()),
        )
        self.assertIn(
            "dut.u_stimulus.guard_preownership_fault_windows_q != 3'b000",
            " ".join(monitor.split()),
        )

    def test_render_geometry_and_hps_witnesses_are_reachable_controls(self) -> None:
        rtl = self.render_repo().rtl.decode("utf-8")
        self.assertIn("render_job_entropy_q <= render_width_cover_q;", rtl)
        self.assertIn("render_width_cover_q <= 1'b0;", rtl)
        self.assertIn(
            "render_kx0_i <= render_width_cover_q ? -23'sd1536 : -23'sd13312;",
            rtl,
        )
        self.assertIn(
            "render_entropy_accepted_coefficients_q <= render_triangle_coefficients_c;",
            rtl,
        )
        self.assertIn(
            "render_directed_accepted_identity_q <= render_triangle_identity_c;", rtl
        )
        self.assertIn("render_directed_accepts_q <= render_directed_accepts_q + 32'd1;", rtl)
        self.assertIn("render_entropy_accepts_q <= render_entropy_accepts_q + 32'd1;", rtl)
        self.assertIn("render_entropy_completions_q <= render_entropy_completions_q + 32'd1;", rtl)
        self.assertIn("guard_beat_count_q < 4'd7", rtl)
        self.assertIn("guard_beat_count_q == 4'd7", rtl)
        self.assertIn("guard_verdict_timeouts_q <= guard_verdict_timeouts_q + 32'd1;", rtl)
        self.assertIn("guard_early_last_errors_q <= guard_early_last_errors_q + 32'd1;", rtl)
        self.assertIn("guard_late_last_errors_q <= guard_late_last_errors_q + 32'd1;", rtl)
        self.assertIn("guard_extra_beat_errors_q <= guard_extra_beat_errors_q + 32'd1;", rtl)
        self.assertIn("protocol_fault_i == 4'd13", rtl)
        self.assertIn("protocol_fault_i == 4'd14", rtl)
        self.assertIn("protocol_fault_i == 4'd15", rtl)
        self.assertIn("guard_preownership_watch_q <= 1'b1;", rtl)
        self.assertIn(
            "!guard_response_pending_q && !geom_guard_req_i.valid && !guard_preownership_fault_windows_q[0]",
            " ".join(rtl.split()),
        )
        self.assertIn(
            "!guard_response_pending_q && geom_guard_req_i.valid && !guard_preownership_fault_windows_q[1]",
            " ".join(rtl.split()),
        )
        self.assertIn(
            "guard_response_pending_q && !guard_verdict_ok_checked_c && !guard_verdict_violation_checked_c && !guard_preownership_fault_windows_q[2]",
            " ".join(rtl.split()),
        )
        self.assertIn(
            "guard_preownership_fault_windows_q[0] <= 1'b1;", rtl
        )
        self.assertIn(
            "guard_preownership_fault_windows_q[1] <= 1'b1;", rtl
        )
        self.assertIn(
            "guard_preownership_fault_windows_q[2] <= 1'b1;", rtl
        )
        self.assertIn(
            "guard_verdict_extra_witnesses_q <= guard_verdict_extra_witnesses_q + 32'd1;",
            " ".join(rtl.split()),
        )
        self.assertIn(
            "guard_post_denial_extra_witnesses_q <= guard_post_denial_extra_witnesses_q + 32'd1;",
            " ".join(rtl.split()),
        )
        self.assertIn(
            "guard_denial_watch_q <= 1'b1; guard_denial_extra_fault_delay_q <= 4'd1;",
            " ".join(rtl.split()),
        )
        self.assertIn(
            "if (guard_verdict_ok_checked_c) begin",
            rtl,
        )
        self.assertIn("guard_preownership_watch_q <= 1'b0;", rtl)
        ok_branch = rtl.index("if (guard_verdict_ok_checked_c) begin")
        pending_extra_check = rtl.index(
            "if ((guard_frame_owned_q || guard_denial_watch_q ||", ok_branch
        )
        self.assertLess(ok_branch, pending_extra_check)
        self.assertIn(
            "guard_beat_count_q <= 4'd1;",
            rtl,
        )
        self.assertIn(
            "guard_verdict_extra_fault_delay_q <= guard_frame_owned_q ? 4'd1 : 4'd0;",
            " ".join(rtl.split()),
        )
        self.assertIn("hps_first_beat_age_q == 8'd16", rtl)
        self.assertIn("protocol_fault_i == 4'd11", rtl)

    def test_manifest_accounting_connections_and_capture_are_exact(self) -> None:
        artifacts = self.render_repo()
        manifest = json.loads(artifacts.manifest)
        self.assertEqual(
            manifest["accounting"],
            {
                "domain_chunks": {"audio": 3, "gpu": 53, "video": 6},
                "domain_output_bits": {"audio": 66, "gpu": 1695, "video": 168},
                "input_bits": 1457,
                "input_ports": 59,
                "non_clock_reset_input_bits": 1453,
                "output_bits": 1929,
                "output_ports": 95,
                "port_count": 154,
                "total_bits": 3386,
            },
        )
        self.assertEqual(manifest["shell_instance"], "u_shell")
        self.assertEqual(
            [(row["name"], row["bits"]) for row in manifest["external_ports"]],
            [
                ("gpu_clk", 1),
                ("vid_clk", 1),
                ("audio_clk", 1),
                ("rst_n", 1),
                ("fit_signature_o", 3),
                ("fit_epoch_o", 3),
            ],
        )
        self.assertEqual(sum(row["bits"] for row in manifest["external_ports"]), 10)

        ports = manifest["ports"]
        output_rows = [row for row in ports if row["direction"] == "output"]
        pseudo_input_rows = [
            row
            for row in ports
            if row["direction"] == "input" and row["driver"] != "top_port"
        ]
        self.assertTrue(all(row["capture_register"] for row in output_rows))
        self.assertTrue(all(row["registered_driver"] is None for row in output_rows))
        self.assertTrue(all(row["registered_driver"] for row in pseudo_input_rows))
        self.assertEqual(len(output_rows), 95)
        self.assertEqual(len(pseudo_input_rows), 55)

        cursors = {"gpu": 0, "video": 0, "audio": 0}
        for row in output_rows:
            domain = row["domain"]
            self.assertEqual(row["signature_offset"], cursors[domain])
            cursors[domain] += row["bit_width"]
        self.assertEqual(cursors, {"gpu": 1695, "video": 168, "audio": 66})

        rtl = artifacts.rtl.decode("utf-8")
        shell_match = re.search(r"\bzhao_shell_top\s+u_shell\s*\((.*?)\n\s*\);", rtl, re.S)
        self.assertIsNotNone(shell_match)
        connection_names = re.findall(r"^\s*\.([A-Za-z_$][A-Za-z0-9_$]*)\s*\(", shell_match.group(1), re.M)
        self.assertEqual(connection_names, [row["name"] for row in ports])
        self.assertNotIn(".*", shell_match.group(1))
        self.assertEqual(
            hashlib.sha256(artifacts.rtl).hexdigest(), manifest["generated_rtl_sha256"]
        )


class ShellElaborationComparatorTests(unittest.TestCase):
    def test_manifest_rows_match_independent_elaboration(self) -> None:
        ports = (
            shell_elab.ElaboratedPort(
                0, "request_i", "input", 4, 4, (), False, "BASICDTYPE", ((3, 0),)
            ),
            shell_elab.ElaboratedPort(
                1, "lanes_o", "output", 18, 9, ((1, 0),), False, "BASICDTYPE", ((8, 0),)
            ),
        )
        rows = [
            {
                "ordinal": 0,
                "name": "request_i",
                "direction": "input",
                "bit_width": 4,
                "element_width": 4,
                "signed": False,
                "packed_dimensions": [{"left": 3, "right": 0}],
                "unpacked_dimensions": [],
            },
            {
                "ordinal": 1,
                "name": "lanes_o",
                "direction": "output",
                "bit_width": 18,
                "element_width": 9,
                "signed": False,
                "packed_dimensions": [{"left": 8, "right": 0}],
                "unpacked_dimensions": [{"left": 1, "right": 0}],
            },
        ]
        shell_elab.compare_manifest_to_elaboration(
            elaborated=ports,
            rows=rows,
            width_key="bit_width",
            label="fixture",
        )

    def test_equal_width_reversed_packed_range_fires(self) -> None:
        port = shell_elab.ElaboratedPort(
            0, "data_i", "input", 8, 8, (), False, "BASICDTYPE", ((0, 7),)
        )
        row = {
            "name": "data_i",
            "direction": "input",
            "bit_width": 8,
            "element_width": 8,
            "signed": False,
            "packed_dimensions": [{"left": 7, "right": 0}],
            "unpacked_dimensions": [],
        }
        with self.assertRaisesRegex(ShellPortError, "packed shape"):
            shell_elab.compare_manifest_to_elaboration(
                elaborated=(port,), rows=(row,), width_key="bit_width", label="fixture"
            )

    def test_equal_width_changed_signedness_fires(self) -> None:
        port = shell_elab.ElaboratedPort(
            0, "data_i", "input", 8, 8, (), True, "BASICDTYPE", ((7, 0),)
        )
        row = {
            "name": "data_i",
            "direction": "input",
            "bit_width": 8,
            "element_width": 8,
            "signed": False,
            "packed_dimensions": [{"left": 7, "right": 0}],
            "unpacked_dimensions": [],
        }
        with self.assertRaisesRegex(ShellPortError, "signedness"):
            shell_elab.compare_manifest_to_elaboration(
                elaborated=(port,), rows=(row,), width_key="bit_width", label="fixture"
            )

    def test_missing_manifest_output_fires_exactly(self) -> None:
        ports = (
            shell_elab.ElaboratedPort(0, "request_i", "input", 4, 4, (), False, "BASICDTYPE"),
            shell_elab.ElaboratedPort(1, "ready_o", "output", 1, 1, (), False, "BASICDTYPE"),
        )
        rows = [
            {"name": "request_i", "direction": "input", "bit_width": 4},
        ]
        with self.assertRaisesRegex(
            ShellPortError,
            r"fixture: elaboration has ports absent from manifest: \['ready_o'\]",
        ):
            shell_elab.compare_manifest_to_elaboration(
                elaborated=ports,
                rows=rows,
                width_key="bit_width",
                label="fixture",
            )


if __name__ == "__main__":
    unittest.main()
