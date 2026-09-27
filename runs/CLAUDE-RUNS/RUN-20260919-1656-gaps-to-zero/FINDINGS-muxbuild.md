# FINDINGS — MUXBUILD, 2026-09-27

Branch `gz/muxbuild`, worktree `C:\programmieren\zencrifice\gz-muxbuild`,
base `b4bd4830`. Reported against the brief's nine numbered Deliverable items.

**The headline, so it is not buried:** the brief said every refusal's stated
reason is now discharged, and that is true — so I built. What stops the swap now
is **a third thing nobody has named in six packets, and it is structural**:
`job_*` is **not a port**. The raster consumer `zhao_raster_tile_pipe_v2` is a
CHILD of `zhao_geom_bin_pipe_v2`, fed over internal wires from the binner inside
that same module. There is **no interface at which a second source could be
admitted**. The multiplex's back end is available exactly as SWAPBUILD proved;
what it would feed has no door.

---

## 1. THE REGISTER, MEASURED BARE, AND `I55` DID NOT CLOSE

`python tools/budget/completion_register.py`, run bare, never through a pipe
(RC 1 is normal while gaps remain):

| | value |
|---|---|
| at base `b4bd4830` | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |
| at my last pushed commit | **MANDATORY GAPS REMAINING : 2** — `I34`, `I55` |

**`I55` did not close, and this packet does not claim it did.** Every pixel
still comes from `zhao_geom_binner_v2`'s on-chip drain. In the console
`paramwalk dirs/chunks/tris` is still `0/0/0` and `walk_valid_i` is still
`1'b0`.

No gap was closed by removing, narrowing, stubbing or tying off anything. One
tie-off was **removed** — the walker's ProjectedVertex arm — and replaced with a
real fetch.

---

## 2. THE MULTIPLEX — THE BACK END IS AVAILABLE AND THERE IS NOWHERE TO PUT IT

### What I built: the vertex-fetch arm, inside `zhao_geom_paramwalk`

This is the piece five packets named as missing and SWAPBUILD deferred only
because the record could not carry the function. PVSCHEMA fixed the record.

`zhao_geom_paramwalk` instantiated `zhao_geom_parambuf` with the **entire**
vertex arm tied to `'0`, under a comment calling it *"an uncashed cheque
authored on purpose"*. It is cashed. Four states — `W_PV_REQ`, `W_PV_VERD`,
`W_PV_BEAT`, `W_PV_DEC` — run three times per descriptor, **between its last
beat and the emit**, so a triangle is offered COMPLETE or not at all. An emit
that ran before its vertices arrived would hand over the previous triangle's
corners with this triangle's ids, which is this repository's own metadata-swap
defect in a new place.

The block now emits the three decoded vertices (x, y, invw24, u/w, v/w, r, g, b,
alpha) beside the ids, plus `t_untex_o`. It emits **decoded fields, not a packed
attribute packet**: the ruling-5 slot order (`GEOM_ATTR_SLOT_*`) lives in the
composer and is already spent on the live path, and packing it a second time in
the walker would be a second expression of that layout with two copies free to
disagree about which slot carries green.

### The drain window is exclusive, and that part of the architecture survives

SWAPBUILD's proof is structural and I re-read it rather than re-deriving it:
`zhao_geom_binner_v2.sv:818` `tri_ready_o = (state == S_IDLE) && !drain_req_r`;
`:931` sets `drain_req_r` unconditionally on `frame_end_i`; `:941` tests it
**before** `tri_valid_i`; bin states 0..5 and drain states 6..11 are disjoint
under one FSM. **That is not the problem.** `u_geom_setup` and
`u_geom_attrpack` really are idle for the drain, and really could be fed.

**I did not exercise the boundary case** the brief asks for (a triangle arriving
as `frame_end_i` pulses), and I say so rather than implying it: there is no mux
to exercise it against, and building a bench for a multiplex that cannot be
connected would be measuring a circuit I already know cannot ship. That case is
the FIRST thing the next packet owes once the door below exists.

### THE BLOCKER: `job_*` is internal, and it costs 2,065 wires to open

