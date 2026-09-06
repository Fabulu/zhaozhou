#!/usr/bin/env python3
"""Fail if a module reads a live INGRESS port after the capture point.

WHY THIS IS A GATE AND NOT A GREP
---------------------------------
The composed texture island read every per-fragment attribute straight off its
own input pins at the point each was CONSUMED -- ten separate signals,
including PERSPUV's u/w and v/w numerators. A fragment spends about twelve
clocks in RCP24 and PERSPUV, so each tap sampled whatever fragment happened to
be at the boundary twelve clocks later, and once submission stopped the pins
simply held the last one.

Measured before the repair, in `island_composed_directed`: of 64 fragments
submitted, only 25 distinct ones came out, 39 were lost, 7 were duplicated, and
the tail was one fragment delivered 24 times. Every activity counter moved and
the totals looked plausible throughout.

The owner's recovery architecture (v2, Appendix B) states the rule the repair
has to hold to:

    Establish one generated transaction packet, one ingress capture event and
    one final lifetime owner. No later computation reads unrelated live
    ingress data.

That is a RULE, and the ten taps were instances of breaking it. A grep run once
by hand fixes the instances and leaves the rule unenforced -- the next port
added to the boundary can be tapped again exactly the same way, and the failure
mode is silent. CLAUDE.md says it plainly:

    A probe that does this was written once and thrown away, so its numbers are
    unreproducible -- commit the probe.

WHAT IT CHECKS
--------------
For each configured module: every input port matching the ingress prefix may be
referenced ONLY inside a permitted region -- the capture process, the service
that admits the fragment, and the ready/handshake wiring. A reference anywhere
else is a late read and is reported with its line.

This does not prove the capture itself is correct; the identity gate in
`island_composed_directed` does that. It proves nobody is bypassing it.

USAGE
    python tools/rtl/check_ingress_capture.py [--list]

Exit 0 clean, 1 on a violation, 2 on its own failure to work.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ---------------------------------------------------------------------------
# The contract, per module.
#
#   prefix   -- input ports beginning with this are the ingress packet
#   allow    -- a reference is permitted if any of these appears on its line or
#               inside the brace/parenthesis region opened by a permitted
#               anchor; anchors are matched as substrings of the enclosing
#               instantiation or process header
# ---------------------------------------------------------------------------
# A CONTRACT THIS GATE DOES NOT YET HAVE, NAMED SO IT IS NOT FORGOTTEN.
#
# `bind_mode_i` and `bind_base_i` on zhao_texture_island_top are GLOBAL ports
# read by zhao_texture_tmu_plan at its OWN T0 handshake, many cycles after the
# fragment that is supposed to own them was admitted. That is a late ingress
# read of exactly the kind this file exists to prevent, and the gate misses it
# because the island contract watches the `frag_` prefix only.
#
# It is NOT added as a `bind_` contract today for one reason worth stating: the
# gate would go red immediately and stay red, because the defect is real and its
# repair is a work item (the mode must travel with the fragment as a labelled
# descriptor, or CLUT and direct-colour must run as separate drained phases).
# A gate that is red for months is a gate people learn to ignore, which is worse
# than the hole.
#
# So it is recorded here and in
# reports/ZHAOZHOU-PREFIT-VERIFICATION-AND-REARCHITECT-20260906.txt section 5.4.
# When the descriptor lands, add the contract and delete this comment -- and if
# you are reading this and the descriptor has landed, the comment is the bug.

CONTRACTS = [
    {
        # GEOM.ASSETFETCH -- added 2026-09-06 from the owner's COMBINE/ASSETFETCH
        # recovery brief, whose second prerequisite is exactly this:
        #
        #   "ASSETFETCH still reads its client identity live after acceptance.
        #    MESHFETCH reads its descriptor address and client live after
        #    acceptance. These must be captured before prefetching makes
        #    overlapping requests ordinary."
        #
        # Both were true, both are now captured, and the rule is guarded so the
        # two-bank rework cannot reintroduce it while adding the very overlap
        # that makes it bite. Today one job is in flight and the caller holds
        # its inputs steady; that is the only reason nothing broke.
        "path": "fpga/rtl/geometry/zhao_geom_assetfetch.sv",
        "prefix": "m_",
        "allow_regions": [
            # the one acceptance point: S_IDLE latching the job
            (r"S_IDLE: if \(m_valid_i\) begin", "        end"),
        ],
        "allow_lines": [
            r"^\s*(input|output)\s",
            # the handshake itself, and the footprint arithmetic that DERIVES
            # the captured line bases. That arithmetic is combinational on the
            # ingress offsets BY DESIGN -- its results (ix_line0_q, vx_line0_q,
            # the counts) are what get latched at acceptance, so it is the
            # capture rather than a late read.
            r"^\s*assign m_ready_o\s*=",
            r"m_valid_i",
            r"ix_bytes_c\s*=",
            r"vx_bytes_c\s*=",
            r"ix_abs_c\s*=",
            r"vx_abs_c\s*=",
            r"bad_count_c\s*=",
            r"bad_align_c\s*=",
        ],
    },
    {
        # GEOM.MESHFETCH -- same brief, same prerequisite. Its guard request
        # took BOTH the client and the 64-byte descriptor address straight off
        # the ingress pins, formed several states after the job was accepted.
        "path": "fpga/rtl/geometry/zhao_geom_meshfetch.sv",
        "prefix": "j_",
        "allow_regions": [
            (r"S_IDLE: begin\s*\n\s*if \(j_valid_i\) begin", "          end"),
        ],
        "allow_lines": [
            r"^\s*(input|output)\s",
            r"^\s*assign j_ready_o\s*=",
            r"j_valid_i",
        ],
    },

    {
        # TERRAIN.VISIBLE -- the visible-patch builder. Its ingress is a VIEW,
        # and the reason it is under contract is that a view is consumed over
        # THOUSANDS of cycles rather than at one consumption point:
        # a radius-16 window is ~3,300 clocks of scanning.
        #
        # So the failure mode here is worse than the island's. The camera moves
        # every frame and the caller is entitled to present the next view the
        # instant the handshake completes. A scan that re-read `v_centre_ix_i`
        # to form its row restart -- the obvious way to write it -- would splice
        # the second half of one window onto the first half of another and emit
        # a patch list that is internally consistent, correctly ordered, and
        # from nowhere the camera has ever been.
        #
        # Everything the scan needs is latched at acceptance: both window
        # bounds and both cursors. `rad_c` is the combinational widening of
        # `v_radius_i` used to form those bounds -- it IS the capture, so the
        # capture block may use it, and it is listed as an alias so that
        # anything ELSE using it is caught as a late read laundered through one
        # extra wire.
        "path": "fpga/rtl/terrain/zhao_terrain_visible.sv",
        "prefix": "v_",
        "allow_regions": [
            (r"if \(v_valid_i && v_ready_o\) begin", "      end"),
        ],
        "allow_lines": [
            r"^\s*(input|output)\s",
            r"^\s*assign v_ready_o\s*=",
            r"^\s*wire signed \[31:0\] rad_c\s*=",
        ],
        "aliases": ["rad_c"],
    },
    {
        # TERRAIN.PAGELOADER -- registered with the block rather than after it.
        # A page load runs for ~11,000 cycles, so "the caller holds its inputs
        # steady" is not a defence here the way it is for a one-cycle service:
        # the sequencer will have moved on to the next job long before this one
        # finishes, and a live read of `j_hps_addr_i` on burst 300 would fetch
        # from whatever address happened to be presented then. Everything is
        # latched in S_IDLE and the machine reads `job_*` afterwards.
        "path": "fpga/rtl/terrain/zhao_terrain_pageloader.sv",
        "prefix": "j_",
        "allow_regions": [
            (r"S_IDLE: begin", "        end"),
        ],
        "allow_lines": [
            r"^\s*(input|output)\s",
            r"^\s*assign j_ready_o\s*=",
        ],
    },
    {
        "path": "fpga/rtl/texture/zhao_texture_island_top.sv",
        "prefix": "frag_",
        # The capture process and the block that ADMITS the fragment are the
        # only places allowed to see a live ingress pin.
        "allow_regions": [
            # the attribute store: one ingress capture event
            (r"always_ff @\(posedge clk\) begin\s*\n\s*if \(frag_valid_i && frag_ready_o\)", "end\n  end"),
        ],
        "allow_lines": [
            # port declarations
            r"^\s*(input|output)\s",
            # the admitting service and its handshake -- depth and valid are
            # consumed AT the boundary, which is the capture point itself
            r"\.v_valid_i\(frag_valid_i\)",
            r"\.d_i\(frag_depth_i\)",
            r"^\s*assign frag_ready_o\s*=",
            # the packed word that is STORED; it is the capture, not a late read
            r"^\s*assign fr_f_ctx_in\s*=",
            r"^\s*logic \[CTXW-1:0\] fr_f_ctx_in",
            r"^\s*wire \[FCTXW-1:0\] fc_wp",
            # the store itself
            r"^\s*uvw_m\[fc_wp\]",
            r"^\s*fctx_m\[fc_wp\]",
            r"^\s*fbase_m\[fc_wp\]",
            r"^\s*fmisc_m\[fc_wp\]",
            r"if \(frag_valid_i && frag_ready_o\)",
            # THE OWNER CREDIT'S TWO HANDSHAKE TERMS.
            #
            # `admit_c` IS the capture event -- it is the same
            # `frag_valid_i && frag_ready_o` the store above is gated on,
            # named once so the credit counter and the store cannot drift
            # apart. And the RCP service's `v_valid_i` is gated at the
            # admission boundary, which is where the owner brief requires it:
            # gating only `frag_ready_o` lets RCP accept a job the caller was
            # told did not handshake.
            #
            # Allowed BY NAME rather than by loosening the pattern, because
            # every future `frag_valid_i` reference should have to argue for
            # itself here the way these two do.
            r"^\s*wire admit_c = frag_valid_i && frag_ready_o;",
            r"\.v_valid_i\(frag_valid_i && credit_available\)",
        ],
        # A LIVE-INGRESS ALIAS: a wire built combinationally from the ingress
        # pins. It IS the capture word, so the store may use it -- but anything
        # else using it is a late read laundered through one extra wire, and
        # that is exactly the historical bug (assign fr_f_ctx = fr_f_ctx_in).
        # Without this the gate passes the very defect it was written for, which
        # was confirmed by mutation before this was added.
        "aliases": ["fr_f_ctx_in"],
    },
    {
        # THE BIGGEST COMPOSITION IN THE TREE -- 21 instantiated blocks.
        #
        # CHECKED AND CLEAN when this contract was added: every `render_*` input
        # is consumed inside the single `zhao_geom_bin_pipe u_render_bin`
        # instantiation, at its own `tri_valid_i` / `tri_ready_o` handshake.
        # One consumer, at the admission point, which is what the island got
        # wrong in ten places.
        #
        # It is guarded rather than merely noted because the island's defect was
        # a CLASS, not an instance: the next per-triangle field added here could
        # be read from a consumer further down the geometry chain, where the
        # latency is variable, and nothing would say so.
        # MUTATION-VERIFIED: inserting a late read of render_kx0_i -- a
        # genuine per-triangle input -- next to zhao_raster_fbwrite is
        # caught; the unmutated tree is clean. A contract that has not been
        # shown to go red is not evidence, and this one had to be narrowed
        # once already before it stopped flagging frame-scoped signals.
        "path": "fpga/rtl/common/zhao_shell_top.sv",
        "prefix": "render_",
        "allow_regions": [
            # the one instantiation that admits the triangle
            (r"zhao_geom_bin_pipe u_render_bin \(", ");"),
        ],
        "allow_lines": [
            r"^\s*(input|output)\s",
            # FRAME-SCOPED CONFIGURATION, NOT PER-TRANSACTION DATA. These are
            # constant for the whole frame, so a consumer reading them late
            # reads the same value -- which is the entire property the rest of
            # this gate is about. `zhao_raster_fbwrite` legitimately takes the
            # framebuffer base and stride at pixel-write time, long after any
            # triangle was admitted.
            #
            # The first version of this contract had no such exemption and
            # flagged them, which would have read as "the shell has the island's
            # bug in two places". It does not. The distinction the gate has to
            # make is transaction-scoped versus frame-scoped, not early versus
            # late -- and getting that wrong makes it a noise generator that
            # trains its reader to skip the output.
            r"render_fb_base_i",
            r"render_fb_stride_i",
            r"render_frame_end_i",
            r"render_frame_begin_i",
            r"render_grid_w_i",
            r"render_grid_h_i",
        ],
    },
    {
        # THE FIELD SERVICE PATH -- six blocks, and the same shape that broke
        # the island: a transaction with a context, dispatched to services whose
        # latency varies.
        #
        # CHECKED AND CLEAN when this contract was added: every `long_*` input
        # is consumed at ONE instantiation, together with its own
        # `long_valid_i` / `long_ready_o` handshake.
        #
        # That makes three compositions examined for this defect -- the island,
        # the shell, and this -- and the island is the only one that had it. The
        # class was real and the spread was not, which is worth knowing before
        # anyone goes looking for it everywhere.
        # MUTATION-VERIFIED: a late read of long_s0_i near the end of the
        # module is caught; the unmutated tree is clean.
        "path": "fpga/rtl/field/zhao_field_v3_svcpath.sv",
        "prefix": "long_",
        "allow_regions": [
            (r"\.long_valid_i\(long_valid_i\)", ");"),
        ],
        "allow_lines": [
            r"^\s*input\s",
            r"^\s*output\s",
        ],
    },
]

# THE PORT NAME IS THE LAST IDENTIFIER, NOT THE ONE AFTER A KNOWN TYPE.
#
# The previous version enumerated the type forms it expected -- optional `var`,
# then one of wire/logic/reg, then an optional PACKAGE-QUALIFIED typedef. An
# UNQUALIFIED user typedef matched none of them, so the capture group landed on
# the TYPE:
#
#     input  var zhao_client_e      m_client_i,
#                ^^^^^^^^^^^^^ captured as the port name
#
# and every such port silently dropped out of the watch set. Found 2026-09-06 by
# MUTATING the very defect this gate had just been extended to catch: restoring
# `guard_req_o.client = m_client_i` in GEOM.ASSETFETCH, and the gate reported the
# file CLEAN. The meshfetch mutant on the same run fired correctly, which is what
# made the asymmetry visible at all.
#
# That is this tree's most-recorded failure: a detector reading LOW, precise and
# wrong, with nobody auditing good news. It had been true of every
# `zhao_guard_rsp_t`, `zhao_client_e` and `zhao_px_stream_t` port under contract
# since the gate was written.
#
# Anchoring on the LAST identifier before an optional unpacked dimension and an
# optional comma needs no list of type spellings, so a new typedef cannot make
# it read low again. Verified against seven real declarations from this tree,
# including the two the old one got wrong.
PORT = re.compile(
    r"^\s*input\s+.*?([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*,?\s*(?://.*)?$",
    re.M,
)

# SELF-CHECK. A regex that matches nothing reports a confident, empty answer,
# and this file exists because a silent measurement lied once already.
assert PORT.match("    input  var logic [23:0] frag_depth_i,"), "port regex is dead"
assert PORT.match("    input  var logic [1:0]  frag_class_i,"), "port regex is dead"


def strip_comments(line):
    i = line.find("//")
    return line[:i] if i >= 0 else line


def check(contract, verbose):
    path = os.path.join(REPO, contract["path"])
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()

    ports = [m.group(1) for m in PORT.finditer(text)
             if m.group(1).startswith(contract["prefix"])]
    if not ports:
        print("check_ingress_capture: parsed NO %s* input ports from %s -- "
              "refusing to report clean, because with an empty port set every "
              "file would pass" % (contract["prefix"], contract["path"]))
        return None

    for alias in contract.get("aliases", []):
        if alias not in text:
            print("check_ingress_capture: alias %s is not in %s -- the contract "
                  "names a signal that no longer exists, so it would guard "
                  "nothing" % (alias, contract["path"]))
            return None

    lines = text.split("\n")

    # Line numbers inside a permitted region.
    permitted = set()
    for start_pat, end_tok in contract["allow_regions"]:
        for m in re.finditer(start_pat, text):
            head = text.count("\n", 0, m.start())
            end = text.find(end_tok, m.end())
            tail = text.count("\n", 0, end if end > 0 else len(text))
            for n in range(head, tail + 1):
                permitted.add(n)

    allow_lines = [re.compile(p) for p in contract["allow_lines"]]
    watched = ports + contract.get("aliases", [])
    ref = re.compile(r"\b(" + "|".join(re.escape(p) for p in watched) + r")\b")

    # A permitted line that does not terminate its statement carries the
    # permission onto the continuation lines. Without this the checker flags the
    # second half of a two-line `assign`, which is the same statement it just
    # allowed -- a false positive that would train the reader to ignore it.
    violations = []
    carry = False
    for n, raw in enumerate(lines):
        line = strip_comments(raw)
        allowed = carry or (n in permitted) or any(p.search(line) for p in allow_lines)
        if allowed and ";" not in line:
            carry = True
        elif ";" in line:
            carry = False
        if not ref.search(line):
            continue
        if allowed:
            continue
        violations.append((n + 1, sorted(set(ref.findall(line))), raw.strip()))

    if verbose:
        print("  %s: %d ingress ports, %d permitted lines"
              % (contract["path"], len(ports), len(permitted)))
    return violations


def main(argv):
    verbose = "--list" in argv
    total = 0
    for contract in CONTRACTS:
        v = check(contract, verbose)
        if v is None:
            return 2
        if v:
            print("check_ingress_capture: LATE INGRESS READ in %s"
                  % contract["path"])
            print("  A consumer is reading a live boundary pin instead of the "
                  "captured packet. By the time it reads, the pin belongs to a "
                  "different fragment.")
            for line_no, names, src in v:
                print("    %s:%d  %s" % (contract["path"], line_no,
                                         ",".join(names)))
                print("        %s" % src[:100])
            total += len(v)
        else:
            print("check_ingress_capture: %s clean" % contract["path"])
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
