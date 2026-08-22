# DIAGNOSIS — Verification evidence integrity: one disease, several presentations

*Diagnostic pass, 2026-08-16, main @ `778b01a`. Analysis only — nothing in the repo was
modified. Everything below was verified against the actual commits, the ledger sources
(`tools/ledger/src/rules.ts`, `design/blocks.yml`, `design/formal_runs.yml`), the formal
harnesses, and the run archive. Nothing heavier than `git show` and `grep` was executed;
no builds, no ctest, no provers (a formal lane may be running).*

---

## 0. Method and what was checked

Read: the five ratification/queue documents in this run dir; `TASK_LOG.md` here and in the
wave-1 run dirs; `REVIEW-FABLE-ALL-WAVES.md` (RUN-20260815-0544); charter v0.2 §2, §4, §19,
§21, §26, §29; `rules.ts` V1–V16 in full; `formal_runs.yml` in full; commits `1bc42a5`,
`9d49806`, `d997ced`, `768ce1a`, `3ca4661`, `e842cc0`, `a7e5964`, `0b8c71c`, `20c5cb3`,
`7ae6b3b`, `dabd46a`, `a4ea5d9`, `3406850`, `72318b1` (messages + stats + selected diffs);
`formal_mem_arbiter.sv` and `video_linebuf_fv.sv` scope guards; `tests/CMakeLists.txt`
format shim; `.github/workflows/ci.yml`; the MEM.VRAM.ARBITER ledger entry in full.

Cheap greps run for costing (results used in §6): 14 "by construction / validated
upstream"-class comments across 7 RTL files, only 2 files carrying an "enforced by"
annotation; 10 `.sby` files on disk, of which the two *bounded* harnesses without a
self-asserting scope guard are `video_framectl_fv.sv` (bmc depth 60) and
`formal_mem_guard.sv` (bmc depth 30) — `video_mode` is an unbounded `prove`,
`formal_mem_refresh` is banked.

**Every incident in the brief checks out against the record**, with the corrections in §8.

---

## 1. Root cause — named

> **The project's evidence system recorded attestations, not observations. A claim of
> verification was cheaper to produce than the verification act, was recorded on the same
> surface as real evidence, and — until V16 — nothing in the loop required any claim to be
> coupled to an event that could have falsified it.**

Shorter: **green that could not have been red**. Every Class A–D incident is a pass, badge,
citation, comment, or constant for which the honest question *"what concrete event would
have made this red?"* had the answer *"nothing"* — either the act never happened (A1, A2,
A4, D11), or the act happened but was structurally incapable of failing (A3, A5, A6, D12,
D14), or the claim lived in prose where no act was ever attached (B7, B8, C9), or two
facts were never compared by anything (C10, the §19 SV/C++ Duo divergence that merged
cleanly at `3406850` and was fixed at `0b8c71c`/`a7e5964`).

Why this project produces such claims at volume — four contributing causes, separated as
requested:

**(a) Inherent to formal/hardware verification.** Vacuity is the classic industrial formal
failure mode: an implication whose antecedent is unreachable is a theorem. Bounded proofs
silently scope themselves (the depth-130 BMC that cannot see a 780-cycle refresh). Golden
CRCs generated from the implementation confirm the implementation (`REVIEW-FABLE-ALL-WAVES`
verdict line: "protected from detection by self-generated goldens"). None of this is
special to Zhaozhou; what is special is that nothing here initially demanded the standard
antidotes (covers, scope assertions, mutation checks).

**(b) Specific to this toolchain.** This toolchain is unusually good at converting "never
ran" and "ran against the wrong model" into green: mingw ahead of oss-cad-suite kills yosys
`0xC0000139` *before parsing* ("never elaborated" wearing a tooling costume —
`RATIFICATION-refresh-urgent-justification.md`); the old SKIP condition matched any
"syntax error" so a never-parsed property reported ready forever (`1bc42a5`); `read_slang`
ties undriven locals to `1'x` and prep's opt folds the branches away *before*
`setundef -anyseq` (`e842cc0`); `(* anyseq *)` on locals elaborates to constants
(`1bc42a5`); `VlThreadPool::~VlThreadPool` deadlocks at exit and strands the verdict in an
unflushed pipe (`7ae6b3b`); ctest prints "100% tests passed" while counting SKIPs. Each is
a mechanism by which an *absent* act produced the same observable as a *successful* act.

**(c) Specific to how this project records evidence.** The ledger validates **shape**, not
**referents**: V3/V6 check that *paths* exist; nothing checked that cited *symbols* exist
(`zref::CmdDma`), that *contract-cited* paths exist (the four phantom random files lived in
`design/contracts/*.md`, which V6 never reads — see §5), that a cited proof ever *ran*
(pre-V16), or that a constant's stated justification is a derivation rather than a
cross-reference (`REFRESH_URGENT = 40`). Maturity promotion is self-service: the same
agent writes the RTL, the tests, the evidence, and the badge, in the same session
(`maturity_log` for MEM.VRAM.ARBITER: three entries, all commit `6bcc4e9`, same day). Some
evidence pins were literally copy-pasted between blocks (`dc3eac0`).

