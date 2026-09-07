# V3.1 O1 — the ready queue's false-empty, confirmed, audited, and bounded

*2026-09-07. The owner's control-fabric recovery architecture
(`ZHAOZHOU_V3_1_CONTROL_FABRIC_RECOVERY_ARCHITECTURE_2026-09-07.txt`) names one
defect by hand in §5 and asks in §5.2 for three things: include `ld_q` in
occupancy, add the three-edge counterexample to the queue's directed test, and
**audit every consumer of `occ_o` and every use of an empty/quiet reduction**.
This is that audit. §16.3 calls the same work O1, "queue accounting correctness
repair".*

---

## 1. The defect, confirmed in the source

> `zhao_texture_v3rq.occ_o` counts body entries plus its two visible head
> registers but omits `ld_q`, the outstanding synchronous body-read return.
> Consequently it can report zero while it still owns a ticket.

`zhao_texture_v3rq.sv:84`

```systemverilog
assign occ_o = body_occ_c + (PW+1)'(h_v_q) + (PW+1)'(s_v_q);
```

`zhao_texture_v3rq.sv:91` — **the block's own internal accounting already gets
it right**:

```systemverilog
assign reserved_c = 3'(h_v_q) + 3'(s_v_q) + 3'(ld_q);
```

So this is not a missing concept. The pending read is understood, counted, and
used to throttle launches; it is omitted from **one** expression, the exported
one.

### The mechanism, and why a single cycle is enough

`rp_q` advances when the read is **issued**, not when it lands:

```systemverilog
if (ld_c) rp_q <= rp_q + (PW+1)'(1);
ld_q <= ld_c;
```

so the entry leaves `body_occ_c` on that edge and only arrives in a head
register on the next one, when `ld_q` is set and `rd_data_c` is placed. Between
the two it is in neither term.

```
edge 1   one push accepted          body 1, ld 0, heads empty    occ 1
edge 2   read issued, rp advances    body 0, ld 1, heads empty    occ 0   <--
edge 3   data lands in the head      body 0, ld 0, head valid     occ 1
```

**The port's own comment describes the trap it then falls into**, which is why
this is worth writing down rather than just fixing:

> TOTAL tickets held, body plus head registers. The drain/quiescence test needs
> "this queue holds nothing", and a body-only occupancy answers a different
> question while looking like the right one.

## 2. The consumer audit (§5.2)

`occ_o` has exactly **one** consumer. All three instances — `u_rq_tmu`,
`u_rq_aux`, `u_rq_init` — land in `rq_occ_c[0..2]`, and those appear in exactly
one expression, `zhao_texture_v3own.sv:764`:

```systemverilog
assign quiet_c = (live_cnt_q == '0) && (unf_cnt_q == '0)
              && ...
              && (rq_occ_c[0] == '0) && (rq_occ_c[1] == '0) && (rq_occ_c[2] == '0)
              && ...
              && (cq_occ_c == '0) && (cmb_res_q == '0)
              && (oq_occ_c == '0) && (out_res_q == '0);
```

`quiet_c` is the **generation-wrap drain**, and it also feeds admission:
`adm_ready_o = (live_cnt_q < OWNERS) && (!wrap_block_c || quiet_c)`.

### The other empty/quiet reductions in the same expression are correct

`cq_occ_c` and `oq_occ_c` are plain `wp - rp` differences over inline queues
with no pending-read stage — so the pointer difference **is** their whole
occupancy. And critically, `quiet_c` ANDs `cmb_res_q` and `out_res_q` beside
them, which are the reservation counters covering exactly what a pointer
difference cannot see.

**So `zhao_texture_v3own` already applies this discipline everywhere else.**
The `ld_q` omission is the single place the same care was not taken, which is
the strongest argument that it is an oversight rather than a design position.

## 3. Is it exploitable today? Not at this consumer — and that is not a defence

`quiet_c`'s first term is `live_cnt_q == '0`, and

```systemverilog
assign live_next_c = live_cnt_q + CNTW'(adm_fire_c) - CNTW'(out_fire_c);
```

An owner is live from admission until ordered-output acceptance. A ticket
sitting in a ready queue belongs to an owner that has been admitted and not yet
retired, so `live_cnt_q > 0` whenever any ready queue holds one — **including
the in-flight cycle**. The false zero is therefore masked at its only consumer,
and the generation-wrap hazard is not reachable through it today.

That is exactly the caution the brief itself raises:

> Its relevance to each caller must be established separately; not every
> false-zero observation implies immediate data loss.

**Masked today is not safe tomorrow, and here the reason is specific rather
than general.** §0 of the V3.1 rearchitecture proposes to *"separate quiescence
from hot admission"* and to replace the broad quiescence feedback path
outright. The term doing the masking, `live_cnt_q`, is precisely the kind of
global count that rework removes from the hot path. A latent defect whose only
shield is a signal the next milestone is scheduled to move is a defect to fix
**before** the milestone, not after.

## 4. What is being changed, and what is deliberately not

**Changed:** `occ_o` gains the `ld_q` term, and `zhao_texture_v3rq` gains its
first directed test — it had a lint lane and nothing else.

**Deliberately not changed**, because §5.2 says so in as many words:

> This first patch is for correctness. It can temporarily make a combinational
> count wider. Do not call that the final timing architecture. The subsequent
> patch replaces the aggregate expression on important paths with a registered
> logical count.

So the registered logical credit of §5.3 is **not** attempted here, and this
patch must not be reported as a timing improvement. It widens a combinational
sum by one term on a path that already sums three.

**Also noted, not acted on.** §5.3 warns: *"A body of 64 plus two heads must
not silently advertise 66 logical owner credits."* `full_o` is
`body_occ_c == DEPTH` with `DEPTH = OWNERS = 64`, so the queue can hold 64 body
entries plus two heads plus one pending read — **67 logical tickets against a
64-owner namespace.** Whether any caller can actually push a 65th is a separate
question about the admission path, and it is not answered here.

## 5. Evidence

The test asserts the invariant rather than the three named states: between an
accepted push and its pop, occupancy is never zero. It also runs a sustained
200-cycle push/pop stream asserting the same property every cycle, so a repair
that special-cases only the empty queue does not pass, and it checks that the
stream actually moved work so the assertion cannot be vacuous.
