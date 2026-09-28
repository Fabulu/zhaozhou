# V2 open questions

Each has an owner, a way to close it, and — where it matters — what happens if
the answer is bad. Questions are not closed by argument; they are closed by a
measurement or an owner ruling.

**Baseline:** `814687ae955a3ce4690010ce750f2b7b5e597ec2`.

---

> ## UPDATED after the R2 review, 2026-09-28
>
> **Q2 is CLOSED, and not in the direction either document expected.** The
> remaining storage lever is **4–7%**, not 31% and not more. The three
> conversions that mattered already landed; what is left is 87 small arrays
> needing ≥87 M10K blocks for ~15,750 ALM (~181–225 ALM/M10K) against ~238 free.
> R2's two extra data points are the already-spent conversions. See D-V2-008.
>
> **Q1 is NARROWED, not open.** `design/contracts/FIELD.SEQ.EARTH.md` already
> specifies 1,089 lattice vertices, 297 update groups, a 128-association stress
> frame and **≤850,000 Field/Earth-slice clocks**. Gate 0 becomes *extract and
> reconcile the existing contracts*, not start from blank paper. What remains
> genuinely open is the **joint** question — how Earth stress coexists with
> maximum geometry, the giant, texture and Duo.
>
> **Q3 is SHARPENED and now contractual.** 850,000 clocks × 60 fps = **51 MHz
> floor on the Field engine alone**. The witness is measured against that, not
> against 60 as a preference.
>
> **NEW Q14 — does the projector's lattice structure generalise?** R2's
> `row(i,j) = A·h + B·i + C·j + D` is sound and unexploited, but applies to the
> regular lattice only. If it is the only such reduction, the rate side is
> narrower than R3 hopes. Closes via X2 plus an audit of other regular-domain
> producers.

