# Plan, after the 2026-09-08 owner brief

The brief's central call is **keep V3, preserve the repairs, change specific
data-access and timing structures — not another whole-island rewrite.** Adopted.

## Its assumptions, checked against source

Every claim I could check in the tree is **true**:

| claim | verified |
|---|---|
| `err_class_invalid_o`, `err_fragrob_wq_overflow_o`, `err_fragrob_id_error_o` have dead fan-in | **true.** `fr_alloc_valid`, `fr_wq_overflow`, `fr_id_error` — and also `fr_combiner_unfrozen` — have **zero** drivers |
| an output-assignment checker misses this | **true, and it did.** `undriven_outputs.py`, written hours earlier, passes this file: the ports have real `always_ff` assignments whose *sources* are dead |
| `sampmeta_m` is 64×3, written once at planning, read by three separate lanes | **true** — bilinear :1626, CLUT :1809, nearest :1934, single write :1369 |
| the three-counter identity sum omits categories | **true.** v3own exposes six — range, stale, unsol, dup, final, issue — and I summed three |
| 11,562 ALM / 19,203 reg / 84.03 MHz is the **legacy** island | **true** — the row is `zhao_texture_island_top@p0c-stageA`, not V3 |
| no completed V3 physical receipt exists | **true** — the first V3 fit is still running |

Its bundled checks re-run clean here (`sha256sum -c` verifies all six files),
and **every check carries a mutant it detects** — the bundle applies to itself
the standard this repository asks for. Z1 labels its own limit in its output:
an abstract witness, not an RTL reachability proof.

### The one place it is most useful

> *"A checker that merely asks whether each output has an assignment can miss
> this."*

That is a precise description of a tool I had just written and just proved
fires. It fires on a **missing driver**; it is blind to a **live driver reading
a dead source**. The distinction the brief draws — *direct driver presence*
versus *transitive live fan-in* — is the actual property, and I had implemented
the weaker one and then demonstrated it working, which is the more convincing
way to be wrong.

## Two corrections I accept

**M6 is amended.** I wrote that a Fmax delta is attributable only when the
worst-path family matches. The brief: *"a successful repair often SHOULD change
which path is worst."* Obviously true once said — requiring a match would reject
exactly the repairs that worked. `worst_path_index.py` **keeps its job and loses
its inference**: recording which path gated a fit is still worth doing, because
the next fit of the same module destroys that evidence. Concluding attribution
from it is not.

**The +10.90 MHz reseed was reported too confidently.** Two post-change seeds
are encouraging; their 1.13 MHz spread is not a bound on seed variation.

**And the 7,500 ALM redline is not a physical cliff.** I registered the V3 fit
under it an hour ago. That stays — as the *historical-rule result*, one of the
two judgments the brief asks for. It is not the *product-allocation decision*,
which needs count-once whole-console accounting and explicit owner approval. An
~11k-ALM island at a genuine 100 MHz may well be a good outcome.

## Work order

**Now — let the fit finish untouched.** The brief: *"Let it finish on its frozen
specimen. Record its failures honestly. Do not change the source under that
run."* The snapshot digest `BAB0DB2C…` matches the committed source byte for
byte, so the receipt will describe the RTL that is in git.

**1. Close the contract holes** (§3), before either architecture experiment:

* **A.** `err_class_invalid_o` from `own_adm_accept && frag_class_i == CLS_ERR`
  — the actual ingress beat and its own raw class, not an ingress event paired
  with a planner-stage value. *(This is the same input/planner misalignment that
  cost gate 2 two rounds; it is still present in the error path.)*
* **B.** the identity sticky must name which of range/stale/unsol/dup/illegal
  issue/unauthorized final it covers, and cover them.
* **C.** the queue-overflow port gets a real meaning or a versioned retirement.
  **Not tied to zero and called preserved.** `valid && !ready` is backpressure,
  not overflow.

**2. Positive fault tests for every retained observable** (§3.2). Clean traffic
clear → inject the specific condition → that exact observable moves → legitimate
traffic resumes. *A permanently-zero flag must not pass a healthy-run test.*

**3. Freeze the encodings** (§4.1) — named pack/unpack for owner/sample
handle/route token/T2 ticket. This is the direct fix for the **five stale
slices** this integration cost, and the brief reaching the same conclusion from
outside is worth something.

**4. Then, and only on the receipt's evidence,** §6 (reserved RCP preparation)
or §7 (one metadata join before class fan-out). The brief is explicit that which
goes first depends on the measured path, and that a *different* measured family
means investigating that family rather than forcing the list.

## Not doing

* A repository-wide parser project — the brief says so directly.
* Restarting the 541/119/392 campaign. It is kept; the new schedules are
  incremental obligations on top.
* Terrain. Owner direction 49fc32e9 still governs and the brief does not touch it.
