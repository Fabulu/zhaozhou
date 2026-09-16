#!/usr/bin/env python3
"""check_counters.py -- does the ledger's `counters:` list name real RTL ports?

WHY THIS EXISTS (D19c)
----------------------
`design/blocks.yml` declares a `counters:` list per block and `spec/counters.md`
governs their behaviour. V12 checks each name is in `counter_catalog`. Nothing
checked that the RTL implements one, so a row like

    counters: [meshlets_fetched, triangles_culled]

on a block whose ports are `meshlets_considered_o` and `descriptors_fetched_o`
is documentation that reads like a claim.

THE RULING BEHIND IT
--------------------
Two conventions were possible: rename every port to `<counter>_o` (mechanical,
60+ blocks, and a port rename sweep during an active fit campaign is how you
discover a `PINMISSING` at hour three), or let a row declare its own mapping.
**The mapping wins**, because it is checkable today, touches no RTL, and does
not foreclose the rename later -- a block whose ports already follow the
default needs no mapping at all, so adopting the rename simply deletes entries.

So resolution is, in order:

  1. an explicit `counter_ports:` entry on the block, `name: port_o`
  2. the default `<name>_o`
  3. otherwise UNRESOLVED -- reported, not tolerated silently

WHAT IT DOES NOT DO
-------------------
It does not check that the counter COUNTS the right thing, or that anything
reads it. `spec/counters.md` governs the first and `check_port_coverage.py`
speaks to the second. A port existing is the weakest of the three claims and
the only one that can be checked from names alone -- which is exactly why the
earlier attempt to answer this question from names alone reported a headline
that was wrong. See the note at the bottom of the output.
"""
from __future__ import annotations

import io
import os
import re
import sys

PORT_RE = re.compile(r"^\s*output\s+(?:var\s+)?(?:[\w:]+\s+)*?(?:\[[^\]]*\]\s*)*(\w+)\s*(?:\[[^\]]*\]\s*)*(?:,|\)|;)?\s*(?://.*)?$")


# ANCHOR EVERY PATH TO THE REPOSITORY, not to the caller's cwd.
#
# `modules()` used to walk the bare string "fpga/rtl" and `read()` used to open
# whatever it was handed, so this tool only worked when it happened to be run
# from the repository root. Imported by a test that ctest runs from the build
# directory, it raised
#
#     FileNotFoundError: 'fpga/rtl\\texture\\zhao_texture_aux_pipe_v2.sv'
#
# on two of fifty-one cases -- and that failure was invisible for as long as the
# lane was also exceeding its CTest timeout, because a killed test reports
# "***Timeout" and never gets to say why. Raising the budget is what surfaced
# it.
#
# tests/tools/test_texjoin_accounting.py already carried a chdir(REPO) helper
# for SOME of its calls into these tools, which is the shape of a workaround
# that has to be remembered at every call site. Anchoring here fixes it for all
# of them.
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def read(p):
    if not os.path.isabs(p):
        p = os.path.join(REPO, p)
    return io.open(p, encoding="utf-8", errors="replace").read()


def modules():
    out = {}
    for root, _d, files in os.walk(os.path.join(REPO, "fpga", "rtl")):
        for f in files:
            if f.endswith(".sv"):
                out[f[:-3]] = os.path.join(root, f)
    return out


# Every line that declares an output, however it is written. If PORT_RE matches
# fewer lines than this does, the parser is DROPPING ports -- which is exactly
# how this tool reported "0 counters match their port" three separate times.
OUTPUT_LINE_RE = re.compile(r"^\s*output\s")

# The self-check has to be self-checked. Its first version was written with a
# `\b` that a shell heredoc turned into a literal BACKSPACE (0x08), so the
# pattern demanded a backspace after "output", matched nothing, and cheerfully
# printed "no silent drops" while the parser was dropping 17 ports. A check
# that can never fire is worse than no check, because it reassures. This
# asserts at import that the pattern still matches an ordinary declaration.
assert OUTPUT_LINE_RE.match("    output var logic [31:0] x_o,"), "dead self-check"

UNPARSED = []

# Block IDs usually map mechanically to zhao_<id>, but selected versioned
# implementations need an explicit checker-side exception until the ledger
# schema grows a first-class implementation-module field.
BLOCK_MODULE_OVERRIDES = {
    "TEXTURE.AUX": "zhao_texture_aux_pipe_v2",
    "TEXTURE.COMBINE": "zhao_texture_material_combine_v3",
}


