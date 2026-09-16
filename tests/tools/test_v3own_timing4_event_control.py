#!/usr/bin/env python3
"""Bound Timing4 V3-owner event-identity assertion controls."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parents[2]
BANK = REPO / "fpga/rtl/texture/zhao_texture_v3bank.sv"
RQ = REPO / "fpga/rtl/texture/zhao_texture_v3rq.sv"
MUTANTS = REPO / "tests/mutants/zhao_texture_v3own_timing4_event_mutants.sv"
OWNER = REPO / "fpga/rtl/texture/zhao_texture_v3own.sv"


def run_control(executable: Path, expected: str) -> int:
    try:
        # The adversarial owner suite is long; the mutant build is slower still
        # because every fired assertion is formatted. A tight bound here would
        # report a timing artifact as a missing assertion.
        completed = subprocess.run(
            [str(executable.resolve())], capture_output=True, text=True,
            errors="replace", timeout=300, check=False)
    except subprocess.TimeoutExpired as exc:
        diagnostic = ((exc.stdout or "") + (exc.stderr or ""))
        print("FAIL: Timing4 owner event control timed out\n" + diagnostic[-4000:],
              file=sys.stderr)
        return 1
    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    labels = re.findall(
        r"Assertion failed in [^:\r\n]*\.([A-Za-z0-9_]+):", diagnostic)
    if not labels or set(labels) != {expected} or "FAIL:" in diagnostic:
        print(
            "FAIL: Timing4 owner event control missed its exact assertion; "
            f"rc={completed.returncode} labels={labels!r}\n{diagnostic[-4000:]}",
            file=sys.stderr,
        )
        return 1
    print(f"V3OWN_TIMING4_EVENT_CONTROL FIRED {expected}")
    return 0


def collision(verilator: Path) -> int:
    with tempfile.TemporaryDirectory(prefix="v3own-t4-collision-") as temporary:
        completed = subprocess.run(
            [
                str(verilator.resolve()), "--lint-only", "--Mdir", temporary,
                "--top-module", "zhao_texture_v3own",
                "-DZHAO_V3OWN_T4_MUTANT_ADMISSION_STALE",
                "-DZHAO_V3OWN_T4_MUTANT_RESERVATION_STALE",
                str(BANK), str(RQ), str(MUTANTS), str(OWNER),
            ],
            cwd=REPO, env=os.environ.copy(), capture_output=True, text=True,
            errors="replace", timeout=120, check=False,
        )
    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    expected = "ZHAO_V3OWN_T4_EVENT_MUTANT_SELECTOR_COLLISION"
    if completed.returncode == 0 or expected not in diagnostic:
        print(
            f"FAIL: Timing4 owner selector collision rc={completed.returncode}\n"
            f"{diagnostic[-4000:]}", file=sys.stderr)
        return 1
    print("V3OWN_TIMING4_EVENT_SELECTOR_COLLISION FIRED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--exe", type=Path)
    mode.add_argument("--collision", action="store_true")
    parser.add_argument("--expected", choices=(
        "a_admission_event_identity", "a_reservation_event_identity"))
    parser.add_argument("--verilator", type=Path)
    args = parser.parse_args()
    if args.exe is not None:
        if not args.exe.is_file() or args.expected is None:
            parser.error("--exe requires an existing file and --expected")
        return run_control(args.exe, args.expected)
    if args.verilator is None or not args.verilator.is_file():
        parser.error("--collision requires --verilator")
    return collision(args.verilator)


if __name__ == "__main__":
    raise SystemExit(main())
