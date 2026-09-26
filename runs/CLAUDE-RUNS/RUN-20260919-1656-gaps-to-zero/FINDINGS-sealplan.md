# FINDINGS -- SEALPLAN, branch `gz/sealplan`, 2026-09-26

Entry **I56**: the per-view pre-frame admission plan, its validator, and
GEOM.PARAMBUF's quota seal. Owner vacation directive 2026-09-23 **section 5**.

Three packets touched I56 and none built the seal. Each refusal was right about
what it refused. **This packet built it.**

---

## 1. The register, measured BARE, before and after

`python tools/budget/completion_register.py`

| | value |
|---|---|
| at `f4b4a653` (branch point), measured bare | **5** |
| at my last commit, measured bare | **4** |

`I56` **CLOSED**. The remaining four are `I13` (boundary), `I34` (boundary),
`I55` (unclassified) and one disconnected implementation
(`zhao_terrain_normalmap`) -- none of them mine.

### And the classification has a history that has to be stated

`completion_register.py` settles an entry on the phrase *"NOT a tie-off"* **in
the head line only**, and I56's head line ended `-- NOT a` with its body
beginning `tie-off:`. The phrase was split by a line wrap, so the entry read
`unclassified` and counted as a gap.

**GIANTQUOTA found that and refused to rejoin the wrap**, and was right:
*"REJOINING THAT WRAP WOULD CLOSE I56 BY REFORMATTING -- a free reduction of
exactly the kind that check exists to prevent."* With the seal still a constant,
closing the entry by reflowing a comment would have been exactly that.

I rejoined it, and the reason it is no longer a free reduction is the commits
above it. The head line now names the producer rather than only asserting the
negative, so a reader sees the claim being made. **If the seal ever reverts to a
constant, that head line is a false statement and not merely a stale one.**

---

## 2. The plan: who produces it, what carries it, who consumes it

    host / HPS
      -> SealFramePlan 0x0003     ABI v4, 48 bytes, spec/commands.zidl
      -> zhao_cmd_exec            decodes by GENERATED offset, checks RECORD
                                  hygiene only, stages, pulses `plan_valid_o`
                                  in the packet's COMMIT walk
      -> zhao_measure_sealplan    validates against four capacities in four
                                  units and against R7's reservation;
                                  REFUSES before the seal; otherwise seals
      -> zhao_geom_paramarena     latches the validated numbers as its quota

**The directive authorises the transport in terms** -- the architect *"is not
required to duplicate a large policy engine merely to avoid adding a command or
mailbox field"* and must *"authorize the necessary generated command,
publication or service transport and connect its real producer and consumer."*

**CMD.EXEC does not validate the plan.** It asks two questions about the
*record* -- are the reserved flag bits zero, does each 32-bit wire field fit the
18-bit seal -- and forwards. Whether a plan *fits* is a question about four
hardware capacities and a reservation, and one block owns it. Two blocks
deciding admission is two blocks that can disagree.

**A plan from an abandoned packet never reaches the validator.** The pulse is in
the commit walk and `st_plan_v` is cleared on abandon beside `st_post_v`.

### What is gone

`zhao_console_core.sv` drove the seal with `18'(GEOM_PA_MAX_VERTS)`,
`18'(GEOM_PA_MAX_TRIS)` and `18'(GEOM_PA_MAX_CHUNKS)`. Those three literals are
the state the directive names in the owner's own words: **"A constant equal to
arena capacity is not an admission plan."** They are gone.

### The default plan, and its objection answered

With no published plan the block seals a **no-giant plan at capacity**, counted
at `default_seals_o`. That is the shape the directive names, so it deserves an
answer rather than a footnote: a frame with no published plan has made no
declaration of a guaranteed giant, so the directive's own release clause --
*"A frame explicitly containing no guaranteed giant may release that
reservation BEFORE sealing"* -- applies to it.

What changed is that it is now a **case of a mechanism** rather than the only
behaviour: it goes through the same validator, it is **counted** so its use is
visible, and the moment a plan declares a giant the reservation binds. A console
that never publishes a plan can read `default_seals_o` and see that it never
did. That visibility is the difference between this and the constant.

---

## 3. The enforcement evidence

### The plan produced and the seal consumed

`measure_sealplan_directed`, **99 checks**, case 1: a plan of
`40000 / 9000 / 5000 / 21000` reaches `seal_verts_o / seal_tris_o /
seal_chunks_o / seal_refs_o`, and the case asserts **not one sealed field
equals its capacity**. That comparison is the directive's acceptance test
written as an assertion.

