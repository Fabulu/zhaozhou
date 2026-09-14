#!/usr/bin/env python3
"""Verify the SuperStation image that runs real Zhaozhou block vectors."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

import verify_superstation_bringup as common

PROJECT = "ZhaozhouSpecs"
EXPECTED_CORE = "Zhaozhou Hardware Specs"
EXPECTED_SIGNATURE = "E5F1C57F"
EXPECTED_ENGINE_MODULES = (
    "zhao_crc32c_fold",
    "zhao_raster_fill",
    "zhao_dual18_mul",
)


def verify_sources(repo: Path) -> tuple[list[str], dict[str, object]]:
    errors, summary = common.verify_sources(repo)
    fpga = repo / "fpga"

    common.require_text(
        fpga / f"{PROJECT}.qsf",
        [
            "set_global_assignment -name TOP_LEVEL_ENTITY sys_top",
            "set_global_assignment -name VERILOG_MACRO ZHAO_DUAL18_CYCLONEV",
            'set_global_assignment -name RESERVE_ALL_UNUSED_PINS "AS INPUT TRI-STATED"',
            "source sys/sys.tcl",
            "source sys/sys_analog.tcl",
            "source files_specs.qip",
        ],
        errors,
    )
    common.require_text(
        fpga / "files_specs.qip",
        [
            f"set_global_assignment -name SDC_FILE {PROJECT}.sdc",
            "rtl/common/zhao_crc32c_fold.sv",
            "rtl/raster/zhao_raster_fill.sv",
            "rtl/common/zhao_dual18_mul.sv",
            "rtl/platform/zhao_ssone_spec_tests.sv",
            "rtl/platform/zhao_ssone_specs_emu.sv",
        ],
        errors,
    )
    tests = common.require_text(
        fpga / "rtl" / "platform" / "zhao_ssone_spec_tests.sv",
        [
            "zhao_crc32c_fold u_crc",
            "zhao_raster_fill #(.W(29)) u_fill",
            "zhao_dual18_mul #(",
            f"EXPECTED_SIGNATURE = 32'h{EXPECTED_SIGNATURE}",
            "INJECT_FAILURE ? 32'h00000001 : 32'h00000000",
        ],
        errors,
    )
    for module in EXPECTED_ENGINE_MODULES:
        if not re.search(rf"\b{module}\b", tests):
            errors.append(f"hardware spec runner does not instantiate {module}")

    emu = common.require_text(
        fpga / "rtl" / "platform" / "zhao_ssone_specs_emu.sv",
        [
            f'"{EXPECTED_CORE};;"',
            "zhao_ssone_spec_tests u_spec_tests",
            "assign ADC_BUS = 'z;",
            "assign USER_OUT = '1;",
            "assign {SD_SCK, SD_MOSI, SD_CS} = 'z;",
            ".outclk_0(clk_core)",
            "assign CLK_VIDEO = clk_core;",
            "spec_signature",
            "spec_fail_code",
        ],
        errors,
    )
    if not re.search(r"assign\s*\{[^;]*SDRAM_DQ[^;]*\}\s*=\s*'z\s*;", emu, re.S):
        errors.append("spec core does not tri-state the complete SDRAM bundle")
    if not re.search(r"assign\s*\{[^;]*DDRAM_CLK[^;]*\}\s*=\s*'0\s*;", emu, re.S):
        errors.append("spec core does not hold the complete HPS DDR request bundle inactive")

    summary["specSourceStatus"] = "ok" if not errors else "failed"
    summary["project"] = PROJECT
    summary["expectedCore"] = EXPECTED_CORE
    summary["expectedSignature"] = EXPECTED_SIGNATURE.lower()
    summary["engineModules"] = list(EXPECTED_ENGINE_MODULES)
    return errors, summary


def verify_build(build: Path, errors: list[str], summary: dict[str, object]) -> None:
    common.verify_patched_sys_top(build, errors, summary)
    common.verify_patched_build_id(build, errors, summary)
    output = build / "output_files"
    flow = output / f"{PROJECT}.flow.rpt"
    mapping = output / f"{PROJECT}.map.rpt"
    fit = output / f"{PROJECT}.fit.rpt"
    sta = output / f"{PROJECT}.sta.rpt"
    pin = output / f"{PROJECT}.pin"
    rbf = output / f"{PROJECT}.rbf"

    flow_text = common.require_text(flow, ["Flow Status", common.EXPECTED_DEVICE], errors)
    if flow_text and not re.search(r"Flow Status\s*;\s*Successful", flow_text):
        errors.append(f"{flow}: flow did not report Successful")

    map_text = common.require_text(
        mapping,
        [
            "Quartus Prime Analysis & Synthesis was successful",
            *EXPECTED_ENGINE_MODULES,
            "cyclonev_mac",
        ],
        errors,
    )
    fit_text = common.require_text(
        fit,
        [common.EXPECTED_DEVICE, "Quartus Prime Fitter was successful"],
        errors,
    )
    sta_text = common.require_text(
        sta,
        [common.EXPECTED_DEVICE, "Quartus Prime TimeQuest Timing Analyzer was successful"],
        errors,
    )
    pin_text = common.require_text(
        pin,
        [f'CHIP  "{PROJECT}"  ASSIGNED TO AN: {common.EXPECTED_DEVICE}'],
        errors,
    )

    if map_text:
        hierarchy: dict[str, dict[str, int]] = {}
        expected_rows = {
            "zhao_ssone_spec_tests:u_spec_tests": (1, 1),
            "zhao_crc32c_fold:u_crc": (1, 0),
            "zhao_raster_fill:u_fill": (1, 0),
            "zhao_dual18_mul:u_mul": (0, 1),
        }
        for node, (minimum_aluts, expected_dsps) in expected_rows.items():
            match = re.search(
                rf"(?m)^;\s*\|{re.escape(node)}\|\s*;\s*"
                rf"(\d+)\s*\(\d+\)\s*;\s*(\d+)\s*\(\d+\)\s*;\s*"
                rf"(\d+)\s*;\s*(\d+)\s*;",
                map_text,
            )
            if not match:
                errors.append(f"{mapping}: no post-map resource row for {node}")
                continue
            aluts, registers, memory_bits, dsps = map(int, match.groups())
            hierarchy[node] = {
                "combinationalAluts": aluts,
                "registers": registers,
                "memoryBits": memory_bits,
                "dspBlocks": dsps,
            }
            if aluts < minimum_aluts:
                errors.append(f"{mapping}: {node} has only {aluts} ALUTs")
            if dsps != expected_dsps:
                errors.append(
                    f"{mapping}: {node} has {dsps} DSPs, expected {expected_dsps}"
                )
        summary["physicalHierarchy"] = hierarchy

        dsp_match = re.search(r"Implemented\s+(\d+)\s+DSP elements", map_text)
        if not dsp_match:
            errors.append(f"{mapping}: missing implemented DSP count")
        elif int(dsp_match.group(1)) < 34:
            errors.append(
                f"{mapping}: only {dsp_match.group(1)} DSPs; packed Zhaozhou block may be absent"
            )
        summary["implementedDsps"] = int(dsp_match.group(1)) if dsp_match else None

    if not rbf.is_file() or rbf.stat().st_size == 0:
        errors.append(f"missing or empty RBF: {rbf}")
    else:
        summary["rbf"] = {
            "path": str(rbf),
            "bytes": rbf.stat().st_size,
            "sha256": hashlib.sha256(rbf.read_bytes()).hexdigest(),
        }

    critical = common.collect_critical_warnings(output, errors)
    if critical:
        errors.append(f"critical Quartus warnings present: {critical[:5]}")
    summary["criticalWarnings"] = critical

    if fit_text:
        common.verify_user_io_high_z(fit_text, fit, errors, summary)

    if pin_text:
        for signal, package_pin in common.EXPECTED_CLOCK_PINS.items():
            location = package_pin.removeprefix("PIN_")
            pattern = (
                rf"(?m)^{re.escape(signal)}\s*:\s*{re.escape(location)}\s*:\s*"
                rf"input\s*:\s*3\.3-V LVTTL"
            )
            if not re.search(pattern, pin_text):
                errors.append(f"{pin}: missing 3.3-V input {signal} at {package_pin}")
        reserved_outputs = re.findall(
            r"(?m)^RESERVED_OUTPUT[^:\r\n]*\s*:\s*[A-Z]{1,2}\d+",
            pin_text,
        )
        if reserved_outputs:
            errors.append(f"{pin}: unused output-driving pins present: {reserved_outputs[:5]}")
        summary["unusedOutputPins"] = reserved_outputs

    slacks: dict[str, float] = {}
    if sta_text:
        for check in ("setup", "hold", "recovery", "removal", "minimum pulse width"):
            match = re.search(
                rf"Worst-case {re.escape(check)} slack is\s+(-?\d+(?:\.\d+)?)",
                sta_text,
                re.IGNORECASE,
            )
            if not match:
                errors.append(f"{sta}: missing worst-case {check} slack")
                continue
            value = float(match.group(1))
            slacks[check] = value
            if value < 0:
                errors.append(f"{sta}: negative worst-case {check} slack: {value}")
        for label in ("Illegal Clocks", "Unconstrained Clocks"):
            match = re.search(rf";\s*{re.escape(label)}\s*;\s*(\d+)\s*;", sta_text)
            if not match or int(match.group(1)) != 0:
                errors.append(f"{sta}: {label} is not reported as zero")
    summary["timingSlacksNs"] = slacks

    if flow_text:
        def flow_value(label: str) -> str | None:
            match = re.search(rf";\s*{re.escape(label)}\s*;\s*([^;]+?)\s*;", flow_text)
            return match.group(1).strip() if match else None

        summary["flow"] = {
            "status": flow_value("Flow Status"),
            "top": flow_value("Top-level Entity Name"),
            "device": flow_value("Device"),
            "alms": flow_value("Logic utilization (in ALMs)"),
            "registers": flow_value("Total registers"),
            "virtualPins": flow_value("Total virtual pins"),
            "dsps": flow_value("Total DSP Blocks"),
        }

    summary["buildDirectory"] = str(build)
    summary["buildStatus"] = "ok" if not errors else "failed"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    errors, summary = verify_sources(args.repo.resolve())
    if args.build_dir is not None:
        verify_build(args.build_dir.resolve(), errors, summary)
    summary["errors"] = errors

    if args.json:
        print(json.dumps(summary, indent=2, sort_keys=True))
    elif errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
    else:
        print(
            "SuperStation hardware specs verification passed: "
            f"device={common.EXPECTED_DEVICE} modules={','.join(EXPECTED_ENGINE_MODULES)}"
        )
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
