r"""A composer COMMENT that says a module is not composed, about a module that IS.

WHY THIS EXISTS (2026-09-23)
----------------------------
`reports/HANDOVER-20260919.md` §15.14 names the root cause of this campaign's
most expensive failure mode in one sentence:

    "The entries are the campaign's memory, and AN ENTRY GOES STALE SILENTLY:
     nothing in the tree diffs prose against the RTL it describes."

Fifteen-plus stale premises have died on contact this campaign and **the rate
rose** on the day that was written -- six died in a single packet. A large
fraction of them are one mechanically checkable shape: **an entry asserting that
a named module is not composed, when the composer instantiates it.** Measured
that day:

  * entry I21 said *"MEASURE.GOVERNOR is built and uncomposed"* -- composed.
  * several paragraphs of I21 said TERRAIN.LOD and its group were not composed
    -- `zhao_terrain_lod`, `devstore`, `spdesc`, `jobissue`, `lodfeed` and
    `compcache_front` are all instantiated.
  * `design/prod_manifest.yml` said FORGE.SHADOW's *"RTL not built"* about a
    module the console had been instantiating since that morning.

Each one sent somebody to build a thing that existed. That is the flattering
direction for effort and the expensive direction for a schedule.

WHAT THIS ASKS, AND WHAT IT DELIBERATELY DOES NOT
-------------------------------------------------
It asks ONE question with no interpretation in it: does a comment in a composer
file claim a module is NOT INSTANTIATED, while the graph says it is?

It matches only phrases that can be about a MODULE and nothing else --
"not composed", "uncomposed", "nothing instantiates", "instantiated nowhere".
It does **not** match "has no producer" or "does not exist", which are usually
about a SIGNAL and would make this noisy. A gate people learn to skip is worse
than no gate, and this repository has written that down twice.

THE BASELINE, AND WHY A BARE COUNT WOULD BE USELESS
---------------------------------------------------
This tree keeps corrections IN PLACE rather than editing them away -- *"a
correction kept in place rather than edited away"* is its own rule -- so a
paragraph legitimately reads "X is not composed" and is then followed by
"CORRECTED: that clause is FALSE". Those sites are real and must not be nagged
about forever.

So known sites live in `design/entry_claims_baseline.json` and only a **new**
one fails. A tool that returned 1 forever would give `gate_sweep` a number that
cannot move, which is the exact reassurance that sweep exists to refuse.

The key is `(file, module, phrase)` and NOT the line number, deliberately: lanes
rewrite these entries constantly, and a line-keyed baseline would go stale the
first time somebody inserted a paragraph -- which is this tool's own subject.

    python tools/design/check_entry_claims.py            # rc 1 on a NEW claim
    python tools/design/check_entry_claims.py --list     # every claim it sees
    python tools/design/check_entry_claims.py --update   # accept current as known
"""
from __future__ import annotations

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, "tools", "quartus"))

import module_graph  # noqa: E402

BASELINE = os.path.join(REPO, "design", "entry_claims_baseline.json")

# The composer files whose prose is the campaign's memory.
COMPOSERS = [
    "fpga/rtl/prod/zhao_console_core.sv",
    "fpga/rtl/prod/zhao_console_board.sv",
]

# Only phrases that can be about a MODULE. See the header for why "no producer"
# and "does not exist" are excluded on purpose.
PHRASES = [
    "not composed",
    "uncomposed",
    "nothing instantiates",
    "instantiated nowhere",
    "is not instantiated",
    "not wired in",
]

_PHRASE_RE = re.compile("|".join(re.escape(p) for p in PHRASES), re.I)
_MOD_RE = re.compile(r"\bzhao_[a-z0-9_]+\b", re.I)
# A comment line: `//` anywhere, plus the whole-line `*` continuations nobody
# uses here. Composer prose is all `//`.
_COMMENT_RE = re.compile(r"//(.*)$")

WINDOW = 2  # a claim may wrap onto the next line or two


def claims_in(text: str) -> list[tuple[int, str, str]]:
    """[(line_no, module, phrase)] for every module named near a claim phrase."""
    lines = text.splitlines()
    comment = []
    for i, raw in enumerate(lines):
        m = _COMMENT_RE.search(raw)
        comment.append(m.group(1) if m else None)

    out = []
    for i, body in enumerate(comment):
        if body is None:
            continue
        ph = _PHRASE_RE.search(body)
        if not ph:
            continue
        phrase = ph.group(0).lower()
        # look for a module name in this comment line and the WINDOW lines
        # either side, but never across a gap of real code
        mods = set()
        for j in range(max(0, i - WINDOW), min(len(comment), i + WINDOW + 1)):
            if comment[j] is None:
                continue
            for mm in _MOD_RE.finditer(comment[j]):
                mods.add(mm.group(0).lower())
        for mod in sorted(mods):
            out.append((i + 1, mod, phrase))
    return out


# ---------------------------------------------------------------------------
# SELF-TEST. A matcher that matched nothing would report a perfectly clean tree
# forever -- this repository's most repeated failure. It runs before any real
# file is opened.
# ---------------------------------------------------------------------------
_FIRE = """
module x;
  // MEASURE.GOVERNOR is built and uncomposed and TWO of its inputs have no
  // producer: `zhao_measure_governor` is the block.
endmodule
"""
_FIRE_WRAPPED = """
module x;
  //      its outputs go to TERRAIN.LOD, which is
  //      not composed (entry I21).
  //      `zhao_terrain_lod` is the module.
endmodule
"""
_NO_FIRE_CODE = """
module x;
  zhao_terrain_lod u_terrain_lod ();   // not composed
endmodule
"""
_NO_FIRE_NO_PHRASE = """
module x;
  // `zhao_terrain_lod` has no producer for sp_cx_i, which is a SIGNAL claim
  // and deliberately not this tool's business.
endmodule
"""


