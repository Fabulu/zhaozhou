r"""Author design/shell_fit_ports_v2.yml from the sibling shell's declaration.

The V1 policy is 154 hand-maintained entries. The sibling has 209, and 154 of
them are the SAME PORTS with different ordinals -- the V2 shell declares its
63 new ports first, so every inherited ordinal shifts. Re-typing 154 entries to
change a number in each is how a transcription error gets into the one file
that says which bits of the design are allowed to be constant.

So: the inherited entries are COPIED from the V1 policy by name, with only the
ordinal recomputed from the V2 declaration. The 63 new ones are authored here,
and that authoring is the whole point of this file.

WHAT IS MECHANICAL AND WHAT IS NOT
----------------------------------
Mechanical: ordinal, name, direction, and the fact that an output takes a
`sink` and an input takes a `driver`.

AUTHORED, and wrong answers here are not caught by any tool:

* `domain` decides which clock the instrument registers a port on. Getting it
  wrong builds a clock-domain crossing INTO the measuring instrument. Every
  assignment below was read out of the RTL rather than inferred from the port
  name -- `lease_open_o` sits in a `posedge gpu_clk` block in
  `zhao_video_ready_bridge_v2` even though it is declared in a section headed
  "the video-domain barrier", and the blank/scanout/swap group really is video
  because its logic gates on `vid_local_rst_n`.

* `driver` decides which handler owns the port, and a port owned by nothing is
  a port driven by a constant, which the fitter folds away. THAT is the failure
  mode this file exists to avoid: the area comes back LOW, which is the
  flattering direction, on the one measurement whose job is policing an ALM
  budget that is already far over.

* `dynamic_mask` declares which bits actually toggle. `shell_fit_smoke.py`
  checks it for EXACT equality against observed toggling, so it cannot drift
  from the stimulus -- but both being too narrow passes, so the mask is not a
  substitute for driving the port properly.

STATE, 2026-09-18 (evening): IT GENERATES, AND IT LINTS.
--------------------------------------------------------
    fpga/rtl/generated/zhao_shell_v2_fit_top.sv        2,557 lines, 217 ports
    fpga/rtl/generated/zhao_shell_v2_fit_top.manifest.json

Lint-clean against the sibling's real 97-source closure with `-DSYNTHESIS=1
-DQUARTUS_SYNTHESIS=1`, top module `shell_v2_top`. Gated by
`shell_v2_fit_generated_freshness` and `lint_shell_v2_fit_top`, both of which
were seen to FIRE before being trusted.

WHAT IS STILL OWED: NOTHING RUNS IT YET.
----------------------------------------
The wrapper exists and elaborates; no flow fits it. `zhao_shell_fit_top` -- V1's
-- has no `- top:` entry in `design/fit_targets.yml` either, because the ten-pin
instrument is not a `run_block_fit` target: it goes through
`tools/quartus/run_shell_fit.ps1`, which is 36 KB hardcoded to V1 at every
level --

    $WrapperRel  = 'fpga/rtl/generated/zhao_shell_fit_top.sv'
    $ProjectRel  = 'fpga/quartus/shell_fit'     # .qsf .sdc .qpf report.tcl
    $GateName    = 'shell_fit_top_clean_characterization'

-- so the sibling needs that launcher parameterised over wrapper, project
directory and gate name, plus its own QSF/SDC/QPF and pin assignment. That is
the next piece of work on this instrument and it is a launcher/project job, not
a generator one.

This is deliberately written down rather than left implied, because a wrapper
that elaborates and that nothing fits is the BUILT-INSTALLED-NOWHERE shape.

AND `tools/budget/uncashed_cheques.py` CANNOT SEE IT. Checked rather than
assumed: the tool runs (self-test 4 fire / 4 no-fire), scans 271 modules and
reports 7 open rows, and `shell_v2_top` is in none of them. Its check-1 entry
condition is "measured OR fit-targeted, instantiated by no .sv in fpga/rtl",
and this wrapper has never been either -- so it is invisible to the detector
precisely BECAUSE the cheque was never even partially cashed.

That is a real gap and it reads in the flattering direction: a clean
uncashed-cheques run does not mean nothing is uninstalled, it means nothing
that was already measured or targeted is uninstalled. A generated wrapper that
nothing has ever fitted falls through. Written here rather than fixed here
because widening the tool's entry condition to "every module under
fpga/rtl/generated" is its own change with its own false-positive question --
but the next person should know the silence is not evidence.

THE BLOCKER BELOW IS CLOSED, and the answer is worth keeping because it is
smaller and sharper than the question was.

    KeyError: 'tri_area2_i'  in _render_triangle_values

The warning that followed it -- do not drive `tri_flat_request_i` with entropy,
because random values are illegal opcodes that park the pipe in refusal, which
SHRINKS the measured area in the flattering direction while the smoke harness
blesses the run because the declared mask still matches what toggled -- was
right, and it asked the next pass to find what unpacks the 1,157-bit metadata
word before authoring anything. Done:

**`zhao_raster_tile_pipe_v2` is the unpacker** (field map at its lines 244-257),
and it contains EXACTLY TWO refusal conditions:

    assign profile_aux_bad_w  = incoming_flat_request_w[268] ||
                                (incoming_flat_request_w[267:44] != 224'd0);
    assign profile_area_bad_w = (incoming_area2_w == 47'd0);

So the whole legality surface is: `tri_area2_i` non-zero -- which
`_render_triangle_values` ALREADY computes from the vertices and already raises
on -- and `tri_flat_request_i` with bit 268 clear and [267:44] zero. That leaves
**73 of 298 bits free** ([43:0] and [297:269]). Everything else the unpacker
reads -- all three 240-bit planes, the 48-bit continuation tail, the 32-bit
fragment state, min_x -- is stored with no test whatever, so it only has to
toggle and to differ between the two triangles.

Two things follow, and both are now in the tree:

* the planes are built as {n0[95:0], dndx[71:0], dndy[71:0]} straight off that
  field map, one edge per plane, so they differ from each other and between
  triangles without being invented;
* `tri_flat_request_i`'s `dynamic_mask` in `design/shell_fit_ports_v2.yml` was
  0x3fff...fff -- all 298 bits declared free, INCLUDING the 225 that must stay
  zero. That is the flattering declaration the warning describes, sitting in the
  policy the whole time. It is now the 73-bit legal mask with a
  `constant_reason` saying why.

And `zhao_raster_texture_v3_fit_top.sv:347-355` already builds a legal
`job_meta_w` for the G8A instrument, including `[297:296] = 2'd1` and
`[424:378] = 47'd16777216`. The encoding was authored one instrument over.

WHAT THE ABI ACTUALLY IS, traced 2026-09-18 so the next pass starts here
-------------------------------------------------------------------------
`zhao_geom_bin_pipe_v2` does NOT decode any of it. It concatenates the whole
carriage into one 1,157-bit metadata word, MSB to LSB, and the binner samples
that on the same `tri_we` edge that stores the 142-bit triangle:

    tri_meta_w = {tri_v_over_w_plane_i, tri_u_over_w_plane_i,
                  tri_invw_plane_i, tri_min_x_i, tri_area2_i,
                  tri_fragment_state_i, tri_continuation_tail_i,
                  tri_flat_request_i}            // METAW = 1157, $bits-checked

So for the BINNER the values are opaque and only their toggling matters. The
legality question is entirely about what unpacks the word downstream, and that
is where the next pass should look before authoring anything.

ONE FIELD IS ALREADY FREE. `_render_triangle_values` computes

    area2 = (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x)

and raises if it is not positive -- so `tri_area2_i` can be driven from that
exact value rather than invented, and a zero or negative area is precisely the
degenerate-triangle case that would make a consumer refuse. Wire that one from
the existing computation; it is the only one of the eight that needs no new
arithmetic.

WHAT WAS LEFT BEFORE, now done except the above
------------------------------------------------
What this file produces today is the ordinals, domains and handler ownership --
the authored decisions that need a human reading the RTL. Running
`gen_shell_fit_top.py` against the result lists the rest precisely, and it is
worth reading that output rather than this paragraph. In summary:

1. **The five new handlers are not registered in the generator yet**, so every
   new input reports `invalid driver`. They need entries in
   `HANDLER_INPUT_PORTS` and stimulus bodies in `_render_stimulus`.

2. **Every new port needs a `dynamic_mask`**, and any constant bits need a
   `constant_reason`. A mask cannot honestly be guessed: `shell_fit_smoke.py`
   checks it for EXACT equality against observed toggling, so the sequence is
   author the stimulus, run the smoke harness, read what actually toggled, and
   write that back. The mask is a RECORD of the stimulus, not a wish about it.

3. **`shell_ports.py:852` requires every driven input to be gpu-domain**, and
   four of the sibling's new inputs are genuinely video-domain: `blank_cmd_i`,
   `scanout_ack_i`, `frame_swap_valid_i` and `frame_swap_slot_i` are consumed
   by logic in `zhao_video_ready_bridge_v2` that gates on `vid_local_rst_n`.
   Declaring them gpu-domain to satisfy the check would build an
   unsynchronised clock-domain crossing INTO the measuring instrument and
   corrupt the Fmax it exists to report. The instrument needs a video-domain
   stimulus bank, symmetric with the video-domain CAPTURE bank it already has.

WHY IT IS PAUSED HERE rather than pushed through: the cheap virtual-pin row
answers the AREA question meanwhile, and area is the binding constraint. The
instrument's own value is a realistic Fmax, which is worth having only once
there is a composed timing report to say what actually binds.

(An earlier draft of this paragraph said the cone's Fmax was dominated by
`zhao_cmd_dma`'s 156 dependent CRC steps. That came from
`reports/REMAINING_BLOCKERS.md`, last touched 2026-08-28; the module has been
reworked twice since and now WALKS its header CRC and payload seed eight bytes
per cycle through one fold. The claim was stale, and repeating it would have
justified skipping this instrument for a reason that stopped being true three
weeks ago -- the second stale-prose trap in one session.)

The new inputs group into five new handlers plus an extension of the existing
renderer:

    v3_config       the binding page channel: BEGIN / ROW / END plus its seal
    v3_palette      the palette loader: BEGIN / 256 writes / END
    fill_responder  the frame-fill data return
    sheet_responder the sheet request's ready
    video_host      blank, scanout ack and the swap echo -- VIDEO domain
    render_producer EXTENDED with the tri_* attribute carriage and the clear
                    word, because they are the same producer as the triangle
                    stream and splitting them would invent a second one
"""
import io
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / 'tools' / 'quartus'))