### The reservation enforced, refused before the seal

* **case 5** -- 14,044 ordinary CHUNKS beside the giant: REFUSED,
  `R_CHUNK_RESV`. 14,044 is far below `MAX_CHUNKS`, so nothing but the
  reservation refuses it.
* **case 6** -- ONE ordinary tile reference beside the giant: REFUSED,
  `R_REF_RESV`.
* **Every refusal case asserts `seal_valid_o` stayed LOW.** A block that
  CLAMPED an illegal plan to a legal one would move no counter and would seal a
  frame nobody authorised. *"Refused before any partial publication"* is the
  directive's wording and the absence of the seal is what *before* means.

### The reservation enforced at `ck_fits_c`, with the giant measured whole

`geom_paramarena_reservemut`, one source, two ctests, opposite polarity. The
arena is shrunk to `MAX_CHUNKS=32` / reserve 5 / ordinary quota 27 -- the
testbench exposes its capacities for exactly this -- so the experiment is 28
pushes rather than 14,044. Production's arithmetic, production's magnitudes
divided.

| | cursor | UNALLOCATED | `quota_overflow_o` | `frame_fault_o` | `giant_reserve_breach_o` |
|---|---|---|---|---|---|
| **PRODUCTION** | 27 | **5** | 1 | 1 | **0** |
| **MUTANT** (`ck_fits_c` `<` -> `<=`) | 28 | 4 | **0** | 0 | **1** |

**Read the mutant's `quota_overflow_o = 0`.** That is the finding, not a detail.
The mutation makes the allocator MORE permissive, so the frame believes it
fits: no fault, no overflow, no golden output moves, every result-checking test
goes on passing -- and one chunk of the giant's reservation has been taken. **A
silent overrun into the reserve is invisible to every other instrument in this
tree**, which is why the counter exists and why it needed a committed mutant.

The production row is the evidence bar, measured: at the moment the ordinary
stream is refused at `ck_fits_c`, the arena still holds **exactly** the
reservation unallocated -- five of five, not four and not six.

### What this does NOT do, stated so it is not inherited as more

There is **no per-chunk "this one is the giant's" class bit**, and GIANTQUOTA's
measurement that one cannot be built at that seam **stands**: a chunk is a
spatial object, the binner drains a tile FIFO in submission order, and the
14-id chunk straddling two instances belongs to both. The guarantee delivered is
stated exactly, in the block header:

> A frame that declares a guaranteed giant is SEALED AT A QUOTA STRICTLY BELOW
> ARENA CAPACITY. The ordinary stream faults at its sealed quota, at which
> moment the reserved units are still PHYSICALLY UNALLOCATED. The giant is
> whole because the ordinary budget provably cannot reach it -- not because the
> arena can tell the chunks apart.

That is weaker than a runtime partition and it is the strongest property this
seam carries. It is enough for R7's actual contract -- the giant is never
*silently truncated* -- because an overrun faults the whole frame and publishes
nothing.

---

## 4. The units, explicitly

| unit | where it is spent | capacity | the giant's reservation |
|---|---|---|---|
| VERTICES | `zhao_geom_paramarena` vertex slots | 65,536 | **none ruled** |
| TRIANGLES | arena triangle descriptors | 16,384 | **none ruled** |
| CHUNKS | arena chunk payload units | 16,384 | **2,341** = ceil(32768/14) |
| TILE REFERENCES | `zhao_geom_binner_v2.ref_ram` | 32,768 | **32,768** |

**Both halves of the 14x confusion are refused, by DIFFERENT rules:**

* 32,768 in the CHUNK field is 200% of `MAX_CHUNKS` -> `R_CHUNKS_CAP`
  (**case 9**). The loud direction.
* 2,341 as the giant's REFERENCE reserve -> `R_GIANT_TRIM` (**case 8**). The
  flattering direction: it looks like a giant paid for and is a fourteenth of
  one. *"Shrink the guaranteed giant"* is on the delegation's list of what it
  does not cover, so a trimmed reservation is not a smaller plan, it is an
  illegal one.
* **case 10 is the one that matters**: a planner who believes the giant costs
  2,341 of *something* asks for `32,768 - 2,341 = 30,427` ordinary references
  beside a guaranteed giant. Under the confusion that is a frame with
  comfortable headroom. It is REFUSED at `R_REF_RESV`, and the reason names
  REFERENCES so the reader learns which unit was wrong.

