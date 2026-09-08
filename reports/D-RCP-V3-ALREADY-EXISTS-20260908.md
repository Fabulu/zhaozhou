# Packet D may be an INTEGRATION, not an implementation

**`zhao_raster_rcp24_v3` already implements the preparation split the post-fit
brief §6 asks for, is fitted, is registered in the production manifest — and is
instantiated nowhere.**

This is docket M3's pattern (*"THE V3 OWNER IS INSTANTIATED NOWHERE"*) repeating
for the reciprocal service.

## The brief's N0, and the block's actual source

> *"N0 ACCEPT/RESERVE: Determine an available context under the existing
> allocation rule. Atomically reserve it and capture raw denominator, token, and
> context index. It is allocated but not eligible for micro-job selection."*

`zhao_raster_rcp24_v3.sv:457-461`:

```systemverilog
a0_v_q <= v_valid_i && v_ready_o;
if (v_valid_i && v_ready_o) begin
  a0_ctx_q    <= free_dout;      // from the free-context FIFO, not a scan
  a0_d_q      <= d_i;
  a0_tok_q    <= v_tok_i;
```

And N1 works from the **registered** value, which is the whole point:

```systemverilog
:276   if (a0_d_q[23-b] && ...) e_c = 5'(b);
:278   m_c = a0_d_q << e_c;
```

Compare `zhao_raster_rcp24_svc.sv:118-120`, which the island actually
instantiates, and which does the same search and shift **combinationally from
the input pin `d_i`**. That is the 15.416 ns cone.

The block also replaces the three context-wide scans with queues — free-context
FIFO, NEW, CONTINUATION, DONE — which is the structure behind the `c_val`
round-robin path its own source comments record at −3.243 ns.

## Measured, on leaf fits

| block | worst slack | deepest data delay |
|---|---|---|
| `zhao_raster_rcp24_svc` | **−4.607** | 16.44 |
| `zhao_raster_rcp24_v3@v3-full` | **−1.045** | 13.733 |
| `zhao_raster_rcp24_v3@v3-rh` | −1.061 | 14.336 |

## WHAT THIS DOES NOT ESTABLISH, and the list is long

The comfortable reading is *"the work is already done, just wire it in"*. That
is the explanation to check hardest, so:

* **The leaf fits are not like-for-like.** svc has 16 ports, v3 has 20; svc uses
  8 contexts, v3 uses 16. Different designs at different sizes measured
  separately. The −4.607 vs −1.045 gap is real but it is not "3.5 ns of island
  Fmax".
* **Neither leaf number predicts the composed island.** The island's RCP path is
  −2.134 with a 3.342 ns favourable skew that a leaf fit does not reproduce.
* **The port contract differs.** svc has `contexts` and `mul_busy_o` that v3 does
  not; v3 adds `mul_jobs_o`, `negcorr_jobs_o`, `occupancy_o`, `phase_jobs_o`,
  `qerr_o`, `zero_jobs_o`. 14 of svc's 16 are shared. The two svc-only ports need
  a decision, not an assumption.
* **The island parameterises `TOKW(14)`.** Whether v3 supports that profile is
  unverified; the earlier brief's warning applies — *"do not claim that changing
  GENW or owner depth works merely because the file has a parameter
  declaration."*
* **16 contexts instead of 8 costs area**, and the queues are not free. This
  could easily be an ALM increase on an island already 13,133 against a 7,500
  rule.
* **It has never been instantiated.** Every integration defect this session
  found in v3own — five stale slices, two stage misalignments, four undriven
  outputs — came from wiring a never-composed block into a top. There is no
  reason to expect this one to be different.

## What I recommend

Treat packet D as **"evaluate integrating `zhao_raster_rcp24_v3`"** rather than
"write a preparation pipeline". Writing a second implementation of a thing that
already exists, is fitted, and is manifest-registered would be the more
expensive mistake — but so would swapping it in on the strength of two leaf
numbers.

The cheap next step is a **pair fit**: `rcp24_v3` composed with `perspuv_svc`,
against the same pair using `rcp24_svc`. That measures the seam that matters at
a fraction of a 4-hour island fit, and the repo already has `zhao_pair_*`
fixtures for exactly this purpose.

**This is an owner decision**, not a barge-ahead: it changes which block is
production in a subsystem whose receipt was recorded four hours ago.

## Two of the caveats, narrowed