def module_for_block(block_id):
    return BLOCK_MODULE_OVERRIDES.get(
        block_id, "zhao_" + block_id.lower().replace(".", "_")
    )


def outputs_of(path):
    """Every output port of the FIRST module in the file.

    Also records any line that declares an output but that PORT_RE could not
    read. Three bugs -- width brackets `[31:0]`, scoped types `pkg::type_t`,
    and the final port carrying no trailing comma -- each silently dropped
    ports and each made this tool report a SMALLER, more alarming number. A
    parser that drops what it cannot match will always report progress, so it
    has to say out loud how much it dropped."""
    names, started = [], False
    for line in read(path).splitlines():
        if not started:
            if re.match(r"^\s*module\s+\w+", line):
                started = True
            continue
        m = PORT_RE.match(line)
        if m:
            names.append(m.group(1))
        elif OUTPUT_LINE_RE.match(line):
            UNPARSED.append((path, line.strip()))
        if re.match(r"^\s*\);\s*$", line):
            break
    return names


COUNTER_SHAPE = re.compile(r"^\s*output\s+(?:var\s+)?(?:[\w:]+\s+)*?\[31:0\]\s*(\w+)")


def counter_shaped_ports(path):
    """Output ports declared `[31:0]` -- the shape a counter has here.

    Suggestion material only. spec/counters.md 3 makes a counter a 32-bit local
    register presented on request, so a 32-bit output is a CANDIDATE and nothing
    more: `dma_bytes_consumed_o` is 32 bits and is a payload, not a counter.
    Offered so the remaining rows can be worked by eye instead of by grep.
    """
    return [m.group(1) for m in COUNTER_SHAPE.finditer(read(path))]


def _counter_list(chunk):
    lines = chunk.splitlines()
    for index, line in enumerate(lines):
        if not line.startswith("    counters:"):
            continue
        payload = line.split(":", 1)[1].strip()
        if not payload:
            continuation = []
            for row in lines[index + 1:]:
                if not row.startswith("      "):
                    break
                continuation.append(row.strip())
                if "]" in row:
                    break
            payload = " ".join(continuation)
        match = re.fullmatch(r"\[(.*)\]", payload)
        if match is None:
            raise ValueError("counter list is not a closed inline/bracket list")
        return [name.strip() for name in match.group(1).split(",")
                if name.strip()]
    return None


def _counter_mapping(chunk):
    lines = chunk.splitlines()
    for index, line in enumerate(lines):
        if line != "    counter_ports:":
            continue
        mapping = {}
        for row in lines[index + 1:]:
            if not row.startswith("      "):
                break
            item = row.strip()
            if ":" not in item:
                raise ValueError("malformed counter_ports row: " + item)
            name, port = item.split(":", 1)
            if name.strip() in mapping:
                raise ValueError("duplicate counter_ports row: " + name.strip())
            mapping[name.strip()] = port.strip()
        return mapping
    return {}


def blocks(text=None):
    s = read("design/blocks.yml") if text is None else text
    out = []
    for chunk in re.split(r"\n  - id: ", s)[1:]:
        bid = chunk.split("\n", 1)[0].strip()
        names = _counter_list(chunk)
        if names is None:
            continue
        out.append((bid, names, _counter_mapping(chunk)))
    return out


def _self_test():
    sample = (
        "schema_version: 1\nblocks:\n"
        "  - id: TEXTURE.AUX\n"
        "    counters:\n"
        "      [aux_jobs_accepted, aux_jobs_completed]\n"
        "    counter_ports:\n"
        "      aux_jobs_accepted: accepted_o\n"
        "      aux_jobs_completed: completed_o\n"
        "  - id: SIMPLE.BLOCK\n"
        "    counters: [simple_count]\n"
    )
    parsed = blocks(sample)
    expected = [
        ("TEXTURE.AUX", ["aux_jobs_accepted", "aux_jobs_completed"],
         {"aux_jobs_accepted": "accepted_o",
          "aux_jobs_completed": "completed_o"}),
        ("SIMPLE.BLOCK", ["simple_count"], {}),
    ]
    if parsed != expected:
        raise AssertionError("counter parser dropped multiline/inline fixture")
    if module_for_block("TEXTURE.AUX") != "zhao_texture_aux_pipe_v2":
        raise AssertionError("AUX selected-module override is not live")
    if module_for_block("TEXTURE.COMBINE") != "zhao_texture_material_combine_v3":
        raise AssertionError("COMBINE selected-module override is not live")
    if module_for_block("SIMPLE.BLOCK") != "zhao_simple_block":
        raise AssertionError("default block-to-module mapping changed")
    return True


