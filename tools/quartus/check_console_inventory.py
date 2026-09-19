#!/usr/bin/env python3
"""THE CONSOLE INVENTORY GATE -- what is allowed to be in the machine.

Owner, 2026-09-19: *"we need a big inventory about what actually gets to be in
the console and a hard gate to exclude the stuff that doesn't go in. This is
ridiculous."*

Said after the console was found fitting the ENTIRE v1 FIELD datapath while all
fourteen `zhao_field_v3_*` modules sat outside it, and after a dead v1 raster
island -- `zhao_raster_tile_pipe`, `zhao_geom_binner`, `zhao_geom_bin_pipe`,
`zhao_raster_blend`, `zhao_raster_quant`, `zhao_raster_rcp24_svc` -- was found
sitting in the console's fit closure instantiating ONLY EACH OTHER, reachable
from nothing.

WHY NOTHING CAUGHT EITHER
-------------------------
`design/prod_manifest.yml` and `check_prod_manifest.py` are exhaustive and
strict -- about `zhao_prod_top`, the DISCONNECTED RESOURCE CENSUS. They say
nothing about `zhao_console_core`, the actual machine. So the question "what
belongs in the console?" had no owner and no answer, and two different kinds of
waste accumulated in the one measurement that decides whether this thing ships.

`completion_register.py` asks a different question again -- is every mandatory
capability present -- and its `superseded_in_closure()` check (added an hour
before this file) used "instantiated by anything in the closure" as its test.
That is too weak, and the dead raster island is the proof: those six DO
instantiate each other, so they looked live. **REACHABILITY FROM THE CONSOLE
ROOT is the test.** A module nothing reaches is not in the console however many
of its friends name it.

WHAT THIS GATE ENFORCES
-----------------------
  G1  Every non-package source in the console's fit closure is REACHABLE from
      `zhao_console_core`. Dead sources are waste in the fit and a lie in any
      area number taken from it.
  G2  Every module reachable from `zhao_console_core` is IN that closure. A
      missing source does not shrink the design, it kills the fit at
      elaboration -- loudly, but hours in.
  G3  No reachable module has a HIGHER-VERSIONED SIBLING on disk. This is the
      owner's ruling, "you only get to fit the latest version", made
      structural. Exceptions must be declared with a reason in the inventory.
  G4  Every module under `fpga/rtl` has a DISPOSITION -- either it is reachable
      (derived, needs no entry) or the inventory says what it is and why. A new
      file with no disposition fails, so nothing can drift in unclassified.

The inventory is `design/console_inventory.yml` and it holds ONLY what cannot be
derived: the dispositions of modules that are deliberately not in the console,
and the few allowed version exceptions. Everything else is computed, for the
reason `completion_register.py`'s own docstring gives -- a hand-kept membership
table is a table that lies the moment somebody forgets.

Exit 0 when the inventory and the tree agree. Exit 1 otherwise, naming every
disagreement and which gate it broke.
"""

from __future__ import annotations

import io
import pathlib
import re
import sys

_HERE = pathlib.Path(__file__).resolve()
ROOT = _HERE.parents[2]
sys.path.insert(0, str(_HERE.parent))
sys.path.insert(0, str(ROOT / "tools" / "budget"))

from module_graph import build  # noqa: E402
import completion_register as reg  # noqa: E402

RTL = ROOT / "fpga" / "rtl"
INVENTORY = ROOT / "design" / "console_inventory.yml"
CONSOLE_ROOT = "zhao_console_core"

# The dispositions a non-console module may carry. Each says something
# different about WHY it is out, and they are not interchangeable: "superseded"
# means there is a newer one and it is a bug to compose this; "oracle" means it
# is deliberately kept executable as a reference; "instrument" means it exists
# to measure, never to ship.
DISPOSITIONS = {
    "superseded",       # a newer version exists; superseded_by: must name it
    "oracle",           # executable reference model / differential oracle
    "instrument",       # fit top, probe, generated measurement harness
    "bench",            # simulation-only model or harness
    "not_v1",           # deliberately out of v1; needs a ruling cite
    "top",              # a root in its own right (the board, the census top)
    "pending_compose",  # BELONGS in the console and is not wired yet
}

# `pending_compose` is the one that must never become comfortable. It is a WORK
# LIST: every entry is a block the console is supposed to contain. It is a
# separate disposition from `not_v1` precisely so that "not composed yet" can
# never be read as "decided to leave out" -- that conflation is how the entire
# v1 FIELD datapath came to be the thing being fitted while v3 sat on disk.
_WORK_LIST = "pending_compose"


