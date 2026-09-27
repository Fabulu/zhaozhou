# DECISION 2026-09-27 — I34's GATHERING FRONT IS PRICED. **+11,979 ALUTs AND +9 DSP, AND IT STILL MISSES THE CLOCK CONTRACT. NOT COMPOSED.**

Taken by packet LANESCOST under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0. It **discharges** the
evidence question GATHERFRONT left open, **changes no production RTL**, and
**does not reopen** the field-major commission.

---

## THE QUESTION

GATHERFRONT built I34's gathering front, proved it exact (÷4, 31 checks, a
committed mutant seen to fail), left it in production RTL at its scalar default
— and refused to compose it for a reason exactly one measurement wide:

> *"It needs `FAB_LANES`→4 = four datapath replicas on a device already over on
> ALMs, and **this packet has no area number**. Owed first: one leaf map at
> `FAB_LANES` 1 vs 4."*

**What does composing it cost in ALUTs, registers, memory and DSP?**

## THE DECISION

**REFUSED, ON THE NUMBER. It costs +11,979 combinational ALUTs — 14.3% of the
shipping part's entire logic budget — and +9 DSP, which is MORE THAN THE WHOLE
CAMPAIGN'S LIFETIME DSP PROGRESS, and at that price it lands at 3.21× the
6,000-clock contract rather than under it.**

This is **not** GATHERFRONT's refusal inherited. That one was *"I have no
number"*; this one is *"here is the number and it decides"*. **The evidence
question is closed forever.** What is refused is **spending the area now**.

### The measurement

Four leaf `-MapOnly` rows of `zhao_field_host_v2`, all on the **shipping part
`5CSEBA6U23I7`**, all `rtlCleanAtHead: true`, same 29-file closure, same
`sourceDigest 92df1324cd4c`, all at **`zhao_console_core.sv`'s own twenty
parameters** for `u_field_host` — not the module defaults, which differ in five
places and are the trap `zhao_field_host.sv:115` names.

| label | est ALM | ALUT | registers | mem bits | DSP | vpins |
|---|---:|---:|---:|---:|---:|---:|
| **A** `L1G1F1` — the console as composed today | 39,252 | **42,789** | 48,997 | 85,282 | **15** | 3,431 |
| **C** `L1G4F1` — + `FAB_GROUP_PTS` 1→4 | 41,267 | 45,723 | 52,045 | 85,282 | 15 | 3,431 |
| **D** `L4G4F1` — + `FAB_LANES` 1→4 | 44,225 | 50,793 | 56,013 | 160,930 | 24 | 3,431 |
| **B** `L4G4F4` — + `FRONT_PTS` 1→4 = **the composed front** | 50,176 | **54,768** | 59,469 | 160,930 | **24** | 9,863 |

**Each ADJACENT pair differs in exactly ONE parameter.**

| step | ALUT | registers | mem bits | DSP |
|---|---:|---:|---:|---:|
| `FAB_GROUP_PTS` 1→4 | +2,934 | +3,048 | 0 | 0 |
| `FAB_LANES` 1→4 | +5,070 | +3,968 | +75,648 | +9 |
| `FRONT_PTS` 1→4 | +3,975 | +3,456 | 0 | 0 |
| **A → B, the whole bill** | **+11,979** | **+10,472** | **+75,648** | **+9** |

Receipt: `reports/synthesis/receipts/lanescost_field_host_v2_lanes_ladder.json`.
Per-row provenance incl. `topParameters`: `reports/synthesis/zhao_block_fit.json`,
rows `zhao_field_host_v2@map-*`.

### THE BRIEF'S EVIDENCE BAR COULD NOT BE MET AS WRITTEN

*"Two rows, same block, same device, differing in ONE parameter — `FAB_LANES`
1 vs 4"* **is not elaborable.** `zhao_field_v3_dispatch.sv:252` requires
`LANES <= GROUP_PTS` **and** `GROUP_PTS % LANES == 0`; at the console's
`FAB_GROUP_PTS = 1`, `FAB_LANES = 4` is illegal. The brief names that second
parameter three paragraphs later and **does not notice that it invalidates its
own bar.** The four-point ladder above meets the bar's *intent* instead.

**One hazard the next packet must not walk into:** that guard is an
`initial $fatal`, a **simulation** construct. Verilator's elaboration honours
it; nothing here demonstrates that `quartus_map` does. **Do not assume an
illegal pairing will fail loudly in a fit** — it may map a wrong circuit
quietly. Every row above is a legal pairing for that reason.

### THE PARAMETERS ARE SEEN TO HAVE TAKEN

`QUARTUS_GOTCHAS 3`: this tool accepts directives and silently ignores them, and
the only symptom is a number that does not move.

