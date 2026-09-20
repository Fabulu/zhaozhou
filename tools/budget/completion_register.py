"""completion_register -- is every mandatory v1 capability REALLY present?

Owner goal, 2026-09-19: build the entire mandatory v1 console, connect it
honestly, and drive mandatory unresolved items to ZERO. A mandatory function
does NOT count as present if it is a tie-off, fake stimulus, an external
placeholder for storage that belongs in hardware, a disconnected
implementation, synthesis-pruned dead logic, a stub, a TODO, or an
unimplemented contract.

Every mandatory function needs:

    real producer -> real implementation -> real consumer -> tests/evidence

THE DESIGN DECISION THAT MATTERS: this register is COMPUTED, not maintained.

A hand-kept completion table is the thing this repository fails at. It has a
phantom-reference register that went stale in six days, thirteen mutant copies
that drifted while staying green, twenty owner documents with no recorded
disposition, and an ENFORCED-BY tag naming a test no target compiles. A status
field a human edits is a status field that lies the moment someone forgets.

So every judgement below is derived from the tree at the moment of the run:

  * the TIE-OFF list is parsed out of `zhao_console_core.sv`'s own
    "INCOMPLETE -- TIED OFF, AND WHY" header block. That block is maintained by
    whoever edits the core, next to the ports it describes, and it is the one
    place a new gap cannot be added without being written down.
  * COMPOSED means the module appears in `zhao_console_core`'s closure in
    `design/fit_targets.yml` AND is instantiated somewhere in that closure.
    A file in the source list that nothing instantiates is not composed -- the
    shell's own history records eight such modules that elaborated nowhere.
  * EVIDENCE means a `tests/CMakeLists.txt` target exists that verilates the
    module. Seven committed tests were found this session that no target
    compiled; a file on disk is not a gate.

Nothing here reads a `status:` field, because there isn't one.

Exit 0 when zero MANDATORY gaps remain. Exit 1 while any remain. Exit 2 if the
self-test fails, because a register that cannot see a gap it should see would
report victory, and that is the one direction this must never fail in.
"""

from __future__ import annotations

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CORE = ROOT / "fpga" / "rtl" / "prod" / "zhao_console_core.sv"
TARGETS = ROOT / "design" / "fit_targets.yml"
CMAKE = ROOT / "tests" / "CMakeLists.txt"

# The second root. See `board_addition()` for what it is allowed to do and for
# the measurement that bounds it.
CONSOLE_ROOT = "zhao_console_core"
BOARD_ROOT = "zhao_console_board"

# A tie-off entry in the core's header: "//  I4. NAME ... -- KIND."
_TIEOFF = re.compile(r"^//\s*(I\d+)\.\s+(.*)$")
_BOUNDARY = re.compile(r"\bBOUNDARY\b")
_TIED_ZERO = re.compile(r"TIED TO ZERO|STRUCTURALLY STUCK", re.I)
_NOT_A_TIEOFF = re.compile(r"NOT a tie-off", re.I)


def tieoffs() -> list[dict]:
    """Parse the core's own INCOMPLETE block. It is the authoritative gap list."""
    if not CORE.exists():
        raise SystemExit("zhao_console_core.sv is missing; there is no console to audit")
    text = CORE.read_text(encoding="utf-8", errors="replace")
    start = text.find("INCOMPLETE -- TIED OFF")
    if start < 0:
        raise SystemExit(
            "zhao_console_core.sv has no 'INCOMPLETE -- TIED OFF' block. Either "
            "every gap is closed and the block was removed -- in which case "
            "delete this check deliberately -- or the header was reformatted "
            "and this register has gone blind. It does not guess."
        )
    out: list[dict] = []
    cur: dict | None = None
    for line in text[start:].splitlines():
        m = _TIEOFF.match(line.strip())
        if m:
            if cur:
                out.append(cur)
            cur = {"id": m.group(1), "head": m.group(2).strip(), "body": ""}
            continue
        if cur is None:
            continue
        if not line.strip().startswith("//"):
            break
        cur["body"] += " " + line.strip().lstrip("/").strip()
    if cur:
        out.append(cur)

    # THE WALK STOPS AT THE FIRST NON-COMMENT LINE, AND THAT IS A SILENT
    # TRUNCATION unless somebody checks. Added 2026-09-20 after it happened.
    #
    # The loop above breaks out of the block the moment a line does not start
    # with `//`. That is the right way to find the block's END -- but a single
    # BLANK LINE accidentally left between two entries ends the walk there too,
    # and everything after it is never parsed. The register then reports a
    # SMALLER number with no complaint whatever.
    #
    # It cost nothing to find only because the drop was implausible: closing one
    # entry moved the count by five. Had the blank line been left between the
    # last two entries, the count would have moved by one -- exactly what was
    # expected -- and the register would have been wrong and believed. This is
    # the broken-instrument law in its purest form: the defect reads LOW, which
    # is the direction nobody audits, and the tool's own docstring already warns
    # that a reformatted header makes it "go blind". It just had no way to say
    # so.
    #
    # The guard is cheap and exact: after the walk, look for any entry head in
    # the REST of the file. An entry the walk never reached is one the register
    # cannot count, so it is a hard failure and it names the id.
    # COLUMN ZERO ONLY, and the restriction is not cosmetic. The walk itself
    # matches on `line.strip()`, so an INDENTED comment deep in the port list
    # ("  // I22.  `zhao_terrain_compcache_front` is composed below") looks
    # exactly like an entry head to it. Every real entry in the block begins at
    # column 0; prose that CITES an id is indented, because it is inside
    # something. Without this, the guard's first run reported I22 -- a genuine
    # sentence about a genuinely CLOSED entry -- and a check that cries wolf on
    # a closed gap is one people learn to skip.
    seen = {t["id"] for t in out}
    missed = [m.group(1) for m in
              (_TIEOFF.match(ln) for ln in text[start:].splitlines()
               if ln.startswith("//"))
              if m and m.group(1) not in seen]
    if missed:
        raise SystemExit(
            "completion_register: THE ENTRY WALK STOPPED EARLY.\n"
            "  parsed %d entries, but %s appear later in the file and were "
            "never reached.\n"
            "  The walk ends at the first line that does not begin with `//`, so "
            "a single\n"
            "  blank line between two entries silently truncates the register -- "
            "and it\n"
            "  truncates it DOWNWARD, which is the direction nobody questions. "
            "Find the\n"
            "  blank line inside zhao_console_core.sv's INCOMPLETE block and make "
            "it `//`."
            % (len(out), ", ".join(missed)))

    # AND A BLOCK THAT YIELDS NO ENTRIES AT ALL IS A FAILURE. Added 2026-09-20
    # (review Q011, finding 6). The guard above compares what was parsed
    # against what appears LATER, so it is blind to the case where NOTHING
    # matches `_TIEOFF` anywhere: `out` is empty, `missed` is empty, and the
    # function returns a clean `[]`. Renumber the ids, change the leading
    # punctuation, wrap the heads differently -- any of it -- and the register
    # reports every tie-off closed.
    #
    # The docstring above already states the disposition for a genuinely empty
    # block: "Either every gap is closed and the block was removed -- in which
    # case delete this check deliberately." A block that still EXISTS and
    # parses to nothing has not been deleted deliberately; it has stopped being
    # readable, and that is the reformatted-header blindness the same paragraph
    # warns about.
    if not out:
        raise SystemExit(
            "completion_register: the 'INCOMPLETE -- TIED OFF' block is present "
            "in zhao_console_core.sv and NOT ONE entry matched. That is not an "
            "empty gap list -- it is a parser that has gone blind on a block it "
            "can see. If every gap really is closed, delete the block and this "
            "check together, deliberately."
        )

    # DUPLICATE ENTRY IDS ARE A HARD FAILURE.
    #
    # On 2026-09-19 two packets, within one hour, both numbered a new entry I42
    # -- one for a deviation store, one for a program store. Entries live in a
    # LIST here, so both were kept, both were counted, and the register reported
    # nothing wrong. Worse, other entries CITE ids in prose ("the same absent
    # owner as I42"), and a reader following that citation lands on whichever of
    # the two they find first.
    #
    # It is not obvious which direction this fails in, which is exactly why it
    # needs a guard rather than a habit: two entries counted is an over-report,
    # but a citation resolving to the wrong entry sends work at the wrong seam,
    # and an editor who "fixes the duplicate" by deleting one silently drops a
    # real gap. The concurrency that caused it is now the normal way this repo
    # works -- four to six packets editing one header -- so it will happen again.
    seen_ids: dict[str, str] = {}
    for t in out:
        if t["id"] in seen_ids:
            raise SystemExit(
                "completion_register: DUPLICATE ENTRY ID %s in "
                "zhao_console_core.sv's INCOMPLETE block.\n"
                "  first : %s\n"
                "  second: %s\n"
                "Two packets numbering the same entry is the normal hazard now "
                "that several edit this header at once. Renumber the LATER one "
                "to the next free id and move any prose that cites it -- do not "
                "delete either, they are different gaps."
                % (t["id"], seen_ids[t["id"]][:90], t["head"][:90]))
        seen_ids[t["id"]] = t["head"]

    for t in out:
        # THE MARKER COUNTS ONLY IN THE HEAD LINE, and that is a correctness fix
        # rather than tidiness.
        #
        # This used to search `head + body`, so ANY occurrence of "NOT a tie-off"
        # anywhere in an entry's prose removed that entry from the mandatory
        # count. On 2026-09-19 the TERRAIN.COMPOSE worker watched its new entry
        # I35 read as CLOSED twice -- once when it used the phrase in an
        # explanation, and again when it QUOTED THE PHRASE while describing the
        # first accident. An entry could be settled by writing a sentence about
        # it.
        #
        # That is the flattering direction, in the instrument whose whole purpose
        # is to prevent free reductions, and it is the same root cause as the
        # three closure defects before it: THE REGISTER WAS READING A CONVENTION
        # RATHER THAN A STRUCTURE. Prose is not a declaration. The head line is,
        # because it is the one line that cannot be written by accident while
        # discussing something else -- and both entries legitimately using this
        # today (I9, I25) already declare it there.
        #
        # A body occurrence is now a HARD FAILURE rather than a silent exclusion,
        # so an author who means it is told where to put it instead of being
        # quietly obeyed.
        if _NOT_A_TIEOFF.search(t["body"]) and not _NOT_A_TIEOFF.search(t["head"]):
            raise SystemExit(
                "completion_register: entry %s says 'NOT a tie-off' in its BODY.\n"
                "  %s\n"
                "That phrase settles an entry and must therefore be a DECLARATION, "
                "not prose: put it in the entry's head line (the `// %s. ...` line), "
                "as I9 and I25 do, or reword the body. An entry that can be closed "
                "by a sentence about it is not a register."
                % (t["id"], t["head"][:100], t["id"]))
        # The OTHER two markers legitimately scan the whole entry: they make an
        # entry MORE of a gap, not less, so prose that mentions them costs
        # nothing but an over-report. Only the settling marker is head-only,
        # because only it can make a gap disappear.
        blob = t["head"] + " " + t["body"]
        t["kind"] = ("resolved-in-composer" if _NOT_A_TIEOFF.search(t["head"])
                     else "tied-to-zero" if _TIED_ZERO.search(blob)
                     else "boundary" if _BOUNDARY.search(blob)
                     else "unclassified")
        # "tied to zero" and "no owner exists" are the two that delete logic or
        # have no implementation at all. Boundary means a real port a board or
        # a producer must drive -- still a gap, but a different repair.
        t["mandatory_gap"] = t["kind"] != "resolved-in-composer"
    return out


def declared_modules(path: pathlib.Path) -> set[str]:
    """Every module a source file DECLARES, comments stripped."""
    try:
        src = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return set()
    src = re.sub(r"//[^\n]*", "", src)
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    return set(re.findall(r"^\s*module\s+(zhao_\w+)", src, re.M))


def declared_on_disk() -> set[str]:
    out: set[str] = set()
    for p in (ROOT / "fpga" / "rtl").rglob("*.sv"):
        out |= declared_modules(p)
    return out

