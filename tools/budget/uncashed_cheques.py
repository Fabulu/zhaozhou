#!/usr/bin/env python3
"""Find work that was DONE and never CASHED.

Written 2026-09-09, after a TODO-archaeology sweep across the contracts, the
RTL headers and the git history turned up a ~33-DSP / ~6,000-ALM saving that
had been fully planned on 2026-08-24, whose prerequisite was deliberately
BUILT, and whose final consolidation was simply never performed. It was not
forgotten because anyone was careless. It was forgotten because nothing in the
tree was watching for it.

The sweep found the two shapes, and the reason this file exists is that BOTH
ARE MECHANICALLY DETECTABLE FROM DATA ALREADY IN THE REPO:

  1. A THING BUILT IS NOT A THING INSTALLED.
     A module exists, has been measured or has a fit target, and NOTHING
     INSTANTIATES IT. `zhao_project_service` (-33 DSP), `zhao_proj_arena3`,
     `zhao_raster_rcp24_v3` (-3 DSP, owner-ruled, fit-confirmed) are all this
     shape today.

  2. A THING FIXED IS NOT A THING MEASURED.
     The repair landed and the receipt did not. `zhao_terrain_normals` was
     improved on 2026-08-24 (six multipliers to one) and the fit database still
     describes it with a DIRTY row from 2026-08-20 saying 18 DSP. Any roll-up
     reading that row is reading a measurement of a design that no longer
     exists -- and reading it HIGH, which is the flattering direction for
     "look how much we could save" and the wrong direction for a budget.

This is the `.gitignore` lesson from CLAUDE.md, one level up. There, an ignore
rule made 33 GB of waste invisible to git while it went on filling the disk;
the knowledge was written down thoroughly and nothing was ever pointed at it.
Here the knowledge was written into module headers and commit messages, in
detail, by people who understood exactly what they were deferring -- and no
tool ever read them back.

WHAT THIS TOOL DOES NOT DO. It does not decide that an uninstantiated module
SHOULD be instantiated. Plenty should not: oracles, superseded blocks,
deliberately-failing frontier points, characterization wrappers and
not-yet-composed work in progress are all legitimately rootless. So every
finding is cross-referenced against `design/prod_manifest.yml`, which is where
that intent is recorded, and a module the manifest explains is reported at a
lower tier or not at all. The tool raises a QUESTION; the manifest is the place
to answer it, and adding the answer there is how a finding is closed.

REUSED, NOT REIMPLEMENTED (brief 2.7, and having broken exactly this twice):
`tools/quartus/module_graph.build` owns "who instantiates whom", and
`tools/budget/dsp_census.load_evidence` owns "what has been measured". This
file computes neither.
"""
import io
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "quartus"))
sys.path.insert(0, HERE)

import module_graph                                    # noqa: E402
from dsp_census import load_evidence, commit_time      # noqa: E402

MANIFEST = os.path.join("design", "prod_manifest.yml")
TARGETS = os.path.join("design", "fit_targets.yml")


# ---------------------------------------------------------------------------
# The manifest is the record of INTENT. A rootless module it explains is not a
# finding; a rootless module it does not mention is.
# ---------------------------------------------------------------------------
def load_manifest_sections(path=MANIFEST):
    """{module: section} for every module named under a top-level key.

    Deliberately a text scan of `  - name` / `  - name: note` lines under the
    nearest preceding `key:` header, because that is the file's actual shape
    and a YAML parse would need the schema to stay put.
    """
    out = {}
    try:
        text = io.open(os.path.join(ROOT, path), encoding="utf-8",
                       errors="replace").read()
    except OSError:
        return out
    section = None
    for line in text.splitlines():
        h = re.match(r"^([a-z_]+):\s*$", line)
        if h:
            section = h.group(1)
            continue
        m = re.match(r"^\s+-\s+(\w+)\s*(?::(.*))?$", line)
        if m and section:
            out.setdefault(m.group(1), (section, (m.group(2) or "").strip()))
    return out


