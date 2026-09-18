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

---

# RESULT: `@cheque-price`, commit 3e467e47, clean tree, 2 sources

```
ALM 6,598   DSP 33   M10K 31   registers 7,009   854.3 s
Fmax 91.52 MHz (single clock)   worst -0.926   TNS -403.0   hold +0.238
status failed:structure -- ruleViolations: ["ALM 6598 > allowed 6300"]
```

`rtlCleanAtHead: true`, `treeCleanAtHead: true`, one honestly declared breach of
a ceiling nobody had ever measured against. Read that way round, as this
repository's own rule says: the fit completed, the number is trustworthy, and
the *budget rule* rejected it.

## Scorecard

| # | predicted | measured | |
|---|---|---|---|
| 1 | ALM 5,600–6,300 | **6,598** | **missed, 4.7% over** |
| 2 | DSP exactly 33 | **33** | yes |
| 3 | arbitration increment 300–900 ALM | ~400 over one wrapper | yes |
| 4 | Fmax 60–75 MHz | **91.52** | better |
| 5 | core's storage only, no arena replicas | 31 M10K, no replicas | yes |

**The falsification test passed and it is the important one.** The prediction
said: *"if the standalone service comes in near 12,000 ALM rather than near
6,000, then arbitration for two clients is not a grant and a demux — it is a
second data path, and 'one shared core' is the wrong description of what was
built."* It came in at 6,598. **One shared core is the right description.**

## The saving, and I overstated it by 60%

Leaf against leaf, every row measured the same way with virtual pins:

```
zhao_geom_project       6,199 ALM   33 DSP   29 M10K   clean
zhao_terrain_project    6,068 ALM   33 DSP   23 M10K   DIRTY -- describes nothing exactly
                      ------------------------------
two unshared          ~12,400 ALM   66 DSP
zhao_project_service    6,598 ALM   33 DSP   31 M10K   clean
                      ------------------------------
SAVING                 ~5,800 ALM   33 DSP
```

**My prediction said 9,000–9,500 ALM. The measured saving is about 5,800** —
I was 60% high, in the flattering direction, and the reason is written in the
prediction itself: *"the comparison sets a standalone leaf against two blocks
measured inside a 147-instance census."* I flagged that exact error and then
quoted the number anyway, pairing the census's 15,470 ALM for the two with a
leaf estimate for the one. Two leaf rows against one leaf row is the honest
comparison and it is the one above.

`zhao_terrain_project`'s row is `rtlCleanAtHead: false` and is stamped `ok`,
while both clean rows here are stamped `failed:structure` — the inversion this
repository documents. Its 6,068 cannot be quoted exactly. It is used above only
as an estimate, corroborated independently: in the whole-machine census the two
cores' OWN logic is 6,908 and 6,883 ALUT, within 0.4%, so treating the two
wrappers as the same size is supported by a second measurement rather than by
the dirty row.

## What 33 DSP means, which is separate and may matter more

Two projectors need **66 of the device's 112 DSPs**; one needs 33. The
whole-machine map wants 123 against 112 and is therefore over the physical part.

**That does NOT mean this saves 33 DSP off the 123**, and the trap is worth
naming: in the census both projectors report **0 DSP**, because the machine had
already run out and Quartus converted their multipliers to logic — which is
exactly why their census ALUT counts (10,263 and 14,289) so far exceed their
leaf ALM. The 33 DSP and the ALUT bulk are two prices for the same multipliers,
and the machine is currently paying the second one twice.

So the honest statement is: **sharing removes one projector's multipliers
entirely, and the machine pays for them in whichever currency is scarce.** At
today's DSP pressure that is ~7,800 ALM of soft multiplier in the census
arrangement; with DSPs available it is 33 blocks.

## Where this leaves the adoption

Everything needed is now measured and nothing is unanswered:

* the shared service exists, is composed, and fits at **91.52 MHz** standalone
  and **102.19 MHz** inside `zhao_terrain_pipe` on physical pins;
* both clients driven costs **6,598 ALM / 33 DSP**, against ~12,400 / 66 for two;
* no adapter is needed — `zhao_vertex_arena`'s `fill_ready_o` is a stated
  constant `1'b1` on both sides;
* what remains is composing geometry onto client A, then the **selection**
  change in `design/prod_manifest.yml`, then a re-census.

## The `max_alms: 6300` rule, left alone deliberately

The ceiling is 4.7% under the first measurement ever taken of this block, and it
was written before any fit existed — a guess, and a close one. **It is not being
raised in the same commit that measures it**, because a rule edited to admit the
number that just broke it stops being a rule. It should be set from this
receipt, deliberately, with the reason recorded — or left red until the service
is adopted and re-measured in the arrangement that will actually ship.