def console_closure() -> set[str]:
    """The modules named in zhao_console_core's fit target."""
    if not TARGETS.exists():
        return set()
    text = TARGETS.read_text(encoding="utf-8", errors="replace")
    # ANCHOR ON THE TARGET HEADER, NOT ON THE NAME.
    #
    # This was `text.find("zhao_console_core")` -- the first occurrence of the
    # string ANYWHERE in the file. On 2026-09-19 an agent added a fit target
    # whose comment says "It is composed in zhao_console_core as well", at line
    # 1726, which is 625 lines ABOVE the real target. The scan anchored on that
    # sentence, read the one source under the comment's own target, and returned
    # a closure of ONE MODULE. The register then reported 124 mandatory gaps
    # against 77, and NOTHING ABOUT THE DESIGN HAD CHANGED.
    #
    # It failed in the loud direction this time, which is luck and not design:
    # had the hijacked target been a large one, the closure would have grown and
    # the count would have FALLEN. This is the third defect of the same family in
    # this instrument -- closure membership counted as connection, the unbounded
    # scan that absorbed the next target, and now an anchor that matches prose --
    # and they share one cause: THE REGISTER WAS READING A CONVENTION RATHER THAN
    # A STRUCTURE. A comment cannot accidentally be a target header, so match the
    # header.
    m = re.search(r"^\s*-\s*top:\s*zhao_console_core\s*$", text, re.M)
    if m is None:
        return set()
    i = m.start()
    out: set[str] = set()
    started = False
    for line in text[i:].splitlines()[1:]:
        s = line.strip()
        if s.startswith("- fpga/rtl/") or s.startswith("- fpga\\rtl\\"):
            rel = s[2:].replace("\\", "/")
            out.add(pathlib.Path(rel).stem)
            # A FILE IS NOT A MODULE. `zhao_hps_arbiter.sv` declares both the
            # two-client wrapper and `zhao_hps_arbiter_n`; adding only the stem
            # made the N-client arbiter -- the one R4 composed -- read as NOT
            # BUILT AT ALL on 2026-09-19. Every module the file declares is in
            # the closure. (The handover's instrument defect 6, a third instance.)
            out |= declared_modules(ROOT / rel)
            started = True
            continue
        # The next target begins at a non-indented key. Only stop once this
        # target's own source list has actually started, or a `sources:` line
        # between the target name and its entries ends the scan immediately and
        # the count reads 0 -- which it did on the first run, and a closure of
        # zero would have looked like "nothing is composed" rather than a bug.
        # BOUND THE SCAN AT THE NEXT TARGET.
        #
        # The original stopped only on an unindented non-`-` key. A fit target's
        # header is `- top: ...`, which STARTS WITH `-`, so the scan ran straight
        # through into the next target and absorbed its sources. Since
        # `zhao_console_core` is near the end of the file, every target appended
        # after it silently joined this closure -- and closure membership is one
        # of the two tests for "connected".
        #
        # That is a free, invisible reduction of the mandatory-gap count, inside
        # the instrument whose whole purpose is to prevent exactly that. Found by
        # the SYS.PLL worker, which declined to append a fit target rather than
        # take the reduction. There WAS a target after it (line 2340), so this
        # was not hypothetical.
        # THE `- top:` BREAK IS NOT GUARDED BY `started`, AND THAT MATTERS.
        # Added 2026-09-20 (review Q011) -- the same defect as the paragraph
        # above, one layer down. `started` exists so a `sources:` line between
        # the target name and its first entry cannot end the scan; it has
        # nothing to do with reaching ANOTHER TARGET'S HEADER, which means we
        # have left this target whatever we have seen. With the guard on, a
        # core target whose source lines stopped being recognised -- a path
        # spelling, a `- ./fpga/rtl/...`, a YAML anchor -- left `started` false,
        # so NEITHER break could fire and the walk ran to end of file absorbing
        # every later target's sources. Closure membership is one of the two
        # tests for "connected", so that is a free, invisible reduction of the
        # gap count inside the instrument meant to prevent one.
        if re.match(r"\s*-\s*top:", line):
            break
        if started and s and not s.startswith(("-", "#")) and line[:1] not in " \t":
            break
    # AND AN EMPTY CLOSURE IS A FAILURE, NEVER AN ANSWER. If the target was
    # found but contributed no source line, every module reads "not in the
    # closure" and the capability walk loses its footing. That is the
    # zero-shaped answer this file's own law says to distrust hardest.
    if not started:
        raise SystemExit(
            "completion_register: the `zhao_console_core` fit target was found "
            "in design/fit_targets.yml but contributed NO source line matching "
            "`- fpga/rtl/...`. Either the path spelling changed or the target "
            "was emptied; this register does not guess."
        )
    return out


def instantiated_in(closure_files: list[pathlib.Path]) -> set[str]:
    """Modules actually INSTANTIATED by something in the closure.

    THIS FUNCTION EXISTS BECAUSE THE DOCSTRING ABOVE WAS A LIE FOR A DAY.

    It claimed COMPOSED meant "in the closure AND instantiated", and the code
    tested only closure membership. A worker composing the geometry front end
    found it and said so. The consequence was not theoretical: **adding a
    filename to `design/fit_targets.yml` would have lowered the mandatory-gap
    count without connecting anything** -- the exact "make the number smaller
    without doing the work" failure the owner's Phase 2 rules forbid, available
    in the instrument that polices it.

    The shell's own source list already records eight modules that were declared
    and elaborated nowhere, so this is a failure mode this repository has met.

    An instantiation is `zhao_foo #(` or `zhao_foo u_name` at statement
    position. A mention inside a comment is not one, so comments are stripped.
    """
    text = []
    for p in closure_files:
        try:
            src = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        src = re.sub(r"//[^\n]*", "", src)
        src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
        text.append(src)
    blob = "\n".join(text)
    live = set(re.findall(r"^\s*(zhao_\w+)\s*(?:#\s*\(|\w+\s*\()", blob, re.M))
    # A PACKAGE is imported, never instantiated, and so is the TOP. Counting
    # either as "listed but not instantiated" would be a false alarm, and a
    # checker that cries wolf gets suppressed -- this repo says so twice.
    for p in closure_files:
        stem = p.stem
        if stem.endswith("_pkg") or stem == "zhao_console_core":
            live.add(stem)
    return live


def closure_paths() -> list[pathlib.Path]:
    if not TARGETS.exists():
        return []
    t = TARGETS.read_text(encoding="utf-8", errors="replace")
    # Same anchor, same reason, same defect -- see console_closure() above. These
    # two functions have now carried the identical bug TWICE (the unbounded scan
    # and this one), which is what happens when a pair of functions is kept in
    # step by the author remembering to. The `assert` in main() comparing their
    # two answers is the standing guard against a third.
    m = re.search(r"^\s*-\s*top:\s*zhao_console_core\s*$", t, re.M)
    if m is None:
        return []
    i = m.start()
    out, started = [], False
    for line in t[i:].splitlines()[1:]:
        s = line.strip()
        if s.startswith("- fpga/rtl/"):
            out.append(ROOT / s[2:])
            started = True
            continue
        # same bound as console_closure(); if these two disagree about where the
        # target ends, one of them is reading another target's sources.
        # NOT guarded by `started` -- see console_closure() for why. (The
        # duplicate copy of this same test below was dead code even before that
        # change; review Q011 finding 19.)
        if re.match(r"\s*-\s*top:", line):
            break
        # BOUND THE SCAN AT THE NEXT TARGET.
        #
        # The original stopped only on an unindented non-`-` key. A fit target's
        # header is `- top: ...`, which STARTS WITH `-`, so the scan ran straight
        # through into the next target and absorbed its sources. Since
        # `zhao_console_core` is near the end of the file, every target appended
        # after it silently joined this closure -- and closure membership is one
        # of the two tests for "connected".
        #
        # That is a free, invisible reduction of the mandatory-gap count, inside
        # the instrument whose whole purpose is to prevent exactly that. Found by
        # the SYS.PLL worker, which declined to append a fit target rather than
        # take the reduction. There WAS a target after it (line 2340), so this
        # was not hypothetical.
        if started and s and not s.startswith(("-", "#")) and line[:1] not in " \t":
            break
    if not started:
        raise SystemExit(
            "completion_register: closure_paths() found the `zhao_console_core` "
            "fit target and no `- fpga/rtl/...` source under it. See "
            "console_closure() for why an empty closure is a failure."
        )
    return out


def has_test_target(module: str) -> bool:
    if not CMAKE.exists():
        return False
    return module in CMAKE.read_text(encoding="utf-8", errors="replace")


BLOCKS = ROOT / "design" / "blocks.yml"
RTL = ROOT / "fpga" / "rtl"


def ledger_blocks() -> list[dict]:
    """Every `kind: rtl` block, with the fields that decide whether it is ours.

    `implementation:` would be the obvious source and it is NOT usable: only 6
    of 98 rtl blocks carry one. So the module is resolved by the repository's
    naming convention and then CHECKED against the filesystem -- a convention
    that resolves to nothing is reported as unresolvable rather than counted as
    missing, because a name heuristic over-reports and an over-reported gap list
    sends people to build things that already exist.
    """
    # A MISSING LEDGER IS A FAILURE, NOT AN EMPTY ONE. Added 2026-09-20
    # (review Q011, finding 1). This returned `[]`, which deletes EVERY
    # capability gap -- the disconnected, the unbuilt, the unresolvable, all of
    # it -- and prints a smaller total with no complaint. `tieoffs()` has
    # refused a missing core since it was written, for exactly this reason; the
    # other of the register's two roots had no such guard, and a moved or
    # renamed ledger would have read as a finished console.
    if not BLOCKS.exists():
        raise SystemExit(
            "completion_register: design/blocks.yml is missing. It is one of "
            "the register's TWO roots, and without it every capability gap "
            "vanishes from the total. That is not an empty ledger, it is a "
            "blind instrument."
        )
    text = BLOCKS.read_text(encoding="utf-8", errors="replace")
    out: list[dict] = []
    cur: dict | None = None
    for line in text.splitlines():
        m = re.match(r"\s*-\s*id:\s*(\S+)", line)
        if m:
            if cur:
                out.append(cur)
            cur = {"id": m.group(1), "kind": None, "deferred": None,
                   "blocked_on": None, "implementation": None,
                   "deferred_note": None, "blocked_note": None,
                   "superseded_by": None}
            continue
        if cur is None:
            continue
        # `superseded_by:` takes the WHOLE rest of the line, because its value
        # is a citation (a ruling, a file, a test), not a token. See
        # superseded_verdict() for what it must carry to excuse anything.
        # Every block carries the field, and 118 of them say `null`.
        m = re.match(r"\s*superseded_by:\s*(.+?)\s*$", line)
        if m:
            v = m.group(1).strip().strip('"').strip("'")
            cur["superseded_by"] = None if v in ("null", "~", "") else v
            continue
        for key in ("kind", "deferred", "blocked_on", "implementation"):
            m = re.match(r"\s*%s:\s*(\S+)" % key, line)
            if m:
                cur[key] = m.group(1)
                if key == "deferred":
                    cur["deferred_note"] = line
                elif key == "blocked_on":
                    cur["blocked_note"] = line
    if cur:
        out.append(cur)
    return [b for b in out if b["kind"] == "rtl"]


