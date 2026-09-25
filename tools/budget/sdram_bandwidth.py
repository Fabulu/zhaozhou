#!/usr/bin/env python3
"""SDRAM bandwidth ledger -- the DENOMINATOR the tree never had.

WHY THIS EXISTS (packet EDGEBAND, 2026-09-23)
=============================================
`TERRAIN.EDGERECON`'s packet P2 is gated on one question: does the frame have
the bandwidth for a PREPARE pass over the deviation store, which owner ruling
R242 moved to SDRAM on 2026-09-22?

Answering it found that the tree has **many numerators and no denominator**:

  * `reports/bandwidth/` -- the "Phase-0 bandwidth matrix" that several
    documents cite -- is an EMPTY DIRECTORY holding a 0-byte `.gitkeep` from
    the 2026-08-14 skeleton commit.  `reports/digests/LANE2-TERRAIN-8KM.md:22`
    and `reports/DOCKET.md:2655` already call citing it a phantom citation.
  * NO tool in `tools/` computes bandwidth.  `tools/budget/` is 20 tools, all
    ALM / DSP / M10K / closure / manifest.
  * `tests/memory/mem_bandwidth_budget.cpp` (2026-08-18) is named like a budget
    and is a STARVATION test: it asserts `scanout_preempted == 0` and prints
    burst counts as "informational".  It never computes a total and never
    compares one against a ceiling.
  * `spec/terrain_rules.md:631-639` says so in its own voice -- "Affordability:
    NOT COSTED ... `spec/memory_rules.md` contains no ratified total-bandwidth
    number" -- and `LANE2-TERRAIN-8KM.md:385-389` records that two independent
    provisional ~41 MB/s terrain figures exist and "nothing anywhere adds them".

So this tool does exactly one thing: it ADDS THEM UP, in the unit the tree
already uses, and says what is left.  It invents no accounting.

WHAT IT IS NOT
==============
It is NOT a board measurement and it cannot become one.  Every SDRAM timing
here is the FROZEN SIM PROFILE from `zhao_sdram_params_pkg.sv`, whose own
header says "provisional sim constants, NOT board truth".  The obligations that
unfreeze them are `spec/memory_rules.md` section 1 (ZH-004).  A number out of
this tool is "what the tree's own declared figures sum to", never "what the
board does".

It is also NOT a gate.  It is deliberately not named `check_*.py`, so
`tools/maintenance/gate_sweep.py` does not discover it and no baseline moves.
It reports; a human decides.  (CLAUDE.md: measurement belongs on the COMPARISON
side -- a tool that says "you are 20% off" is good; a tool that decides a value
is not.)

EVERY NUMBER BELOW IS A NAMED, EDITABLE CONSTANT WITH ITS PROVENANCE.
CLAUDE.md rule 6: "Never remove the owner's control in the name of fidelity ...
'This is generated from the reference, so it is not a knob' is how a wrong
number becomes an unadjustable wrong number."  Each entry carries the file and
line it came from and whether it is MEASURED, DERIVED or PROVISIONAL.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field

# ===========================================================================
# THE MACHINE'S CONSTANTS -- the frozen sim profile, with provenance
# ===========================================================================

# fpga/rtl/sys/zhao_sys_pll.sv:201 and design/contracts/SYS.PLL.md:173.
# DERIVED and corroborated two ways, NOT measured: SYS.PLL.md's own words.
# Corroboration (b) is worth keeping in view because it is independent of the
# PLL: REFRESH_INTERVAL 780 against 8192 rows / 64 ms = 7.8125 us per row gives
# 780 / 7.8125 us = 99.84 MHz.  Two routes agreeing is corroboration, not a
# measurement -- ZH-004 is what would make it one.
SDRAM_CLK_HZ = 100_000_000

# spec/video_rules.md D1 / zhao_pkg.sv ZHAO_TIMING: v_total 262 at 60 Hz.
FRAME_HZ = 60

# fpga/rtl/memory/zhao_sdram_params_pkg.sv:38 (BURST_LENGTH = 8 words) and
# :72 (WORD_BYTES = 2).  16-bit bus; this is the tree's ONLY bus-width
# statement.  8 x 2 = 16 bytes land per SDRAM burst.
SDRAM_BURST_BYTES = 8 * 2

# fpga/rtl/memory/zhao_vram_arbiter.sv:64 -- "client_req[].len is BYTES
# (1..64, zhao_pkg law); ctrl_req.len at the SDRAM edge is WORDS (1..8, 0
# encodes 8) -- the arbiter is the converter", and :167 -- "a 64-byte request
# is split one burst further".  THIS IS THE FACTOR EVERY BYTES/FRAME FIGURE IN
# THE TREE QUIETLY OMITS: a "64-byte burst" in prose is FOUR SDRAM bursts.
FABRIC_REQUEST_BYTES = 64
SDRAM_BURSTS_PER_REQUEST = FABRIC_REQUEST_BYTES // SDRAM_BURST_BYTES  # 4

# fpga/rtl/memory/zhao_sdram_ctrl.sv:30-31 -- "Grant-to-grant spans (words=8):
# read 12/15/18 (hit/miss/conflict), write 10/13/16."  These are the tree's own
# cycle-exact derivation, not an estimate.
SPAN_READ = {"hit": 12, "miss": 15, "conflict": 18}
SPAN_WRITE = {"hit": 10, "miss": 13, "conflict": 16}

# zhao_sdram_params_pkg.sv:42 and :53 -- REFRESH_INTERVAL 780 cycles between
# AUTO_REFRESH, REFRESH_OVERHEAD = T_RP + T_RC = 12 cycles stolen by each.
REFRESH_INTERVAL = 780
REFRESH_OVERHEAD = 12

# The number of live/visible patches in a frame.  spec/terrain_rules.md 4.2,
# quoted by zhao_terrain_devstore.sv:39-41 and by spec/memory_rules.md:351.
# It is the shipped visible set, not a best case.
LIVE_PATCHES = 256


def frame_sdram_cycles() -> int:
    """SDRAM cycles in one 60 Hz frame.

    NOTE, because this is the trap `design/budgets/latency.md:50` warns about
    at length: the tree carries TWO "cycles per frame" numbers that differ
    6.6x.  1,666,666 is the COMPUTE budget (100 MHz / 60, workloads.yml:51);
    251,520 / 217,984 / 318,592 are the per-mode VIDEO DEADLINES
    (`zhao_pkg.sv` ZHAO_TIMING.frame_gpu_cycles = v_total * h_total * 2).

    For a BANDWIDTH question the denominator is neither of those by analogy --
    it is SDRAM_CLK_HZ / FRAME_HZ, which happens to coincide with the compute
    budget only because both clocks are quoted at 100 MHz.  That coincidence is
    NOT a reason to conflate them: SDRAM_CLK_HZ is derived from the refresh
    interval, the compute budget from the gpu clock, and ZH-004 could move
    either independently.
    """
    return SDRAM_CLK_HZ // FRAME_HZ


# ===========================================================================
# THE DEMAND LEDGER
# ===========================================================================

# Arbiter priority classes -- fpga/rtl/memory/zhao_vram_arbiter.sv:1-41.
# This is load-bearing and is the whole reason a flat percentage misleads:
# TERRAIN_BUILD is served ONLY when nothing else is pending, DEBUG included.
CLASS_SCANOUT = "scanout (strict priority)"
CLASS_RR = "round-robin (guaranteed)"
CLASS_BACKGROUND = "background (ruling T3: served only when nothing else pends)"


@dataclass
class Demand:
    """One subsystem's declared per-frame SDRAM traffic.

    `read_requests` / `write_requests` are counts of 64-BYTE FABRIC REQUESTS,
    because that is the unit every declaration in this tree is written in.
    The tool converts to SDRAM bursts and grant-clocks; the declarations stay
    in the unit their authors used, so each stays checkable against its source.
    """

    name: str
    priority: str
    read_requests: int
    write_requests: int
    basis: str
    provenance: str
    # CONFIDENCE is part of the number.  "provisional" figures are the ones
    # spec/terrain_rules.md:631-639 refuses to freeze before ZH-004.
    confidence: str = "provisional"

    def requests(self) -> int:
        return self.read_requests + self.write_requests

    def bytes_per_frame(self) -> int:
        return self.requests() * FABRIC_REQUEST_BYTES

    def mb_per_s(self) -> float:
        return self.bytes_per_frame() * FRAME_HZ / 1_000_000.0

    def grant_clocks(self, page: str) -> int:
        r = self.read_requests * SDRAM_BURSTS_PER_REQUEST * SPAN_READ[page]
        w = self.write_requests * SDRAM_BURSTS_PER_REQUEST * SPAN_WRITE[page]
        return r + w


def devstore_emit() -> Demand:
    """The deviation store's EXISTING per-frame read, post-R242.

    MEASURED, not derived, and measured twice independently:

      (1) by hand off the read FSM in zhao_terrain_devstore.sv: R_IDLE ->
          R_HREQ issues ONE history burst; R_HWAIT -> R_DREQ issues the first
          record burst; R_STREAM re-enters R_DREQ whenever r_ptr_q[1:0] == 3,
          i.e. once per four of the sixteen records.  1 + 4 = 5 reads.
          R_HWR issues ONE history WRITE burst, taken only when the LOD pass
          emitted a decision (`h_any_q || h_valid_i`).

      (2) by the committed directed test `terrain_lodpath_directed` case 8,
          which asserts `rd1 + 5 == bursts_read_o` ("a patch read is ONE
          history burst plus FOUR record bursts") and `wr0 + 4` for the write
          side.  EDGEBAND ran it: 472 checks, 0 failures, and case 8 printed
          "55 bursts read, 22 written, 77 guard requests; a patch read cost
          660 clocks of memory wait in total" -- 55/5 = 11 patch reads, and
          660/55 = 12 clocks per 64-byte burst ON THAT BENCH'S PLAYED FABRIC.

    THAT 12 IS NOT THE REAL COST AND MUST NOT BE QUOTED AS ONE.  design/
    blocks.yml:2613 says the bench fabric runs "at 2-cycle read latency".  A
    real 64-byte request is FOUR SDRAM bursts at 12-18 grant-clocks each, so
    the honest figure is 48-72, and the bench floor understates by ~4-6x.  The
    bench proves the COUNT; the span table converts it to TIME.
    """
    return Demand(
        name="TERRAIN.DEVSTORE EMIT (existing)",
        priority=CLASS_BACKGROUND,
        read_requests=LIVE_PATCHES * 5,
        write_requests=LIVE_PATCHES * 1,
        basis="256 live patches x (1 history + 4 record reads, + 1 history writeback)",
        provenance="zhao_terrain_devstore.sv R_HREQ/R_DREQ/R_HWR; "
                   "terrain_lodpath_directed case 8; spec/memory_rules.md:351-355",
        confidence="measured",
    )


def devstore_prepare() -> Demand:
    """The PREPARE pass EDGERECON's packet P2 would add.  THE QUESTION.

    PREPARE must read every admitted patch's deviations and history to compute
    lvl[] before EMIT opens, so its READ demand is EMIT's exactly -- the same
    five bursts for the same 256 patches.

    ITS WRITE DEMAND IS ZERO.  THAT WAS A REQUIREMENT ON P2 AND IS NOW A BUILT
    AND MEASURED PROPERTY -- EDGEPREP, 2026-09-25.

    The requirement was real: a PREPARE teed naively off `r_start_i` inherits
    R_HWR, so it would write each patch's history row back a second time per
    frame, and EMIT -- which reads history at R_HREQ BEFORE its own LOD runs --
    would then read PREPARE's write as "the previous frame's" level.  A
    hysteresis corruption with every counter balancing, not a bandwidth cost.

    IT IS SUPPRESSED, AND devstore NEEDED NO CHANGE.  The read FSM enters
    R_HWR on `(h_any_q || h_valid_i)` and `h_any_q` is set ONLY inside
    `if (h_valid_i)` (zhao_terrain_devstore.sv:859-865, :877), while the
    producer of `h_valid` is `zhao_terrain_jobissue` (:343).
    `zhao_terrain_lodshare` routes PREPARE's ladder output to the RECONCILER
    and not to jobissue, so jobissue is starved for the whole pass and emits no
    history at all -- devstore's own comment already describes the path, "A
    patch whose LOD pass emitted NOTHING skips the write entirely."

    That block ALSO gates the history port explicitly, belt and braces, and
    `hist_leak_o` counts any history beat offered while PREPARE owns the
    ladder.  `terrain_lodshare_directed` case 5 forwards a beat in EMIT and
    watches the identical beat be swallowed in PREPARE, with the counter firing
    on the offer -- so the zero here is a reading rather than a hope.

    SO THE 256 WRITES THIS DOCSTRING USED TO THREATEN ARE NOT OWED.  If a
    future edit routes PREPARE's output to jobissue, `hist_leak_o` moves and
    the crack, not the bandwidth, is the defect to fix.
    """
    return Demand(
        name="TERRAIN.DEVSTORE PREPARE (proposed, EDGERECON P2)",
        priority=CLASS_BACKGROUND,
        read_requests=LIVE_PATCHES * 5,
        write_requests=0,
        basis="identical read to EMIT; writeback SUPPRESSED and measured "
              "(zhao_terrain_lodshare, hist_leak_o, EDGEPREP 2026-09-25)",
        provenance="design/contracts/TERRAIN.EDGERECON.md:583-593; "
                   "EDGEBAND derivation 2026-09-23; "
                   "EDGEPREP build + terrain_lodshare_directed case 5",
        confidence="derived",
    )


def prepare_list_reread() -> Demand:
    """P2's THIRD pass over the sealed patch list.  A DIFFERENT SOCKET.

    ADDED 2026-09-25 (EDGEPREP), and the reason it is reported rather than
    summed is the whole point of the row.

    `zhao_terrain_prepwalk` re-reads the frame's sealed T5 patch list to
    enumerate the admitted set without touching the compose spine.  That is a
    third pass over bytes `zhao_terrain_cmd` already reads twice -- once to
    fold the CRC, once to emit -- and it is what makes the walk REPLAYABLE
    rather than a tee off a one-shot stream that cannot terminate.

    THE COST IS 256 x 32 B = 8 KiB/frame, and it lands on the HPS-DDR BRIDGE,
    NOT on the local SDRAM this ledger adds up.  `zhao_terrain_cmd` and
    `zhao_terrain_prepwalk` both take `cfg_hps_client_i` and drive
    `zhao_hps_burst_req_t` through `u_terr_hps_arb` into `zhao_hps_bridge`;
    the local-SDRAM clients reach `zhao_vram_arbiter` through
    `zhao_mem_guard`.  Two sockets, two budgets.

    SO THIS ROW IS NOT IN `declared_demands()` AND MUST NOT BE ADDED TO THE
    TOTAL.  Folding it in would overstate the local SDRAM by 8 KiB/frame and,
    worse, would establish that the two sockets are interchangeable in this
    tool -- which is the kind of quiet category error that survives for
    months.  It exists here so the number is WRITTEN DOWN somewhere a later
    packet will look, because `zhao_terrain_cmd`'s own header already costs
    its two passes ("16 KB of HPS traffic per frame against the 684 KB that
    T7's 32 whole pages already cost -- 2.4% more") and a third pass belongs
    beside that sentence: 24 KB against 684 KB, ~3.5%.

    A REAL HPS-BRIDGE LEDGER IS OWED AND DOES NOT EXIST.  Nothing under
    tools/ adds up that socket, exactly as nothing added up this one before
    EDGEBAND wrote this file.  Naming the gap is cheaper than inventing a
    denominator for it.
    """
    return Demand(
        name="TERRAIN.CMD sealed-list re-read, PREPARE pass (HPS BRIDGE, not SDRAM)",
        priority=CLASS_BACKGROUND,
        read_requests=(LIVE_PATCHES * 32) // FABRIC_REQUEST_BYTES,
        write_requests=0,
        basis="256 records x 32 B = 8 KiB/frame on the HPS-DDR bridge; "
              "a THIRD pass over a list zhao_terrain_cmd already reads twice",
        provenance="zhao_terrain_cmd.sv header ('read it twice', 16 KB/frame); "
                   "zhao_terrain_prepwalk.sv (EDGEPREP 2026-09-25); ruling T5",
        confidence="derived",
    )


def composed_cache_publish() -> Demand:
    """TERRAIN.COMPOSED_HEIGHT + COMPOSED_VELOCITY publication. THE QUESTION
    the owner directive's section 1 asks, costed here for the first time.

    ADDED 2026-09-25 (COMPOSEPUB), because the directive says in as many words
    that publishing lattices adds write traffic and that it must be charged.

    THE ARITHMETIC.  `spec/memory_rules.md` 5b ratifies both regions as
    256 x 2,304 B.  2,304 / 64 = 36 fabric requests per slot per channel;
    two channels over 256 live patches is 256 x 36 x 2 = 18,432 WRITE
    requests per frame, which at four 16-byte bursts each and the hit-page
    write span is a little over 737,000 grant-clocks -- roughly 44% of the
    frame ON TOP of the 80% the ledger already carries.

    SO THE PUBLICATION AS SPECIFIED DOES NOT FIT, and that is a finding rather
    than a reason to shrink it.  It is worst-on-worst by the tool's own
    convention: every live patch republished every frame.  The real driver is
    the DIRTY set -- TERRAIN.PATCH already reduces per-vertex dirt to
    `subpatch_dirty_o`, so a publisher would write only patches a field or a
    bake actually moved.  At a dirty fraction d the cost is d x this row, and
    the break-even against the ledger's own 19.83% headroom is d ~ 0.45.
    A packet that builds the publisher owes that fraction MEASURED on a real
    scene, not assumed.

    THIS ROW IS NOT IN `declared_demands()` and does not move the baseline
    total, because nothing publishes today: across all 377 SystemVerilog files
    of `fpga/rtl` the two region names appear only in comments, there is no
    base constant, and `zhao_mem_guard` has no window for either.  It is
    reported under --proposed so the number exists before the build does.
    """
    return Demand(
        name="TERRAIN.COMPOSED_HEIGHT+VELOCITY publish (proposed, COMPOSEPUB)",
        priority=CLASS_BACKGROUND,
        read_requests=0,
        write_requests=LIVE_PATCHES * (2_304 // FABRIC_REQUEST_BYTES) * 2,
        basis="256 live patches x 2,304 B x 2 channels, every patch every "
              "frame (worst-on-worst); scale by the dirty fraction",
        provenance="spec/memory_rules.md:301-302 (5b, ratified 2026-09-02); "
                   "OWNER_VACATION_DIRECTIVE_2026-09-23 section 1; "
                   "COMPOSEPUB measurement 2026-09-25",
        confidence="derived",
    )


def composed_cache_restage() -> Demand:
    """The FILL-side backing read that would make the publication a CONSUMER.

    `zhao_terrain_compcache_front`'s own header is the design intent:
    "The full 256-patch composed store is 256 x 2,178 B for heights plus as
    much again for velocity = 8.92 Mbit = 161% of this device's entire
    5.53 Mbit of M10K ... The SDRAM backing attaches later on the FILL side
    without changing the serve ports."

    "Attaches later" is the unbuilt half, and it is the ONLY thing that would
    make a composed-cache write a read by anybody -- the directive's own "A
    DMA into unused memory is not a consumer."  Costed here at one re-stage
    per live patch per frame, the symmetric worst case to the publish row.
    A real re-stage policy is driven by tap misses, not by patch count.
    """
    return Demand(
        name="TERRAIN.COMPCACHE fill-side backing read (unbuilt, COMPOSEPUB)",
        priority=CLASS_BACKGROUND,
        read_requests=LIVE_PATCHES * (2_304 // FABRIC_REQUEST_BYTES) * 2,
        write_requests=0,
        basis="the symmetric re-stage of what the publish row writes",
        provenance="zhao_terrain_compcache_front.sv:12-20 ('the SDRAM backing "
                   "attaches later on the FILL side'); COMPOSEPUB 2026-09-25",
        confidence="derived",
    )


def declared_demands() -> list[Demand]:
    """Everything else the tree declares, each with its source and its date.

    These are NOT EDGEBAND's numbers.  They are the figures already written
    down elsewhere, gathered here so that -- for the first time -- they can be
    added.  Where a figure is bytes/frame rather than requests, it is converted
    at 64 B/request, which is how it reaches the arbiter.
    """
    return [
        Demand(
            name="VIDEO.SCANOUT (Duo, worst mode)",
            priority=CLASS_SCANOUT,
            read_requests=245_760 // FABRIC_REQUEST_BYTES,
            write_requests=0,
            basis="512 x 240 x 2 B displayed per frame",
            provenance="zhao_pkg.sv ZHAO_DISPLAYED_BYTES_DUO; spec/memory_rules.md:176-181",
            confidence="derived",
        ),
        Demand(
            name="CMD.DMA blit (full-canvas DebugFrameBlit)",
            priority=CLASS_RR,
            read_requests=0,
            write_requests=245_760 // FABRIC_REQUEST_BYTES,
            basis="one full canvas written per frame, the R4 worst case",
            provenance="spec/memory_rules.md:176-181 (ratified 2026-08-14)",
            confidence="derived",
        ),
        Demand(
            name="TERRAIN streaming (page upload, ruling T7)",
            priority=CLASS_BACKGROUND,
            read_requests=0,
            write_requests=(32 * 21_376) // FABRIC_REQUEST_BYTES,
            basis="32 pages/frame x 21,376 B = 684 KB/frame ~ 41 MB/s",
            provenance="spec/terrain_rules.md:496-500 -- 'provisional against the "
                       "Phase-0 bandwidth matrix (board truth pending; re-check at "
                       "ZH-004, do not freeze)'",
            confidence="provisional",
        ),
        Demand(
            name="TERRAIN bake VRAM traffic",
            priority=CLASS_BACKGROUND,
            read_requests=(64 * 10_760 // 2) // FABRIC_REQUEST_BYTES,
            write_requests=(64 * 10_760 // 2) // FABRIC_REQUEST_BYTES,
            basis="10,760 B/patch-bake x 64 patches = 673 KiB/frame ~ 41.3 MB/s, "
                  "split evenly read/write for want of a stated split",
            provenance="spec/terrain_rules.md:626-630, self-flagged "
                       "'Affordability: NOT COSTED'",
            confidence="provisional",
        ),
    ]


# ===========================================================================
# THE LEDGER
# ===========================================================================

@dataclass
class Ledger:
    demands: list[Demand] = field(default_factory=list)
    page: str = "hit"

    def refresh_clocks(self) -> int:
        return (frame_sdram_cycles() // REFRESH_INTERVAL) * REFRESH_OVERHEAD

    def total_grant_clocks(self) -> int:
        return sum(d.grant_clocks(self.page) for d in self.demands)

    def committed(self) -> int:
        return self.total_grant_clocks() + self.refresh_clocks()

    def headroom(self) -> int:
        """Positive = cycles to spare.  NEGATIVE = the frame is oversubscribed."""
        return frame_sdram_cycles() - self.committed()

    def utilisation(self) -> float:
        return 100.0 * self.committed() / frame_sdram_cycles()

    def fits(self) -> bool:
        return self.headroom() >= 0


def render(ledger: Ledger, title: str) -> str:
    out: list[str] = []
    frame = frame_sdram_cycles()
    out.append("=" * 78)
    out.append(title)
    out.append("=" * 78)
    out.append(
        "frame = %s SDRAM cycles (%s Hz / %d fps); page model = %s "
        "(read span %d, write span %d)"
        % (f"{frame:,}", f"{SDRAM_CLK_HZ:,}", FRAME_HZ, ledger.page,
           SPAN_READ[ledger.page], SPAN_WRITE[ledger.page])
    )
    out.append(
        "a 64-byte fabric request = %d SDRAM bursts of %d B "
        "(zhao_vram_arbiter.sv:167)"
        % (SDRAM_BURSTS_PER_REQUEST, SDRAM_BURST_BYTES)
    )
    out.append("")
    hdr = "%-46s %9s %11s %9s  %s" % (
        "demand", "KiB/frame", "grant-clk", "% frame", "confidence")
    out.append(hdr)
    out.append("-" * len(hdr))
    for d in sorted(ledger.demands, key=lambda x: -x.grant_clocks(ledger.page)):
        gc = d.grant_clocks(ledger.page)
        out.append("%-46s %9.1f %11s %8.2f%%  %s" % (
            d.name[:46], d.bytes_per_frame() / 1024.0, f"{gc:,}",
            100.0 * gc / frame, d.confidence))
    rc = ledger.refresh_clocks()
    out.append("%-46s %9s %11s %8.2f%%  %s" % (
        "AUTO_REFRESH (unavoidable)", "-", f"{rc:,}",
        100.0 * rc / frame, "derived"))
    out.append("-" * len(hdr))
    out.append("%-46s %9.1f %11s %8.2f%%" % (
        "TOTAL COMMITTED",
        sum(d.bytes_per_frame() for d in ledger.demands) / 1024.0,
        f"{ledger.committed():,}", ledger.utilisation()))
    out.append("")
    if ledger.fits():
        out.append("HEADROOM: %s SDRAM cycles free (%.2f%% of the frame)."
                   % (f"{ledger.headroom():,}",
                      100.0 * ledger.headroom() / frame))
    else:
        short = -ledger.headroom()
        out.append("SHORTFALL: the frame is oversubscribed by %s SDRAM cycles "
                   "(%.2f%% over)." % (f"{short:,}", 100.0 * short / frame))

    # -------------------------------------------------------------------
    # THE VIEW THAT ACTUALLY DECIDES IT, and the reason a flat percentage
    # of the frame is the wrong instrument for this client.
    #
    # TERRAIN_BUILD is arbiter client 6 and ruling T3 makes it BACKGROUND:
    # "served only when NOTHING else is pending, DEBUG included"
    # (zhao_vram_arbiter.sv:33-41).  So its budget is not a share of the
    # frame -- it is whatever the guaranteed classes and refresh leave
    # behind.  A row reading "4.30% of frame" invites the reading that
    # devstore has 95% spare; it does not, and this block says by how much.
    guaranteed = sum(d.grant_clocks(ledger.page) for d in ledger.demands
                     if d.priority in (CLASS_SCANOUT, CLASS_RR))
    background = sum(d.grant_clocks(ledger.page) for d in ledger.demands
                     if d.priority == CLASS_BACKGROUND)
    residue = frame - guaranteed - ledger.refresh_clocks()
    if background:
        out.append("")
        out.append("BACKGROUND-CLASS VIEW (ruling T3 -- this is the binding one)")
        out.append("  guaranteed classes + refresh take   %11s" % f"{guaranteed + ledger.refresh_clocks():,}")
        out.append("  IDLE RESIDUE left for TERRAIN_BUILD %11s" % f"{residue:,}")
        out.append("  TERRAIN_BUILD actually wants        %11s  (%.1f%% of the residue)"
                   % (f"{background:,}", 100.0 * background / residue if residue else float("inf")))
        if background > residue:
            out.append("  => TERRAIN_BUILD IS STARVED by %s cycles. Ruling T3 makes that"
                       % f"{background - residue:,}")
            out.append("     the RULED behaviour, not a defect -- the frame-critical")
            out.append("     record read simply arrives late and LOD decisions DELAY.")
        else:
            out.append("  => fits within the residue, with %s cycles to spare."
                       % f"{residue - background:,}")
    return "\n".join(out)


# ===========================================================================
# THE SELF-TEST -- it must be seen to report a SHORTFALL, not only a surplus
# ===========================================================================
# CLAUDE.md: "A detector that has not been shown to FIRE has not been tested",
# and "the defect always made the answer look BETTER, SMALLER or SIMPLER than
# the truth".  A bandwidth tool that can only ever say "there is room" is
# precisely that instrument.  So the self-test drives BOTH directions and
# checks the arithmetic against a case computed by hand.

def self_test() -> int:
    failures = 0

    def chk(ok: bool, what: str, expected=None, actual=None) -> None:
        nonlocal failures
        if ok:
            print("ok: %s" % what)
        else:
            failures += 1
            print("FAIL: %s (expected %r, actual %r)" % (what, expected, actual))

    # --- 1. the hand-checkable case -------------------------------------
    # ONE patch read: 5 fabric requests x 4 SDRAM bursts x 12 clocks = 240.
    one = Demand(name="one patch read", priority=CLASS_BACKGROUND,
                 read_requests=5, write_requests=0, basis="hand", provenance="hand")
    chk(one.grant_clocks("hit") == 240,
        "one patch read costs 5 x 4 x 12 = 240 grant-clocks at page-hit",
        240, one.grant_clocks("hit"))
    chk(one.bytes_per_frame() == 320,
        "one patch read moves 5 x 64 = 320 B, which is the slot footprint",
        320, one.bytes_per_frame())
    # The slot footprint is stated independently at zhao_terrain_devstore.sv:129-137
    # as 256 B of deviations + 64 B of history = 320 B.  Two routes, same number.

    # --- 2. the frame denominator ---------------------------------------
    chk(frame_sdram_cycles() == 1_666_666,
        "frame is 100 MHz / 60 = 1,666,666 SDRAM cycles",
        1_666_666, frame_sdram_cycles())

    # --- 3. POSITIVE CONTROL: a small ledger must report a SURPLUS ------
    small = Ledger(demands=[one], page="hit")
    chk(small.fits(), "a one-patch ledger fits", True, small.fits())
    chk(small.headroom() > 0, "and reports positive headroom",
        "> 0", small.headroom())

    # --- 4. NEGATIVE CONTROL: THE ONE THAT MATTERS ----------------------
    # Construct a demand that MUST overflow, and confirm the tool says so.
    # Without this, "it fits" is an untested claim in the flattering direction.
    huge = Demand(name="deliberately oversubscribed", priority=CLASS_RR,
                  read_requests=100_000, write_requests=0,
                  basis="self-test negative control", provenance="self-test")
    # 100,000 x 4 x 12 = 4,800,000 grant-clocks against a 1,666,666 frame.
    over = Ledger(demands=[huge], page="hit")
    chk(not over.fits(), "the tool REPORTS A SHORTFALL when the frame overflows",
        False, over.fits())
    chk(over.headroom() < 0, "and the headroom goes negative",
        "< 0", over.headroom())
    expected_short = 4_800_000 + over.refresh_clocks() - 1_666_666
    chk(-over.headroom() == expected_short,
        "and the shortfall is the hand-computed size",
        expected_short, -over.headroom())
    rendered = render(over, "self-test negative control")
    chk("SHORTFALL" in rendered,
        "and the rendered report says SHORTFALL out loud",
        "SHORTFALL in text", rendered.splitlines()[-1])

    # --- 5. the page model must MATTER ----------------------------------
    # If hit/miss/conflict all produced the same answer, the span table would
    # not be wired in at all -- a real risk, and exactly the shape of a
    # detector whose two operands move together.
    chk(one.grant_clocks("conflict") > one.grant_clocks("hit"),
        "a bank conflict costs strictly more than a page hit",
        "conflict > hit",
        (one.grant_clocks("conflict"), one.grant_clocks("hit")))

    # --- 6. the 4x split must MATTER ------------------------------------
    chk(SDRAM_BURSTS_PER_REQUEST == 4,
        "a 64-byte fabric request is four 16-byte SDRAM bursts",
        4, SDRAM_BURSTS_PER_REQUEST)

    print("")
    print("sdram_bandwidth self-test: %d failure(s)" % failures)
    return 1 if failures else 0


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--self-test", action="store_true",
                    help="prove the tool can report a shortfall, not only a surplus")
    ap.add_argument("--page", choices=("hit", "miss", "conflict"), default="hit",
                    help="SDRAM page model for the span table (default: hit, "
                         "the most FLATTERING; pass 'conflict' for the "
                         "unflattering direction)")
    ap.add_argument("--with-prepare", action="store_true",
                    help="include EDGERECON P2's proposed PREPARE pass")
    ap.add_argument("--with-composed-publish", action="store_true",
                    help="include COMPOSEPUB's proposed TERRAIN.COMPOSED_HEIGHT"
                         "/VELOCITY publication (write side)")
    ap.add_argument("--with-composed-restage", action="store_true",
                    help="also include the unbuilt compcache fill-side backing "
                         "read that would make that publication a consumer")
    ap.add_argument("--terrain-only", action="store_true",
                    help="show only the TERRAIN_BUILD background client, whose "
                         "budget is the idle residue rather than a share")
    args = ap.parse_args(argv)

    if args.self_test:
        return self_test()

    demands = [devstore_emit()] + declared_demands()
    if args.with_prepare:
        demands.append(devstore_prepare())
    if args.with_composed_publish or args.with_composed_restage:
        demands.append(composed_cache_publish())
    if args.with_composed_restage:
        demands.append(composed_cache_restage())
    if args.terrain_only:
        demands = [d for d in demands if d.priority == CLASS_BACKGROUND]

    ledger = Ledger(demands=demands, page=args.page)
    title = "SDRAM BANDWIDTH LEDGER%s" % (
        " -- WITH EDGERECON P2 PREPARE" if args.with_prepare else "")
    print(render(ledger, title))
    print("")
    print("READ THIS BEFORE QUOTING THE NUMBER ABOVE")
    print("-" * 78)
    print("* Every SDRAM timing here is the FROZEN SIM PROFILE, not board truth")
    print("  (zhao_sdram_params_pkg.sv:18-22).  ZH-004 is what would settle it.")
    print("* The rows marked 'provisional' are figures their own authors refused")
    print("  to freeze -- spec/terrain_rules.md:631-639, 'Affordability: NOT")
    print("  COSTED'.  Summing provisional numerators gives a provisional total.")
    if args.with_prepare:
        other = prepare_list_reread()
        print("* AND ONE COST OF PREPARE IS NOT IN THE TABLE ABOVE, ON PURPOSE.")
        print("  %s" % other.name)
        print("  is %d read request(s)/frame = %d KiB/frame, and it lands on the"
              % (other.read_requests, (other.read_requests * FABRIC_REQUEST_BYTES) // 1024))
        print("  HPS-DDR BRIDGE, not the local SDRAM this ledger adds up. Two")
        print("  sockets, two budgets: folding it in would overstate this one and")
        print("  would teach the next reader that they are interchangeable.")
        print("  No tool adds up the HPS socket yet. That gap is named, not filled.")
    print("* TERRAIN.DEVSTORE rides arbiter client 6, TERRAIN_BUILD, which ruling")
    print("  T3 makes BACKGROUND: served only when nothing else is pending, DEBUG")
    print("  included (zhao_vram_arbiter.sv:33-41).  Its budget is therefore the")
    print("  IDLE RESIDUE, not a share of the frame, and a percentage of the")
    print("  frame OVERSTATES what it can actually get.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