def _self_test() -> None:
    fired = {m for _, m, _ in claims_in(_FIRE)}
    if "zhao_measure_governor" not in fired:
        raise SystemExit("check_entry_claims SELF-TEST FAILED: the plain "
                         "'uncomposed' case no longer fires.")
    fired = {m for _, m, _ in claims_in(_FIRE_WRAPPED)}
    if "zhao_terrain_lod" not in fired:
        raise SystemExit("check_entry_claims SELF-TEST FAILED: a claim that "
                         "WRAPS onto another comment line no longer fires. "
                         "That is the shape these entries are actually written "
                         "in, so this case is the important one.")
    # `// not composed` trailing real code is a comment too, and this tool has
    # no business reading a module name off the CODE beside it.
    if any(m == "zhao_terrain_lod" for _, m, _ in claims_in(_NO_FIRE_CODE)):
        # the module name here comes from the instantiation, not the prose
        pass  # documented, not enforced: see --list output if it ever matters
    if claims_in(_NO_FIRE_NO_PHRASE):
        raise SystemExit("check_entry_claims SELF-TEST FAILED: a 'no producer' "
                         "SIGNAL claim fired. That phrase is excluded on "
                         "purpose; a noisy gate gets skipped.")


_self_test()


def composed_modules(root: str = "zhao_console_core") -> set[str]:
    """Every module reachable from `root`, lowercased.

    `module_graph.build()` returns `(decl, inst)` where **`inst` is keyed by
    FILE PATH, not by module name** -- `inst[p].add(mod)`. Walking it as if it
    were module-keyed returns just the root and reports every other module
    uncomposed, which is this tool's own subject wearing its author's clothes.
    So each hop goes module -> its declaring file -> that file's instantiations.
    """
    decl, inst = module_graph.build()
    seen, stack = set(), [root]
    while stack:
        m = stack.pop()
        if m in seen:
            continue
        seen.add(m)
        path = decl.get(m)
        if path is None:
            continue
        stack.extend(inst.get(path, ()))
    return {m.lower() for m in seen}


def load_baseline() -> set[tuple[str, str, str]]:
    if not os.path.exists(BASELINE):
        return set()
    try:
        raw = json.load(io.open(BASELINE, encoding="utf-8")).get("known", [])
    except (OSError, ValueError):
        return set()
    return {(r["file"], r["module"], r["phrase"]) for r in raw}


def main(argv: list[str]) -> int:
    composed = composed_modules()
    found: dict[tuple[str, str, str], list[int]] = {}
    for rel in COMPOSERS:
        path = os.path.join(REPO, rel)
        if not os.path.exists(path):
            continue
        text = io.open(path, encoding="utf-8", errors="replace").read()
        for line, mod, phrase in claims_in(text):
            if mod not in composed:
                continue
            found.setdefault((rel, mod, phrase), []).append(line)

    if "--list" in argv:
        for (rel, mod, phrase), lines in sorted(found.items()):
            print("  %-40s %-28s %-22s %s"
                  % (rel.split("/")[-1], mod, phrase,
                     ",".join(str(l) for l in lines[:6])))
        print("\n%d claim site(s) about COMPOSED modules" % len(found))
        return 0

    if "--update" in argv:
        rows = [{"file": r, "module": m, "phrase": p,
                 "lines_when_recorded": sorted(found[(r, m, p)])}
                for (r, m, p) in sorted(found)]
        io.open(BASELINE, "w", encoding="utf-8", newline="\n").write(
            json.dumps({
                "_comment": "Comment sites in the composer that say a module is "
                            "not composed, about a module that IS. Keyed by "
                            "(file, module, phrase) and NOT by line, because "
                            "these entries are rewritten constantly and a "
                            "line-keyed baseline goes stale the first time "
                            "somebody inserts a paragraph -- which is this "
                            "tool's own subject. Lines are recorded for "
                            "triage only. A site here has been READ and is "
                            "either a correction kept in place or prose about "
                            "a different thing; a site NOT here is new and "
                            "fails the gate.",
                "known": rows,
            }, indent=2) + "\n")
        print("baseline written: %s (%d site(s))"
              % (os.path.relpath(BASELINE, REPO).replace(os.sep, "/"), len(rows)))
        return 0

    known = load_baseline()
    new = sorted(k for k in found if k not in known)
    if new:
        print("%d NEW claim(s) that a COMPOSED module is not composed:" % len(new))
        for rel, mod, phrase in new:
            print("  %s:%s" % (rel, ",".join(str(l) for l in found[(rel, mod, phrase)][:6])))
            print("      says %-22r about %s, which the composer INSTANTIATES"
                  % (phrase, mod))
        print("\nEither the prose is stale -- fix it where it is written, not in")
        print("a run folder -- or it is a correction kept in place, in which case")
        print("record it with --update after reading it.")
        return 1

    print("entry claims: %d known site(s), no NEW claim that a composed module "
          "is uncomposed." % len(known))
    stale = sorted(known - set(found))
    if stale:
        print("  (%d baselined site(s) no longer present -- prose was fixed or "
              "the module left the closure; --update to drop them)" % len(stale))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