**(d) Specific to how agents work here.** Agents complete checklists fluently as *text*.
Charter §21 names 15 steps and ends "No step is skipped because the output looks correct" —
but a step recorded as done is indistinguishable, on the recording surface, from a step
done. Session kills (the 429 kills logged 2026-08-15), parked branches, and salvage merges
create pressure to record *intended* state; the W2.2 worktree contained a scratch script
that would have blind-promoted all four VIDEO blocks in lockstep (TASK_LOG, video-merge
section). Agents also faithfully *propagate* prose claims across layers: the wave-1 recon
FINDINGS (`RUN-20260814-1852-wave1-fixed-point/FINDINGS.md` §3) already contained the
strict-`>` fill rule *while citing the D3D convention it contradicts*; the spec copied it,
the raster implemented it faithfully, and it survived until `57f1639`. Small but telling:
this run's own `TASK_LOG.md` still opens with "[Describe objective here]" and
`ARCHIVE.md` contains only the template — the process's own record-keeping carries
unexecuted boilerplate too.

### Hypothesis stress-test — what does *not* fit

A theory that explains everything explains nothing, so:

- **D13 (teardown deadlock) does not fit the disease.** There the verification act was
  *real and complete* — every check passed — and the *transport* destroyed the verdict.
  That is a genuine toolchain defect (cause (b)), not an unfalsifiable claim. It belongs
  in the diagnosis only through its corollary: prior green fast lanes were intermittently
  one hang from timeout, i.e. the *lane's* health record was luck. The fix (`zhao::exit_hard`,
  `7ae6b3b`) is correct and complete for this strain.
- **Half of incident 2 does not fit.** "B = 40 was wrong twice in ways that partly
  cancelled" is an ordinary derivation error — smart people make those, and the proof,
  once actually elaborated, *did* fail at 40 (`9d49806`). The disease explains why the
  wrong number *survived a wave*, not why it existed. The distinction matters for fixes:
  no ledger rule prevents a wrong derivation; only "prove tight in both directions,
  derived-then-confirmed" does, and that discipline already worked when applied.
- **Class E does not fit and should not be forced to.** Five-hour merges, orphaned
  solvers, and agents parking between waits are process *economics*, not evidence
  *epistemics*. They share only this link: expensive verification raises the temptation to
  skip it, which feeds the disease. Treated separately in §6 (lane hygiene).

---

## 2. The evidence trail, verified

| # | Claim in the brief | Verdict against the repo |
|---|---|---|
| A1 | MEM.GUARD "Formally proven", never elaborated, vacuous, real `blit_span` wrap escape | **Confirmed** — `1bc42a5` message + diff; CRITICAL-3 in `REVIEW-FABLE-ALL-WAVES.md` |
| A2 | Arbiter B=40 wrong twice, partly cancelling; proven tight at `9d49806` | **Confirmed** — `9d49806`, ledger regression_reason, `formal_runs.yml` (tight both directions: fails at 51/33, passes at 52/34) |
| A3 | INPUT.SNAPSHOT RTL_VERIFIED, no cover, all assertions `$past(frame_tick.pulse)`-guarded; caught by V16 | **Confirmed** — `formal_runs.yml` entry; RATIFICATION-refresh-urgent §"carried forward" |
| A4 | Phantom `zref::CmdDma`/`Crc32c`/`framePixelCrc` + four phantom test files | **Confirmed, one correction** — `768ce1a`. The four phantom file citations lived in `design/contracts/*.md`, **not** in `blocks.yml` — which is why V6 (which does existence-check all four `tests.*` paths past SPECIFIED) never fired. See §5. |
| A5 | `cmd_dma_crc_gate` needed ≥153,600 B vs 64-B harness buffer | **Confirmed** — `3ca4661` message; `formal_runs.yml` notes |
| A6 | linebuf stimulus folded pre-`setundef`; buffer 1 unfillable; earlier pass void | **Confirmed** — `e842cc0`, TASK_LOG video section (found via unreached cover `c_fresh_both`) |
| B7 | "validated upstream" false; lawless mode byte indexed `ZHAO_TIMING` OOB | **Confirmed** — `768ce1a` (caught by 100k soak: RTL published DUO, oracle 0x78) |
| B8 | "toggle-free by construction" false for FULL; real CDC hazard | **Confirmed** — `e842cc0`; header now names the true enforcer (vblank-only aborts) |
| C9 | REFRESH_URGENT=40 justified by cross-reference; dissolved; derivation still owed | **Confirmed** — RATIFICATION doc; still item 4 in QUEUE-next-waves |
| C10 | Duo displayed 245,760 = allocation coincidence; SV/C++ disagreed and merged cleanly | **Confirmed** — `a7e5964` (three numbers, three names), `0b8c71c` (§29-6 delegation) |
| D11 | Nightly/soak never run; fast lanes passed by PCG luck; 2 real RTL defects | **Confirmed** — `768ce1a`. Mechanism note: CI *has* a scheduled nightly job (`ci.yml` cron 03:00 UTC), but work sat on parked branches the cron never sees — never-run lanes are a structural consequence of the parked-branch workflow, not just forgetfulness. |
| D12 | Random lanes failed only on nondeterminism | **Confirmed** — `20c5cb3`; all four salvaged tests ignored `tb.failures` |
| D13 | Teardown deadlock; verdict stranded; pre-existing | **Confirmed** — `7ae6b3b`; TASK_LOG records `mem_sdram_directed` dying identically on a tree without the merge |
| D14 | format_check SKIPped, counted as pass | **Confirmed locally** — `tests/CMakeLists.txt:239-251`. Mitigation exists: the CI `format` job (pinned LLVM 15) is the authoritative gate. The misleading artifact is the local "100%". |
| E15 | ~5 h merge, most on one property | **Corroborated by timestamps** — W2.6 done 04:04, salvage 04:46, merge 04:47, final ledger 08:55 ≈ 4.8 h; the linebuf property spans 05:41 → 08:48 ≈ 3.1 h of it. "Nine harness revisions" is not reconstructible from the squashed commits — plausible, unverified. |
| E16 | Orphaned `abc pdr`, 95 min | **Not independently verifiable** — only the standing-constraint note in QUEUE-next-waves records the phenomenon; no log with the 95-minute figure survives in the archive. |
| E17 | Agents park after background waits | Orchestration-level; not recorded in the repo; taken as owner report. |