if not _self_test():
    raise AssertionError("check_counters self-test failed")


def resolve_block(bid, names, mapping, mods):
    """Resolve one ledger block through the same path used by ``main``."""
    mod = module_for_block(bid)
    if mod not in mods:
        return [], [], [], (bid, len(names))
    ports = set(outputs_of(mods[mod]))
    cand = counter_shaped_ports(mods[mod])
    has_snap = "zhao_counter_snap_t" in read(mods[mod])
    by_default, by_mapping, unresolved = [], [], []
    for name in names:
        if name in mapping:
            if mapping[name] in ports:
                by_mapping.append((bid, name, mapping[name]))
            else:
                unresolved.append((
                    bid, name,
                    "mapped to %s, which is not a port" % mapping[name],
                    has_snap,
                ))
        elif name + "_o" in ports:
            by_default.append((bid, name))
        else:
            why = "no %s_o and no mapping" % name
            if cand and "--suggest" in sys.argv:
                why += "   candidates: " + " ".join(sorted(set(cand))[:6])
            unresolved.append((bid, name, why, has_snap))
    return by_default, by_mapping, unresolved, None


def main() -> int:
    mods = modules()
    by_default, by_mapping, unresolved, no_module = [], [], [], []

    for bid, names, mapping in blocks():
        defaults, mapped, missing, absent = resolve_block(
            bid, names, mapping, mods
        )
        by_default.extend(defaults)
        by_mapping.extend(mapped)
        unresolved.extend(missing)
        if absent is not None:
            no_module.append(absent)

    total = len(by_default) + len(by_mapping) + len(unresolved)
    print("counters: %d declared on blocks with a module; %d resolve by the "
          "default <name>_o, %d by an explicit mapping, %d UNRESOLVED. "
          "(%d block(s) have counters but no module file yet.)"
          % (total, len(by_default), len(by_mapping), len(unresolved), len(no_module)))

    snapped = [u for u in unresolved if u[3]]
    bare = [u for u in unresolved if not u[3]]

    if snapped:
        print("\nUNRESOLVED, BUT THE BLOCK HAS A SNAP CHANNEL (%d) -- the D9 "
              "form (spec/counters.md 3). The counter is owned locally and "
              "presented as a zhao_counter_snap_t, so there is no <counter>_o "
              "port and there is NOT MEANT TO BE ONE. These want a "
              "`counter_ports:` entry, not an implementation:" % len(snapped))
        cur = None
        for bid, n, why, _ in snapped:
            if bid != cur:
                print("  %s" % bid)
                cur = bid
            print("      %-28s %s" % (n, why))

    if bare:
        print("\nUNRESOLVED, AND NO SNAP CHANNEL EITHER (%d) -- the block has "
              "neither a <counter>_o port nor a zhao_counter_snap_t, so this "
              "counter has no visible presentation path at all. THIS is the "
              "list that is about missing work:" % len(bare))
        cur = None
        for bid, n, why, _ in bare:
            if bid != cur:
                print("  %s" % bid)
                cur = bid
            print("      %-28s %s" % (n, why))

    if by_mapping and "--quiet" not in sys.argv:
        print("\nRESOLVED BY EXPLICIT MAPPING (%d):" % len(by_mapping))
        for bid, n, p in by_mapping:
            print("  %-22s %-28s -> %s" % (bid, n, p))

    if UNPARSED:
        print("\nPARSER DROPPED %d OUTPUT DECLARATION(S) it could not read. "
              "Every one is a port this tool is BLIND to, so every number above "
              "is a LOWER BOUND. Fix the pattern before believing them:"
              % len(UNPARSED))
        for path, line in UNPARSED[:12]:
            print("  %-30s %s" % (os.path.basename(path), line[:64]))
        if len(UNPARSED) > 12:
            print("  ... and %d more" % (len(UNPARSED) - 12))
    else:
        print("\nparser read every `output` line it met -- no silent drops.")

    print("\nNOTE: this checks that a PORT EXISTS. It does not check that the "
          "counter counts the right thing (spec/counters.md) or that anything "
          "reads it (check_port_coverage.py). It REPORTS; it does not gate.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
