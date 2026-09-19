#!/usr/bin/env python3
"""Emit fpga/rtl/prod/zhao_console_board.sv -- THE BOARD AROUND THE CONNECTED MACHINE.

WHY A GENERATOR AND NOT A HAND-WRITTEN FILE
-------------------------------------------
`zhao_console_board.sv`'s own header, from 2026-09-19, listed the three ways a
board can instantiate `zhao_console_core` and refused all three:

  (a) name every port -- a hand copy of a file that grew 687 -> 732 ports in
      forty minutes;
  (b) `zhao_console_core u_core (.*);` -- which binds by NAME and therefore
      needs an identically-named declaration in this scope for every port, so
      it is (a) with extra steps;
  (c) a generated pass-through with a committed generator and a staleness gate,
      the `gen_prod_top.py` pattern -- "the correct long-term answer and it is
      deliberately NOT taken in this pass", because a generator run against a
      file under concurrent rewrite emits a snapshot that is stale within
      minutes.

THE REASON HAS EXPIRED AND IT EXPIRED IN THE DIRECTION THAT FAVOURS (c), NOT
AGAINST IT. The core is still moving -- it was 687 ports, then 732, and it is
1,038 today -- and a moving sibling is precisely the argument FOR a generator.
CLAUDE.md's chapter is unambiguous about the alternative: *"a committed mutant
is a COPY, and a copy goes stale in the flattering direction"*; thirteen
combiner copies and eight AUX-pipe copies went stale while staying GREEN. A
1,038-port hand transcription is the same object with a worse blast radius,
because nothing downstream of it is a test that could go red -- a dropped port
is a `PINMISSING` that only a fit discovers, hours in.

So (c), and the staleness gate is the whole point: when the core's port list
moves and nobody regenerates, `--check` goes RED IN SECONDS instead of the fit
failing at elaboration.

WHAT IS DERIVED AND WHAT IS AUTHORED
------------------------------------
Everything an owner would want to adjust is AUTHORED, in this file, as text:
the prose header, the PLL and reset parameters, the board pins, which
oscillator the console uses and the two module instantiations that make the
clock/reset network. CLAUDE.md: *"never remove the owner's control in the name
of fidelity ... 'this is generated from the reference, so it is not a knob' is
how a wrong number becomes an unadjustable wrong number."* Not one number in
the emitted file was computed by this script.

Exactly two things are DERIVED, and both are pure transcription:

  * the core's parameter list, carried through verbatim WITH ITS COMMENTS, so
    the board can be re-parameterised at its own edge and so port widths
    written in those parameters resolve;
  * the core's port list, carried through verbatim WITH ITS COMMENTS, minus the
    four ports the board drives itself, and re-exported on the board's edge in
    the same direction under the same name.

Re-exporting is not tying off. A port that leaves this module is still a
declared seam with an owner named in the core's own "INCOMPLETE -- TIED OFF,
AND WHY" table; it has simply moved one level out, which is where a board
boundary is supposed to put it. The section comments come across with the
ports, so entry I1's text sits above entry I1's pins at the board edge too.

THE SEAM, AND WHY IT IS FOUR PORTS AND NOT MORE
-----------------------------------------------
`SEAM` below is the ONLY map from a core port to a board net. It is four
entries and every one of them is a rename the board header already settled:
`design/blocks.yml` is the authority on SYS.PLL's output names (`video_clk`),
the shell lineage is the authority on the consumer's (`vid_clk`), and neither
gives because neither has to -- an explicit connection costs nothing and a file
is not allowed to rename another file's port.

`rst_n` takes `rst_n_gpu_o` and NOT a per-domain net, because the core has one
reset domain and says so. `zhao_sys_reset`'s other three outputs stay on the
board's edge rather than being connected to something that would drop them: a
seam that accepts a per-domain reset and ignores it is a lie, and the repair is
a `zhao_shell_top_v2` packet, not a board one.

USAGE
    python tools/quartus/gen_console_board.py            # write the file
    python tools/quartus/gen_console_board.py --check    # RC 1 if stale
"""

from __future__ import annotations

import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CORE_PATH = os.path.join(ROOT, "fpga", "rtl", "prod", "zhao_console_core.sv")
OUT_PATH = os.path.join(ROOT, "fpga", "rtl", "prod", "zhao_console_board.sv")

CORE_MODULE = "zhao_console_core"
BOARD_MODULE = "zhao_console_board"

# THE ONLY FOUR CORE PORTS THE BOARD DRIVES. Everything else is re-exported.
# Keep this map small and explicit: every entry added here is a seam the board
# claims to own, and a wrong one is invisible -- the elaboration succeeds and
# the machine is clocked by the wrong thing.
SEAM = {
    "gpu_clk":   "gpu_clk_o",
    "vid_clk":   "video_clk_o",
    "audio_clk": "audio_clk_o",
    "rst_n":     "rst_n_gpu_o",
}

# Board-side nets that are NOT core ports. A core port arriving with one of
# these names would be silently shorted to the clock tree, so the generator
# refuses instead -- this is the collision check that makes the verbatim
# transcription safe.
BOARD_OWN_NETS = {
    "fpga_clk1_50_i", "fpga_clk2_50_i", "fpga_clk3_50_i",
    "hard_reset_n_i", "pll_arst_i",
    "gpu_clk_o", "sdram_clk_o", "video_clk_o", "audio_clk_o", "pll_locked_o",
    "rst_n_gpu_o", "rst_n_sdram_o", "rst_n_video_o", "rst_n_audio_o",
    "seq_done_o", "pll_lock_lost_o", "reset_assertions_o",
    "unused_board_clk_w",
}

