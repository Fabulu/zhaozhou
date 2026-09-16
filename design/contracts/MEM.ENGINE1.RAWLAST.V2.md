# Contract — MEM.ENGINE1.RAWLAST.V2

> Leaf: `zhao_engine1_raw_last_v2` · status: specified, not yet adopted by the shell

## Scope and boundary

This leaf turns one **accepted, muxed guard request** into an independently
validated ENGINE1 raw-halfword stream. `guard_accept_i` is the physical
acceptance event (`guard_req.valid && guard_rsp.ready`) supplied by the mux. The
leaf must not recreate source-valid, inspect an address, or own a client ID. The
mux and guard retain those decisions.

The leaf receives two facts from different boundaries:

* each `raw16_valid_i/raw16_data_i` pulse is one ENGINE1-qualified raw halfword;
* `controller_retire_halfwords_i` is the controller's independent physical
  retirement count, in halfwords, wired from `client_rsp[3].credits`.

The raw stream cannot prove its own terminal condition. Controller retirement is
the independent fact against which its count and final candidate are checked.

A source that disappears before guard acceptance creates no leaf event: without
`guard_accept_i`, the leaf captures nothing, changes no state or observation, and
cannot produce a framed event. If the mux has already captured an offer and then
observes it disappear or change, that is a mux-level clearable offer-source
protocol fault; it is still not a leaf acceptance.

## Reset and public state

The public state remains `IDLE -> PENDING -> ARMED`, encoded 0/1/2. Internally,
`ARMED` contains both streaming and `FINAL_HELD`; keeping public state 2 and
`armed_o=1` while the final tuple is stalled preserves the established observer
contract.

Asynchronous active-low reset clears state, both counts, legality, final payload,
and the reset-lifetime structural-fault latch. The aliases are direct
observations:

* `idle_o` iff state is `IDLE`;
* `pending_verdict_o` iff state is `PENDING`;
* `armed_o` iff state is `ARMED`, including `FINAL_HELD`.

Once `structural_fault_o` is latched, the leaf is externally idle/disarmed,
exposes zero expected and raw-retired counts, suppresses framed output, and
ignores subsequent traffic until reset. A fault is not a recoverable denial.

## Acceptance and verdict

1. Only an acceptance in `IDLE` is legal. On that edge capture
   `len_bytes_i[6:1]`, exactly `len_bytes_i >> 1`, as the expected halfword count,
   clear both retirement counts, and latch whether the original byte length is
   legal.
2. The only legal byte lengths are **16, 32, and 64**, hence expected counts
   **8, 16, and 32**. An illegal length still has the captured `len >> 1`, but its
   legality latch remains false.
3. While `PENDING`, wait for the later registered verdict. Neither verdict bit is
   legal outside this state. Exactly `ok` arms a legal request; exactly `denied`
   returns to clean `IDLE`, clears the request state, and retires zero. Denial is
   not a fault.
4. Approval of an illegal length is a structural/protocol fault. Approval and
   denial together are always a fault and produce no disposition.
5. A verdict when not `PENDING`—including on the acceptance edge, in `ARMED`, or
   after completion—is a structural/protocol fault.

## Raw stream and independent retirement

After approval, every `raw16_valid_i` pulse in streaming `ARMED` advances the raw
count exactly once. A nonzero controller retirement advances the independent
controller count by `controller_retire_halfwords_i`, whose legal per-cycle range
is 1–8.

At every nonterminal controller-retirement edge, cumulative controller count must
equal cumulative raw count through that edge. Controller retirement is illegal
outside streaming `ARMED`, above eight halfwords in one cycle, or beyond the
captured request length.

When controller retirement reaches the exact request length, all of the following
must be true on that edge:

* cumulative raw count through the edge equals the expected count;
* an exact final raw candidate exists, either captured earlier or arriving on
  that same edge;
* no extra or duplicate terminal raw pulse has appeared.

This makes a genuinely missing final raw pulse observable at physical retirement.
Silence cannot manufacture completion, and the raw-valid count is not reused as
its own checker.

## Framed output and LAST

Nonterminal raw halfwords pass directly as
`framed_raw_valid_o/framed_raw_data_o` with `framed_raw_last_o=0`. Because the
controller raw edge has no backpressure, `framed_raw_ready_i` must be high for
every such tuple; otherwise the leaf enters fail-stop.

The exact final raw halfword is withheld until independent controller retirement
validates the complete request. It then enters `FINAL_HELD` and presents a
conventional held tuple:

* `framed_raw_valid_o=1`;
* `framed_raw_data_o` is the captured exact final halfword;
* `framed_raw_last_o=1`;
* all three remain stable until `framed_raw_ready_i=1`.

Only that handshake returns the leaf to clean `IDLE` and clears request-local
state. `raw_last_o` is a legacy alias of `framed_raw_last_o`; it no longer
observes the unregistered input cycle. Raw or controller evidence after entering
`FINAL_HELD` is a fault, not another retirement.

## Structural-fault set

The reset-lifetime fault includes invalid state, overlapping acceptance,
unsolicited or simultaneous verdicts, illegal approval, raw or controller
retirement in the wrong state, an illegal controller-credit shape, either count
exceeding the request, raw/controller mismatch at a retirement boundary, missing
or mismatched final evidence, a duplicate final raw candidate, action/checker
mismatch under mutation, nonterminal framed backpressure, and framed-LAST
mismatch.

The checker operands are intentionally independent of mutant-selectable action
expressions. A detector cannot cite two quantities clocked by the behavior it is
supposed to police.

## Mux/controller integration invariant

The mux retains the accepted request and verdict context under backpressure and
qualifies only ENGINE1 raw data into this leaf. The controller-credit input is
wired directly from the ENGINE1 client response retirement—not synthesized from
`raw16_valid_i`, framed valid, LAST, geometry completion, or route selection.
The terminal adapter must hold complete downstream publication identity until its
consumer accepts it; it may not use this leaf's return to `IDLE` as permission to
change unrelated held metadata.

## Directed controls and committed mutants

The directed control is `tests/memory/engine1_raw_last_v2_directed.cpp`.
Acceptance criteria cover legal 16/32/64-byte requests, full 8-word and split
3+5 controller credits, delayed physical retirement, terminal payload arriving
before retirement, `FINAL_HELD` backpressure/stability, a real missing final raw
pulse, short/mismatched/excess credits, denial, fail-stop/reset recovery, and all
public observations.

Early-LAST, missing-LAST, late-LAST, and invalid-state controls are represented by
committed mutants in `tests/mutants/zhao_engine1_raw_last_v2_mutants.sv`. Their
inverse-polarity tests require the independent exact detector to fire; the
mutants are test instruments only and are never production sources.

This document records the contract, not an adoption or fit result. Packet H is
explicitly excluded and **not adopted**. There is no resource, timing, board, or
shell-integration claim here; the connected controller/mux/guard/framer gate and
Packet-H sibling shell remain open.
