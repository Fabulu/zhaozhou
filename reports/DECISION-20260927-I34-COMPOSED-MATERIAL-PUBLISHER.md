# DECISION — `TERRAIN.COMPOSED_MATERIAL`'s publisher: shape, tap, and the cost it must be measured against

Taken 2026-09-27 by packet LASTGAP under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0, and under the owner ruling
`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`, which commissions
the region and forbids refusing it on the existing estimate. Format per §0:
question; chosen option; reason and alternatives; constraints/cost;
consequences.

---

## QUESTION

The owner commissioned `TERRAIN.COMPOSED_MATERIAL` — `[0x058B_0000,
0x05AB_0000)`, 2 MiB as 256 × 8 KiB — and declined the COMPOSEPUB-shaped refusal
that packet `I34CLOSE` recommended. **What exactly is published, from where, and
what does it cost on a real scene rather than at the worst case?**

COMPOSEPUB refused the SIBLING regions (`COMPOSED_HEIGHT`,
`COMPOSED_VELOCITY`) on two independent grounds, recorded in
`spec/memory_rules.md`: **no consumer** (*"a DMA into unused memory is not a
consumer"*) and **bandwidth** (124% of the frame for the write alone, 177% with
the re-stage read). The owner's ruling is explicit that the second does not carry
here on an estimate, and the first is materially different for this channel.

## WHAT IS DIFFERENT ABOUT MATERIAL, AND IT IS NOT AN ARGUMENT FROM SYMMETRY

**The fabric consumer exists and is now proven live end to end.** As of this
packet the composed material is not merely routed — it is *composed*:
`terrmat field_composed=1024 token_refused=0` in `-FieldActive`, one composed
cell per compose-cache cell, every one a legal v1 token, reaching
`zhao_terrain_matjoin` and displacing the authored layer-E triple into the
mosaic. That is the thing COMPOSEPUB could not say about height or velocity.

So the publication question is genuinely a **destination** question, not a
"does anything read this channel" question, and the R64 / consumer-absence
precedent does not dispose of it. The owner's own reasoning — NAV was
**replaced**, not removed — is the same distinction.

## DECISION

**Publish the composed layer-E plane per patch from `zhao_terrain_matjoin`'s own
composed write face, dirty-gated, and measure the dirty fraction on the real
scene before accepting or refusing the bandwidth.**

| | chosen |
|---|---|
| **tap** | `zhao_terrain_matjoin`'s `o_we_o` / `o_ci_o` / `o_cj_o` / `{o_mat_a_o, o_mat_b_o, o_weight_o}` — the composed write face, port for port the compose cache's layer E |
| **payload** | 4 B per cell: `{8'h00, weight, mat_b, mat_a}`, 1,024 cells = **4,096 B per patch** inside the owner's 8 KiB slot |
| **granularity** | one 64-byte fabric write per 16 cells — **64 requests per published patch** |
| **gate** | published only for a patch whose composed plane actually changed, which `field_composed_o` and `terr_pt_subpatch_dirty_o` already measure |

**The tap is chosen and not found.** `zhao_terrain_patch_acc` — which owns
`out_mat_*`, the resolved material u32 — is BUILT AND COMPOSED NOWHERE, and it
is **not** the right tap: it is the four-bank *field-major* patch accumulator
(16 M10K of scratch, its own INIT/ACCUM/DRAIN phases), belonging to the
`patch_v2` topology this console does not run. Publishing from it would couple
the destination to a rewrite the destination does not need. Verified: its only
tree-wide instantiation is `tests/differential/tb_terrain_fieldmajor.sv`, in no
fit closure. The brief's instruction to check it before commissioning anything
new is discharged — it was checked, and it is the wrong block for this job.

## REASON, AND THE ALTERNATIVES REJECTED

1. **Matjoin's write face is the ONLY place the composed triple exists as a
   unit.** Upstream it is an authored plane plus a field lane; downstream it is
   already a mosaic pick. FIELDACTIVE and MATFIELD independently named this same
   tap, which is corroboration rather than one packet's preference.
