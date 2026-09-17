r"""Packet-H gate clause: the nested V3 elaborates with NO migration shadows.

The clause reads: *nested V3 is explicitly `MIGRATION_SHADOWS=1'b0` with
`shadow_present=0` and no shadow comparator/counters.*

That is a property of how the sibling shell's closure ELABORATES, and the
tempting way to check it is to read `zhao_raster_tile_pipe_v2.sv`, see
`.MIGRATION_SHADOWS(1'b0)`, and call it done. That check would pass forever
after somebody adds a second instantiation somewhere else with the default --
which is `1'b1`, because `zhao_texture_island_v3_top` defaults shadows ON.

So this asserts the three facts that together make the clause true, and the
third is the one a single-site reading misses:

  1. the island's default really is `1'b1`, so an UNPARAMETERISED instantiation
     would turn shadows on -- if this ever becomes `1'b0`, the clause stops
     being load-bearing and this witness should be re-read, not silently kept;
  2. EVERY `.MIGRATION_SHADOWS(...)` override in fpga/rtl passes either a
     literal `1'b0` or a parameter it is itself forwarding -- there is no site
     that turns shadows on;
  3. the elaboration guard that catches it at runtime still exists.

It is a text check and says so. It cannot prove elaboration; it can prove that
no source site asks for shadows, which is the part a reader would otherwise
have to take on trust across three files.
"""
import io
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

# The RTL root is overridable ONLY so this witness can be fired at a
# synthetic tree that turns shadows on. It has no other caller and no
# default other than the real one -- a detector that has not been shown to
# fire has not been tested, and this one cannot be fired by editing
# production RTL without a live-tree hazard.
RTL = REPO / 'fpga/rtl'
ISLAND = RTL / 'texture/zhao_texture_island_v3_top.sv'
TILE = RTL / 'raster/zhao_raster_tile_pipe_v2.sv'

OVERRIDE = re.compile(r'\.MIGRATION_SHADOWS\s*\(\s*([^)]*?)\s*\)')
DEFAULT = re.compile(r'parameter\s+bit\s+MIGRATION_SHADOWS\s*=\s*(\S+?)\s*,')
GUARD = 'MIGRATION_SHADOWS=0 exposed shadow state'

# A value that does NOT turn shadows on: a zero literal, or a bare identifier
# being forwarded from an enclosing parameter (the forwarding site cannot
# decide the value; whoever instantiates IT is checked by the same rule).
OFF = re.compile(r"^(?:1'b0|1'd0|0|'0)$")
FORWARD = re.compile(r'^[A-Za-z_]\w*$')


def configure(rtl_root):
    """Point the witness at a different tree. Used only by the fire test."""
    global RTL, ISLAND, TILE
    RTL = Path(rtl_root)
    ISLAND = RTL / 'texture/zhao_texture_island_v3_top.sv'
    TILE = RTL / 'raster/zhao_raster_tile_pipe_v2.sv'


def main():
    rc = 0
    with io.open(ISLAND, encoding='utf-8', errors='replace') as fh:
        text = fh.read()
    m = DEFAULT.search(text)
    if not m:
        print('FAIL: could not read MIGRATION_SHADOWS default from %s' % ISLAND.name)
        return 1
    default = m.group(1)
    print('island default            %s' % default)
    if OFF.match(default):
        print('  NOTE: the default is OFF, so an unparameterised instantiation would')
        print('  no longer turn shadows on. This witness is written against a default')
        print('  of 1\'b1 -- re-read it rather than keeping it silently.')

    sites = []
    for path in sorted(RTL.rglob('*.sv')):
        with io.open(path, encoding='utf-8', errors='replace') as fh:
            t = fh.read()
        for mm in OVERRIDE.finditer(t):
            line = t[:mm.start()].count('\n') + 1
            try:
                rel = path.relative_to(REPO).as_posix()
            except ValueError:
                # The fire test points RTL at a temp tree outside the repo.
                rel = path.relative_to(RTL).as_posix()
            sites.append((rel, line, mm.group(1)))

    print('override sites            %d' % len(sites))
    if not sites:
        print('FAIL: no .MIGRATION_SHADOWS(...) site found at all. Either the')
        print('  parameter was renamed or this regex stopped matching; a witness')
        print('  that matches nothing reports a clean sheet.')
        return 1

    for rel, line, value in sites:
        if OFF.match(value):
            kind = 'OFF literal'
        elif FORWARD.match(value):
            kind = 'forwarded parameter'
        else:
            kind = 'TURNS SHADOWS ON'
            rc = 1
        print('  %-52s :%-5d %-22s %s' % (rel, line, value, kind))

    with io.open(TILE, encoding='utf-8', errors='replace') as fh:
        guard = GUARD in fh.read()
    print('elaboration guard present %s' % guard)
    if not guard:
        print('FAIL: the $fatal that catches exposed shadow state is gone')
        rc = 1

    print()
    print('packet-h shadow witness: %s' % ('PASS' if rc == 0 else 'FAILED'))
    return rc


# SELF-CHECK. Both patterns must still bite on known-good text.
assert OVERRIDE.search(".MIGRATION_SHADOWS(1'b0),").group(1) == "1'b0"
assert OVERRIDE.search('.MIGRATION_SHADOWS(MIGRATION_SHADOWS),').group(1) == 'MIGRATION_SHADOWS'
assert DEFAULT.search("parameter bit MIGRATION_SHADOWS = 1'b1,").group(1) == "1'b1"
assert OFF.match("1'b0") and not OFF.match("1'b1")
assert FORWARD.match('MIGRATION_SHADOWS') and not FORWARD.match("1'b1")

if __name__ == '__main__':
    sys.exit(main())
