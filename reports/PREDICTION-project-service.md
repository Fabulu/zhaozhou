# Prediction for the `zhao_project_service` leaf fit, before it starts

## Why this fit, and its question stated exactly

`design/fit_targets.yml:527` has declared `- top: zhao_project_service` with
`max_dsp: 33` and `max_alms: 6300` for long enough that nobody remembers, and
`zhao_block_fit.json` holds **no row for it**. The one measurement that prices
the machine's largest single overrun has been one command away the whole time.

**The question: what does ONE shared projection service cost, with BOTH clients
driven, against the 24,399 ALUT that two unshared cores cost in the production
census?**

Both halves of that comparison exist already:

```
production census   zhao_geom_project -> its own core   10,220 ALUT (own 6,908)
                    zhao_terrain_project -> its own core 14,179 ALUT (own 6,883)
                                                        ----------------------
                                                         24,399 ALUT ~ 15,470 ALM

G8B @g8b-t11-pins-s2  ONE core inside zhao_terrain_pipe   8,694 ALUT, 24 M9K, 0 DSP
```

**The G8B number is a floor and this fit is what turns it into a cost.** In
G8B's top, client A is not driven — `zhao_proj_subsystem`'s own header says the
geometry producer "is not composed anywhere in this tree" — so Quartus may have
folded away arbitration a two-client service has to carry. A standalone
`zhao_project_service` fit puts BOTH clients on top-level pins, so nothing can
be folded, and the arbitration increment becomes visible.

## Prediction

1. **ALM 5,600–6,300.** The rule's `max_alms: 6300` was written by someone who
   had a number in mind; the two leaf rows for the unshared wrappers are 6,199
   and 6,068 ALM, and one shared core plus arbitration should land near one
   wrapper rather than near two.
2. **DSP exactly 33** — the structural one-default-core ceiling the rule
   asserts. Any other value means the multiplier count is not what the
   architecture says it is, and that matters more than the ALM number.
3. **The arbitration increment is small: 300–900 ALM** over what one core alone
   costs, i.e. a round-robin grant, a demux and one extra rider path.
4. **Fmax 60–75 MHz.** `zhao_project_core` measured 61.09 MHz before its
   stage-5b repair and the leaf rows show 73.62; there is no `min_fmax_mhz` on
   this target and its clock contract belongs to the core, so this is context,
   not a gate.
5. **M10K is the number to read per replica, not in total** — the target's own
   comment says so. Expect the core's own storage only, no arena replicas,
   because the arena is not in this closure.

## The saving this prices, written now so it cannot be quoted loosely later

If (1) holds, the selected machine carries **~15,470 ALM of projection where one
service would cost ~6,000** — a saving of roughly **9,000–9,500 ALM**, about
**23% of the 41,910-ALM device** and more than three times the shell's entire
remaining margin under its 30,000 budget.

**What would make that number wrong, in the direction that matters.** The
comparison sets a standalone leaf against two blocks measured inside a
147-instance census. Leaf fits carry virtual-pin boundary logic the composed
case does not, so a leaf reads HIGH — which makes the saving a conservative
estimate, not an optimistic one. The opposite error is available too: the two
census numbers include each wrapper's own arenas and caches, and one shared
service does not automatically dedupe those. `zhao_geom_wcache` at 5,132 ALUT is
counted separately in the census and survives the sharing.

**So the honest claim this fit can support is about the CORES, not the
subsystem:** two cores become one. Everything hanging off them is a separate
question and must not be folded into the headline.

## What would falsify the reasoning rather than the numbers

If the standalone service comes in near 12,000 ALM rather than near 6,000, then
arbitration for two clients is not a grant and a demux — it is a second data
path — and "one shared core" is the wrong description of what was built. That
would be worth knowing before any selection changes, and it is exactly what a
fit with both clients driven can show and G8B's receipt cannot.
