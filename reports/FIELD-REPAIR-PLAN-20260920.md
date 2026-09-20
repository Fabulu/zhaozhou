# Implementation plan — SHARED FIELD PRODUCTION REPAIR

Against owner directive `reports/Zhaozhou_SHARED_FIELD_Repair_Architecture_2026-09-20.txt`
(owner commit `6262868c`, 3,108 lines), whose own first instruction is
*"THE ASSIGNMENT IS IMPLEMENTATION, NOT ANOTHER REFUSAL REPORT."*

Produced 2026-09-20 by a read-only architect pass at HEAD `5e558d19`. Nothing in
the tree was modified to produce it. **It lives in `reports/` and not in a run
folder deliberately** — every pass creates a new run folder, so anything durable
left in the current one is orphaned by the next.

---

## 0. Reconciliation — verified by ANCESTRY, not by reading its prose

| pin | the directive's claim | measured |
|---|---|---|
| `c55e0417` | analysis baseline | **is an ancestor of HEAD** |
| `a72d631d` | projadopt merge, "now merged" | **is an ancestor** |
| `f98e7f86` | `gz/fieldp4` R101 lane, "now merged" | **is an ancestor** (merged at `4f1c9aba`) |

So the directive's LATE LIVE-REPOSITORY RECONCILIATION is current with the tree.
The directive commit `6262868c` sits **between** `a72d631d` and the fieldp4
merge, which means its §7.2/§20.2 instruction *"adopt R101's repair, do not
recreate it"* **is already satisfied by the merge, not by a packet.**

Register measured at HEAD: **21** (9 boundary tie-offs + 11 disconnected + 1
unbuilt). RC 1 is normal while gaps remain.

---

## 1. Decision inventory — FH01…FH30

**SAT** = the tree already does this · **PART** = partly · **NO** = does not ·
**NEW** = a new choice with nothing to satisfy yet.

**None of these is recorded as pre-existing law.** The directive's own §20.1 says
they become authority *on adoption*, so they are dated to adoption and cite the
owner commit. Nothing is back-dated.

