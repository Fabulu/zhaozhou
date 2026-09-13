from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
PATCH_PATH = REPO / "tools" / "board" / "patch_mister_sys_top.py"
SPEC = importlib.util.spec_from_file_location("patch_mister_sys_top", PATCH_PATH)
assert SPEC is not None and SPEC.loader is not None
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)
UPSTREAM = (REPO / "fpga" / "sys" / "sys_top.v").read_bytes()


class MisterSysTopPatchTest(unittest.TestCase):
    def test_pinned_input_and_safe_output(self) -> None:
        self.assertEqual(PATCH.sha256(UPSTREAM), PATCH.UPSTREAM_SHA256)

        patched = PATCH.patch_bytes(UPSTREAM)

        self.assertEqual(PATCH.sha256(patched), PATCH.PATCHED_SHA256)
        self.assertIn(PATCH.SCALER_SAFE, patched)
        self.assertNotIn(PATCH.SCALER_ORIGINAL, patched)
        self.assertIn(PATCH.USER_IO_SAFE, patched)
        self.assertNotIn(PATCH.USER_IO_ORIGINAL, patched)
        for bit in range(7):
            self.assertIn(f"assign USER_IO[{bit}] = 1'bZ;".encode(), patched)

    def test_mutated_upstream_blob_fires(self) -> None:
        with self.assertRaisesRegex(ValueError, "not the pinned upstream blob"):
            PATCH.patch_bytes(UPSTREAM + b"\n// committed mutant\n")

    def test_anchor_loss_fires_after_hash_positive_control(self) -> None:
        mutant = UPSTREAM.replace(PATCH.SCALER_ORIGINAL, b"// scaler anchor removed\n", 1)
        old_hash = PATCH.UPSTREAM_SHA256
        PATCH.UPSTREAM_SHA256 = PATCH.sha256(mutant)
        self.addCleanup(setattr, PATCH, "UPSTREAM_SHA256", old_hash)

        with self.assertRaisesRegex(ValueError, "scaler mode anchor count is 0"):
            PATCH.patch_bytes(mutant)

    def test_double_patch_fires(self) -> None:
        patched = PATCH.patch_bytes(UPSTREAM)
        with self.assertRaisesRegex(ValueError, "not the pinned upstream blob"):
            PATCH.patch_bytes(patched)


if __name__ == "__main__":
    unittest.main()