def _read_inventory() -> dict:
    """A deliberately small YAML subset -- two nested levels, no lists.

    Hand-rolled rather than pulling PyYAML in, for the same reason the rest of
    tools/ does: this file is a GATE, and a gate that cannot run because an
    interpreter lacks a package is a gate that gets skipped.
    """
    if not INVENTORY.exists():
        return {"modules": {}, "version_exceptions": {}}
    modules: dict[str, dict] = {}
    exceptions: dict[str, str] = {}
    section = None
    cur = None
    for raw in io.open(INVENTORY, encoding="utf-8", errors="replace"):
        line = raw.rstrip("\n")
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if re.match(r"^modules:\s*$", line):
            section, cur = "modules", None
            continue
        if re.match(r"^version_exceptions:\s*$", line):
            section, cur = "exceptions", None
            continue
        m = re.match(r"^  (\S+):\s*$", line)
        if m and section == "modules":
            cur = m.group(1)
            modules[cur] = {}
            continue
        m = re.match(r"^  (\S+):\s*(.+)$", line)
        if m and section == "exceptions":
            exceptions[m.group(1)] = m.group(2).strip().strip('"')
            continue
        m = re.match(r"^    (\w+):\s*(.*)$", line)
        if m and section == "modules" and cur:
            modules[cur][m.group(1)] = m.group(2).strip().strip('"')
            continue
    return {"modules": modules, "version_exceptions": exceptions}


def _is_package(stem: str, decl: dict) -> bool:
    p = decl.get(stem)
    if p is None:
        hits = list(RTL.rglob(stem + ".sv"))
        if not hits:
            return False
        p = hits[0]
    try:
        text = io.open(p, encoding="utf-8", errors="replace").read()
    except OSError:
        return False
    return re.search(r"^\s*package\s+\w+\s*;", text, re.M) is not None


def reachable_from(root: str, decl: dict, inst: dict) -> set[str]:
    """Modules the console actually elaborates.

    THE TEST THAT MATTERS. "Instantiated somewhere in the closure" is weaker and
    was fooled by the dead v1 raster island, whose six modules instantiate one
    another quite happily while nothing reaches them from the top.
    """
    seen: set[str] = set()
    stack = [root]
    while stack:
        mod = stack.pop()
        if mod in seen or mod not in decl:
            continue
        seen.add(mod)
        for child in inst.get(decl[mod], ()):
            stack.append(child)
    return seen


def newer_siblings(mod: str, on_disk: set[str]) -> list[str]:
    """Higher-versioned siblings, in BOTH shapes this tree uses.

        suffix   zhao_texture_cache_pipe_v2  supersedes  zhao_texture_cache_pipe
        infix    zhao_field_v3_len           supersedes  zhao_field_len

    A grep for one finds half of them, which is a large part of why the v1 FIELD
    datapath went unnoticed.
    """
    out = []
    for cand in on_disk:
        if cand == mod:
            continue
        if re.fullmatch(re.escape(mod) + r"_v\d+", cand):
            out.append(cand)
            continue
        m = re.fullmatch(r"(zhao_[a-z0-9]+)_v(\d+)_(.+)", cand)
        if m and "%s_%s" % (m.group(1), m.group(3)) == mod:
            out.append(cand)
    return sorted(out)


