# Task Log: RUN-20260817-0752 - the Form compiler (W3), typed HIR through exact C++ lowering

**Created:** 2026-08-22 21:40 UTC+02:00 (RECONSTRUCTED — see below)
**Status:** Complete
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260817-0752-form-compiler-w3-exact-lowering/

---

## RECONSTRUCTED, and what that means

This run folder did not exist. It was rebuilt on 2026-08-22 from git history
after the owner pointed out that runs had stopped being created.

**What a reconstruction recovers:** what was committed, when, in what order, and
what each commit said about itself.

**What it does NOT recover, and nobody should read into its absence:** the
owner's asks in their own words, the approaches tried and rejected, the
measurements that came out the wrong way, the subagents spawned and what they
found, and the reasoning behind the sequencing. Those lived in the session and
are gone. That loss is the whole argument for creating the run at the START of a
session rather than writing it up afterwards — a contemporaneous log is not a
tidier version of git history, it is a different and unrecoverable record.

Treat every claim below as inferred from commit subjects and touched paths
unless it names a file that still exists.

---

## Objective

*(inferred)* Build the Form language compiler's W3 lane: a typed HIR, a
deterministic ZIR scheduler, and lowering to deterministic C++17 cartridges,
with exactness enforced rather than assumed.

---

## Progress Timeline

### 00:42-00:57 — NOT PART OF THIS RUN

Five commits (`38d8a63`, `eced813`, `ed54b43`, `3bb36c1`, `1f60806`, `b85c9b7`)
close out the previous deep-keel session: the frozen keel default, the Mosaic
pattern laws, the rim degrade order, randomized forge/mosaic differentials, and
`TEXTURE.MOSAIC` / `FORGE.CLIFF` to REFERENCE_COMPLETE. The last of them is
literally "runs: close out the deep-keel session", so that session DID keep its
run record. Listed here only so the day's history is complete.

### 07:52 — `273142d` stars: reconstruct Noctis indexed motion smear

The day opens with a repair to the star smear rather than compiler work.

### 08:09-09:02 — the compiler spine, in five commits

- `8b0e7ee` typed HIR and a deterministic ZIR scheduler
- `0e20070` canonical maps and cost reports
- `0090705` deterministic C++17 cartridge emission
- `99cbb5c` refuse unlinked field applications
- `ecd2624` lower presentation view contracts

### 10:16-23:00 — thirty-eight commits of exactness

The remainder of the day is a long tail of admission rules, canonicalisation and
identity fixes on that spine. Grouped by what they were defending:

**Typing and numeric exactness.** `1cd7e92` exact Form typing and stagger
lowering; `4977161` reduce aggregate constants exactly; `fae35e5` check Q-format
literals with exact rationals; `30c0f69` fx24 signed-64 admission rails;
`e32d353` exact bounds and qualified flow calls.

**Naming, scoping and shadowing** — the largest single group, which is usually a
sign that the name-resolution model was being discovered rather than
implemented: `6674426` isolate authored C++ identifiers; `5343b78` prevent
inherited C++ helper collisions; `86e3358` honor aggregate shadows over module
qualifiers; `ed6eb48` respect lexical shadows in bare Form calls; `dd1abf1`
canonicalize callable resolution; `9f44327` reserve future lets per lexical
body; `3f48592` Form call and future-let precedence.

**Determinism and identity of artifacts.** `d838c29` regenerate exact semantic
artifacts; `386f935` exact source and resource identities; `fba476b` unify
canonical source maps; `0d1859d` canonical native SOURCE_MAP interop; `31330b5`
the source-map size law; `c5c0c39` filesystem-safe artifact paths.

**Truthfulness of reports.** `1200b52` restore mandatory truthful Form cost
reports; `0a8a337` ratify stagger cadence and cost accounting; `bea957b`
nominal owners and stagger RNG safety.

**Scope discipline.** `249e3eb` keeps concurrent capture specification OUT of
W3.3 — a deliberate refusal to widen the wave.

---

## Subagent Spawns

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| — | — | unrecorded | — | — |

Not recoverable from git history. The one-agent-at-a-time rule was given on
2026-08-15 and tightened on 2026-08-17, so agents were likely in use this day,
but nothing in the commit record identifies them.

---

## Files Created

Not separable from files modified in a reconstruction. Path distribution for the
day, by top-level directory:

| directory | commits touching it |
| --- | ---: |
| `compiler/` | 38 |
| `spec/` | 7 |
| `tools/` | 5 |
| `tests/` | 5 |
| `reference/` | 4 |

---

## Decisions Made

*(inferred from commit subjects)*

- Cost reports must be **truthful or absent** — `1200b52` restores that as a
  mandatory property rather than a nicety.
- Concurrent capture specification is **out of scope for W3.3** (`249e3eb`).
- Authored C++ identifiers are isolated from generated ones, and inherited helper
  collisions are prevented — the generated cartridge may not be able to collide
  with user code.

---

## Next Steps

*(as of the end of this run; superseded since)*

The compiler lane continued into 2026-08-18. **In hindsight this day is the one
the owner later ruled against**: the standing instruction since is *hardware
first, Nanquan is provisional — stop compiler overengineering while proven
hardware remains the centerpiece*. Thirty-eight commits of compiler exactness in
a single day, against a machine that could not yet fit its own shell, is what
that ruling was reacting to. Recorded here because a run log that omits the
mistake is worth less than one that names it.