BOARD_OWN_PARAMS = [
    "REF_CLK_HZ", "GPU_CLK_HZ", "SDRAM_CLK_HZ", "VIDEO_CLK_HZ", "AUDIO_CLK_HZ",
    "SIM_GPU_DIV", "SIM_SDRAM_DIV", "SIM_VIDEO_DIV", "SIM_AUDIO_DIV",
    "SIM_LOCK_CYCLES", "LOST_W",
    "RELEASE_SPAN", "RELEASE_STEP_SDRAM", "RELEASE_STEP_GPU",
    "RELEASE_STEP_VIDEO", "RELEASE_STEP_AUDIO", "SYNC_STAGES", "ASSERT_W",
]


# ---------------------------------------------------------------------------
# PARSING
#
# Everything structural runs against a MASK -- the file with every comment
# replaced by spaces of the same length -- so offsets into the mask are offsets
# into the original and the slices that get emitted keep their comments. Cutting
# on the stripped text and emitting from it is how a port list loses the section
# headers that say who owns each seam.
# ---------------------------------------------------------------------------

def mask_comments(text: str) -> str:
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = " "
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != "\n":
                    out[k] = " "
            i = j
        elif text[i] in "\"":
            j = i + 1
            while j < n and text[j] != "\"":
                j += 2 if text[j] == "\\" else 1
            i = min(j + 1, n)
        else:
            i += 1
    return "".join(out)


def match_paren(mask: str, open_at: int) -> int:
    """Index of the `)` closing the `(` at `open_at`."""
    assert mask[open_at] == "(", "match_paren must start on an open paren"
    depth = 0
    for j in range(open_at, len(mask)):
        if mask[j] == "(":
            depth += 1
        elif mask[j] == ")":
            depth -= 1
            if depth == 0:
                return j
    raise ValueError("unbalanced parentheses")


def split_top_level(text: str, mask: str) -> list[str]:
    """Split a parameter or port list on its TOP-LEVEL commas.

    Nested commas appear inside `{...}`, `[...]` and `(...)` -- an unpacked
    dimension, a concatenation default, a `$clog2(a, b)`-shaped call -- and a
    naive split shreds them into fragments that parse as ports.
    """
    items, depth, start = [], 0, 0
    for i, c in enumerate(mask):
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        elif c == "," and depth == 0:
            items.append(text[start:i])
            start = i + 1
    tail = text[start:]
    if tail.strip():
        items.append(tail)
    return items


_IDENT = re.compile(r"[A-Za-z_][A-Za-z_0-9$]*")


def declared_name(item_mask: str) -> str | None:
    """The identifier a declaration DECLARES.

    For a parameter it is the identifier immediately before the `=`; for a port
    it is the last identifier in the item. Taking "the last identifier" for
    both is wrong and wrong quietly: `parameter int unsigned X = $clog2(DEPTH)`
    would declare `DEPTH`.
    """
    head = item_mask.split("=", 1)[0]
    # Drop packed/unpacked dimensions: their contents are expressions, and the
    # name is never inside one.
    head = re.sub(r"\[[^\]]*\]", " ", head)
    names = _IDENT.findall(head)
    return names[-1] if names else None


class CoreInterface:
    def __init__(self, imports: str, params: list[str], ports: list[str],
                 param_names: list[str], port_names: list[str]) -> None:
        self.imports = imports
        self.params = params
        self.ports = ports
        self.param_names = param_names
        self.port_names = port_names


def read_core(path: str = CORE_PATH, module: str = CORE_MODULE) -> CoreInterface:
    text = io.open(path, encoding="utf-8", errors="replace").read()
    mask = mask_comments(text)

    m = re.search(r"\bmodule\s+" + re.escape(module) + r"\b", mask)
    if m is None:
        raise SystemExit("gen_console_board: no `module %s` in %s" % (module, path))
    i = m.end()

    # A module may import packages BEFORE its parameter block. Missing this
    # makes the PARAMETER list parse as the port list -- which comes back with
    # plausible-looking contents and is entirely the wrong list.
    imports = ""
    while True:
        imp = re.match(r"\s*import\b[^;]*;", mask[i:])
        if imp is None:
            break
        imports += text[i + imp.start():i + imp.end()]
        i += imp.end()

    params: list[str] = []
    hash_at = mask.find("#", i)
    paren_at = mask.find("(", i)
    if hash_at != -1 and hash_at < paren_at:
        popen = mask.index("(", hash_at)
        pclose = match_paren(mask, popen)
        body, bmask = text[popen + 1:pclose], mask[popen + 1:pclose]
        params = split_top_level(body, bmask)
        i = pclose + 1

    popen = mask.index("(", i)
    pclose = match_paren(mask, popen)
    body, bmask = text[popen + 1:pclose], mask[popen + 1:pclose]
    ports = split_top_level(body, bmask)

    # Names come from the MASKED item, so an identifier that appears only in a
    # comment cannot be mistaken for the declaration. This is the difference
    # between a parser and a grep, and it is the one the section headers in the
    # core's port list would break.
    param_names = [declared_name(mask_comments(x)) for x in params]
    port_names = [declared_name(mask_comments(x)) for x in ports]

    if any(p is None for p in param_names):
        raise SystemExit(
            "gen_console_board: could not name every parameter of %s -- item %d"
            % (module, param_names.index(None)))
    if any(p is None for p in port_names):
        raise SystemExit(
            "gen_console_board: could not name every port of %s -- item %d"
            % (module, port_names.index(None)))

    return CoreInterface(imports, params, ports, param_names, port_names)