# Capabilities whose module name the convention cannot construct. Every entry
# was resolved BY HAND against the tree on 2026-09-19 and carries what was
# found, so a reader can re-check rather than trust. `None` means the search
# genuinely found nothing -- those are real "not built" gaps, and leaving them
# in the unresolvable bucket would have reported "0 NOT BUILT AT ALL", which is
# the flattering direction and was the first run's actual output.
_ALIAS: dict[str, str | None] = {
    "MEM.VRAM.ARBITER":  "zhao_vram_arbiter",
    "MEM.HPS.ARBITER":   "zhao_hps_arbiter_n",  # R4: the N-client arbiter; the 2-client module is its superseded wrapper
    "MEM.HPS.BRIDGE":    "zhao_hps_bridge",
    # THE ALIAS NAMED A FROZEN GENERATION. Corrected 2026-09-19, and this one
    # MAKES THE NUMBER SMALLER, so it carries four independent witnesses rather
    # than an argument. `zhao_field_v2_core` is not the console's field engine
    # and cannot become it; the console's field engine is composed, and the
    # register was pointed at the wrong file the entire time.
    #
    # This is the SAME defect `superseded_in_closure()`'s docstring already
    # confesses to -- "the instrument could not see v3 at all, and 'connected'
    # was satisfiable by wiring the old one" -- seen from the other side. That
    # fix stopped the register REWARDING a v2 composition. It did not stop the
    # register PUNISHING the v3 one, so the day FIELD v3 was composed the
    # capability went on reading as a gap and nothing said why.
    #
    #   1. `design/prod_manifest.yml:253` heads its entry, in these words,
    #      "FIELD.SEQ.CORE as a console organ: the sequencer, its program store
    #      (two M10K-inferred RAMs), the residency directory above, and the
    #      arbiter that lets the profiles take turns on ONE engine" -- and the
    #      line under that heading is `- zhao_field_host`.
    #   2. `zhao_console_core.sv`'s own header, in the FIELD.PROGCACHE entry:
    #      "FIELD.SEQ.CORE is composed as `u_field_host`".
    #   3. `zhao_field_host.sv` line 5 declares `Contract:
    #      design/contracts/FIELD.SEQ.CORE.md`, and it instantiates
    #      `zhao_field_v3_engine` -- the composed v3 machine, not v1 and not v2.
    #      (The manifest comment above that list still says "so `zhao_field_host`
    #      composes v1"; that sentence predates commit da57defe and the closure
    #      disagrees with it. The instantiation is the evidence, not the prose.)
    #   4. `zhao_field_v2_core.sv`'s own first lines: "STATUS RULING 2026-08-27
    #      ... FIELD v2 IS FROZEN ... NOT the Earth60 production path, and it
    #      will not become it." The core's header says the same and draws the
    #      conclusion: v2 is unconnected, "and it is FROZEN as the fallback, so
    #      that is a ruling rather than a gap."
    #
    # So the capability has a real producer, a real implementation and a real
    # consumer; only this line disagreed. Aliasing to `zhao_field_host` rather
    # than to `zhao_field_v3_engine` is deliberate: the host is what the console
    # instantiates and what the manifest calls the organ, and the engine inside
    # it can be re-parameterised or replaced without this line going stale.
    "FIELD.SEQ.CORE":    "zhao_field_host",
    # ONE LAW, TWO SCHEDULES, AND THE CONSOLE RUNS THE SECOND (2026-09-19, geom2,
    # owner ruling R31). This makes the number smaller, so it carries four
    # independent witnesses rather than an argument. The convention constructs
    # `zhao_geom_depthquant`, the one-vertex FSM schedule, which since R31 is
    # used only by tests/shell/tb_zhao_shell.sv. The capability -- w to invw24
    # under the profile, `zref::depth_of_raw` -- is performed in the console by
    # `zhao_geom_depthquant_stream`, the tagged schedule of the SAME law:
    #   1. the instantiation graph: `u_geom_vattr` (zhao_console_core section 11)
    #      instantiates `zhao_geom_vattr`, which instantiates
    #      `zhao_geom_depthquant_stream` (`u_dq`) on a private rcp24_v4;
    #   2. both schedules instantiate the same two leaves,
    #      `zhao_geom_depthquant_pre`/`_post`, declared in the SAME file -- the
    #      arithmetic exists once (zhao_geom_depthquant.sv, "ONE LAW, TWO
    #      SCHEDULES");
    #   3. `design/console_inventory.yml` gives the FSM `superseded_by:
    #      zhao_geom_depthquant_stream`, and `design/prod_manifest.yml` counts the
    #      stream INSIDE `zhao_geom_vattr` and declares the FSM superseded;
    #   4. the stream is differenced against `zref::depth_of_raw` itself, out of
    #      order, in tests/geometry/geom_depthquant_stream_directed.cpp, and in
    #      composition by tests/geometry/geom_vattr_directed.cpp (every row).
    "GEOM.DEPTHQUANT":   "zhao_geom_depthquant_stream",
    "TERRAIN.COMPCACHE": "zhao_terrain_compcache_front",
    "TERRAIN.ISLAND":    "zhao_terrain_island_dir",
    # THE NAME CONVENTION RESOLVES THIS ONE TO A SUPERSEDED PROTOTYPE, which is
    # the block-id-to-module failure CLAUDE.md's broken-instrument section
    # already records ("three of them existed under a name the rule did not
    # construct") -- except here the constructed name EXISTS, so nothing looked
    # wrong. `zhao_terrain_residency.sv` is the direct-mapped first draft and
    # its own header opens "FIRST BLOCK OF THE WORLD LAYER. Nothing
    # instantiates it yet."
    #
    # THIS CHANGE MAKES THE NUMBER SMALLER, so it is the kind that has to be
    # checked hardest. It was checked against `design/blocks.yml`'s own
    # TERRAIN.RESIDENCY row, which says it three independent ways:
    #   * purpose: "...the direct-mapped prototype is superseded because two
    #     islands may legally overlap in local patch coordinates";
    #   * tests:   terrain_residency_v2_directed.cpp / _random.cpp;
    #   * maturity UNIT_VERIFIED, evidence terrain_residency_v2_directed.cpp.
    # And structurally: `zhao_terrain_seq`'s master ports match v2 port for
    # port ({epoch, island, ix, iz} key, SEQW claim sequence, pin/unpin) and do
    # NOT match v1's {px, py} lookup at all. `zhao_prod_top` instantiates v2.
    "TERRAIN.RESIDENCY": "zhao_terrain_residency_v2",
    # TERRAIN.BAKE FOLLOWS THE SELECTED CENSUS, 2026-09-20, owner ruling R86.
    # This alias makes the number NEITHER smaller NOR larger -- both modules are
    # unconnected today, so the capability is a gap either way -- and that is
    # exactly why it has to be written now rather than when it closes. The
    # convention resolves TERRAIN.BAKE to `zhao_terrain_bake`, which
    # `design/console_inventory.yml` records as `superseded_by:
    # zhao_terrain_bake_v2` and `design/prod_manifest.yml` now excludes on that
    # ground. Leaving the alias unwritten means the gap would be CLOSEABLE BY
    # WIRING THE SUPERSEDED MODULE, and the register would have said "connected"
    # -- which is the defect `superseded_in_closure()`'s docstring confesses to,
    # arriving a second time by the same route. Checked against blocks.yml's own
    # TERRAIN.BAKE row: its contract, directed and random tests are shared by
    # both implementations, so nothing in that row prefers v1.
    "TERRAIN.BAKE":      "zhao_terrain_bake_v2",
    "GEOM.POSE":         "zhao_geom_pose_decode",
    # GEOM.PROJECT IS THE SHARED SERVICE'S CLIENT A, BY OWNER RULING. Resolved
    # 2026-09-19 (geom packet). THIS MAKES THE NUMBER SMALLER, so it carries
    # four witnesses, and the first is a ruling rather than an argument:
    #   1. reports/OWNER-RULINGS-20260919-EVENING.md R3 (owner, explicit): "Keep
    #      the time-multiplex. No third port in v1." -- geometry (and particles
    #      and FORGE.SHADOW) are projected through `zhao_proj_subsystem` client A.
    #      Composing `zhao_geom_project` as well would be a SECOND
    #      `zhao_project_core` (~6,199 ALM / 33 DSP) doing arithmetic the console
    #      already performs, which the ruling forbids.
    #   2. `zhao_geom_project.sv`'s own header: "THIS BLOCK IS NOW A THIN SHELL"
    #      around `zhao_project_core` -- "a ready/valid handshake, the accepted-
    #      vertex counter, and nothing else".
    #   3. design/blocks.yml GEOM.PROJECT notes: "MERGED 2026-08-24 ... a thin
    #      shell around fpga/rtl/common/zhao_project_core.sv".
    #   4. zhao_console_core.sv: `u_proj_subsystem` client A is driven by
    #      GEOM.GROUP_SEQ (through PART.PROJECT) and its results land in
    #      `u_geom_proj_lane` -- real producer, real core, real consumer.
    # The CENSUS question (prod_manifest still prices the shell separately) is a
    # different question with its own named precondition -- the composed fit --
    # and is deliberately not touched here.
    "GEOM.PROJECT":      "zhao_proj_subsystem",
    # GEOM.LIGHT IS zhao_light_stream, BY OWNER RULING R2 (2026-09-19, owner
    # explicit): "zhao_light_stream owns vertex light. zhao_geom_light is
    # superseded." The naming convention resolves GEOM.LIGHT to the SUPERSEDED
    # scalar block -- supersession by RENAME, the third shape the core header's
    # lighting section says no tool looks for -- so "connected" would have been
    # satisfiable by wiring the wrong one. Witnesses: the ruling;
    # zhao_light_stream.sv's own first section, "THIS REPLACES THE OWNER, IT DOES
    # NOT ADD A SECOND LAW"; console_inventory.yml, zhao_geom_light superseded_by
    # zhao_light_stream; and the composition, `u_light_stream` fed by GEOM.SKIN.NORM
    # through `u_light_skin_adapter`.
    "GEOM.LIGHT":        "zhao_light_stream",
    # TERRAIN.PROJECT IS THE SAME SHARED SERVICE'S CLIENT B. Resolved 2026-09-19
    # (terrain packet), the twin of the GEOM.PROJECT line above and for the same
    # reason. THIS MAKES THE NUMBER SMALLER, so four witnesses, and the fourth is
    # a MEASUREMENT rather than a reading:
    #   1. `zhao_terrain_project.sv`'s own header: "THE PROJECTOR IS NO LONGER IN
    #      THIS FILE" -- it keeps the vertex sequencer, triangle reassembly and
    #      riders around ONE `zhao_project_core`; `zhao_proj_subsystem`'s header
    #      names its triangle output as "zhao_terrain_project's packet plus three
    #      `w`" -- the same packet, the Mosaic riders included.
    #   2. design/blocks.yml TERRAIN.PROJECT notes: "MERGED 2026-08-24 ... The
    #      projection law now lives once, in fpga/rtl/common/zhao_project_core.sv".
    #   3. zhao_console_core.sv's own TERRAIN.PROJECT paragraph: "REFUSED, AND IT
    #      IS A SAVING RATHER THAN A GAP. It is a SECOND PROJECTOR" (6,068 ALM /
    #      33 DSP); terrain reaches the shared core on client B through
    #      TERRAIN.GROUP_SEQ, and composing the shell would undo the dedup that
    #      `zhao_project_core` exists for. Owner ruling R3's "keep the
    #      time-multiplex" is the same sharing decision made for client A.
    #   4. MEASURED in the console smoke bench: `proj_b_grants_o` = 81 and
    #      `proj_replay_triangles_o` = 128 -- terrain vertices ARE projected
    #      through client B and replayed as triangles in this composition.
    # Its downstream consumer is GEOM.SETUP/GEOM.CLIP's door (entry I13), which
    # is a separately counted tie-off; this line does not touch it. The census
    # question (prod_manifest prices the shell separately) is the fit's, as for
    # GEOM.PROJECT, and is not touched. Reversible in one line if the owner
    # rules client B is not the terrain projector's home.
    "TERRAIN.PROJECT":   "zhao_proj_subsystem",
    # ---- THE TEXTURE CLUSTER, hand-resolved 2026-09-19 ---------------------
    #
    # SIX capabilities resolved by the naming convention to SIX SUPERSEDED
    # PROTOTYPES, which is the `TERRAIN.RESIDENCY` failure above repeated six
    # times: the constructed name EXISTS, so nothing looked wrong, and the
    # register sent readers to compose modules `design/prod_manifest.yml`
    # explicitly forbids composing ("must not put a second root into the
    # census").
    #
    # WHAT WAS ACTUALLY THE CASE, measured rather than argued:
    # `zhao_texture_island_v3_top` is ALREADY in the console closure and
    # ALREADY live -- `zhao_console_core` -> `zhao_shell_top_v2` ->
    # `zhao_raster_tile_pipe_v2` (line 802) -> `zhao_raster_texture_stage_v3`
    # (line 362) -> the island. Every one of its seventeen submodules returns
    # True from both `console_closure()` and `instantiated_in(closure_paths())`.
    # So the texture path was composed; only its NAMES were wrong here.
    #
    # `successor_in()` below cannot and must not fix this. Its comment already
    # refuses `zhao_texture_cache_pipe_v2` as an automatic successor of
    # `zhao_texture_cache`, correctly: the automatic rule would also match
    # `zhao_texture_tmu_plan_v2` to `zhao_texture_tmu`, and that file's own
    # third line says it succeeds the PLANNER. A loose prefix rule retires real
    # gaps by coincidence of naming. These are hand resolutions with evidence
    # instead, one per line, so a reader can re-check rather than trust.
    #
    # THIS CHANGE MAKES THE NUMBER SMALLER, so each entry carries TWO
    # independent witnesses: what `prod_manifest.yml` says, and what the
    # successor's own header says about the module it replaces.
    #
    #   prod_manifest.yml:699 "zhao_texture_cache: superseded  by
    #   zhao_texture_cache_pipe_v2 inside the Packet B selected V3 root", and
    #   that file's own line 9: "The unversioned cache remains the executable
    #   old-island oracle."
    "TEXTURE.CACHE":     "zhao_texture_cache_pipe_v2",
    #   prod_manifest.yml:701 names zhao_texture_aux_pipe_v2, the ledger's own
    #   TEXTURE.AUX row already points at `design/contracts/TEXTURE.AUX.V2.md`
    #   with tests `texture_aux_pipe_v2_*`, and that file's line 3 says it "is
    #   the versioned adapter owned by TEXTURE.AUX.V2".
    "TEXTURE.AUX":       "zhao_texture_aux_pipe_v2",
    #   prod_manifest.yml:474 "material_combine_v3 is the combiner inside the
    #   Packet B selected V3 root"; the ledger's TEXTURE.COMBINE row already
    #   describes the eight-recipe R9 engine and tests
    #   `material_combine_v3_diff.cpp`; and v3's own header says "V2 remains
    #   the executable oracle for the old island".
    "TEXTURE.COMBINE":   "zhao_texture_material_combine_v3",
    #   prod_manifest.yml:711 "zhao_texture_v3own is the sole owner inside the
    #   selected V3 subsystem", and the island's own header: "zhao_texture_v3own
    #   is the only fragment-lifecycle owner". FRAGROB was the differential
    #   CANDIDATE beside TEXJOIN, which is why its row says `not-yet-adopted`
    #   rather than `superseded` -- the CAPABILITY still has an owner.
    "TEXTURE.FRAGROB":   "zhao_texture_v3own",
    #
    # TEXTURE.TMU RESOLVES TO THE V3 OWNER, BY OWNER RULING R9 (provisional,
    # reports/OWNER-RULINGS-20260919-EVENING.md): "Retire TEXTURE.TMU as a
    # single module in favour of the v3 path, and give `texture_samples` ONE
    # owner: the v3 block that retires a filtered sample to the fragment. It
    # counts samples it actually delivered."
    #
    # It stayed a gap until 2026-09-19 evening for a reason that was right: the
    # manifest supersedes the old TMU "by the Packet B texture plan/cache/
    # dispatch PATH", and no single stage of that path IS the TMU -- pointing
    # this at the planner would be the false reduction `successor_in()` refuses.
    # R9 answers the question that refusal asked, by naming the RETIREMENT
    # rather than a sampling stage. Two witnesses, as for FRAGROB above:
    #   * `zhao_texture_v3own.sv` publishes a TMU response into its fragment's
    #     commit bitplane only when C4 is valid and the slot generation holds,
    #     and `ev_texture_samples_o` counts exactly those published sample bits;
    #   * the island exports it as `cnt_texture_samples_o`, and
    #     tests/texture/texture_island_v3_packet_b_directed.cpp asserts it
    #     exactly: +0 for a count-zero PASSTHRU, +1 NEAR, +1 CLUT.
    # Two capabilities resolving to one module is deliberate: the ruling makes
    # the fragment owner the sample owner. If R9 is revised, THIS LINE is the
    # edit, and `zhao_texture_tmu` goes back to `pending_compose`.
    "TEXTURE.TMU":       "zhao_texture_v3own",
    #    # ---- AND ONE CORRECTION IN THE OTHER DIRECTION -------------------------
    #
    # MATERIAL.RESOLVE was aliased to `zhao_texture_material_combine_v2`, which
    # made an UNBUILT capability read as BUILT-BUT-NOT-CONNECTED. That is the
    # flattering direction and it is wrong on the tree's own evidence:
    #
    #   * the ledger row is `maturity: SPECIFIED` with `maturity_log: []` and
    #     BOTH tests recorded as "PLANNED -- NOT WRITTEN", and its note says it
    #     is blocked on a cartridge decision (audit R4);
    #   * `zhao_texture_combine.sv`'s own header lists "resolution
    #     (MATERIAL.RESOLVE's)" among the things it REFUSES to do. The combiner
    #     consumes a material record; it does not produce one.
    #
    # So this is `None` -- searched and genuinely absent. The mandatory total
    # does not fall because of this line; one capability moves from the
    # disconnected bucket to the unbuilt one, which is where it belongs. It
    # matters beyond bookkeeping: MATERIAL.RESOLVE is the owner of
    # `tri_flat_request_i`, the half of the console core's entry I20 that
    # GEOM.ATTRPACK did not close.
    # BUILT 2026-09-19 (fpga/rtl/.../zhao_material_resolve.sv). FOURTH catch by
    # _stale_none_aliases(), this one minutes after the texture packet correctly
    # set it to None -- the block it said was absent then exists now.
    "MATERIAL.RESOLVE":  "zhao_material_resolve",
    # searched and genuinely absent -- no file matches these at all
    # BUILT 2026-09-19 (fpga/rtl/mem/zhao_mem_upload.sv, 89 directed checks).
    # This entry said None until the block existed, and nothing would have
    # re-checked it -- see _stale_none_aliases(), which now does, and which
    # fired on this very line.
    "MEM.UPLOAD":        "zhao_mem_upload",
    # BUILT 2026-09-19 (fpga/rtl/geometry/zhao_geom_loom.sv). The THIRD entry
    # _stale_none_aliases() has caught in one day -- MEM.UPLOAD, FORGE.SHADOW,
    # and now this one, which it caught while the block was still being written
    # and was blocking every concurrent worker's register run until fixed.
    # Pointing it at the module is correct whoever does it.
    "GEOM.LOOM":         "zhao_geom_loom",
    # BUILT 2026-09-19 (fpga/rtl/forge/zhao_forge_shadow.sv, 39 directed checks).
    # The SECOND entry _stale_none_aliases() has caught the same day it was
    # added, which is the argument for the guard rather than for my memory.
    "FORGE.SHADOW":      "zhao_forge_shadow",
    # the owner revoked their deferral 2026-09-18; both were cut BEFORE their
    # contracts were written, so each needs its spec authored first.
    # INPUT.SNAC: spec (spec/input_rules.md 7), contract and RTL written
    # 2026-09-20 under ruling R7, and COMPOSED in zhao_console_core between its
    # pad_*_i ports and the shell's INPUT.SNAPSHOT. It is no longer a None.
    "INPUT.SNAC":        "zhao_input_snac",
    # GEOM.WARP: BUILT 2026-09-20 as fpga/rtl/geometry/zhao_geom_warp.sv by
    # packet W1, so this is no longer a None. It moves the block from
    # `unbuilt` to `built_not_connected` and THE TOTAL DOES NOT CHANGE --
    # :1480-1482 sums both terms, which is the register working as designed.
    #
    # It is NOT connected and the Field port is NOT tied off. Those are two
    # different statements and only the first is visible to this tool: the
    # tie-off list is parsed out of zhao_console_core.sv's own header block
    # (:67-244) and there is NO structural scan for a port tied to a constant.
    # So a future composition that ties the Field request port off would read
    # CONNECTED here and drop the total to 20 -- green, and wrong. The guard
    # against that is design/blocks.yml's GEOM.WARP note and the packet
    # protocol, not this instrument. Written down because an instrument that
    # cannot see a fault must say so where somebody will read it.
    "GEOM.WARP":         "zhao_geom_warp",
    # POST.ECHO: spec written and BUILT 2026-09-19 (ruling R7) -- composed on
    # POST.COMPOSITE's echo tap inside the shell's zhao_post_lease.
    "POST.ECHO":         "zhao_post_echo",
    # `deferred`/`blocked_on` used to skip these before resolution ever ran, so
    # they never needed an alias. Now that nothing is excused by a bare flag,
    # they reach the resolver and must resolve.
    "SYS.CDC":           "zhao_cdc_snapshot",
    "MEM.SDRAM":         "zhao_sdram_ctrl",
    # BUILT 2026-09-19, and this edit MAKES THE NUMBER SMALLER, so it is the
    # kind to check hardest. What changed is not the rule but the tree:
    # `fpga/rtl/sys/zhao_sys_pll.sv` and `fpga/rtl/sys/zhao_sys_reset.sv` now
    # exist, lint 0/0 under -Wall with a fired positive control, pass
    # `check_quartus17_syntax`, and each has a registered directed ctest whose
    # counters are SEEN TO MOVE as deltas. The naming convention resolves both
    # on its own -- SYS.PLL -> zhao_sys_pll -- so these two lines could simply
    # be deleted; they are spelled out instead so that the transition from
    # "hand-searched and genuinely absent" to "built" stays legible in the one
    # place a reader will look.
    #
    # THEY REMAIN GAPS, in the DISCONNECTED bucket, and that is the honest
    # result: they are composed in `zhao_console_board`, not in
    # `zhao_console_core`, so they are not in the closure this register reads.
    # The mandatory total does not move -- only which bucket they sit in.
    "SYS.PLL":           "zhao_sys_pll",
    "SYS.RESET":         "zhao_sys_reset",
}


