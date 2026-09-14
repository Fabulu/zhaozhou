#!/usr/bin/env python3
"""Run one exact Packet-G dual-selector control with a hard timeout."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools" / "rtl"))
import texture_v3_interface_parser as interface


def manifest(name: str) -> tuple[str, ...]:
    path = REPO / "tests" / "video" / name
    rows = path.read_text(encoding="utf-8").splitlines()
    if not rows or any(not row or "\\" in row for row in rows):
        raise RuntimeError(f"invalid Packet-G source manifest: {path}")
    return tuple(rows)


PROFILES = {
    "slot-sv": {
        "kind": "sv",
        "top": "zhao_video_slotmgr_v2",
        "defines": (
            "ZHAO_SLOT_V2_MUTANT_TERM_OMIT_WRITER",
            "ZHAO_SLOT_V2_MUTANT_TERM_SLOT_ONLY",
        ),
        "sources": manifest("packet_g_slotmgr_v2.sources.txt"),
        "diagnostic": "ZHAO_VIDEO_SLOTMGR_V2_MUTANT_SELECTOR_COLLISION",
    },
    "slot-cpp": {
        "kind": "cpp",
        "defines": (
            "ZHAO_EXPECT_SLOT_MUTANT_TERM_OMIT_WRITER",
            "ZHAO_EXPECT_SLOT_MUTANT_TERM_SLOT_ONLY",
        ),
        "source": "tests/video/video_slotmgr_v2_directed.cpp",
        "diagnostic": "ZHAO_VIDEO_SLOTMGR_V2_CPP_MUTANT_SELECTOR_COLLISION",
    },
    "cdc-sv": {
        "kind": "sv",
        "top": "zhao_fb_ready_cdc_v2",
        "defines": (
            "ZHAO_FB_CDC_MUTANT_IGNORE_FULL",
            "ZHAO_FB_CDC_MUTANT_BYPASS_BARRIER",
        ),
        "sources": manifest("packet_g_cdc_v2.sources.txt"),
        "diagnostic": "ZHAO_FB_READY_CDC_V2_MUTANT_SELECTOR_COLLISION",
    },
    "cdc-cpp": {
        "kind": "cpp",
        "defines": (
            "ZHAO_EXPECT_CDC_MUTANT_IGNORE_FULL",
            "ZHAO_EXPECT_CDC_MUTANT_BYPASS_BARRIER",
        ),
        "source": "tests/video/fb_ready_cdc_v2_directed.cpp",
        "diagnostic": "ZHAO_FB_READY_CDC_V2_CPP_MUTANT_SELECTOR_COLLISION",
    },
}


def run(profile: str, verilator: Path, cxx: Path) -> int:
    spec = PROFILES[profile]
    try:
        if spec["kind"] == "sv":
            with tempfile.TemporaryDirectory(prefix=f"packet-g-{profile}-") as temporary:
                command = [
                    str(verilator), "--lint-only", "--Mdir", temporary,
                    "--top-module", str(spec["top"]),
                    *(f"-D{define}" for define in spec["defines"]),
                    *(str(REPO / source) for source in spec["sources"]),
                ]
                completed = subprocess.run(
                    command, cwd=REPO,
                    env=interface.verilator_environment(verilator, REPO),
                    capture_output=True, text=True, errors="replace", timeout=120,
                    check=False,
                )
        else:
            command = [
                str(cxx), "-E", "-x", "c++",
                *(f"-D{define}" for define in spec["defines"]),
                str(REPO / str(spec["source"])),
            ]
            completed = subprocess.run(
                command, cwd=REPO, capture_output=True, text=True,
                errors="replace", timeout=30, check=False,
            )
    except subprocess.TimeoutExpired:
        print(f"FAIL: Packet-G selector control {profile} timed out", file=sys.stderr)
        return 1

    diagnostic = (completed.stdout or "") + (completed.stderr or "")
    if completed.returncode == 0:
        print(f"FAIL: Packet-G selector control {profile} unexpectedly compiled", file=sys.stderr)
        return 1
    if str(spec["diagnostic"]) not in diagnostic:
        print(
            f"FAIL: Packet-G selector control {profile} missed exact diagnostic; "
            f"rc={completed.returncode}\n{diagnostic[-4000:]}",
            file=sys.stderr,
        )
        return 1
    print(f"PACKET_G_SELECTOR_COLLISION[{profile}] FIRED")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profile", choices=tuple(PROFILES), required=True)
    parser.add_argument("--verilator", type=Path, required=True)
    parser.add_argument("--cxx", type=Path, required=True)
    args = parser.parse_args()
    if not args.verilator.is_file() or not args.cxx.is_file():
        parser.error("--verilator and --cxx must name existing executables")
    return run(args.profile, args.verilator.resolve(), args.cxx.resolve())


if __name__ == "__main__":
    raise SystemExit(main())