**ceil(32768/14) is an ELABORATION constant, not a wire field and not a
divider.** `GIANT_REFS` is fixed by R7 and a plan may not move it, so the chunk
payload is known at build time. Putting it on the wire would give the confusion
a second chance.

### What the reservation actually covers, and what it does not

R7 rules a number for **TILE REFERENCES only**. The directive's *"and all
required vertices/descriptors/metadata"* names the obligation and **rules no
figure for it** -- REFPUSH found this and it is the real argument for the plan
coming from the HPS at all. So the vertex and triangle columns above are
honestly blank, and the plan is where all four units are stated together.

---

## 5. The ABI change, and what proves the field is READ

`spec/commands.zidl`: **`command SealFramePlan 0x0003 implemented`**, 48 bytes
(16-byte header + 32 payload), `abi version 3 -> 4`.

The version bump is **mandatory, not chosen**: a new opcode is a wire change
under the frozen rule, and a v3 decoder reports `ZH_ABI_UNKNOWN_OPCODE` on
0x0003. Same reasoning the v3 bump recorded for `SetEnvironment 0x0311`.

`npm run abi:check` -> **clean, 38 outputs match** (was 37; the new golden is
the 38th).

**And the checker is not the evidence.** It is a DRIFT gate and goes green on a
field nothing reads. What proves the field is read:

* `zhao_cmd_exec` decodes every field by its **generated offset**, with a
  `$fatal` elaboration guard on `ZHAO_SEAL_FRAME_PLAN_BYTES != 48`;
* `zhao_measure_sealplan` **differences every field against a capacity or a
  reservation** -- 99 directed checks in which a changed field changes the
  outcome;
* `zhao_geom_paramarena` **latches** `seal_giant_refs_i` / `seal_giant_chunks_i`
  and presents them, and the console smoke **asserts** on what it latched;
* and the console smoke asserts `plans_malformed_o == 0` and
  `plans_forwarded_o == 0` -- a record arriving from nowhere would be caught.

### A second uncashed cheque cashed

`semantic_weight` has ridden the draw dispatch since the ABI shipped. Entry I56
recorded that nothing read the eight bits; REFPUSH refined that to *"true of
POLICY and false of WIRING"*. **The selector is its first policy consumer**:
a streaming max over `{semantic_weight, instance_id}`, highest weight wins,
lowest instance id breaks the tie. One comparator. **Not a heap** -- charter
section 9 forbids one and a running max cannot answer *"the second largest"*, so
it is not a step toward one. So the demotion order needed **no new ABI field**,
exactly as the entry said.

---

## 6. Claims in the brief or the entry that I found FALSE

**1. "The binner now holds 32,768 references -- R7's number" is true and its
consequence had not been drawn.** `MAX_REFS == GIANT_REFS`. **The reserve is the
ENTIRE capacity.** So a plan declaring the guaranteed giant must declare **zero**
ordinary tile references, and any positive ordinary reference demand is refused.
That is the ruled number enforced honestly and it is also a statement about the
machine: **at today's binner capacity a giant frame admits no ordinary
geometry.** GIANTREFS sized the capacity to the guarantee *exactly*, which
leaves nothing for anything beside it.

The answer is **more reference capacity, which is a fit**. It is **not** trimming
the giant, which the delegation does not cover. In CHUNKS there is real room --
2,341 reserved of 16,384, leaving 14,043 -- which is why the chunk arm has the
non-degenerate pressure case and the reference arm does not. Recorded in the
block header, in `zhao_console_core.sv` beside the constant, and as **case 6**,
which asserts `MAX_REFS == GIANT_REFS` so the finding cannot rot.

**2. The brief's "make it a build target, not a test" for a `$fatal` guard is
wrong, and I proved it wrong by doing it.** See section 8 -- it is my own error
as much as the brief's, because I acted on it without checking.

**3. `test_golden_abi_info` was ALREADY RED at the branch point, MEASURED at
`f4b4a653` and not inferred from dates.** That test requires every committed
`.zcap` to carry the CURRENT `ZHAO_ZIDL_SHA256`. At the branch point, before any
commit of mine:

```
generated zidl sha256 : c91773e0c7c0e4b28ca6bbaacbe5943f...
capture  zidl sha256  : 54abc34d6b67f51675a93b8c79976bd9...   <- differs
capture  abi_version  : 3   (matched, at that commit)
```