Every version of entry `I55`, and
`reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md`, speaks of taking `job_*`
*"from that path rather than from the binner's on-chip drain"* as though `job_*`
were a stream one could mux. **It is not a stream at the composition at all.**

* `zhao_geom_bin_pipe_v2.sv:339-349` declares `job_valid_w`, `job_ready_w`,
  `job_ax_w`..`job_cy_w`, `job_first_w`, `job_last_w`, `job_tile_x_w`,
  `job_tile_y_w`, `job_source_w`, `job_meta_w`, `job_profile_bad_w`,
  `job_tile_index_w` as **internal wires**.
* The producer is `u_binner` (`zhao_geom_binner_v2`) at `:351-386`; the consumer
  is `u_tile` (`zhao_raster_tile_pipe_v2`) at `:415-432`. **Both are children of
  that one module.**
* The only `job_`-named things on its port list are two counters,
  `jobs_taken_o` and `job_stall_clocks_o`, and `zhao_shell_top_v2` wires both to
  unused nets.

Counted from the declarations, at the console's ratified `METAW = 1877`:

> valid + ready + 6x21 corners + first + last + 2x12 tile + 16 tile_index
> + 16 src_id + 1877 meta + 2 profile_bad = **2,065 wires**

Those must become ports on `zhao_geom_bin_pipe_v2` **and then on
`zhao_shell_top_v2`**, or `u_tile` must be lifted out of the bin pipe entirely.
Either is a subsystem retirement on the block the fit budget is tightest on, and
this packet is forbidden a console fit. **It is named and not taken.**

This is not an engineering impossibility. It is a build with a number on it, and
it is the next packet's.

---

## 3. THE SDRAM SLOT QUESTION, ANSWERED — NO NEW SLOT IS NEEDED, AND NONE EXISTS

Confronted **before** designing the feed, as the brief required, and verified in
the tree rather than inherited from the handover or from my two predecessors.

**The vertex arm needs no slot.** Its reads issue on `guard_req_o`, the socket
`zhao_geom_paramwalk` already owns. That FSM already multiplexed three request
kinds — directory, chunk, descriptor — onto one port, so a fourth kind is a
**state-machine addition, not a client addition**. Directive §7's *"prefer
sharing an appropriate existing guaranteed read route"*, taken literally.

**And there is no slot to be had if one were needed:**

* **`zhao_geom_mem_adapter` has ten requesters `a`..`j`, all ten driven** by real
  producers (MESHFETCH, ASSETFETCH, MATERIAL.RESOLVE, DRAWJOB, PART.TABLE,
  FORGE.PRIM, GEOM.POSE, GEOM.LADDERBANK, TEXTURE.PALETTELOAD,
  TERRAIN.NORMALMAP). Its header line 1 still says *"NINE"* and is stale by one.
  Two connections (`c_req_i`, `i_req_i`) are built in an `always_comb` whose
  first line is `'0` — that is the comb **default**, immediately overridden, and
  not a tie-off. PVSCHEMA's refinement is confirmed: **there is no `N`
  parameter**; the letters are hardcoded ports, so widening is a port pair plus
  arbitration plus a round-robin re-proof.
* **`u_geom_wshare` is `zhao_mem_share_wr #(.N(3), .CLIENT_ID(3), .RQ(4))`** and
  all three read legs are driven: `gs_req[0] = ma_m_req` (the merged adapter),
  `gs_req[1] = pa_req` (the arena writer), `gs_req[2] = pw_req` (the walker).
  Only the WRITE halves of legs 0 and 2 are tied, deliberately.
* Every other share in the tree is also full: `u_build_share` `N=5`,
  `u_terrain_rdshare` `N=3`, `u_engine0_share` `N=3`.

### AND DIRECTIVE §7's ESCAPE HATCH IS SHUT — nobody had checked

§7 says to prefer *"the currently reserved client slot **if the LIVE design
still has it available**"*. **It does not have it available.**

