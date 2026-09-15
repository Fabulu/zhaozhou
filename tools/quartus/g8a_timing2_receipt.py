#!/usr/bin/env python3
"""Build/check the second combined post-CRC timing-recovery G8A receipt."""

from pathlib import Path

import g8a_receipt as receipt


REPO = Path(__file__).resolve().parents[2]
receipt.ROW_NAME = receipt.MODULE + "@g8a-timing2"
# The runner snapshots these bytes before Quartus, verifies them after the
# measured run, and atomically retains them before receipt derivation. Rebind
# this to the immutable archive copy after the completed attempt is preserved.
receipt.FIT_MANIFEST = (
    REPO / "reports/synthesis/blockpaths/"
    "zhao_raster_texture_v3_fit_top@g8a-timing2.fit.manifest.json"
)
receipt.RECEIPT = (
    REPO / "reports/synthesis/zhao_g8a_raster_texture_timing2.json"
)

_BASE_VALIDATE_RAM = receipt.validate_ram


def validate_timing2_ram(map_text: str) -> dict[str, object]:
    """Require the two ordered-retirement body payloads to remain inferred RAM."""
    result = _BASE_VALIDATE_RAM(map_text)
    hierarchies = [
        row["hierarchy"].lower() for row in receipt.parse_entity_rows(map_text)
    ]
    owner = "|zhao_texture_v3own:u_own|altsyncram:"
    result_ram = any(owner + "oq_res_q_rtl_0" in row for row in hierarchies)
    context_ram = any(owner + "oq_ctx_q_rtl_0" in row for row in hierarchies)
    result["ordered_retirement_result_ram_present"] = result_ram
    result["ordered_retirement_context_ram_present"] = context_ram
    result["pass"] = bool(result["pass"] and result_ram and context_ram)
    return result


def main() -> int:
    receipt.validate_ram = validate_timing2_ram
    return receipt.main()


if __name__ == "__main__":
    raise SystemExit(main())