from shell_ports import (  # noqa: E402
    discover_type_signedness, discover_type_widths, parse_module_declaration,
)

V1_POLICY = REPO / 'design/shell_fit_ports.yml'
V2_SHELL = REPO / 'fpga/rtl/common/zhao_shell_top_v2.sv'
OUT = REPO / 'design/shell_fit_ports_v2.yml'

TRAFFIC_PROFILE = 'shell-fit-legal-ish-v2'

# Ports whose CONSUMER forbids some bits from moving. See the long note at the
# mask assignment for why this is the one legitimate reason to narrow a mask,
# and why it is the opposite of the case that rule guards against.
#
# Traced 2026-09-18 in `zhao_raster_tile_pipe_v2`, which unpacks the binner's
# 1,157-bit metadata word (field map at its lines 244-257) and carries exactly
# two refusal conditions:
#
#     profile_aux_bad_w  = incoming_flat_request_w[268] ||
#                          (incoming_flat_request_w[267:44] != 224'd0);
#     profile_area_bad_w = (incoming_area2_w == 47'd0);
#
# `tri_area2_i` is not listed here: it is constrained to be NON-ZERO rather than
# to hold specific bits, every bit of it may still move, and the generator
# already derives it from a triangle whose positive area is checked.
CONSUMER_CONSTRAINED_MASKS = {
    'tri_flat_request_i': (
        ((1 << 44) - 1) | (((1 << 29) - 1) << 269),
        'zhao_raster_tile_pipe_v2 raises profile_aux_bad_w unless bit 268 and '
        'bits [267:44] are zero, which parks the tile pipe in refusal and '
        'shrinks the measured area in the flattering direction while a '
        'full-span mask still matches what toggled; 73 of 298 bits stay free '
        '([43:0] and [297:269])',
    ),
}