2. **Rejected: publish from the compose cache's serve side.** It answers per
   *fragment*, not per cell, so the same cell publishes many times and the write
   count stops being a property of the lattice.
3. **Rejected: publish every patch unconditionally.** That is the shape
   COMPOSEPUB priced at 124% of frame, and it spends the whole budget to
   re-transmit cells nothing changed. The dirty gate is not a trim to meet the
   budget — it is the correct semantics, because an unchanged patch's published
   plane is already in the region.
4. **Rejected: an 8 B/cell layout filling the slot.** 4 B carries the triple with
   a reserved byte; 8 would double every number below to buy nothing declared.

## CONSTRAINTS AND COST

**The write-side arithmetic, stated so it can be checked rather than believed.**
4,096 B ÷ 64 = **64 fabric requests per published patch**. COMPOSEPUB's own
ledger prices the sibling channels at 18,432 requests/frame taking the frame from
80.17% to 124.41%, a delta of 737,280 grant-clocks — which is **exactly 40.00
grant-clocks per request**, and the fact that it comes out an integer is the
check that the two numbers in that table describe one model. Frame total is
1,666,698 grant-clocks; the headroom is **19.83%** = 330,500 grant-clocks.

| patches published per frame | requests | added % of frame | verdict against the 19.83% headroom |
|---|---|---|---|
| 256 (every patch, COMPOSEPUB's worst case) | 16,384 | 39.32% | **does not fit** |
| 200 | 12,800 | 30.72% | does not fit |
| **129 (break-even)** | 8,256 | **19.82%** | **at the line** |
| 100 | 6,400 | 15.36% | fits |
| 1 (this console's measured scene) | 64 | **0.15%** | fits with room to spare |

**So the break-even is 129 published patches per frame — just over half the 256
slots — and the question is entirely the dirty fraction**, which is exactly what
COMPOSEPUB's record commissions: *"The packet that builds the publisher owes that
fraction measured on a real scene."*

> **I FIRST WROTE 82 HERE AND IT WAS WRONG**, recorded rather than quietly fixed
> because the error is instructive: I scaled the request count without dividing
> the headroom by the per-request cost, and 82 is the *more* pessimistic number,
> so it would have argued toward a refusal the owner has already declined once.
> A derived figure gets its arithmetic checked before it gets quoted.

**THE ROW ABOVE IS DERIVED, NOT MEASURED, AND IT IS LABELLED SO.** It is scaled
from COMPOSEPUB's grant-clock ledger by request count. MATFIELD's standard
applies and is the one the owner set: a derived figure is not an escalation, and
a refusal resting on one will be sent back. The number that decides this is
`tools/budget/sdram_bandwidth.py`'s own row for this region, run against the
measured published-patch count — not this table.

## CONSEQUENCES

* **Code.** A new block; `zhao_pkg` `ZHAO_TERRAIN_COMPOSED_MATERIAL_BASE`/`_SPAN`
  (`0x058B_0000` / `0x0020_0000`, agreeing to the byte with the table's
  inclusive `0x05AA_FFFF`); a `MEM.GUARD` write arm with its own non-vacuity
  cover; a region row in `spec/memory_rules.md` §5b carved out of the reserved
  tail; console composition; `blocks.yml`, `fit_targets.yml` and the manifest.
* **A GUARD WINDOW OPENS WITH ITS BLOCK, NEVER AHEAD OF IT.** That is
  `TERRAIN.DEVSTORE`'s precedent and COMPOSEPUB's stated reason for leaving the
  sibling windows shut. The arm lands in the same commit as the writer or not at
  all.
* **This is NOT zero silicon**, and it must not be reported as though the
  constant-pool fix's zero-silicon evidence covered it. It is a new M10K buffer,
  a new fabric client and a new guard arm, and the owner asked for its cost
  measured either way.
* **`gen_prod_top --check` and `gen_console_board --check` GO STALE** the moment
  a core port appears, and must be regenerated in the same commit.