> ## UPDATED AGAIN after the R4 review, 2026-09-28
>
> **Q2 is REOPENED, and no percentage replaces it.** R4 showed my "4-7%" rested
> on two unsound steps: `check_ram_inference.py:901` labels an array
> ALREADY INFERRING when the MODULE SUBTREE holds at least that many memory bits
> (so an unrelated RAM can mask an array in flops), and `bits / 4` is capacity
> arithmetic rather than an integrated area measurement. The honest wording is
> R4's: **the known large in-place conversions are already incorporated; the
> remaining candidates appear less attractive; their net saving is not
> established by this scan; broader state reorganisation is a separate,
> unmeasured question.**
>
> **Q3's number is withdrawn and replaced by the contract's own.** My 51 MHz
> derivation was invalid -- a cycle CEILING plus a deadline gives a relationship,
> not a minimum clock, and 51 MHz would spend 100% of the frame anyway.
> `FIELD.SEQ.EARTH.md:48-53` states it directly instead: designed for the shared
> **100 MHz** GPU domain, **~80 MHz lowest credible**, and the measured v2 leaf
> **59.22 MHz explicitly rejected** at 108% of its own budget. The test is
> **useful work per second, complete latency and reserve together**, not MHz.
>
> **NEW Q15 -- can the uop store be banked at all, and at what schedule cost?**
> `zhao_field_v3_exec.sv:302` holds 15,360 bits in flip-flops (~62% of that
> node's registers) and Quartus names the cause: *uninferred due to asynchronous
> read logic*. Measured cause: the read enable `issue_c` (`:378`) is derived from
> `dot_inflight_c` (`:429`) which is derived from `s1_uop_r.op` -- **the store's
> own read data**. Breaking that loop needs a narrow hazard side-table or another
> stage before the hazard check. **Closes inside X2, priced as a schedule change,
> not as a saving.**
>
> **NEW Q16 -- how many other arrays are invisible to the scanner?** The
> declaration recogniser is `^\s*(?:logic|reg|bit)`, so every typedef-declared
> array is unseen. The store is one confirmed instance. A repaired recogniser
> plus a re-rank is cheap and is the only way to know whether the remaining
> storage question is small or merely unmeasured.

## Blocking — nothing should be allocated or built until these resolve

| # | question | closes via | if the answer is bad |
|---|---|---|---|
| **Q1** | **What is the legal joint workload envelope?** No numeric per-frame joint demand vector exists; `V1-RELEASE-DEFINITION.md` is in gameplay terms. | **Owner ruling (P1)**, informed by **E3** | If every declared maximum must co-occur, R0's own conjunction is 18.4× over at four lanes and full capability is not implementable on this part at any architecture. |
| **Q2** | **What is the real conversion rate of flop arrays to banked M10K?** One data point (0.521 ALM/register, favourable case) carries the entire budget. | **E1** — six conversions, map-only | Median < 0.35 ALM/register, or ≥3 of 6 needing >2 concurrent read ports, and the storage lever is materially smaller than assumed. Re-derive everything before any RTL. |
| **Q3** | **Can a minimal composed skeleton hold 60 MHz on the real `5CSEBA6U23I7` with real pins?** Only composed datum is 18.5 MHz, −44 ns slack, non-target device, virtual pins, ¼ scale. | **E2** | Below 60 MHz the frame budget shrinks, rate-bound paths need *more* parallelism, and area and time move in opposite directions. No escape from that combination. |

## Material — affect the architecture, not the go/no-go

| # | question | closes via |
|---|---|---|
| Q4 | What is the actual fabric↔HPS bandwidth and latency on this board? Decides how much of the residual a host-heavy partition can absorb — the most promising unexplored direction. | E2, extended |
| Q5 | Does "carry identities, fetch locally" pay for itself, or does it convert an area problem into a memory-service problem? | E2 traffic mix |
| Q6 | How much of the 911-node glue class is genuinely irreducible sequencing versus per-feature duplication that a scheduled engine absorbs? This is the ~50% the whole plan rests on. | a Gate-B skeleton that implements one engine and measures it against the V1 nodes it replaces |
| Q7 | DSP↔ALUT joint optimisation: V1 is at 128/112 already. `zhao_field_v3_mulbank` is 3,328 ALUTs with 8 registers — soft products. Which way should each block move? | per-block map sweeps |
| Q8 | What working sets and caches does the ratified envelope require, and do they fit the M10K left after banking? | Gate 0 output + sizing |
| Q9 | Which V1 fixed latencies are publicly observable and which are implementation choices? R0 is right to demand this be resolved explicitly; it is not resolved. | contract review against `design/contracts` |
| Q10 | Is `5CSEBA6U23I7` here the DE10-Nano/MiSTer part, and is the connected SuperStation One that part? The memory note says to use the connected board for spec runs; the exact part/package must be confirmed from the tool report before signoff. | board + tool report |

## Deferred — real, but not on the critical path

| # | question |
|---|---|
| Q11 | Oracle-discrepancy ledger: seed with the Field index-68→4 truncate-then-check, the PARAMWALK illegal-descriptor flag with no consumer, the prepared-value reachability case, and the geometry/material lifetime pairing. Each needs a reproducer preserved and an adjudication against canonical semantics rather than against whichever output is easier. |
| Q12 | Capture/ABI: confirm `.zcap`/`.zpak` and generated ABI types are sufficient for V2 observation before anyone proposes a second format. |
| Q13 | Does V1 itself get a timing programme, or is it frozen as an oracle at whatever clock it reaches? (Owner P3.) |

---

## Questions R0 raised that are already answered, and should stop being asked

* **"Is whole-console placement still an acceptance criterion?"** —
  `design/V1-RELEASE-DEFINITION.md` declares *"a complete MiSTer-targeted
  bitstream passing resource and timing analysis under documented platform
  assumptions."* It is. An escalation of mine asking this was over-asking, and
  has been retired at its site.
* **"Is the legal joint workload defined anywhere?"** — verified: it is not.
  R0's premise is correct; Q1 stands.
