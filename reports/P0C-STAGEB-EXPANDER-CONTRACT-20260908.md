# Stage B's expander — the contract it must reproduce, read out of fragrob

Stage B's acceptance test is *"the expander must reproduce fragrob's accepted
request sequence bit-identically"*. That is only checkable if the sequence is
written down first. This is it, traced from
`fpga/rtl/texture/zhao_texture_fragrob.sv`.

## The expansion, in four steps

**1. Accept.** `accept_c = f_valid_i && f_ready_o` (:356). `alloc_valid_o` and
the allocation event fire on the same term, so ONE fragment is admitted per
accepted beat and its slot is `alloc_slot_c`.

**2. The request mask, from the sample count** (:359-367):

| `f_sample_count_i` | `req_mask_c` | requests |
|---|---|---|
| 0 | `3'b000` | **none** — a zero-work fragment issues nothing |
| 1 | `3'b001` | sidx 0 |
| 2 | `3'b011` | sidx 0, 1 |
| 3 (default) | `3'b111` | sidx 0, 1, 2 |

**3. The queue writes** (:744-750). All required samples are pushed in ONE
cycle, to three consecutive addresses computed from the same write pointer:

    for s in 0..2: if (req_mask_c[s]) wq_m[wq_wa_c[s]] <= {alloc_slot_c, 2'(s)};
    wq_wp_q <= wq_wp_q + (WQW+1)'(f_sample_count_i);

`wq_wa_c[s] = (wq_wp_q + s)[WQW-1:0]`, so entry `s` lands at `wp+s` and the
pointer advances by the COUNT. **Order is therefore ascending sidx within a
fragment, and FIFO across fragments.** Note the pointer advances by
`f_sample_count_i` and NOT by `$countones(req_mask_c)` — those agree for 0-3
and the `default:` arm makes count 3 the only multi-mapping, but an expander
that advanced by the popcount would diverge the moment a count above 3 appeared.

**4. Fragrob's drain is TWO STAGES because ITS descriptor is in a RAM**
(:374-395): `I_IDLE → I_READ → I_HOLD`, with
`tmu_valid_o = (i_st_q == I_HOLD)`. The request presents `{slot, sidx, gen}`
plus `u`, `v`, `binding`, `lod` read from fragrob's own
`desc_u_m`/`desc_v_m`/`desc_met_m [3][DEPTH]` copies.

> **CORRECTION to the first version of this document.** I originally wrote that
> "an expander that presents combinationally is not reproducing this contract".
> **That is wrong**, and it would have sent the implementer to build fragrob's
> latency for no reason.
>
> The two-stage drain exists because FRAGROB HOLDS ITS OWN DESCRIPTOR COPIES.
> The architecture's §1.4 is explicit that the expander does not: it takes `u`/`v`
> from PERSPUV's output directly (`pu_u`/`pu_v`) and its binding/LOD from *"the
> attribute-table reads that island_top:735-780 already does today"*, keeping
> `f_binding_c + s` (island_top:869-881) verbatim. **No private descriptor RAM,
> therefore no bank-read stage.**
>
> What must be reproduced bit-identically is the request SEQUENCE — which slot,
> which sidx, in what order, how many. The LATENCY of producing it is a
> different property, and fragrob's is an artefact of storage the expander does
> not have.

## What the expander must NOT carry

Per the architecture's §0 decision, the expander holds no results, no ordering
and no lifetime. Everything above is request GENERATION. The state fragrob keeps
alongside it — `iss_q`/`auxiss_q` (:210-211), the claim/commit bitplanes, the
ROB — is what v3own replaces and what the deletion ledger removes.

## The falsifier, restated concretely

The architecture asks for *"exact per-fragment issue counts, not just output
equality"*, citing the counters-see-what-pictures-cannot law. Concretely, for a
replayed workload:

* the request SEQUENCE matches element for element, modulo the declared
  slot4→slot6 and SRCW-16→18 re-map;
* `$countones` of issued requests per fragment equals `f_sample_count_i`
  exactly — not ≥, not "eventually";
* a `f_sample_count_i == 0` fragment issues **zero** requests and still
  allocates.

That last case is the one an implementation is most likely to get wrong, because
it is the only one where an accepted fragment produces no downstream traffic at
all — and `zhao_texture_v3own` has a matching provision (§9.1's "admission can
create a zero-work ready owner"), so the two must agree.

## Status

Contract captured; **no RTL written**. Stage B's prerequisite (`cache_pipe`'s
`SRCW` parameter) is landed and green. The expander is writable against this
document rather than against a reading of fragrob taken at the keyboard.