# ---------------------------------------------------------------------------
# THE AUTHORED TEXT.  Nothing below is derived from anything.
# ---------------------------------------------------------------------------

HEADER = r"""// zhao_console_board.sv -- THE BOARD FRAMEWORK around the connected machine.
//
// GENERATED by tools/quartus/gen_console_board.py from zhao_console_core.sv.
// DO NOT EDIT THE PORT LIST OR THE `u_core` INSTANCE BY HAND. Everything else
// -- this header, the parameters, the board pins, the PLL and the reset
// sequencer -- is AUTHORED IN THE GENERATOR and is where an owner changes a
// number. Regenerate with:
//
//     python tools/quartus/gen_console_board.py
//
// and the `console_board_generated_freshness` ctest fails if you do not.
//
//   reports/Zhaozhou_True_Console_Completion_Plan_2026-09-18.txt S1.3:
//     "zhao_console_core:  the connected machine, with platform transaction
//                          interfaces.
//      zhao_console_board: the selected real FPGA/board framework around that
//                          core."
//
// This is the second of those two tops, and as of 2026-09-19 IT CONTAINS THE
// FIRST. `zhao_console_core` takes clocks and resets as INPUTS; something has
// to make them, and S12.2 says what: "use vendor PLL primitives behind
// wrappers, with model equivalents in simulation. Generate documented
// GPU/video/audio clocks from the real oscillator. Assert reset on external
// reset/unlocked PLL as appropriate; synchronize deassertion in each domain."
// That is this file, and it is exactly SYS.PLL + SYS.RESET -- now with the
// machine those clocks were made for actually attached to them.
//
// ===========================================================================
// THE BOARD IT IS THE FRAMEWORK FOR -- MEASURED
// ===========================================================================
// `zhaozhou-board-bringup-20260913/reports/board_truth.json`, captured
// 2026-09-13 at commit eee32c4e, SuperStation One (RCSH-1001/1002):
//
//     FPGA_CLK1_50   50 MHz   pin V11   3.3-V LVTTL
//     FPGA_CLK2_50   50 MHz   pin Y13   3.3-V LVTTL
//     FPGA_CLK3_50   50 MHz   pin E11   3.3-V LVTTL
//     buildTarget    5CSEBA6U23I7, UFBGA-672, speed grade 7
//     reset          "MiSTer HPS/framework RESET, synchronized release after
//                     core PLL lock"
//
// NO PIN MAP IS INVENTED HERE. The pin names above are recorded as comments on
// the ports that carry them; the assignment itself belongs in a QSF that does
// not exist yet, and the completion plan S12.1 is explicit that "no pin map is
// invented here."
//
// ===========================================================================
// WHAT THIS FILE DOES NOW: IT INSTANTIATES zhao_console_core
// ===========================================================================
// The previous revision of this header carried a section titled "WHAT THIS
// FILE DELIBERATELY DOES NOT DO: INSTANTIATE zhao_console_core". It is kept
// here in summary rather than deleted, because the reason it gave was honest
// and the way it EXPIRED is the instructive part:
//
//   "`fpga/rtl/prod/zhao_console_core.sv` declared 687 ports when this pass
//    started and 732 forty minutes later."
//
// and SystemVerilog has no port forwarding, so instantiating it meant a hand
// copy of a moving file -- the stale-copy failure CLAUDE.md has a chapter
// about. That was correct. What was NOT correct was the conclusion drawn from
// it. The file has not stopped moving; it declares 1,038 ports today. A
// sibling that moves is the argument FOR the generator, which that same header
// called "the correct long-term answer", and against BOTH hand forms.
//
// So the seam is soldered and the solder is machine-made:
//
//   * every core port is re-exported on this module's edge, same name, same
//     direction, same width, WITH THE CORE'S OWN SECTION COMMENTS, so entry I1
//     is still labelled entry I1 out here;
//   * every core parameter is re-declared here and passed back explicitly, so
//     the board is re-parameterisable and the widths above it resolve;
//   * the four clock/reset ports are the ONLY connections this file invents,
//     and they are the ones the previous header already settled.
//
// RE-EXPORTING IS NOT TYING OFF, and the distinction is the whole reason this
// is allowed. A tie-off drives a constant into a port and tells the fitter the
// producer does not exist. A re-export moves the seam one level out and leaves
// it a seam: `part_fld_valid_i` is as unowned at the board's edge as it was at
// the core's, it is listed in the core's "INCOMPLETE -- TIED OFF, AND WHY"
// table under the owner that is missing, and NOTHING NEW became fake. This
// pass adds ZERO tie-offs and removes none.
//
// ---------------------------------------------------------------------------
// THE THREE PORT FACTS -- STILL TRUE, AND NOW LOAD-BEARING
// ---------------------------------------------------------------------------
//   1. THE `rst_n_vid` / `rst_n_audio` GAP IS NOT IN THE CORE.
//
//      This block produces four resets and the core takes one. The conclusion
//      that the core "needs two more ports" was wrong, and measuring the core
//      says why. RE-MEASURED 2026-09-19 RATHER THAN COPIED FORWARD, because an
//      inherited count is exactly the kind of claim this tree keeps finding
//      stale -- and one half of it HAD gone stale:
//
//        * `vid_clk`/`audio_clk` appear in exactly FOUR places, unchanged: the
//          two port declarations and the two connections to `u_shell`.
//        * the earlier header said every `always_ff` is `@(posedge gpu_clk or
//          negedge rst_n)`, "four of four, no exceptions". There are SIX now
//          and one of them is `@(posedge gpu_clk)` with no reset edge at all.
//          It is the GEOM.ATTRPACK/GEOM.SETUP lockstep assertion behind
//          `// synthesis translate_off`, so it is not in the fabric -- but
//          "no exceptions" was still the wrong sentence to carry forward.
//
//      The CONCLUSION survives the correction and is in fact stronger: all six
//      are on `gpu_clk`. The core has no video or audio domain of its own; its
//      single vid/audio consumer is `u_shell`, and `zhao_shell_top_v2.sv`
//      declares exactly one `rst_n`. Two new core ports would terminate
//      nowhere, and a seam that accepts a per-domain reset and drops it is a
//      lie.
//
//      SO `rst_n_video_o` AND `rst_n_audio_o` STAY ON THIS MODULE'S EDGE,
//      UNCONNECTED TO THE CORE, AND THAT IS THE HONEST STATE. They are real
//      outputs of a real sequencer with nothing downstream that can use them
//      yet. The defect is one level down and it is an uncashed cheque: the
//      per-domain reset ports ALREADY EXIST on the shell's own children and
//      the shell feeds every one of them the same net --
//
//        zhao_shell_top_v2.sv  .gpu_rst_n(rst_n), .vid_rst_n(rst_n)  u_fb_cdc
//                              .gpu_rst_n(rst_n), .vid_rst_n(rst_n)  u_ready_bridge
//                              .src_rst_n(rst_n), .dst_rst_n(rst_n)  u_starve_mbx
//                              .rst_audio_n(rst_n)                   audio fifo
//
//      -- plus the pure vid-domain `u_mode`, `u_scaler`, `u_framectl`, `u_crc`.
//      Somebody built the split and nobody ever drove it. Closing it is a
//      `zhao_shell_top_v2` packet: two new shell ports, about a dozen rewires,
//      and a reset-port SPLIT in `zhao_video_scanout`, which is genuinely
//      dual-domain. The core and this board then follow in two lines each --
//      and on this board the two lines are two entries in the generator's
//      `SEAM` map, which is the whole edit.
//
//   2. `sdram_clk` DOES NOT BELONG ON THE CORE YET, AND THE REASON IS NOT
//      "NOBODY ASKED FOR IT".
//
//      MEM.SDRAM is not outside the core at all: `zhao_sdram_ctrl` is
//      instantiated INSIDE it, by the shell, as `.clk(gpu_clk), .rst_n(rst_n)`.
//      The real question is whether the controller moves into its own domain,
//      and today it does not -- this board's own parameters set SDRAM_CLK_HZ ==
//      GPU_CLK_HZ == 100 MHz, so a separate clock buys nothing; moving it
//      demands a real CDC between `zhao_vram_arbiter` (gpu_clk) and the
//      controller, and `zhao_sdram_ctrl`'s FROZEN LAW TABLE is a cycle-exact
//      contract mirrored by `zref::SdramController`; `design/blocks.yml` holds
//      MEM.SDRAM at maturity SPECIFIED, blocked_on: hardware until ZH-004
//      delivers device code, speed grade and measured tRCD/tRP/tRC; and the PHY
//      pins do not reach the core boundary at all.
//
//      So `sdram_clk_o` stays an output of this block, available the day
//      MEM.SDRAM's domain question is actually answered.
//
//   3. BOTH NAMES STAY. THE SEAM IS ALLOWED TWO NAMES; A FILE IS NOT ALLOWED
//      TO RENAME ANOTHER FILE'S PORT.
//
//      The PRODUCER side keeps `video_clk`: `design/blocks.yml` gives SYS.PLL
//      `outputs: [gpu_clk, sdram_clk, video_clk, audio_clk, pll_locked]` and
//      the ledger is the authority on a block's declared ports. The CONSUMER
//      side keeps `vid_clk`: `zhao_console_core.sv` carries
//      `zhao_shell_top_v2`'s declaration through VERBATIM by its own stated
//      rule, and the shell has said `vid_clk` since W2.
//
//      This is why `.*` is the WRONG FORM here even though the smoke bench
//      uses it. `.*` binds each port to an identically-named net, so it cannot
//      express a seam whose two sides are deliberately named differently
//      without four explicit overrides sitting beside it -- and it would still
//      need every one of the other 1,034 names declared in this scope. Named
//      connection says what is connected to what, at every one of 1,038 ports,
//      and a generator pays no price for the verbosity. The bench may use `.*`
//      because a bench is elaborated by Verilator alone; this file is meant for
//      `quartus_map`, and an explicit port map is the form no tool argues with.
//
// ===========================================================================
// WHAT IS STILL MISSING FROM "THE BOARD BOUNDARY", listed so its absence is not
// mistaken for coverage (completion plan S12.1 C36 is the full list):
// ===========================================================================
//   * pin assignments, I/O standards, drive strengths            -- no QSF
//   * an SDC. NOTHING here is constrained: not the three input clocks, not the
//     generated clocks, not the ref->domain crossings at the reset
//     synchronisers, not recovery/removal on any downstream reset flop
//   * the MiSTer sys/ framework, HPS transport and the three bridges -- so the
//     1,034 re-exported core ports leave this module and stop, which is what a
//     board boundary looks like before its transports exist
//   * SDRAM pins and calibration (MEM.SDRAM, ZH-004 seam)
//   * video output, audio output, controller input, SNAC
//   * `fpga/Zhaozhou.sv` is still the W1 placeholder and is untouched by this
//
// ===========================================================================
// NO FIT, NO MAP, NO BOARD.
// ===========================================================================
// This file has never been through `quartus_map` and has no fit row. CLAUDE.md:
// a block that has never been through quartus_map has not been shown to be
// synthesizable, however clean its lint. And the board carries an explicit
// review hold -- board_truth.json `reviewHold.futurePhysicalLoadsAuthorized:
// false`, seven prerequisites -- so nothing here has been, or may be, loaded.
//
// ===========================================================================
// AND THE COMPLETION REGISTER STILL CANNOT SEE SYS.PLL OR SYS.RESET.
// ===========================================================================
// Said here rather than left for someone to discover. `tools/budget/
// completion_register.py` answers "is this capability connected?" by reading
// the source list under `- top: zhao_console_core` in `design/fit_targets.yml`
// -- the CORE's closure, not the machine's. `zhao_sys_pll` and `zhao_sys_reset`
// are composed HERE, one level above that root, so they remain in the
// register's DISCONNECTED bucket and the mandatory-gap count does not move.
//
// That is a fact about the instrument's root, not about the design, and it must
// not be fixed by quietly re-pointing the root: doing so would change what
// "connected" means for all 77 mandatory capabilities at once, in the
// direction that makes the number smaller, which is the direction CLAUDE.md
// says to check hardest. `check_console_inventory.py` DOES see them -- its
// reachability walk has this module as a second root -- so they are no longer
// "built and not composed" anywhere a gate can read it.
"""

