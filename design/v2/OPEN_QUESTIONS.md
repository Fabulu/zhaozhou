# V2 open questions

Each has an owner, a way to close it, and — where it matters — what happens if
the answer is bad. Questions are not closed by argument; they are closed by a
measurement or an owner ruling.

**Baseline:** `814687ae955a3ce4690010ce750f2b7b5e597ec2`.

---

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
