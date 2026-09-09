#!/usr/bin/env python3
"""The resource bill: select ONE measurement per instance across every ledger.

REWRITTEN 2026-09-09 against the owner's memory-first rescue brief, section 2.
The previous version's headline -- "152 DSP, and it is a FLOOR" -- is WITHDRAWN.
It was wrong in both directions at once, which is why "floor" was the worst
possible word for it:

  * it opened only zhao_block_fit.json, so it never saw zhao_block_map.json or
    the separate shell receipt;
  * it counted `zhao_terrain_normals` at 18 DSP from an obsolete full fit while
    a LATER map row says 3 and the current RTL walks six products through one
    multiplier -- a 15-DSP OVERCOUNT;
  * it scored `zhao_geom_pose_decode` (18), `zhao_terrain_bake` (17) and
    `zhao_forge_cliff` (2) as ZERO, because their only evidence is in the map
    ledger -- 37 DSP of UNDERCOUNT;
  * it omitted the shell entirely: 16 DSP, 12,707 ALM, 26 M10K.

A partial subtotal is not a mathematical lower bound on the optimised console,
and a sum of isolated fits is not an upper bound either -- composition changes
mapping, replication, pruning and packing. The honest phrase is PARTIAL MIXED
EVIDENCE with the missing costs disclosed, and that is what this prints.

THE VOCABULARY (brief section 2.1), used consistently below:

  INVENTORY              every implementation/probe/version in the tree
  SELECTED DESIGN        one chosen implementation per function
  CURRENT MEASUREMENT    bytes/configuration match the selected closure
  HISTORICAL MEASUREMENT a real measurement of a different/unverified closure
  PROPOSED ALLOCATION    a ceiling for an unqualified candidate
  UNPRICED REQUIREMENT   intended functionality with no applicable measurement

SELECTION ORDER for one instance (brief section 2.3):

  1. enumerate applicable measurements INCLUDING labelled variants
  2-3. match device/closure/parameters; retain a conflict if unclear
  4. prefer a completed applicable FITTED result for area and timing
  5. prefer a NEWER applicable MAP result over an obsolete fit for CURRENT DSP
     structure -- label it mapped, leave fitted area/timing UNRESOLVED
  6. when nothing matches: historical shown as historical, current cost UNKNOWN.
     Never substitute zero. Never silently pick the smallest.

PER-METRIC NULLS. A map row knows DSP and does not know ALM. `None` means
UNKNOWN and is never summed as zero -- the previous version read a missing ALM
correctly but had no way to say "DSP known, ALM unknown" about the same row.

DIRTINESS IS NOT IDENTITY. The old tool called every dirty-tree digest
meaningless. Wrong: a digest can identify the exact bytes of a dirty but
immutable captured specimen, while a clean commit can carry a mismatched
parameter profile. Provenance is reported on two independent axes.

Usage:
    python tools/budget/dsp_census.py
    python tools/budget/dsp_census.py --json
    python tools/budget/dsp_census.py --closure   # nonzero if anything unknown
    python tools/budget/dsp_census.py --self-test
"""
import io
import json
import os
import subprocess
import sys

NLC = chr(10)

sys.path.insert(0, os.path.join("tools", "quartus"))

DEVICE = {"alm": 41910, "dsp": 112, "m10k": 553}
OWNER_DSP_TARGET = 94          # brief: "fewer than 95", target DSP <= 94

FIT_LEDGER = os.path.join("reports", "synthesis", "zhao_block_fit.json")
MAP_LEDGER = os.path.join("reports", "synthesis", "zhao_block_map.json")
SHELL_LEDGER = os.path.join("reports", "synthesis", "zhao_shell_fit.json")

# Evidence stages, most authoritative for AREA first.
FIT, MAP = "fit", "map"


