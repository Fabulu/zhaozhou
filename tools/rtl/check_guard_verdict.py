#!/usr/bin/env python3
"""Fail if a MEM.GUARD client tests `ready` and `ok` in the same cycle.

WHY THIS IS A GATE AND NOT A GREP
---------------------------------
`zhao_mem_guard` answers a request in TWO cycles, and its own source says so:

    rsp.ready     = !fwd_active;   // level; verdict 1 cycle after accept
    rsp_ok_q      <= 1'b1;         // pulse, the cycle AFTER the accept

The accept is what raises `fwd_active`. So on a PASSING request `ready` is high
and `ok` is low in the accepting cycle, and `ok` is high while `ready` is low in
the next one. **They are never high together.** A client written as

    if (guard_rsp_i.ready) begin
      if (guard_rsp_i.ok) ... else ...   // <-- reached only via the else
    end

therefore treats every pass as a denial, silently, with `guard_denied_o` staying
at zero because the guard only raises `violation` when it actually refuses.

Found 2026-09-06 by D22 tread 10, in BOTH geometry fetchers. It had survived
because every bench PLAYED the guard and every played responder raised ready and
ok together -- so the RTL, the harnesses and the measurements all agreed with
each other about a machine that does not exist. `zhao_raster_fbwrite` (W_VERD)
and `zhao_debug_frameblit` (B_GUARD_VERDICT) had it right the whole time:

    four clients, two protocols, and the two that were wrong are exactly the
    two whose memory was played.

Fixing the two instances leaves the RULE unenforced. The next guard client can
be written the same way, and the failure mode is a block that looks idle.

WHAT IT CHECKS
--------------
For every module that declares a `zhao_guard_rsp_t` input, find each state-machine
arm guarded on `<rsp>.ready` and report it if `.ok` is tested inside that same
arm rather than in a following state. The shape that passes is: `ready` moves the
machine to a verdict state, and `ok` / `violation` are read there.

WHAT IT DOES NOT CHECK
----------------------
That the verdict state is entered on the right cycle, or that the client waits
rather than assuming a denial when neither bit is set. Those are behavioural and
belong to the directed tests. This catches the STRUCTURAL mistake only -- which
is the one that was made, twice, and could not be seen from any bench.
"""

import io
import re
import sys