def run_check() -> int:
    decl, inst = build("fpga/rtl")
    inv = _read_inventory()
    mods, exceptions = inv["modules"], inv["version_exceptions"]

    # A FILE IS NOT A MODULE, and assuming so put a real defect in this gate.
    #
    # The first version took the module universe to be `{p.stem for p in
    # RTL.rglob("*.sv")}`. Five files in this tree declare more than one module,
    # and one of them cost a breakage the same hour: `zhao_raster_quant_fin` is
    # the SECOND module inside `zhao_raster_quant.sv`. G1 therefore called that
    # file dead (no module of its NAME was live), I removed it from the console
    # closure on the gate's say-so, and G2's complaint that `zhao_raster_quant_fin`
    # was unlisted led me to add a source line for a file that does not exist --
    # which broke the console lint outright. The FIELD packet found it and
    # restored the real file.
    #
    # So: modules come from SCANNING (`module_graph.build` already does exactly
    # that), files come from the fit list, and the two are related by a map
    # rather than by a naming convention. This is the same lesson the completion
    # register learned five times -- READ THE STRUCTURE, NOT THE CONVENTION --
    # and I wrote it into that file's comments before repeating it here.
    def _rel(q) -> str:
        """Repo-relative posix, because the two sources agree on neither.

        `module_graph.build()` yields repo-relative paths, `closure_paths()`
        yields absolute ones, and on Windows one uses backslashes. The first
        version compared them raw and G2 fired for EVERY module in the
        console -- a gate that fails on everything is as useless as one that
        fails on nothing, and rather more alarming.
        """
        r = pathlib.Path(q)
        try:
            r = r.resolve().relative_to(ROOT)
        except (ValueError, OSError):
            pass
        return r.as_posix()

    file_of = {mod: _rel(path) for mod, path in decl.items()}
    modules_in = {}
    for mod, path in decl.items():
        modules_in.setdefault(_rel(path), set()).add(mod)

    all_modules = set(decl)
    live = reachable_from(CONSOLE_ROOT, decl, inst)
    closure_files = {_rel(p) for p in reg.closure_paths()}

    errors: list[str] = []

    # ---- G1: nothing dead in the closure -----------------------------------
    # A FILE is dead when NONE of the modules it declares is reachable.
    for f in sorted(closure_files):
        declared = modules_in.get(f, set())
        if not declared:
            # declares no module: a package, or a file of typedefs
            continue
        if declared & live:
            continue
        # A FILE THAT DECLARES A PACKAGE is pulled in by `import`, not by
        # instantiation, so reachability says nothing about it. Several here
        # carry a package AND a small guard module beside it --
        # zhao_render_texture_pkg.sv is package + layout guard -- and the
        # guard being unreachable is not evidence the package is dead weight.
        try:
            if re.search(r"^\s*package\s+\w+\s*;",
                         io.open(ROOT / f, encoding="utf-8",
                                 errors="replace").read(), re.M):
                continue
        except OSError:
            pass
        errors.append(
            "G1 DEAD IN CLOSURE: %s is a console fit source and none of the "
            "module(s) it declares (%s) is reachable from %s. It is area the "
            "fit compiles and the machine never uses."
            % (f, ", ".join(sorted(declared)), CONSOLE_ROOT))

    # ---- G2: nothing missing from the closure ------------------------------
    # A live MODULE is missing when the file DECLARING it is not a source.
    for mod in sorted(live):
        path = file_of.get(mod)
        if path is None:
            continue
        if path not in closure_files:
            errors.append(
                "G2 MISSING FROM CLOSURE: %s is elaborated by the console and "
                "the file declaring it (%s) is not in the fit source list -- "
                "the fit dies at elaboration."
                % (mod, path))

    # ---- G3: the latest version, and only the latest ------------------------
    for mod in sorted(live):
        newer = newer_siblings(mod, all_modules)
        if not newer:
            continue
        if mod in exceptions:
            continue
        errors.append(
            "G3 SUPERSEDED IS WIRED: the console elaborates %s, but %s exists. "
            "Owner ruling 2026-09-19: \"YOU ONLY GET TO FIT THE LATEST "
            "VERSION. IF IT IS BROKEN YOU FIX IT.\" Compose the newer one, or "
            "add %s to version_exceptions with a reason."
            % (mod, ", ".join(newer), mod))

    # ---- G4: everything has a disposition ----------------------------------
    for stem in sorted(all_modules - live):
        if _is_package(stem, decl):
            continue
        entry = mods.get(stem)
        if entry is None:
            errors.append(
                "G4 UNCLASSIFIED: %s is in fpga/rtl, is not in the console, and "
                "design/console_inventory.yml does not say what it is. Give it "
                "a disposition (%s) and a reason."
                % (stem, "|".join(sorted(DISPOSITIONS))))
            continue
        d = entry.get("disposition")
        if d not in DISPOSITIONS:
            errors.append(
                "G4 BAD DISPOSITION: %s has disposition %r, which is not one of "
                "%s." % (stem, d, "|".join(sorted(DISPOSITIONS))))
            continue
        if not entry.get("why"):
            errors.append("G4 NO REASON: %s is %s but says no why." % (stem, d))
        if d == "superseded":
            by = entry.get("superseded_by")
            if not by:
                errors.append(
                    "G4 NO SUCCESSOR: %s is marked superseded and does not name "
                    "what superseded it." % stem)
            elif by not in all_modules:
                errors.append(
                    "G4 PHANTOM SUCCESSOR: %s says it is superseded by %s, "
                    "which does not exist in fpga/rtl." % (stem, by))

    # ---- report -------------------------------------------------------------
    print("console inventory: %d modules declared, %d elaborated by %s, "
          "%d fit source files" % (len(all_modules), len(live), CONSOLE_ROOT,
                                    len(closure_files)))
    if not errors:
        print("console inventory OK -- every source is reachable, every "
              "reachable module is a source, the latest version is the one "
              "wired, and everything out has a declared reason.")
        return 0
    print()
    print("CONSOLE INVENTORY FAILED -- %d problem(s)" % len(errors))
    for e in errors:
        print("  - " + e)
    return 1


def _self_test() -> None:
    """The gate must be able to SEE a fault, or its silence means nothing."""
    fake_disk = {"zhao_a", "zhao_a_v2", "zhao_field_len", "zhao_field_v3_len"}
    assert newer_siblings("zhao_a", fake_disk) == ["zhao_a_v2"], "suffix shape"
    assert newer_siblings("zhao_field_len", fake_disk) == ["zhao_field_v3_len"], "infix shape"
    assert newer_siblings("zhao_a_v2", fake_disk) == [], "newest has no successor"
    d = {"r": pathlib.Path("r.sv"), "k": pathlib.Path("k.sv"), "x": pathlib.Path("x.sv")}
    i = {pathlib.Path("r.sv"): {"k"}, pathlib.Path("x.sv"): {"x"}}
    assert reachable_from("r", d, i) == {"r", "k"}, "reachability follows instantiation"
    assert "x" not in reachable_from("r", d, i), "a self-instantiating island is NOT reachable"


if __name__ == "__main__":
    _self_test()
    print("check_console_inventory: self-test PASSED "
          "(both version shapes; an island that names itself is not reachable)")
    raise SystemExit(run_check())
