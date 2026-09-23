# FINDINGS — packet EDGERECON2 (`gz/edgerecon2`), 2026-09-23

**Register: 12 before, 12 after. PIXELS: unchanged.** Nothing was closed and
nothing was opened. This is a **REFUSAL with a proof and a scoped plan**, which
the packet brief names as an acceptable and valuable outcome.

Baseline and final both measured in this worktree:

```
python tools/budget/completion_register.py   ->  12   (8 tie-offs + 4 disconnected)
python tools/maintenance/gate_sweep.py       ->  RC 0, "every gate matches the
                                                 committed baseline (29 gate(s))"
```

---

## 1. THE FIVE STALE PREMISES — ALL FIVE CONFIRMED STALE, AS BRIEFED

| # | the premise | the measurement that killed it |
|---|---|---|
| 1 | I21 (a): MEASURE.GOVERNOR uncomposed, `px_err0/1_i` and `view_count_i` have no producer | composed at `zhao_console_core.sv:25813`, all three driven — `cx_gov_view_count_c`, `cx_gov_px_err0_c`, `cx_gov_px_err1_c`. **Item (a) is spent.** |
| 2 | I21 (c): `zhao_terrain_devstore`'s 185 M10K of 553 | ruling **R242**, `zhao_terrain_devstore.sv:20-53` — *"Neither array exists any more; this block now infers NO MEMORY AT ALL … 320 KiB of local SDRAM."* **Item (c) is spent.** |
| 3 | the TERRAIN.LOD group is not composed | all composed: `lod:26074`, `devstore:25896`, `spdesc:25987`, `jobissue:26197`, `lodfeed:25231`, `compcache_front:22435`. Only `edgerecon` and `visible` are out. |
| 4 | walker step 2's `sp_*` assembler does not exist | `zhao_terrain_spdesc` is that assembler, built 2026-09-21 and composed. |
| 5 | GEOM.CLIP has one triangle producer, no merge | `zhao_geom_clipdoor` composed at `:14167` with `.NCLIENT (3)`. |

### And the broken-grep trap arrived from a THIRD direction — my own

The brief warns that `^\s*<module>\s+u_` misses parameterised instantiations and
that **an absence reads as a build**. I used the brief's corrected pattern,
`^\s*<module>\s*(#\(|[A-Za-z_])`, and it reported `zhao_terrain_lod` composed at
`:25231`. It is not — that line is **`zhao_terrain_lodfeed`**, and the trailing
`[A-Za-z_]` alternative happily matched the `f` of `feed`. A **presence** was
manufactured this time rather than an absence, and it pointed at the wrong line
of the wrong block. The pattern needs a word boundary:

```
^\s*<module>\b\s*(#\(|[A-Za-z_])
```

Three packets have now been bitten by this family in two days, each from a
different side. The lesson is not the regex; it is that a composition grep must
be **verified against one instance by hand** before its totals are believed.

---

## 2. THE DECISION THE CONTRACT DELIBERATELY LEFT OPEN IS NOW MADE — AND IT IS NONE OF ITS THREE CANDIDATES

`TERRAIN.EDGERECON.md` leaves `sp_cx`/`sp_cz` to *"whoever builds the walker"*
and lists three candidates, recording the obstacle as *"getting its PLACED world
x/z"*. Measured in `zhao_terrain_place.sv`:

```
:41    wx(i) = (patch_ix * 32 + i) <<< (16 + pitch_log2)
:318   vtx_wx_o = place32(units_of(ix_q, vtx_vi_i), pitch_q);
:282   env_ok_c = (org_x_c == hdr_env_x0_i) && (org_z_c == hdr_env_z0_i);
```

**`hdr_env_x0_i`/`hdr_env_z0_i` are a CHECK, not an operand.** No page payload
reaches placement. It is a pure function of (patch coordinate, vertex index,
`pitch_log2`), so **a patch's world x/z is knowable without composing it** —
which makes candidate (1) "a second TERRAIN.PLACE instance" and candidate (2) "a
cx/cz column on devstore" both pay a store to carry a number that is two shifts
away.

**The actual missing datum is `pitch_log2` for a resident patch — two bits.**
`zhao_terrain_hdrread.sv:31-35` names every reader of `pitch_log2` in the tree
and records that nothing produces it but the page's own header (spec 2.1 +2). It
is retained nowhere per slot.

**Candidate (3) is dead for a reason the contract does not give:** `hdr_ready_o`
is tied high unconditionally (`:287`) and a header acceptance **displaces** the
latched patch (`:361-375`) *and* launches the 66-write `pos_we_o` fill into the
compose cache. You cannot ask PLACE about another patch without destroying the
patch it is placing.