* **DSP 15 → 24** across the `LANES` step — two settings differing in
  multiplier count must differ in DSP count, and these do.
* **`zhao_field_alu_vec:u_alu` 833 → 3,571 ALUT**, itemised in the per-entity
  table as `gen_lane[0]..[3]` at 930 + 867 + 860 + 914. **Four visible replicas.**
* `GROUP_PTS` moved ALUTs and registers while leaving DSP and memory bits
  identical to the digit — the right signature for a group-width change.
* `FRONT_PTS` moved **virtual pins 3,431 → 9,863**; `req_in_i` is
  `CLIENTS*FRONT_PTS*IN_LANES*32`, so the boundary is obliged to widen.

### AND THE LEAF IS A FAITHFUL PROXY — CROSS-CHECKED, NOT ASSUMED

`reports/HANDOVER-20260919.md` attributes `zhao_field_host_v2:u_field_host`
**inside the composed console** at **40,989 ALUT / 48,614 REG / 85,282 memory
bits**. Row A, independently, reads **42,789 / 48,997 / 85,282**.

**The memory bits agree to the digit**, registers to 0.8%, ALUTs to 4.4% — and
the console row was taken on the **sizing** part, where ALUTs read **high**,
which is the direction that explains the residual. So the standing worry that
virtual pins inflate a standalone row is real, is about 4%, is not a factor,
and **the deltas transfer.**

## THE REASON, AND THE ALTERNATIVES

### 1. The price, against the ceilings that actually bind

The console needs **293,352 ALUTs against 83,820** (350%) and **375 DSP against
112** (335%); it must **remove 209,532 ALUTs**.

* **+11,979 ALUTs is 14.3% of the entire part's logic budget**, spent in the
  wrong direction. The largest single ALUT lever this campaign has ever landed
  is the flop-array move at **−1,531**. This would need **eight of them just to
  stand still.**
* **+9 DSP.** The handover's own sentence: *"DSP is a THIRD wall nobody has
  worked … and the campaign has moved it by 6 in total."* **One parameter change
  would undo the campaign's entire lifetime DSP progress, one and a half times
  over.**
* **+75,648 memory bits is the cheap half** — 1.3% of the part's 5,662,720, on
  the one axis measured at 62% with 2.1 Mbit free. That part of the bill is
  exactly the standing *trade ALMs for M10K* instruction and is not an objection.

### 2. THE DECIDING FACT: IT DOES NOT REACH THE CONTRACT

`field_gather_front_census`, 31 checks, green at this commit:

| | clk/group | per association | vs 6,000 |
|---|---:|---:|---:|
| scalar front, as composed today | 248.00 | 74,507 | 12.42× |
| **the composed gathering front** | **62.00** | **19,265** | **3.21×** |
| + `INIT_PROOF` | 30.00 | 9,761 | 1.63× |
| the ceiling | 17.30 | 6,000 | 1.00× |

The ÷4 is real, exact and measured. **It is also not enough.** And the 1.63×
line is **contingent on software this console does not contain**: kind 6's
producer is SW.STREAM, outside the console, so `INIT_PROOF` is an authoring act
on the image, not a state of this repo.

**So the proposal is: spend 14.3% of the part's logic and 8% of its DSP, and
still miss the budget by 1.63×–3.21×.**

### 3. And the shape would be spent twice

The term that would close it — two runs outstanding — is not merely unbuilt.
On this host **a run's identity IS its program slot** (`store[ctx*PLAN+pc]`,
`fab_start_ctx = cur_slot`), so it needs a **redesign of the run state machine**
*and* a second resident program slot out of eight. **A two-deep front changes
the front's own registers again**, so area spent on today's shape is area spent
on the thing the next change replaces — which is GATHERFRONT's own argument
about the scalar adapter, one level up.

### 4. And a third bill this packet did NOT price

`req_in_i` and `resp_out_o` widen by `FRONT_PTS`, so composing obliges **all
four client adapters** to present four points and read four results. **EARTH
wants to; STAMP, FLOW and WARP do not** — they would replicate their single
point and discard the surplus, paying the widening for nothing. Those adapters
are **outside this closure**, so **+3,975 on the `FRONT_PTS` step is a FLOOR.**

### Alternatives considered

* **Compose `FAB_LANES=4` and leave `FRONT_PTS=1`** (row D). **Strictly worse
  than doing nothing**: +8,004 ALUTs and +9 DSP for **zero** clocks, because
  that is precisely today's waste — four lanes recomputing one replicated point
  and discarding three.