class Evidence(object):
    """One measurement of one module, from one ledger, at one stage."""

    def __init__(self, module, stage, source, dsp=None, alm=None, m10k=None,
                 regs=None, membits=None, status=None, commit=None,
                 clean=None, label=None):
        self.module = module
        self.stage = stage          # FIT or MAP
        self.source = source        # which ledger file
        self.dsp = dsp
        self.alm = alm
        self.m10k = m10k
        self.regs = regs
        self.membits = membits
        self.status = status
        self.commit = commit
        self.clean = clean
        self.label = label          # the @suffix, or None

    @property
    def usable(self):
        """A killed process with partial output is not a completed measurement.

        Brief 2.3: "A failed POLICY check may follow a successful physical fit:
        its measured counts remain evidence, with the failed gate retained."
        So `failed:structure` IS usable -- the fit completed and the budget rules
        rejected it. `timeout`, `incomplete:` and a killed run are not.
        """
        s = (self.status or "")
        if s.startswith("incomplete") or s == "timeout":
            return False
        return self.dsp is not None or self.alm is not None

    @property
    def policy_failed(self):
        return (self.status or "").startswith("failed:")

    def __repr__(self):
        return "<%s %s dsp=%s alm=%s>" % (self.module, self.stage, self.dsp, self.alm)


# ---------------------------------------------------------------------------
# LOADING -- all three inputs, brief section 2.3
# ---------------------------------------------------------------------------
def _load_json(path):
    try:
        return json.load(io.open(path, encoding="utf-8"))
    except (OSError, ValueError):
        return None


def load_evidence(fit_path=FIT_LEDGER, map_path=MAP_LEDGER,
                  shell_path=SHELL_LEDGER):
    """Every measurement from every ledger, keyed by BASE module name.

    Labelled rows are kept, not dropped. Brief 2.3: "An @label is not extra
    silicon, but it can be the ONLY correct measurement." The old tool discarded
    them wholesale and therefore could not see, for instance, that the only
    current RCP evidence is labelled.
    """
    ev = {}

    def add(e):
        ev.setdefault(e.module, []).append(e)

    d = _load_json(fit_path)
    if d:
        for r in d.get("blocks", []):
            name = r.get("module", "")
            base, _, lab = name.partition("@")
            add(Evidence(base, FIT, fit_path, dsp=r.get("dspBlocks"),
                         alm=r.get("alms"), m10k=r.get("ramBlocks"),
                         regs=r.get("registers"), membits=r.get("blockMemoryBits"),
                         status=r.get("status"), commit=r.get("sourceCommit"),
                         clean=r.get("rtlCleanAtHead"), label=lab or None))

    d = _load_json(map_path)
    if d:
        for r in d.get("blocks", []):
            name = r.get("module", "")
            base, _, lab = name.partition("@")
            # A MAP row has no `alms` and no `ramBlocks` -- `estimatedAlms` is an
            # ESTIMATE and is deliberately not loaded as `alm`. Recording an
            # estimate in the same field as a fitted measurement is how a guess
            # becomes a number nobody questions.
            add(Evidence(base, MAP, map_path, dsp=r.get("dspBlocks"),
                         alm=None, m10k=None,
                         regs=r.get("registers"), membits=r.get("blockMemoryBits"),
                         status=r.get("status"), commit=r.get("sourceCommit"),
                         clean=r.get("rtlCleanAtHead"), label=lab or None))

    d = _load_json(shell_path)
    if d:
        res = d.get("resources") or {}
        stages = d.get("stages") or {}
        ok = stages.get("fitter") == "success"
        add(Evidence(d.get("design", {}).get("top", "zhao_shell_top"), FIT,
                     shell_path, dsp=res.get("dspBlocks"),
                     alm=res.get("logicUtilizationAlms"),
                     m10k=res.get("ramBlocks"), regs=res.get("registers"),
                     membits=res.get("blockMemoryBits"),
                     status="ok" if ok else "incomplete:shell",
                     commit=d.get("sourceCommit"),
                     clean=d.get("sourceConeParity")))
    return ev


# ---------------------------------------------------------------------------
# RECENCY -- needed for rule 5, "prefer a NEWER applicable map over an obsolete fit"
# ---------------------------------------------------------------------------
_DATE_CACHE = {}


def commit_time(sha):
    """Committer timestamp, or None when the commit is unknown to this tree."""
    if not sha:
        return None
    if sha in _DATE_CACHE:
        return _DATE_CACHE[sha]
    try:
        out = subprocess.run(["git", "show", "-s", "--format=%ct", sha],
                             capture_output=True, text=True)
        t = int(out.stdout.strip()) if out.returncode == 0 and out.stdout.strip() else None
    except (OSError, ValueError):
        t = None
    _DATE_CACHE[sha] = t
    return t