The four `captures/golden/wave2/*.zcap` last moved **2026-09-21** (`a6ff3f7d`)
while `spec/commands.zidl` last moved **2026-09-25** (`eca5b8d3`, FRAGSTATE).
So the test was already failing on the DIGEST check. My bump adds an
`abi_version` mismatch on top of a red, and does not cause it. **Not repaired here:
regenerating a wave-2 capture is a 22-minute shell golden run and a decision
about golden provenance that belongs to whoever owns those captures, not inside
an I56 packet.** Named so it is not attributed to the ABI bump.

**4. The two `zhao_console_core_*_mutant` wrappers were ALREADY behind.**
`wrapper_port_parity.py` reported `wrapper=1568 real=1581 missing=13` -- the 13
are mine -- but regenerating the block moved **~480 further lines** of port-block
commentary that earlier packets had changed in production and never carried
across. The port *count* was only 13 stale; the port *block* was much further
behind. Regenerated by the wrappers' own documented recipe; parity is now exact
for all three wrappers.

---

## 7. What I refused

**A per-chunk class bit in the arena.** GIANTQUOTA measured that a chunk has no
owning instance at that seam and the measurement holds; I did not overturn it
and I did not fabricate one. What I did instead is state precisely what the
reservation DOES guarantee (section 3) rather than leaving the reader to infer
a stronger claim -- which is the failure mode CLAUDE.md's refusal chapter names:
*"a packet rightly refusing to lay a wire does not thereby establish that the
missing law is open, and that second, larger claim is the one that gets
inherited."*

**Trimming, rescoping or redefining the guaranteed giant.** 32,768 references.
The block **refuses any plan that declares a different reservation**, so the
fence is enforced in hardware rather than written down.

**Raising the binner's reference capacity.** It is the obvious answer to finding
1, it moves M10K on a device already over on ALM, and it needs a fit that this
packet is not authorised to start.

**Regenerating the wave-2 golden captures.** See finding 3.

**Any Quartus run.** No console fit, no full-device fit, no `-MapOnly`. **I
quote no synthesis row**, so no `rtlCleanAtHead` claim is made anywhere in this
packet. `zhao_measure_sealplan` is registered as a leaf fit target in
`design/fit_targets.yml` so the next fit window can price it without a console
run; **its ALM cost is unmeasured and is declared as unmeasured.** Two 16-bit
counters, one comparator and twelve refusal terms -- no multiplier, no memory --
is a structural expectation, not a measurement.

---

## 8. What I got wrong and caught myself

**The elaboration guard, registered as a green that could not go red.** I
registered it as a BUILD target with `EXCLUDE_FROM_ALL`, reasoning that *"a
`$fatal` under ctest wedges the runner, so build it and never run it -- the
guard fires at elaboration, which is a compile step."* I then built it with
`GIANT_REFS=65536` and **the build returned RC=0**. Verilator compiles an
`initial` block into the model; `$fatal` fires when the BINARY RUNS. I had
registered a target that could never fail, as evidence that a guard works.
That is the broken-instrument shape with no red available to it, and it would
have been quoted as a fired guard.

CLAUDE.md's *"`--lint-only` does not run `initial` blocks"* is about LINT, and
the correct inference is that the guard needs the model to RUN.
`measure_starve_mask_guard` was already doing it right **in the same file**,
through `run_expect_fatal.cmake`, which exists precisely because Verilator's
abort path does not return under ctest's pipes on Windows -- the real form of
the hazard I was routing around. Now registered that way, with the wrapper's own
negative control (`WILL_FAIL` against the healthy build), and the wrong
reasoning is left in the CMakeLists beside the fix because the next person will
reach for the same shortcut.

**A test harness that could not re-arm the thing it tested.** My `frame_begin()`
helper set the level high, ticked, then set it low **without ticking**, so the
edge detector's `fb_q` never cleared and every frame after the first found no
rising edge. Two checks went red immediately, which is the good outcome -- but
an edge detector is only as good as a harness that lets the signal fall.

**A confident impossibility I talked myself into and out of.** I spent a long
time trying to make the reservation a runtime partition at the arena, going
round in circles because the giant's chunks and the ordinary ones are not
separable at that seam. The thing that ended it was reading
`zhao_geom_paramarena`'s own header, which had said all along that the
reservation *"is a decision made by whoever computes `seal_chunks_i`, not
here"*. **The block I was modifying had written down where the decision lives
and I had read past it twice.**

