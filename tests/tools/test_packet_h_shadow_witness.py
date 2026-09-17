"""The Packet-H shadow witness must PASS on the tree and FIRE on a broken one.

`packet_h_shadow_witness.py` checks a gate clause -- nested V3 elaborates with
no migration shadows -- and a checker that has only ever returned PASS is a
claim, not evidence. This runs it twice: once against the real RTL, and once
against a synthetic tree where a third instantiation turns shadows on, where it
must return non-zero.

The synthetic tree is built in a temp directory, so firing the detector never
puts `MIGRATION_SHADOWS=1'b1` into the working tree even briefly.
"""
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools' / 'design'))

import packet_h_shadow_witness as witness  # noqa: E402

REAL_RTL = REPO / 'fpga' / 'rtl'


def _synthetic(tmp, extra_value):
    """A minimal tree the witness can read, with one extra override site."""
    rtl = Path(tmp) / 'rtl'
    (rtl / 'texture').mkdir(parents=True)
    (rtl / 'raster').mkdir(parents=True)
    io.open(rtl / 'texture' / 'zhao_texture_island_v3_top.sv', 'w',
            encoding='utf-8').write(
        "module zhao_texture_island_v3_top #(\n"
        "  parameter bit MIGRATION_SHADOWS = 1'b1,\n"
        ") ();\nendmodule\n")
    io.open(rtl / 'raster' / 'zhao_raster_tile_pipe_v2.sv', 'w',
            encoding='utf-8').write(
        "module zhao_raster_tile_pipe_v2 ();\n"
        "  zhao_texture_island_v3_top #(.MIGRATION_SHADOWS(1'b0)) u_i ();\n"
        "  // MIGRATION_SHADOWS=0 exposed shadow state\n"
        "endmodule\n")
    io.open(rtl / 'raster' / 'zhao_other.sv', 'w', encoding='utf-8').write(
        "module zhao_other ();\n"
        "  zhao_texture_island_v3_top #(.MIGRATION_SHADOWS(%s)) u_j ();\n"
        "endmodule\n" % extra_value)
    return rtl


class ShadowWitnessTests(unittest.TestCase):
    def tearDown(self):
        witness.configure(REAL_RTL)

    def test_passes_on_the_real_tree(self):
        witness.configure(REAL_RTL)
        self.assertEqual(witness.main(), 0)

    def test_fires_when_a_site_turns_shadows_on(self):
        with tempfile.TemporaryDirectory() as tmp:
            witness.configure(_synthetic(tmp, "1'b1"))
            self.assertEqual(
                witness.main(), 1,
                "the witness did not fire on a site that turns shadows on")

    def test_does_not_fire_on_a_second_legal_site(self):
        # The control's own control: an extra site passing 1'b0 must NOT fire,
        # or the test above would pass for the wrong reason -- any third site
        # rather than a shadows-on one.
        with tempfile.TemporaryDirectory() as tmp:
            witness.configure(_synthetic(tmp, "1'b0"))
            self.assertEqual(witness.main(), 0)

    def test_fires_when_the_elaboration_guard_is_removed(self):
        with tempfile.TemporaryDirectory() as tmp:
            rtl = _synthetic(tmp, "1'b0")
            tile = rtl / 'raster' / 'zhao_raster_tile_pipe_v2.sv'
            text = io.open(tile, encoding='utf-8').read()
            io.open(tile, 'w', encoding='utf-8').write(
                text.replace('  // MIGRATION_SHADOWS=0 exposed shadow state\n', ''))
            witness.configure(rtl)
            self.assertEqual(
                witness.main(), 1,
                "the witness did not fire when the $fatal guard was gone")


if __name__ == '__main__':
    unittest.main(verbosity=2)