def load_profiles(path=os.path.join("design", "prod_manifest.yml")):
    """module -> selected label, from the manifest's `selected_profiles:` block.

    Brief 2.6.C. A DECLARATION, not a heuristic. "Prefer the unlabelled row" is
    right for zhao_geom_skin (unlabelled default, @MUL_LANES alternatives) and
    WRONG for the texture island, whose unlabelled row is the LABORATORY build
    and whose shipping configuration is the labelled @g2-prod. Nothing in the
    ledger distinguishes those two shapes; only the manifest can say which
    instance the console contains.

    A value of None means "the unlabelled row is the shipping profile".
    """
    prof = {}
    try:
        txt = io.open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return prof
    i = txt.find(NLC + "selected_profiles:")
    if i < 0:
        return prof
    # [2:], not [1:]. txt[i:] begins with the newline BEFORE the header, so
    # element 0 is empty and element 1 is "selected_profiles:" itself -- which
    # does not start with "- " and tripped the end-of-block `break` on the first
    # iteration, returning an empty dict silently. A parser that returns nothing
    # looks exactly like a manifest with nothing declared.
    for line in txt[i:].split(NLC)[2:]:
        st = line.strip()
        if st.startswith("#") or not st:
            continue
        if not st.startswith("- "):
            break                      # next top-level key ends the block
        body = st[2:].split("#")[0].strip()
        if ":" not in body:
            continue
        name, _, val = body.partition(":")
        val = val.strip().strip('"').strip("'")
        prof[name.strip()] = None if val in ("null", "", "~") else val.lstrip("@")
    return prof


def select(cands, declared="__undeclared__"):
    """Apply the brief's selection order. Returns (chosen, why, alternates).

    `chosen` may be None -- that is the honest answer when nothing is usable,
    and it is NOT zero.
    """
    usable = [e for e in cands if e.usable]
    if not usable:
        return None, "no usable measurement", cands

    # A LABEL IS A DIFFERENT PARAMETER PROFILE, not another measurement of the
    # same one. Brief 2.3: "The actual island uses different reciprocal
    # parameters from the twelve-context leaf comparison. Do not attach the
    # twelve-context area or throughput to an eight-context instance."
    #
    # Caught by this tool's own first real run. `zhao_geom_skin` has an
    # UNLABELLED fit at 9 DSP / 2,225 ALM plus @MUL_LANES=1 (3 DSP) and
    # @MUL_LANES=6 (18 DSP). Selecting across all of them took the 18-DSP
    # variant as "the fit", let a map supersede it, and reported 9 DSP with ALM
    # UNKNOWN -- the right DSP by luck, and 2,225 ALM thrown away.
    #
    # So: the unlabelled row is the selected profile. Labelled rows are used
    # ONLY when there is no unlabelled evidence, and then the profile is flagged
    # unconfirmed rather than quietly adopted -- because an @label can still be
    # the only correct measurement, which is the other half of the same rule.
    # A DECLARED profile outranks the heuristic entirely.
    if declared != "__undeclared__":
        want = declared                      # None means "the unlabelled row"
        picked = [e for e in usable if (e.label or None) == want]
        if picked:
            usable = picked
            profile_note = (" [profile DECLARED in prod_manifest.yml: %s]"
                            % ("@" + want if want else "unlabelled"))
            fits = [e for e in usable if e.stage == FIT and e.dsp is not None]
            maps = [e for e in usable if e.stage == MAP and e.dsp is not None]
            if fits:
                best = max(fits, key=lambda e: commit_time(e.commit) or 0)
                return best, "fitted result" + profile_note, [e for e in usable if e is not best]
            if maps:
                best = max(maps, key=lambda e: commit_time(e.commit) or 0)
                return (best, "MAP only -- fitted area and timing UNKNOWN" + profile_note,
                        [e for e in usable if e is not best])
        else:
            return (None,
                    "DECLARED profile %s has NO usable measurement -- cost UNKNOWN,"
                    " and deliberately not substituted from another profile"
                    % ("@" + want if want else "unlabelled"), cands)

    plain = [e for e in usable if not e.label]
    profile_note = ""
    if plain:
        usable = plain
    else:
        profile_note = (" PROFILE UNCONFIRMED: only labelled measurements exist,"
                        " and the manifest does not declare the selected"
                        " parameters for this instance")

    fits = [e for e in usable if e.stage == FIT and e.dsp is not None]
    maps = [e for e in usable if e.stage == MAP and e.dsp is not None]

    # Rule 5: a NEWER map beats an OBSOLETE fit for current DSP structure.
    if fits and maps:
        newest_fit = max(fits, key=lambda e: commit_time(e.commit) or 0)
        newest_map = max(maps, key=lambda e: commit_time(e.commit) or 0)
        tf, tm = commit_time(newest_fit.commit) or 0, commit_time(newest_map.commit) or 0
        if tm > tf and newest_map.dsp != newest_fit.dsp:
            return (newest_map,
                    ("newer MAP (%s) supersedes an older fit that says %s DSP; "
                     "fitted area and timing remain UNRESOLVED"
                     % (newest_map.commit[:8] if newest_map.commit else "?", newest_fit.dsp))
                    + profile_note,
                    [e for e in usable if e is not newest_map])

    # Rule 4: prefer a completed fitted result for area and timing.
    if fits:
        best = max(fits, key=lambda e: commit_time(e.commit) or 0)
        return best, "fitted result" + profile_note, [e for e in usable if e is not best]
    if maps:
        best = max(maps, key=lambda e: commit_time(e.commit) or 0)
        return (best, "MAP only -- DSP known, fitted area and timing UNKNOWN" + profile_note,
                [e for e in usable if e is not best])
    return None, "no usable measurement", cands


