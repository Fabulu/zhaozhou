# TASK LOG — Manafold pass 10, IMPLEMENTER

Run `RUN-20260906-1446-manafold-pass10-impl`. Lane
`C:\programmieren\zencrifice\manafold-p10-impl\{zhaozhou,Upheaval}`, both cloned
fresh from `origin/main` (zhaozhou `c148514b`, Upheaval `79e6e18`).

Plan followed: `Upheaval/creature/Manafold/PASS-10-ARCHITECTURE.md` (five stages,
serial). Architect's cut order if squeezed: **D.3, B.3, D.2 — never A or C.**

## Where I am (update BEFORE reading any long-running result)

- [x] Lane cloned, run initialised
- [ ] Baseline build + baseline render (needed by everything)
- [ ] C.2 prototype (go/no-go, runs FIRST per R1)
- [ ] Stage 0 — instruments that cannot lie
- [ ] Stage A — silhouette exclusion (THE SPINE)
- [ ] Stage B — plane lifecycle
- [ ] Stage C — antenna stage 1
- [ ] Stage D — eyes
- [ ] Close: re-render, re-encode, archive, publish, findings

## Journal

### 14:46 — oriented
Read in full: `PASS-10-ARCHITECTURE.md`, `09-ENGINE-GOTCHAS.md` (§§0–17),
`10-GATE-CHECKLIST.md` (30 items). Digests of `OWNER-DIRECTION-7`,
`PASS-9-REVIEW`, `PASS-9-QA` and `PASS-10-INPUTS` commissioned in parallel.

Code read against the plan's claims before touching anything:
- `zhao_reel.cpp:3323–3356` — the mist follow arithmetic, inline, exactly where
  the architect said (0.1's target). Confirmed: nothing to call, so no probe can
  reach it. `mist_shift` IS callable; the residual/`follow_pm` scaling is not.
- `manafold_fx.h:1973–2030` — `mist_composite`. Confirmed the A.1 fault site:
  the only rejection is `cell_d > frame_depth[...]` (line 2007). A creature
  pixel whose splat sat nearer is composited over. There is no coverage input
  at all, which is why no density value can fix it.
- `manafold_fx.h:490–591` — the mist constants and the variant table.
  `kMistFollowPm = 820`; the `parked` variant is follow 0.

### 15:0x — STAGE C.2 PROTOTYPE: **NO-GO**. The stop rule fires.

Run first thing in the pass, as R1 requires. `tools/reel/manafold_c2proto.cpp`,
committed, wired into `build-direct.sh` as `mc2proto`. Probe-only: it touches no
rig, no mesh, no shipped constant.

**The instrument corroborated itself before reporting anything.** Leg 1
reconstructs the CURRENT single-segment arm analytically and reproduces the
committed probe's published sweep worst at **989 pm, drift 0**. Without that the
rest would be void, and the probe says so and exits 2.

**Form A — the architecture's design (two-link residual closure, tip lands ON
the posed anchor): NO-GO, but not for the predicted reason.** The rim is
*comfortable* — 564 pm against a 1120 gate. The blocker is **REACH**: hinge D's
pivot sits **924..1575 mm** from the posed anchor and a 630+640 chain reaches at
most **1270**, so **9 of 24 fold scales cannot close at all**. This also explains
the shipped 989: today's arm does not land on the anchor either — it aims and
falls short, and 989 pm *is* that shortfall.

**Form B — the reachable form (D aims exactly as today, so closure is untouched
at every fold scale; the re-entry joint carries a bounded authored bend):
7.5 degrees.** Below the ~10 degree floor for a bend to read at 240p on a 70 mm
strut. **NO-GO.**