* **A ÷2 gather at `LANES=GROUP_PTS=FRONT_PTS=2`.** Legal under the guard, and
  **nobody has named it** — the axis is continuous, not binary. *Arithmetic,
  declared as such and NOT measured*: ~124 clk/group, ~37,700, ~6.3× — so it
  costs roughly half the bill and misses by twice as much. Not worth pricing
  now; recorded so the next packet knows the option exists.
* **Compose anyway because the console does not fit regardless.** Refused: that
  argument licenses every increment, and the directive is explicit that the
  shipping target stays `5CSEBA6U23I7` and that *"a measured engineering
  impossibility is a finding, not permission to invent a pass."*

## THE FINDING THAT CUTS AGAINST THIS PACKET'S OWN CONCLUSION, STATED FIRST

**`FAB_LANES` IS NOT THE EXPENSIVE PARAMETER, AND BOTH DOCUMENTS THAT FEARED IT
WERE POINTING AT THE WRONG ONE.**

Entry I34 (G4) and the brief both frame the bill as *"four lanes are FOUR ALU
AND REGISTER-FILE REPLICAS"* on a device over on ALMs — which implies a ~4×
blow-up of a 42,789-ALUT block. **Measured, the `LANES` step alone is +5,070
ALUTs: +11%, not +300%.**

The mechanism is in the RTL and both documents had already quoted the sentence
that explains it. `zhao_field_v3_exec.sv:85`: the instruction stream, decode and
control are **shared** and *"only operands, results and products carry four
values instead of one"*. And `:89`: **`zhao_field_v3_mulbank` "has always
computed FOUR lanes per grant and the engine tied three of them off"** — three
quarters of the bank was already in the source, with Quartus pruning it, which
is why the DSP step is **+9 and not +45**.

**`FRONT_PTS` (+3,975) and the undocumented `FAB_GROUP_PTS` tax (+2,934)
together cost +6,909 — MORE than the lanes themselves.** The bill nobody
itemised is larger than the bill everybody feared.

**This is recorded before the conclusion deliberately**, because it is the fact
most likely to overturn it. It does not: +11,979 is still 14.3% of the part,
+9 DSP is still more than the campaign's lifetime progress, and the composed
arrangement still misses its contract by 3.21×. But a later packet with a real
ALUT surplus should re-read **this** paragraph rather than the scary one.

## CONSTRAINTS AND COST

* **No production RTL changed.** One fit target, one repaired tool, one new
  tool-control test, four committed map summaries and one receipt.
* **`I34` does not close, and must not be reported as closing.** Register
  measured bare: **2 → 2** (`I34`, `I55`).
* **Nothing is deleted, narrowed or tied off.** The front stays built, tested
  and in production RTL at `FRONT_PTS=1`, which is measured bit-identical.
* **Velocity's chain is untouched** — nothing composed, no port moved:
  `terrain_veljoin_directed` 19/0, `part_terrain_tap_directed` 1297/0.

## WHAT CLOSES THIS, NAMED SO IT IS A TEST AND NOT AN ARGUMENT

Per CLAUDE.md's *a refusal is only as good as its scope* and *if you write a
default, execute it* — **the condition that flips this decision to COMPOSE:**

> **Build the two-deep front and the second resident program slot, then run
> `tools/field/measure_earth_budget.cpp` at a REAL Earth program (not the
> census's deliberate two-uop floor). If that lands ≤ 6,000 clocks per
> association, the 11,979 ALUTs buy CONTRACT COMPLIANCE**, the composition
> becomes one act spent once on an arrangement known to meet its budget, and it
> is then a trade to make against the ALM liberation roadmap rather than a
> speculative spend.

**Until that measurement exists, the area is the whole objection and the number
is +11,979 / +9.** Nobody needs to re-price it: the ladder is committed and
reproducible from `design/fit_targets.yml`.

**NOT refused, and not to be re-inherited as open:** the gathering front itself;
the field-major commission; `zhao_terrain_field_walk` and `zhao_terrain_patch_acc`
as its front and back; and the eventual composition. **Only the SPEND is
deferred, and now on a measured price rather than on an absence of evidence.**

## CONSEQUENCES FOR CODE, TESTS AND COMPATIBILITY

* New fit target `zhao_field_host_v2` in `design/fit_targets.yml` (29 files,
  both packages leading), so the ladder is reproducible.
* `tools/quartus/extract_map_receipt.py` repaired — its RAM-summary self-check
  rejected `AUTO`-typed rows, i.e. the commonest output of the only run type it
  exists to read. New ctest `extract_map_receipt_controls`, 6 checks, keeping
  the guard's positive control separate and seen to fire.
* `tests/field/field_gather_front_census.cpp` now prints the measured area
  beside its clocks, replacing *"their AREA IS NOT MEASURED BY THIS FILE"*.
* **No port, no ABI, no `spec/commands.zidl` change. No `-G` default moved.**
