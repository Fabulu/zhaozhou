"""The fault-OR parity check must PASS on the tree and FIRE on divergence.

The thing being guarded is a hand-written copy: `bin_fault_w` in the harness
lists the same structural faults as `v2_fault_level_c` in the shell. The
failure mode is that one grows a term and the other does not, and NOTHING GOES
RED -- the directed checks never read the list, so a harness attributing four
faults where the shell attributes six passes exactly as cleanly.

So the controls are about the instrument. The last one is the one that earns
its place: if the extractor ever stops finding either expression it returns two
empty sets, and two empty sets compare EQUAL, so the tool would pass for the
worst possible reason. It must fail instead.
"""
import io
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools' / 'design'))

import packet_h_fault_or_parity as parity  # noqa: E402

SHELL = """module zhao_shell_top_v2 ();
  assign v2_fault_level_c = v2_bin_frame_fault_w || v2_bin_raster_abort_w ||
                            v2_cdc_gpu_fault_w;
endmodule
"""

HARNESS = """module zhao_shell_v2_lease_path ();
  assign bin_fault_w = bin_frame_fault_o || bin_raster_abort_o ||
                       cdc_gpu_protocol_fault_o;
endmodule
"""


def _pair(tmp, shell=SHELL, harness=HARNESS):
    root = Path(tmp)
    s, h = root / 'shell.sv', root / 'harness.sv'
    with io.open(s, 'w', encoding='utf-8') as fh:
        fh.write(shell)
    with io.open(h, 'w', encoding='utf-8') as fh:
        fh.write(harness)
    return parity.main(['--shell', str(s), '--harness', str(h)])


class FaultOrParityTests(unittest.TestCase):
    def test_passes_on_the_real_tree(self):
        self.assertEqual(parity.main([]), 0)

    def test_passes_when_both_lists_agree(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(_pair(tmp), 0)

    def test_fires_when_the_shell_gains_a_term(self):
        # The direction that matters: the machine that SHIPS attributes a
        # fault the harness never sees, so every directed check stays green
        # while measuring a weaker machine.
        shell = SHELL.replace('v2_cdc_gpu_fault_w;',
                              'v2_cdc_gpu_fault_w || v2_bin_attr_abort_w;')
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(_pair(tmp, shell=shell), 1)

    def test_fires_when_the_harness_gains_a_term(self):
        harness = HARNESS.replace('cdc_gpu_protocol_fault_o;',
                                  'cdc_gpu_protocol_fault_o || bin_attr_abort_o;')
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(_pair(tmp, harness=harness), 1)

    def test_does_not_fire_on_a_spelling_difference(self):
        # The control's own control. The two files spell the same fault
        # differently on purpose, and if normalisation broke, every run would
        # fail -- which would get "fixed" by weakening the comparison.
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(_pair(tmp), 0)
        self.assertEqual(parity.normalise('v2_cdc_gpu_fault_w'),
                         parity.normalise('cdc_gpu_protocol_fault_o'))

    def test_fires_when_an_expression_cannot_be_FOUND(self):
        # Two empty sets compare equal. If the extractor stops matching, the
        # tool must refuse rather than report agreement.
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(
                _pair(tmp, shell='module x (); endmodule\n'), 1,
                'the tool reported parity between nothing and nothing')


if __name__ == '__main__':
    unittest.main(verbosity=2)