# Modules that master a MEM.GUARD port. Listed rather than discovered so that a
# new client has to be added deliberately -- a scanner that finds its own inputs
# reports "0 problems" just as happily when its pattern stops matching, and this
# tree has been bitten by exactly that.
CLIENTS = [
    "fpga/rtl/geometry/zhao_geom_assetfetch.sv",
    "fpga/rtl/geometry/zhao_geom_meshfetch.sv",
    # The adapter is a guard client in its own right, and its coverage audit
    # caught it the moment the file existed -- which is what the audit is for.
    "fpga/rtl/geometry/zhao_geom_mem_adapter.sv",
    "fpga/rtl/raster/zhao_raster_fbwrite.sv",
    # POST.COMPOSITE's lease (2026-09-19), added WITH the blocks: the source
    # read-back and POST.ECHO master the guard through ENGINE0's share, and the
    # lease itself reads the share's verdict pulses to order write data.
    "fpga/rtl/compositor/zhao_post_fbread.sv",
    "fpga/rtl/compositor/zhao_post_echo.sv",
    "fpga/rtl/compositor/zhao_post_lease.sv",
    # TERRAIN.PAGELOADER. Added with the block, not after it -- the coverage
    # audit is only exact if a new client joins this list in the same change
    # that creates it.
    "fpga/rtl/terrain/zhao_terrain_pageloader.sv",
    # TERRAIN.WRITEBACK -- the same port in the other direction. It is the first
    # guard client in the tree that READS the terrain page pool, and it has TWO
    # request/verdict pairs (the page header, then each sheet chunk), so it has
    # two places to make this exact mistake instead of one.
    "fpga/rtl/terrain/zhao_terrain_writeback.sv",
    # TERRAIN.DEVSTORE -- owner ruling R242, 2026-09-22. Added WITH the change
    # that made it a guard client, which is the only way this audit stays
    # exact. It has ONE request/verdict pair for four kinds of op (record
    # burst out, history row out, history row in, record burst in), because
    # one memory engine serves the write FSM and the read FSM in turn -- so
    # there is exactly one place to make this mistake and it is `M_REQ` /
    # `M_VERD`, split for the reason this file exists.
    #
    # NOTE ON THE REDS BESIDE IT, so this entry is not read as a clean bill:
    # seven other files consume a verdict and are NOT in this list at base
    # (zhao_forge_pagebank, zhao_geom_clipread, zhao_geom_drawjob,
    # zhao_geom_ladderbank, zhao_mem_share_wr, zhao_part_table_loader and
    # zhao_terrain_pageio -- the last despite zhao_mem_guard.sv's own header
    # claiming it is registered here), and two are listed but no longer
    # consume one. That debt is not this packet's and is not silently
    # inherited: it is named here, and in the DEVSDRAM packet's findings, which
    # are the commit message of the empty commit on `gz/devsdram` titled
    # "DEVSDRAM FINDINGS" -- the harness refuses to let a packet write its own
    # findings FILE, so a citation to one would be a promise nothing keeps.
    "fpga/rtl/terrain/zhao_terrain_devstore.sv",
    "fpga/rtl/debug/zhao_debug_frameblit.sv",
    "fpga/rtl/video/zhao_scanout_fetch.sv",
    # Pass-through wrapper: it routes the port down to zhao_scanout_fetch and
    # tests no verdict itself. Listed so the coverage audit stays exact rather
    # than carrying an exception.
    "fpga/rtl/video/zhao_video_scanout.sv",
    # A SYNTHESIS PROBE, and it is a real client -- it drives a guard and reads
    # the answer. Its header says the guard is already integrated in
    # zhao_shell_top and re-instantiating it would be measuring the same block
    # twice; that is about AREA, not about protocol, and the protocol still has
    # to be right or the probe measures a path that never completes.
    "fpga/rtl/synth/zhao_probe_render_fb.sv",
    # ---------------------------------------------------------------------
    # SEVEN ADDED 2026-09-19, AND THE REASON THEY WERE MISSING IS THE POINT.
    #
    # This tool CRASHED -- `UnicodeDecodeError` inside discover(), on one
    # cp1252 em dash in `fpga/rtl/terrain/zhao_terrain_bake_v2.sv`. It never
    # reached the coverage audit, so every client added after that byte landed
    # went unlisted and unscanned, and the tool's exit code said "1" for a
    # reason nobody connected to guard protocol at all. The audit that exists
    # to stop the hand list rotting was itself dead, which is the
    # broken-instrument law with the crash standing in for the silent zero.
    #
    # GEOM.PARAMARENA and GEOM.PARAMWALK -- owner completion ruling ITEM 4,
    # 2026-09-22. Added WITH the change that made them guard clients, which is
    # the only way this audit stays exact. Each has ONE request/verdict pair:
    # the arena's M_REQ / M_VERD serves every write, and the walker's
    # W_*_REQ / W_*_VERD triples serve the directory, the chunk and the
    # descriptor. They are the FIRST ENGINE1 clients in the tree that WRITE.
    #
    # NOTE ON THE REDS BESIDE THEM, so this entry is not read as a clean bill
    # of health for the file. This gate was ALREADY RED at commit 5e9565f2,
    # with SEVEN clients missing from this list -- zhao_forge_pagebank,
    # zhao_geom_clipread, zhao_geom_drawjob, zhao_geom_ladderbank,
    # zhao_mem_share_wr, zhao_part_table_loader and zhao_terrain_pageio -- plus
    # two entries (zhao_console_core, zhao_console_board) that are listed and
    # no longer match the pattern. Measured at the base commit and at HEAD, so
    # this is not an inference. None of the nine is PARAMARENA's, none is
    # repaired here, and saying so is the point: an entry added quietly beside
    # nine unexplained reds reads as though the file were clean.
    "fpga/rtl/geometry/zhao_geom_paramarena.sv",
    "fpga/rtl/geometry/zhao_geom_paramwalk.sv",
    # Three are REAL CLIENTS that test a verdict themselves:
    "fpga/rtl/terrain/zhao_terrain_pagestream.sv",
    "fpga/rtl/mem/zhao_mem_upload.sv",
    "fpga/rtl/memory/zhao_render_asset_mux.sv",
    # Two ROUTE a verdict down to a client and test nothing themselves --
    # the same standing `zhao_video_scanout` is listed under above. Listed
    # rather than excepted, so the audit stays exact.
    "fpga/rtl/generated/zhao_shell_fit_top.sv",
    "fpga/rtl/generated/zhao_shell_v2_fit_top.sv",
    #
    # THE TWO CONSOLE COMPOSERS WERE HERE AND ARE GONE, 2026-09-23. They were
    # added under the sentence above -- "route a verdict down to a client" --
    # and that sentence has since EXPIRED. `zhao_console_core` gave up its
    # guard edge (its own comment records it: "`geom_guard_rsp_o` and the three
    # `geom_beat_*_o` were this module's edge"), so neither file declares a
    # `zhao_guard_rsp_t` input any more and the coverage audit had been calling
    # both stale for weeks.
    #
    # Scanning them bought nothing measurable and the measurement says so:
    # `zhao_console_board` contains ZERO `*rsp*.ready` references, and
    # `zhao_console_core` contains exactly ONE -- `.mem_req_ready_i
    # (mr_guard_rsp.ready)`, a port connection, not a state-machine arm. So the
    # "clean" these two reported was structurally guaranteed, which is the
    # comfortable kind of green this tree keeps learning to distrust.
    #
    # The verdicts inside the core are tested by the LEAVES it instantiates,
    # and every one of those leaves is listed here in its own right.
    # ---------------------------------------------------------------------
    # THE TERRAIN COMPOSE PATH's TWO NEW CLIENTS, 2026-09-19. Added in the
    # change that created them, which is what the note on TERRAIN.PAGELOADER
    # above asks for.
    #
    # MEM.SHARE2 is the generic two-requester / one-permitted-client share
    # (GEOM.MEM.ADAPTER is now a wrapper over it), so the protocol it has to
    # get right is the guard's, once, for both its users.
    "fpga/rtl/memory/zhao_mem_share2.sv",
    # TERRAIN.HDRREAD reads the 64-byte patch header on the COMPOSE path.
    "fpga/rtl/terrain/zhao_terrain_hdrread.sv",

    # ---- THE SEVEN INHERITED CONSUMERS, ADDED 2026-09-23 BY THE COORDINATOR --
    # These were named as known debt in the DEVSTORE entry above and carried as
    # base-red findings for weeks. Listing them is NOT a claim that they are
    # correct -- it is the opposite. The gate only inspects its listed clients,
    # so while these sat outside the list its SILENCE SAID NOTHING ABOUT THEM,
    # and "nine findings" was repeatedly mis-read (by me, in a handover, until
    # corrected) as "nine bugs". It was a COVERAGE hole, and an unmeasured one.
    #
    # Adding them converts an unmeasured hole into a measured answer: whatever
    # this gate now says about these seven is the first real evidence anyone has
    # had. If it reports arms, they are real and inherited; if it reports none,
    # that silence finally means something, because the gate can now see them.
    #
    # The defect it hunts, restated because it is subtle: zhao_mem_guard drives
    # `ready` as a LEVEL and `ok` as a PULSE ONE CYCLE LATER. They are never
    # high together on a passing request, so an arm waiting for BOTH reads
    # EVERY PASS AS A DENIAL -- and that failure is silent and total.
    "fpga/rtl/forge/zhao_forge_pagebank.sv",
    "fpga/rtl/geometry/zhao_geom_clipread.sv",
    "fpga/rtl/geometry/zhao_geom_drawjob.sv",
    "fpga/rtl/geometry/zhao_geom_ladderbank.sv",
    "fpga/rtl/memory/zhao_mem_share_wr.sv",
    "fpga/rtl/particles/zhao_part_table_loader.sv",
    "fpga/rtl/terrain/zhao_terrain_pageio.sv",
]

