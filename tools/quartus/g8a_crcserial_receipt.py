#!/usr/bin/env python3
"""Build/check the one post-CRC-serialization G8A receipt."""

from pathlib import Path

import g8a_receipt as receipt


REPO = Path(__file__).resolve().parents[2]
receipt.ROW_NAME = receipt.MODULE + "@g8a-crcserial"
receipt.FIT_MANIFEST = (
    REPO / "fpga/rtl/generated/zhao_raster_texture_v3_fit_top.manifest.json"
)
receipt.RECEIPT = (
    REPO / "reports/synthesis/zhao_g8a_raster_texture_crcserial.json"
)


if __name__ == "__main__":
    raise SystemExit(receipt.main())