def superseded_in_closure() -> list[tuple[str, list[str]]]:
    """Composed modules for which a HIGHER-VERSIONED sibling exists on disk.

    OWNER RULING, 2026-09-19, verbatim and unambiguous:

        "YOU ONLY GET TO FIT THE LATEST VERSION. IF IT IS BROKEN YOU FIX IT."

    It was issued because the console had composed the ENTIRE v1 FIELD datapath
    -- alu, seq, ring, rot, noise, mul, len, normalize -- while all FOURTEEN
    `zhao_field_v3_*` modules sat outside the closure. Fitting that measures a
    machine nobody ships: it spends ALM and DSP budget on dead weight and makes
    the eventual resource number describe the wrong design, on a device already
    over on both (47,582 ALM against 41,910; 151 DSP against 112).

    THE REGISTER HELPED CAUSE IT. `FIELD.SEQ.CORE` resolves to
    `zhao_field_v2_core` and `FIELD.PROGCACHE` to `zhao_field_progcache` -- one
    superseded, one unversioned -- so the instrument could not see v3 at all,
    and "connected" was satisfiable by wiring the old one. That is a NEW
    flattering direction and it belongs with the five already fixed here: a gap
    closed by composing the wrong version is not a gap closed.

    TWO NAMING SHAPES, because this tree uses both:

        suffix  zhao_texture_cache_pipe_v2  supersedes  zhao_texture_cache_pipe
        infix   zhao_field_v3_len           supersedes  zhao_field_len

    `successor_in()` handles the suffix shape and only to REDUCE the count. This
    is the opposite question and it is asked of the closure rather than of a
    capability: what have we actually wired that something newer exists for?

    Reported, not fatal, and that is a deliberate and temporary choice: the FIELD
    v1 -> v3 swap is in flight as this lands, and a hard failure would block
    every concurrent worker's register run for the duration. TURN IT FATAL once
    the swap is committed -- a report nobody is forced to read is how the v1
    datapath got composed in the first place.
    """
    # INSTANTIATED, not merely listed. A module that sits in the source list but
    # nothing elaborates costs the fitter nothing -- Quartus builds what is
    # reachable from the top -- so flagging it would be a false alarm, and a
    # check that cries wolf about dead sources is a check people learn to skip.
    # The real fault is having WIRED the old one.
    on_disk = {p.stem for p in RTL.rglob("*.sv")}
    live = instantiated_in(closure_paths())
    out: list[tuple[str, list[str]]] = []
    for mod in sorted(console_closure() & live):
        newer: list[str] = []
        for cand in on_disk:
            if cand == mod:
                continue
            # suffix:  <mod>_v2, <mod>_v3, ...
            m = re.fullmatch(re.escape(mod) + r"_v(\d+)", cand)
            if m:
                newer.append(cand)
                continue
            # infix:   zhao_<sub>_v3_<rest>  against  zhao_<sub>_<rest>
            m2 = re.fullmatch(r"(zhao_[a-z0-9]+)_v(\d+)_(.+)", cand)
            if m2 and "%s_%s" % (m2.group(1), m2.group(3)) == mod:
                newer.append(cand)
        if newer:
            out.append((mod, sorted(newer)))
    return out


def newer_versions(mod: str, on_disk: set[str]) -> list[str]:
    """Higher-versioned siblings of `mod` that exist on disk, in both shapes.

    ONE COPY, because there were three and they had to be kept in step by
    somebody remembering -- which is how `superseded_in_closure()` and
    `superseded_in_prod_fit()` came to hold byte-identical loops.
    """
    newer: list[str] = []
    for cand in on_disk:
        if cand == mod:
            continue
        # suffix:  <mod>_v2, <mod>_v3, ...
        if re.fullmatch(re.escape(mod) + r"_v(\d+)", cand):
            newer.append(cand)
            continue
        # infix:   zhao_<sub>_v3_<rest>  against  zhao_<sub>_<rest>
        m = re.fullmatch(r"(zhao_[a-z0-9]+)_v(\d+)_(.+)", cand)
        if m and "%s_%s" % (m.group(1), m.group(3)) == mod:
            newer.append(cand)
    return sorted(newer)