MODULE_OPEN = """
module zhao_console_board
%(imports)s#(
    // ======================================================================
    // THE BOARD'S OWN KNOBS.
    //
    // Forwarded verbatim to SYS.PLL. Every one of them is documented at its
    // declaration in `fpga/rtl/sys/zhao_sys_pll.sv`, with its MEASURED /
    // DERIVED / UNSETTLED tag and the evidence for it. They are repeated here
    // as overridable parameters and NOT re-documented, because two copies of a
    // justification is how one of them goes stale.
    // ======================================================================
    parameter int unsigned REF_CLK_HZ         = 50_000_000,
    parameter int unsigned GPU_CLK_HZ         = 100_000_000,
    parameter int unsigned SDRAM_CLK_HZ       = 100_000_000,
    parameter int unsigned VIDEO_CLK_HZ       = 0,             // UNSETTLED
    parameter int unsigned AUDIO_CLK_HZ       = 12_288_000,
    parameter int unsigned SIM_GPU_DIV        = 1,
    parameter int unsigned SIM_SDRAM_DIV      = 1,
    parameter int unsigned SIM_VIDEO_DIV      = 2,
    parameter int unsigned SIM_AUDIO_DIV      = 4,
    parameter int unsigned SIM_LOCK_CYCLES    = 8,
    parameter int unsigned LOST_W             = 16,

    // Forwarded verbatim to SYS.RESET.
    parameter int unsigned RELEASE_SPAN       = 16,
    parameter int unsigned RELEASE_STEP_SDRAM = 0,
    parameter int unsigned RELEASE_STEP_GPU   = 3,
    parameter int unsigned RELEASE_STEP_VIDEO = 6,
    parameter int unsigned RELEASE_STEP_AUDIO = 9,
    parameter int unsigned SYNC_STAGES        = 2,
    parameter int unsigned ASSERT_W           = 16,

    // ======================================================================
    // THE CORE'S OWN KNOBS, CARRIED THROUGH VERBATIM -- GENERATED.
    //
    // Not decoration. A re-exported port whose width is written in the core's
    // parameters cannot be declared out here without them, and re-declaring
    // them rather than hard-coding the widths is what keeps the board
    // re-parameterisable: override PART_REC_W on this module and the edge, the
    // instance and the core all move together.
    //
    // Each is passed back to `u_core` explicitly below, so a parameter this
    // file fails to forward is a WIDTH MISMATCH the compiler reports, not a
    // silently truncated bus.
    // ======================================================================
%(core_params)s
) (
    // ======================================================================
    // THE BOARD PINS -- MEASURED, board_truth.json clocks.fpgaInputs. The pin
    // names are recorded here; the ASSIGNMENT belongs in a QSF that does not
    // exist.
    // ======================================================================
    input  logic fpga_clk1_50_i,     // 50 MHz, pin V11, 3.3-V LVTTL
    input  logic fpga_clk2_50_i,     // 50 MHz, pin Y13, 3.3-V LVTTL
    input  logic fpga_clk3_50_i,     // 50 MHz, pin E11, 3.3-V LVTTL

    // MEASURED as a CONTRACT, not as a pin: board_truth.json reset.coreContract
    // "MiSTer HPS/framework RESET, synchronized release after core PLL lock".
    // reset.physicalButtonMapping is null and `physical_button_reset_mapping`
    // is still in openCapabilities -- WHICH pin drives this is unanswered.
    input  logic hard_reset_n_i,

    // The vendor PLL's own asynchronous, active-high reset. See SYS.PLL.md: it
    // is a real port and it is also the only legal stimulus that fires
    // pll_lock_lost_o.
    input  logic pll_arst_i,

    // ======================================================================
    // THE CLOCK/RESET NETWORK THIS FRAMEWORK EXISTS TO MAKE.
    //
    // These are OUTPUTS and they are also what drives `u_core` below. They stay
    // on the edge -- rather than becoming internal wires now that there is a
    // consumer -- because they are what an SDC will have to constrain and what
    // a board-level test has to observe. `rst_n_video_o` and `rst_n_audio_o`
    // in particular go NOWHERE inside this module, and port fact 1 in the
    // header is why: the core has one reset domain and honestly says so.
    // ======================================================================
    output logic gpu_clk_o,
    output logic sdram_clk_o,
    output logic video_clk_o,
    output logic audio_clk_o,
    output logic pll_locked_o,

    // ---- SYNCASYNCNET, AND IT IS A FINDING THE JOIN CREATED ----------------
    // `rst_n_gpu_o` is the console's whole reset net, and Verilator reports it
    // "flopped as both synchronous and async". THIS WARNING CANNOT EXIST UNTIL
    // THE BOARD INSTANTIATES THE CORE: linting `zhao_console_core` alone is
    // SILENT at RC 0, because there `rst_n` is a top-level input whose driver
    // the tool cannot see. Joining the two tops is what put a producer and a
    // consumer on one net, and the first thing that fell out of it is a real
    // structural fact about the machine.
    //
    // MEASURED, not asserted: `python tools/quartus/reset_discipline_census.py`
    // walks the console's own fit closure and reports the split. At 1e77bb35 it
    // is 19 modules holding 43 `always_ff` blocks that read this net as a
    // SYNCHRONOUS reset -- zhao_texture_v3own, zhao_part_table,
    // zhao_vertex_arena, zhao_project_core, zhao_field_v3_curve and fifteen
    // more -- against the core's own four, which are asynchronous.
    //
    // IT IS WAIVED AND NOT REPAIRED, and the reason is that the board is not
    // where it could be repaired. Mixed discipline on one net is safe exactly
    // when the release is synchronised per domain and long enough for every
    // synchronous user to see it, which is what `zhao_sys_reset` is for:
    // SYNC_STAGES=2 into each domain and RELEASE_SPAN=16 reference cycles of
    // stagger. Changing the discipline would be a 19-module edit against
    // blocks with their own frozen contracts, decided by the reset owner and
    // not by the file that happens to elaborate them all first.
    //
    // The precedent is this tree's own: `tests/shell/v3_closure_inherited.vlt`
    // already carries `lint_off -rule SYNCASYNCNET -file "*zhao_shell_top_v2.sv"`
    // for the same net one level down. The waiver is LOCAL rather than another
    // line in that file so that it sits beside the reason, and so that removing
    // it to re-measure is a one-line edit in the generator.
    /* verilator lint_off SYNCASYNCNET */
    output logic rst_n_gpu_o,
    /* verilator lint_on SYNCASYNCNET */
    output logic rst_n_sdram_o,
    output logic rst_n_video_o,
    output logic rst_n_audio_o,

    // ---- the platform census ----------------------------------------------
    output logic                seq_done_o,
    output logic [LOST_W-1:0]   pll_lock_lost_o,
    output logic [ASSERT_W-1:0] reset_assertions_o,

    // ======================================================================
    // THE CORE'S BOUNDARY, RE-EXPORTED -- GENERATED.
    //
    // Same name, same direction, same width, and the core's own section
    // comments come with them so each seam keeps the entry number and the
    // owner it is listed under in `zhao_console_core.sv`'s "INCOMPLETE --
    // TIED OFF, AND WHY" table.
    //
    // These are NOT tie-offs and NOT stimulus. Each one is a seam whose
    // producer or consumer does not exist yet, moved one level out to where a
    // board boundary belongs. When the MiSTer transports arrive they attach
    // here; until then they leave this module and stop, which is visible and
    // honest, and is the opposite of a constant standing in for a producer.
    // ======================================================================
%(core_ports)s
);
"""

