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

---

## §23 FIRST, second half: the four-way endpoint classification

The brief's first instruction has two parts. The entity attribution is above.
This is the other:

> FIRST: export the existing owner fit's entity/resource breakdown and four-way
> timing endpoint classification. The reported 5,678 ALMs are not yet
> attributed. **A path beginning in a queue register and ending at
> `adm_accept_o` is not proven register-to-register core timing merely because
> its startpoint is internal.**

All 2,000 summarised paths of `zhao_texture_v3own@v3-full`, classified at both
ends (`tools/quartus/split_setup_paths.py`):

| class | paths | worst slack | implied |
|---|---:|---:|---:|
| core → core | 1,510 | −1.225 | 89.09 MHz |
| **core → port** | **239** | **−3.194** | **75.79 MHz** |
| port → core | 204 | −1.181 | 89.44 MHz |
| port → port | 47 | −1.104 | 90.06 MHz |

**The reported 75.79 MHz is set entirely by the core→port class**, and the
worst path in it is the exact one the brief names:

```
-3.194  zhao_texture_v3rq:u_rq_tmu|wp_q[0]  ->  adm_accept_o
-2.601  Mux1~10_OTERM3229                   ->  adm_ready_o
-1.587  Mux5~9_OTERM3125_OTERM3329          ->  adm_owner_o[4]
```

Ten paths end at the admission outputs, and they are the ten worst in the fit.

### Why it is not a pin artefact, and the chain that makes it

`adm_accept_o` is a real output a real neighbour samples, so only the pad
routing is artificial — the logic between `wp_q` and it is not. And that logic
is nameable end to end:

```
u_rq_tmu|wp_q  ->  body_occ_c = wp_q - rp_q        (v3rq.sv:71)
               ->  occ_o                            (v3rq.sv:84)
               ->  rq_occ_c[0..2] == 0
               ->  quiet_c                          (v3own.sv:758-768)
               ->  adm_ready_o = (live_cnt_q < OWNERS) && (!wrap_block_c || quiet_c)
                                                    (v3own.sv:271)
               ->  adm_accept_o = adm_fire_c        (v3own.sv:273)
```

**That is precisely the "broad quiescence feedback" §0 proposes to remove**, and
FOURTH states the fix: *"separate normal owner admission from global quiet.
Admission uses a registered owner credit, a local staging credit, and registered
epoch/fence permission."* The measurement says that instruction is not a tidiness
preference — this path is the demonstrator's binding constraint.

Three independent routes reached the same place: the brief named it by
inspection, the FABLE architect ranked it R3 from its own split, and this
classification puts it at the top of the list.

### And my own O1 repair sits on this path

`occ_o` gained the `ld_q` term this morning. §5.2 warned in advance — *"It can
temporarily make a combinational count wider. Do not call that the final timing
architecture"* — and the classification now says exactly what that costs: **the
widened sum feeds the single worst path in the design.**

That does not make the repair wrong. A queue that under-reports its occupancy
into the generation-wrap drain is a correctness defect, and §5.2 orders the
correctness patch first on purpose. But the honest statement is that O1 very
likely made 75.79 MHz slightly worse, and the next owner fit is what will say by
how much. **The remedy is THIRD, not a revert:** a registered
accepted-minus-popped count removes the whole sum from the path rather than
trimming a term from it.