---

## 3. Which existing countermeasures actually work — and why

The record shows the project has *already invented* every element of the cure, piecemeal,
under fire. The working patterns share one property: **they force a claim to be coupled to
an event that can fail.**

1. **V16 (formal-run registry)** — works, and the record proves it: caught INPUT.SNAPSHOT
   at introduction (`d997ced` era), then ran *pre-badge* on the W2.6 merge exactly as
   designed ("fourth contact, first pre-badge catch", TASK_LOG), forcing the discovery of
   A5 and the two RTL law defects behind D11. Its two deep design virtues: (i) it inverts
   the default — an unregistered `.sby` is a *hard failure*, so absence of evidence is
   visible; (ii) `covers: true` is a falsifiability requirement, not paperwork — every
   incident it caught, it caught *through* a cover that could not be reached.
2. **Verified-by-negative-probe** — V16 itself was tested by flipping the arbiter entry to
   `never_ran` and confirming `ledger:check` goes red (TASK_LOG W2.5 section). This is the
   pattern everything else should copy: a checker is not evidence until *it* has been seen
   to fail.
3. **Mutation verification of proofs** — MEM.GUARD's four mutations each caught by the
   semantically right assertion (`1bc42a5`); AUDIO.FIFO's `wr_ready=1` mutation exposing
   the single-clock vacuity (TASK_LOG W2.4). Ad hoc, unscripted, but it is the only
   technique in the record that detects "a lane that cannot fail" *directly*.
4. **Tight-both-directions bisection** — pass at B, fail at B−1, adjacent (`9d49806`).
   Kills "adopt the number that passed" for bounds.
5. **Self-asserting scope guards** — `a_horizon_is_refresh_free`
   (`formal_mem_arbiter.sv:222`), `a_scope_four_sessions` (`video_linebuf_fv.sv:242`).
   A bounded proof that *states its own scope as an assertion* converts silent scope drift
   into a red.
6. **Honest registry states** — `banked` with measurements instead of a manufactured
   timeout (`mem_sdram_refresh_bound`); `covers: false` recorded plainly for
   `audio_fifo_bounds` with the ratchet documented. The registry can tell the truth about
   partial evidence, which removes the incentive to fake completeness.

What has *not* worked: prose. Every unenforced sentence in the record — "Formally
proven.", "validated upstream", "toggle-free by construction", "= the arbiter liveness
bound", "every committed wave-2 capture" (REVIEW minor 8: the directory does not exist) —
eventually diverged from the machine. The charter's §21 "no step is skipped" is itself
prose with no enforcer.

---

## 4. V17 assessed honestly, per incident

V17 as proposed (TASK_LOG §"phantom-pointer door"): (1) `reference_model` symbol must have
a definition hit under `reference/` for blocks ≥ REFERENCE_COMPLETE; (2) cited `tests.*`
paths must exist for blocks ≥ UNIT_VERIFIED; (3) optionally, contract "Scalar reference
function" symbol must match the ledger's `reference_model`.

| Incident | Would V17 have caught it? |
|---|---|
| A1 MEM.GUARD vacuous proof | **No** (V16 territory; V16 caught it) |
| A2 B=40 wrong bound | **No** — and *nothing mechanical* catches a wrong derivation whose proof was never run except V16; nothing at all catches one whose proof passes. Only the tight-bisection discipline does. |
| A3 no-cover vacuity | **No** (V16 `covers: true` caught it) |
| A4 phantom symbols | **Partially yes.** V17.1 catches `zref::CmdDma`/`zref::Crc32c` (they were in `blocks.yml reference_model`). `zref::framePixelCrc` lived in a *contract* → needs V17.3. The four phantom test files lived in *contracts* → **V17.2 as drafted misses them entirely**, because it re-checks `blocks.yml` paths that V6 already gates (see §5). It also catches the near-miss `zref::Scanout`-vs-`VideoSys` naming drift the video merge fixed pre-promotion (`3406850` notes). |
| A5 unreachable-in-cone assertion | **No** — the symbol and files all existed; only a cover targeting the gate finds it (V16 did). |
| A6 folded stimulus | **No** — same; found via an unreached cover. |
| B7 / B8 prose claims | **No** — comments are invisible to V17. |
| C9 / C10 constants | **No** — `zref::` symbols existed; the *numbers* were the lie. |
| D11–D14 lanes | **No** — V17 knows nothing about whether tests ran. |

