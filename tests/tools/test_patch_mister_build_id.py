from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
PATCH_PATH = REPO / "tools" / "board" / "patch_mister_build_id.py"
SPEC = importlib.util.spec_from_file_location("patch_mister_build_id", PATCH_PATH)
assert SPEC is not None and SPEC.loader is not None
PATCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCH)


class MisterBuildIdPatchTest(unittest.TestCase):
    def test_pinned_input_produces_exact_projectless_output(self) -> None:
        upstream = (REPO / "fpga" / "sys" / "build_id.tcl").read_bytes()
        patched = PATCH.patch_bytes(upstream)
        self.assertEqual(PATCH.sha256(upstream), PATCH.UPSTREAM_SHA256)
        self.assertEqual(PATCH.sha256(patched), PATCH.PATCHED_SHA256)
        self.assertNotIn(b"project_open", patched)
        self.assertNotIn(b"project_close", patched)
        self.assertNotIn(b"get_global_assignment", patched)
        self.assertIn(b"set revision [lindex $quartus(args) 0]", patched)
        self.assertIn(b"set device   [lindex $quartus(args) 1]", patched)
        self.assertIn(b"set outpath  [lindex $quartus(args) 2]", patched)

    def test_mutated_upstream_digest_is_rejected(self) -> None:
        upstream = (REPO / "fpga" / "sys" / "build_id.tcl").read_bytes()
        with self.assertRaisesRegex(ValueError, "not the pinned upstream blob"):
            PATCH.patch_bytes(upstream + b"mutant")

    def test_missing_anchor_is_rejected(self) -> None:
        old_digest = PATCH.UPSTREAM_SHA256
        PATCH.UPSTREAM_SHA256 = PATCH.sha256(b"no anchor")
        try:
            with self.assertRaisesRegex(ValueError, "anchor count is 0"):
                PATCH.patch_bytes(b"no anchor")
        finally:
            PATCH.UPSTREAM_SHA256 = old_digest

    def test_double_patch_is_rejected(self) -> None:
        upstream = (REPO / "fpga" / "sys" / "build_id.tcl").read_bytes()
        patched = PATCH.patch_bytes(upstream)
        with self.assertRaisesRegex(ValueError, "not the pinned upstream blob"):
            PATCH.patch_bytes(patched)


if __name__ == "__main__":
    unittest.main()
