#!/usr/bin/env python3
"""Build/check the first combined post-CRC timing-recovery G8A receipt."""

from pathlib import Path

import g8a_receipt as receipt


REPO = Path(__file__).resolve().parents[2]
receipt.ROW_NAME = receipt.MODULE + "@g8a-timing1"
# The runner snapshotted these bytes before Quartus, verified them after the
# measured run, and atomically retained them before receipt derivation. This path
# now names the immutable archived copy rather than the moving blockpath output.
receipt.FIT_MANIFEST = (
    REPO / "reports/characterization/g8a_raster_texture_single_owner_characterization/"
    "8908bc6f-20260915T003314Z-timing1/fit.manifest.json"
)
receipt.RECEIPT = (
    REPO / "reports/synthesis/zhao_g8a_raster_texture_timing1.json"
)


if __name__ == "__main__":
    raise SystemExit(receipt.main())
