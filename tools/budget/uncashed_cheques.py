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
def _manifest_sections_from_text(text):
    """Parse only authoritative ``top``/``excluded`` disposition rows."""
    out = {}
    section = None
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].rstrip()
        h = re.match(r"^([a-z_]+):\s*$", line)
        if h:
            section = h.group(1)
            continue
        m = re.match(r"^\s+-\s+(\w+)\s*(?::(.*))?$", line)
        if m and section in ("top", "excluded"):
            name = m.group(1)
            if name in out:
                raise ValueError(
                    "duplicate authoritative production disposition for " + name)
            out[name] = (section, (m.group(2) or "").strip())
    return out


def load_manifest_sections(path=MANIFEST):
    """{module: section} for authoritative ``top``/``excluded`` dispositions.

    Deliberately a text scan of `  - name` / `  - name: note` lines under the
    nearest preceding `key:` header, because that is the file's actual shape
    and a YAML parse would need the schema to stay put. Ancillary lists such as
    ownership providers and retired census slots may repeat module names, but
    they are not disposition records. Inline YAML comments are removed before
    matching, and duplicate authoritative dispositions fail closed.
    """
    try:
        with io.open(os.path.join(ROOT, path), encoding="utf-8",
                     errors="replace") as stream:
            text = stream.read()
    except OSError:
        return {}
    return _manifest_sections_from_text(text)


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


def _yaml_uncommented(line):
    """The line with any `#` comment removed.

    ADDED 2026-09-20, and it is a REAL DEFECT REPAIR, not tidying. This file's
    reader searched raw lines for `reference_model:`, so a COMMENT saying what a
    row used to declare was read as a declaration. That is not hypothetical: the
    R94 repair of FORGE.SHADOW records the phantom `zref::forge::shadow_hull` in
    prose immediately above the row, and without this the tool would have gone on
    seeing a law that was deleted -- a parser that reads its own changelog. The
    self-test below fires on exactly that line.

    blocks.yml has no `#` inside any reference_model value, so stripping from the
    first `#` is sufficient here and is deliberately not a YAML parser.
    """
    return line.split("#", 1)[0]


def declared_laws(path=BLOCKS):
    """{reference_model: [block ids]} from design/blocks.yml."""
    out = {}
    try:
        text = io.open(os.path.join(ROOT, path), encoding="utf-8",
                       errors="replace").read()
    except OSError:
        return out
    cur = None
    for raw in text.splitlines():
        line = _yaml_uncommented(raw)
        m = re.match(r"^\s*-\s+id:\s*([A-Za-z0-9._]+)\s*$", line)
        if m:
            cur = m.group(1)
            continue
        r = re.search(r"reference_model:\s*([A-Za-z_][A-Za-z0-9_:]*)", line)
        if r and cur:
            out.setdefault(r.group(1), []).append(cur)
    return out