def ledger_superseded() -> dict[str, str]:
    """`{module: superseded_by}` from `design/console_inventory.yml`.

    THE THIRD NAMING SHAPE, AND THE ONLY ONE THAT IS NOT A CONVENTION.
    `newer_versions()` keys on the FILENAME -- `_v2` suffix, `_v3_` infix -- and
    CLAUDE.md already warns that a grep for one finds half. It is worse than
    half, because this tree also supersedes with no version number anywhere in
    the name:

        zhao_mem_share2    -> zhao_mem_share_n      (owner ruling R4)
        zhao_hps_arbiter   -> zhao_hps_arbiter_n    (the same ruling)
        zhao_geom_project  -> zhao_proj_subsystem   (owner ruling R3)
        zhao_terrain_project -> zhao_proj_subsystem (owner ruling R3)

    A shape check is structurally blind to all four. The ledger is not a
    convention -- it is a declared fact with a cited ruling beside it -- so
    reading it back is the check that cannot be fooled by a rename. And reading
    it back is the whole point: on 2026-09-20 `zhao_prod_top` composed
    `zhao_geom_project` AND `zhao_terrain_project`, each of which the ledger
    describes as "a second projector", ~6,100 ALM and 33 DSP apiece, while
    `check_console_inventory` printed "the latest version is the one wired".
    """
    inv = ROOT / "design" / "console_inventory.yml"
    if not inv.exists():
        return {}
    out: dict[str, str] = {}
    cur = None
    disposition = None
    for line in inv.read_text(encoding="utf-8", errors="replace").splitlines():
        m = re.match(r"^  (zhao_\w+):\s*$", line)
        if m:
            cur, disposition = m.group(1), None
            continue
        if cur is None:
            continue
        d = re.match(r"^\s+disposition:\s*(\S+)\s*$", line)
        if d:
            disposition = d.group(1)
            continue
        s = re.match(r"^\s+superseded_by:\s*(\S+)\s*$", line)
        if s and disposition == "superseded":
            out[cur] = s.group(1)
    return out


def declaring_file(mod: str) -> pathlib.Path | None:
    """The .sv file that DECLARES `mod`, not the one whose stem matches it.

    `p.stem` is right for almost every module here and wrong for the ones that
    matter: `zhao_mem_share_n` is declared inside `zhao_mem_share2.sv`, and
    `zhao_sys_pll_simclk` inside `zhao_sys_pll.sv`. A stem lookup returns None
    for those, and a root that returns None gets SKIPPED -- silence in the
    flattering direction, which is the whole family of defect this file is
    about.
    """
    return _declaration_index().get(mod)


_DECL_INDEX: dict[str, pathlib.Path] = {}


def _declaration_index() -> dict[str, pathlib.Path]:
    """`{module: file}` for every module under fpga/rtl, built once.

    Cached because the sweep asks this of ~140 roots and the uncached version
    re-read every file each time.
    """
    if _DECL_INDEX:
        return _DECL_INDEX
    pat = re.compile(r"^\s*module\s+(\w+)", re.M)
    for p in sorted(RTL.rglob("*.sv")):
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for name in pat.findall(text):
            _DECL_INDEX.setdefault(name, p)
    return _DECL_INDEX


def production_roots() -> list[str]:
    """Every root whose closure the console SHIPS or PRICES.

    R86, 2026-09-20: *"extend the superseded check to EVERY root, not just the
    core: a blind spot the size of a top-level is not a gap in coverage, it is a
    second machine nobody audits."*

    WHAT "EVERY ROOT" HAD TO MEAN, because the literal reading is a check that
    cries wolf and therefore a check nobody runs. `design/fit_targets.yml` has
    ~100 `- top:` rows and most of them are LEAF MEASUREMENTS or retained
    ORACLES -- `zhao_texture_tmu_pipe` and `zhao_raster_rcp24_v3` are kept
    precisely because they are the superseded version, and a gate that fails on
    them is teaching people to pass `--no-verify` to their own instrument.

    So the fatal set is the set of roots that CLAIM TO BE THE MACHINE:

      * `zhao_console_board` -- the only module in this tree that could be
        programmed onto hardware;
      * `zhao_console_core`  -- the machine inside it (already covered by
        `superseded_in_closure()`, kept here so the two cannot drift);
      * `zhao_prod_top`      -- what R80's completion fit actually measures;
      * every `top:` in `design/prod_manifest.yml` -- the owner's own list of
        "exactly one chosen implementation of every intended production block".
        If one of THOSE wires a superseded module, the superseded module is in
        the console by the manifest's own definition.

    Everything else in `design/fit_targets.yml` is reported separately and is
    not fatal, so the blind spot stays visible without the gate going soft.
    """
    roots = {BOARD_ROOT, CONSOLE_ROOT, "zhao_prod_top"}
    manifest = ROOT / "design" / "prod_manifest.yml"
    if manifest.exists():
        text = manifest.read_text(encoding="utf-8", errors="replace")
        m = re.search(r"^top:\s*$", text, re.M)
        if m:
            for line in text[m.end():].splitlines():
                if re.match(r"^[A-Za-z_]", line):      # the next section
                    break
                hit = re.match(r"^\s{2}-\s+(zhao_\w+)\s*$", line)
                if hit:
                    roots.add(hit.group(1))
    return sorted(roots)


def advisory_roots() -> list[str]:
    """Fit-target tops that are NOT production roots: instruments and oracles."""
    if not TARGETS.exists():
        return []
    text = TARGETS.read_text(encoding="utf-8", errors="replace")
    tops = set(re.findall(r"^\s*-\s*top:\s*(zhao_\w+)\s*$", text, re.M))
    return sorted(tops - set(production_roots()))


def superseded_in_roots(
    roots: list[str] | None = None,
) -> tuple[list[tuple[str, str, list[str]]], list[str]]:
    """`(root, wired_module, newer)` for every root, plus roots not found.

    A root is asked only about what IT INSTANTIATES, one file deep, for the same
    reason `superseded_in_closure()` gives: a module in a source list that
    nothing elaborates costs the fitter nothing, and a check that cries wolf
    about dead sources is one people learn to skip. The transitive case is
    covered because every intermediate module of the production hierarchy is
    itself a `top:` in the manifest and therefore itself a root here.
    """
    on_disk = {p.stem for p in RTL.rglob("*.sv")}
    ledger = ledger_superseded()
    out: list[tuple[str, str, list[str]]] = []
    unfound: list[str] = []
    for root in (roots if roots is not None else production_roots()):
        path = declaring_file(root)
        if path is None:
            unfound.append(root)
            continue
        for mod in sorted(instantiated_in([path])):
            if mod == root:
                continue
            # BOTH SIGNALS, UNIONED. The shape check finds what the ledger has
            # not been told about yet; the ledger finds what no filename could
            # express. Either alone reads clean on half of this tree.
            newer = newer_versions(mod, on_disk)
            if mod in ledger and ledger[mod] not in newer:
                newer = sorted(newer + [ledger[mod]])
            if newer:
                out.append((root, mod, newer))
    return out, unfound


def superseded_in_prod_fit(
    core_hits: list[tuple[str, list[str]]] | None = None,
) -> list[tuple[str, list[str]]]:
    """The same question asked of the PRODUCTION FIT's root, not the core's.

    WHY THIS EXISTS (terrain7, 2026-09-20). `superseded_in_closure()` above is
    scoped to `console_closure()` -- the modules under `zhao_console_core`'s fit
    target. `zhao_prod_top` is a DIFFERENT ROOT, it is what the production fit
    actually builds, and nothing was asking it this question. So the owner's
    capitalised ruling had a blind spot exactly the size of the pin-out top.

    IT IS NOT HYPOTHETICAL. On the day this was written:

      * `fpga/rtl/prod/zhao_prod_top.sv:4076` instantiates `zhao_terrain_bake`;
      * `design/console_inventory.yml:360` records that very module as
        `disposition: superseded`, `superseded_by: zhao_terrain_bake_v2`,
        `why: "... owner ruling 2026-09-19 'only the latest version'"`;
      * `fpga/quartus/prod_fit_sources.ORPHANED.txt:122` carried
        `zhao_terrain_bake.sv` into the production fit -- note the name:
        that file was ALREADY orphaned when this was written, and citing
        it is the very mistake described below. It is renamed so a grep
        hit labels itself (owner ruling R175); read it as history, never
        as the current fit list;
      * and `check_console_inventory.py`, `check_prod_manifest.py` and
        `gen_prod_top.py --check` were all GREEN.

    The ledger recorded the ruling, the fit list ignored it, and every gate
    agreed. That is this repository's standing shape -- the knowledge was
    written down, correctly, and nothing read it back -- and it is the same
    shape as the `.gitignore` that hid 33 GB and the projector cheque nobody
    cashed. `zhao_prod_top` is GENERATED, so it wears a reassuring provenance
    line while doing it.

    IT IS FATAL SINCE 2026-09-20, and it is no longer alone: the decision landed
    (owner ruling R86), the swap is committed, and `superseded_in_roots()` above
    asks the same question of every production root rather than of this one.
    This function is retained because it names the incident, and because a
    narrow re-derivation of the same query is a cheap independent check on the
    general one.

    Same two naming shapes, same INSTANTIATED-not-listed rule, same reasons.
    """
    top = RTL / "prod" / "zhao_prod_top.sv"
    if not top.exists():
        return []
    on_disk = {p.stem for p in RTL.rglob("*.sv")}
    live = instantiated_in([top])
    # Anything the CORE closure already reports is that check's to raise; this
    # one exists for what only the pin-out top wires, so the two lists do not
    # double-count the same module and neither reads as the other's echo.
    already = {mod for mod, _ in
               (superseded_in_closure() if core_hits is None else core_hits)}
    out: list[tuple[str, list[str]]] = []
    for mod in sorted(live - already):
        newer: list[str] = []
        for cand in on_disk:
            if cand == mod:
                continue
            m = re.fullmatch(re.escape(mod) + r"_v(\d+)", cand)
            if m:
                newer.append(cand)
                continue
            m2 = re.fullmatch(r"(zhao_[a-z0-9]+)_v(\d+)_(.+)", cand)
            if m2 and "%s_%s" % (m2.group(1), m2.group(3)) == mod:
                newer.append(cand)
        if newer:
            out.append((mod, sorted(newer)))
    return out


_BOARD_CACHE: dict[str, set[str]] = {}


def board_addition() -> set[str]:
    """Modules the BOARD composes that the CORE does not -- the second root.

    WHY THIS EXISTS, and why it is not "widening the register".

    `zhao_console_board` instantiates `zhao_console_core` and drives it from
    `zhao_sys_pll` and `zhao_sys_reset`. It is the only module in this tree that
    could be programmed onto hardware; the core is an organ of it. So SYS.PLL
    and SYS.RESET are COMPOSED -- they have a real producer (the 50 MHz board
    oscillator characterised in `board_truth.json`), a real implementation, and
    a real consumer (every clock and reset the core runs on). The owner's
    standard is that a capability is absent when it is "a disconnected
    implementation". These are not disconnected. They are one level up.

    THE OBJECTION THE BOARD PACKET RAISED, AND WHY IT DOES NOT SURVIVE
    MEASUREMENT. It declined to re-point this register because "that would
    change what CONNECTED means for all 77 capabilities at once, in the
    direction that makes the number smaller". That is the right instinct and it
    is the reason this function returns a DIFFERENCE rather than a union of
    closures. The difference is measurable and it was measured:

        board closure - core closure = {zhao_console_board, zhao_sys_pll,
                                        zhao_sys_pll_simclk, zhao_sys_reset,
                                        zhao_sys_reset_sync}

    FIVE modules, of which TWO are ledger capabilities. It is not a change to 77
    judgements; it is a change to exactly the judgements about modules the board
    adds, and every run prints that set by name (`--json` carries it too), so a
    sixth member could never arrive unnoticed. Nothing that the core composes
    moves, in either direction, because the set is disjoint from the core's
    closure by construction.

    WHAT KEEPS IT FROM BECOMING A LAUNDRY. Three things, and the first is the
    one that matters:

      * THE BOARD MUST INSTANTIATE THE CORE. If it ever stops, this returns the
        EMPTY SET rather than a five-module amnesty -- so a board that has been
        detached from the machine cannot go on excusing the blocks hanging off
        it. `check_console_inventory.py` makes the same check and fails loudly;
        here the failure mode is chosen to be the one that COUNTS MORE GAPS,
        because this file's whole discipline is to fail upward.
      * It is derived from the instantiation graph (`module_graph.build`), the
        same walk `check_console_inventory.py` uses for its second root, not
        from a list anybody maintains. There is no fit target for the board and
        this deliberately does not invent one -- a fit source list is a claim
        about what gets MEASURED for area, and the board's own area question is
        a separate packet.
      * A module the board adds still has to be a ledger capability to count for
        anything. `zhao_sys_pll_simclk` and `zhao_sys_reset_sync` are helpers
        inside those two files and resolve to no block id at all.
    """
    if "d" in _BOARD_CACHE:
        return _BOARD_CACHE["d"]
    try:
        sys.path.insert(0, str(ROOT / "tools" / "quartus"))
        from module_graph import build            # noqa: PLC0415
    except ImportError:                           # pragma: no cover
        _BOARD_CACHE["d"] = set()
        return _BOARD_CACHE["d"]
    decl, inst = build(str(ROOT / "fpga" / "rtl"))
    _BOARD_CACHE["d"] = _board_addition_from(decl, inst)
    return _BOARD_CACHE["d"]