BODY_PLATFORM = """
  // ---------------------------------------------------------------------------
  // THE CONSOLE USES ONE REFERENCE.
  //
  // FPGA_CLK1_50 feeds the PLL and the reset sequencer's stagger counter. CLK2
  // and CLK3 are real board pins and are carried here so that this module
  // describes the board it is the framework for -- but a second PLL on a second
  // oscillator would be a SECOND FREQUENCY AUTHORITY, which is the one thing
  // SYS.PLL's contract exists to forbid. They are explicitly sunk rather than
  // dropped from the port list, so that "the console ignores two of the three
  // oscillators" is a visible decision instead of an absent line.
  // ---------------------------------------------------------------------------
  wire unused_board_clk_w = &{1'b0, fpga_clk2_50_i, fpga_clk3_50_i};

  zhao_sys_pll #(
      .REF_CLK_HZ      (REF_CLK_HZ),
      .GPU_CLK_HZ      (GPU_CLK_HZ),
      .SDRAM_CLK_HZ    (SDRAM_CLK_HZ),
      .VIDEO_CLK_HZ    (VIDEO_CLK_HZ),
      .AUDIO_CLK_HZ    (AUDIO_CLK_HZ),
      .SIM_GPU_DIV     (SIM_GPU_DIV),
      .SIM_SDRAM_DIV   (SIM_SDRAM_DIV),
      .SIM_VIDEO_DIV   (SIM_VIDEO_DIV),
      .SIM_AUDIO_DIV   (SIM_AUDIO_DIV),
      .SIM_LOCK_CYCLES (SIM_LOCK_CYCLES),
      .LOST_W          (LOST_W)
  ) u_pll (
      .ref_clk_i       (fpga_clk1_50_i),
      // The board hard reset also clears the PLL block's housekeeping. It does
      // NOT reset the PLL itself -- that is pll_arst_i, and keeping the two
      // apart is what lets pll_lock_lost_o survive the event it counts.
      .arst_n_i        (hard_reset_n_i),
      .pll_arst_i      (pll_arst_i),
      .gpu_clk_o       (gpu_clk_o),
      .sdram_clk_o     (sdram_clk_o),
      .video_clk_o     (video_clk_o),
      .audio_clk_o     (audio_clk_o),
      .pll_locked_o    (pll_locked_o),
      .pll_lock_lost_o (pll_lock_lost_o)
  );

  zhao_sys_reset #(
      .RELEASE_SPAN       (RELEASE_SPAN),
      .RELEASE_STEP_SDRAM (RELEASE_STEP_SDRAM),
      .RELEASE_STEP_GPU   (RELEASE_STEP_GPU),
      .RELEASE_STEP_VIDEO (RELEASE_STEP_VIDEO),
      .RELEASE_STEP_AUDIO (RELEASE_STEP_AUDIO),
      .SYNC_STAGES        (SYNC_STAGES),
      .ASSERT_W           (ASSERT_W)
  ) u_reset (
      // The stagger counter runs on the REFERENCE, not on gpu_clk: it has to
      // be counting while the PLL is unlocked, which is the whole situation it
      // exists to come out of.
      .ref_clk_i          (fpga_clk1_50_i),
      .pll_locked_i       (pll_locked_o),
      .hard_reset_n_i     (hard_reset_n_i),
      .gpu_clk_i          (gpu_clk_o),
      .sdram_clk_i        (sdram_clk_o),
      .video_clk_i        (video_clk_o),
      .audio_clk_i        (audio_clk_o),
      .rst_n_gpu_o        (rst_n_gpu_o),
      .rst_n_sdram_o      (rst_n_sdram_o),
      .rst_n_video_o      (rst_n_video_o),
      .rst_n_audio_o      (rst_n_audio_o),
      .seq_done_o         (seq_done_o),
      .reset_assertions_o (reset_assertions_o)
  );
"""