def strip_cxx_comments(text):
    """Remove `//` and `/* */` comments. CODE ONLY IS EVIDENCE OF A DECLARATION.

    ADDED 2026-09-20 by the coordinator, after CHECK 5 was caught being blind to
    the very next variant of the defect it was written for.

    R94's phantom, `zref::forge::shadow_hull`, had ZERO occurrences under
    `reference/`, so a substring test over raw file text found it. But the blob
    included COMMENTS, and the moment somebody writes a header that says "this
    replaces the zref::GeomWarp phantom", THE OBITUARY RESOLVES THE CORPSE. That
    is not hypothetical: `reference/include/zref/zref_geom_warp.hpp` names
    `zref::GeomWarp` in prose at lines 1 and 6, and had the warp lane not also
    added a real `using GeomWarp = geom_warp::GeomWarp;`, the GEOM.WARP row would
    have passed this check on the strength of a sentence explaining that it
    should not.

    Demonstrated before repairing, per CLAUDE.md's broken-instrument law: the
    function was called with a blob containing the name ONLY inside a `//`
    comment and returned no rows, while the same name absent entirely reported
    correctly. The positive control worked; the interesting case was invisible.
    `_comment_blindness_self_test()` below keeps both halves.

    This does not make the check a parser, and it is not meant to. A name in a
    string literal or in dead `#if 0` code still resolves, and only a compiler
    settles the general question. It removes the ONE failure mode that the act of
    documenting a phantom creates, which is the mode that was about to bite.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j            # keep the newline itself
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            out.append(" ")                  # a token separator, not a join
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _reference_symbols():
    """Every identifier-bearing byte of CODE under reference/, as one blob.

    Comments are stripped -- see `strip_cxx_comments`. Prose is not a symbol.
    """
    parts = []
    base = os.path.join(ROOT, "reference")
    for dirpath, _dirs, names in os.walk(base):
        for n in names:
            if n.endswith((".hpp", ".cpp", ".h", ".c")):
                try:
                    parts.append(strip_cxx_comments(
                        io.open(os.path.join(dirpath, n),
                                encoding="utf-8",
                                errors="replace").read()))
                except OSError:
                    pass
    return "\n".join(parts)


def _comment_blindness_self_test():
    """CHECK 5's own positive control. It must FIRE, not merely be present.

    CLAUDE.md: a detector reading zero is a claim, and it is the claim to check
    hardest. These four cases are the four answers this resolution can give, and
    the two middle ones are the ones that were wrong before the strip.
    """
    laws = {"zref::PhantomSymbol": ["SELFTEST.BLOCK"]}

    def rows(blob):
        return unresolved_reference_models(laws, ref_blob=blob, decl={})

    # 1. Absent entirely -> MUST report. (This half always worked.)
    assert rows("// nothing\n"), "CHECK 5 blind to an absent symbol"
    # 2. Named only in a line comment -> MUST report. (This was the blindness.)
    assert rows("// one day we will write zref::PhantomSymbol\n"), \
        "CHECK 5 resolves a symbol that exists only in a // comment"
    # 3. Named only in a block comment -> MUST report.
    assert rows("/* replaces the zref::PhantomSymbol phantom */\n"), \
        "CHECK 5 resolves a symbol that exists only in a /* */ comment"
    # 4. Actually declared -> MUST NOT report. The negative control: without it
    #    a check that reported EVERYTHING would pass the three above.
    assert not rows("struct PhantomSymbol { int x; };\n"), \
        "CHECK 5 reports a symbol that is genuinely declared"
    # 5. The block-comment strip must not weld two identifiers into one.
    assert "AB" not in strip_cxx_comments("A/* c */B"), \
        "strip_cxx_comments joined tokens across a comment"


def unresolved_reference_models(laws, ref_blob=None, decl=None):
    """CHECK 5: a `reference_model:` that names a symbol the tree does not have.

    OWNER RULING R94, 2026-09-20: `design/blocks.yml`'s FORGE.SHADOW row declared
    `zref::forge::shadow_hull`, which has ZERO occurrences under `reference/`.

    WHY THIS IS A DETECTOR AND NOT A LINT. Check 3 above finds two blocks that
    have declared the SAME ratified law -- the signal that caught the 66-DSP
    projector duplication, sitting in this file the whole time with nothing
    reading it. A name that resolves to NOTHING can never collide with anything,
    so a row like that is silently EXEMPT from check 3. The defect is invisible
    and it is in the flattering direction: one fewer collision reported.

    Two namespaces, two questions, because the repo uses both:
      `zref::...`  the LEAF identifier must appear somewhere under `reference/`.
                   Leaf rather than the full path, because the declarations are
                   written with varying namespace depth and matching the whole
                   string reports everything and is therefore useless.
      `rtl::...`   the named MODULE must be declared in fpga/rtl. These are
                   blocks whose oracle is another block, which is a legitimate
                   thing to say and a different resolution.

    It REPORTS. Turning it into a gate means first deciding what the eight
    currently-unresolved rows should say, and a gate that is red on arrival is a
    gate people learn to skip -- which is how the v1 FIELD datapath got composed.
    """
    if ref_blob is None:
        ref_blob = _reference_symbols()
    # Strip HERE, not only in `_reference_symbols`. This is the one place the
    # question "does this name resolve" is answered, and a caller that supplies
    # its own blob -- every self-test above does -- must get the same semantics
    # as the live tree. The first version of this repair stripped only in the
    # loader, and `_comment_blindness_self_test` caught it immediately by still
    # resolving a comment-only name. Stripping twice is idempotent on code.
    ref_blob = strip_cxx_comments(ref_blob)
    if decl is None:
        decl = {}
    rows = []
    for sym, blocks in sorted(laws.items()):
        leaf = sym.split("::")[-1]
        if not leaf:
            continue
        if sym.startswith("rtl::"):
            ok = leaf in decl
            kind = "rtl module"
        else:
            ok = leaf in ref_blob
            kind = "reference/ symbol"
        if not ok:
            rows.append((sym, kind, sorted(set(blocks))))
    return rows


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


# ---------------------------------------------------------------------------
# CHECK 4 -- a thing composed is not a thing ADOPTED
# ---------------------------------------------------------------------------
PROD_TOP = "zhao_prod_top"


def adopted_nowhere(decl, inst, ev, manifest, targets, prod_top=PROD_TOP):
    """Measured or targeted modules the PRODUCTION top cannot reach.

    Check 1 asks whether anything instantiates a module. That question has a
    blind spot which hid the largest saving in the machine for weeks, and the
    shape is worth stating exactly, because it reads as health:

        zhao_terrain_pipe -> zhao_proj_subsystem -> zhao_project_service
                                                 -> ONE zhao_project_core

    Every link there has a root, so check 1 is silent on all four. The chain is
    fitted at 102.19 MHz on physical pins. And `zhao_prod_top` instantiates
    none of it -- production still carries `zhao_geom_project` and
    `zhao_terrain_project` with a `zhao_project_core` each, 24,399 ALUT of one
    circuit built twice. Rooted at every step, adopted at none.

    So this check walks the instantiation graph FROM THE PRODUCTION TOP and
    reports what it cannot reach. Modules check 1 already holds (rootless
    entirely) are left to it, so the two checks partition rather than overlap.

    A CLOSED manifest disposition silences a row here exactly as it does in
    check 1: `probe`, `superseded` and friends are answers. `not-yet-adopted`
    and `unused` are deferrals, which is the whole point -- the manifest has
    said "not-yet-adopted" about this chain the entire time and nothing read it
    back.
    """
    kids = {m: set(inst.get(p, ())) for m, p in decl.items()}
    reachable, stack = set(), [prod_top]
    while stack:
        m = stack.pop()
        if m in reachable or m not in decl:
            continue
        reachable.add(m)
        stack.extend(kids.get(m, ()))

    installed = set()
    for mods in inst.values():
        installed |= mods

    rows = []
    for mod, path in sorted(decl.items()):
        if mod == prod_top or mod in reachable:
            continue
        if mod not in installed:
            continue          # rootless entirely -- check 1 owns this row
        measured = ev.get(mod) or []
        has_target = mod in targets
        if not measured and not has_target:
            continue
        section, note = manifest.get(mod, (None, ""))
        v = verdict(section, note)
        if v == "CLOSED":
            continue
        rows.append({
            "module": mod, "path": path, "section": section, "note": note,
            "targeted": has_target, "verdict": v,
            "dsp": next((e.dsp for e in measured if e.dsp is not None), None),
            "alm": next((e.alm for e in measured if e.alm is not None), None),
        })
    return rows, len(reachable)

def gate_failures(check1_rows, check4_rows):
    """The rows --gate refuses: measured or targeted, open, and undocumented.

    Extracted from main() so both polarities can be asserted below. A gate
    whose decision lives inline in a print-and-exit block is a gate nobody
    has watched fire.
    """
    return [r for r in list(check1_rows) + list(check4_rows)
            if r["verdict"] == "UNDECLARED"]

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

    # THE COMMENT STRIPPER, FIRED. This exact line is in blocks.yml today (the
    # R94 repair records the phantom it removed), and without _yaml_uncommented
    # the reader takes it for a live declaration -- a parser reading its own
    # changelog, and the failure is silent and flattering.
    ghost = "    # `reference_model: zref::forge::shadow_hull` STOOD HERE UNTIL"
    assert re.search(r"reference_model:\s*([A-Za-z_][A-Za-z0-9_:]*)",
                     ghost) is not None, "the self-test's own ghost line stopped matching"
    assert re.search(r"reference_model:\s*([A-Za-z_][A-Za-z0-9_:]*)",
                     _yaml_uncommented(ghost)) is None,         "FIRE case missed: a commented-out reference_model is still read as one"
    assert _yaml_uncommented("    reference_model: zref::render::project_vertex")         .strip() == "reference_model: zref::render::project_vertex",         "FALSE POSITIVE: the stripper ate a live declaration"

    # CHECK 5 in both polarities, on the shape R94 names.
    u = unresolved_reference_models(
        {"zref::forge::shadow_hull": ["FORGE.SHADOW"],
         "zref::render::project_vertex": ["GEOM.PROJECT"],
         "rtl::zhao_real_block": ["A.BLOCK"],
         "rtl::zhao_absent_block": ["B.BLOCK"]},
        ref_blob="int32_t project_vertex(const mat4fx& vp);",
        decl={"zhao_real_block": "r.sv"})
    u_got = {s for s, _k, _b in u}
    assert "zref::forge::shadow_hull" in u_got,         "FIRE case missed: a zref name with no symbol under reference/"
    assert "rtl::zhao_absent_block" in u_got,         "FIRE case missed: an rtl:: name with no module"
    assert "zref::render::project_vertex" not in u_got,         "FALSE POSITIVE: a law that does resolve"
    assert "rtl::zhao_real_block" not in u_got,         "FALSE POSITIVE: an rtl:: name that does resolve"
    # And on LIVE data it must not go vacuous in the other direction: with a
    # blob that contains nothing, every declared law is unresolved.
    assert len(unresolved_reference_models(declared_laws(), ref_blob="",
                                           decl={})) > 50 or not os.path.exists(
        os.path.join(ROOT, BLOCKS)), "check 5 went vacuous against an empty tree"
    # And CHECK 5's COMMENT blindness, which the four cases above could not see
    # because every one of them passes a blob that is already pure code.
    _comment_blindness_self_test()

    # CHECK 4 in both polarities, on the shape that actually hid the projector:
    # a chain that is rooted at every link and adopted at none. `m_adopted` is
    # reachable from the production top; `m_orphan_chain` is instantiated (so
    # check 1 is silent) but the production top cannot reach it.
    a_decl = {"zhao_prod_top": "p.sv", "m_adopted": "a.sv",
              "m_side_top": "s.sv", "m_orphan_chain": "o.sv",
              "m_orphan_closed": "c.sv"}
    a_inst = {"p.sv": {"m_adopted"},
              "s.sv": {"m_orphan_chain", "m_orphan_closed"}}
    a_ev = {"m_adopted": [_E(dsp=1, alm=10)],
            "m_orphan_chain": [_E(dsp=2, alm=8694)],
            "m_orphan_closed": [_E(dsp=0, alm=5)]}
    a_rows, a_reach = adopted_nowhere(
        a_decl, a_inst, a_ev,
        {"m_orphan_closed": ("excluded", "probe  a leaf-fit pair")},
        {"m_side_top"})
    a_got = {r["module"] for r in a_rows}
    assert "m_orphan_chain" in a_got, "FIRE case missed: composed but never adopted"
    assert "m_adopted" not in a_got, "FALSE POSITIVE: reachable from the production top"
    assert "m_side_top" not in a_got, "FALSE POSITIVE: rootless -- check 1 owns it"
    assert "m_orphan_closed" not in a_got, "FALSE POSITIVE: a CLOSED disposition is an answer"
    assert a_reach == 2, "reachability walk went wrong: %d" % a_reach

    # ANTI-VACUITY for check 4. On the real tree the production top must reach
    # a substantial hierarchy; a walk that reaches almost nothing would report
    # the entire design as unadopted and be quietly, spectacularly wrong.
    # The failure direction here is the opposite of the usual one -- too LOUD
    # rather than too quiet -- so it would be caught, but only after being
    # believed once.
    # The --gate decision in both polarities. A PENDING backlog must NOT fail
    # the gate (it is 16 rows today and the tool exists to read them back);
    # a single UNDECLARED row must.
    assert gate_failures([{"verdict": "PENDING"}], [{"verdict": "PENDING"}]) == [],         "GATE FALSE POSITIVE: a written deferral must not fail the ratchet"
    assert len(gate_failures([{"verdict": "UNDECLARED", "module": "m", "path": "m.sv"}], [])) == 1,         "GATE FIRE case missed: an undocumented rootless module must fail"
    assert len(gate_failures([], [{"verdict": "UNDECLARED", "module": "n", "path": "n.sv"}])) == 1,         "GATE FIRE case missed: an undocumented unadopted module must fail"

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
    sample_manifest = """
