"""The Packet-H sibling diff must PASS on the tree and FIRE on drift.

`packet_h_sibling_diff.py` asserts that the eighteen instances the sibling
shell carried over from the protected V1 have not drifted. It has only ever
returned PASS, and this file exists because a checker that has only ever
returned PASS is a claim.

The controls build synthetic pairs of shells in a temp directory rather than
editing production RTL -- the live-tree hazard, and it would leave no evidence
behind either. Each pair is small: two instances, one declared as changed and
one carried over untouched, which is the real file's shape in miniature. The
DECLARED table is swapped for a two-row one to match, because the real table's
seventeen markers cannot all appear in a four-line shell.

The important control is `test_does_not_fire_on_a_comment_...`. A tool that
fires on any edit at all would pass the drift test for the wrong reason, so a
comment added to the carried-over instance must NOT fire. That is the
distinction the whole tool rests on, and it is the one worth a test.
"""
import hashlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools' / 'design'))

import packet_h_sibling_diff as diff  # noqa: E402

MARKER = 'zhao_geom_bin_pipe_v2 u_render_bin'

SYNTHETIC_DECLARED = [
    (MARKER, 'the bin pipe swap'),
    ('module zhao_shell_top_v2', 'the module rename'),
]

SEED = """module zhao_shell_top ();
  zhao_geom_bin_pipe u_render_bin (
    .clk(gpu_clk),
    .tri_count_i(tri_count)
  );
  zhao_debug_frameblit u_frameblit (
    .clk(gpu_clk),
    .blit_base_i(blit_base)
  );
endmodule : zhao_shell_top
"""

# The sibling as Packet H would legitimately produce it: the bin pipe swapped
# for its V2 (declared), the frameblit carried over byte-for-byte.
CLEAN = (SEED
         .replace('zhao_geom_bin_pipe u_render_bin', MARKER)
         .replace('module zhao_shell_top ()', 'module zhao_shell_top_v2 ()')
         .replace('endmodule : zhao_shell_top', 'endmodule : zhao_shell_top_v2'))


def _pair(tmp, sibling):
    """Write a seed and a sibling, and return (dir, the seed's real hash)."""
    root = Path(tmp)
    seed = root / diff.V1_NAME
    with io.open(seed, 'w', encoding='utf-8', newline='') as fh:
        fh.write(SEED)
    with io.open(root / diff.V2_NAME, 'w', encoding='utf-8', newline='') as fh:
        fh.write(sibling)
    return root, hashlib.sha256(seed.read_bytes()).hexdigest()


def _run(root, seed_hash):
    return diff.main(['--rtl', str(root), '--expect-seed-hash', seed_hash])


class SiblingDiffTests(unittest.TestCase):
    def setUp(self):
        self._real = diff.DECLARED
        diff.DECLARED = SYNTHETIC_DECLARED

    def tearDown(self):
        diff.DECLARED = self._real

    def test_passes_on_the_real_tree(self):
        diff.DECLARED = self._real
        self.assertEqual(diff.main([]), 0)

    def test_passes_on_a_clean_synthetic_sibling(self):
        with tempfile.TemporaryDirectory() as tmp:
            root, h = _pair(tmp, CLEAN)
            self.assertEqual(_run(root, h), 0)

    def test_fires_on_drift_in_a_carried_over_instance(self):
        # One port of the untouched frameblit rewired. Nothing in DECLARED
        # names it, so it must be reported.
        drifted = CLEAN.replace('.blit_base_i(blit_base)',
                                '.blit_base_i(blit_base_v2)')
        self.assertNotEqual(drifted, CLEAN, 'the control edited nothing')
        with tempfile.TemporaryDirectory() as tmp:
            root, h = _pair(tmp, drifted)
            self.assertEqual(
                _run(root, h), 1,
                'the diff did not fire on a rewired carried-over instance')

    def test_does_not_fire_on_a_comment_in_a_carried_over_instance(self):
        # The control's own control. Packet H added `// TIE:` reasons to
        # inherited tie-offs; if those counted as drift the tool would be
        # unusable, and the drift test above would pass merely because
        # something changed rather than because BEHAVIOUR changed.
        commented = CLEAN.replace(
            '.blit_base_i(blit_base)',
            '.blit_base_i(blit_base)  // TIE: inherited from V1')
        self.assertNotEqual(commented, CLEAN, 'the control edited nothing')
        with tempfile.TemporaryDirectory() as tmp:
            root, h = _pair(tmp, commented)
            self.assertEqual(_run(root, h), 0,
                             'a comment was misreported as drift')

    def test_fires_when_the_seed_has_moved(self):
        # The comparison is only meaningful against the protected baseline.
        with tempfile.TemporaryDirectory() as tmp:
            root, _ = _pair(tmp, CLEAN)
            self.assertEqual(_run(root, '00' * 32), 1,
                             'the diff ran against an unverified seed')

    def test_fires_on_a_dead_declared_marker(self):
        # A permission whose marker matches nothing grants nothing while
        # looking like it grants something -- and the region it was written to
        # cover is then unguarded. Here the sibling never swaps the bin pipe,
        # so MARKER is absent from it.
        stale = CLEAN.replace(MARKER, 'zhao_geom_bin_pipe u_render_bin')
        self.assertNotIn(MARKER, stale)
        with tempfile.TemporaryDirectory() as tmp:
            root, h = _pair(tmp, stale)
            self.assertEqual(_run(root, h), 1,
                             'a declared marker matching nothing was accepted')


if __name__ == '__main__':
    unittest.main(verbosity=2)