| # | Requirement | Status | Checked at |
|---|---|---|---|
| FH01 | One shared v3 fabric; no private Warp executor, no v1 resurrection | **SAT** | `zhao_console_core.sv:15642` composes one `zhao_field_host`; v1 `zhao_field_seq` instantiated nowhere; `check_console_inventory.py` G1 is the standing gate |
| FH02 | Versioned association-aware host; old host kept as a NAMED ORACLE | **NO** | `zhao_field_host_v2.sv` does not exist |
| FH03 | Three identities — program / association / context | **NO** | One `state`, one `cur_slot` (`zhao_field_host.sv:828–841`); client supplies a raw `req_slot_i` |
| FH04 | Productionize the Translator; one lowerer for packer and benchmark | **PART** | Exists at `field_v3_earth_directed.cpp:266`, test-only. **Better than the directive implies** — see F4 |
| FH05 | Nonempty required-output mask, output count, output-source map | **PART** | Mask exists (`:301, 585, 1074`); **count and source map do not** |
| FH06 | Uniform outputs are results; a plan may return a prepared scalar | **NO** | `out_none_c = (cur_out_seen == '0')` at `:862` forces `ST_NO_RESULT` |
| FH07 | Three status families kept distinct | **PART** | Separate today mostly by accident, not by a generated enum |
| FH08 | Skip per-point clear only under a checked init contract | **NO** | `E_ZERO` unconditional at `:1116` — *"once per point. It is not optional"* |
| FH09 | One active prepared-data domain, many running points | **NO** | `zhao_field_v3_sbank.sv:74` is one flat `SLOTS=64` array, no program namespace |
| FH10 | Association switches are counted work | **NO** | No association object exists to switch |
| FH11 | Useful lanes, exact per-point status, live mask | **NO** | Composed `.FAB_LANES(1)`, `.FAB_GROUP_PTS(1)`; `zhao_field_alu_vec.sv` ORs per-lane flags into group flags |
| FH12 | Legal-but-cold routes to canonical execution on the same fabric | **PART** | `zhao_field_ops_pkg.sv:68` states `OP_RING` (0x21) *"is still absent and stays absent"* |
| FH13 | reserve → fill → validate → seal → bind | **NO** | Current protocol is LOAD/COMMIT/LOOKUP with software-guessed slots |
| FH14 | op 3 explicitly decoded; unknown ops counted refusals | **NO — confirmed exactly as written** | `zhao_field_doorbell.sv:440`: the final `else` sends everything to `D_LOAD`; `post_op_i` is `[1:0]` so 3 is encodable |
| FH15 | Backing storage ≠ active caches; pinned HPS staging | **NO** | No HPS capsule path for FIELD |
| FH16 | No per-point directory lookup; pin at association open | **PART** | `zhao_field_progdir.sv:22` already states the POLICY; the binding object is missing |
| FH17 | All consumers speak one result contract | **NO** | Two adapters, different contracts (`zhao_field_stamp_adapter.sv:101` defaults 12/4, instantiated 13/7) |
| FH18 | Earth is field-major, one accumulator owner | **PART — far nearer than the directive knows.** See F1 | `zhao_probe_walk_earth.sv`, `zhao_probe_patch_acc.sv` |
| FH19 | Warp semantics stay W01–W18 | **SAT** | `OWNER-RATIFICATION-20260920-WARP.md`; `zref_geom_warp.hpp:76,79` encode `kInLanes=15`/`kOutLanes=6` |
| FH20 | Credit before acceptance; reserve terminal result location | **NO** | One response at a time, no credit pool |
| FH21 | One generated hardware-capability table | **PART** | SV side single-sourced; **the C++ side is not generated from it** |
| FH22 | Warp 48 / global 64; REGS=64, ≥64-word code store | **NO — and expensive.** See C1 | Composed `.REGS(32)`, `.INSTR_N(32)` at `:15657,15667` |
| FH23 | Publish shapes, do not invent ALM limits | **NEW** | A reporting rule |
| FH24 | Four gates reported separately | **NEW** | Adopt as discipline |
| FH25 | No unowned shared prerequisites; one integration owner | **NEW** | Coordinator action |
| FH26 | Legacy is an explicit NAMED mode, never the strict default | **PART** | `mask == 0` legacy preserved (R111), but it is a VALUE, not a MODE |
| FH27 | Bounds and signatures checked above numeric execution | **NO** | Nothing binds a profile signature; `zref::geom_warp::check_signature` does it for Warp only, in the reference |
| FH28 | Capture everything that selects a result | **NO** | No FIELD capture bundle |
| FH29 | Fairness includes activation and maintenance | **NO** | `ld_valid` has unbounded precedence over client grant |
| FH30 | Implement the entire commissioned slice | **NEW** | Scope statement |

**2 SAT, 9 PART, 14 NO, 5 NEW.** The only two fully satisfied entries were
closed by *other* lanes this week.

---

## 2. THE TRAP, ANSWERED PRECISELY

The directive flags it. A plan has to specify it.

* **R101's `hdr_outreq` is indexed by CONTIGUOUS CAPTURE-WINDOW POSITION.**
  `zhao_field_host.sv:851` computes `out_idx_c = fab_wr_reg - hdr_outbase[cur_slot]`,
  valid when that difference is `< OUT_LANES`. Bit *k* means "physical register
  `out_base + k` was written". Width `OUT_LANES` (7 composed).
* **FH2's `required_mask` is indexed by CANONICAL OUTPUT ORDINAL.** Bit *j*
  means "canonical output *j* of the profile is declared". Width = profile
  output count (Earth 4, Warp 6, Flow 7, Stamp 3). Lives in
  `PROGRAM_META.required_mask:u8`.