ownership_roles:
  providers:
    - retired_a
retired_census_slots:
  - retired_a
top:
  - live_root # inline comment must not hide this row
excluded:
  - retired_a: superseded retained oracle # disposition comment
"""
    parsed = _manifest_sections_from_text(sample_manifest)
    assert parsed == {
        "live_root": ("top", ""),
        "retired_a": ("excluded", "superseded retained oracle"),
    }, "ancillary rows or inline comments corrupted authoritative dispositions"
    try:
        _manifest_sections_from_text(sample_manifest +
                                     "  - retired_a: not-yet-adopted duplicate\n")
    except ValueError:
        pass
    else:
        raise AssertionError("duplicate authoritative disposition did not fire")

    if os.path.exists(os.path.join(ROOT, MANIFEST)):
        manifest = load_manifest_sections()
        n = len(manifest)
        assert n > 50, ("load_manifest_sections matched %d entries -- the "
                        "file's shape moved and the regex went vacuous" % n)
    return True


if not __debug__:
    raise RuntimeError(
        "uncashed_cheques refuses Python -O: optimized mode removes its "
        "assertion-based positive controls")
if not self_test():
    raise AssertionError("uncashed_cheques self-test failed")


def main():
    os.chdir(ROOT)
    decl, inst = module_graph.build()
    ev = load_evidence()
    manifest = load_manifest_sections()
    targets = load_fit_targets()

    print("uncashed_cheques: self-test PASSED -- fire and no-fire controls "
          "for checks 1-5, the YAML comment stripper, the verdict split, "
          "and the --gate ratchet")
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

    print("\n== CHECK 4: COMPOSED BUT NEVER ADOPTED "
          "(instantiated somewhere, unreachable from %s)" % PROD_TOP)
    arows, reach = adopted_nowhere(decl, inst, ev, manifest, targets)
    for r in sorted(arows, key=lambda r: -(r["alm"] or 0)):
        cost = []
        if r["alm"] is not None:
            cost.append("%d ALM" % r["alm"])
        if r["dsp"] is not None:
            cost.append("%d DSP" % r["dsp"])
        print("  ** %-32s %-10s %s"
              % (r["module"], r["verdict"],
                 ", ".join(cost) or ("fit target, never measured"
                                     if r["targeted"] else "")))
        if r["note"]:
            print("       manifest says: %s" % r["note"][:96])
    print("\n  %s reaches %d of %d modules; %d measured/targeted module(s) "
          "outside\n  that hierarchy carry an OPEN disposition."
          % (PROD_TOP, reach, len(decl), len(arows)))
    print("  Check 1 cannot see these -- every one of them IS instantiated.")
    print("  A module can be built, composed into a subsystem, composed into a")
    print("  pipe and fitted on physical pins, and still not be what production")
    print("  instantiates. Rooted at every step, adopted at none.")

    print("\n== CHECK 5: A `reference_model:` THAT RESOLVES TO NOTHING "
          "(owner ruling R94)")
    urows = unresolved_reference_models(laws, decl=decl)
    for sym, kind, blocks in urows:
        print("  ** %-40s no %-18s  declared by %s"
              % (sym, kind, ", ".join(blocks)))
    print("\n  %d of %d declared reference models do not resolve."
          % (len(urows), len(laws)))
    print("  These rows are EXEMPT FROM CHECK 3 ABOVE, silently: a name that")
    print("  resolves to nothing can never collide with another block's, so a")
    print("  wrong string buys an exemption from the one tool this tree has")
    print("  against a second implementation of ratified arithmetic. That is")
    print("  the broken-instrument law in the ledger rather than in a counter:")
    print("  the defect makes the report SHORTER.")
    print("  Repair is one of two things and never a third: name the law that")
    print("  exists, or REMOVE the key and say in the row why the block has no")
    print("  reference model. Inventing a plausible symbol is the same defect.")

    # --gate is a RATCHET, not a verdict on the backlog. PENDING rows are
    # allowed: the manifest wrote the cheque down, and this tool exists to
    # read those back, not to forbid them. UNDECLARED rows are the failure --
    # a module that is measured or fit-targeted, is reachable from no
    # production hierarchy, and about which nobody wrote anything at all.
    #
    # That distinction is what keeps the gate non-vacuous in both directions.
    # A gate that failed on PENDING would be red today, stay red for months,
    # and be suppressed; a gate that failed on nothing would be the green
    # that this file spent 600 lines warning about. The fire case is one line
    # in prod_manifest.yml being deleted, which is exactly the regression
    # worth catching on the day it happens.
    if "--gate" in sys.argv:
        undeclared = gate_failures(rows, arows)
        if undeclared:
            print("\nGATE FAILED: %d module(s) carry NO disposition at all:"
                  % len(undeclared))
            for r in undeclared:
                print("  %-34s %s" % (r["module"], r["path"]))
            print("Record why it is rootless or unadopted in "
                  "design/prod_manifest.yml. A deferral in words is accepted;",
                  "silence is not.")
            return 1
        print("\nGATE PASSED: every open row carries a written disposition "
              "(%d PENDING, 0 UNDECLARED)."
              % len([r for r in rows + arows if r["verdict"] == "PENDING"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