# `if (foo_rsp.ready)` / `else if (guard_rsp_i.ready)` -- the accepting arm.
READY = re.compile(r"\b(\w*rsp\w*)\.ready\b")
OK = re.compile(r"\b(\w*rsp\w*)\.(ok|violation)\b")


def arm_body(lines, i, col=0):
    """The text of the arm that opens on line i, by brace/begin-end depth.

    Deliberately simple: it walks to the matching `end` of the `begin` that
    opens on or just after the `ready` test, and stops at the next unindented
    state label if there is no `begin`. A one-line arm is its own body.

    `col` IS NOT COSMETIC -- IT IS A HOLE THIS TOOL HAD.
    On the opening line only, the walk starts at the `ready` match rather than
    at column 0, because the commonest arm shape in this tree is

        end else if (guard_rsp_i.ready) begin

    and that line's LEADING `end` closes the PREVIOUS arm. Counted from column
    0 it cancelled this arm's `begin`, depth went straight to zero, the walker
    stopped on the opening line, and the body -- the part that contains the
    `.ok` -- was never read. The tool therefore reported CLEAN on the exact
    defect it exists to catch, in that shape.

    Found 2026-09-06 by deliberately breaking TERRAIN.WRITEBACK and watching the
    alarm NOT go off. A detector that has not been shown to fire has not been
    tested, and this one had been shown to fire only on a shape whose opening
    line has no `end` on it.
    """
    # THE ARM'S SHAPE IS DECIDED BY ITS OPENING LINE AND NOWHERE ELSE.
    #
    # The walk used to infer "this arm has a body" from whichever line carried
    # the first `begin`. For a BRACELESS arm that line is the NEXT CASE ITEM --
    #
    #     S_DRD_REQ:  if (guard_rsp_i.ready) state_q <= S_DRD_VERD;
    #     S_DRD_VERD: begin
    #       if (guard_rsp_i.violation) ...
    #
    # -- so the `not started` break never fired, the walker ran off the end of
    # the arm, and it read the NEIGHBOUR's `.ok` as though it belonged here.
    #
    # That is a FALSE POSITIVE generator, and it fired: on 2026-09-23 the gate
    # reported four `.ok` defects in zhao_terrain_pageio at lines 926, 966,
    # 1151 and 1182 whose four arms are the correct two-state shape this tool
    # exists to PRESCRIBE. Note the direction -- this is the rare break that
    # reads WORSE than the truth, which is why it was caught on first contact
    # instead of living for weeks. See _GOOD_BRACELESS below.
    opening = lines[i][col:]
    braceless = not re.search(r"\bbegin\b", opening)

    depth = 0
    started = False
    out = []
    for k in range(i, min(i + 60, len(lines))):
        t = lines[k][col:] if k == i else lines[k]
        if braceless:
            # A single-statement arm ends at its first `;` and may NEVER cross
            # into a following case item.
            if k > i and re.match(r"\s*(?:\w+|\d+'[bdhoBDHO][0-9a-fA-FxXzZ_]+|default)\s*:(?!:)", t):
                break
            out.append((k + 1, t))
            if ";" in t:
                break
            continue
        out.append((k + 1, t))
        opens = len(re.findall(r"\bbegin\b", t))
        closes = len(re.findall(r"\bend\b(?!case|module|function)", t))
        depth += opens - closes
        if opens:
            started = True
        if started and depth <= 0:
            break
    return out