def _reach(root: str, decl: dict, inst: dict) -> set[str]:
    if root not in decl:
        return set()
    seen, stack = {root}, [root]
    while stack:
        cur = stack.pop()
        for child in inst.get(decl[cur], ()):
            if child not in seen and child in decl:
                seen.add(child)
                stack.append(child)
    return seen


def _board_addition_from(decl: dict, inst: dict) -> set[str]:
    """The difference, split out so the self-test can drive it on a toy graph."""
    board = _reach(BOARD_ROOT, decl, inst)
    # THE GUARD, and it is the whole safety of this function: a board that does
    # not contain the core is not the machine's top, so nothing under it is
    # "composed into the console" by any reading.
    if CONSOLE_ROOT not in board:
        return set()
    return board - _reach(CONSOLE_ROOT, decl, inst)


def _stale_none_aliases() -> list[tuple[str, str]]:
    """Hand-resolved `None` entries for which a module now EXISTS.

    A `None` in `_ALIAS` means "searched by hand on 2026-09-19 and genuinely
    absent". That is a statement about a moment, and this campaign's whole job is
    to make such statements false by building the thing. Nothing re-checked them,
    so the first block built to close one of these entries stayed counted as
    UNBUILT -- the register would have gone on reporting a gap that had been
    filled, and the only way to notice was to wonder why a number did not move.

    MEM.UPLOAD proved it the same day it was written. So the hand-resolutions are
    now re-checked against the tree on every run, and a stale one is a HARD
    FAILURE rather than a silent under-count: an alias that outlives its search
    is exactly the phantom-citation shape this file exists to prevent.
    """
    stale = []
    for cap, mod in _ALIAS.items():
        if mod is not None:
            continue
        cand = "zhao_" + cap.lower().replace(".", "_")
        if list(RTL.rglob(cand + ".sv")):
            stale.append((cap, cand))
    return stale


def resolve_module(block_id: str, implementation: str | None) -> str | None:
    if block_id in _ALIAS:
        return _ALIAS[block_id]          # may be None: searched, genuinely absent
    if implementation:
        p = ROOT / implementation
        if p.exists():
            return p.stem
    # PART.STATE -> zhao_part_state ; GEOM.SKIN.NORM -> zhao_geom_skin_norm
    cand = "zhao_" + block_id.lower().replace(".", "_")
    if (list(RTL.rglob(cand + ".sv"))):
        return cand
    return None


def successor_in(mod: str, closure: set[str], live: set[str]) -> str | None:
    """Is a VERSIONED SUCCESSOR of `mod` composed instead of `mod` itself?

    TEXTURE.CACHE resolves by convention to `zhao_texture_cache`, which nothing
    composes -- but `zhao_texture_island_v3_top` IS composed and instantiates
    `zhao_texture_cache_pipe_v2`. The capability is present; the resolver was
    naming the superseded file. Counting that as a mandatory gap inflates the
    number and sends someone to compose a module the tree has already replaced.

    THIS CORRECTION MAKES THE NUMBER SMALLER, which is the flattering direction,
    so the bar is deliberately high: the successor must be BOTH in the closure
    AND instantiated, and its name must be `mod` plus a version suffix -- never a
    fuzzy match. The successor is reported by name so every reduction is
    auditable rather than asserted.
    """
    for cand in sorted(closure & live):
        if cand == mod or not cand.startswith(mod + "_"):
            continue
        tail = cand[len(mod) + 1:]
        # THE TAIL MUST BE A VERSION AND NOTHING ELSE.
        #
        # The first draft allowed `(?:\w+_)?v\d+`, and it immediately produced a
        # FALSE reduction: it matched `zhao_texture_tmu_plan_v2` as a successor
        # of `zhao_texture_tmu`. That file's own third line says it is the
        # successor of `zhao_texture_tmu_plan.sv` -- the PLANNER, not the
        # sampler. Two different modules whose names happen to share a prefix.
        #
        # Same for `zhao_texture_cache_pipe_v2` (successor of
        # `..._cache_pipe`) and `zhao_texture_aux_pipe_v2`. A loose prefix rule
        # retires a real gap by coincidence of naming, and it does so in the
        # direction that makes the campaign look further along than it is --
        # which is the one direction this register must never fail in.
        if re.fullmatch(r"v\d+", tail):
            return cand
    return None


_CITED_PATH = re.compile(
    r"\b((?:reference|tests|runtime|spec|design|tools|compiler|fpga)/"
    r"[\w./-]+\.(?:hpp|cpp|h|sv|md|py|ts|zidl))\b")


def superseded_verdict(note: str, cmake_text: str,
                       exists=lambda rel: (ROOT / rel).exists()) -> str | None:
    """Does a ledger `superseded_by:` note EXCUSE its capability? None = yes.

    Added 2026-09-19 for owner ruling R16: a hardware block whose function a
    RULING moved to another provider (TERRAIN.VISIBLE, moved into SW.STREAM by
    T5) is a duplicate provider, and removing it is allowed -- but only if the
    replacement is really there. THIS MAKES THE NUMBER SMALLER, so the bar is
    the one `successor_in()` sets, applied to prose: every condition below is
    checked against the TREE, and a note that fails any of them is counted AS A
    GAP (reported with the uncited excuses), never as a pass.

      1. it cites a RULING by name (`ruling T5`, `R16`), not an opinion;
      2. it names at least one repository file, and EVERY file it names exists
         -- a supersession pointing at a deleted file is an uncashed cheque;
      3. at least one named file is a test under `tests/` that
         `tests/CMakeLists.txt` actually builds -- "implemented and tested" is
         the ruling's own condition, and a test nothing compiles tests nothing.
    """
    if not re.search(r"\bruling\s+[A-Z]+\d+\b|\bR\d+\b", note):
        return "cites no ruling"
    paths = _CITED_PATH.findall(note)
    if not paths:
        return "names no file"
    missing = [p for p in paths if not exists(p)]
    if missing:
        return "names files that do not exist: %s" % ", ".join(missing)
    tests = [p for p in paths if p.startswith("tests/")]
    built = [p for p in tests if p[len("tests/"):] in cmake_text]
    if not built:
        return "names no test that tests/CMakeLists.txt builds"
    return None


def disconnected() -> dict:
    """Mandatory capabilities whose implementation is NOT in the console.

    The owner's standard: a mandatory function does not count as present if it
    is "a disconnected implementation". Closure membership is exactly that test,
    and it is the one that cannot be satisfied by a file existing on disk.
    """
    closure = console_closure()
    live = instantiated_in(closure_paths())   # the half the docstring promised
    board = board_addition()                  # the second root; see its docstring
    absent, unbuilt, unresolvable, deferred_ok, connected = [], [], [], [], []
    listed_not_live: list[str] = []
    uncited: list[str] = []
    via_board: list[str] = []
    superseded_ok: list[str] = []
    cmake = ROOT / "tests" / "CMakeLists.txt"
    cmake_text = cmake.read_text(encoding="utf-8", errors="replace") if cmake.exists() else ""
    for b in ledger_blocks():
        if b["superseded_by"]:
            why = superseded_verdict(b["superseded_by"], cmake_text)
            if why is None:
                superseded_ok.append(b["id"])
                continue
            # A supersession that does not check out is an excuse that does not
            # hold: counted with the uncited flags, AS A GAP, and named.
            uncited.append("%s (superseded_by: %s)" % (b["id"], why))
            continue
        # THE OWNER'S RULE: "Only explicitly deferred/non-v1 features may remain
        # absent, and each must CITE THE CONTROLLING RULING/SPEC."
        #
        # So a bare flag is not an excuse. On 2026-09-19 the ledger still carried
        # `deferred: true` on four blocks the owner had revoked the day before --
        # this register read that stale field and reported them as "not a gap",
        # under-counting by four. A flag with no citation beside it is exactly
        # how that happens, so an uncited flag is now counted AS A GAP.
        cite = (b["deferred_note"] or "") + (b["blocked_note"] or "")
        has_cite = bool(re.search(r"\b(ruling|SS\d|§\d|spec|charter|plan)\b", cite, re.I))
        if b["deferred"] == "true" or b["blocked_on"] == "hardware":
            if has_cite:
                deferred_ok.append(b["id"])
                continue
            uncited.append(b["id"])                # excused by nothing: a GAP
        mod = resolve_module(b["id"], b["implementation"])
        if mod is None and b["id"] in _ALIAS:
            unbuilt.append(b["id"])      # hand-searched and absent: a REAL gap
        elif mod is None:
            unresolvable.append(b["id"])
        elif mod in closure and mod in live:
            connected.append(b["id"])
        elif mod in board:
            # COMPOSED IN THE BOARD, not in the core. Named on every run rather
            # than folded silently into `connected`, because this is the only
            # reduction here that does not come from the core's own closure and
            # it must stay auditable at a glance.
            connected.append("%s (in %s)" % (b["id"], BOARD_ROOT))
            via_board.append(b["id"])
        elif successor_in(mod, closure, live):
            connected.append("%s (via %s)" % (b["id"], successor_in(mod, closure, live)))
        elif mod in closure:
            # in the source list but nothing instantiates it. NOT connected --
            # this is the shell's eight-struck-modules failure, and counting it
            # as present is how a filename becomes "progress".
            listed_not_live.append(b["id"])
            absent.append((b["id"], mod))
        elif list(RTL.rglob(mod + ".sv")) or mod in declared_on_disk():
            absent.append((b["id"], mod))          # built, NOT connected
        else:
            unbuilt.append(b["id"])                # no RTL at all
    return {"connected": connected, "built_not_connected": absent,
            "unbuilt": unbuilt, "unresolvable": unresolvable,
            "deferred_or_blocked": deferred_ok, "uncited_excuse": uncited,
            "listed_but_not_instantiated": listed_not_live,
            "connected_in_board": via_board,
            "superseded_by_ruling": superseded_ok}