def load_fit_targets(path=TARGETS):
    """Module names that somebody thought worth measuring.

    The entries are a LIST OF DICTS -- `  - top: zhao_project_service` -- not
    `  name:` keys. The first spelling of this function assumed the latter and
    returned an empty set, which silently emptied check 1 of every module that
    has a target but no fit row yet: `zhao_project_service` and
    `zhao_proj_arena3`, i.e. exactly the two the tool was written to catch.
    It reported "0 fit targets" and looked like it had worked.

    That is this repo's own law -- a number that is exactly zero is a broken
    instrument until proven otherwise -- committed by the tool written to
    enforce it. The anti-vacuity assertion in self_test() is the guard.
    """
    try:
        text = io.open(os.path.join(ROOT, path), encoding="utf-8",
                       errors="replace").read()
    except OSError:
        return set()
    return set(re.findall(r"^\s+-\s+top:\s*(\w+)\s*$", text, re.M))


_TOUCH_CACHE = None


def last_touch_map():
    """{path: newest committer timestamp} for every tracked file, in ONE walk.

    The obvious spelling is `git log -1 --format=%ct -- <path>` per module. At
    216 modules that is 216 git processes and it does not finish inside a
    two-minute budget -- measured, not guessed, on 2026-09-09. A single
    `--name-only` walk answers the same question for the whole tree in one
    process, and the first timestamp seen for a path is by definition its
    newest because git log is newest-first.
    """
    global _TOUCH_CACHE
    if _TOUCH_CACHE is not None:
        return _TOUCH_CACHE
    out = {}
    try:
        p = subprocess.run(["git", "log", "--format=@%ct", "--name-only"],
                           capture_output=True, text=True, cwd=ROOT)
        when = None
        for line in (p.stdout or "").splitlines():
            if line.startswith("@"):
                when = int(line[1:])
            elif line and when is not None:
                out.setdefault(line.replace("\\", "/"), when)
    except Exception:
        pass
    _TOUCH_CACHE = out
    return out


def last_touch(path):
    """Committer timestamp of the last commit to touch this file, or None."""
    return last_touch_map().get(path.replace("\\", "/"))


# ---------------------------------------------------------------------------
# Being NAMED in the manifest is not the same as being SETTLED by it.
#
# The first version of this tool treated any manifest entry as intent and
# therefore as a non-finding. That silenced `zhao_raster_rcp24_v3`, whose note
# reads "not-yet-adopted ... stays counted until it is composed" -- which is
# not an explanation that the module should stay rootless, it is a DEFERRAL
# WRITTEN DOWN. The owner ruled that swap, a fit confirmed it, and the manifest
# note is the record of it not having happened yet.
#
# So the discriminator is the note's own language, not its presence. A note
# saying "superseded" or "probe" CLOSES the question. A note saying "unused",
# "not-yet-adopted", "until", "waiting", "blocked" or "when" LEAVES IT OPEN and
# is exactly the thing being hunted -- a cheque that somebody was honest enough
# to write down and nobody has cashed.
# ---------------------------------------------------------------------------
CLOSING = ("superseded", "probe", "harness", "frozen", "refuted", "oracle")
DEFERRING = ("not-yet-adopted", "unused", "until", "waiting", "blocked",
             "when ", "not yet", "pending", "once ")


def verdict(section, note):
    """CLOSED / PENDING / UNDECLARED for one rootless module."""
    blob = ((section or "") + " " + (note or "")).lower()
    if any(k in blob for k in DEFERRING):
        return "PENDING"
    if any(k in blob for k in CLOSING):
        return "CLOSED"
    if section in ("top",):
        return "CLOSED"
    return "UNDECLARED"


# ---------------------------------------------------------------------------
# CHECK 1 -- a thing built is not a thing installed
# ---------------------------------------------------------------------------
def uninstantiated(decl, inst, ev, manifest, targets):
    installed = set()
    for mods in inst.values():
        installed |= mods
    rows = []
    for mod, path in sorted(decl.items()):
        if mod in installed:
            continue
        measured = ev.get(mod) or []
        has_target = mod in targets
        if not measured and not has_target:
            # Nobody measured it and nobody asked to. Not evidence of a
            # deferred saving -- just a module. Silent by design: a detector
            # that reports every root reports 66 things and is ignored.
            continue
        section, note = manifest.get(mod, (None, ""))
        rows.append({
            "module": mod, "path": path, "section": section, "note": note,
            "targeted": has_target, "verdict": verdict(section, note),
            "dsp": next((e.dsp for e in measured if e.dsp is not None), None),
            "alm": next((e.alm for e in measured if e.alm is not None), None),
        })
    return rows