**⚠ THE NUMBER I ALMOST SHIPPED WAS 27.5 DEGREES — a GO.** Leg 3's unreachable
branch used `continue`, which also skipped leg 4, so Form B was silently measured
over only the 15 REACHABLE fold scales — missing fold 700, where the worst rim
lives. It was caught by one thing: **Form B at zero bend must equal the
straight-strut control by construction, and it read 545 against 991.** That
identity is now an assertion in the probe (`IDENTITY CHECK`, drift <= 4, exits 2
on failure) so the next change cannot lose it. Gotcha section 16 exactly: the
half-fixed number was plausible, alarming in the right direction, and actionable.

**The structural finding for pass 11, which is the real deliverable here:** the
straight strut already measures **991 pm against a 1120 pm gate — 129 pm of total
headroom.** There is no room at the re-entry ball for *any* visible joint. The
lever is the arm/anchor geometry (reach), not the joint's design. Pass 9 swept arm
length and anchor against the *single-joint* form; nobody has swept them against a
*two-segment* form, and Form A's comfortable 564 pm says that sweep is where a
pass-11 design round should start.

**Consequences, per the architecture's own stop rule:** C.2 is ABORTED. C.1 ships
alone. 0.3's declared gap stays loud (`kReentryJointLanded` stays false). This is
also the answer to owner question 3, taken at its stated default: **yes** — ship
C.1's posed-anchor slide without a bend at the ball this pass.

### STAGE A — LANDED. The spine.

`mist_composite` gains a per-pixel `cover` input; a covered pixel never receives
mist. The mask is **the same one the cel ink is grown from**, hoisted out of that
block, plus the ink ring — not a second derivation of the same idea.

Evidence, all one binary:
* mask painted green and LOOKED at first (R2), before any bound was wired to it.
  Selected: body, antenna, eye, ink ring. Not selected: sky, terrain, mist cells,
  the motes.
* `probes/stagea_hue_gate.py`: **0.0 deg** hue rotation on the antenna band with
  STAGE A on; **115.0 deg** with it off, saturation 172.5 -> 73.8. Watched
  failing through the shipping compositor, not a copy.
* **R4, the protected travelling trails: PASS.** `hasty` f120 and `fall` f90,
  Stage A on vs off: the chunky teal streak and the plume are IDENTICAL; only
  0.6%/0.7% of pixels move and they are the creature's own. The fix improves
  those clips too — the body keeps its crimson and the ink line stays crisp.

**The loud missing-mask warning earned its keep on its first run** by catching my
own error: the first render omitted `ZIXX_EXP=celmain`, so it had no mask AND was
under the wrong rig (gotcha 12). A silent fallback would have produced a
confident, wrong "before" plate.

### C.1 — live, and the amplitude needs authoring

Aiming the closure at the POSED anchor works, but **it had to be proved rather
than assumed.** The closure metric did not move at all after the change, which is
the classic tell. A first falsification test (bump the constant 13x, watch the
number) was WEAK and I nearly accepted it: the reported number is a MAX over the
bank, and the key holding that max is in a clip the knob does not touch, so a
real effect elsewhere is invisible to it.

Replaced with a direct, committed measurement in `manafold_probe.cpp`: how far
the posed anchor actually travels from its bind position, per clip.
**13 mm, on 14 of 16 clips — LIVE.** A flat zero now FAILS the probe, so this
cannot quietly die again.

But 13 mm re-aims a 1270 mm arm by 0.6 degrees — under a pixel. The knob works
and now has to be *authored*. The closure gate is insensitive to it (the anchor
stays at radius 153 mm inside a 450 mm body, so burial holds by construction):
39 mm at 2700, 78 mm at 5400, 116 mm at 8100, and the bank worst stays 1043 pm
against the 1120 gate at every one. So the amplitude is a by-eye call, which is
what it should be. Rendering 900 vs 5400 on `manafold-antenna-fixed` to look.

### 0.6 — the pass-9 record corrected (appended, never rewritten)
Three items: the overstated "not one smear code path is touched"; the follow
figures (the true range is ~2x the published one, and "deltas to 258" cannot be
per-channel); and cross-pass ablation deltas declared VOID.