**They coincide only when output registers happen to be contiguous from
`out_base`, and R111 measured that they never are.**
`tools/field/zprog_output_coverage.py` reports `0x17 / 0x1D / 0x17` for the three
shipped Earth programs — every one leaves three of seven window lanes unwritten.
Worked example: `crater_ring` writes R13,R14,R15,R17 with `out_base=13` →
**window mask `0x17`, ordinal mask `0x0F`.**

**The translation lives in `OUTPUT_MAP`** — one 8-byte row per *ordinal*,
carrying `source_kind ∈ {VECTOR_REG, PREPARED_SCALAR}` and `source_index`:

```
window_mask[k]  ⟸ set iff ∃ ordinal j with source_kind[j]==VECTOR_REG
                            and source_index[j] == out_base + k
export_value[j] ⟸ the granted write to source_index[j]        (VECTOR_REG)
                  the sealed prepared scalar at source_index[j] (PREPARED_SCALAR)
seen[j]         ⟸ per-ORDINAL, set by the write targeting source_index[j],
                  or seeded at point start for PREPARED_SCALAR
complete        ⟸ (seen & required_mask) == required_mask  AND fence
```

**The rule that goes in the schema and is GATED: `required_mask` and
`window_mask` are different named fields with different widths. Never assign one
to the other.** The generated schema gives them distinct type names so a wire-up
mistake is a **compile error, not a wrong value.**

**An ordinal with no window position — three cases, each decided explicitly
rather than falling through:**

1. `PREPARED_SCALAR` — legitimate, has no window position by construction,
   seeded at point start under FH06.
2. `VECTOR_REG` whose `source_index` is **outside** `[out_base, out_base+OUT_LANES)`
   — **a load-time refusal, `BAD_IMAGE`.** This is the case that silently
   produces a wrong value today: the window cannot observe the write, so the
   ordinal can never be seen and the point hangs or refuses for the wrong
   reason. Caught by the C++ validator **and** the hardware install check, and
   **seen to fire.**
3. Two ordinals aliasing one `source_index` — legal; both `seen[j]` set from the
   same granted write.

**And R111's standing hazard:** a plan writer emitting `mask == 0` silently
restores the defect and passes every gate. So **`required_mask == 0` is a
refusal at bind**, and the legacy permissive meaning is reachable only through an
explicitly named compatibility binding — never by omission. That check owns a
positive control.

---

## 3. Packets

Three run concurrently, each in its own worktree. File sets are disjoint
**within a wave**. Shared files (`zhao_console_core.sv`, `tests/CMakeLists.txt`,
`design/*.yml`, `zhao_prod_top.sv`) are staged by hunk from a private index,
never `git add <file>`.

### WAVE 1

**S1 — SCHEMA, the one generated source of truth.**
Owns: `spec/form/field-host-image.md` (new), `tools/field/gen_field_host_schema.py` (new),
`reference/include/zfield/generated/zfield_host_image.hpp`,
`fpga/rtl/field/generated/zhao_field_host_image_pkg.sv`,
and **the Formation parenthetical only** in `spec/form/field-ir.md` (see F2).
Emits the 64-byte header, section directory, `PROGRAM_META`, `INPUT_MAP`,
`OUTPUT_MAP`, `INIT_PROOF`, `ASSOCIATION_META`, `PRELOAD` from ONE schema into
both C++ and SV. Gives the two masks **distinct generated type names**.
Cross-checks the C++ opcode-shape table against `zhao_field_ops_pkg.sv`
mechanically — FH21's missing half.

*Evidence:* `field_host_schema_roundtrip`, byte-stable across two clean runs.
**Negative control seen to fire:** a deliberately mismatched C++/SV
`PROGRAM_META` offset makes the cross-check FAIL; restore, it passes.
**Negative control:** assigning `required_mask` to a `window_mask`-typed field is
a **compile error**, committed under `tests/mutants/` as a compile-fail case
with a driver asserting non-zero rc.

