# Contract — MEM.ENGINE1.RAWLAST.V2

> Leaf: `zhao_engine1_raw_last_v2` · status: specified, not yet adopted by the shell

## Scope and boundary

This leaf turns one **accepted, muxed guard request** into an ENGINE1 raw-halfword
retirement contract. `guard_accept_i` is the physical acceptance event
(`guard_req.valid && guard_rsp.ready`) supplied by the mux. The leaf must not
recreate source-valid, inspect an address, or own a client ID. It has no data
path and does not validate address, byte-enable, or client identity; the mux and
guard own those decisions.

A source that disappears before guard acceptance creates no leaf event: without
`guard_accept_i`, the leaf captures nothing, changes no state or observation, and
cannot produce a raw event. If the mux has already captured an offer and then
observes that offer disappear or change, that is a mux-level clearable
offer-source protocol fault; it is still not a leaf acceptance.

## Reset and state

The clocked state is `IDLE -> PENDING -> ARMED`. Asynchronous active-low reset
clears state, expected and retired counts, legality, and the reset-lifetime
structural-fault latch. The aliases are direct observations:

* `idle_o` iff state is `IDLE`;
* `pending_verdict_o` iff state is `PENDING`;
* `armed_o` iff state is `ARMED`.

Once `structural_fault_o` is latched, the leaf is externally idle/disarmed,
exposes zero expected and retired counts, and ignores subsequent traffic until
reset. A fault is therefore not a recoverable denial.

## Acceptance and verdict

1. Only an acceptance in `IDLE` is legal. On that edge capture
   `len_bytes_i[6:1]`, exactly `len_bytes_i >> 1`, as the expected halfword
   count, clear retired to zero, and latch whether the original byte length is
   legal.
2. The only legal byte lengths are **16, 32, and 64**, hence expected counts
   **8, 16, and 32**. An illegal length still has the captured `len >> 1`, but
   its legality latch must remain false.
3. While `PENDING`, wait for the later registered verdict. Neither verdict bit is
   legal outside this state. Exactly `ok` arms a legal request; exactly `denied`
   returns
   to clean `IDLE`, clears expected/retired/legality, and retires zero. Denial is
   not a fault.
4. An approval of an illegal length is a structural/protocol fault. Approval
   and denial together are always a fault and produce no disposition.
5. A verdict when not `PENDING` (including on the acceptance edge, in `ARMED`,
   or after completion) is a structural/protocol fault.

## Raw retirement and LAST

After approval, each `raw16_valid_i` pulse while `ARMED` retires exactly one
ENGINE1-qualified raw 16-bit halfword. There is no leaf ready, data, or physical
input-LAST port. `expected_halfwords_o` and `retired_halfwords_o` expose the
registered counts.

`raw_last_o` is a combinational sideband and is high **only** when all of these
hold in the same cycle:

* the leaf is `ARMED`;
* `raw16_valid_i` is high;
* the next retired count equals the captured expected count; and
* `structural_fault_o` is low.

Thus LAST occurs only on the exact final halfword: beat 8, 16, or 32 for the
legal 16-, 32-, or 64-byte request. LAST is low when raw valid is low. The final
valid edge clears the state and all counters/legality back to clean `IDLE`; a
subsequent raw pulse is not an extra retirement.

Raw valid outside `ARMED`—before approval, while pending, after denial, after
completion, or while externally faulted—is a protocol event and latches the
reset-lifetime structural fault. The leaf likewise faults on an invalid state,
an acceptance outside `IDLE`, or any other verdict outside `PENDING`.

## Mux integration invariant

The mux must retain the accepted request and its verdict context under
backpressure, then qualify only the ENGINE1 raw stream after approval. Its
physical raw LAST check must be independent of the action/packing terminal:
track the captured request's exact raw count and compare the incoming physical
LAST with `raw_next == expected_halfwords`. A missing or mismatched physical
LAST must not be repaired by silence, a logical geometry LAST, route selection,
or a shared counter. No data or client-ID semantics are added here.

## Directed controls and committed mutants

The directed control is `tests/memory/engine1_raw_last_v2_directed.cpp`.
Acceptance criteria cover all three legal lengths, approval sequencing, clean
zero-retirement denial, pending/armed/idle observations, raw-valid-before/
after-approval faults, verdict overlap and unsolicited-verdict faults, reset
fail-stop and reset recovery, and exact final LAST timing. Early-LAST,
missing-LAST, late-LAST, and invalid-state controls are represented by the
committed mutants in `tests/mutants/zhao_engine1_raw_last_v2_mutants.sv`; the
mutants are test instruments only and are not production adoption.

This document records the contract, not an execution result. Packet H is
explicitly excluded and **not adopted**. There is no fit, resource, timing,
board, or shell-integration claim here; the referenced render-asset mux remains
outside the adopted shell boundary.
