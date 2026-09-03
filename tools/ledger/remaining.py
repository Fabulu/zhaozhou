#!/usr/bin/env python3
"""What is left to finish the console, derived rather than audited by hand.

reports/CONSOLE_REMAINING.md was written by hand on 2026-08-31 and its own
opening note says the first pass was WRONG -- it listed seven blocks as ready
to build on the strength of their ledger entries, and reading the contracts
changed that to zero. Ten blocks have been built since, so the hand list is
stale in the other direction too.

A list that goes stale in three days and was wrong on its first pass should not
be maintained by hand. This derives it from four sources that cannot quietly
disagree:

  * design/blocks.yml      -- the ledger's own maturity
  * fpga/rtl/**            -- whether RTL actually exists
  * design/prod_manifest.yml -- whether it is in the production machine
  * design/contracts/**    -- whether there is a law to build against

The interesting output is not the counts. It is the DISAGREEMENTS: a block the
ledger calls SPECIFIED whose RTL exists, or one it calls verified with no RTL
at all. Each is either a stale field or a phantom, and both have cost real time
in this repository.
"""
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "quartus"))


def load_ledger(path="design/blocks.yml"):
    s = io.open(path, encoding="utf-8").read()
    out = []
    for m in re.finditer(r"^  - id: (\S+)$", s, re.M):
        i = m.start()
        j = s.find("\n  - id: ", i + 1)
        seg = s[i:j if j > 0 else len(s)]

        def field(k):
            mm = re.search(r"^    " + k + r":\s*(.*)$", seg, re.M)
            return mm.group(1).strip() if mm else None

        out.append({
            "id": m.group(1),
            "kind": field("kind"),
            "subsystem": field("subsystem"),
            "maturity": field("maturity"),
            "contract": field("contract"),
            "deferred": field("deferred"),
            "blocked_on": field("blocked_on"),
            "superseded_by": field("superseded_by"),
        })
    return out


def rtl_modules(rtl_dir="fpga/rtl"):
    mods = {}
    for root, _d, names in os.walk(rtl_dir):
        for n in names:
            if not n.endswith(".sv"):
                continue
            p = os.path.join(root, n).replace(os.sep, "/")
            for m in re.finditer(r"^\s*module\s+(\w+)", io.open(p, encoding="utf-8",
                                                                errors="replace").read(), re.M):
                mods[m.group(1)] = p
    return mods


def candidates(block_id):
    """Module names a block id could plausibly own.

    The ledger has no `rtl:` field, so the link is by convention:
    TERRAIN.MIPGEN -> zhao_terrain_mipgen. Several ids do not follow it, which
    is why a MISS is reported rather than assumed to mean "not built".
    """
    parts = block_id.lower().split(".")
    joined = "_".join(parts)
    cands = ["zhao_" + joined]
    if len(parts) > 1:
        cands.append("zhao_" + parts[0] + "_" + parts[-1])
        cands.append("zhao_" + "_".join(parts[:2]))
    # Some ids carry a subsystem word the module name does not repeat --
    # MEM.HPS.ARBITER is zhao_hps_arbiter, not zhao_mem_hps_arbiter. Without
    # this, three verified memory blocks read as MISSING HARDWARE, which is the
    # most expensive kind of wrong this report can be.
    if len(parts) > 1:
        cands.append("zhao_" + "_".join(parts[1:]))
        cands.append("zhao_" + parts[1])
    # the rebuilt implementations and the wrapped ones carry a suffix
    for base in list(cands):
        for suffix in ("_v2", "_pipe", "_svc", "_ctrl", "_cache", "_decode"):
            cands.append(base + suffix)
    return cands


def main():
    ledger = load_ledger()
    mods = rtl_modules()
    sys.path.insert(0, os.path.join(HERE, "..", "quartus"))
    from check_prod_manifest import read_manifest, module_edges, closure
    tops, excluded = read_manifest()
    _decl, edges = module_edges()
    in_machine = set(tops)
    for t in tops:
        in_machine |= closure(edges, t)

    rows = []
    for b in ledger:
        hit = None
        for c in candidates(b["id"]):
            if c in mods:
                hit = c
                break
        contract_ok = bool(b["contract"]) and b["contract"] != "null" and \
            os.path.exists(b["contract"])
        rows.append(dict(b, module=hit, in_machine=(hit in in_machine) if hit else False,
                         contract_ok=contract_ok))

    VERIFIED = ("UNIT_VERIFIED", "RTL_VERIFIED")
    hw = [r for r in rows if r["kind"] == "rtl"]

    print("=" * 74)
    print("WHAT IS LEFT -- derived %s" % ("from ledger + RTL + manifest + contracts"))
    print("=" * 74)
    print("ledger: %d blocks (%d rtl, %d software, %d profile)"
          % (len(rows), len(hw),
             sum(1 for r in rows if r["kind"] == "software"),
             sum(1 for r in rows if r["kind"] == "profile")))

    no_rtl = [r for r in hw if not r["module"]]
    print()
    print("-- RTL BLOCKS WITH NO MODULE FOUND (%d) --" % len(no_rtl))
    print("   the missing hardware, plus any id that breaks the naming convention")
    for r in sorted(no_rtl, key=lambda r: (r["subsystem"] or "", r["id"])):
        flags = []
        if r["blocked_on"] and r["blocked_on"] != "null":
            flags.append("blocked_on:" + r["blocked_on"])
        if not r["contract_ok"]:
            flags.append("NO CONTRACT")
        print("   %-22s %-20s %-12s %s"
              % (r["id"], r["maturity"], r["subsystem"] or "-", " ".join(flags)))

    stale = [r for r in hw if r["module"] and r["maturity"] == "SPECIFIED"]
    print()
    print("-- LEDGER SAYS SPECIFIED BUT RTL EXISTS (%d) --" % len(stale))
    print("   either the field is stale or the module is somebody else's")
    for r in sorted(stale, key=lambda r: r["id"]):
        print("   %-22s %-34s %s"
              % (r["id"], r["module"], "in machine" if r["in_machine"] else "NOT in manifest"))

    ghost = [r for r in hw if r["maturity"] in VERIFIED and not r["module"]]
    print()
    print("-- LEDGER SAYS VERIFIED BUT NO RTL FOUND (%d) --" % len(ghost))
    for r in sorted(ghost, key=lambda r: r["id"]):
        print("   %-22s %s" % (r["id"], r["maturity"]))

    orphan = [r for r in hw if r["module"] and not r["in_machine"]
              and r["maturity"] in VERIFIED]
    print()
    print("-- BUILT AND VERIFIED BUT NOT IN THE PRODUCTION MACHINE (%d) --" % len(orphan))
    print("   verified hardware nothing will instantiate: an unfinished seam")
    for r in sorted(orphan, key=lambda r: r["id"]):
        print("   %-22s %-34s %s" % (r["id"], r["module"], r["maturity"]))

    print()
    print("-- SUMMARY --")
    print("   rtl blocks with RTL:            %d" % sum(1 for r in hw if r["module"]))
    print("   rtl blocks without RTL:         %d" % len(no_rtl))
    print("   of those, blocked on hardware:  %d"
          % sum(1 for r in no_rtl if r["blocked_on"] == "hardware"))
    print("   of those, with no contract:     %d"
          % sum(1 for r in no_rtl if not r["contract_ok"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
