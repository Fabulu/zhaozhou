from __future__ import annotations

import copy
import importlib.util
import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
VALIDATOR_PATH = REPO / "tools" / "board" / "validate_board_truth.py"
SPEC = importlib.util.spec_from_file_location("validate_board_truth", VALIDATOR_PATH)
assert SPEC is not None and SPEC.loader is not None
VALIDATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VALIDATOR)
BOARD_TRUTH = json.loads((REPO / "reports" / "board_truth.json").read_text("utf-8"))


class BoardTruthValidationTest(unittest.TestCase):
    def test_current_partial_truth_passes(self) -> None:
        self.assertEqual(VALIDATOR.validate(copy.deepcopy(BOARD_TRUTH), REPO), [])

    def test_premature_sdram_timing_claim_fires(self) -> None:
        data = copy.deepcopy(BOARD_TRUTH)
        data["memory"]["fpgaSdram"]["timingStatus"] = "measured"

        errors = VALIDATOR.validate(data, REPO)

        self.assertTrue(any("memory.fpgaSdram.timingStatus" in error for error in errors))

    def test_flash_access_claim_fires(self) -> None:
        data = copy.deepcopy(BOARD_TRUTH)
        data["fpga"]["volatileLoad"]["configurationFlashTouched"] = True

        errors = VALIDATOR.validate(data, REPO)

        self.assertTrue(any("configurationFlashTouched" in error for error in errors))

    def test_dropping_open_capability_fires(self) -> None:
        data = copy.deepcopy(BOARD_TRUTH)
        data["openCapabilities"].remove("full_zhaozhou_shell")

        errors = VALIDATOR.validate(data, REPO)

        self.assertTrue(any("openCapabilities mismatch" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