def in_if_condition(lines, i, col, back=8):
    """Is `lines[i][col]` inside the parenthesised condition of an `if`?

    Walks LEFT from the match, across lines, unwinding one parenthesis level at
    a time. Each unmatched `(` is an enclosing group; if the text before it ends
    in `if` we are in a branch condition, and otherwise we step outside that
    group and keep going. `back` bounds the walk so a runaway file cannot make
    this quadratic; eight lines covers every conditional in this tree and the
    quiescence predicates that motivated the filter are far longer than that.
    """
    depth = 0
    k = i
    seg = lines[i][:col]
    while k >= 0 and (i - k) < back:
        j = len(seg) - 1
        while j >= 0:
            ch = seg[j]
            if ch == ")":
                depth += 1
            elif ch == "(":
                if depth == 0:
                    if re.search(r"\bif\s*$", seg[:j]):
                        return True
                    # not an `if` -- step outside this group and keep looking
                else:
                    depth -= 1
            j -= 1
        k -= 1
        if k >= 0:
            seg = lines[k]
    return False


def scan(path):
    """Return a list of (line, text) offences for one file."""
    try:
        src = io.open(path, encoding="utf-8").read()
    except OSError:
        return [(0, "MISSING FILE -- a client that cannot be read is not a client that passes")]
    # STRIP COMMENTS FIRST. The first version of this tool did not, and it
    # reported three offences that were all PROSE -- including two in the very
    # comments that document the repair, and one in fbwrite's header where it
    # QUOTES the guard line to explain why it gets this right. A gate that
    # flags the documentation of a fix as the fix being absent is worse than
    # no gate: it trains the reader to ignore it.
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    lines = [re.sub(r"//.*", "", ln) for ln in src.split(chr(10))]
    bad = []
    for i, line in enumerate(lines):
        m = READY.search(line)
        if not m:
            continue
        # The declaration itself, and the guard's own driver, are not clients.
        if "output" in line or "assign" in line and "rsp.ready" in line:
            continue
        # THE OFFENCE IS ALWAYS A BRANCH CONDITION, so the `.ready` has to sit
        # inside an `if`. Added 2026-09-19 with the seven clients below, because
        # the first exact coverage run flagged `zhao_render_asset_mux.sv`'s
        # `quiet_o` -- a flat `always_comb` conjunction of a dozen `!x.ready` /
        # `!x.ok` terms describing IDLENESS, which arm_body() then walked as
        # though it were a state arm. That is not a protocol test at all, and a
        # gate that flags a quiescence predicate trains the reader to skip the
        # one line that matters.
        #
        # THE FIRST VERSION OF THIS FILTER WAS `"if" in line[:m.start()]` AND IT
        # SILENCED A TRUE POSITIVE, which is the whole reason it is written out
        # here. `zhao_mem_upload.sv` opens its condition on one line and puts
        # the `.ready && .ok` on the NEXT:
        #
        #     if (guard_wready_i &&
        #         ((beat_q != '0) || (guard_rsp_i.ready && guard_rsp_i.ok))) begin
        #
        # so the offending line has no `if` on it at all, and a same-line test
        # reported the file CLEAN -- a detector going quiet in the flattering
        # direction, in the same edit that revived it. Caught only by asking why
        # a known hit had disappeared. The test below walks LEFT through the
        # open parentheses instead, across lines, which is what "inside an if
        # condition" actually means.
        if not in_if_condition(lines, i, m.start()):
            continue
        sig = m.group(1)
        for (ln, text) in arm_body(lines, i, m.start()):
            mo = OK.search(text)
            if mo and mo.group(1) == sig:
                bad.append((ln, text.strip()))
                break
    return bad