**L1 — LOWERER, C++ only, no RTL.**
Owns: `reference/include/zfield/zfield_host_plan.hpp`, `reference/src/zfield/zfield_host_plan.cpp`,
`tools/field/pack_field_host.cpp`, `tests/differential/field_host_plan_directed.cpp`
(**all four verified absent**).
Lifts `Translator`, `contract_smoothstep`, `match_smoothstep` and the
uniform-broadcast discipline out of the test into the library. Completes the
opcode shapes. Emits `INIT_PROOF` masks by the symbolic defined-set walk, and
`OUTPUT_MAP` with the §2 translation.
**Does not touch `field_v3_earth_directed.cpp`** — that is E1's, to avoid
colliding with the 273→297 correction.

*Evidence:* FT005–FT010, each shape with a deliberate miswire control. FT034 —
an `INIT_PROOF` whose read is absent from both preload and point sets is
**rejected**, refusal string checked, with the preload-added case as the positive
control. FT015 byte-identical capsules, hashes quoted. **FT030: a descriptor
with `required_mask == 0` and a nonempty declared output list is refused by the
packer — R111's gate, shown failing.** `-Wall`, plus the sanitizer run.

**F1 — FABRIC: status, and the two missing canonical routes.**
Owns: `zhao_field_alu_vec.sv`, `zhao_field_ops_pkg.sv`, `zhao_field_v3_ring_svc.sv`,
`zhao_field_v3_ring.sv`, `zhao_field_rcp.sv`, `zhao_field_v3_svcpath.sv`,
`zhao_field_v3_dispatch.sv`, and three directed tests.
Carries per-lane `l_sadd`/`l_smul`/`l_srescale` out with lane/context
attribution instead of OR-ing into a group flag. Adds canonical RCP using the
**existing exact Field reciprocal leaf — explicitly NOT the raster/projector
reciprocal, whose width merely looks similar.** Adds a bounded varying-radius
RING through shared services.

*Evidence:* FT039 one lane saturates, three do not. **Mutant required** —
`zhao_field_alu_vec_flag_broadcast_mutant.sv` re-ORs flags into every lane and
must go red; negative control compiles it with the selector undefined and
confirms the output differs. FT012 RCP **through the shared service route, not
the leaf.** FT013 varying-radius RING. FT014 every advertised opcode has a live
request→service→result path — a structural census with a driver, not a table
read. FT040 `rcp0` in one lane does not label neighbours.

### WAVE 2 — requires S1 + L1 + F1 merged

**D1 — DOORBELL AND LOADER** (FH13, FH14, the FH2 transaction).
Owns the doorbell, a new `zhao_field_loader.sv`, progdir/progcache pin and
publication semantics, and **a refresh of `zhao_field_doorbell_mutant.sv`, which
is a COPY and goes stale the moment production moves.**
*Evidence:* FT060 with `zhao_field_doorbell_op3_alias_mutant.sv` restoring the
catch-all `else`. FT049–FT057 each with its invalid control, FT057 forcing an
eviction different from the software hint. **FT061: two queued posts with
different plan/epoch metadata while `cfg_plan_base_i` moves — the live-pin defect
at `:425,435`, with the old behaviour shown failing.**

**H1 — THE HOST** (FH02, FH03, FH05, FH06, FH08, FH09, FH20).
Owns `zhao_field_host_v2.sv` (new), sbank, exec, rf, its directed test and the
contract. **Retains `zhao_field_host.sv` untouched as the named oracle — do not
delete it.**
*Evidence:* FT017–FT032. **FT018 and FT019 driven independently — missing-value
vs zero-value distinguished by separate validity, not by the data.** FT024
(write coincident with END) and FT025 (delayed final overwrite) are the two the
mask alone cannot catch; they need the fence. FT033 slow-clear vs no-clear from
**independently randomized** initial RF contents with poisoned scratch.
*Mutants:* required-mask tested as ANY not ALL (the R101 defect re-planted
against the new host); final overwrite dropped after `seen` set; uniform output
seeded from the previous preparation. **fieldp4's case 1d — one program, mask
`0b0011` → OK, `0b0111` → refused — is carried forward as the permanent
both-polarity control.**

