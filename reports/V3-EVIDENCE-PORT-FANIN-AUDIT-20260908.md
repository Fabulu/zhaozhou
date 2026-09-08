# The V3 top's evidence ports: fan-in audit

The owner brief's §3.1 asks for exactly this and no more: *"Audit the fan-in of
this top's evidence ports, add focused checks, and move on."*

**30 evidence ports. 3 with dead fan-in.** The brief named all three.

| port | driven by | status |
|---|---|---|
| `err_class_invalid_o` | `fr_alloc_valid && f_class_bad_c` | **permanently zero** — `fr_alloc_valid` has 0 drivers |
| `err_fragrob_wq_overflow_o` | `fr_wq_overflow` | **permanently zero** — 0 drivers |
| `err_fragrob_id_error_o` | `fr_id_error` | **permanently zero** — 0 drivers |
| the other 27 | — | live |

`fr_combiner_unfrozen` is also undriven, but nothing reads it, so it is dead
code rather than a dead observable.

## The brief was more accurate than my tool

A quick heuristic audit flagged **four**, adding `err_aux_degenerate_o`. That
one is a **false positive**: it merely shares an `always_ff` with two of the
real ones, and its own source `aux_degenerate` is driven by the aux_pipe
instance. Proximity is not fan-in.

Worth writing down, because the correction runs the direction one does not
expect: the outside review named three and meant three, while the local
mechanical check over-reported and would have sent someone to repair a working
port.

## Every one of these passes the checker I wrote and proved

`undriven_outputs.py` reports *"every declared output has a driver"* for this
file, and that is **true**. All three ports have real `always_ff` assignments.
The defect is one level up the cone, and the brief's distinction — direct driver
presence versus transitive live fan-in — is the property that matters.

A generalised fan-in checker was attempted and reverted: it flagged 23 signals
in this file of which about four are real, the rest being arrays written as
`mat_m[idx] <= ...` and input ports. The brief forbids the parser project in the
same paragraph that names the gap, and it is right to.

## Also found, not a fault

`fr_aux_rslot` / `fr_aux_rgen` are still assigned at :2498-2499 but nothing
reads them — the AUX return now takes `aux_out_tok` whole. Harmless to
synthesis, misleading to a reader, and they should go with the §3 repairs.

## Repairs, BLOCKED until the fit releases the closure

All three touch `zhao_texture_island_v3_top.sv`, which is in the running fit's
18-file closure. The brief: *"Do not change the source under that run."* The
frozen specimen's digest matches the committed file byte for byte, and it stays
that way until the receipt lands.

Then, per §3.1:

* **A.** `err_class_invalid_o` from `own_adm_accept && (frag_class_i == CLS_ERR)`
  — the actual ingress beat and *that same beat's* raw class. Not the
  planner-stage `f_class_bad_c`. **This is the same input-versus-planner
  misalignment that cost gate 2 two debugging rounds, still live in the error
  path** — the data path was repaired and its observability was not.
* **B.** the identity sticky must **name** which of range / stale / unsolicited /
  duplicate / illegal issue / unauthorized final it covers. The current
  `cnt_fragrob_id_errors_o` sums three of six.
* **C.** `err_fragrob_wq_overflow_o` gets a real capacity-violation event from
  the expander, or a versioned retirement. Not tied to zero and called
  preserved. `valid && !ready` is backpressure, not overflow.

And per §3.2, each gets a **positive** fault test: clean traffic clear → inject
the condition → that exact observable moves → legitimate traffic resumes. A
permanently-zero flag passes a healthy-run test, which is how all three survived
119 green checks.

## The positive fault test exists and FAILS, which is the point

`tests/texture/island_v3_fault_directed.cpp`, run against today's RTL:

```
[island_v3_fault_directed] 1/5 checks FAILED
FAIL: AND err_class_invalid_o MOVED ... expected 0x1, got 0x0
```

The four checks that PASS are what make the failing one mean something:

| check | result | why it matters |
|---|---|---|
| clean traffic was actually admitted | pass | a phase that admits nothing makes everything below vacuous |
| clean traffic leaves the flag clear | pass | the healthy-run half, which is all the suite had |
| invalid-class fragments were **accepted at ingress** | pass | so "the counter did not move" cannot mean "nothing arrived" |
| **the observable moved** | **FAIL** | the port is a constant zero |
| legitimate traffic still admitted afterwards | pass | the island does not wedge on an invalid class |

Without the third of those, the failure would be ambiguous — a counter that
stays at zero because nothing reached it looks identical to one wired to
ground. The test admits the bad fragments first and *then* asks.

This is the shape the brief asks for in §3.2, and it is now demonstrated on the
real defect rather than asserted. When repair A lands, this test flips to 5/5
and gets its `add_test` line in the same commit — so it is seen to fail and then
to pass, which is the only ordering that proves it can see the thing it tests.