# ---------------------------------------------------------------------------
# THE BILL
# ---------------------------------------------------------------------------
def build_bill(tops, ev, targets_text="", profiles=None):
    profiles = profiles or {}
    rows = []
    for m in sorted(tops):
        cands = ev.get(m, [])
        chosen, why, alts = select(cands, profiles[m]) if m in profiles else select(cands)
        has_target = ("- top: %s\n" % m) in targets_text
        if chosen is None:
            kind = ("UNPRICED (no fit target -- nobody can measure it)"
                    if not has_target else "UNPRICED (target exists, never run)")
            rows.append({"module": m, "kind": kind, "why": why, "dsp": None,
                         "alm": None, "m10k": None, "stage": None,
                         "label": None, "clean": None, "policy_failed": False,
                         "alternates": len(cands)})
            continue
        kind = ("CURRENT (fitted)" if chosen.stage == FIT else "CURRENT (mapped)")
        rows.append({"module": m, "kind": kind, "why": why,
                     "dsp": chosen.dsp, "alm": chosen.alm, "m10k": chosen.m10k,
                     "stage": chosen.stage, "label": chosen.label,
                     "clean": chosen.clean, "policy_failed": chosen.policy_failed,
                     "alternates": len(alts)})
    return rows


def totals(rows):
    t = {"dsp": 0, "alm": 0, "m10k": 0,
         "dsp_unknown": 0, "alm_unknown": 0, "m10k_unknown": 0}
    for r in rows:
        for k in ("dsp", "alm", "m10k"):
            if r[k] is None:
                t[k + "_unknown"] += 1
            else:
                t[k] += r[k]
    return t