SEAM_NOTE = """
  // ===========================================================================
  // THE SEAM -- SOLDERED.
  //
  // Four connections are AUTHORED and 1,034 are TRANSCRIBED. The four are the
  // only place this file decides anything:
  //
  //   .gpu_clk   (gpu_clk_o)     the console's one GPU domain
  //   .vid_clk   (video_clk_o)   two legitimate names, one wire -- port fact 3
  //   .audio_clk (audio_clk_o)
  //   .rst_n     (rst_n_gpu_o)   CORRECT ONLY for gpu, and the core claims no
  //                              more than that. Video and audio are reset
  //                              from this net INSIDE the shell; splitting
  //                              them is a zhao_shell_top_v2 packet, and when
  //                              it lands this map grows by two lines.
  //
  // Everything else is `.x (x)`. That is deliberately boring: a pass-through
  // whose two sides are the same name cannot be transcribed wrong in a way
  // that elaborates, and the freshness gate catches the way it CAN go wrong,
  // which is a port that appeared in the core after this file was written.
  // ===========================================================================
"""


def render(core: CoreInterface) -> str:
    # ---- collision check ---------------------------------------------------
    # A core port sharing a name with a board net would be shorted to the clock
    # tree by the very `.x (x)` transcription that makes this safe. It has never
    # happened and that is exactly why it is checked here rather than trusted.
    collisions = sorted(set(core.port_names) & BOARD_OWN_NETS)
    if collisions:
        raise SystemExit(
            "gen_console_board: core port(s) %s collide with the board's own "
            "nets. Rename on one side or add an explicit SEAM entry; do NOT "
            "let the transcription short them together." % ", ".join(collisions))
    pcoll = sorted(set(core.param_names) & set(BOARD_OWN_PARAMS))
    if pcoll:
        raise SystemExit(
            "gen_console_board: core parameter(s) %s collide with the board's "
            "own PLL/reset parameters." % ", ".join(pcoll))
    missing = sorted(set(SEAM) - set(core.port_names))
    if missing:
        raise SystemExit(
            "gen_console_board: SEAM names %s which %s does not declare. The "
            "clock/reset contract moved and this map has to move with it."
            % (", ".join(missing), CORE_MODULE))

    # ---- the re-exported port list ----------------------------------------
    #
    # A dropped port takes its LEADING COMMENT with it unless that comment is
    # salvaged, and in this file the four dropped ports sit directly under
    # "zhao_shell_top_v2's DECLARATION, CARRIED THROUGH VERBATIM" -- a section
    # header that owns the ~200 ports AFTER them. Losing it would silently
    # unlabel the largest seam on the board's edge, which is the kind of loss
    # that reads as tidy output.
    kept: list[str] = []
    pending = ""
    dropped: list[str] = []
    for name, item in zip(core.port_names, core.ports):
        lead_end = len(item) - len(mask_comments(item).lstrip())
        if name in SEAM:
            pending += item[:lead_end]
            dropped.append(name)
            continue
        if pending:
            # Each dropped item contributed its own leading blank line, so the
            # salvage is a run of whitespace-only lines with the real comment
            # inside it. Normalise rather than emit four blank lines that look
            # like a deletion nobody meant.
            salvaged = "\n".join(x.rstrip() for x in pending.splitlines())
            salvaged = salvaged.rstrip("\n ")
            note = ("  // %s: DRIVEN BY THIS BOARD and therefore NOT re-exported.\n"
                    "  // They are the whole of the seam -- see THE SEAM below.\n"
                    % ", ".join(dropped))
            item = salvaged + "\n" + note + item.lstrip("\n")
            pending, dropped = "", []
        kept.append(item)
    if pending:
        raise SystemExit("gen_console_board: a seam port is the LAST port of %s; "
                         "its salvaged comment has nowhere to go." % CORE_MODULE)

    ports_text = ",".join(x.rstrip() for x in kept).strip("\n")
    ports_text = re.sub(r"^\s*\n", "", ports_text)

    params_text = ",".join(x.rstrip() for x in core.params).strip("\n")
    params_text = re.sub(r"^\s*\n", "", params_text)

    imports = core.imports.strip()
    imports = ("  " + imports + "\n") if imports else ""

    out = [HEADER]
    out.append(MODULE_OPEN % {
        "imports": imports,
        "core_params": params_text,
        "core_ports": ports_text,
    })
    out.append(BODY_PLATFORM)
    out.append(SEAM_NOTE)

    # ---- the instance ------------------------------------------------------
    pw = max(len(p) for p in core.param_names)
    nw = max(len(n) for n in core.port_names)
    lines = ["  " + CORE_MODULE + " #("]
    for i, p in enumerate(core.param_names):
        sep = "," if i < len(core.param_names) - 1 else ""
        lines.append("      .%-*s (%s)%s" % (pw, p, p, sep))
    lines.append("  ) u_core (")
    for i, n in enumerate(core.port_names):
        sep = "," if i < len(core.port_names) - 1 else ""
        lines.append("      .%-*s (%s)%s" % (nw, n, SEAM.get(n, n), sep))
    lines.append("  );")
    out.append("\n".join(lines) + "\n")
    out.append("\nendmodule : " + BOARD_MODULE + "\n")
    return "".join(out)