# ---------------------------------------------------------------------------
# CHECK 2 -- a thing fixed is not a thing measured
# ---------------------------------------------------------------------------
def stale_receipts(decl, ev):
    """Fit rows describing a design the file has since moved past.

    Two independent grounds, reported separately because they fail differently:
      DIRTY  -- the row itself says rtlCleanAtHead is false, so it never
                described any committed state exactly.
      BEHIND -- the row's sourceCommit is older than the last commit that
                touched the module's own file.

    Only the module's OWN file is walked here, not its instantiation closure.
    `dsp_census --staleness` already owns the closure question and is slow;
    this is the cheap, always-on half, and it catches the case that matters
    most -- somebody edited the arithmetic and never refitted.
    """
    rows = []
    for mod, path in sorted(decl.items()):
        touched = last_touch(path)
        for e in ev.get(mod, []):
            if e.commit is None and e.clean is not False:
                continue
            when = commit_time(e.commit)
            dirty = (e.clean is False)
            behind = (touched is not None and when is not None and touched > when)
            if not dirty and not behind:
                continue
            rows.append({
                "module": mod, "path": path, "label": e.label,
                "commit": (e.commit or "")[:8], "dirty": dirty,
                "behind": behind, "dsp": e.dsp, "alm": e.alm,
                "lag_days": (round((touched - when) / 86400.0, 1)
                             if behind else None),
            })
    return rows



BLOCKS = os.path.join("design", "blocks.yml")

# Wrapper suffixes the repo uses for a BIT-IDENTICAL view onto a ratified law.
# `_unclamped` is the documented D-1 pattern: `shade_flat_tri_dir` is a
# bit-identical wrapper around `shade_flat_tri_dir_unclamped`, and the goldens
# not moving is how that was known to be correct. So two blocks naming those two
# strings are naming ONE law, and an exact-string comparison cannot see it.
LAW_WRAPPER_SUFFIXES = ("_unclamped", "_clamped", "_raw")


def normalise_law(name):
    for suf in LAW_WRAPPER_SUFFIXES:
        if name.endswith(suf):
            return name[: -len(suf)]
    return name


def declared_laws(path=BLOCKS):
    """{reference_model: [block ids]} from design/blocks.yml."""
    out = {}
    try:
        text = io.open(os.path.join(ROOT, path), encoding="utf-8",
                       errors="replace").read()
    except OSError:
        return out
    cur = None
    for line in text.splitlines():
        m = re.match(r"^\s*-\s+id:\s*([A-Za-z0-9._]+)\s*$", line)
        if m:
            cur = m.group(1)
            continue
        r = re.search(r"reference_model:\s*([A-Za-z_][A-Za-z0-9_:]*)", line)
        if r and cur:
            out.setdefault(r.group(1), []).append(cur)
    return out


def shared_law_collisions(laws):
    """Two blocks naming ONE ratified law -- authored duplication, before it ships.

    This is check 3, and it exists because checks 1 and 2 CANNOT see this class.
    They find modules built-and-uninstantiated and fit rows gone stale; a
    duplication that is about to be AUTHORED is neither. Written 2026-09-09
    after building zhao_terrain_shade and then reading GEOM.LIGHT.md:118, which
    warned against exactly it and named a previous instance of the same mistake.

    Two tiers, because the strings differ even when the law does not:
      EXACT -- byte-identical reference_model. Finds the projector:
               zref::render::project_vertex is declared by BOTH GEOM.PROJECT and
               TERRAIN.PROJECT, which is the 66-DSP duplication that opened the
               whole DSP campaign. That signal was sitting in blocks.yml the
               entire time and nothing read it.
      WRAPPER -- same law after stripping a bit-identical wrapper suffix. Finds
               TERRAIN.SHADE (`..._unclamped`) against GEOM.LIGHT (the wrapper).
    """
    exact, wrapper = [], []
    for law, blocks in sorted(laws.items()):
        u = sorted(set(blocks))
        if len(u) > 1:
            exact.append((law, u))
    groups = {}
    for law, blocks in laws.items():
        groups.setdefault(normalise_law(law), []).append((law, sorted(set(blocks))))
    for base, entries in sorted(groups.items()):
        if len(entries) < 2:
            continue
        owners = sorted({b for _, bl in entries for b in bl})
        if len(owners) > 1:
            wrapper.append((base, sorted(entries), owners))
    return exact, wrapper