---

## 9. A live defect found by composing, not by reading

**One frame was re-sealing the arena 2,531 times.**

`render_frame_begin_i` is **not a pulse**. It is `zhao_renderer_lease_v2`'s
`frame_req_valid_i`, a ready/valid request, and `tb_zhao_console_core_smoke.sv`
**holds it** until the lease admits a frame -- with a long comment of its own
explaining why a one-cycle pulse was wrong there. Measured: **2,531 cycles**.

The previous composition drove `seal_valid_i` from that level directly. And
`zhao_geom_paramarena`'s `seal_fire_c` **flips the view** and zeroes
`n_verts_q`, `n_tris_q` and `n_chunks_q`. So one frame produced 2,531 view flips
and 2,531 frame restarts.

    before   SMOKE: sealplan ... default=2531
    after    SMOKE: sealplan ... default=1

**It was invisible for a reason worth writing down.** That bench releases its
draws one line AFTER the level drops, so nothing had been allocated to lose --
the arena reports `verts=30 tris=14 chunks=10 frames=1` both before and after,
identically. A console that let geometry flow while the lease was still being
granted would have lost it **silently** and reported a clean, short frame.
Nothing in the tree could have said so: the cursors reset to zero, which is
where they start.

I did not find it by reading. I found it because composing the seal behind a
**counter** made the console say how many times it had sealed, and 2,531 is not
1. That is *"counters see what pictures cannot"*, one block over.

The repair is both halves: the request is **raised on the rising edge and held
until it fires** -- exactly one seal per frame, and the retry survives, because
edge-only would lose the seal whenever the arena is not ready on that cycle.
`measure_sealplan_directed` case 22 is the committed test and is written as a
repair rather than as a property that was always true.

---

## 10. Counters, and the rule that none is asserted zero unseen

Entry I56 records this campaign's own shape three times: six binner counters
terminating in shell wires named `_unused`, three `geom_tidq_*` counters
declared in the smoke and asserted on by nothing, and `ck_fits_c` never seen to
fire anywhere until GIANTQUOTA fired it.

So every counter this packet adds leaves the core **and is asserted on**:

| counter | how it was seen to move |
|---|---|
| `plans_staged_o` | case 1, by stimulus |
| `plans_sealed_o` | cases 1, 20, 22 |
| `plans_refused_o` | TWELVE refusal cases, one per reason (2a-2d, 5, 6, 8, 10b-10d, 18, 19) |
| `refuse_reason_o` | all twelve reasons named by their own case |
| `default_seals_o` | cases 7, 21; and the composed smoke |
| `seal_lost_o` | case 20, arena not ready |
| `giant_mismatch_o` | cases 14 and 15 FIRE; case 16 is the negative control |
| `draws_seen_o` | cases 11-13; asserted nonzero in the composed smoke |
| `plans_forwarded_o` / `plans_malformed_o` | asserted in the composed smoke |
| `giant_reserve_breach_o` | **committed mutant**, unreachable otherwise |

The smoke's assertion on `geom_pa_reserve_breach_o` says in its own comment that
its zero is **a claim about that run, not evidence** -- the mutant is what makes
it evidence.

---

## 11. Gates at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` | **4** (from 5; I56 closed) |
| `check_console_inventory.py` | OK |
| `check_prod_manifest.py` | OK |
| `gen_prod_top.py --check` | fresh, 86 instances |
| `gen_console_board.py --check` | FRESH, 1,586 core ports |
| `check_console_closure_lint.py` (gate 31) | OK, self-test 5/5 |
| `check_quartus17_syntax.py` | RC 0, no rejected forms |
| `gen_shell_paired_diff.py --check` | fresh, harness and mutant |
| `check_case_labels.py` | OK |
| `mutant_copy_drift.py` (run AFTER commit) | OK, 77 copies |
| `wrapper_port_parity.py` | exact, all three wrappers |
| `npm run abi:check` | clean, 38 outputs |
| console smoke | PASS, `raster pixels=2560`, `frames_admitted=1` |
| `measure_sealplan_directed` | **99 checks** |
| `geom_paramarena_reservemut_{fires,silent}` | 0 failures each |
| `measure_sealplan_giant_guard` + wrapper control | Passed |

**No Quartus was run and no synthesis row is quoted.**
