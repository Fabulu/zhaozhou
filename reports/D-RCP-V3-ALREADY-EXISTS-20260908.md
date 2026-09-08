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

---

# The island-profile fit FAILED, and the cause is not yet known

`-TopParameters NCTX=8,TOKW=14` -> **`incomplete:failed:quartus_map.exe`** in
38.3 s. ALMs and Fmax never produced.

```
Error (10205): array has more than 2**28 bits
  zhao_raster_rcp24_v3.sv(146..152)
```

Lines 146-152 are the context planes, all declared `[NCTX]`. For
`logic [23:0] p_m_q [NCTX]` to exceed 2^28 bits, **NCTX must have arrived above
eleven million.**

## What this is NOT

**It is not "the block rejects NCTX=8".** I nearly wrote that. `CW =
$clog2(NCTX)` is 3 at NCTX=8 and every plane is a plain `[NCTX]` array — the
declaration is valid at 8. Nothing in the source objects to that value.

So Quartus did not receive 8. It received garbage.

## What is established, and what separates the remaining possibilities

* `TOKW=14` **alone**, at the block's native NCTX=16, cleared synthesis and is
  still fitting past 15 minutes. So the parameter-override mechanism works for
  at least one setting.
* `NCTX=8,TOKW=14` together produced a nonsense NCTX.

Two candidates remain and one cheap run separates them:

  (a) the block genuinely misbehaves at NCTX=8 in a way the source does not show;
  (b) `run_block_fit.ps1` mis-emits when given **two** `-TopParameters`, or
      Quartus mishandles two `set_parameter` lines in that QSF.

**Fit `NCTX=8` alone.** If it fails the same way, it is (a). If it succeeds, it
is (b) — and (b) would be a tool defect affecting every multi-parameter
experiment anyone runs, which matters well beyond this question.

That run is queued behind the current fit rather than launched alongside it; two
concurrent Quartus fits on one machine is how the disk filled on 2026-09-06.

## The tool's own warning, which applies in the other direction

`run_block_fit.ps1` warns that Quartus *"accepts directives and silently ignores
them, and the only symptom is a number that does not move."* Here the failure
was loud, which is the better outcome — a silently ignored `NCTX=8` would have
produced a clean row at NCTX=16 labelled `@island-profile`, and that row would
have been quoted as the island-profile measurement for as long as anyone
believed it.

**The parameter must be verified as TAKEN in any row that survives**, per the
tool's own instruction: at NCTX=8 the context storage must be visibly smaller
than the NCTX=16 rows.

## Emission verified for the single-parameter case

Read from the live fit's generated QSF in its workspace:

```
line   7: set_global_assignment -name TOP_LEVEL_ENTITY zhao_raster_rcp24_v3
line 275: set_parameter -name TOKW 14
```

One clean directive, on its own line, against the right entity. And the
PowerShell that writes it is sound: `$qsf = Get-Content ...` yields an **array**,
so `+=` appends elements and `| Set-Content` writes one per line. There is no
string-concatenation bug gluing two directives together, which was my first
suspicion.

The failed two-parameter run's workspace has been cleaned, so its QSF cannot be
read. **That leaves the question open**, and it narrows rather than settles:
emission is correct for one parameter; whether it stays correct for two, and
whether NCTX specifically survives the round trip, is what the `NCTX=8`-alone
fit decides.

A plausible mechanism worth checking when that runs: `TOKW` is consumed as a
WIDTH (`[TOKW-1:0]`), while `NCTX` is consumed as an ARRAY BOUND (`[NCTX]`) and
inside `$clog2(NCTX)`. Those are different evaluation contexts, and a parameter
delivered as a string could survive one and not the other. **That is a
hypothesis, not a finding** — it fits the evidence, which today has repeatedly
not been enough.
