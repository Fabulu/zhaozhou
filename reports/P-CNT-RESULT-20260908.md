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