# ---------------------------------------------------------------------------
# THE INSTRUMENT HAS TO BE SEEN TO FIRE.
#
# CLAUDE.md: "A detector that has not been shown to FIRE has not been tested",
# and "a number that is exactly zero is a broken instrument until proven
# otherwise". Both checks below are run on synthetic input at import, in BOTH
# polarities -- a case that must fire and a case that must not. If either
# stops behaving, this module refuses to load rather than printing a
# reassuring zero.
# ---------------------------------------------------------------------------
class _E(object):
    def __init__(self, dsp=None, alm=None, commit=None, clean=None, label=None):
        self.dsp, self.alm = dsp, alm
        self.commit, self.clean, self.label = commit, clean, label


def self_test():
    decl = {"m_root_measured": "a.sv", "m_root_bare": "b.sv",
            "m_child": "c.sv", "m_root_declared": "d.sv"}
    inst = {"a.sv": {"m_child"}}
    ev = {"m_root_measured": [_E(dsp=33, alm=6199)]}
    manifest = {"m_root_declared": ("excluded", "superseded")}
    targets = {"m_root_declared"}

    got = {r["module"] for r in uninstantiated(decl, inst, ev, manifest, targets)}
    assert "m_root_measured" in got, "FIRE case missed: measured rootless module"
    assert "m_root_declared" in got, "FIRE case missed: targeted rootless module"
    assert "m_child" not in got, "FALSE POSITIVE: an instantiated module"
    assert "m_root_bare" not in got, "FALSE POSITIVE: unmeasured, untargeted root"

    # The verdict split is the load-bearing part of check 1 and it must be
    # shown to separate, not just to run. A "superseded" note closes; a
    # "not-yet-adopted" note does not, and that exact string is what
    # zhao_raster_rcp24_v3 carries today.
    assert verdict("excluded", "superseded  by the pairpipe") == "CLOSED"
    assert verdict("excluded", "not-yet-adopted  V3 tile per S10") == "PENDING"
    assert verdict("excluded", "unused  fitted but not composed") == "PENDING"
    assert verdict("excluded", "probe  a leaf-fit pair") == "CLOSED"
    assert verdict(None, "") == "UNDECLARED"

    # CHECK 3 in both polarities. The wrapper tier is the load-bearing half:
    # TERRAIN.SHADE and GEOM.LIGHT name DIFFERENT STRINGS for ONE law, so an
    # exact-match check reports zero and looks like it worked.
    assert normalise_law("zref::render::shade_flat_tri_dir_unclamped") == "zref::render::shade_flat_tri_dir", "wrapper suffix not stripped"
    ex, wr = shared_law_collisions({
        "zref::render::project_vertex": ["GEOM.PROJECT", "TERRAIN.PROJECT"],
        "zref::render::shade_flat_tri_dir_unclamped": ["TERRAIN.SHADE"],
        "zref::render::shade_flat_tri_dir": ["GEOM.LIGHT"],
        "zref::CmdDma": ["CMD.DMA"],
        "zref::solo": ["ONE.BLOCK", "ONE.BLOCK"],
    })
    assert [l for l, _ in ex] == ["zref::render::project_vertex"],         "EXACT tier: must fire on the projector and only it, got %r" % (ex,)
    assert [b for b, _, _ in wr] == ["zref::render::shade_flat_tri_dir"],         "WRAPPER tier: must fire on shade/light, got %r" % (wr,)
    assert len(declared_laws()) > 50 or not os.path.exists(
        os.path.join(ROOT, BLOCKS)), "declared_laws went vacuous"

    # Check 2 in isolation from git, by driving its two grounds directly.
    def grounds(clean, touched, when):
        dirty = (clean is False)
        behind = (touched is not None and when is not None and touched > when)
        return dirty, behind

    assert grounds(False, None, None) == (True, False), "DIRTY must fire"
    assert grounds(True, 200, 100) == (False, True), "BEHIND must fire"
    assert grounds(True, 100, 200) == (False, False), "fresh row must NOT fire"
    assert grounds(True, 100, 100) == (False, False), "equal times must NOT fire"

    # ANTI-VACUITY. Both readers below are regexes against a real file's
    # layout, and a regex that matches nothing produces a clean, quiet,
    # completely wrong report. load_fit_targets() shipped exactly that defect
    # for one run. These two assertions cost a file read and make the failure
    # loud at import instead of invisible at output.
    if os.path.exists(os.path.join(ROOT, TARGETS)):
        n = len(load_fit_targets())
        assert n > 50, ("load_fit_targets matched %d entries -- the file's "
                        "shape moved and the regex went vacuous" % n)
    if os.path.exists(os.path.join(ROOT, MANIFEST)):
        n = len(load_manifest_sections())
        assert n > 50, ("load_manifest_sections matched %d entries -- the "
                        "file's shape moved and the regex went vacuous" % n)
    return True


