#!/usr/bin/env python3
"""Build/check the first combined post-CRC timing-recovery G8A receipt."""

from pathlib import Path

import g8a_receipt as receipt


REPO = Path(__file__).resolve().parents[2]
receipt.ROW_NAME = receipt.MODULE + "@g8a-timing1"
# This is snapshotted in memory before Quartus and published only after the
# measured run verifies the generated authority did not move. After the raw run
# is archived, rebind this path to the immutable retained attempt manifest.
receipt.FIT_MANIFEST = (
    REPO / "reports/synthesis/blockpaths/"
    "zhao_raster_texture_v3_fit_top@g8a-timing1.fit.manifest.json"
)
receipt.RECEIPT = (
    REPO / "reports/synthesis/zhao_g8a_raster_texture_timing1.json"
)


if __name__ == "__main__":
    raise SystemExit(receipt.main())
