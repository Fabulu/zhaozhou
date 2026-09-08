# Experiment P-CNT: measured

Post-fit brief §4.2. Palette diagnostic counters moved from the live
classification to the already-registered verdict.

## Result

| | baseline | **P-CNT** | delta |
|---|---|---|---|
| ALM | 540 | **542** | +2 |
| registers | 628 | **648** | +20 |
| Fmax | 98.06 | **104.08** | **+6.02 MHz** |
| status | ok | **ok** | |

**The leaf now meets 100 MHz.** 1,652 s, digest `79c4c3e377ec`.

## The path evidence is stronger than the Fmax delta

| | worst path | slack |
|---|---|---|
| before | `gen_r[3][0] -> cold_o[4]~reg0` | **−0.198 ns** |
| after | `gen_r[1][6] -> l1_stale_q` | **+0.392 ns** |

**The old gate terminated at `cold_o` — the counter this experiment removed from
the live cone.** The new gate terminates at `l1_stale_q`, the registered verdict
that P-CNT deliberately made the endpoint.

That is causal attribution of a kind an Fmax number alone cannot give. The
change did not merely coincide with an improvement; **the specific endpoint it
targeted stopped being the endpoint.**

This is the M6 amendment working as intended. Under the old M6 rule — *"a delta
is attributable only if the worst path family is the same on both sides"* — this
result would have been rejected, because the family changed. The owner brief's
correction is exactly right: *"a successful repair often SHOULD change which
path is worst."* Here the family change **is** the evidence.

## It also resolves the missing-digest problem

The 540/98.06 baseline row has **no `.sources.sha256`**, so it could not be tied
to the pre-P-CNT file, and I flagged that as a caveat before reading.

The path record settles it structurally: the previous worst path **ends at
`cold_o[4]~reg0`**, and after P-CNT `cold_o` is no longer on that cone at all.
A row whose gating endpoint is a structure the current source does not have can
only describe the earlier design. **Structural evidence where the provenance
record is missing.**

`worst_path_index.py`'s `previous` field made this possible — written this
morning precisely because the next fit of a module overwrites its setup report.
First use, and it decided the question.

## Caveats, stated

* **Leaf fit.** The island's palette cone starts at
  `rsp_dispatch|cq_rp[0][0]`, outside this block. A leaf fit cannot include it,
  so **+6.02 MHz here does not convert to +6.02 MHz of island Fmax.**
* **Seed noise.** The docket records leaf fitter seed variation around
  **4.70 MHz**. +6.02 is above that but not enormously, so the Fmax figure alone
  would be weak evidence. The endpoint migration is what carries this result.
* **+20 registers** for +2 ALM. Small, and within the range seed placement moves
  things; not worth a story.
* **Not yet simulated.** P-CNT has been fitted but not functionally verified —
  the fast suite has held the build tree. The counters now trail by one cycle
  and `frag_expand`/island tests must confirm the totals are unchanged. **A fit
  is not a correctness result.**

## Next

Functional verification, then the same question at the composed level: whether
removing this endpoint moves the island's `cq_rp -> cold_o` family, which is
41 ps behind the nominal worst path and therefore co-equal with it.

## The partition is equivalent, checked exhaustively by hand

P-CNT is fitted but not yet simulated, so the equivalence claim was still
resting on the brief's assertion. It is now checked against the actual
expressions in `zhao_texture_palette_res.sv`, over all eight combinations of the
three Boolean inputs:

```
st  = (gen_r[lu_slot] != lu_gen) || begin_same_slot     the live stale verdict
res = res_r[lu_slot]                                    live residency
bss = begin_same_slot

OLD: stale on st;          cold on !st && !res
NEW: stale on l1_stale_q;  cold on !l1_stale_q && !l1_res_q
     where l1_stale_q <= st,  l1_res_q <= res && !bss
```

**8 cases checked, 0 divergences.**

The case that could have diverged is `!st && bss` — where the old form counts
cold and the new one would not, because `l1_res_q` folds `!bss` in. It is
**unreachable**: `st = genmismatch || bss`, so `bss = 1` forces `st = 1`, and
`!st && bss` is the empty set. An accepted same-slot BEGIN always classifies as
stale and therefore never reaches the cold-only branch under either form.

That is the brief's C3 argument, confirmed on this repository's source rather
than accepted from the brief. **It is still not a substitute for running the
test** — this proves the counting partition is identical, not that the pipeline
carries the right `l1_v_q` for each lookup, nor that no beat is counted twice
across a stall. Those need the simulation the build tree has been holding.

## The double-count concern, also settled from source

The equivalence check above proved the counting **partition** is identical. I
listed a second concern it did not cover: *"nor that no beat is counted twice
across a stall."*

Settled, without simulation:

* The palette has **no `lu_ready`** — a lookup is not handshaked, it is simply
  presented.
* The island ties `disp_clut_ready = 1'b1`, commented *"the palette lookup is
  unconditional"*.
* `rsp_dispatch` drives `clut_valid_o = (cq_n[0] != '0)` — high while the CLUT
  queue is non-empty, popping one entry per cycle since ready is tied high.

So `lu_valid_i` is high for **exactly one cycle per lookup**. `l1_v_q <=
lu_valid_i` therefore asserts for exactly one cycle per lookup, and the new
counters fire once each. When the queue holds several entries `lu_valid_i` stays
high across consecutive cycles — but each of those cycles is a *distinct*
lookup, and each counts once.

**Both forms count identically**, because the old code also keyed on
`lu_valid_i`. There is no stall on this path to be counted twice across.

### What is still not verified

The simulation. Source reasoning has now covered the partition (8/8 cases) and
the one-pulse-per-lookup property, and both are structural arguments about the
code as written. What they cannot show is that the assembled design behaves as
the code reads — which is the entire reason this repository prefers differential
tests, and the reason three of today's confident readings were wrong.

`island_composed_directed` and `island_v3_composed_directed` both assert palette
lookup/stale/cold totals. Those are the tests that decide it, and they run when
the build tree is free.
