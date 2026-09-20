# The pass-19 identity leg was missing an operand (found by the 22-subject scope proof)

**What was claimed, all pass:** `ZHAO_U02_KNEAD_DIP_PM=0 ZHAO_U02_REAR_BOW=legacy`
"reproduces the pass-19 bank byte for byte". The close said it, the re-review
re-measured it and confirmed it, and `e-identity-pass19` is a gate-matrix leg.

**What it was ever checked on:** three clips -- Hover `0xA2D0E051`, Inspect
`0x779615BB`, Taunt III `0x75BC4777`.

**What the 22-subject bank found:** that two-switch leg reproduces pass 19 on
**19 of 22** clips. **Blown, Fall and Trick differ.**

| subject | pass-19 recorded | two switches off | three switches off |
|---|---|---|---|
| manafold-blown | `0xCCAC7CAB` | `0x053C4391` | **`0xCCAC7CAB`** |
| manafold-fall | `0x0DE69D1C` | `0x3F9F7CBA` | **`0x0DE69D1C`** |
| manafold-trick | `0x71BB47B9` | `0xBEDD03D3` | **`0x71BB47B9`** |
| manafold-taunt3 (control) | `0x75BC4777` | `0x75BC4777` | `0x75BC4777` |

**The cause, and it is by design.** Pass 20 has THREE exact-off switches, not
two. The particle reaction is the third (`ZHAO_U02_FOLD_DIP_PM`), and its own
constant block in `manafold_art.h` says so in as many words: *"the agitation
reverts to the version-18/19 speed-only term and the mana is byte-for-byte what
it was"*.

And the reaction is deliberately **read from the POSE, not from the dip's
schedule** -- `mana_fold` measures carrier B's sag below the A/C midline, so it
answers **any** authored dip, not only this pass's beat. `manafold_fx.h` names
that as a feature: *"the reaction is free on ANY authored dip... Taunt III's
crown shuffle drops B through the same geometry and the fold now answers it
too."* Blown, Fall and Trick evidently carry ambient sag past the 90 mm onset
somewhere in their motion, so with the knead beat off the fold still reacts,
and those three clips are not pass 19.

**So this is a finding about an INSTRUMENT, not about the creature.** Nothing
that ships is affected: the shipping bank is what the re-review looked at, the
four verified CRCs reproduce exactly, and the third switch is a documented,
tested exact-off control. What is wrong is the *claim* -- "reproduces pass 19
byte for byte" is true of the three-switch leg and false of the two-switch one,
and it was only ever sampled on three clips that happen to have no ambient sag.

It is the pass's own pattern once more, and the fifth instance of it: **a
comparison wired to fewer operands than the thing it is comparing.** The pass
found four detectors reading the wrong operand; this is an identity leg reading
too few. Three clips passing is not 22 clips passing, and a leg that samples
3 of 22 cannot see the 3 that differ.

**Corrected.** The scope proof and the gate-matrix identity leg both now carry
all three switches; the shipping scope table below is measured that way, and
the two-switch bank is kept in the table as the evidence for this finding.

## The corrected scope, measured on all 22 subjects

`scope-attribution.txt`, four complete banks from the one hashed binary
(MD5 `e95faca916627d1bddb02892c5eb67e1`) in the production environment:

| leg | switches | result |
|---|---|---|
| **pass19** | `KNEAD_DIP_PM=0 REAR_BOW=legacy FOLD_DIP_PM=0` | **22/22 byte-identical to pass 19's committed table.** Every byte that differs from pass 19 is produced by the three pass-20 mechanisms and nothing else. |
| **rearonly** | `KNEAD_DIP_PM=0 FOLD_DIP_PM=0` (shipping bow) | differs from pass19 on **22/22** -- the rear repair is structural, so every clip's rear changed. |
| **shipping** vs rearonly | the beat + its reaction | differs on **21/22**. The exception is **Taunt III**, which authors no dip, exactly as declared. |
| **reaction-only** vs rearonly | `KNEAD_DIP_PM=0` only | differs on **3/22**: Blown, Fall, Trick. |

## A second, smaller finding in the same place

`manafold_fx.h` says of the pose-driven reaction: *"Taunt III's crown shuffle
drops B through the same geometry and the fold now answers it too, which is the
behaviour the owner was already pointing at when he named the nodule taunt."*

**Measured over all 368 frames, it does not.** Taunt III is byte-identical
across `rearonly`, `reaction-only` and `shipping` (`0xC81598AA` three times), so
neither the beat nor the reaction moves a single byte there. Its crown shuffle
does put B lowest -- the re-review proved that independently -- but it does so
without sagging B past the 90 mm `kFoldDipOnsetMm` relative to the A/C midline,
so the fold never notices.

The three clips the reaction DOES answer without a beat are Blown, Fall and
Trick, which is a better illustration of the same intended behaviour: those are
the clips whose own motion throws the loop around hard enough to sag B. Nothing
needs to change in the code; the comment overstates one example and the true
list is now recorded beside it.

**Neither finding changes a shipped byte.** The shipping bank is exactly what
the re-review looked at: hover `0x93B95AEE`, inspect `0xD00478A1`, taunt3
`0xC81598AA`, blown `0xC6AAF7AD`, all four reproduced.