The honest shape is a **stateless query port on PLACE** reusing `place32`/
`units_of`, so PREPARE and EMIT are bit-identical *by construction* rather than
by two implementations agreeing — which is the property the symmetry law needs
and the one a second implementation cannot promise.

---

## 3. THREE BLOCKERS THE CONTRACT DOES NOT CARRY

### A. A PREPARE PASS TEED OFF THE ISSUE PORT CANNOT TERMINATE

The contract's own correction says the enumerator *"EXISTS AND IS COMPOSED"* —
`u_terrain_seq`'s issue port. True, and unusable that way.

* PREPARE must hold the **whole** admitted set before EMIT opens.
* The issue stream is **one-shot**: `is_valid_o == (st == S_ISSUE)`, the record
  overwritten on the next fetch, frame state wiped on `fr_start_i`, and
  architecture 2.5 **rejects by name** the persistent cache a replay would need.
* So a walker must drink the stream as it flies — and **that stream's ready is
  the compose cache's**: `tis_ready = thr_j_ready && tce_can_start`,
  `tce_can_start = !tcc_fill_busy` (`:21699`, `:21704`), against a front holding
  **exactly two** lattices (`compcache_front:284-291`, asserted `:746-749`).

Holding tessellation back until the last patch is issued stalls the compose,
drops `tce_can_start`, stalls the issue port, and **the walk never reaches the
last patch**. The walker must be a **second, independent reader of the sealed
list** that never touches the compose spine — a different and larger block than
"drive the ladder from the issue port".

### B. STEP 2'S COST IS BANDWIDTH, NOT AREA, AND NOBODY HAS COSTED IT

Ruling **R242 moved `zhao_terrain_devstore` to SDRAM on 2026-09-22 — the same day
`TERRAIN.EDGERECON.md` was written** — and the contract's step 2 still describes
the M10K store it inherited. A read now costs **two full 64-byte bursts before
the first descriptor and another every four** (`:880-883`), instrumented on
`rd_wait_clocks_o`. A PREPARE pass over 256 admitted patches is **~1,280 extra
64-byte SDRAM reads per frame, on top of the identical ~1,280 EMIT already
spends.**

That — not the ~3 M10K bank the contract sizes by hand — is the number that
decides whether this walker is affordable, and it has never been measured.

R242 also made devstore's `w_ready_o` **fall** during a burst;
`zhao_console_core.sv:17273-17275` records that same change silently making a
histogram count one record **86 times**. A walker joining against that ready
inherits the hazard.

### C. THE DETERMINISM PREMISE IS NOT SATISFIED BY THE CURRENT COMPOSITION

Step 3 rests on *"identical `sp_*` and identical governor targets give identical
`lvl[]`"*. The console captures the held governor targets `gv_*_q` under an
`else if (tld_idle)` arm (`~:25577`), and `tld_idle` is TERRAIN.LOD's own
`idle_o` — **high between every patch**. They are re-sampled ~256 times a frame.

MEASURE.GOVERNOR itself decides on `frame_i`, a frame-boundary pulse, so `mgv_*`
is stable within a frame and the re-latch is harmless **today**. But
`veye0_*`/`veye1_*` come from the view block (`~:25721`), and a PREPARE pass
would sample them at a different point in the frame from EMIT. **Any motion
between the two passes banks a level the patch does not tessellate at — a crack,
with every counter balancing**, which is this repository's most expensive
recurring shape. The walker owes a **frame-scoped freeze** of `gv_*_q`, and that
is a behaviour change to composed, working serve-path code.

---

## 4. ONE BLOCKER THAT DIES ON MEASUREMENT — recorded so it is not raised again

The ledger sets TERRAIN.LOD at *"1 decision per patch per frame"*, and
`zhao_terrain_lod.sv:52-56` justifies choosing the 32-step §7.2 isqrt over a
squared-domain multiply **by that rate**. A prepare+emit pair doubles it, which
reads at first like a breach of a recorded design decision.

**It is not.** `:157-159` records ~784 clocks a patch and states that 256 live
patches is *"still about 8x the required rate"*. Two passes are **401,408 clocks
of a 1.67 M-clock frame — ~24%, with ~4x margin remaining.**