# ---------------------------------------------------------------- authored --
# Clock domain for every port the sibling adds. Read from the RTL, not guessed.
VIDEO = {
    'blank_cmd_i', 'scanout_ack_i', 'frame_swap_valid_i', 'frame_swap_slot_i',
    'blank_ack_o', 'blank_active_o', 'frame_slot_ready_o',
}

# Handler ownership for every input the sibling adds.
DRIVERS = {
    'v3_config': (
        'cfg_valid_i', 'cfg_op_i', 'cfg_page_generation_i', 'cfg_selector_i',
        'cfg_row_i', 'cfg_crc32_i', 'cfg_rsp_ready_i',
    ),
    'v3_palette': (
        'pal_load_valid_i', 'pal_load_op_i', 'pal_load_slot_i',
        'pal_load_gen_i', 'pal_load_idx_i', 'pal_load_rgb565_i',
        'pal_load_crc_ok_i',
    ),
    'fill_responder': (
        'fill_req_ready_i', 'fill_data_valid_i', 'fill_data_i',
        'fill_refused_i',
    ),
    'sheet_responder': (
        'sheet_req_ready_i',
    ),
    'video_host': (
        'blank_cmd_i', 'scanout_ack_i', 'frame_swap_valid_i',
        'frame_swap_slot_i',
    ),
    'render_producer': (
        'tri_area2_i', 'tri_invw_plane_i', 'tri_u_over_w_plane_i',
        'tri_v_over_w_plane_i', 'tri_flat_request_i',
        'tri_continuation_tail_i', 'tri_fragment_state_i',
        'frame_clear_word_i',
    ),
}

SINK_FOR_DOMAIN = {'gpu': 'gpu_capture', 'video': 'video_capture',
                   'audio': 'audio_capture'}