**Parameter profile.** `zhao_raster_rcp24_v3` **elaborates cleanly at
`TOKW=14`** — Verilator lint, 0 diagnostics, with `NCTX` at its default 16.
The island instantiates `rcp24_svc` with `#(.NCTX(8), .TOKW(14))`.

That discharges *elaboration*, and nothing more. The earlier brief's warning is
exactly about this gap:

> *"Do not claim that changing GENW or owner depth works merely because the file
> has a parameter declaration."*

A block that elaborates at a width is a block that has not yet been shown to
*work* at it. `TOKW` here carries the owner handle through a ten-clock feedback
loop and four queues; every identity defect this session found was width-legal.
The profile needs a differential run, not a lint.

**No pair fixture exists.** `fpga/rtl/synth/` holds five `zhao_pair_*` fixtures
— fragment/tilestore, pagestream/patch, setup/binner, tess/normals, tmu/cache —
and **none covers rcp or perspuv**. So the cheap measurement I recommended needs
a new fixture written first. That is a fixture, not production RTL:
`check_forbidden_sources.py` already excludes `fpga/rtl/synth/` from every
production closure, so it cannot leak into a shipped number.

## Suggested sequence, if the owner takes this route

1. Write `zhao_pair_rcp_perspuv.sv` twice-parameterised, or two fixtures — one
   binding `rcp24_svc`, one binding `rcp24_v3`, identical otherwise.
2. Fit both. **Same fixture shape on both sides** is the only way the delta
   means anything; the leaf rows above are not comparable and should not be
   quoted as the expected gain.
3. Only then decide whether the island swap is worth a 4-hour composed fit.

Step 2 is where the M6 amendment applies: a successful change *should* move the
worst-path family, so compare data delay on the preparation chain, not which
endpoint the report names.

---

# QUALIFICATION: v3 improves the `d_i` cone by 2.1 ns, not by the slack gap

Checked before recommending anything further, because *"the work is already
done"* was doing a lot of work in the section above.

| block | deepest cone | data delay | slack | skew |
|---|---|---|---|---|
| `rcp24_svc` | `d_i[18] -> c_m~23` | **16.44 ns** | −3.201 | +3.299 |
| `rcp24_v3@v3-rh` | `d_i[18] -> always0~5_OTERM385` | **14.336 ns** | −1.061 | +3.335 |
| `rcp24_v3@v3-full` | `rst_n -> altsyncram` | 13.733 ns | **+1.223** | +5.016 |

Three things follow, and two of them cut against the earlier section:

**1. v3 STILL has a `d_i`-origin cone, at 14.336 ns.** Registering `d_i` into
`a0_d_q` did not remove the deep chain from that pin in the `@v3-rh` specimen.
So the block is not a completed packet D — it is a partial one.

**2. The gain on the chain packet D targets is 2.104 ns**, not the 3.562 ns the
worst-slack comparison implies. The slack gap is inflated by structure and skew
elsewhere in the block — the queues replacing the scans, which is a different
improvement from the preparation split. **Attributing the whole slack gap to the
preparation change would be exactly the M6 error the docket already recorded.**

**3. `@v3-full`'s deepest path is `rst_n` into a RAM at +1.223 ns slack** — a
reset-distribution path, not a datapath, and comfortably positive. Its 13.733 ns
figure is therefore *not* a preparation-chain measurement and must not be
compared against svc's 16.44 as if it were. The two specimens differ in what
their deepest cone even is.

## What this does to the recommendation

It does not reverse it — evaluating `rcp24_v3` is still cheaper than writing a
second preparation pipeline, and the block genuinely does reserve at acceptance
and normalize from a register. But the expected prize shrinks:

* **~2.1 ns on the preparation chain**, measured leaf-to-leaf on the one
  comparison where both specimens actually gate on `d_i`.
* Whatever the queues-for-scans change is worth, separately, and it is not
  packet D.
* The island's own RCP path is −2.134 with +3.342 ns of favourable skew that
  no leaf fit reproduces, so neither figure converts to island Fmax.

And it strengthens the pair-fixture caution in the other direction: since the
RCP->PERSPUV seam is **already registered** (`px_r_q`, `px_k_q`, `px_dz_q`) and
the cone of interest is entirely inside rcp24, a pair fit buys little for this
question. **A same-shape leaf comparison, read on data delay, is the right
instrument** — and `@v3-rh` versus `svc` is very nearly that comparison already.
