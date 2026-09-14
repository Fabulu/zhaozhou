#!/usr/bin/env python3
"""Verify the pinned SuperStation One MiSTer bring-up project and build."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

EXPECTED_SYS_TREE = "9f95eddd65ebfca9b8dd94ed1a48e3f867165aa8"
EXPECTED_PATCHED_SYS_TOP_SHA256 = "24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9"
LOCAL_SYS_FILES = {"LICENSE", "PROVENANCE.md"}
EXPECTED_DEVICE = "5CSEBA6U23I7"
EXPECTED_CLOCK_PINS = {
    "FPGA_CLK1_50": "PIN_V11",
    "FPGA_CLK2_50": "PIN_Y13",
    "FPGA_CLK3_50": "PIN_E11",
}
EXPECTED_PLL_BLOBS = {
    "fpga/rtl/pll.qip": "22278c8f7c5338a3b70fb43ddad19d9ca3a86f80",
    "fpga/rtl/pll.v": "6446867ebec35c20f136ce5f321344a6c559b156",
    "fpga/rtl/pll/pll_0002.qip": "aec45eb73ea83ceab9b5ad1b7d71b5869c92036f",
    "fpga/rtl/pll/pll_0002.v": "c599468749fd26ad95fca4593c869fbeace4572f",
}


def object_id(kind: str, body: bytes) -> bytes:
    header = f"{kind} {len(body)}\0".encode("ascii")
    return hashlib.sha1(header + body).digest()


def canonical_blob(path: Path, sys_root: Path) -> bytes:
    # fpga/sys/** is -text in .gitattributes, so the checkout bytes are the
    # pinned upstream blobs, including upstream's deliberate mixed EOL files.
    path.relative_to(sys_root)
    return path.read_bytes()


def tree_id(directory: Path, sys_root: Path, *, root: bool = False) -> bytes:
    entries: list[tuple[bytes, bool, bytes]] = []
    for path in directory.iterdir():
        if root and path.name in LOCAL_SYS_FILES:
            continue
        name = path.name.encode("utf-8")
        if path.is_dir():
            entries.append((name, True, tree_id(path, sys_root)))
        elif path.is_file():
            entries.append((name, False, object_id("blob", canonical_blob(path, sys_root))))

    entries.sort(key=lambda entry: entry[0] + (b"/" if entry[1] else b""))
    body = bytearray()
    for name, is_dir, oid in entries:
        mode = b"40000" if is_dir else b"100644"
        body.extend(mode + b" " + name + b"\0" + oid)
    return object_id("tree", bytes(body))


def require_text(path: Path, snippets: list[str], errors: list[str]) -> str:
    if not path.is_file():
        errors.append(f"missing file: {path}")
        return ""
    data = path.read_bytes()
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        # Quartus 17 writes localized degree symbols through the Windows ANSI
        # code page even when the surrounding report is ASCII.
        text = data.decode("cp1252")
    for snippet in snippets:
        if snippet not in text:
            errors.append(f"{path}: missing required text: {snippet}")
    return text


def verify_sources(repo: Path) -> tuple[list[str], dict[str, object]]:
    errors: list[str] = []
    fpga = repo / "fpga"
    sys_dir = fpga / "sys"

    require_text(
        repo / ".gitattributes",
        [
            "fpga/sys/** -text",
            "fpga/rtl/pll.qip -text",
            "fpga/rtl/pll.v -text",
            "fpga/rtl/pll/** -text",
        ],
        errors,
    )

    require_text(
        repo / "tools" / "board" / "patch_mister_sys_top.py",
        [
            EXPECTED_PATCHED_SYS_TOP_SHA256,
            "assign USER_IO[0] = 1'bZ;",
            "assign USER_IO[6] = 1'bZ;",
            "{1'b0,~lowlat,LFB_EN ? LFB_FLT : |scaler_flt,2'b00}",
        ],
        errors,
    )
    require_text(
        repo / "tools" / "board" / "superstation_build_manifest.py",
        [
            EXPECTED_PATCHED_SYS_TOP_SHA256,
            '"sourceFiles"',
            '"buildInputs"',
            '"artifacts"',
            '"manifestSha256"',
            '"tools/env/zhao-env.ps1"',
            "ARTIFACT_SUFFIXES = (",
            '"sta.summary"',
        ],
        errors,
    )
    require_text(
        repo / "tools" / "board" / "invoke_superstation_probe.ps1",
        [
            "SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU",
            "HostKeyAlgorithms=ssh-ed25519",
            "Assert-BoardIdentity",
            "Assert-BoardState",
            "bridge:lwhps2fpga",
            "bridge:hps2fpga",
            "bridge:fpga2hps",
            "Assert-WatchdogEvidence",
            "watchdog-write-ok",
            "watchdog-menu-ok",
            "superstation_build_manifest.py",
            "BUILD-MANIFEST-V2.json",
            "Physical probe loads require a new explicit -ReceiptPath",
        ],
        errors,
    )
    for script_name in ("build_superstation_bringup.ps1", "build_superstation_specs.ps1"):
        require_text(
            repo / "tools" / "board" / script_name,
            [
                "patch_mister_sys_top.py",
                "sys\\sys_top.v",
                "SuperStation sys_top safety overlay failed",
                "superstation_build_manifest.py",
                "--phase source",
                "--phase complete",
                "'tools/env/zhao-env.ps1'",
                "tools\\env\\zhao-env.ps1",
            ],
            errors,
        )

    if not sys_dir.is_dir():
        errors.append(f"missing directory: {sys_dir}")
        actual_tree = ""
    else:
        actual_tree = tree_id(sys_dir, sys_dir, root=True).hex()
        if actual_tree != EXPECTED_SYS_TREE:
            errors.append(
                "vendored MiSTer sys tree mismatch: "
                f"expected {EXPECTED_SYS_TREE}, got {actual_tree}"
            )

    actual_pll_blobs: dict[str, str] = {}
    for relative, expected in EXPECTED_PLL_BLOBS.items():
        path = repo / relative
        if not path.is_file():
            errors.append(f"missing pinned MiSTer PLL file: {path}")
            continue
        actual = object_id("blob", path.read_bytes()).hex()
        actual_pll_blobs[relative] = actual
        if actual != expected:
            errors.append(
                f"pinned MiSTer PLL blob mismatch for {relative}: "
                f"expected {expected}, got {actual}"
            )

    sys_tcl = require_text(
        sys_dir / "sys.tcl",
        [
            f"set_global_assignment -name DEVICE {EXPECTED_DEVICE}",
            "set_global_assignment -name DEVICE_FILTER_PACKAGE UFBGA",
            "set_global_assignment -name DEVICE_FILTER_PIN_COUNT 672",
            "set_global_assignment -name DEVICE_FILTER_SPEED_GRADE 7",
            'set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to FPGA_CLK1_50',
            'set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to FPGA_CLK2_50',
            'set_instance_assignment -name IO_STANDARD "3.3-V LVTTL" -to FPGA_CLK3_50',
        ],
        errors,
    )
    for signal, pin in EXPECTED_CLOCK_PINS.items():
        assignment = f"set_location_assignment {pin} -to {signal}"
        if assignment not in sys_tcl:
            errors.append(f"sys.tcl: missing clock assignment: {assignment}")

    require_text(
        sys_dir / "sys_top.sdc",
        [
            'create_clock -period "50.0 MHz"  [get_ports FPGA_CLK1_50]',
            'create_clock -period "50.0 MHz"  [get_ports FPGA_CLK2_50]',
            'create_clock -period "50.0 MHz"  [get_ports FPGA_CLK3_50]',
        ],
        errors,
    )

    require_text(
        fpga / "ZhaozhouBringup.qsf",
        [
            "set_global_assignment -name TOP_LEVEL_ENTITY sys_top",
            'set_global_assignment -name RESERVE_ALL_UNUSED_PINS "AS INPUT TRI-STATED"',
            "source sys/sys.tcl",
            "source sys/sys_analog.tcl",
            "source files_bringup.qip",
        ],
        errors,
    )

    require_text(
        fpga / "files_bringup.qip",
        [
            "set_global_assignment -name SDC_FILE ZhaozhouBringup.sdc",
            "set_global_assignment -name SYSTEMVERILOG_FILE rtl/platform/zhao_ssone_bringup.sv",
        ],
        errors,
    )

    core = require_text(
        fpga / "rtl" / "platform" / "zhao_ssone_bringup.sv",
        [
            'assign ADC_BUS = \'z;',
            "assign USER_OUT = '1;",
            "assign {SD_SCK, SD_MOSI, SD_CS} = 'z;",
            ".outclk_0(clk_core)",
            ".locked(pll_locked)",
            "wire reset_request = RESET | !pll_locked | status[0] | buttons[1];",
            "assign CLK_VIDEO = clk_core;",
            "assign CE_PIXEL = ce_pixel_q;",
            "assign AUDIO_L = '0;",
            "assign AUDIO_R = '0;",
        ],
        errors,
    )
    if not re.search(r"assign\s*\{[^;]*SDRAM_DQ[^;]*\}\s*=\s*'z\s*;", core, re.S):
        errors.append("bring-up core does not tri-state the complete SDRAM bundle")
    if not re.search(r"assign\s*\{[^;]*DDRAM_CLK[^;]*\}\s*=\s*'0\s*;", core, re.S):
        errors.append("bring-up core does not hold the complete HPS DDR request bundle inactive")

    summary: dict[str, object] = {
        "sourceStatus": "ok" if not errors else "failed",
        "misterSysTreeExpected": EXPECTED_SYS_TREE,
        "misterSysTreeActual": actual_tree,
        "misterPllBlobsExpected": EXPECTED_PLL_BLOBS,
        "misterPllBlobsActual": actual_pll_blobs,
        "device": EXPECTED_DEVICE,
        "clockPins": EXPECTED_CLOCK_PINS,
    }
    return errors, summary


def verify_patched_sys_top(
    build: Path, errors: list[str], summary: dict[str, object]
) -> None:
    path = build / "sys" / "sys_top.v"
    if not path.is_file():
        errors.append(f"missing patched build-copy sys_top: {path}")
        return
    data = path.read_bytes()
    actual = hashlib.sha256(data).hexdigest()
    summary["patchedSysTop"] = {
        "path": str(path),
        "sha256": actual,
        "expectedSha256": EXPECTED_PATCHED_SYS_TOP_SHA256,
    }
    if actual != EXPECTED_PATCHED_SYS_TOP_SHA256:
        errors.append(
            f"{path}: expected safety-overlay digest "
            f"{EXPECTED_PATCHED_SYS_TOP_SHA256}, got {actual}"
        )
    text = data.decode("utf-8")
    for bit in range(7):
        required = f"assign USER_IO[{bit}] = 1'bZ;"
        if required not in text:
            errors.append(f"{path}: missing unconditional high-Z USER_IO[{bit}]")
    if "SW[1] ? HDMI_" in text:
        errors.append(f"{path}: physical SW[1] can still drive USER/SNAC")
    if "{1'b0,~lowlat,LFB_EN ? LFB_FLT : |scaler_flt,2'b00}" not in text:
        errors.append(f"{path}: scaler mode input is not explicitly 5 bits")


def collect_critical_warnings(
    output: Path, errors: list[str]
) -> list[str]:
    critical: list[str] = []
    zero_summary = re.compile(r"Critical Warnings?\s*[:=;]\s*0\b", re.IGNORECASE)
    for report in sorted(output.glob("*.rpt")):
        report_text = require_text(report, [], errors)
        for line in report_text.splitlines():
            if re.search(r"\bCritical Warning\b", line, re.IGNORECASE) and not zero_summary.search(line):
                critical.append(f"{report.name}: {line.strip()}")
    return critical


def verify_user_io_high_z(
    fit_text: str, fit: Path, errors: list[str], summary: dict[str, object]
) -> None:
    disabled_user_io = sorted(
        {
            int(bit)
            for bit in re.findall(
                r"Pin USER_IO\[(\d)\] has a permanently disabled output enable",
                fit_text,
            )
        }
    )
    if disabled_user_io != list(range(7)):
        errors.append(
            f"{fit}: USER/SNAC high-Z proof is {disabled_user_io}, expected all bits 0..6"
        )
    summary["userIoDisabledOutputEnables"] = disabled_user_io


def verify_build(build: Path, errors: list[str], summary: dict[str, object]) -> None:
    verify_patched_sys_top(build, errors, summary)
    output = build / "output_files"
    flow = output / "ZhaozhouBringup.flow.rpt"
    fit = output / "ZhaozhouBringup.fit.rpt"
    sta = output / "ZhaozhouBringup.sta.rpt"
    pin = output / "ZhaozhouBringup.pin"
    rbf = output / "ZhaozhouBringup.rbf"

    flow_text = require_text(
        flow,
        ["Flow Status", "Top-level Entity Name", EXPECTED_DEVICE],
        errors,
    )
    if flow_text and not re.search(r"Flow Status\s*;\s*Successful", flow_text):
        errors.append(f"{flow}: flow did not report Successful")

    fit_text = require_text(
        fit,
        [EXPECTED_DEVICE, "Quartus Prime Fitter was successful"],
        errors,
    )
    sta_text = require_text(
        sta,
        [EXPECTED_DEVICE, "Quartus Prime TimeQuest Timing Analyzer was successful"],
        errors,
    )
    pin_text = require_text(
        pin,
        [f'CHIP  "ZhaozhouBringup"  ASSIGNED TO AN: {EXPECTED_DEVICE}'],
        errors,
    )

    if not rbf.is_file() or rbf.stat().st_size == 0:
        errors.append(f"missing or empty RBF: {rbf}")
    else:
        summary["rbf"] = {
            "path": str(rbf),
            "bytes": rbf.stat().st_size,
            "sha256": hashlib.sha256(rbf.read_bytes()).hexdigest(),
        }

    critical = collect_critical_warnings(output, errors)
    if critical:
        errors.append(f"critical Quartus warnings present: {critical[:5]}")
    summary["criticalWarnings"] = critical

    if fit_text:
        verify_user_io_high_z(fit_text, fit, errors, summary)

    if pin_text:
        for signal, package_pin in EXPECTED_CLOCK_PINS.items():
            location = package_pin.removeprefix("PIN_")
            pattern = (
                rf"(?m)^{re.escape(signal)}\s*:\s*{re.escape(location)}\s*:\s*"
                rf"input\s*:\s*3\.3-V LVTTL"
            )
            if not re.search(pattern, pin_text):
                errors.append(
                    f"{pin}: missing 3.3-V input {signal} at {package_pin}"
                )
        reserved_outputs = re.findall(
            r"(?m)^RESERVED_OUTPUT[^:\r\n]*\s*:\s*[A-Z]{1,2}\d+",
            pin_text,
        )
        if reserved_outputs:
            errors.append(f"{pin}: unused output-driving pins present: {reserved_outputs[:5]}")
        summary["unusedOutputPins"] = reserved_outputs
        summary["reservedInputPins"] = len(
            re.findall(r"(?m)^RESERVED_INPUT(?:_[A-Z_]+)?\s*:\s*[A-Z]{1,2}\d+", pin_text)
        )

    timing_slacks: dict[str, float] = {}
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
            slack = float(match.group(1))
            timing_slacks[check] = slack
            if slack < 0:
                errors.append(f"{sta}: negative worst-case {check} slack: {slack}")

        for label in ("Illegal Clocks", "Unconstrained Clocks"):
            match = re.search(rf";\s*{re.escape(label)}\s*;\s*(\d+)\s*;", sta_text)
            if not match:
                errors.append(f"{sta}: missing {label} count")
            elif int(match.group(1)) != 0:
                errors.append(f"{sta}: {label} is {match.group(1)}, expected 0")

        unconstrained: dict[str, int] = {}
        for label in (
            "Unconstrained Input Ports",
            "Unconstrained Input Port Paths",
            "Unconstrained Output Ports",
            "Unconstrained Output Port Paths",
        ):
            match = re.search(rf";\s*{re.escape(label)}\s*;\s*(\d+)\s*;", sta_text)
            if match:
                unconstrained[label] = int(match.group(1))
        summary["unconstrainedIo"] = unconstrained
    summary["timingSlacksNs"] = timing_slacks

    if flow_text:
        def flow_value(label: str) -> str | None:
            match = re.search(rf";\s*{re.escape(label)}\s*;\s*([^;]+?)\s*;", flow_text)
            return match.group(1).strip() if match else None

        summary["flow"] = {
            "status": flow_value("Flow Status"),
            "top": flow_value("Top-level Entity Name"),
            "family": flow_value("Family"),
            "device": flow_value("Device"),
            "alms": flow_value("Logic utilization (in ALMs)"),
            "registers": flow_value("Total registers"),
            "pins": flow_value("Total pins"),
            "virtualPins": flow_value("Total virtual pins"),
            "memoryBits": flow_value("Total block memory bits"),
            "dsps": flow_value("Total DSP Blocks"),
            "plls": flow_value("Total PLLs"),
        }

    summary["buildDirectory"] = str(build)
    summary["buildStatus"] = "ok" if not errors else "failed"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Zhaozhou repository root",
    )
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    repo = args.repo.resolve()
    errors, summary = verify_sources(repo)
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
            "SuperStation bring-up verification passed: "
            f"device={EXPECTED_DEVICE} sys_tree={EXPECTED_SYS_TREE}"
        )
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
