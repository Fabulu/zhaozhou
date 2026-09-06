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
    # TERRAIN.PAGELOADER. Added with the block, not after it -- the coverage
    # audit is only exact if a new client joins this list in the same change
    # that creates it.
    "fpga/rtl/terrain/zhao_terrain_pageloader.sv",
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
]

# `if (foo_rsp.ready)` / `else if (guard_rsp_i.ready)` -- the accepting arm.
READY = re.compile(r"\b(\w*rsp\w*)\.ready\b")
OK = re.compile(r"\b(\w*rsp\w*)\.(ok|violation)\b")


def arm_body(lines, i):
    """The text of the arm that opens on line i, by brace/begin-end depth.

    Deliberately simple: it walks to the matching `end` of the `begin` that
    opens on or just after the `ready` test, and stops at the next unindented
    state label if there is no `begin`. A one-line arm is its own body.
    """
    depth = 0
    started = False
    out = []
    for k in range(i, min(i + 60, len(lines))):
        t = lines[k]
        out.append((k + 1, t))
        opens = len(re.findall(r"\bbegin\b", t))
        closes = len(re.findall(r"\bend\b(?!case|module|function)", t))
        depth += opens - closes
        if opens:
            started = True
        if started and depth <= 0:
            break
        if not started and k > i:
            # no `begin`: a single-statement arm, already captured
            break
    return out


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
        sig = m.group(1)
        for (ln, text) in arm_body(lines, i):
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
_GOOD = chr(10).join([
    "        S_REQ: if (guard_rsp_i.ready) begin",
    "          st_q <= S_VERD;",
    "        end",
    "",
    "        S_VERD: begin",
    "          if (guard_rsp_i.ok) st_q <= S_FILL;",
    "        end",
])


def _selftest():
    import tempfile
    import os
    for text, want in ((_BAD, True), (_GOOD, False)):
        fd, name = tempfile.mkstemp(suffix=".sv")
        os.close(fd)
        io.open(name, "w", encoding="utf-8", newline=chr(10)).write(text)
        got = bool(scan(name))
        os.unlink(name)
        if got != want:
            raise SystemExit(
                "check_guard_verdict SELF-TEST FAILED: the %s example was %s. "
                "The detector is broken and would report clean on real files."
                % ("one-cycle" if want else "two-cycle", "missed" if want else "flagged"))


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