# ---------------------------------------------------------------------------
# THE DETECTOR IS SHOWN TO FIRE BEFORE IT IS TRUSTED.
#
# CLAUDE.md: "A detector that has not been shown to FIRE has not been tested.
# Break it on purpose, watch the alarm go off, put it back." Three tools in this
# tree now self-check at import because one was written with an escape a shell
# heredoc ate, so it matched nothing and printed reassurance for its whole life.
# ---------------------------------------------------------------------------
_BAD = chr(10).join([
    "        S_REQ: if (guard_rsp_i.ready) begin",
    "          if (guard_rsp_i.ok) begin",
    "            st_q <= S_FILL;",
    "          end else begin",
    "            st_q <= S_IDLE;",
    "          end",
    "        end",
])
# THE SHAPE THE TOOL USED TO MISS: the arm's opening line carries the previous
# arm's `end`, which cancelled its own `begin` in the depth walk.
_BAD_ELSE = chr(10).join([
    "          end else if (guard_rsp_i.ready) begin",
    "            if (guard_rsp_i.ok) state <= S_RDATA; else state <= S_IDLE;",
    "          end",
])
_GOOD = chr(10).join([
    "        S_REQ: if (guard_rsp_i.ready) begin",
    "          st_q <= S_VERD;",
    "        end",
    "",
    "        S_VERD: begin",
    "          if (guard_rsp_i.ok) st_q <= S_FILL;",
    "        end",
])
# THE NEGATIVE CONTROL THE TOOL DID NOT HAVE, and the gap is exactly the one a
# self-test is for: _GOOD above covers the correct two-state shape only in its
# BRACED form, so the arm walker's inability to bound a BRACELESS arm was never
# measured. zhao_terrain_pageio writes it braceless in all four of its request
# states, and the gate reported all four as defects on 2026-09-23.
#
# Note the second arm must follow immediately, with no blank line: that
# adjacency IS the test. The walker has to stop at the `;` rather than run on
# into `S_VERD` and read a neighbour's `.ok`.
_GOOD_BRACELESS = chr(10).join([
    "        S_REQ:  if (guard_rsp_i.ready) st_q <= S_VERD;",
    "        S_VERD: begin",
    "          if (guard_rsp_i.violation) begin",
    "            denied_o <= denied_o + 32'd1;",
    "          end else if (guard_rsp_i.ok) begin",
    "            st_q <= S_FILL;",
    "          end",
    "        end",
])
# And the same arm wrapped across two lines, because a braceless arm is bounded
# by its `;`, not by its line.
_GOOD_BRACELESS_WRAPPED = chr(10).join([
    "        S_REQ:",
    "          if (guard_rsp_i.ready)",
    "            st_q <= S_VERD;",
    "        S_VERD: begin",
    "          if (guard_rsp_i.ok) st_q <= S_FILL;",
    "        end",
])
# THE TWO POSITIVE CONTROLS FOR THE NARROWED WALKER, and they are the whole
# reason the repair above is allowed to ship.
#
# Bounding a braceless arm makes the walk read FEWER lines, and reading fewer
# lines is precisely how a detector goes quiet. A fix for a false positive that
# is not paired with a demonstration that the true positive still fires is the
# broken-instrument law being committed on purpose: the gate would go green,
# the four pageio reds would vanish, and nobody would be able to tell that
# outcome apart from the gate having been switched off.
#
# So: the SAME braceless shape, written with the defect in it, must still be
# caught -- on one line, and wrapped across two.
_BAD_BRACELESS = chr(10).join([
    "        S_REQ:  if (guard_rsp_i.ready) if (guard_rsp_i.ok) st_q <= S_FILL; else st_q <= S_IDLE;",
    "        S_VERD: begin",
    "          st_q <= S_IDLE;",
    "        end",
])
_BAD_BRACELESS_WRAPPED = chr(10).join([
    "        S_REQ:",
    "          if (guard_rsp_i.ready)",
    "            if (guard_rsp_i.ok) st_q <= S_FILL;",
    "        S_VERD: begin",
    "          st_q <= S_IDLE;",
    "        end",
])