def audit() -> dict:
    ties = tieoffs()
    closure = console_closure()

    # THE CROSS-CHECK, and it exists because the claim that it existed was made
    # before the code did. `console_closure()` and `closure_paths()` read the
    # same region of the same file by two separate copies of the same walk, and
    # they have now carried the identical defect twice -- the unbounded scan that
    # absorbed the following target, and the substring anchor that matched a
    # COMMENT mentioning the core 625 lines above the real target. The second one
    # collapsed the closure to a single module and moved the headline number from
    # 77 to 124 with nothing in the design changed.
    #
    # Two independent walks that must agree are worth far more than one walk
    # trusted, so their disagreement is now a HARD FAILURE rather than a sentence
    # in a commit message. It is checked here rather than in main() so that every
    # consumer of audit() gets it, including --json.
    stale = _stale_none_aliases()
    if stale:
        raise SystemExit(
            "completion_register: %d hand-resolved _ALIAS entry(ies) say a module "
            "is absent, but it now EXISTS in the tree:\n%s\n"
            "Point each alias at its module. A `None` that outlives its search "
            "under-counts progress and hides a block somebody already built."
            % (len(stale),
               "\n".join("  %-18s -> %s" % (c, m) for c, m in stale)))

    paths = {p.stem for p in closure_paths()}
    for p in closure_paths():
        paths |= declared_modules(p)      # both walks resolve MODULES, not filenames
    if paths != closure:
        only_c = sorted(closure - paths)
        only_p = sorted(paths - closure)
        raise SystemExit(
            "completion_register: the two closure walks DISAGREE, so neither "
            "number can be trusted.\n"
            "  console_closure() only: %s\n"
            "  closure_paths()  only: %s\n"
            "One of them is reading the wrong region of design/fit_targets.yml. "
            "Fix that before reading any gap count." % (only_c, only_p)
        )
    gaps = [t for t in ties if t["mandatory_gap"]]
    dis = disconnected()
    # UNRESOLVABLE COUNTS AS A GAP. It was excluded at first on the reasoning
    # that a name heuristic over-reports -- true, but the conclusion was wrong:
    # a capability whose implementation cannot even be located is not EXCUSED,
    # it is UNDEMONSTRATED, and the owner's standard is that a mandatory
    # function does not count as present unless it is really there. Excluding it
    # made the total smaller, which is the direction this file exists to resist.
    # It is reported as its own class so nobody mistakes it for a build task.
    total = (len(gaps) + len(dis["built_not_connected"])
             + len(dis["unbuilt"]) + len(dis["uncited_excuse"])
             + len(dis["unresolvable"]))
    # SUPERSEDED MODULES BLOCK COMPLETION, and until 2026-09-20 they did not.
    # `superseded_in_closure()` was computed in main(), printed inside a row of
    # exclamation marks, and then DROPPED: it was absent from `--json`, absent
    # from this dict, and absent from the exit code -- so the campaign could
    # reach "0 gaps, exit 0" with the old version of a block composed, and the
    # honest fit would then measure a machine nobody ships. That is the one
    # thing the owner put in capitals: "YOU ONLY GET TO FIT THE LATEST
    # VERSION." Found by review Q011, finding 7.
    #
    # It is NOT added to `mandatory_gaps`, because it is not a missing
    # function and folding it in would make the headline number mean two
    # things. It is its own list, and it gates the exit code beside the total.
    sup = superseded_in_closure()
    _root_hits = superseded_in_roots()
    return {
        "tieoffs_total": len(ties),
        "tieoff_gaps": len(gaps),
        "mandatory_gaps": total,
        "superseded_composed": [[mod, list(newer)] for mod, newer in sup],
        # The production fit's own root, reported beside the core's. FATAL since
        # 2026-09-20 (owner ruling R86); see superseded_in_prod_fit()'s docstring.
        "superseded_in_prod_fit": [
            [mod, list(newer)] for mod, newer in superseded_in_prod_fit(sup)],
        # EVERY production root, not just the two we happened to know about.
        # `superseded_roots_unfound` is not cosmetic: a root whose file cannot
        # be located is a root that was NOT CHECKED, and a silent skip there
        # reads exactly like a clean result.
        "superseded_in_roots": [[root, mod, list(newer)]
                                for root, mod, newer in _root_hits[0]],
        "superseded_roots_unfound": list(_root_hits[1]),
        "superseded_roots_checked": production_roots(),
        # ASKED, AND REPORTED WITHOUT BEING FATAL. An earlier draft listed these
        # as "not checked", which is a truthful label on a useless field: a list
        # of roots nobody asked is precisely the blind spot R86 is about. They
        # are asked; they do not gate, because most are leaf measurements and
        # retained oracles that exist BECAUSE they are the superseded version.
        "superseded_in_advisory_roots": [
            [root, mod, list(newer)]
            for root, mod, newer in superseded_in_roots(advisory_roots())[0]],
        "superseded_roots_advisory": advisory_roots(),
        "closure_modules": len(closure),
        "by_kind": {k: sum(1 for t in ties if t["kind"] == k)
                    for k in sorted({t["kind"] for t in ties})},
        "gaps": [{"id": t["id"], "kind": t["kind"],
                  "head": t["head"][:100]} for t in gaps],
        "capability": {k: len(v) for k, v in dis.items()},
        "listed_but_not_instantiated": dis["listed_but_not_instantiated"],
        # THE SECOND ROOT, reported by name on every run. The modules are what
        # `zhao_console_board` adds on top of the machine it contains; the
        # capabilities are the subset of those that are ledger blocks. If the
        # first list ever grows, the second is where to look and why.
        "board_addition_modules": sorted(board_addition()),
        "connected_in_board": dis["connected_in_board"],
        "built_not_connected": [m for _, m in dis["built_not_connected"]],
        "unbuilt": dis["unbuilt"],
        "unresolvable": dis["unresolvable"],
        "uncited_excuse": dis["uncited_excuse"],
        "superseded_by_ruling": dis["superseded_by_ruling"],
    }


def _self_test() -> None:
    """The register must be able to SEE a gap. Prove it on a synthetic header."""
    sample = (
        "// INCOMPLETE -- TIED OFF, AND WHY\n"
        "//  I1. SOMETHING (`a_i`) -- BOUNDARY. no owner exists.\n"
        "//      more prose about it\n"
        "//  I2. OTHER (`b_i`) -- TIED TO ZERO, a real interface mismatch.\n"
        "//  I3. THIRD (`c_i`) -- NOT a tie-off: the core assigns it.\n"
    )
    global CORE
    real = CORE
    try:
        tmp = ROOT / "tools" / "budget" / ".completion_selftest.sv"
        tmp.write_text(sample, encoding="utf-8")
        CORE = tmp
        ts = tieoffs()
        ids = [t["id"] for t in ts]
        kinds = {t["id"]: t["kind"] for t in ts}
        bad = []
        if ids != ["I1", "I2", "I3"]:
            bad.append("parsed %r, expected I1 I2 I3" % ids)
        if kinds.get("I1") != "boundary":
            bad.append("I1 kind %r" % kinds.get("I1"))
        if kinds.get("I2") != "tied-to-zero":
            bad.append("I2 kind %r" % kinds.get("I2"))
        if kinds.get("I3") != "resolved-in-composer":
            bad.append("I3 kind %r" % kinds.get("I3"))
        if sum(1 for t in ts if t["mandatory_gap"]) != 2:
            bad.append("expected 2 mandatory gaps, got %d"
                       % sum(1 for t in ts if t["mandatory_gap"]))
        # THE POSITIVE CONTROL for the truncation guard above. A detector that
        # has not been seen to FIRE has not been tested (CLAUDE.md), and this
        # one guards a failure that reads LOW -- so it is the one to prove.
        # A blank line between I1 and I2 must be a hard failure naming I2 and
        # I3, not a quiet register of one.
        truncated = (
            "// INCOMPLETE -- TIED OFF, AND WHY\n"
            "//  I1. SOMETHING (`a_i`) -- BOUNDARY. no owner exists.\n"
            "\n"
            "//  I2. OTHER (`b_i`) -- TIED TO ZERO, a real interface mismatch.\n"
            "//  I3. THIRD (`c_i`) -- NOT a tie-off: the core assigns it.\n"
        )
        tmp.write_text(truncated, encoding="utf-8")
        fired = ""
        try:
            tieoffs()
        except SystemExit as exc:
            fired = str(exc)
        if "WALK STOPPED EARLY" not in fired or "I2" not in fired or "I3" not in fired:
            bad.append("the truncation guard did not fire on a planted blank "
                       "line (got %r)" % fired[:120])

        # THE BLIND-BLOCK CONTROL (2026-09-20, review Q011 finding 6). The
        # truncation guard above compares what was parsed against what appears
        # LATER, so it is structurally blind to "nothing matched anywhere":
        # both lists are empty and the register returns a clean zero. The block
        # here is present and every head is unreadable.
        blind = (
            "// INCOMPLETE -- TIED OFF, AND WHY\n"
            "//  J1) SOMETHING (`a_i`) -- BOUNDARY. no owner exists.\n"
            "//  J2) OTHER (`b_i`) -- TIED TO ZERO.\n"
        )
        tmp.write_text(blind, encoding="utf-8")
        fired = ""
        try:
            tieoffs()
        except SystemExit as exc:
            fired = str(exc)
        if "NOT ONE entry matched" not in fired:
            bad.append("the blind-block guard did not fire on a block whose "
                       "entry heads all stopped matching (got %r)" % fired[:120])

        if bad:
            sys.stderr.write("completion_register SELF-TEST FAILED:\n")
            for b in bad:
                sys.stderr.write("  %s\n" % b)
            raise SystemExit(2)
    finally:
        CORE = real
        try:
            (ROOT / "tools" / "budget" / ".completion_selftest.sv").unlink()
        except OSError:
            pass
    _board_self_test()
    _superseded_self_test()
    _blind_root_self_test()


def _blind_root_self_test() -> None:
    """FIRE the three guards that stop a blind root reading as a finished
    console, plus the exit rule that superseded modules block completion.

    All four were added 2026-09-20 from review Q011, and all four guard the
    same shape: an input that VANISHES makes the total SMALLER, which is the
    direction nobody audits. Each is proved to fire here, because a guard whose
    alarm nobody has heard is an argument.
    """
    global BLOCKS, TARGETS
    real_blocks, real_targets = BLOCKS, TARGETS
    bad = []
    tmp = ROOT / "tools" / "budget" / ".completion_selftest.yml"
    try:
        # 1. A MISSING LEDGER must refuse, not return an empty capability list.
        BLOCKS = ROOT / "tools" / "budget" / ".no_such_blocks.yml"
        fired = ""
        try:
            ledger_blocks()
        except SystemExit as exc:
            fired = str(exc)
        if "blind instrument" not in fired:
            bad.append("a missing design/blocks.yml did not refuse (got %r)"
                       % fired[:120])
        BLOCKS = real_blocks

        # 2. A CORE TARGET WITH NO SOURCES must refuse. Before the fix this
        #    walked to end of file absorbing every later target's sources.
        tmp.write_text(
            "targets:\n"
            "  - top: zhao_console_core\n"
            "    sources:\n"
            "  - top: zhao_other\n"
            "    sources:\n"
            "      - fpga/rtl/common/zhao_pkg.sv\n",
            encoding="utf-8")
        TARGETS = tmp
        for fn, name in ((console_closure, "console_closure"),
                         (closure_paths, "closure_paths")):
            fired = ""
            try:
                fn()
            except SystemExit as exc:
                fired = str(exc)
            if "NO source line" not in fired and "no `- fpga/rtl/" not in fired:
                bad.append("%s did not refuse an empty core closure (got %r)"
                           % (name, fired[:120]))
    finally:
        BLOCKS, TARGETS = real_blocks, real_targets
        try:
            tmp.unlink()
        except OSError:
            pass

    # 3. ZERO GAPS IS NOT ENOUGH while a superseded module is composed. This is
    #    the one that could have ended the campaign early, so it is asserted
    #    both ways round rather than only in the failing direction.
    if _done({"mandatory_gaps": 0, "superseded_composed": []}) != 0:
        bad.append("_done refused a genuinely finished design")
    if _done({"mandatory_gaps": 0,
              "superseded_composed": [["zhao_old", ["zhao_old_v2"]]]}) != 1:
        bad.append("_done returned SUCCESS with a superseded module composed "
                   "-- the owner's one capitalised rule")
    if _done({"mandatory_gaps": 3, "superseded_composed": []}) != 1:
        bad.append("_done returned SUCCESS with gaps remaining")

    # 4. THE R86 ROOT SWEEP, FIRED IN EVERY DIRECTION IT CAN FAIL IN.
    #    A detector that has not been seen to fire has not been tested, and this
    #    one replaces a check that was deliberately non-fatal -- so its silence
    #    is exactly the claim that needs the hardest evidence.
    if _done({"mandatory_gaps": 0, "superseded_composed": [],
              "superseded_in_roots": [["zhao_root", "zhao_old",
                                       ["zhao_old_v2"]]]}) != 1:
        bad.append("_done returned SUCCESS with a superseded module wired in a "
                   "production root (R86)")
    if _done({"mandatory_gaps": 0, "superseded_composed": [],
              "superseded_in_prod_fit": [["zhao_old", ["zhao_old_v2"]]]}) != 1:
        bad.append("_done returned SUCCESS with a superseded module wired in "
                   "zhao_prod_top -- the defect R86 was written about")
    #    A root nobody could find is NOT a clean root. This is the half a
    #    `continue` would have swallowed.
    if _done({"mandatory_gaps": 0, "superseded_composed": [],
              "superseded_roots_unfound": ["zhao_vanished"]}) != 1:
        bad.append("_done returned SUCCESS with a production root that was "
                   "never checked at all")

    # 5. newer_versions() IS THE ONE PREDICATE ALL THREE CHECKS RESTITE ON.
    #    Both naming shapes, and the two near-misses that must NOT match: a
    #    module whose name merely starts with another's, and a lower version.
    shapes = {"zhao_field_len": ({"zhao_field_len", "zhao_field_v3_len"},
                                 ["zhao_field_v3_len"]),
              "zhao_shell_top": ({"zhao_shell_top", "zhao_shell_top_v2"},
                                 ["zhao_shell_top_v2"]),
              "zhao_terrain_bake": ({"zhao_terrain_bake",
                                     "zhao_terrain_bake_delta"}, []),
              "zhao_geom_binner_v2": ({"zhao_geom_binner_v2",
                                       "zhao_geom_binner"}, [])}
    for mod, (disk, want) in shapes.items():
        got = newer_versions(mod, disk)
        if got != want:
            bad.append("newer_versions(%s) = %s, expected %s"
                       % (mod, got, want))

    if bad:
        sys.stderr.write("completion_register BLIND-ROOT SELF-TEST FAILED:\n")
        for b in bad:
            sys.stderr.write("  %s\n" % b)
        raise SystemExit(2)


