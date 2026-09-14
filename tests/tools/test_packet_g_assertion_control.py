#!/usr/bin/env python3
"""Bound the Packet-G unreachable CDC full-guard assertion control."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


EXPECTED = "fb_ready_cdc_v2: READY FIFO RAM ownership exceeded three"


def as_text(value: str | bytes | None) -> str:
    if value is None:
        return ""
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    executable = args.exe.resolve()
    if not executable.is_file():
        parser.error("--exe must name the built assertion-control executable")

    timed_out = False
    try:
        completed = subprocess.run(
            [str(executable)], capture_output=True, text=True, errors="replace",
            timeout=15, check=False,
        )
        diagnostic = (completed.stdout or "") + (completed.stderr or "")
        returncode = completed.returncode
    except subprocess.TimeoutExpired as exc:
        timed_out = True
        diagnostic = as_text(exc.stdout) + as_text(exc.stderr)
        returncode = None

    if (EXPECTED not in diagnostic or "Verilog $stop" not in diagnostic
            or diagnostic.count("Assertion failed") != 1):
        print(
            "FAIL: Packet-G full-guard assertion control missed its exact "
            f"diagnostic; timeout={timed_out} rc={returncode}\n{diagnostic[-4000:]}",
            file=sys.stderr,
        )
        return 1
    if not timed_out and returncode == 0:
        print("FAIL: Packet-G assertion diagnostic returned success", file=sys.stderr)
        return 1
    print("PACKET_G_ASSERTION_CONTROL[cdc-ignore-full] FIRED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
