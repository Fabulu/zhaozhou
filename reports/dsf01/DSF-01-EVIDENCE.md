# DSF-01 — projector divider compare/subtract fusion: evidence record

*Experiment from `reports/Zhaozhou_Divider_Fusion_Implementation_Guide.txt`
(owner, 2026-09-17). This file records what was actually done and what it
measured. It is not a recommendation until the fit gates close.*

---

## G0 — baseline, isolation, policy

| | |
|---|---|
| BASE commit | `b956533c` |
| isolated checkout | `C:\programmieren\zencrifice\zhaozhou-dsf01`, branch `dsf01/divider-fusion` |
| active lane | untouched — `zhaozhou-ceiling-lane-20260912` was not edited |
| policy | the guide's DEFAULT AUTOMATIC-ADOPTION POLICY, frozen before any area result was looked at |

Baseline confirmed against the guide's Section 3: `DIV_STEPS = 31`, three lanes,
and the exact slice at `zhao_project_core.sv:1000-1015`.

The guide asked that the experiment not be run in the active Packet-H checkout.
It is a `git worktree` of the same repository at the same commit, so the
sources are identical and the shell lane's files and HEAD are untouched.

---

## G1 — arithmetic proof, and the proof's own controls — **PASS**

`reports/dsf01/divstep_formula.smt2` is the guide's Appendix C transcribed
verbatim. It asserts the NEGATION of equivalence over all 63-bit `dv` and
31-bit `d`, so `unsat` means no counterexample exists.

    z3 -smt2 divstep_formula.smt2   ->   unsat

**And an `unsat` from a formula that cannot express disagreement is worth
nothing**, so `reports/dsf01/run_controls.py` mutates the candidate side four
ways and requires every one to come back `sat`:

| control | mutation | result |
|---|---|---|
| `BIT31` | reads bit 31 as the borrow — the guide's named trap | **sat, FIRED** |
| `STRICT` | strict `>` instead of `>=`; wrong exactly at `t == d` | **sat, FIRED** |
| `SIGNED` | sign-extends operands that are unsigned | **sat, FIRED** |
| `NOMUX` | takes the wrapped difference unconditionally | **sat, FIRED** |

All four fire. The prover demonstrably distinguishes the four ways to get this
wrong, which is what makes the production `unsat` evidence rather than
decoration.

---

## G3 — functional regressions — **PASS**

With the patch applied, 10/10:

    geom_project_directed · geom_project_random · geom_project_random_nightly
    lint_zhao_geom_project · terrain_project_directed · terrain_project_random
    terrain_project_random_nightly · lint_terrain_project
    terrain_project_chain · shell_project_path_directed

Run after building the executables. The first attempt reported eight failures
that were `Unable to find executable` — the guide is explicit that this is not a
pass, and it is not a failure either.

---

## G2 — matched Quartus maps

Same tree, same source list, same tool, same device, same 314 virtual pins;
A and B differ only in the divider slice.

| field | A baseline | B candidate | delta |
|---|---:|---:|---:|
| estimated ALMs | 5,579 | 5,242 | **−337** |
| combinational ALUTs | 8,730 | 8,769 | **+39** |
| registers | 7,205 | 7,298 | **+93** |
| DSP blocks | 33 | 33 | 0 |
| block memory bits | 3,186 | 3,093 | **−93** |
| errors | 0 | 0 | |

**No cheap stop.** The guide says to stop before fitting if the map shows
Quartus already implements both forms as the same physical structure. It does
not: ALUTs, registers and memory bits all move.

### The part that does not fit the comfortable story

`+93` registers and `−93` memory bits is the same number twice, and it is not
arithmetic. Diffing the inferred-memory lists, the 24 entries are identical
except one:

    A:  31 x 8  Simple Dual Port      (248 bits)
    B:  31 x 5  Simple Dual Port      (155 bits)

One 31-deep delay line narrowed by three bits per word, and those 93 bits are
now flops. So **the −337 estimated ALMs is a packing result, not removed
logic** — combinational ALUTs went UP by 39 — and it arrives together with a
structural side effect that pushes bits out of memory and into ALMs. Against
the standing owner direction that M10K is plentiful and ALMs are not, that
side effect points the wrong way, even though the net estimate does not.

It is 93 bits. It is recorded because it is the kind of detail that the
headline number would otherwise bury, not because it is large.

### What this evidence is not

`estimatedAlms` is a MAP estimate for a leaf with virtual pins. It is not
placed ALMs, and this repository's own rules are explicit that the two are
different questions. **Nothing here should be entered in a resource budget.**
The policy threshold of 250 ALMs applies to placed ALMs at a matched fit seed,
which is G4.

---

## G4/G5 — matched physical fits

*In progress. Results, or the decision not to spend them, appended below.*

---

## Standing decision

Not adopted. The patch lives only in the isolated checkout
(`dsf01/divider-fusion`) and has not been proposed to the active lane. A clean
negative result finishes this experiment, and so does a positive one that fails
a policy threshold — the guide is explicit that a smaller saving is
"SMALL WIN — NOT AUTO-ADOPTED", not a reason to move the threshold.