**Net: V17 would have caught exactly one of the fourteen incidents (A4), and only fully if
extended to read contracts.** That is still worth its ~half-day cost — A4 was seven
distinct phantom citations, and the check is nearly free forever after — but V17 must not
be sold as the systemic fix. Its structural blind spots, in order of danger:

1. **Existence ≠ substance.** A stub `struct CmdDma {}` satisfies the regex.
   MEM.HPS.BRIDGE already demonstrated the aliasing variant: `tests.random` pointed at a
   real file that never instantiated the bridge (REVIEW recap MAJOR; fixed `dc3eac0`).
   Path/symbol existence checks are satisfied by files *about something else*.
2. **It checks the ledger's pointers, not the world's** — comments, spec cross-references,
   and constants stay unexamined.
3. **It cannot see whether anything ran** — that is V16's job for formal and nobody's job
   for sim lanes (fix 2 below).

---

## 5. Correction that redirects V17.2 — where the phantom files actually lived

`rules.ts` V6 (lines 158–170) already existence-checks `tests.directed/random/formal/unit`
for every block past SPECIFIED. The W2.6 phantoms survived because they were cited in
`design/contracts/CMD.*.md` / `DEBUG.*.md` — free text the ledger never parses
(`768ce1a`'s diff for that portion touches only the four contract files). So:

- **V17.2 as drafted is ~redundant with V6** (its only addition would be gating at
  UNIT_VERIFIED specifically, which V6's `!= SPECIFIED` already subsumes).
- The real gap is **contract ↔ ledger coherence**: contracts have a fixed 17-heading
  structure (`## Scalar reference function` at line 47 in each — verified for
  MEM.VRAM.ARBITER and CMD.DMA), so a rule can mechanically extract the backticked symbol
  under that heading and every `tests/...` path in the contract body, and require: symbol
  == block's `reference_model`; every cited repo path exists. That catches all seven A4
  phantoms including the three that V17-as-drafted misses.

---

## 6. Fix catalogue — what it catches, costs, would-have-prevented, how it fails

Ranked by (incidents prevented ÷ cost). "Prevented" counts recurrence of the recorded
strains; costs are estimated in agent-hours from the observed costs of comparable work in
this repo (V15 + tests was part of one commit; V16 + registry + negative probes was part
of one session). Where I cannot ground a cost, I say so.

### R1. V17+ — citation resolution, extended to contracts (supersedes V17.1–.3)
- **What:** ledger rule: (a) `reference_model` last-segment must have a definition hit in
  `reference/**` (regex per the TASK_LOG proposal, injected like `formalTasksOnDisk`);
  (b) contract §"Scalar reference function" symbol must equal `reference_model`;
  (c) every `tests/...`-shaped path appearing in a block's contract must exist on disk;
  (d) *anti-alias tie*: the file at `tests.directed`/`tests.random` must textually
  reference the `reference_model` symbol's last segment (one grep) — this is what
  the MEM.HPS.BRIDGE alias would have failed.
- **Catches:** all of A4 (7 phantoms); the HPS-bridge alias class; naming drift like
  `zref::Scanout`/`VideoSys`.
- **Cost:** ~half a day incl. unit tests (V15/V16 precedent). Runs in the existing
  `ledger:check` CTest, so it costs nothing recurring.
- **Would have prevented:** incident 4 in full; the `dc3eac0` alias MAJOR.
- **How it fails/is gamed:** stub symbols; a test that merely *names* the oracle without
  differentialing against it. It raises the effort of lying from "type a name" to "write
  a fake file", which is the most that static checks buy. Accept that; R4 covers the rest.

### R2. Lane-run registry — V16 generalised to sim lanes ("V18")
- **What:** `design/lane_runs.yml`: one entry per CTest lane (fast / formal / nightly /
  soak), recording date, commit, pass/skip/fail counts, and the log path. Wrapper-appended
  (a ctest `--output-junit` post-step), not hand-written. Ledger rules: (a) a maturity
  entry ≥ UNIT_VERIFIED whose evidence is a test path requires a recorded run of a lane
  *containing that test*, at or after the evidence commit, with that test in the pass
  column (not skip); (b) any test registered with a `nightly`/`soak` label and zero
  recorded runs is a warning, hard failure once cited as evidence — exactly V16(e).
- **Catches:** D11 (never-run lanes — the disease's largest single yield: two real RTL
  defects); D14's misleading side (skips become first-class, "100%" stops being quotable
  because the registry stores three numbers, not one).
- **Cost:** ~1 day (wrapper + rule + backfill from the runs already recorded in TASK_LOGs).
  Recurring cost near zero — the wrapper writes the entry.
- **Would have prevented:** incident 11; the CMD/DEBUG promotions would have failed
  `ledger:check` at the branch, pre-merge, the same way V16 fired.
- **How it fails/is gamed:** an agent can hand-edit the YAML — same trust model as V16,
  same mitigation (entries carry the log path; the negative-probe habit spot-checks).
  Structural residual: the parked-branch workflow keeps branch lanes out of CI cron;
  merge briefs must keep the "run the branch's own nightlies at merge" rule (it is what
  found B7 and the CRC drop).

### R3. Prose-claim lint — "by construction" must name its enforcer
- **What:** repo lint (a `ledger:check` sub-rule or a standalone fast test): any
  RTL/header/contract line matching `by construction|validated upstream|guaranteed
  by|cannot happen|never occurs` must be followed within N lines by `ENFORCED-BY:
  <path-or-property-or-assertion>`; the checker resolves the path/property (formal
  properties resolve against `formal_runs.yml`, giving transitivity with V16). Existing
  hits: 14 comment-sites across 7 RTL files, of which only `zhao_pkg.sv` and
  `zhao_scanout_linebuf.sv` currently carry an enforcer annotation — so adopting this
  costs ~a dozen annotations, each of which is *exactly the audit the incidents say we
  need* (B7 and B8 were both found by asking "who enforces this sentence?").
- **Catches:** recurrence of B7/B8; converts Class B from review-only to mechanical.
- **Cost:** ~2–3 h for the lint; ~2–4 h to annotate or fix the 12 unannotated sites
  (some annotations will *fail to be writable* — those are live B-class defects, which is
  the point).
- **How it fails/is gamed:** naming an irrelevant enforcer. The citation is at least
  resolvable and reviewable; a reviewer checking one pointer is far cheaper than a
  reviewer re-deriving an invariant.

### R4. Falsification probe at promotion — "the lane must be seen to fail" (the deep fix)
- **What:** promotion to UNIT_VERIFIED or RTL_VERIFIED requires one committed,
  re-runnable negative probe per headline law: a `-DZHAO_MUTATION_<name>` build (or a
  formal mutation task) that must turn the cited lane red, recorded in the registry
  (`mutations:` field alongside `covers:`). The probe targets the *contract's headline
  law*, named in the ledger entry, not an arbitrary line.
- **Catches:** the entire "act incapable of failing" family — A1 (vacuous assertions), A3,
  A5, A6, D12 — which no static check reaches. Note that *every* incident in that family
  was in fact found by an ad-hoc member of this technique (a cover that would not reach, a
  mutation that would not fail); R4 just makes the discovery systematic instead of lucky.
- **Cost:** the real one. Grounded estimate: MEM.GUARD's four mutations and AUDIO.FIFO's
  two were done inside single sessions alongside other work — call it 1–3 h per block
  headline law. For the 14 blocks currently ≥ UNIT_VERIFIED: roughly 2–4 agent-days if
  done as a sweep; near-zero marginal if required only *at* future promotions.
- **Would have prevented:** 1, 3, 5, 6, 12 — five of the fourteen, including both classes
  V16 was built from. This is the best defects-per-effort of the deep fixes, and the only
  one that addresses the root cause (falsifiability) rather than a symptom surface.
- **How it fails/is gamed:** choosing a trivial mutation the lane catches while the deep
  property stays untested. Mitigations: the mutation must be in the *law named by the
  ledger entry*; review checks the pairing once per block, which is cheap. Residual risk
  accepted — this check bounds the lie, it cannot eliminate it.

### R5. Bounded-proof scope guards, made a rule
- **What:** every `.sby` whose load-bearing task is `bmc` (not `prove`) must contain a
  harness assertion named `a_scope_*` stating the structural fact that makes the bound
  meaningful (the `a_horizon_is_refresh_free` / `a_scope_four_sessions` pattern), or a
  header waiver saying why scope is total. Enforced as a grep-level rule in `ledger:check`
  (`.sby` → harness file → `a_scope_` present).
- **Current debt:** exactly two harnesses — `video_framectl_fv.sv` (depth 60 ≈ 4 abstract
  frame periods: the scope claim "4 periods suffice for the counting laws" is currently a
  comment) and `formal_mem_guard.sv` (depth 30; scope claim: single-request window).
- **Answer to the owner's direct question — yes, every bounded proof should carry one**,
  because the failure it prevents (someone raises the depth or the parameters and the
  numbers silently change meaning) is precisely a Class-A pattern, and the cost is a few
  lines per harness. The pattern's virtue is that it is *self*-asserting: the guard
  travels with the proof, not with a wiki page.
- **Cost:** ~1 h rule + ~1 h for the two guards. **Catches:** future silent scope drift;
  would not have caught any *past* incident except as hardening of A2's aftermath.

### R6. Constants: parity manifest + derivation-shaped justifications (Class C)
- **What, part 1 (mechanisable):** extend the existing §19 machinery (`abi:check`,
  `tables:check`) with a *constants manifest*: every law constant that exists in more than
  one language (canvas/displayed/allocation bytes per mode, arbiter bounds, REFRESH_*,
  FIFO geometry, timing tables) is emitted from one source and byte-compared across
  C++/TS/SV. The Duo divergence (C10, `0b8c71c`) merged cleanly precisely because
  `zhao_canvas_bytes` lived *outside* the generated surface. This is the §19 promise
  ("byte-identical across C++/TS/SV") applied to constants, where it demonstrably lapsed.
- **What, part 2 (semi-mechanisable):** a constants ledger for *frozen numeric law
  constants only* (the zhao_pkg/params_pkg localparams): each carries a `derivation:`
  (formula + inputs) and a trivial evaluator test recomputing it. A lint flags any
  constant whose derivation references another constant's *name* with no formula — which
  is literally what `REFRESH_URGENT: "= the arbiter liveness bound"` was.
- **Is Class C detectable mechanically? Partially.** The *coincidence* (two numbers equal
  for unrelated reasons) is not machine-detectable — no checker knows the reasons. What is
  machine-checkable is the *shape of the justification*: derivation-from-inputs vs
  cross-reference. Forcing the shape makes the rationalisation visible to a reviewer in
  one glance. The judgment stays human; the surface becomes 10 lines instead of a repo.
- **Cost:** part 1 ~1 day (enumerate + wire into the generated outputs — the generator and
  check scaffolding already exist); part 2 ~1 day for the ~dozen frozen law constants.
- **Would have prevented:** C10's silent SV/C++ divergence outright; C9's rationalisation
  would have been *visible* at freeze time (not provably prevented — someone still had to
  ask for the tREF derivation).

### R7. Harness soundness — detecting folded stimulus and unreachable properties generically
Three layers, cheapest first:
- **(a) Covers per assertion-precondition (house style, already de facto post-V16).**
  This is what actually found A5 and A6. Make it explicit in the formal harness template:
  every `assert (A |-> C)` pairs with `cover (A)`. V16's `covers: true` is per-*property*;
  the discipline should be per-*assertion*. Cost: template + review habit; no tooling.
- **(b) Structural cone check (mechanical, this-toolchain-specific).** After `prep`, run a
  yosys script asserting every harness top-level input port remains in the fan-in cone of
  at least one `$assert`/`$cover` cell (`select -assert-any` over the cone). The
  `read_slang` 1'x-fold (A6) deletes exactly this connectivity, so the check catches the
  fold *structurally* instead of by cover luck; it also catches an accidentally-constant
  port. Cost: ~half a day to prototype against the linebuf harness (the known-bad example
  exists in history at `6a795ea` for a regression test), then a line in each `.sby`.
  Fragile across tool updates — pin it to the suite version, and let it fail loudly.
- **(c) Cover-count assertion in the wrapper.** The sby wrapper already greps for status;
  additionally assert the *number* of reached cover points equals the expected count
  declared in the `.sby` header. A silently dropped cover (optimised away) then fails the
  lane instead of vanishing. Cost: ~1 h.
- **Catches:** A6's whole class; degraded-harness drift over time. **Would have
  prevented:** 6 (and likely shortened 15, since the 3-hour linebuf battle was mostly
  *discovering* the fold).

### R8. Lane hygiene bundle (Class D/E residue)
- **(a) Skip discipline:** a fast-lane wrapper that prints pass/skip/fail separately and
  fails if a *skip allowlist* is exceeded (today: `format_check` on machines without
  clang-format, formal on machines without the suite). Cost ~2 h. Prevents D14's local
  illusion; CI already carries the authoritative format gate.
- **(b) Orphan sweep:** the formal lane epilogue lists surviving `yosys`/`abc`/`btormc`/
  `smtbmc` processes parented to dead `sby` and fails loudly (or kills them, stated).
  Cost ~2 h. Prevents E16 recurrence.
- **(c) Exit protocol:** done (`zhao::exit_hard`, `7ae6b3b`) — verify by policy that every
  *new* Verilated main uses it; a grep-lint (~30 min) closes it permanently.
- **(d) Timeouts state their meaning:** any CTest timeout in a formal/soak lane is
  reported as "TIMEOUT (verdict unknown)", never as failure-with-no-output — cosmetic but
  it is what confused the D13 triage twice.

### R9. Maturity ladder: demotion trigger, and evidence independence
- **(a) Demotion trigger (cheap, do it):** codify what the arbiter ratification already
  did by hand — any `ledger:check` violation against a block's *cited* evidence
  (V16/V17/V18 class) auto-demotes the block one rung in the same commit that records the
  finding, with `regression_reason`. The ledger schema already supports regressions (V2
  checks `regression_reason`); this only makes the response non-discretionary. Cost: ~2 h
  rule + doc.
- **(b) A new rung is NOT recommended.** The ladder's rungs are fine; the failures were
  never "the ladder lacks a state", they were "the state was entered on false evidence".
  Adding EVIDENCE_AUDITED would add a field agents fill in fluently — the disease's
  favourite food.
- **(c) Evidence independence (the real gap, costliest):** promotion to RTL_VERIFIED
  requires the evidence run to be *observed by a different session/agent than authored the
  RTL* — i.e. the owner's original loop's QA pass, recorded as `verified_by:` distinct
  from the authoring run ID. This is weakly enforceable (honor system on IDs), but the
  *act* it forces — a second context looking at the lane — is what caught nearly
  everything in this record (every Class A–D discovery happened in a merge/review/fix
  session, never in the authoring session). Cost: process change, not tooling; roughly
  one extra agent-pass per block cluster, which the W2.6/W2.2 merges show is ~2–5 h each.

### Ranking (defects-prevented ÷ cost)

| Rank | Fix | Prevented (from the list) | Cost |
|---|---|---|---|
| 1 | R3 prose-claim lint | B7, B8 class | ~half day |
| 2 | R1 V17+ (contract-aware) | A4 (7 phantoms) + alias class | ~half day |
| 3 | R5 scope-guard rule | A2-aftermath class | ~2 h |
| 4 | R2 lane-run registry | D11 (2 real RTL bugs), D14 | ~1 day |
| 5 | R4 falsification probe | A1, A3, A5, A6, D12 | 2–4 days sweep, ~0 marginal at future promotions |
| 6 | R9a demotion trigger | non-discretionary response | ~2 h |
| 7 | R8 hygiene bundle | D13/D14/E16 residue | ~1 day total |
| 8 | R6 constants manifest | C10, C9 surface | ~2 days |
| 9 | R7b/c cone + cover-count | A6 class structurally | ~1 day |
| 10 | R9c evidence independence | everything, weakly | recurring process cost |

R4 ranks below R1–R3 only on cost; on *defects prevented* it is first by a wide margin
(five incidents, including both that spawned V16). If the owner funds exactly one
non-trivial item, fund R4.

---

## 7. Cheap wins vs deep fixes

**Do now (≈ 2–3 agent-days total, all before the next promotion lands):**
- R1 (V17+, contract-aware — *not* the drafted V17.2), R3, R5, R9a — four ledger/lint
  rules in the existing `rules.ts` pattern with unit tests and negative probes.
- R2 lane-run registry with wrapper-appended entries; backfill from TASK_LOG numbers.
- R8a/c skip-allowlist and exit-hard grep-lint (b, the orphan sweep, with the next formal
  session).
- Write the two missing scope guards (framectl, mem_guard).

**Before Phase 11 (RTL freeze copies semantics into hardware — last cheap moment):**
- R4 falsification-probe sweep over the 14 blocks ≥ UNIT_VERIFIED, headline law each.
- R6 constants manifest (part 1 at least — the §19 lapse is exactly the kind of thing
  Phase 11 freezes into silicon-equivalent).
- R7b structural cone check, prototyped against the archived bad linebuf harness.
- R9c: institute the QA-pass-before-RTL_VERIFIED rule going forward, and run the wave-1
  QA/test-writer wave (QUEUE-next-waves item 6) — see §8 for how to sample it first.
- The owed `REFRESH_URGENT` derivation (already queued; it is also R6's pilot case).

**Not worth doing (say no):**
- Industrial vacuity-checking research tooling, k-induction everywhere, or proving the
  depth-900 refresh BMC — `banked` with measurements is the correct state for it.
- A new maturity rung or an "evidence audit" document per block — fields get filled.
- Auditing all 68 SPECIFIED blocks — they carry no evidence to be false; their risk is
  spec-level (fill-rule class) and is better caught by the implementing wave's oracle
  differential than by re-reading prose now.
- Any scheme that requires agents to attest harder (oaths in commit messages, double
  sign-offs in prose). The disease *is* attestations; more attestation is more disease.

---

## 8. Honest assessment of scale, and the sampling plan

**The observed base rate.** Seven block-clusters have so far been examined properly (merge
review, adversarial review, or forced-real formal): MEM (4 blocks), INPUT (2), AUDIO (1),
CMD/DEBUG (4), VIDEO (4), the W3.5 software renderer, and the wave-1 numeric core.
**Seven of seven yielded evidence-integrity defects; five of seven also yielded genuine
functional defects** (MEM: blit_span escape + wrong bound; CMD/DEBUG: mode-byte and
CRC-drop; VIDEO: the CDC hazard + wrong Duo constant; W3.5: sky drum + sun alpha;
wave-1 core: SPLINE C1, unit8 C2, fx_sin OOB, fill rule). The two that yielded only
integrity defects (INPUT, AUDIO) are also the two whose authoring sessions *ran their own
nightly lanes* (TASK_LOG W2.3/W2.4 record the soaks green in-wave) — consistent with the
theory: where the acts really happened, the blocks were sound; where only the claims
happened, defects pooled.

**Estimate for the unexamined remainder.** At cluster granularity the point estimate is
that **essentially every unexamined cluster carries at least one evidence-integrity
defect** (7/7 observed; even the most conservative reading of seven-for-seven puts the
underlying rate above one in three at 95% confidence), and **roughly half to two-thirds
carry a genuine functional defect** that a sound lane would catch (5/7 observed). The
principal unexamined surface, in risk order:

1. **Wave-1 tooling at UNIT_VERIFIED** — SW.TOOLS.CAPTURE, SW.TOOLS.LEDGER, SW.TOOLS.ABIDOC
   — never received the QA/test-writer passes (QUEUE-next-waves item 6). These are
   *evidence-producing* tools: a defect here multiplies (a capture/replay bug makes every
   downstream golden self-confirming; one latent v1 abi-gen pad bug is already on record,
   found only when W2.1 touched it). Highest priority.
2. **Wave-1 REFERENCE_COMPLETE software** — SW.ZREF, SW.FIELDIR — partially audited (the
   two Fable reviews hand-verified the numeric core and corpus parity), so the *arithmetic*
   is likely sound; the un-reviewed remainder is test-lane soundness (do the random lanes
   fail on mismatch? D12 was found in *salvaged* tests, and nobody has asked the question
   of the wave-1 lanes).
3. **The two wave-2 blocks never re-examined after authoring** — INPUT.RUMBLE and
   DEBUG.COUNTERS ("genuinely green unmodified" at the merge — i.e. examined least).

**Sampling plan (≈ 1.5 agent-days, instead of auditing everything):**

1. **Free census first.** Land R1/R2/R3/R5 and run `ledger:check` over all 88 blocks.
   Every flag is a *measured* integrity defect at zero marginal cost; this converts most
   of the estimate above from projection to count, for the citation/lane classes.
2. **Stratified deep sample, 3 blocks × ~2 h each,** chosen for information yield, one
   per stratum: **SW.TOOLS.CAPTURE** (stratum 1 — evidence-producing tool, everything
   downstream leans on it), **SW.FIELDIR** (stratum 2 — lane-soundness question on
   corpus parity), **INPUT.RUMBLE** (stratum 3 — least-examined RTL). For each, three
   probes, all pass/fail: (a) resolve every citation in ledger + contract (R1 by hand if
   not yet landed); (b) run its lanes once *including* nightly/soak flags, record
   pass/skip/fail; (c) one falsification probe against the headline law — mutate, expect
   red. **Do not run these while the background formal lane is active.**
3. **Decision rule, stated in advance.** 0 of 3 blocks defective → the mechanical rules
   plus at-promotion probes suffice; fold the wave-1 QA wave into normal Phase-11 prep.
   1 of 3 → schedule the wave-1 QA wave as queued, ordinary priority. 2 or 3 of 3 → the
   base rate holds everywhere; the QA/test-writer wave jumps the queue ahead of new
   feature waves (it will be finding real RTL/law defects, as it did both times it ran).
4. **One targeted extra:** enumerate CTest tests carrying `nightly`/`soak` labels and
   diff against every lane run recorded anywhere in the TASK_LOGs — a 20-minute census
   that directly measures the D11 residue ("which lanes have *never* run, anywhere").

---

## 9. Corrections to the brief's framing

Mostly the brief survives contact with the repo. The deltas, plainly:

1. **Incident 4 / V17.2:** the four phantom test files were cited in
   `design/contracts/*.md`, not `design/blocks.yml` — `blocks.yml` paths were already
   existence-gated by V6. V17.2 as drafted would therefore have caught **none** of them;
   the fix must parse contracts (§5). This is the one place the proposed remedy misses
   its own motivating incident.
2. **Incident 14 is half-mitigated already:** CI's `format` job (pinned LLVM 15) is the
   authoritative gate and runs on every push; the misleading artifact is only the *local*
   ctest "100%". Fix the local reporting, don't rebuild the gate.
3. **Incident 11's mechanism is structural, not negligence:** CI has had a nightly cron
   since the M1 review fixes — but the work sat on parked branches that scheduled CI never
   sees. Never-run lanes are a *consequence of the parked-branch workflow*; the merge-time
   "run the branch's own nightlies" rule is the load-bearing countermeasure, and it should
   be stated as such in merge briefs (it currently is, in QUEUE-parked-branch-merges).
4. **Incident 15's numbers check out approximately:** ≈ 4.8 h wall-clock (04:04 → 08:55 by
   commit timestamps), ≈ 3.1 h of it on the linebuf property. "Nine harness revisions" is
   not reconstructible from the squashed history — plausible, unverified.
5. **Incident 16 (95-minute orphaned pdr) is not verifiable from the archive** — only the
   standing-constraint note attests the phenomenon. I believe it; I cannot cite it.
6. **Incident 13's "every prior green fast lane was one coin-flip from hanging"** is
   rhetorically strong but directionally supported: the TASK_LOG records the deadlock as
   intermittent and pre-existing (`mem_sdram_directed` died identically on a tree without
   the video merge). "Intermittently exposed" is the defensible claim.
7. **One small credit the brief under-claims:** incident 2's correction discipline
   (derive, then bisect tight in both directions) and incident 9's dissolution are not
   just fixes — they are two of the six *working patterns* (§3) the systemic fixes should
   generalise. The project has repeatedly invented the right countermeasure locally; the
   failure has been generalising it before the next strain appeared. That, more than any
   single rule, is what R1–R9 are for: they are the already-proven patterns, promoted from
   habit to law.
