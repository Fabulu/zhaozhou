#!/usr/bin/env python3
"""Apply the SuperStation safety overlay to a copied MiSTer sys_top.v.

The pinned vendor tree remains byte-identical. Board builds copy it into their
owned workspace, then run this exact two-anchor transform before Quartus.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

UPSTREAM_SHA256 = "9bc5562bcc9d923aa3bff1a9c976c52492edb9ef981f428b7a4101c919a711b8"
PATCHED_SHA256 = "24eea7b0f76848239c872f626a48f4e0c6150423b9e6561fd3dd63f2a99501e9"

SCALER_ORIGINAL = b"\t\t.mode     ({~lowlat,LFB_EN ? LFB_FLT : |scaler_flt,2'b00}),\n"
SCALER_SAFE = b"\t\t.mode     ({1'b0,~lowlat,LFB_EN ? LFB_FLT : |scaler_flt,2'b00}),\n"

USER_IO_ORIGINAL = b"""assign USER_IO[0] =                       !user_out[0]  ? 1'b0 : 1'bZ;
assign USER_IO[1] =                       !user_out[1]  ? 1'b0 : 1'bZ;
assign USER_IO[2] = !(SW[1] ? HDMI_I2S   : user_out[2]) ? 1'b0 : 1'bZ;
assign USER_IO[3] =                       !user_out[3]  ? 1'b0 : 1'bZ;
assign USER_IO[4] = !(SW[1] ? HDMI_SCLK  : user_out[4]) ? 1'b0 : 1'bZ;
assign USER_IO[5] = !(SW[1] ? HDMI_LRCLK : user_out[5]) ? 1'b0 : 1'bZ;
assign USER_IO[6] =                       !user_out[6]  ? 1'b0 : 1'bZ;

assign user_in[0] =         USER_IO[0];
assign user_in[1] =         USER_IO[1];
assign user_in[2] = SW[1] | USER_IO[2];
assign user_in[3] =         USER_IO[3];
assign user_in[4] = SW[1] | USER_IO[4];
assign user_in[5] = SW[1] | USER_IO[5];
assign user_in[6] =         USER_IO[6];
"""

USER_IO_SAFE = b"""// Zhaozhou SuperStation safety overlay: USER/SNAC is never driven.
// A peripheral may be attached and physical SW[1] is outside this image's
// control, so even the MiSTer audio-multiplex behavior is prohibited here.
assign USER_IO[0] = 1'bZ;
assign USER_IO[1] = 1'bZ;
assign USER_IO[2] = 1'bZ;
assign USER_IO[3] = 1'bZ;
assign USER_IO[4] = 1'bZ;
assign USER_IO[5] = 1'bZ;
assign USER_IO[6] = 1'bZ;

assign user_in[0] = USER_IO[0];
assign user_in[1] = USER_IO[1];
assign user_in[2] = USER_IO[2];
assign user_in[3] = USER_IO[3];
assign user_in[4] = USER_IO[4];
assign user_in[5] = USER_IO[5];
assign user_in[6] = USER_IO[6];
"""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def replace_exact(data: bytes, original: bytes, replacement: bytes, label: str) -> bytes:
    count = data.count(original)
    if count != 1:
        raise ValueError(f"{label} anchor count is {count}, expected exactly 1")
    return data.replace(original, replacement, 1)


def patch_bytes(data: bytes) -> bytes:
    actual = sha256(data)
    if actual != UPSTREAM_SHA256:
        raise ValueError(
            "sys_top input is not the pinned upstream blob: "
            f"expected {UPSTREAM_SHA256}, got {actual}"
        )
    data = replace_exact(data, SCALER_ORIGINAL, SCALER_SAFE, "scaler mode")
    data = replace_exact(data, USER_IO_ORIGINAL, USER_IO_SAFE, "USER/SNAC")
    actual_patched = sha256(data)
    if actual_patched != PATCHED_SHA256:
        raise ValueError(
            "patched sys_top digest drifted: "
            f"expected {PATCHED_SHA256}, got {actual_patched}"
        )
    return data


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path, help="copied build-workspace sys_top.v")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    try:
        before = args.path.read_bytes()
        after = patch_bytes(before)
        args.path.write_bytes(after)
    except (OSError, ValueError) as exc:
        print(f"SUPERSTATION_SYS_TOP_PATCH_FAIL: {exc}", file=sys.stderr)
        return 1

    result = {
        "status": "ok",
        "path": str(args.path),
        "upstreamSha256": sha256(before),
        "patchedSha256": sha256(after),
        "patches": [
            "ascal-mode-leading-zero-4-to-5-bit",
            "user-snac-unconditional-high-impedance",
        ],
    }
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        print(
            "SuperStation sys_top safety overlay applied: "
            f"{result['patchedSha256']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