What step 3 *does* owe is the time-share itself: TERRAIN.LOD has **no** mode,
bypass or phase input (its only behavioural switches are `cam*_en_i` and
`dual_i`), so PREPARE-vs-EMIT selection across twelve `sp_*` inputs and fourteen
`out_*` outputs must be a **named block**, never composer wires — the
hidden-adapter failure entry I21 already refused once.

---

## 5. WHAT I DID NOT DO, AND WHY

* **Did not compose `zhao_terrain_edgerecon`.** Its own manifest row gives the
  reason and the row is right: it would dangle `f_*` and the two phase pulses at
  the core boundary, move the register **up** by one, and change nothing a frame
  can see. **Do not buy a gap with a tie-off** (R75).
* **Did not build any part of the walker.** With blocker A unresolved, a walker
  driven from the issue port is a **deadlock** shipped into a shared console core.
* **Did not build P1 (the PLACE query port) on its own.** A port nothing reads is
  CLAUDE.md's *"BUILT, INSTALLED NOWHERE"* — an authored uncashed cheque inside
  the commit meant to remove one.
* **Did not touch the retained `8'h00`.** The owner's instruction to retain it
  stands; it is the least bad constant, not a safe one, and that distinction is
  the argument for the build rather than against it.
* **Did not run Quartus.** No fit is owed until P4, whose question is named below.
* **Added no counter**, so none is owed a demonstration that it can fire.

---

## 6. THE SCOPED PLAN — FOUR PACKETS, ONE FIT

| | work | fit |
|---|---|---|
| **P1** | per-slot `pitch_log2` retention from TERRAIN.HDRREAD + a **stateless query port** on TERRAIN.PLACE. Port change on a composed block: regenerate `gen_prod_top`, `gen_console_board`, `gen_shell_paired_diff`, and connect **every bench** instantiating PLACE. Directed test asserts the new port equals the latched port for the same operands. | none |
| **P2** | the **sealed-list PREPARE reader** (blocker A's real shape), gated on B's bandwidth measurement being taken first. | none |
| **P3** | the LOD **time-share block** (`out_hold_o` suppressed in PREPARE) and the **frame-scoped governor freeze**. | none |
| **P4** | compose EDGERECON, wire `edge_*`, land an **acceptance bench** on `tests/prod/partmat_acceptance.cpp`'s pattern. | **one** |

**P4's fit question, named in advance:** *does the terrain island still close at
NCTX with the walker, the pitch table and EDGERECON added?*

**P4's evidence cannot be the console smoke.** It fails every terrain page's CRC,
so no page becomes resident, `zhao_terrain_devstore` holds no record, and the
walker is quiescent — the owner ruling's own *"an otherwise green smoke whose
upstream fixture never reaches the new path does not prove the path."*

---

## 7. AN OWNER DECISION FOUND, WITH EVIDENCE AND A RECOMMENDATION

**Question.** May `zhao_terrain_island_dir`'s **frame-scoped**
`desc_pitch_log2_i` (`:70`) be ratified as authoritative for every page of its
island, rather than each page carrying its own `pitch_log2` in its header
(spec 2.1 +2)?

**Evidence.** `zhao_terrain_hdrread.sv:31-35` searched the whole tree and records
these as **two different fields** with two different readers —
`zhao_terrain_place` takes the page header's, `zhao_terrain_island_dir` and
`zhao_terrain_visible` take the island descriptor's — and that **nothing produces
`pitch_log2`** but the page header. `zhao_terrain_place.sv:282`'s `env_ok_c`
already cross-checks each page's declared origin against the one computed from
its own pitch, so a disagreement between the two fields is *detectable* today but
not *ruled on*.

**Recommendation: ratify the island descriptor as authoritative.** It removes
P1's per-slot table entirely, it makes a patch's placement computable from the
sealed list alone, and the per-page field becomes a checked redundancy rather
than a second source of truth. **It is not mine to decide** — it is a spec 2.1
question, and deciding it inside a packet is exactly what rule 4 of the packet
protocol forbids.

---

## 8. WHERE THE NEXT LANE READS

Updated in this commit, beside the things they govern rather than in this run
folder:

* `fpga/rtl/prod/zhao_console_core.sv` — entry **I21 item (e)** rewritten with
  all four walker steps re-measured, the three blockers, the dead objection and
  the four-packet plan.
* `design/prod_manifest.yml` — the `zhao_terrain_edgerecon` row's remainder
  replaced with the same, in the block's own voice.
* `design/contracts/TERRAIN.EDGERECON.md` — a new section before **Notes**,
  correcting step 2's pre-R242 description of devstore and recording the
  `sp_cx`/`sp_cz` decision.
