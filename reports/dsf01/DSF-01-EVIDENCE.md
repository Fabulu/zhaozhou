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

## G4 — matched fits, seed 2

### First pair: VIRTUAL pins — and that is my error, recorded as one

The guide specifies a **matched physical-pin** seed-2 pair. I took
`run_block_fit.ps1`'s default, which is `virtual-top-ports`. The pair is
internally matched -- same tree, same seed, same pin mode, and the source
digests confirm exactly one file differs (`zhao_geom_project.sv` identical,
`zhao_project_core.sv` differing) -- but it is not the configuration the
policy is written against. The physical-pin pair follows below.

| field | A baseline | B candidate | delta |
|---|---:|---:|---:|
| **ALMs** | 6,286 | 4,794 | **−1,492** |
| registers | 6,946 | 6,834 | −112 |
| Fmax | 94.79 MHz | 101.49 MHz | +6.70 |
| setup slack | −0.550 ns | **+0.147 ns** | +0.697 |
| setup TNS | −31.515 | **0** | +31.515 |
| DSP blocks | 33 | 33 | 0 |
| RAM blocks | 29 | 29 | 0 |
| block memory bits | 3,408 | 3,315 | −93 |
| **hold slack** | +0.274 ns | **−0.068 ns** | **−0.342** |
| **hold TNS** | 0 | **−0.079** | **−0.079** |

The ALM result is large and it is matched: **−1,492 placed ALMs**, roughly
six times the policy threshold, on a pair that differs by one file. The
setup result is better still -- the baseline does not meet 100 MHz at this
leaf configuration (94.79 MHz, −31.5 ns TNS) and the candidate does, with
TNS at zero.

### The hold violation, which is the number that decides it

Under the predeclared policy B **fails**: "all applicable hold/removal/
recovery/minimum-pulse requirements must pass", and B has two violated hold
paths. The guide is explicit that a timing trade is rejected "even if it
saves 1,000 ALMs", and this saves 1,492. So the policy answer is REJECT,
and it does not get softened because the area number is attractive.

**But look at where the violations are before concluding anything:**

    -0.068  cfg_data_i[13]  ->  zhao_project_core:u_core|mat[0][13][13]
    -0.011  cfg_data_i[13]  ->  zhao_project_core:u_core|mat[0][5][13]

Both launch from a **top-level input port** and land in the configuration
matrix register file. **The divider is in neither path.** The patch changed
the divider recurrence; `cfg_data_i -> mat[..]` is the matrix load path,
which it does not touch. What changed is the floorplan: 1,492 fewer ALMs is
a different placement, and a marginal input path moved with it.

And these are **virtual** pins. Input-to-register hold at a virtual boundary
has no real launch model, which is exactly why the guide asked for physical
pins. So this pair cannot settle the hold question in either direction --
it can only say that the question is open, which is what
"INCONCLUSIVE rather than zero" means here.

### Second pair: PHYSICAL pins — NOT ACHIEVABLE for this block

The baseline physical-pin fit **failed** in 66 seconds:

    status        incomplete:failed:quartus_fit.exe
    ioMode        physical-top-ports
    Total pins    344
    virtual pins  0

`zhao_geom_project` presents **344 ports**. As real I/O on a
5CSEBA6U23I7 that is far more than the package has user pins, so the
fitter cannot place it. Analysis & synthesis completed (7,070 registers,
3,408 memory bits, 33 DSP are real); ALMs and Fmax were never produced.

The candidate fit had already been launched and failed IDENTICALLY -- same
status, same 344 pins, 69.9 s against 66.3 s. That is better evidence than
stopping would have been: the failure is the boundary, not the patch, and a
matched failure says so where a single one would have left room to wonder.

    A  incomplete:failed:quartus_fit.exe  344 pins  7,070 registers  66.3 s
    B  incomplete:failed:quartus_fit.exe  344 pins  7,163 registers  69.9 s

Making this configuration exist means building a pin-reducing wrapper --
the technique `zhao_terrain_pipe_rpp3_matw18_fit_top` uses, collapsing the
boundary to a signature and an epoch. That is a new artifact with its own
correctness question, and the guide is explicit: *"Do not broaden this task
to ... new dividers ... Those are independent projects."* So it is not
started, and the gap is reported instead.

---

## Verdict

**NOT ADOPTED. The area result is strong; the policy cannot be satisfied as
written, and the reason is the configuration, not the patch.**

| gate | outcome |
|---|---|
| G0 isolation, frozen baseline, predeclared policy | PASS |
| G1 exact arithmetic proof + four live failure controls | **PASS** |
| G2 matched maps, no cheap stop | PASS, with one structural finding |
| G3 functional regressions, 10/10 | **PASS** |
| G4 matched seed-2 pair, virtual pins | ran; −1,492 ALMs, hold violated |
| G4 matched seed-2 pair, **physical pins** | **NOT ACHIEVABLE** — 344 pins |
| G5 seed-4 confirmation | not reached |
| G6 adoption | **no** |

### What is solid

The arithmetic is proved for the full input domain, including `d = 0`, and
the prover is proved to catch the four ways of getting it wrong. Ten
projector regressions pass bit-exactly. On a matched virtual-pin seed-2
pair differing by exactly one file, the candidate places **1,492 fewer
ALMs** — six times the policy threshold — and turns a leaf that misses 100
MHz by −31.5 ns of TNS into one that meets it with TNS at zero.

### What is not

The hold result. Two paths violate by −0.068 and −0.011 ns, both launching
from a top-level input into the configuration matrix, **on a path the patch
does not touch** — the divider appears in neither. At a virtual boundary
there is no real launch model for an input, which is exactly why the policy
names physical pins; and physical pins cannot be produced for this block.

So the honest label is the guide's own: **INCONCLUSIVE on timing, not zero
and not a pass.** Under the policy as written, an unsatisfied hold
requirement is a reject, and that is the answer recorded here. It is not
softened because −1,492 is attractive; the guide anticipated exactly that
temptation and said a timing trade is rejected "even if it saves 1,000
ALMs".

### What the owner may want to decide

Two things would each unblock a real verdict, and both are the owner's call
because both change the experiment's declared shape:

1. **Amend the policy to name an achievable configuration** — e.g. accept a
   matched virtual-pin pair for area, and take the timing verdict from a
   composed fit where this core is not the top.
2. **Authorise a pin-reducing fit wrapper** for the projector, which would
   make the physical-pin pair possible and would also be reusable.

Neither is started. The patch is on branch `dsf01/divider-fusion` and has
not been proposed to the active lane.

### One thing worth keeping regardless of the verdict

The candidate is the only configuration measured in this experiment that
meets 100 MHz at this leaf. If the projector's timing ever becomes the
binding constraint, this patch is the first thing to re-open — the evidence
is here and the arithmetic is already proved.

---

## Standing decision (unchanged by the above)

Not adopted. The patch lives only in the isolated checkout
(`dsf01/divider-fusion`) and has not been proposed to the active lane. A clean
negative result finishes this experiment, and so does a positive one that fails
a policy threshold — the guide is explicit that a smaller saving is
"SMALL WIN — NOT AUTO-ADOPTED", not a reason to move the threshold.