* `zhao_vram_arbiter.sv:107` — `localparam logic [2:0] RESERVED_ID = 3'd5;`
* `:246` — *"RESERVED_ID appears in no arm above"*
* `:353` — `port_grant[RESERVED_ID] = 1'b0;` — the port cannot even latch a
  request, deliberately, because a slot the selector can never pick would read
  at the requester as a hang rather than as a refusal.

`zhao_terrain_prepshare.sv:45-60` had already measured exactly this for its own
lane and written it down. **Confirmed, not re-derived** — and worth recording
here because the directive's conditional is the kind of sentence a future packet
will quote as though it were a permission.

**On `spec/memory_rules.md`'s 22 MiB self-contradiction:** I did not resolve it
by picking a half. PVSCHEMA's tiebreaker stands and is RTL rather than prose —
`zhao_pkg.sv:329-330` defines `ZHAO_RENDER_ASSET_BASE`/`_SPAN` over exactly
`[0x06A0_0000, 0x0800_0000)` and `zhao_mem_guard` admits ENGINE1 reads there. **A
region with a live guard window is not unmapped.** This packet needed none of it:
the vertex arm adds no address space at all, and TD v2 (§7 below) fits inside the
view's own declared span.

---

## 4. `paramwalk` DID NOT MOVE OFF ZERO IN THE CONSOLE — REFUSED, SIXTH TIME

```
SMOKE (unchanged, not re-run by me): paramwalk dirs=0 chunks=0 tris=0
```

`walk_valid_i` is still `1'b0`, `walk_head_i` still `32'd0`, and every `t_*`
output is still unconnected at `zhao_console_core.sv:31049-31065`.

**Refused for the sixth time, on the ground the previous five gave and which is
still exactly true:** with no consumer on `t_*`, a tile sequencer would count
triangles and drop them. Nothing in this packet changes that, because the
consumer is what §2's 2,065 wires are about. Wiring it would move
`chunks_walked_o` and `tris_emitted_o` off zero tomorrow and would be logic added
to make a counter move.

**It moved a long way in the bench, and that is real.** `verts_read_o` reads
**57** — fifty-seven ProjectedVertex records fetched back through the **real**
guard, arbiter, controller and SDRAM model and decoded by the real
`zhao_geom_parambuf`. That is not the console and I do not present it as the
console.

---

## 5. NO PIXEL DEPENDS ON BYTES THAT WENT THROUGH SDRAM — AND HERE IS WHY NOT

The measurement, not a deferral: **there is no port on the raster consumer.**
§2 has the count (2,065 wires) and the two ways to open it. Until one is taken,
no back end — multiplexed, duplicated or otherwise — can deliver a `job_*` to
`zhao_raster_tile_pipe_v2`, because nothing outside `zhao_geom_bin_pipe_v2` can
address it.

**What CAN be shown without the back end, and is:** the six Packet-D plane
inputs survive the round trip at full precision. `geom_paramarena_directed`
case 13 requires every field of all three vertices of all six of frame A's
triangles back **bit-identical** through the real fabric, and **asserts its own
discriminating premise first**: frame A's eight reds are `0x1200..0x1207`, eight
distinct values that v1's `(v+128)>>8` maps to **one byte**. The test proves that
collision before requiring them back distinct. `{255,255,255}` proves nothing —
saturated white survives any quantiser — and is not used.

That is the half of *"the planes are recomputable"* that can be demonstrated
without a back end, and until this packet it had never been demonstrated at all.

---

## 6. THE ID REPAIR AND SCHEMA v2 BOTH HOLD, WITH THEIR CONTROLS RE-FIRED

`ctest -R "paramarena|arenabin|parambuf|vertid|tidq|cmd_exec_directed"` —
**16/16, 100%, 0 failed.** In particular:

* `geom_tidq_directed` — RASTERSWAP's repair, **Passed**, unchanged.
* `geom_vertid_directed` — schema v2's producer half, **Passed**.
* `geom_paramarena_directed` — **550 checks, 0 failures** (351 at PVSCHEMA's
  commit).
* `geom_arenabin_directed`, `geom_arenabin_price` — **Passed**.
* the three committed arena mutants, `fires` **and** `silent` — **all six
  Passed**.
* `mutant_copy_drift.py` — RC 0, 78 copies, **run AFTER the commit** per R121.