# ---------------------------------------------------------------------------
# FIXTURES -- brief section 2.7, driven through the REAL loader and selector
# ---------------------------------------------------------------------------
def self_test():
    """Small fixtures whose correct answer is checkable by hand.

    Brief 2.7's final instruction is the important one: "Test the ACTUAL
    loader/selector path, not a reimplementation of the intended rule inside a
    self-test that never calls it." The previous version's self-test built its
    own dicts and called `census()` on them; it could not have caught the
    missing map ledger, because it never opened a ledger.

    These write real ledger files and call load_evidence()/select().
    """
    import tempfile
    import shutil
    d = tempfile.mkdtemp()
    try:
        fit = os.path.join(d, "fit.json")
        mp = os.path.join(d, "map.json")
        sh = os.path.join(d, "shell.json")

        # normals: an OLD fit at 18 and a NEWER map at 3. HEAD is newer than any
        # ancestor, so HEAD stands in for "newer" without inventing timestamps.
        head = subprocess.run(["git", "rev-parse", "HEAD"],
                              capture_output=True, text=True).stdout.strip()
        old = subprocess.run(["git", "rev-list", "--max-parents=0", "HEAD"],
                             capture_output=True, text=True).stdout.strip().split("\n")[0]

        io.open(fit, "w", encoding="utf-8").write(json.dumps({"blocks": [
            {"module": "normals", "dspBlocks": 18, "alms": 900, "ramBlocks": 1,
             "status": "ok", "sourceCommit": old, "rtlCleanAtHead": True},
            {"module": "nulldsp", "dspBlocks": None, "alms": None,
             "status": "ok", "sourceCommit": head},
            {"module": "policyfail", "dspBlocks": 8, "alms": 494, "ramBlocks": 0,
             "status": "failed:structure", "sourceCommit": head, "rtlCleanAtHead": True},
            {"module": "killed", "dspBlocks": None, "alms": None,
             "status": "incomplete:failed:quartus_map.exe", "sourceCommit": head},
            {"module": "skin", "dspBlocks": 9, "alms": 2225, "ramBlocks": 2,
             "status": "ok", "sourceCommit": old, "rtlCleanAtHead": True},
            {"module": "skin@MUL_LANES=6", "dspBlocks": 18, "alms": 2595,
             "status": "ok", "sourceCommit": head, "rtlCleanAtHead": True},
            {"module": "labelledonly@NCTX=12", "dspBlocks": 3, "alms": 986,
             "status": "ok", "sourceCommit": head, "rtlCleanAtHead": True},
            {"module": "island", "dspBlocks": 17, "alms": 13133, "ramBlocks": 45,
             "status": "failed:structure", "sourceCommit": old, "rtlCleanAtHead": True},
            {"module": "island@g2-prod", "dspBlocks": 17, "alms": 10837,
             "ramBlocks": 49, "status": "ok", "sourceCommit": head,
             "rtlCleanAtHead": True},
        ]}))
        io.open(mp, "w", encoding="utf-8").write(json.dumps({"blocks": [
            {"module": "normals", "dspBlocks": 3, "estimatedAlms": 700,
             "status": "ok", "sourceCommit": head},
            {"module": "maponly", "dspBlocks": 17, "estimatedAlms": 500,
             "status": "ok", "sourceCommit": head},
        ]}))
        io.open(sh, "w", encoding="utf-8").write(json.dumps({
            "design": {"top": "shell"}, "stages": {"fitter": "success"},
            "resources": {"dspBlocks": 16, "logicUtilizationAlms": 12707,
                          "ramBlocks": 26, "registers": 14812},
            "sourceCommit": head, "sourceConeParity": True}))

        ev = load_evidence(fit, mp, sh)

        # 1. old fit 18 + newer applicable map 3 -> current mapped 3
        chosen, why, _ = select(ev["normals"])
        assert chosen is not None and chosen.dsp == 3, \
            "newer map did not supersede the obsolete fit: %r" % (chosen,)
        assert chosen.stage == MAP, "supersession did not label the stage as mapped"
        assert chosen.alm is None, \
            "a map row must leave fitted ALM UNRESOLVED, not carry estimatedAlms"

        # 2. null DSP -> incomplete, never zero
        chosen, why, _ = select(ev["nulldsp"])
        assert chosen is None, "a row with no numbers was selected: %r" % (chosen,)

        # 3. a completed fit with a FAILED POLICY check keeps its counts
        chosen, _, _ = select(ev["policyfail"])
        assert chosen is not None and chosen.dsp == 8 and chosen.policy_failed, \
            "failed:structure must retain its measured counts and its failed gate"

        # 4. a killed run is not a measurement
        chosen, _, _ = select(ev["killed"])
        assert chosen is None, "a killed/incomplete run was treated as evidence"

        # 5. map-only module is priced from the map ledger, not scored zero
        chosen, _, _ = select(ev["maponly"])
        assert chosen is not None and chosen.dsp == 17 and chosen.stage == MAP, \
            "a map-only module was not priced from the map ledger"

        # 6. the shell arrives from its own file, as one root
        chosen, _, _ = select(ev["shell"])
        assert chosen is not None and chosen.dsp == 16 and chosen.alm == 12707, \
            "the separate shell receipt was not loaded"

        # 7. unknown is not zero, in the totals
        rows = build_bill(["normals", "nulldsp", "maponly", "shell"], ev)
        t = totals(rows)
        assert t["dsp"] == 3 + 17 + 16, "dsp total wrong: %s" % t
        assert t["dsp_unknown"] == 1, "an unknown DSP was not counted as unknown"
        assert t["alm_unknown"] == 3, \
            "map rows and the null row must all report ALM unknown: %s" % t

        # 8. A LABEL IS A DIFFERENT PROFILE. `skin` has an unlabelled fit at 9
        #    DSP / 2,225 ALM plus a MUL_LANES=6 variant at 18. The unlabelled row
        #    must win and must KEEP its ALM. This fixture exists because the
        #    first real run of this tool got 9 by luck -- via the 18-DSP variant
        #    being superseded by a map -- and threw the 2,225 ALM away.
        chosen, why, _ = select(ev["skin"])
        assert chosen is not None and chosen.dsp == 9,             "the unlabelled profile was not selected: %r" % (chosen,)
        assert chosen.alm == 2225,             "a labelled variant displaced the unlabelled fit and lost its ALM"
        assert chosen.label is None, "a labelled row was selected over an unlabelled one"

        # 9. when ONLY labelled rows exist, use one but say the profile is
        #    unconfirmed -- an @label can be the only correct measurement.
        chosen, why, _ = select(ev["labelledonly"])
        assert chosen is not None and chosen.dsp == 3,             "a labelled-only module must still be priced, not scored zero"
        assert "PROFILE UNCONFIRMED" in why,             "a labelled-only selection must be flagged, not quietly adopted"

        # 10. A DECLARED PROFILE OUTRANKS THE HEURISTIC, and this fixture is why
        #     the mechanism exists. "Prefer the unlabelled row" is right for
        #     `skin` and WRONG for the texture island, whose unlabelled row is
        #     the LABORATORY build and whose shipping configuration is labelled.
        #     Nothing in a ledger distinguishes those two shapes.
        heur, _, _ = select(ev["island"])
        assert heur.alm == 13133, "fixture drift: the heuristic should take the lab row"
        decl, why, _ = select(ev["island"], "g2-prod")
        assert decl.alm == 10837 and decl.label == "g2-prod",             "a declared profile did not override the unlabelled-preferring heuristic"
        assert "DECLARED" in why, "a declared selection must say so in its reason"

        # 11. A DECLARED profile with NO measurement is UNKNOWN. It must never
        #     silently fall back to another profile's number -- that would be
        #     attaching one instance's cost to a different instance, which is the
        #     specific error the brief calls out about NCTX=12 versus NCTX=8.
        miss, why, _ = select(ev["island"], "does-not-exist")
        assert miss is None and "NO usable measurement" in why,             "a declared-but-unmeasured profile fell back instead of reporting unknown"

        # 12. MUTATION PROOF (brief 2.7): drop the map ledger and the normals
        #    answer must change. A fixture that passes with the map ledger
        #    ignored is not testing the thing it names.
        ev_nomap = load_evidence(fit, os.path.join(d, "does-not-exist.json"), sh)
        chosen, _, _ = select(ev_nomap["normals"])
        assert chosen is not None and chosen.dsp == 18, \
            "with the map ledger removed the answer must revert to the stale 18"
        assert "maponly" not in ev_nomap, \
            "a map-only module must vanish entirely without its ledger"
        return True
    finally:
        shutil.rmtree(d, ignore_errors=True)


