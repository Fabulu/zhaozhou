# V3.1 M0 — the owner's physical attribution, from disk

*2026-09-07. `reports/ZHAOZHOU_TEXTURE_V3_1_REARCHITECTURE_2026-09-07.txt` §0
orders M0 first: "Recover owner physical attribution and timing endpoint
classes; read the existing old-island census immediately, **without queuing a
redundant map**." The FABLE architect independently ranked the same work R1,
first, ahead of everything else. Both are answered below with no Quartus run:
`zhao_texture_v3own@v3-full.map.rpt` was already on disk.*

---

## The answer, and it is unambiguous

```
zhao_texture_v3own@v3-full        ALUT   (self)     REG   (self)   mem bits  DSP
  TOP: zhao_texture_v3own         6532     6416     4220     4085     20640    0
    zhao_texture_v3rq:u_rq_aux      38       38       45       45       896    0
    zhao_texture_v3rq:u_rq_init     40       40       45       45       896    0
    zhao_texture_v3rq:u_rq_tmu      38       38       45       45       896    0
    zhao_texture_v3bank:u_ctx        0        0        0        0      4096    0
    zhao_texture_v3bank:u_ares       0        0        0        0      2560    0
    zhao_texture_v3bank:u_fres       0        0        0        0      2560    0
    zhao_texture_v3bank:g_sres[0-2]  0        0        0        0     7680    0
```

**97% of the registers (4,085 of 4,220) and 98% of the ALUTs (6,416 of 6,532)
are in `zhao_texture_v3own` itself.**

The three ready queues cost **38–40 ALUTs each**. The six banks cost **zero
ALUTs and zero registers** — they are pure M10K, exactly as designed. Together
every named child accounts for **116 ALUTs and 135 registers** out of 6,532 and
4,220.

## What that settles

The demonstrator's 5,678 fitted ALMs are **3.15× its 1,800-ALM allowance**, and
the question M0/R1 exists to answer is *where*. It is not the banks, and it is
not the ready queues. **It is the control plane, entirely.**

That is the strongest available confirmation of V3.1's thesis, and it arrives
from evidence that was already on disk. §0 proposes replacing exactly this:

> per-slot generation records + repeated dynamically indexed state reads
> + three producer-class ready FIFOs + broad quiescence feedback
>
> → one bounded live-sequence window + source-owned control bitplanes
> + eight local ready selectors with two tiny candidate registers each
> + explicit registered drain control and reserved output capacity

Every clause of the "current" line is top-level control logic, and the census
says top-level control logic is 98% of the area. **The banked payload
architecture V3.1 keeps is measurably not the problem, and the control plane
V3.1 replaces measurably is.**

## What it does NOT settle

* **Which part of the control plane.** 6,416 self-ALUTs is one number, not an
  attribution across generation records, dynamic state reads, the three FIFOs
  and quiescence. §19's ablation experiments are what separate them, and this
  census cannot.
* **That the replacement will fit.** V3.1's own header says its area and
  frequency are unmeasured, and this changes nothing about that.
* **Anything about the OLD island's 13,459 top-level registers.** Those are a
  different top in a different fit. §0 is explicit that the two lanes are
  separate, and the census tool reports each on its own.

## Two corrections to the numbers as they have been quoted

* V3.1 §1.1 notes the mapped-vs-fitted trap: **4,220 mapped registers, 4,864
  fitted**, and the legacy 64-deep FRAGROB comparison figure of 9,431 is
  *mapped*. Map against map. The delta is 9,431 → 4,220, not 9,431 → 4,864.
* The row carries **952 virtual pins**, so its 75.79 MHz is a boundary-
  contaminated leaf number. Split core-to-core it is **89.09 MHz**
  (`tools/quartus/split_setup_paths.py`), which is the figure the FABLE report
  works from.

## Provenance and one instrument repaired in passing

`tools/quartus/entity_census.py` is committed rather than retyped, under the
rule CLAUDE.md states for the ground-contact probe. Its first version printed
`UNINFERRED RAM (2)` followed by `0 ... 0`, because its reason-regex was
`[a-z ]+` and stopped at the capital in "inappropriate **RAM** size". Caught
within a minute by *"a number that is exactly zero is a broken instrument until
proven otherwise"* — the tool's own author, by the tool's own docstring. It now
matches mixed case and prints any unrecognised reason **verbatim** rather than
dropping it, because a parser that discards what it cannot classify reports
fewer problems than exist.