def _self_test() -> None:
    """The parser must be SEEN to work, and seen to notice a fault.

    CLAUDE.md: a detector that has not been shown to fire has not been tested,
    and a tool whose pattern matches nothing prints reassurance for its whole
    life. Every assertion below is on a hand-checkable example.
    """
    src = (
        "// module zhao_fake (not this one)\n"
        "module zhao_fake\n"
        "  import p::*;\n"
        "#(\n"
        "  parameter int A = 4,\n"
        "  // a comment mentioning input logic ghost_i\n"
        "  parameter int B = $clog2(A) + 1\n"
        ") (\n"
        "  // ---- section ----\n"
        "  input  logic clk,\n"
        "  output logic [B-1:0] q_o,\n"
        "  input  logic [3:0] arr_i [0:1]\n"
        ");\n"
        "endmodule\n"
    )
    path = os.path.join(os.environ.get("TEMP", "."), "board_gen_selftest.sv")
    io.open(path, "w", encoding="utf-8").write(src)
    ci = read_core(path, "zhao_fake")
    assert ci.param_names == ["A", "B"], ci.param_names
    assert ci.port_names == ["clk", "q_o", "arr_i"], ci.port_names
    assert "import p::*;" in ci.imports, ci.imports
    # The comment quoting a port must NOT have produced one -- the mask is what
    # makes that true, and this is the check that would fail if it stopped.
    assert "ghost_i" not in ci.port_names
    # Verbatim means verbatim: the section comment survives into the emitted
    # item, which is the whole reason the parser masks instead of strips.
    assert any("---- section ----" in x for x in ci.ports), "comments must survive"
    # And the name finder must not be fooled by an expression default.
    assert declared_name("parameter int unsigned X = $clog2(DEPTH)") == "X"
    os.remove(path)


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--core", default=CORE_PATH)
    ap.add_argument("--out", default=OUT_PATH)
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if the file on disk is not what would be written")
    args = ap.parse_args(argv)

    _self_test()
    core = read_core(args.core)
    text = render(core)

    if args.check:
        have = io.open(args.out, encoding="utf-8", errors="replace").read() \
            if os.path.exists(args.out) else ""
        if have == text:
            print("gen_console_board --check: FRESH (%d core ports, %d parameters)"
                  % (len(core.port_names), len(core.param_names)))
            return 0
        print("gen_console_board --check: STALE. %s does not match what "
              "zhao_console_core.sv's port list would produce today "
              "(%d ports, %d parameters). Re-run:\n"
              "    python tools/quartus/gen_console_board.py"
              % (os.path.relpath(args.out, ROOT), len(core.port_names),
                 len(core.param_names)))
        return 1

    with io.open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print("gen_console_board: wrote %s -- %d core ports re-exported, %d driven "
          "by the board, %d parameters forwarded"
          % (os.path.relpath(args.out, ROOT),
             len(core.port_names) - len(SEAM), len(SEAM),
             len(core.param_names)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
