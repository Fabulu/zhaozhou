# FINDINGS — TAGPROD (entry I20, `tri_continuation_tail_i`'s producer)

**2026-09-21. Register 21 → 21. Branch `gz/tagprod`, head `07b8ef82`, merged.**

Transcribed by the coordinator from the lane's two commit messages. The harness
refused the lane's own write to this path, as its brief predicted.

**Headline: I20 REFUSED — the third refusal, and FORGESHADOW was right. What is
new is that the refusal is now FIELD BY FIELD, each blocker verified
first-hand. And R219's mandate was delivered anyway: the glow is provable end
to end.**

---

## 1. The refusal, field by field

`tri_continuation_tail_i` is 48 bits and each field fails differently:

* **`vertex_rgb` [47:24]** — the vertex's. **No provoking-vertex law exists
  anywhere in the tree** (searched all `.sv/.md/.hpp/.zidl`, zero hits), so
  there is not even a convention to appeal to.
* **`vertex_alpha` [23:16]** — R89 ruled the *route*; the **value** is
  unproduceable. Re-verified at this HEAD: `cast_strength_i` has no producer
  outside the LFSR census top, and `zhao_geom_lodstate` is instantiated nowhere.
* **`stencil_reference` [7:0]** — **named for the first time.** Every prior pass
  said "four constants" without saying which. Its only producers are bench
  stimulus and a probe. `RASTER.FRAGMENT`'s contract says it **invented** the
  stencil function set because no spec defines one — *a value whose enum was
  invented by its consumer has no producer by construction.*
* **`effect_tag` [15:8]** — see §2; *"this one is not what it looks like."*

### A stale sentence was sending the next packet to the wrong lane

I20 named **I49** (MATERIAL.RESOLVE's issue/join) as *"the exact blocker for the
three open ports"*. That was true for `tri_flat_request_i`, **which closed.**
**Neither remaining port was ever I49's** — the lane walked
`zhao_material_resolve.sv`'s whole response port and *not one* of the tail's
four fields appears in it. **Corrected in place.**

## 2. The effect tag is half-built already, and the seam is the *other* port

`spec/stars_and_flares.md` §1 is frozen: strength = **the source texel's CLUT
intensity** — per-fragment, not per-triangle. `zhao_raster_fragment.sv` already
implements it, and `s1_tidx_r` is the index `zhao_raster_texture_stage_v3` emits
as `frag_texel_idx_o` **one port over** from `frag_tag_o`, off the same result.
The selector is `tri_fragment_state_i[21]` / `[23:22]`.

**So for the star recipes the tail is not the seam at all.**

**The half that genuinely needs it is `sun_additive`**, and the reference model
says so in its own words — *"the sun's glow tag therefore rides the packet's
constant `tag` field"*. Nothing in the ABI carries it. **Owner decision, and a
small one.** Without it `sun_additive` cannot bloom.

*(Coordinator: re-docketed by **R224**. The tail is the reference model's
`Frag` constant group field for field — 24/8/8/8 — so the values are already
ratified and the gap is the per-draw constant PATH, i.e. the CMD executor gap
of I14/I30. Not a new ABI field.)*

## 3. The glow IS now provable end to end — R219's mandate, delivered

`run_console_core_smoke.ps1 -GlowTag`, green in both polarities:

| form | lit | pixels w/ tail colour | bloom cells | words changed |
|---|---:|---:|---:|---:|
| **-GlowTag** | **1062** | 1062 | **1344** | **1344** |
| plain | 0 | 0 | 0 | 0 |

**Nothing closes I20** — the bench drives a boundary port, which is *stimulus*;
the composer may not. **The colour is load-bearing:** the glow borrows the
fragment's own colour, so the tag alone would have moved a counter and lit
nothing.

### The lane's first assertion was wrong and taught the most

*"Tag on every triangle ⇒ all 2560 lit"* **failed at 1062.**
`gather_fragments_o` **is not the covered-fragment count** — RESOLVE sweeps a
touched *tile whole*, so 1,498 of those are tile-clear, and untagged is correct
for them. Confirmed by three counters the lane does not own.

**The shipped assertion is now a cross-check between two instruments sharing no
logic:** the law's counter must equal the bench walking the framebuffer.

**Counters proved to fire:** `post_bloom_cells_contributing_o` **was connected
in the core and read by nothing** — now 0 (negative) / 1344 (positive).
`gather_frag_lit_o` 0 → 1062.

**Also settled:** POST.ECHO taps the compositor's **output**, not its source —
unanswerable under an identity pass; now asserted in every armed form. And the
sentinel-overwrite count was comparing to a *colour* (`== 16'h0000`), true only
while I20 keeps fragments black; **it now counts against the sentinel.**

## 4. An instrument defect found by causing it

`completion_register.py` matches `^//\s*(I\d+)\.\s+` against **every line** of
the INCOMPLETE block, so **a wrapped sentence merely BEGINNING with an entry
number registers a phantom gap** — it took the count **21 → 22 with no RTL
change.**

**It reads HIGH**, which is the audited direction; the tool is already hardened
against the low one. **Deliberately NOT changed — twelve lanes gate on its
number.** The exact guard is recorded in its header: entry ids are strictly
monotonic (`I9 I13 I14 I17 I20 I21 I25 I27 I29 I32 I34 I40`) and the phantom
landed between I20 and I21.

> **Do not "fix" it by tightening the indent — `I9` is legitimately
> two-space-aligned.**

## 5. Gates — all green at `07b8ef82`

register 21 · tie-off audit **0 SILENT** · inventory · prod_manifest ·
quartus17 RC0 · case_labels · **wrapper_port_parity 1264 = 1264, missing=0
stale=0** · mutant_drivers · uncashed_cheques · check_counters ·
refmodel_liveness · duplicate_functions · gen_prod_top / gen_console_board /
gen_shell_paired_diff all **fresh** (no port changed — *checked, not assumed*).

**All eight smoke forms PASS with zero `%Fatal` lines** (R207 checked in the
logs, not the exit codes): plain, **-GlowTag**, -LintOnly, -Mutant (fired 1x),
-UntexMutant (fired 16x), -BadVertex, -NoEchoArm, -BadTraceArm.

`mutant_copy_drift` RC=1 on **one pre-existing** copy, **not this lane's**:
`zhao_geom_bonesrc_latefetch_mutant`. *(Coordinator: resolved at `e66909db` —
the upstream change was comment-only; the comparison is recorded in the mutant's
own header rather than the gate silenced.)*

**No tie-off created.** No file narrowed, stubbed or disconnected; the identity
assertion is **inverted** in one form, never removed.