def _superseded_self_test() -> None:
    """superseded_verdict() excuses a gap, so it is FIRED on every note that
    must not excuse one before its silence is believed."""
    have = {"reference/include/zref/x.hpp", "tests/t/x_directed.cpp",
            "tests/t/unbuilt.cpp"}
    cm = "add_executable(test_x t/x_directed.cpp)\n"
    ex = lambda rel: rel in have
    good = "SW.STREAM, ruling T5: reference/include/zref/x.hpp, tests/t/x_directed.cpp"
    must_fail = {
        "no ruling":    "SW.STREAM: reference/include/zref/x.hpp, tests/t/x_directed.cpp",
        "no file":      "SW.STREAM, ruling T5",
        "missing file": good + ", reference/include/zref/gone.hpp",
        "no test":      "SW.STREAM, ruling T5: reference/include/zref/x.hpp",
        "unbuilt test": "SW.STREAM, ruling T5: reference/include/zref/x.hpp, tests/t/unbuilt.cpp",
    }
    bad = []
    if superseded_verdict(good, cm, ex) is not None:
        bad.append("a complete note was refused: %s" % superseded_verdict(good, cm, ex))
    for k, note in must_fail.items():
        if superseded_verdict(note, cm, ex) is None:
            bad.append("a note with %s EXCUSED its capability" % k)
    if bad:
        sys.stderr.write("completion_register SUPERSEDED SELF-TEST FAILED:\n")
        for b in bad:
            sys.stderr.write("  %s\n" % b)
        raise SystemExit(2)


def _board_self_test() -> None:
    """The second root must ADD exactly the board's own blocks, and must STOP.

    CLAUDE.md: a detector that has not been shown to fire has not been tested,
    and this one's failure mode is an amnesty rather than an error message. The
    dangerous direction is the board going on excusing SYS.PLL after it has
    stopped instantiating the core -- there is nothing to notice, the count just
    stays low. So the guard is driven on a graph where it must fire.
    """
    decl = {"zhao_console_board": "b.sv", "zhao_console_core": "c.sv",
            "zhao_sys_pll": "p.sv", "zhao_inner": "i.sv", "zhao_orphan": "o.sv"}
    joined = {"b.sv": {"zhao_console_core", "zhao_sys_pll"},
              "c.sv": {"zhao_inner"}}
    got = _board_addition_from(decl, joined)
    bad = []
    if got != {"zhao_console_board", "zhao_sys_pll"}:
        bad.append("joined board should add exactly board+pll, got %r" % (got,))
    # THE GUARD: the board no longer instantiates the core. The set must collapse
    # to EMPTY, not merely lose the core -- otherwise SYS.PLL keeps its amnesty
    # while the console is topless, which is the one outcome this must not have.
    detached = {"b.sv": {"zhao_sys_pll"}, "c.sv": {"zhao_inner"}}
    if _board_addition_from(decl, detached) != set():
        bad.append("a board that dropped the core still granted membership: %r"
                   % (_board_addition_from(decl, detached),))
    # And a board that does not exist grants nothing either.
    if _board_addition_from({"zhao_console_core": "c.sv"}, {}) != set():
        bad.append("a missing board granted membership")
    if bad:
        sys.stderr.write("completion_register BOARD SELF-TEST FAILED:\n")
        for b in bad:
            sys.stderr.write("  %s\n" % b)
        raise SystemExit(2)


def _done(rep: dict) -> int:
    """The exit code, in ONE place so the two returns cannot drift apart.

    Zero gaps is necessary and not sufficient: a superseded module in the
    closure blocks completion too, because fitting an old version measures a
    machine nobody ships.

    SINCE 2026-09-20 (owner ruling R86) that applies to EVERY production root,
    not only the console core's closure, and a root whose file could not be
    found is treated as a failure rather than as a pass. A check that skips what
    it cannot locate reports the same thing as a check that found nothing wrong.
    """
    return 0 if (rep["mandatory_gaps"] == 0
                 and not rep["superseded_composed"]
                 and not rep.get("superseded_in_prod_fit")
                 and not rep.get("superseded_in_roots")
                 and not rep.get("superseded_roots_unfound")) else 1


def main(argv: list[str]) -> int:
    _self_test()
    rep = audit()
    if "--json" in argv:
        print(json.dumps(rep, indent=2))
        return _done(rep)

    print("completion register -- computed from the tree, not maintained by hand")
    print("self-test PASSED: the parser sees a planted gap and classifies it\n")
    sup = [(mod, newer) for mod, newer in rep["superseded_composed"]]
    if sup:
        print("!" * 74)
        print("SUPERSEDED MODULES ARE COMPOSED -- %d of them." % len(sup))
        print("Owner ruling 2026-09-19: \"YOU ONLY GET TO FIT THE LATEST VERSION.")
        print("IF IT IS BROKEN YOU FIX IT.\"  Fitting an old version measures a")
        print("machine nobody ships and spends budget on dead weight.")
        for mod, newer in sup:
            print("   composed %-30s superseded by %s" % (mod, ", ".join(newer)))
        print("THIS ALONE MAKES THE EXIT CODE NONZERO, however many gaps remain.")
        print("!" * 74)
        print()

    pfs = [(mod, newer) for mod, newer in rep["superseded_in_prod_fit"]]
    if pfs:
        print("-" * 74)
        print("SUPERSEDED MODULES ARE WIRED IN THE PRODUCTION FIT's ROOT")
        print("(`zhao_prod_top`) -- %d of them.  The check above asks this of the"
              % len(pfs))
        print("CONSOLE CORE's closure; the pin-out top is a different root and")
        print("nothing was asking it.  A generated file carries a reassuring")
        print("provenance line while doing this.")
        for mod, newer in pfs:
            print("   wired    %-30s superseded by %s" % (mod, ", ".join(newer)))
        print("FATAL SINCE 2026-09-20, owner ruling R86.  The production fit's")
        print("root is what R80 measures; an old version there makes the area")
        print("number wrong in the direction nobody questions.")
        print("-" * 74)
        print()

    roots = [(root, mod, newer)
             for root, mod, newer in rep["superseded_in_roots"]]
    unfound = rep["superseded_roots_unfound"]
    if roots or unfound:
        print("!" * 74)
        print("SUPERSEDED MODULES ARE WIRED IN A PRODUCTION ROOT -- %d hit(s)"
              % len(roots))
        print("across %d roots checked (owner ruling R86: 'extend the superseded"
              % len(rep["superseded_roots_checked"]))
        print("check to EVERY root ... a blind spot the size of a top-level is")
        print("not a gap in coverage, it is a second machine nobody audits').")
        for root, mod, newer in roots:
            print("   %-24s wires %-28s superseded by %s"
                  % (root, mod, ", ".join(newer)))
        for root in unfound:
            print("   %-24s NOT CHECKED -- no file declares this module, so its"
                  % root)
            print("   %-24s silence is not evidence" % "")
        print("THIS ALONE MAKES THE EXIT CODE NONZERO, however many gaps remain.")
        print("!" * 74)
        print()
    else:
        print("superseded check: %d production roots CLEAN"
              % len(rep["superseded_roots_checked"]))

    adv = rep["superseded_in_advisory_roots"]
    print("    %d further fit-target tops asked, NOT fatal (leaf measurements"
          % len(rep["superseded_roots_advisory"]))
    print("    and retained oracles, kept BECAUSE they are the old version)"
          "  -- %d hit(s)" % len(adv))
    for root, mod, newer in adv:
        print("      note   %-26s wires %-26s superseded by %s"
              % (root, mod, ", ".join(newer)))
    print()

    print("zhao_console_core closure modules : %d" % rep["closure_modules"])
    if rep["board_addition_modules"]:
        print("%s adds %d more    : %s"
              % (BOARD_ROOT, len(rep["board_addition_modules"]),
                 ", ".join(rep["board_addition_modules"])))
        print("    capabilities that reach the machine only through the board: %s"
              % (", ".join(rep["connected_in_board"]) or "none"))
    else:
        print("!! %s adds NOTHING -- either it is missing or it no longer "
              "instantiates %s. Every block hanging off it now counts as a GAP, "
              "which is the safe direction and probably not the true one."
              % (BOARD_ROOT, CONSOLE_ROOT))
    print("tie-off entries in its header     : %d" % rep["tieoffs_total"])
    for k, n in sorted(rep["by_kind"].items()):
        print("    %-22s %d" % (k, n))
    c = rep["capability"]
    print("\nmandatory rtl capabilities (design/blocks.yml)")
    print("    connected in the console  : %d" % c["connected"])
    print("    BUILT BUT NOT CONNECTED   : %d" % c["built_not_connected"])
    print("    NOT BUILT AT ALL          : %d" % c["unbuilt"])
    print("    deferred / waits for board: %d   (cite the ruling, not a gap)"
          % c["deferred_or_blocked"])
    print("    EXCUSED BY AN UNCITED FLAG: %d   <- counted AS GAPS: the owner's"
          % c["uncited_excuse"])
    print("                                     rule is that a deferral must CITE")
    print("                                     its controlling ruling or spec")
    print("    UNRESOLVABLE              : %d   <- counted AS GAPS: not excused,"
          % c["unresolvable"])
    print("                                     UNDEMONSTRATED. Locate the module")
    print("                                     or add it to the alias table.")
    print("    superseded by a ruling    : %d   (cites the ruling, the replacement"
          % c["superseded_by_ruling"])
    print("                                     file and a BUILT test; each named)")
    # EVERY NAME IS PRINTED. These lists were sliced `[:20]` and on 2026-09-19
    # the count read 22 while 20 names were shown, so zhao_forge_cliff and
    # zhao_post_gather were counted and invisible -- a gap list that hides its
    # tail is the flattering direction, and nobody audits a list that looks
    # complete.
    if rep["superseded_by_ruling"]:
        print("\n  SUPERSEDED BY A RULING (not a gap; the note is checked against the tree):")
        for m in rep["superseded_by_ruling"]:
            print("    %s" % m)
    if rep["uncited_excuse"]:
        print("\n  EXCUSES THAT DO NOT HOLD (counted as gaps):")
        for m in rep["uncited_excuse"]:
            print("    %s" % m)
    if rep["built_not_connected"]:
        print("\n  BUILT BUT NOT CONNECTED (a disconnected implementation does not count):")
        for m in rep["built_not_connected"]:
            print("    %s" % m)
    if rep["unbuilt"]:
        print("\n  NOT BUILT AT ALL:")
        for m in rep["unbuilt"]:
            print("    %s" % m)

    print("\nMANDATORY GAPS REMAINING          : %d" % rep["mandatory_gaps"])
    print("  (%d tie-offs + %d disconnected + %d unbuilt + %d uncited + %d unresolvable)"
          % (rep["tieoff_gaps"], c["built_not_connected"], c["unbuilt"],
             c["uncited_excuse"], c["unresolvable"]))
    if rep["gaps"]:
        print()
        for g in rep["gaps"]:
            print("  %-4s %-20s %s" % (g["id"], g["kind"], g["head"]))
        print("\nA mandatory function is not present if it is a tie-off, fake")
        print("stimulus, an external placeholder for hardware storage, a")
        print("disconnected implementation, pruned dead logic, a stub, a TODO,")
        print("or an unimplemented contract. Drive this to ZERO.")
    elif sup:
        print("\nZero mandatory gaps -- AND THE DESIGN IS NOT READY TO FREEZE.")
        print("%d superseded module(s) are composed (listed at the top). Fitting"
              % len(sup))
        print("an old version measures a machine nobody ships, so this run still")
        print("exits nonzero. Replace them, then read this line again.")
    else:
        print("\nZERO mandatory gaps and no superseded module composed.")
        print("Freeze this design and measure it.")
    return _done(rep)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
