#!/usr/bin/env python3
"""Bound the V3-owner bypass-pointer assertion positive control."""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import subprocess
import sys


EXPECTED_LABEL = "a_out_structure"
EXPECTED_CONTROL = "V3OWN_BYPASS_POINTER_ASSERT_CONTROL"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    args = parser.parse_args()
    executable = args.exe.resolve()
    if not executable.is_file():
        parser.error("--exe must name the built V3-owner assertion-control executable")

    try:
        completed = subprocess.run(
            [str(executable)], capture_output=True, text=True, errors="replace",
            timeout=15, check=False,
        )
    except subprocess.TimeoutExpired as exc:
        diagnostic = (exc.stdout or "") + (exc.stderr or "")
        print("FAIL: V3-owner assertion control timed out\n" + diagnostic[-4000:],
              file=sys.stderr)
        return 1

    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    labels = re.findall(r"Assertion failed in [^:\r\n]*\.([A-Za-z0-9_]+):", diagnostic)
    if (completed.returncode != 0 or labels != [EXPECTED_LABEL]
            or EXPECTED_CONTROL not in diagnostic
            or "fired=1 emitted=0" not in diagnostic
            or "FAIL:" in diagnostic):
        print(
            "FAIL: V3-owner bypass-pointer control missed its exact assertion; "
            f"rc={completed.returncode} labels={labels!r}\n{diagnostic[-4000:]}",
            file=sys.stderr,
        )
        return 1
    print("V3OWN_ASSERTION_CONTROL[bypass-pointer] FIRED a_out_structure")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
