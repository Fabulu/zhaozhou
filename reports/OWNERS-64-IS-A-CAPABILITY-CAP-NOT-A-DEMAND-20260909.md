# `OWNERS = 64` sizes the island's largest ALM consumer -- and the measured peak is 32

> **The title of this document changed once, and the change is the finding.** It
> opened as *"...and has no demand behind it"*, which was true of the evidence
> that existed when it was written. Measuring `cnt_live_peak_o` before the stress
> phase produced **32 of 64** under ordinary composed traffic, so 64 is about 2x a
> real observed peak -- an ordinary credit-ring margin rather than an
> unjustified cap. **The lever is closed.** The final section carries the
> measurement; the sections before it are the reasoning that led to taking it,
> kept because the reasoning is what generalises.

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

---

# MEASURED, same day: 32 of 64 -- and it CLOSES the lever rather than opening it

The section above said the actionable step was to sample `ev_live_peak_o` under
real traffic and that *"if it comes back near 64, close the lever honestly
instead of leaving it open as a hope."*

It was already exposed as the island port `cnt_live_peak_o`, and
`island_composed_directed` was already reading it -- but only at the END of the
credit phase, where it asserts `== 64`. That phase shuts the consumer until
cycle 4,000 precisely to prove the ring fills, so 64 there is what the stress was
built to produce. **Nobody had looked at the value the port carries on the way
in**, and the port is a running maximum, so that value is the peak over every
ordinary phase before it.

One sample point later:

| workload | live-owner peak |
|---|---|
| `island_v3_fault_directed`, zero-work wrap traffic | 10 / 64 |
| **`island_composed_directed`, all phases before the stress** | **32 / 64** |
| production profile (`MIGRATION_SHADOWS=0`) | **32 / 64** |
| the oracle island | **31 / 64** |
| the stress phase, sink deliberately shut | 64 / 64 (by construction) |

## What 32 does to the lever

**It closes it, or very nearly.** Ordinary composed traffic -- CLUT, bilinear,
aux and mosaic work with the sink open -- already peaks at **half the ring**.
Halving `OWNERS` to 32 would leave this very workload with **zero headroom**, and
any burstier scene, deeper miss latency or slower consumer would stall on
admission. A ring at 32 is not a smaller version of this design; it is a design
that refuses fragments the current one accepts.

So the fourth lever is not the answer either, and now for a measured reason
rather than a suspected one. **`OWNERS = 64` is roughly 2x the observed peak of
the most representative workload available**, which is an ordinary and defensible
margin for a credit ring -- not the unjustified capability cap the absence of a
workload entry made it look like.

## What that leaves for the island's 3,336-ALM overage

Every lever this investigation opened is now closed or bounded:

| lever | verdict |
|---|---|
| memory-back the per-owner arrays | **impossible** -- read AND written in full every clock; an M10K has two write ports, not 64 |
| narrow the state via the monotone chains | ~320 bits, on the order of 160 ALM -- **6%** |
| the queued ROM packets (7.2-7.4) | aimed at 618 ALM of this island; **cannot close 3,336** |
| reduce `OWNERS` | **32 of 64 already used** by ordinary traffic |
| swap `rcp24_svc` for `v3` | ALM and DSP yes, but 869.8 ALM is 8% of the island |

**The island does not have a 3,336-ALM lever in it.** That is the honest
conclusion of the day's texture work, and it is a result rather than a failure:
four candidate remedies were each priced and each found insufficient, three of
them by measurement. Whatever closes this redline is either a larger
architectural change than any packet in the brief describes, or a decision to
move the redline -- and the second is explicitly the owner's under 0.2.

The measurement is printed, never asserted. A bound at 32 would freeze one
workload's incidental peak into a gate, which is the mistake this whole document
is about.