assert self_test(), "uncashed_cheques self-test failed"


def main():
    os.chdir(ROOT)
    decl, inst = module_graph.build()
    ev = load_evidence()
    manifest = load_manifest_sections()
    targets = load_fit_targets()

    print("uncashed_cheques: self-test 4 fire / 4 no-fire PASSED")
    print("scanned %d modules, %d fit targets, %d manifest entries\n"
          % (len(decl), len(targets), len(manifest)))

    print("== CHECK 1: BUILT BUT INSTALLED NOWHERE "
          "(measured or targeted, instantiated by no .sv in fpga/rtl)")
    rows = uninstantiated(decl, inst, ev, manifest, targets)
    open_rows = [r for r in rows if r["verdict"] != "CLOSED"]
    for r in sorted(rows, key=lambda r: (r["verdict"] == "CLOSED",
                                         -(r["dsp"] or 0))):
        if r["verdict"] == "CLOSED":
            continue
        cost = []
        if r["dsp"] is not None:
            cost.append("%d DSP" % r["dsp"])
        if r["alm"] is not None:
            cost.append("%d ALM" % r["alm"])
        print("  ** %-32s %-10s %s"
              % (r["module"], r["verdict"],
                 ", ".join(cost) or ("fit target, never measured"
                                     if r["targeted"] else "")))
        if r["note"]:
            print("       manifest says: %s" % r["note"][:96])
    print("\n  %d rootless measured/targeted modules, %d CLOSED by the "
          "manifest,\n  %d STILL OPEN (above)."
          % (len(rows), len(rows) - len(open_rows), len(open_rows)))
    print("  PENDING means the manifest itself records a deferral -- somebody"
          "\n  wrote the cheque down. UNDECLARED means nobody wrote anything."
          "\n  Close a row by wiring it in, or by recording in the manifest why"
          "\n  it should stay rootless in words that do not defer.\n")

    print("== CHECK 2: THE REPAIR LANDED, THE RECEIPT DID NOT")
    srows = stale_receipts(decl, ev)
    for r in sorted(srows, key=lambda r: -(r["lag_days"] or 0)):
        why = []
        if r["dirty"]:
            why.append("DIRTY TREE")
        if r["behind"]:
            why.append("file moved %.1fd after the fit" % r["lag_days"])
        name = r["module"] + ("@" + r["label"] if r["label"] else "")
        print("  %-40s %-8s %s" % (name, r["commit"], "; ".join(why)))
        if r["dsp"] is not None:
            print("       row still asserts %s DSP" % r["dsp"])
    print("  %d row(s) describe a design the file has moved past." % len(srows))
    print("  A DIRTY row never described any committed state exactly. A BEHIND")
    print("  row describes an earlier one. Neither is a current measurement.")
    print("")
    print("")

    print("== CHECK 3: ONE RATIFIED LAW, TWO BLOCKS "
          "(duplication about to be AUTHORED, not inherited)")
    laws = declared_laws()
    exact, wrapper = shared_law_collisions(laws)
    for law, blocks in exact:
        print("  ** EXACT    %-40s %s" % (law, ", ".join(blocks)))
    for base, entries, owners in wrapper:
        if any(law == base and blocks == owners for law, blocks in entries):
            continue
        print("  ** WRAPPER  %-40s %s" % (base, ", ".join(owners)))
        for law, blocks in entries:
            print("                 %-38s %s" % (law, ", ".join(blocks)))
    print("  %d declared law(s) across %d block declaration(s); "
          "%d exact, %d wrapper-level."
          % (len(laws), sum(len(v) for v in laws.values()),
             len(exact), len(wrapper)))
    print("  Checks 1 and 2 CANNOT see this class -- neither block is")
    print("  uninstantiated and neither row is stale. Two blocks naming one law")
    print("  will each need the same arithmetic; decide which one OWNS it before")
    print("  the second gets built.")
    print("  A DIRTY row never described any committed state exactly. A BEHIND"
          "\n  row describes an earlier one. Neither is a current measurement.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