**I re-fired the controls rather than quoting PVSCHEMA's numbers**, because my
change alters the bench all six mutant drivers share and PVSCHEMA's own headline
finding was that a width change one file away can turn a mutation into a no-op
with every instrument green.

### Two NEW counters, both fired with legal stimulus, both shown silent first

Neither owes a committed mutant, and the reason is the useful one: **both faults
are things the round trip can produce**, so `poke16` into the SDRAM model
reaches them.

* `verts_illegal_o` — a ProjectedVertex whose **status byte** is malformed
  (`status[7:4] != 0`, which `GEOM.VERTID.md` rules *"nonzero is a malformed
  record"*). Fired **2 → 3**.
* `t_pv_split_o` — one triangle's three vertices disagreeing about `untex`.
  Fired **13 → 14**, isolated from the above, with `verts_illegal_o` held
  unchanged across it to prove the two watch different faults.
* Both **stop** when the record is restored. A control that fires and then keeps
  firing on healthy input is a stuck bit, not a detector.

**Case 14 built its own frame for them**, and that mattered: no earlier frame in
that file is sound in the relevant way, because the fixture gives every vertex
its own status byte, so `t_pv_split_o` fires there **by construction**. A counter
shown firing without first being shown silent is not a control. Case 13's
expectation for the fixture is therefore **computed from the fixture** rather
than pinned to a number.

### And a detector was repaired before it could go blind

`burst_unaligned_o` tested `m_addr_q`, which was the only source of
`guard_req_o.addr` when it was written. The vertex arm's address is
**combinational** — `td_buf_q` is still shifting when `W_TD_BEAT` decides the
next state, and `pv_idx_q` advances between the three requests — so the tripwire
would have gone silent on **exactly the requests this packet added**, while
keeping its name and its zero. It now tests `guard_req_o.addr`. It remains an
invariant over one value against a constant, so the lockstep-blindness question
does not arise. Case 12 still fires it.

### `pv_illegal_o` has a reachable reader for the first time

PVSCHEMA moved the s21 refusal to the encoder and left `pv_illegal_o` watching
the status byte — correctly, and with no consumer anywhere in the design. The
walk is now one, and case 14 is its demonstration.

---

## 7. CLAIMS IN THE BRIEF OR THE RECORDS FOUND FALSE

### 7.1 — THE BIG ONE: "take `job_*` from that path" — there is no `job_*` to take

`reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md`, under CHOSEN OPTION:
*"feed them from the SDRAM walk instead of from GEOM.CLIP, and take `job_*` from
that path rather than from the binner's on-chip drain."*

The first half is available. **The second half has no referent.** §2 has the
evidence and the 2,065-wire price. The record is right that the multiplex is the
correct architecture and right that the instance form is refusable; it is wrong
that the output side is a matter of choosing a source.

**Note the direction.** The false reading made the remaining work look SIMPLER —
"mux the source" rather than "open a 2,065-wire interface through two module
boundaries". That is this file's own law, and it is the third consecutive packet
to find the inherited plan understated in exactly that direction.

### 7.2 — `kc0 + kc1 + kc2 = 2A` is CIRCULAR in this RTL, and believing it would have shipped a derivation that cannot fail

The barycentric identity is true, and `zhao_geom_setup` emits all three
constants, so `2A` looks recoverable from its own outputs with two adds and no
multiplier. `zhao_geom_setup.sv:384-387`:

```systemverilog
      s3_kc0 <= sxprod(s2_p0) - sxprod(s2_p1);
      s3_kc1 <= sxprod(s2_p2) - sxprod(s2_p3);
      s3_kc2 <= s2_area2 - (sxprod(s2_p0) - sxprod(s2_p1))
                         - (sxprod(s2_p2) - sxprod(s2_p3));
```

**`kc2` is DEFINED as `area2 - kc0 - kc1`.** The block computes only TWO of the
three cross products and spends the supplied `2A` to avoid the third. A back end
built on the identity would have computed `kc2` as `(kc0+kc1+kc2) - kc0 - kc1` —
**identically `kc2` for any garbage `area2` whatsoever** — producing plausible
planes from an unconstrained number with every handshake healthy and every
counter balanced. **A derivation that cannot fail is not a derivation.**

The check that separated them was reading four lines of the block being fed.

### 7.3 — `GEOM.SETUP` takes five fields the arena does not store, and its WRITER is not even handed them

Neither decision record mentions `tri_area2_i` or the four s12 scan-box bounds.
`zhao_geom_vertid`, which writes the TriangleDescriptor, receives the corners,
the attributes, `tri_untex_i`, `tri_material_i`, `tri_raster_i` and
`tri_src_id_i` — **and not those five**. Decided in
`reports/DECISION-20260927-TRIANGLEDESCRIPTOR-V2.md`: TD v2 at 32 bytes, first 16
byte-identical to v1, fitting at full R7 capacity with 524,288 bytes spare.
**Decided, not built here** — see §8.

### 7.4 — Two stale numbers in `zhao_geom_paramarena.sv`, both unchecked for a reason the file itself states

* `:602` — `localparam int unsigned PV_B = ZHAO_PARAMBUF_PV_BYTES;   // w=24 bytes`.
  The package has said **32** since PVSCHEMA.
* `:654` — `CHUNK_OFF_B ... // 2,359,264`. The value is **2,359,296**;
  `VIEW_USED_B`'s own comment two lines later (3,407,872) implies it and is
  correct.

`check_localparam_comments` **skips a constant whose value reaches a package
import** — which the block's comment at `:647-651` explains at length, two lines
before the unchecked number is wrong. Both corrected in this packet's last
commit.

### 7.5 — A dead, stale constant in the directed test

`geom_paramarena_directed.cpp` carried `constexpr uint32_t PV_B = 24;` under a
comment reading *"`PV_B` is the RECORD (R7 freezes it at 24 bytes)"*. Schema v2
made it 32 under directive §4. **Nothing went red, because the constant is
dead** — every address in that file is built from `PV_SLOT_B`. A dead constant
with an authoritative comment is a document that cannot go stale loudly.
Corrected.

### 7.6 — A dead wire in `zhao_geom_paramwalk`, and it is INHERITED

`id_c` has no reader. The CHUNKSER repair added `id0_c` for a chunk's first id
and `nxt_id_c` for the next, and the wire between them was orphaned. It survives
because `tests/shell/v3_closure_inherited.vlt` waives `UNUSEDSIGNAL` across whole
directories — the brief's own warning, arriving in the block the brief sent me
to.

**Measured inherited, not asserted inherited**, per CLAUDE.md's rule about
reading the dates before calling a red somebody else's: linting a throwaway copy
of the file at `b4bd4830` reports the **identical one warning**. Left in place
and named here; removing it is not this lane's business and would add diff noise
to a file the mutants sit beside.

### 7.7 — The brief's "handover says two, it carries TEN" is right, and the sharper fact is that there is no `N`

Confirmed independently (§3). The adapter is not parameterised on its requester
count at all, so "widen `N`" understates the cost.

---

## 8. WHAT I REFUSED, AND WHAT I GOT WRONG AND CAUGHT MYSELF

### Refused

* **Exporting `job_*` from `zhao_geom_bin_pipe_v2`, or lifting `u_tile` out.**
  2,065 wires through two module boundaries on the block the fit budget is
  tightest on, with a console fit forbidden to me. Named with a number, not
  routed around. This is the deliverable the brief said a refusal with a number
  would be.
* **Building TD v2.** Decided and recorded with its capacity arithmetic; not
  built. The size is the weak reason and I give the strong one: **TD v2 costs
  live SDRAM write bandwidth every frame for a consumer that cannot exist yet.**
  The descriptor arm goes from 2 beats to 4 on the WRITE path, which runs on
  every triangle of every frame in the shipped console — while the only thing
  that would read `area2` and the box is a back end that has nowhere to deliver
  a `job_*` until the 2,065 wires above are opened. That is an uncashed cheque
  with a measured price attached, which is the one shape this campaign has paid
  for most often.

  **The vertex arm is deliberately not in that category**, and the distinction
  is worth stating because it is what made one buildable now and not the other:
  `walk_valid_i` is tied off, so the walker never walks in the console and the
  arm costs **no traffic at all** there — it is area, and it is measured by the
  coordinator'''s fit like any other. TD v2 would start paying immediately.

  So the ordering is: open the raster door, THEN TD v2, THEN the multiplex. The
  decision is executed to the point of being buildable and I say plainly that I
  stopped there, and why.
* **Wiring `walk_valid_i` / `t_ready_i`.** Sixth refusal; §4.
* **ORing the walk into the live stream.** Forbidden and not done.
* **Recomputing `2A` or the scissored box on the walk side.** A second
  EXPRESSION of `zhao_geom_clip.sv:460`'s winding-flipped area law and of the
  §8 scissor law — precisely the verification burden the time multiplex was
  chosen to avoid.
* **Adding `untex` to the descriptor.** It is already `status[2]` of every
  ProjectedVertex; a second storage site for one fact is how two copies come to
  disagree.
* **Touching `zhao_geom_setup`'s `tri_src_id`.** Not widened, not narrowed, not
  touched — I did not edit that file at all. Nothing in this packet makes
  `reports/DECISION-20260927-I34-MATERIAL-CARRIER.md`'s widening of that rider
  harder: the vertex arm carries no `src_id`, and the mux that would have
  touched the fork was not built.
* **Any Quartus run.** No fit, no map, no `-MapOnly`. **This packet makes no
  area or timing claim whatsoever.**

### Wrong, and caught

1. **I nearly built the feed on the barycentric identity.** It is elegant, it is
   true, and it would have produced a circuit whose `kc2` was correct for any
   `area2` at all. I checked only because I was about to write the port list and
   wanted to know whether `area2` was a passthrough. **The check was four lines
   of a file I already had open** — CLAUDE.md's own line about where the error
   lives, for the third packet running.
2. **I put my two new cases at the END of `main` and they measured a frame that
   was no longer published.** Five failures: the walk returned zero triangles
   because by then the live frame was not frame A. Moved into case 1's own walk —
   which is better anyway, because case 2b asserts **absolute**
   `dirs_read_o`/`chunks_walked_o`/`tris_emitted_o` and an extra walk there would
   have broken three unrelated checks. A test that renumbers its neighbours'
   counters to make room is a test that will be blamed for their next failure.
3. **I asserted `t_pv_split_o == 0` on frame A, which is false by
   construction.** The fixture gives every vertex its own status byte, so
   consecutive vertices genuinely disagree about `untex`. Had the placement error
   not made it fail loudly, I would have shipped a counter "shown silent" on a
   fixture where it cannot be silent — and then "fired" it, with the negative
   control vacuous. The repair is a dedicated frame whose three vertices share
   one status byte, plus an expectation **computed from the fixture**.
4. **My first patch would have left the alignment tripwire watching the wrong
   register.** I wrote the combinational vertex address and only then noticed
   `burst_unaligned_o` tests `m_addr_q`. It would have passed every gate, kept
   reading zero, and stopped watching a quarter of the block's requests.
5. **I walked into the documented heredoc trap.** `PACKET-PROTOCOL.md` warns
   that heredocs into `bash -c` fail on quote-heavy text; I used one for a Python
   patch script full of nested quotes and got `unexpected EOF`. Failed loudly, no
   damage — but it is written down and I did it anyway.
6. **My first patch script matched nothing, and the cause was line endings.**
   The walker is CRLF (756 of 756 lines) and my anchors were LF, so the patcher
   reported "anchor occurs 0 times". CLAUDE.md's *"normalise the base's line
   endings before you believe the conflicts"*, arriving as a patcher instead of
   as a merge. The script now normalises in memory and restores on write — and
   it then found, correctly, that `zhao_console_core.sv` is LF while its
   neighbours are CRLF, exactly as the brief warned.

---

## 9. BRANCH, COMMITS AND GATES

**Branch `gz/muxbuild`. Pushed. Never `--force`, never `--force-with-lease`.
Not merged to the integration branch.**

| commit | what |
|---|---|
| `d91e91af` | `decide(MUXBUILD)`: TriangleDescriptor v2 carries 2A and the scan box |
| `6a5f8a0d` | `feat(MUXBUILD)`: the ProjectedVertex fetch arm, and the cheque is cashed |
| `5ab05d6c` | `docs(MUXBUILD)`: entry `I55` amended -- the blocker is that `job_*` is not a port -- plus the two stale arena comments and these FINDINGS |
| `373e6357` | `test(MUXBUILD)`: the three arena mutant copies refreshed and all six re-fired |

**`mutant_copy_drift.py` went RED at `5ab05d6c` and it was right and it was
MINE.** The signal is PROVENANCE, not similarity: a comment-only correction to
`zhao_geom_paramarena.sv` made production newer than its three copies, and a
copy that predates production cannot contain what production gained. That the
change was *only comments* is exactly the reasoning the tool refuses to accept,
and it is correct to refuse it. Refreshed with the committed command
`tools/rtl/gen_paramarena_mutants.py`, then **re-fired** -- because PVSCHEMA'''s
headline finding was a mutant with perfect provenance and an intact mutation
that had silently become a no-op. `ctest -R paramarena`: **7/7**, three
`_fires` firing and three `_silent` silent, on the refreshed copies. Drift is
RC 0 at `373e6357`, measured after the commit.

### Gate state

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — no higher than the 2 I started at |
| `check_console_inventory.py` | **OK** — 407 modules, 287 elaborated |
| `check_prod_manifest.py` | **OK** — 407 modules, 88 tops |
| `gen_prod_top.py --check` | **fresh** (regenerated; 88 instances) |
| `gen_console_board.py --check` | **fresh** (1613 core ports) |
| `gen_shell_paired_diff.py --check` | **fresh**, harness and mutant |
| `check_quartus17_syntax.py` | **RC 0**, 666 files, self-test 13 fire / 22 no-fire |
| `check_case_labels.py` | **OK**, self-test 3 fire / 1 no-fire |
| `mutant_copy_drift.py` | **RC 0**, 78 copies, **run AFTER the commit** (R121) |
| `ctest -R "paramarena\|arenabin\|parambuf\|vertid\|tidq\|cmd_exec_directed"` | **16/16, 100%** |
| `test_cmd_exec_directed` (ruling R60, run directly — it does not match the ctest regex above) | **977 checks passed**, RC 0 |
| `check_entry_claims.py` (run after editing entry I55) | **OK** — 23 known sites, no NEW claim |
| Verilator `-Wall` on `zhao_geom_paramwalk` | **base-neutral**: 1 inherited warning, measured identical at `b4bd4830` |
| `npm run abi:check` | not implicated — `spec/commands.zidl` untouched |

**Console smoke and the console-board lint were not run.** This packet changed no
`zhao_console_core` port — only a leaf's, which `zhao_prod_top` absorbs — and the
coordinator gates the merged result. Stated rather than implied.

---

## THE ONE-PARAGRAPH HANDOVER

The vertex arm is **built and tested**: the six Packet-D plane inputs come back
out of real SDRAM bit-identical, and a v1 record provably could not have carried
them. The SDRAM question is **closed** — no new slot is needed, none exists, and
directive §7's reserved-client escape hatch is structurally shut. What remains is
**two builds, both now sized**. First, `TriangleDescriptor` v2, decided and
costed, because `GEOM.SETUP` consumes `2A` and the scissored box and neither is
in the record nor recoverable from `GEOM.SETUP`'s own outputs — `kc2` is
*defined* from `area2`, so the identity that looked like an escape is circular.
Second, and it is the one nobody had seen: **`job_*` is not a port.** The raster
consumer is a child of `zhao_geom_bin_pipe_v2`, fed over internal wires from the
binner in that same module, so the multiplex has a back end and no door. Opening
it is 2,065 wires through two module boundaries, or lifting `u_tile` out. That is
a subsystem retirement on the tightest block in the design and it wants the fit
this packet was forbidden — so name it in the plan, spend one fit on it, and do
not let the next brief inherit "mux the source" as though it were a wire.
