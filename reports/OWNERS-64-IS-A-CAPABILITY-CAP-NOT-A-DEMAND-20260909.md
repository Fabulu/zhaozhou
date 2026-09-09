# `OWNERS = 64` dominates the island's largest ALM consumer and has no demand behind it

2026-09-09. Follows `V3OWN-IS-NOT-A-MEMORY-CANDIDATE-20260909.md`, which ruled
out the memory remedy and left four vague levers. This prices the fourth one and
finds the evidence gap under it.

## Why this parameter and not the others

`zhao_texture_v3own` is **2,706.7 ALM — 25% of the island and 81% of its
3,336-ALM redline overage.** Its cost is not one big structure; it is eleven
per-owner arrays, each read in full and written in full every clock, plus the
per-entry next-state logic for all of them. **Every term of that scales with
`OWNERS`.**

Rough shape, stated as a shape and not a measurement: ~1,472 bits of per-owner
state is perhaps 736 ALM at two registers per ALM, leaving roughly 2,000 ALM of
combinational next-state and dynamic-index read logic. Narrowing the state
encoding — the monotone chains the block asserts about itself, `cmt ⊆ clm ⊆ iss
⊆ req` and `fdn → fcl → cbi → crs`, which would let four monotone flags become
three bits — is worth about 320 bits, so on the order of 160 ALM. **Six percent.
Not the answer.**

`OWNERS` is the answer-shaped lever, because every one of those eleven arrays
and all of their update logic is sized by it.

## Where 64 comes from

```systemverilog
// 64 owners. Section 5.1: "The baseline owner capacity is 64."
parameter int unsigned OWNERS = 64,
```

A **baseline capacity** from a specification sentence. `design/budgets/workloads.yml`
has no entry for owners at all — not a demand, not a cap, not a deadline.

## The two numbers that exist, and why neither is a demand

Brief 2.6.E: *"Distinguish a capability cap, a stress profile, an observed demand
and a mandatory admission/deadline requirement."* All four are distinct here and
only two have values:

| | value | what it is |
|---|---|---|
| capability cap | **64** | the parameter, from a spec sentence |
| stress profile | **64** | `island_composed_directed` — *"accepted while the sink was SHUT 64, live peak 64 of 64"*. It reaches 64 **because the test shuts the consumer** to exercise the full-credit path. By construction, not by demand. |
| observed demand | **10** | `island_v3_fault_directed` — *"live peak 10"* on zero-work wrap traffic. One incidental workload, not a representative scene. |
| mandatory admission/deadline requirement | **none stated** | nothing in the contracts or the workload model |

**Quoting either number as "the demand" would be wrong in opposite directions.**
The 64 is a test that deliberately fills the ring; the 10 is whatever one probe
happened to need.

## The instrument already exists and nobody has run a scene through it

```systemverilog
if (live_next_c > peak_q) peak_q <= live_next_c;   // :1339
assign ev_live_peak_o = peak_q;                     // :1601
```

`v3own` already tracks and exports its own live-owner peak. Deriving a real
demand needs no new RTL and no fit — it needs a representative workload driven
through the island with `ev_live_peak_o` sampled. That is a simulation question.

## What this does and does not establish

**Establishes:** the parameter that sizes the island's largest ALM consumer rests
on a capability statement with no demand derivation, while the instrument to
derive one is already wired out and unused.

**Does NOT establish that 64 is wrong.** A credit ring sized to the stress case
is a legitimate engineering choice — a smaller ring that stalls under
backpressure trades area for throughput, and the composed test's shut-sink phase
is exactly the case that would suffer. **This is not a recommendation to lower
`OWNERS`.**

**And it is not free even if the demand turns out to be small.** `OWNERS` is
`SLOTW`-coupled (`SLOTW = 6`, `CNTW = SLOTW + 1`), the owner handle is carried
through RCP, PERSPUV and the expander as part of the token, and the 64-entry
address space is what the generation/identity scheme is built on. Changing it is
a contract-level change to identity transport, not a parameter tweak — which is
precisely the kind of concession brief 0.2 says a request to save resources does
not authorise.

## The actionable part

The cheapest next step on the island's ALM problem is not RTL. It is **measuring
`ev_live_peak_o` under a representative scene**, which turns the fourth lever
from "reconsider whether 64 is right" into a number — and if the answer is near
64, closes the lever honestly instead of leaving it open as a hope.