**A1 — ADAPTERS** (FH17, FH26).
*Evidence:* FT097 preserves R40's `sat_s11((v'-v) >> 8)`, `rec_take` lifetime
and the no-second-integrator rule through queued execution. FT098/FT099
`LEGACY_STAMP_BRUSH` and `CANONICAL_STAMP` as **two named bindings tested
separately**, the `(2*i+1)*512` texel-center and
`floor((u*65535 + 32768)/65536)` at both endpoints, the half case and out of
range. **Negative control: the legacy bridge passing does NOT mark the canonical
binding tested**, and the receipt names which case discriminated.

### WAVE 3

**E1 — EARTH** (FH18, closes I34). Promotes the two probes to production names
and homes, adds `zhao_terrain_patch_v2.sv`, corrects `field_v3_earth_directed.cpp`
**273 → 297**.
**W1 — WARP** (closes GEOM.WARP). `zhao_geom_warp.sv`, `DrawWarpedForm` at
**0x0304 — verified free**, appended at the END of `commands.zidl`.
**C1 — COMPOSITION.** Coordinator-owned, serialised, never concurrent: core,
prod_top and board (regenerated), the three yml files, the smoke bench,
`tests/CMakeLists.txt`.

---

## 4. Dependency order, and what `zhao_geom_warp` actually needs

```
S1 ─┬─► D1 ─┐
L1 ─┤       ├─► H1 ─┬─► A1 ─┬─► E1 ─┐
F1 ─┘       │       │       │       ├─► C1
            └───────┘       └─► W1 ─┘