def _selftest():
    import tempfile
    import os
    cases = (
        ("one-cycle", _BAD, True),
        ("one-cycle, leading `end`", _BAD_ELSE, True),
        ("two-cycle, braced", _GOOD, False),
        ("two-cycle, braceless", _GOOD_BRACELESS, False),
        ("two-cycle, braceless wrapped", _GOOD_BRACELESS_WRAPPED, False),
        ("one-cycle, braceless", _BAD_BRACELESS, True),
        ("one-cycle, braceless wrapped", _BAD_BRACELESS_WRAPPED, True),
    )
    for label, text, want in cases:
        fd, name = tempfile.mkstemp(suffix=".sv")
        os.close(fd)
        io.open(name, "w", encoding="utf-8", newline=chr(10)).write(text)
        got = bool(scan(name))
        os.unlink(name)
        if got != want:
            raise SystemExit(
                "check_guard_verdict SELF-TEST FAILED: the %s example was %s. "
                "The detector is broken and would %s on real files."
                % (label,
                   "missed" if want else "flagged",
                   "report clean" if want else "report defects that are not there"))


def discover():
    """Every RTL file that declares a `zhao_guard_rsp_t` INPUT.

    THE HAND LIST IS THE AUTHORITY AND THIS IS ITS AUDITOR, not the other way
    round. Listing clients deliberately is what stops a scanner whose pattern
    quietly stops matching from reporting "0 problems"; but a hand list also
    rots the moment someone adds a client, and a gate that silently skips the
    one new file is exactly the instrument that reads low.

    So: the list is checked, and a client that is not on it is an ERROR rather
    than something quietly picked up. Adding a guard client is a deliberate act
    and so is putting it under this gate.
    """
    import os
    found = []
    for root, _dirs, files in os.walk("fpga/rtl"):
        for f in files:
            if not f.endswith(".sv"):
                continue
            path = os.path.join(root, f).replace(chr(92), "/")
            try:
                text = io.open(path, encoding="utf-8").read()
            except OSError:
                continue
            text = re.sub(r"//.*", "", text)
            # an INPUT of the response type: this module consumes a verdict.
            # OPTIONAL PACKAGE QUALIFIER. Without it this regex missed
            # `input zhao_pkg::zhao_guard_rsp_t guard_rsp_i` in
            # zhao_debug_frameblit, and reported that file as having STOPPED
            # being a client -- a discovery pattern reading low on its very
            # first run, which is the law this tree keeps relearning.
            if re.search(r"input\s+(var\s+)?(\w+::)?zhao_guard_rsp_t", text):
                found.append(path)
    return sorted(found)


def main():
    _selftest()
    offences = 0

    listed = set(CLIENTS)
    actual = set(discover())
    missing = sorted(actual - listed)
    stale = sorted(listed - actual)
    for path in missing:
        print("check_guard_verdict: %s consumes a guard verdict and is NOT in CLIENTS" % path)
        offences += 1
    for path in stale:
        print("check_guard_verdict: %s is in CLIENTS but no longer consumes a guard verdict" % path)
        offences += 1
    if missing or stale:
        print("")
        print("The client list is checked against the tree so it cannot rot into a")
        print("gate that skips the one file nobody added. Update CLIENTS deliberately.")
        print("")

    for path in CLIENTS:
        bad = scan(path)
        if not bad:
            print("check_guard_verdict: %s clean" % path)
            continue
        for (ln, text) in bad:
            print("check_guard_verdict: %s:%d tests .ok in the SAME arm as .ready" % (path, ln))
            print("    %s" % text)
            offences += 1
    if offences:
        print("")
        print("zhao_mem_guard drives ready as a LEVEL and ok as a PULSE ONE CYCLE LATER.")
        print("They are never high together on a passing request, so this arm reads")
        print("every pass as a denial. Move the verdict into its own state.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
