# DECISION: `lane_desync_o` arm (a) is a FALSE POSITIVE and must be replaced, not gated

Coordinator, 2026-09-28, under the vacation directive's delegation. Decided,
with rationale and consequences, rather than handed back as a question.

## The question

Both field console forms exit 1 on `fld_earth_lane_desync_o`. What is the
repair?

## The measurement that settles which arm

`zhao_field_earth_adapter` sums three conditions into one counter, so its value
could never say which moved. A bench-side hierarchical probe
(`tb_zhao_console_core_smoke.sv`, committed) splits them:

```
desyncarm total=1  a[vtx_live!=ans_ready]=1  b[vtx_fire&&rep_idx!=lanes]=0
                                              c[E_REQ&&cap_a!=lane_a]=0
```

The probe's total agrees with the adapter's own `desync=1`, which is what makes
the split evidence rather than three new numbers. **Arm (a), and only arm (a).**

## Why arm (a) cannot hold at this composition

Arm (a) is deliberately UNGATED -- `vtx_live != ans_ready_i`, every cycle -- on
the stated ground that *"`zhao_terrain_patch` raises `busy` on the same vertex
accept this module raises `vtx_live` on, and clears it on the same last-lane
handshake."*

* **`zhao_terrain_patch` has no `busy` output.** The justification names a port
  that does not exist.
* **The console wires `.ans_ready_i(tvj_a_ready)`**, and
  `zhao_terrain_veljoin.sv:278` drives `a_ready_o = fork_open && both_ready` --
  a downstream flow-control READY.

A ready and a busy are not the same quantity and are not complements. A ready
may sit high while idle; a busy may not. **Arm (a) therefore asserts a property
no correct console can satisfy**, and it fires once per run on a design whose
results are independently correct (`runs=1089 faults=0`,
`field_composed=1024 token_refused=0`, tile 212 at the consumer, 6 restored
under the control).

**A detector that fires on a correct design is broken in the expensive
direction**: false reds get silenced, and the silencing takes the real faults
with it. That is precisely how this entry was inherited -- "do not silence it"
was the right instruction while the cause was unknown, and it is now known.

## DECIDED

**Arm (a) is to be REMOVED and replaced by a consumer-side property. Arms (b)
and (c) are RETAINED unchanged.**

Rationale, and what is superseded:

1. **Gating arm (a) is REFUSED**, on the file's own reasoning: *"Gating the
   check on `vtx_live` would have made it blind to the one direction that
   matters most: the consumer holding a vertex open that this module believes
   is finished."* Gating converts a false positive into a blind spot.
2. **Wiring a `busy` into `ans_ready_i` is REFUSED, and this is the trap worth
   recording.** `ans_ready_i` is load-bearing FLOW CONTROL for the answer
   handshake. Replacing it with a busy would change when answers are accepted --
   a functional change made to satisfy a detector. The obvious repair is the
   wrong one.
3. **A dedicated `ans_busy_i` port fed by a new `zhao_terrain_veljoin.busy_o`
   is possible but REJECTED as disproportionate**: a new port on two production
   modules, both wrapper mutants, a bench wire and a reader -- four costs -- so
   that a detector can observe a quantity the consumer does not otherwise need
   to expose. A detector requiring a dedicated port from its subject is a design
   smell, not a fix.
4. **The replacement shape already exists in this file and is named in it.**
   `err_overwrite_o` watches the same class of property *from the CONSUMER's
   side, where no producer term can cancel it* -- the shape that fixes this
   class. The replacement for arm (a) must be a property expressible from
   signals that exist at this composition, differenced across two independently
   loaded enables, exactly as arms (b) and (c) already are.

**Consequence:** until the replacement lands, the field forms keep exiting 1,
and that is correct -- the counter is telling the truth about a detector whose
invariant the wiring does not supply. **The bench assertion is NOT to be
weakened in the meantime**, because a red with a written cause is cheaper than a
green with an unwritten one.

## Not done here, and why

The replacement property needs the answer-consumer's vertex lifecycle read
properly -- `zhao_terrain_veljoin` is a combinational fork whose readiness is
`fork_open && both_ready`, and whether it has any vertex-scoped state worth
differencing is a question for the block, not for this note. **That is a packet
with a test, not an edit**, and landing a rewritten safety detector unreviewed
would repeat the mistake this note exists to correct.

**What IS done:** the arm is identified by measurement rather than argument, the
committed probe makes it reproducible, and the two wrong repairs are recorded so
the next person does not spend the night on either.