```

**R103's nine prerequisites, mapped:**

| | requirement | supplied by | covered? |
|---|---|---|---|
| P1 | 15 input lanes — `.IN_LANES(13)` at **three** sites plus generated `prod_top` | C1, one atomic act | YES |
| P2 | a free client port — `CLIENTS(2)`, both taken | A1 + C1 (`2→3`) | YES |
| P3 | sparse output map | S1 + H1 | YES — this is §2's work |
| P4 | all-outputs completion | **already merged** (R101) + H1 strict mode | YES, partly done |
| P5 | per-context prepared uniforms | H1 (FH09) | **PARTLY, BY A WEAKER MEANS** |
| P6 | four independent tables — host exposes `TABLES(2)` | H1 + C1 | YES |
| P7 | 48-instruction capacity — `INSTR_N(32)` | C1 | YES |
| P8 | handle → resident slot binding | D1 | YES |
| P9 | per-point cost / the R91 lever | H1 + E1 | YES — and it discharges R91's caveat |

**P5 is the honest exception.** FH09 gives one *active* domain with exclusive
ownership, not per-context uniforms. That satisfies Warp only if Warp never
interleaves with Earth inside a frame. **Report P5 as DEFERRED WITH A MEASURED
JUSTIFICATION, never as closed.**

**W1's hard rule:** `zhao_geom_warp.sv` must not be built with its Field port
tied off. The warp lane already refused exactly that — it would convert an
honestly-absent entry into a tie-off. **W1 lands after A1 and C1, or it does not
land.**

---

## 5. Contradictions — flagged for the owner, NOT resolved

**C1 — FH22's `REGS=64` collides with a deliberate, argued, measured choice, and
the directive has not noticed.** FH22 asks for `REGS=64`; `zhao_console_core.sv:15662–15667`
composes `.REGS(32)` because *"REGS is the length of this front's E_ZERO … REGS=64
would double that from 32 clocks to 64 on the critical path."* FH08 removes
`E_ZERO`, dissolving the reason — **but only after H1 lands.**
*Recommendation:* sequence REGS 32→64 **after** the no-clear proof, never
before, or per-point cost doubles on a path already at 481% of allowance (R91).
**The intermediate state is worse than either endpoint**, so the sequencing needs
explicit ratification.

**C2 — FH11's useful lanes reverse the gz/pfs savings and the area is not free.**
The console composes `.FAB_LANES(1)` with a written argument, and prices the
reversal at *"about +6,000 ALM on this axis alone"* and *"roughly +12 DSP"*. The
directive's §17.3 concedes a faster correct machine can cost more. **The part the
directive does not have: the console is already 5,672 ALM and 39 DSP over, and
`zhao_block_fit.json`'s `zhao_console_core` row DOES NOT CONTAIN FIELD** — its
`.sources.sha256` lists exactly one field file.
*Recommendation:* adopt FH11's **semantics** (exact per-point status, no padding
contamination) in H1; take the **width** to the owner with a fit, not a plan.

**C3 — FH18 vs `zhao_terrain_patch.sv` Law 1 is NOT a conflict.** The block at
`:67–72` already records the escape hatch: *"if a later increment puts that cache
inside this block the intake can be turned around without changing the
arithmetic."* FH18 is exercising it, not overruling it.
*Recommendation:* amend the **citation**, not the law.

**C4 — FH18's "the old serial implementation is not in the shipping datapath" is
a REMOVAL, and the rules forbid closing a gap that way.** `zhao_terrain_patch`
carries the ratified footprint test, the dirty-mask law and `programs_rejected_o`.
*Recommendation:* `zhao_terrain_patch_v2` **retains those four laws by
factoring, not reimplementation** — `uncashed_cheques.py` check 3 exists exactly
to catch a second implementation of ratified arithmetic — and the old block stays
as the paired numeric oracle with a committed paired-diff test. Only the
*datapath selection* changes.

**C5 — `console_inventory.yml` contradicts R44, invisibly.** R44 says promote the
two probes rather than rebuild them; `console_inventory.yml:241,253` marks both
`disposition: instrument` — *"measures, never ships"*. **`instrument` is a
SETTLED disposition, so `uncashed_cheques.py` will never flag them.** The
inventory is suppressing the very cheque R44 wrote.
*Recommendation:* E1 changes both to `pending_compose` **on its first commit,
before it builds anything**, so the tool starts watching immediately.

**C6 — amendments the directive correctly declares** (FH18 amending R91's means
and the vertex-major port arrangement; FH12/FH26 amending the transport's legacy
meaning; §10.2's LDADDRW 7→8). Honest, labelled — ratify, do not treat as
conflicts.

---

## 6. False absences and false presences

### Present, and the directive does not know it

**F1 — `zhao_probe_walk_earth.sv` EXISTS, is field-major, is differentially
tested, and already emits 297 groups.** The largest finding. The directive
commissions "the field-major scheduler" as new work and **never mentions this
file in 3,108 lines.** It already: generates lattice points from two prepared
33-entry tables, deleting the 27,225-clock/association v2 transport; emits **297
row-bounded groups, not 273** — the exact correction FT088 demands; applies
§9.1's closed-interval test per vertex with the covered box as a hint only,
which is §13.4's requirement already satisfied; and takes a prepared descriptor,
which is the directive's association model in all but name.
**R44 already ruled "promote, don't rebuild", and `zhao_console_core.sv:2732`
says so in capitals**: *"BEFORE BUILDING THE WALKER THAT FEEDS THIS LANE, READ
`fpga/rtl/synth/zhao_probe_walk_earth.sv` … the file is exactly the shape this
repository lost three weeks to once already."*
**E1's brief must open with that paragraph.**

**F2 — Warp's "(14)" is already corrected, but the row below it is still wrong
and is LIVE WORK.** `spec/form/field-ir.md:523` already reads `(15)`, corrected
under W01. **The same line's Formation entry is still `(11)` against twelve
listed fields** — `index(1) + time(1) + rot2(2) + trans2(2) + p0..p5(6) = 12`.
The identical defect that nearly cost P3, sitting one row below the fix.

**F3 — `0x0304` is still free.** Range: 0x0300 DrawForm, 0x0301 DrawPopulation,
0x0302 DrawProcedural, 0x0303 SetPopulation. **Append at the END of
`commands.zidl`** — the golden generator stamps each command's index as its
sample `source_id`, so inserting beside DrawForm rewrites nine unrelated goldens.

**F4 — the Translator already refuses non-adjacent groups.**
`field_v3_earth_directed.cpp:323` returns false with *"an operand group whose
members are not consecutive registers"*. L1 inherits a correct behaviour, not a
hazard.

**F5 — `zhao_field_progdir` already holds per-association.** `:22` states the
policy. The POLICY is present; the BINDING OBJECT (P8) is not.

**F6 — the smoke's negative assertion is exactly where the directive says.**
`tb_zhao_console_core_smoke.sv:6410` fatals if `fld_runs_o != 0`, with three
sibling assertions that are already a coherent refusal-path control. **Preserve
all four; add `-FieldActive` as a NEW SWITCH WITH ITS OWN BUILD-DIRECTORY TAG** —
a switch without its own tag silently shares the plain run's object directory.

### Absent as claimed — verified by `ls`, not inferred

`zfield_host_plan.hpp`, `zfield_host_plan.cpp`, `pack_field_host.cpp`,
`zhao_field_host_v2.sv`, `zhao_terrain_patch_v2.sv` — **none exists.** The
directive labels these "PROPOSED new paths, not existing evidence", which is
exactly right.

### Present as claimed — verified rather than inherited

`zhao_field_doorbell.sv:440` (catch-all `else`), `:425,435` (`cfg_plan_base_i`
sampled at drain), `zhao_field_host.sv:1116` (`E_ZERO` not optional),
`zhao_field_v3_sbank.sv:74` (one flat bank), `zhao_terrain_patch.sv:125–126`
(`fld_add_*` trace only), `zhao_field_ops_pkg.sv:68` (`OP_RING` absent),
`field_v3_earth_directed.cpp:145` (`kGroupsPerAssoc = 273`).

### The one the directive is right about without realising why

**`zhao_terrain_patch.sv:154–156` has `fld_valid_i / fld_ready_o / fld_height_i`
and NOTHING ELSE — no velocity, material or nav input on the block at all**, and
the console boundary is the same at `zhao_console_core.sv:4971–4973`. So §20.8's
warning not to *"close I34 by wiring only height while declaring the other three
channels present"* is not a hypothetical temptation: **wiring only height is the
only thing the current ports permit.** Closing I34 honestly requires a port
change → `gen_prod_top.py` and `gen_console_board.py` regeneration → C1's act.
**Put this in E1's brief explicitly, or E1 will close I34 wrongly and pass every
gate.**

### The claim the directive discharges without saying so

**R91 carries an open caveat returned to the owner:** *"`E_ZERO` guards
read-before-write, so the fast path needs `zfield::decode` to DECLARE the uniform
set — if that declaration cannot be made honestly, the lever does not exist."*
The directive answers it in §6.3: `zfield::decode` does **not** discover
constancy; `zfield::plan` accepts a **varying mask**, and the association builder
chooses it from the real producer and freezes the uniforms. **The declaration is
made honestly by a different component than R91 assumed.**
*Recommendation:* record R91's caveat as discharged by FH08 + FH09 + §6.3, naming
the component. **An unread instruction and a satisfied instruction look identical
from here.**

---

## 7. What needs a fit, and what does not

**Verilator — seconds, no fit:** output completeness and the ordinal↔window
translation; retirement fences; per-lane status; the no-clear differential;
loader transaction and every refusal control; scheduler fairness and credit
isolation; Earth semantic equivalence and the 297/273 counts; Warp
identity/nonidentity; adapter conversions; parameter-sensitivity guards;
determinism and replay. **Throughput in clocks is Verilator too** — the
6,000/850,000 envelopes are cycle counts.

**Quartus, batched into ONE fit at C1:** the ALM/DSP cost of FH11's width
reversal; M10K inference for the Earth accumulator's 273-row tail; the RF's
12 × 128×128 arrays; Fmax of the new host's arbitration and routing boundaries.
State **PHYSICAL FIT PENDING** rather than launching one early.

**Unmeasured numbers in the directive — quote as structural, never as results**
(it says so itself): "can require 32 M10Ks" (structural calculation), "about 20
M10Ks" (packing candidate, explicitly conditional on inference), "about 48
M10Ks" for the RF (structural expectation), "at CTX32 can double it"
(unmeasured), the historical placed 18.5 MHz (a PAST result, not this design's),
"49 + T_run" / "51 + T_run" (arithmetic on the FSM, not a run).

**And one number the directive does not have:** the console fit row reading
47,582 ALM / 151 DSP **does not contain FIELD.** Every FIELD area number in this
campaign is additive to a budget already over.

---

## 8. Register movement

Measured at HEAD: **21** (9 + 11 + 1).

**Closes — 2, at the far end:** **I34** by E1, and only if all four Earth
channels are routed (needs the port change above). **GEOM.WARP** by W1, and only
if it composes live — a tie-off does not count and must not be attempted.

**Possibly closes — 1, needs a decision:** `zhao_terrain_velocity`. Its
`lane_velocity_i` is Earth out-lane 1 of the same evaluation that feeds height,
so E1 unblocks its data — but `zhao_console_core.sv:965–968` records a second
blocker: **two walkers over one page**, and *"joining two address masters is a
scheduler, and a composer may not write one."* **A stretch goal with its own
owner decision, not a promised close.**

**Does not touch — 18.**

**Three hazards that would CREATE gaps, each with its avoidance:**

1. **W1 building `zhao_geom_warp.sv` with a tied-off Field port** — converts
   "honestly absent" into "present and disconnected". Total unchanged, character
   worse. *Avoidance:* W1 gated behind A1 + C1.
2. **E1 promoting the probes without composing them** — moving them out of
   `synth/` changes their disposition from `instrument` (settled) to something
   that must be composed or counted; landing built-and-uninstalled adds a
   disconnected entry. *Avoidance:* promote and compose in the same wave.
3. **`zhao_field_host.sv` retained as an oracle but still instantiated** —
   `superseded_in_closure()` fires. *Avoidance:* the oracle lives in `tests/`,
   never in the production source list.

**Most honest statement: 21 → 19, and the first five packets leave it at 21.**
The directive says so itself at §20.2 — *"This is a real shared correctness
commit even if the mandatory gap count stays 21"* — and it is right. Waves 1 and
2 are dependency work; a packet reporting "the register did not move" there has
done its job, exactly as fieldp4 did.

---

## 9. Standing rules — cut verbatim into every packet brief

1. **Never close a gap by removing, narrowing, stubbing, tying off or
   disconnecting function.** Where the directive's shortest path does this (C4),
   the honest path is factoring plus a paired oracle.
2. **Never compose an older version.** The old host is an oracle *in tests*,
   never in a source list.
3. **Every packet ends in evidence. A detector reading zero is a claim.** Fire it
   by stimulus or commit a mutant with a driver AND a negative control — and for
   a macro-selected mutant, **compile with the macro undefined and confirm the
   output differs**, or the selector never engaged.
4. **A mutant is a COPY and goes stale in the flattering direction.** Run
   `mutant_copy_drift.py`; three-way merge to refresh, never transplant.
5. **Shared files are staged by hunk from a private index**; check
   `git diff --cached --name-only` before every commit. `[IO.File]` gets an
   absolute path, always.
6. **A suite reads the live tree.** Do not edit RTL while a `ctest` runs.
7. **Report four gates separately** — semantic, composed, workload, physical —
   and say PHYSICAL FIT PENDING rather than inventing one.
8. **Capture full logs first, then summarise.** Name the case that discriminates
   the defect from a correct implementation; **a test count is not coverage.**

> *A mask that counts the wrong thing is more dangerous than no mask at all,
> because it looks like it is watching.*

That is the whole assignment in one sentence, and it is why the ordinal-versus-
window translation gets its own named types and its own compile-fail control
before a single line of the new host is written.