def main():
    self_test()
    args = sys.argv[1:]
    if "--self-test" in args:
        print("dsp_census self-test: all twelve fixtures pass through the real "
              "loader and selector, including the mutation proof that removing "
              "the map ledger changes the answer.")
        return 0

    from check_prod_manifest import read_manifest
    tops, excluded = read_manifest()
    ev = load_evidence()
    try:
        targets_text = io.open(os.path.join("design", "fit_targets.yml"),
                               encoding="utf-8", errors="replace").read()
    except OSError:
        targets_text = ""

    # The shell is a root of the machine and is NOT in the production manifest's
    # `top:` list. Brief 1.1: "The separate shell receipt contains 16 DSP."
    roots = list(tops)
    if "zhao_shell_top" in ev and "zhao_shell_top" not in roots:
        roots.append("zhao_shell_top")

    rows = build_bill(roots, ev, targets_text, load_profiles())
    t = totals(rows)

    if "--json" in args:
        print(json.dumps({"rows": rows, "totals": t,
                          "device": DEVICE, "dspTarget": OWNER_DSP_TARGET},
                         indent=1))
        return 0

    print("PARTIAL MIXED EVIDENCE -- not a floor, not a ceiling.")
    print("Composition changes mapping, replication, pruning and packing, and "
          "the unpriced rows below are missing entirely.")
    print()
    print("  %-6s %9s %9s %9s" % ("", "COUNTED", "DEVICE", "unknown rows"))
    for k, lab in (("dsp", "DSP"), ("alm", "ALM"), ("m10k", "M10K")):
        print("  %-6s %9d %9d %9d" % (lab, t[k], DEVICE[k], t[k + "_unknown"]))
    print()
    print("  owner target: DSP <= %d. Counted %d, with %d rows unpriced."
          % (OWNER_DSP_TARGET, t["dsp"], t["dsp_unknown"]))
    print()

    sup = [r for r in rows if "supersedes" in (r["why"] or "")]
    if sup:
        print("  SUPERSEDED FITS -- a newer map result was preferred for current")
        print("  DSP structure; fitted area and timing are unresolved for these:")
        for r in sup:
            print("     %-34s %s DSP   %s" % (r["module"], r["dsp"], r["why"]))
        print()

    mapped = [r for r in rows if r["kind"] == "CURRENT (mapped)" and r not in sup]
    if mapped:
        print("  MAPPED-ONLY (%d): DSP known, ALM and M10K UNKNOWN, never zero:"
              % len(mapped))
        for r in mapped[:10]:
            print("     %-34s %s DSP" % (r["module"], r["dsp"]))
        print()

    unpriced = [r for r in rows if r["dsp"] is None]
    print("  UNPRICED REQUIREMENTS (%d) -- current cost UNKNOWN, not zero:"
          % len(unpriced))
    notgt = [r for r in unpriced if "no fit target" in r["kind"]]
    print("     %d have no fit target at all, so nobody can measure them"
          % len(notgt))
    print("     %d have a target and have not been run" % (len(unpriced) - len(notgt)))
    print()

    # SPECIFIED FUNCTIONS WITH NO IMPLEMENTATION AT ALL.
    #
    # These are not in `top:` -- there is nothing to fit -- so they are not among
    # the "unpriced" rows above either. They were INVISIBLE to every version of
    # this tool until 2026-09-09. Brief 1.3: they "cannot disappear from the bill
    # just because they do not yet have accepted implementations."
    try:
        man = io.open(os.path.join("design", "prod_manifest.yml"),
                      encoding="utf-8", errors="replace").read()
    except OSError:
        man = ""
    j = man.find(NLC + "unpriced_requirements:")
    unbuilt = []
    if j >= 0:
        for line in man[j:].split(NLC)[2:]:
            st = line.strip()
            if st.startswith("#") or not st:
                continue
            if not st.startswith("- "):
                break
            unbuilt.append(st[2:])
    if unbuilt:
        print()
        print("  SPECIFIED BUT NOT BUILT (%d) -- no RTL, nothing to measure, and"
              % len(unbuilt))
        print("  therefore NOT in the %d DSP above. The machine is incomplete by"
              % t["dsp"])
        print("  this much before any of it is optimised:")
        for u in unbuilt:
            print("     %s" % u)

    pf = [r for r in rows if r["policy_failed"]]
    if pf:
        print("  COUNTED BUT GATE-FAILED (%d): the fit completed and the budget"
              % len(pf))
        print("  rules rejected it. The counts are evidence; the gate stays failed.")
        for r in pf[:8]:
            print("     %-34s %s DSP  %s ALM" % (r["module"], r["dsp"], r["alm"]))

    if "--closure" in args:
        if t["dsp_unknown"] or t["alm_unknown"]:
            print()
            print("CLOSURE VERDICT REFUSED: %d DSP and %d ALM costs are unknown."
                  % (t["dsp_unknown"], t["alm_unknown"]))
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
