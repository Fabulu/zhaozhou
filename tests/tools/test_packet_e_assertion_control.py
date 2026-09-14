#!/usr/bin/env python3
"""Bound the Packet-E unreachable reservation assertion positive control."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


EXPECTED = (
    "cache_pipe: reservation identity failed rs_resv=2 expected=1 "
    "(rs=0 c1o=0 c2o=0 fill=0 prepaid=0/1/0)"
)


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
            "FAIL: Packet-E double-reservation assertion control missed its exact "
            f"diagnostic; timeout={timed_out} rc={returncode}\n{diagnostic[-4000:]}",
            file=sys.stderr,
        )
        return 1
    if not timed_out and returncode == 0:
        print("FAIL: Packet-E assertion diagnostic returned success", file=sys.stderr)
        return 1
    print("PACKET_E_ASSERTION_CONTROL[double-reservation] FIRED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