def main(check=False):
    v1 = json.load(io.open(V1_POLICY, encoding='utf-8'))
    inherited = {p['name']: p for p in v1['ports']}

    text = io.open(V2_SHELL, encoding='utf-8', errors='replace').read()
    pkg = io.open(REPO / 'fpga/rtl/common/zhao_pkg.sv',
                  encoding='utf-8', errors='replace').read()
    # `shell_ports.parse_module_declaration` rather than the simpler port
    # splitter: it resolves WIDTHS, including typedef ports such as the 103-bit
    # `zhao_guard_req_t`, and a width is what a full-span mask is made of.
    declaration = parse_module_declaration(
        text, 'zhao_shell_top_v2',
        type_widths=discover_type_widths(pkg),
        type_signedness=discover_type_signedness(pkg),
    )
    decl = [(p.direction, p.name, p.bit_width) for p in declaration.ports]

    owner = {}
    for driver, names in DRIVERS.items():
        for n in names:
            if n in owner:
                print('FAIL: %s is owned by both %s and %s'
                      % (n, owner[n], driver))
                return 1
            owner[n] = driver

    ports = []
    new_names = []
    for ordinal, (direction, name, width) in enumerate(decl):
        if name in inherited:
            row = dict(inherited[name])
            row['ordinal'] = ordinal
            ports.append(row)
            continue

        new_names.append(name)
        row = {'ordinal': ordinal, 'name': name, 'direction': direction}
        row['domain'] = 'video' if name in VIDEO else 'gpu'
        if direction == 'output':
            row['sink'] = SINK_FOR_DOMAIN[row['domain']]
        else:
            drv = owner.get(name)
            if drv is None:
                print('FAIL: new input %r has no declared driver.' % name)
                print('  A port owned by nothing is a port driven by a')
                print('  constant, and the fitter folds those away -- the')
                print('  area comes back LOW, which is the direction nobody')
                print('  audits. Add it to DRIVERS with the handler that')
                print('  should own it.')
                return 1
            row['driver'] = drv

        # FULL SPAN, as a first declaration and not as a result. The honest
        # value of this field is whatever the stimulus actually toggles, and
        # `shell_fit_smoke.py` checks it for EXACT equality -- so a full-span
        # mask is a CLAIM that every bit moves, and the smoke harness is what
        # turns it into a measurement or refuses it.
        #
        # Declaring it narrow here would be worse: a narrow mask that matches a
        # narrow stimulus passes, and the area comes back low.
        #
        # THE ONE EXCEPTION, and it is the opposite case. A port whose CONSUMER
        # forbids some bits from moving cannot honestly claim they do, and the
        # rule above assumes the only reason to narrow is a weak stimulus. When
        # the RTL itself refuses the value, full span is the dishonest
        # declaration: it claims bits move that the design would reject, and the
        # smoke harness then refuses a run that is correct.
        #
        # Every entry owes a `constant_reason` naming the consumer and the
        # condition, so the narrowing is auditable rather than convenient.
        narrow = CONSUMER_CONSTRAINED_MASKS.get(name)
        if narrow is not None:
            mask, reason = narrow
            if mask >> width:
                print('FAIL: constrained mask for %s exceeds its %d-bit width'
                      % (name, width))
                return 1
            row['constant_reason'] = reason
            row['dynamic_mask'] = '0x%0*x' % ((width + 3) // 4, mask)
        else:
            row['dynamic_mask'] = '0x%0*x' % ((width + 3) // 4,
                                              (1 << width) - 1)
        ports.append(row)

    unused = sorted(set(owner) - set(new_names))
    if unused:
        print('FAIL: DRIVERS names ports the sibling does not add: %s'
              % ', '.join(unused))
        print('  A driver entry that owns nothing is a dead rule.')
        return 1

    doc = {
        'schema_version': v1['schema_version'],
        'module': 'zhao_shell_top_v2',
        'traffic_profile': TRAFFIC_PROFILE,
        'ports': ports,
    }
    body = json.dumps(doc, indent=2) + '\n'

    if check:
        cur = io.open(OUT, encoding='utf-8').read() if OUT.exists() else ''
        if cur.replace('\r\n', '\n') != body:
            print('FAIL: %s is STALE; regenerate it.'
                  % OUT.relative_to(REPO).as_posix())
            return 1
        print('shell_fit_ports_v2: fresh (%d ports, %d new)'
              % (len(ports), len(new_names)))
        return 0

    io.open(OUT, 'w', encoding='utf-8', newline='\n').write(body)
    print('wrote %s' % OUT.relative_to(REPO).as_posix())
    print('  %d ports, %d inherited, %d new'
          % (len(ports), len(ports) - len(new_names), len(new_names)))
    print('  new inputs by handler:')
    for driver in sorted(DRIVERS):
        owned = [n for n in DRIVERS[driver] if n in new_names]
        print('    %-16s %d' % (driver, len(owned)))
    return 0


if __name__ == '__main__':
    sys.exit(main(check='--check' in sys.argv))
