# FINDINGS — SWAPBUILD, 2026-09-27

Branch `gz/swapbuild`, worktree `C:\programmieren\zencrifice\gz-swapbuild`,
base `62d8b6a7`. Reported against the brief's nine numbered Deliverable items.

**The headline, so it is not buried:** the entry's central claim — repeated by
five consecutive packets — is **false**, and it is false in the flattering
direction. The planes are **not** recomputable from the arena record. The swap
is blocked on a **record schema**, not on a back end, and nobody had looked.

---

## 1. THE REGISTER, MEASURED BARE, AND I55 DID NOT CLOSE

```
BEFORE (62d8b6a7)   MANDATORY GAPS REMAINING : 2
                      (2 tie-offs + 0 disconnected + 0 unbuilt + 0 uncited + 0 unresolvable)
                      I34  boundary       TERRAIN.PATCH's FIELD-HEIGHT LANE
                      I55  unclassified   GEOM.PARAMBUF's WALK REQUEST and DECODED OUTPUT

AFTER               MANDATORY GAPS REMAINING : 2   (unchanged)
```

Run bare, never through a pipe. RC 1 is normal while gaps remain.

**`I55` DID NOT CLOSE AND THIS PACKET DOES NOT CLAIM IT DID.** Every pixel still
comes from `zhao_geom_binner_v2`'s on-chip drain. `paramwalk dirs/chunks/tris`
is still `0/0/0` and `walk_valid_i` is still `1'b0`.

---

## 2. WHAT WAS BUILT OF THE SECOND BACK END — NO RTL, AND THAT IS THE FINDING

**No RTL was written.** Not the vertex-fetch arm, not the mux, not the
sequencer, not the retirement. That is a deliberate stop and section 7 is its
reason: **the record the back end would be fed from cannot carry the function**,
so building the fetch arm against it would be measuring a circuit already known
to be wrong — CLAUDE.md's own rule about spending a measurement on an
arrangement you know is broken.

What this packet produced instead is the thing that had to come first and had
never been done: **the architecture decided on measured numbers, and the
blocker that invalidates the inherited plan found.**

* `reports/DECISION-20260927-I55-SWAP-ARCHITECTURE.md` — the decision record in
  directive section 0's format.
* `reports/synthesis/receipts/swapbuild_second_backend_price.json` — the price
  of the architecture that is refused, with the map report's SHA-256.
* Entry `I55` amended with the refutation, the costed path, and three stale
  numbers corrected.

**The next link is now a BUILD, not a decision**, and it is specified to the
file and line in the entry: ProjectedVertex v2 at 32 bytes, the writer at
`zhao_geom_vertid.sv:493-500`, the decoder at `zhao_geom_parambuf.sv:199-206`,
the packer's `pv_bytes_c` at `zhao_geom_paramarena.sv:804-811`, the reference
model, and the three committed arena mutants — which are COPIES, and copies go
stale in the flattering direction.

### The architecture that was chosen, and why it is not what the entry asked for

Five packets have written that the swap owes **a SECOND setup and attrpack back
end** and that this is *"ADDITION, not substitution"*. That is one
implementation of the requirement and it is the expensive one. The question
nobody asked is whether the second back end must be a second **instance**.

It need not, because the idleness is **structural, not scheduled**:

```systemverilog
  // zhao_geom_binner_v2.sv:818
  assign tri_ready_o = (state == S_IDLE) && !drain_req_r;
  // :931, unconditional, outside the case
        if (frame_end_i) drain_req_r <= 1'b1;
  // :941 -- and drain_req_r is tested BEFORE tri_valid_i
          S_IDLE: begin
            if (drain_req_r) begin ... state <= D_TILE;
            end else if (tri_valid_i) begin
```

Bin states are 0..5, drain states 6..11, **one FSM owns both, and the ranges are
disjoint.** So for the entire window in which the raster consumes `job_*`,
`u_geom_setup` and `u_geom_attrpack` provably have no other work. Neither
carries state across a triangle — `grep -c frame zhao_geom_setup.sv` is **0**,
and `zhao_geom_attrpack`'s three `frame` hits are all comment text.

Feeding that provably-idle pair from the walk is the **same silicon**, so the
planes are bit-identical **by construction** rather than by verification, and
the DSP bill is zero. `zhao_forge_assemble.sv:62-66` states the admissible form
of this argument: *"A second INSTANCE of one law is not a second law; a second
EXPRESSION of it would be."*

---

## 3. `paramwalk` DID NOT MOVE OFF ZERO, AND IT IS REFUSED A FOURTH TIME

```
SMOKE: paramwalk  dirs=0 dirmiss=0 chunks=0 stale=0 illegal=0 depth=0
SMOKE: paramwalk  tris=0 trisbad=0 cut=0 denied=0 short=0 stray=0 genrace=0
```

(Those are RASTERSWAP's measured lines at this same tree, quoted as theirs.)

Refused for the same reason BINARENA, ARENACOMPOSE and RASTERSWAP refused it:
every `t_*` output dangles, so a tile sequencer would count triangles and drop
them. **This packet adds a reason those three did not have.** Even with a
consumer on `t_*`, the triangles the walk delivers could not reproduce the
picture, because three of the six planes cannot be rebuilt from what the arena
stores. Moving the counter would have been a demonstration that the walk runs,
attached to a claim that the swap works, and the second half would have been
false.

---

## 4. THE PRICE, RE-MEASURED — AND A NEW PRICE NOBODY HAD TAKEN

**The consumer side is unmoved and is NOT re-quoted as mine.** This packet
changed no RTL in the closure of `geom_arenabin_price`, so entry I55's
4.12 / 29.58 / 5.88 clocks per reference and the 7.18x ratio stand as RASTERSWAP
re-measured them at this same tree. **This packet did not re-run them**, and
says so rather than presenting them as its own.

**The price that is new is the one that decides the architecture.**
`zhao_geom_attrpack` had **ZERO rows in `zhao_block_fit.json` and
`zhao_block_map.json`** — the back end five packets demanded had never been
mapped or fitted. Row `zhao_geom_attrpack@swapbuild-secondback`:

| | value |
|---|---|
| status | `map_only` (analysis & synthesis, 30 s) |
| `rtlCleanAtHead` / `treeCleanAtHead` | **true / true** |
| `measuredDevice` | **5CSEBA6U23I7** (the shipping part) |
| `sourceCommit` | `62d8b6a7` — this packet's base |
| combinational ALUTs | **1,121** |
| estimate of ALMs needed | **2,354** |
| registers | **2,327** |
| block memory bits | 0 |
| **DSP blocks** | **36** |

With `zhao_geom_setup`'s existing **clean** map row on the same part (500 ALUT /
938 est ALM / 1,340 reg / 4 DSP — **not** the dirty full-fit row, which reads
743 ALM and disagrees in both directions), a second back end costs:

> **+1,621 combinational ALUTs, +3,292 estimated ALMs, +3,667 registers, +40 DSP.**

The part has **112 DSP**. That is **35.7% of the device's entire multiplier
budget for one duplicated block**, on a console already ~335% over on DSP, and
**4.4x the +9 DSP on which LANESCOST refused I34's gathering front** on the same
part in the same week.

**Two things about this row I declare rather than let be inherited:**

* **`notTargetDevice: true` on the row is WRONG.** `run_block_fit.ps1:882-887`
  stamps that flag whenever `-Device` is passed, *without checking whether the
  value is the shipping part*. It was `5CSEBA6U23I7`. The row's own `sizingNote`
  contradicts itself in one sentence: *"fitted on 5CSEBA6U23I7 ONLY to measure
  size; the target is 5CSEBA6U23I7"*.
* **ALUTs live only in `.map.rpt`, and `.map.rpt` is gitignored**
  (`.gitignore:158`). That is *why* no ALUT number survives on disk for seven of
  eight geometry modules — the evidence file cannot be committed. The numbers
  are transcribed into a committed receipt with the report's SHA-256, which is
  the pattern LANESCOST established. This is CLAUDE.md's `.gitignore` chapter
  wearing a new coat: a rule written to stop waste being committed also stopped
  the evidence being committed, and nothing noticed for weeks.

---

## 5. THE ID REPAIR STILL HOLDS — AND IS UNTOUCHED BY CONSTRUCTION

**This packet changed no RTL at all**, so `zhao_geom_tidq`, the console door,
the seal-abort push and the flush are byte-identical to RASTERSWAP's commit.
`geom_tidq_directed` and its 21 base-RTL failures are therefore undisturbed for
a structural reason rather than because a test was re-run: the only file this
packet modified under `fpga/rtl` is `zhao_console_core.sv`, and the only region
it modified there is inside a `//` comment block (entry I55's text).

`git diff 62d8b6a7..HEAD --stat -- fpga/rtl/` names exactly one file, and the
diff is comment lines only.

---

## 6. CLAIMS IN THE BRIEF OR THE ENTRY FOUND FALSE

### 6.1 — THE BIG ONE: "the planes are RECOMPUTABLE". They are not.

Entry I55, in every version since WALKSWAP, and in four FINDINGS files:

> The 24-byte ProjectedVertex **DOES** carry what those need (x, y, invw24,
> status, u/w, v/w, rgba), so the planes are **RECOMPUTABLE** — but only by
> standing up a SECOND setup and attrpack back end fed from SDRAM.

`zhao_geom_vertid.sv:499` — what the arena actually stores:

```systemverilog
  assign pv_rgba_o   = {unit8_of_fx16(al_c), unit8_of_fx16(b_c),
                        unit8_of_fx16(g_c),  unit8_of_fx16(r_c)};
```

`:397` — `unit8_of_fx16` is a **lossy quantiser**: `(v + 128) >> 8`, with
`v[31]` clamped to 0 and `v[30:16] != 0` clamped to 255.

`:487-490` — and its inputs are the **full 32-bit attribute slots**:

```systemverilog
  wire [31:0] r_c  = a_c[3*32 +: 32];
  wire [31:0] g_c  = a_c[4*32 +: 32];
  wire [31:0] b_c  = a_c[5*32 +: 32];
  wire [31:0] al_c = a_c[6*32 +: 32];
```

Those are the same slots `zhao_geom_attrpack` reads to build the six planes. So
the record keeps **8 of 16 fractional bits and saturates at both ends**, and the
**R, G and B Gouraud planes — the three owner ruling R234 D1 added, the ones
that widened METAW from 1157 to 1877 — cannot be rebuilt from it by any back end
whatever.**

This blocks the second-instance architecture **exactly as hard** as it blocks
the multiplex. It is not an argument about which back end to build.

**Note the direction.** The false claim made the remaining work look SIMPLER —
"a second back end" rather than "a wider record AND a back end". It listed
`rgba` beside `x`, `y` and `u/w` as though the four were the same kind of
quantity. Nobody audits good news, and this was quoted forward five times.

**The fix costs no address space**, which is the part worth inheriting.
`PV_STRIDE_B` is 32 and the record is 24, so every slot already carries **eight
bytes of declared slack**. A v2 record at `x 21 + y 21 + invw 24 + status 8 +
u/w 32 + v/w 32 + r 32 + g 32 + b 32 + alpha 8` = **242 bits = 30.25 bytes**,
fitting the allocated stride with 14 bits spare. `VERT_CAP_B` does not move and
no region in `spec/memory_rules.md` section 5c changes. Narrowing x/y to s21 is
**not** a truncation: `zhao_geom_parambuf`'s `pv_illegal_o` already refuses any
vertex failing `fits_s21`, so s21 is the declared domain.

Directive section 4 authorises the amendment by name and forbids the shortcut:
*"never silently overload a field, truncate a handle, or substitute a convenient
zero."* The open sub-question — alpha at its full 32 bits makes the record 266
bits and does **not** fit — is stated in the entry so it gets decided rather than
discovered.

### 6.2 — Entry I55's paragraph (4) is stale, and RASTERSWAP inherited it

Paragraph (4) says re-architecting GEOM.ARENABIN's storage *"is the next
experiment and it is NOT done here"* and quotes **146,414 registers** as the
cost. **ARENAINFER did it**, in commit `4bb430cf` — the staging banks are
`zhao_dc_sdp_ram` instances and row `zhao_geom_arenabin@arenainfer` reads
**1,010 registers / 291,456 memory bits**.

`4bb430cf` is an **ancestor of RASTERSWAP's own base `0cc81ee5`**, by about four
hours. RASTERSWAP's FINDINGS section 3 nonetheless told the next packet
*"resolve GEOM.ARENABIN's storage inference first (it is the named next
experiment and it is an 87%-of-device problem), then build the back end"*. It
was already resolved before that packet started. **Verified with
`git merge-base --is-ancestor` and both commit dates**, per CLAUDE.md's rule
about reading the two dates before calling a red inherited.

### 6.3 — The binner rows quoted in I55 describe a binner the console does not build

The rows quoted as *"`zhao_geom_binner_v2` mapped alone on the SHIPPING part is
2,109 registers and 191,296 memory bits"* (`@walkswap`, `@giantrefs-shipped`)
carry **no `topParameters`**, so they were mapped at the module default
`METAW = 1157` (`zhao_geom_binner_v2.sv:245`). The console ships **1877** —
`zhao_geom_bin_pipe_v2.sv:306-309` `$fatal`s if it is anything else, and `:73-77`
says so in words: R234 D1 *"widened METAW from 1157 to 1877, which is 29 -> 47
forty-bit slices of the binner's metadata bank"*. So `meta_ram` in the console is
**92,160 bits larger** than the quoted row. The rows are honest measurements of
the **pre-R234** block, quoted as though they described the shipped one.

### 6.4 — The brief's "template" is three things and two of them are not what it says

The brief names `zhao_forge_assemble`'s `u_dq` / `u_rcp` / `pack_attr` as the
template. `u_dq` and `u_rcp` are **module instances**
(`zhao_geom_depthquant_stream`, `zhao_raster_rcp24_v4`); `pack_attr` is a
**function**, not a module. More usefully: the block has **no
`zhao_guard_req_t` port at all** — it is a projector client on a demux arm. It
is a template for the back-end *shape* and never for the *fetch*.

### 6.5 — The handover's SDRAM share counts are wrong in the dangerous direction

`reports/HANDOVER-20260919.md` says the record fetch wants *"a third ENGINE1
requester; `zhao_geom_mem_adapter` carries two."* It carries **ten** (`N=10`,
requesters A–J, all driven). **Its own header line 1 says "NINE" and is stale by
one.** And `u_geom_wshare` — the share `zhao_geom_paramwalk` actually sits on —
is `N=3` with all three driven. **Neither has a free slot.** A new read client
costs an `N`->`N+1` widening plus a re-proof of `zhao_mem_share_n`'s round-robin
bound at the new width. (This is why the vertex arm belongs *inside*
`zhao_geom_paramwalk`, on the socket it already owns — the chosen design avoids
that cost rather than paying it.)

### 6.6 — `spec/memory_rules.md` contradicts itself about 22 MiB

Section 5c line 711 lists `0x06A0_0000..0x07FF_FFFF` as *"reserved / unmapped,
pending evidence"*. Line 903, in the same file, lists the identical range as
`RENDER.ASSET_POOL`, 22 MiB, ENGINE1 **read-only**, live under owner ruling R32.
The second is the true one. This matters because the first reading is exactly
what an architect looking for room to widen a record would find first, and it
would look like 22 MiB of free space.

### 6.7 — `RAM-INFERENCE-SCAN` recommends the fourth killer by name

`reports/synthesis/RAM-INFERENCE-SCAN.txt` prints, as its **remedy**, *"One flat
array per element inside a generate, outer index a genvar."* That is precisely
ARENAINFER's arm 0 — an array declared inside a generate FOR-loop — which
measured **10,386 registers / 0 memory bits** against module scope's
**0 / 10,368** on the shipping part, at one iteration with every other property
held fixed. The scan's advice, followed, produces flip-flops, **and the scan
stays silent about it** because it has no rule for the shape it recommends.
Flattering direction, and it is a tool this tree consults when deciding storage.

### 6.8 — ARENAINFER's "THE KILLER IS THE GENERATE FOR-LOOP AND NOTHING ELSE" is over-broad

`zhao_geom_binner_v2.sv:579-591` declares `meta_ram` **inside a generate
FOR-loop** — the shape ARENAINFER measured as fatal — and it **infers**: the
binner's map row reads 2,109 registers against 191,296 memory bits, which at
`METAW=1157` is consistent only with `meta_ram` in block memory. So there is a
live counterexample in the same tree, on the same device, to a conclusion stated
as universal.

**This packet did not determine what the distinguishing property is and does not
guess.** It records that the universal form of the claim is refuted, and that
the next person diagnosing an inference failure should not stop at "it is in a
generate for-loop". Noting the direction: this one is ALARMING rather than
flattering — it would make someone rewrite storage that is already fine.

---

## 7. WHAT WAS REFUSED

**THE RASTER SWAP ITSELF, for a fourth consecutive packet — and for a NEW
reason that supersedes the old one.**

The standing refusal was *"blocker 2 is 1,749 bits and the swap owes a second
setup and attrpack back end"*. That is two separate claims and they now have
different statuses:

* **The 1,749-bit gap is real** and unchanged.
* **"A second back end closes it" is FALSE.** No back end closes it, because
  three of the six planes depend on 8 fractional bits the arena record throws
  away. The gap is a **schema** gap.

So this packet refuses the swap on a **measured record-width blocker**, and
refuses the *inherited architecture* on a **measured area number** (+40 DSP,
35.7% of the device, 4.4x a bill already refused). Both are BUILD refusals, not
decision refusals: directive section 4 pre-authorises the schema amendment in
terms, and this packet decided the architecture rather than escalating it.

**Also refused, and each for its own reason:**

* **Building the vertex-fetch arm against the v1 record.** It would have been
  real RTL, in an already-composed block, with its decoder present and tied off
  — a tempting, landable increment that three packets have named as the missing
  piece. Refused because the record it would fetch cannot carry the function, so
  the arm would be measured against a circuit already known to be wrong.
* **Wiring `walk_valid_i` to a tile sequencer.** Fourth refusal; see section 3.
* **Half-doing the swap by ORing the walk into the live stream.** Fifth refusal.
* **A console or full-device fit.** Not run. One `-MapOnly` on one block, with
  `-Device` named, was run and is the only synthesis claim this packet makes.
* **Widening `zhao_geom_mem_adapter` or `u_geom_wshare`.** Not needed by the
  chosen design, and it would have cost a round-robin re-proof at a new width.

---

## 8. WHAT I GOT WRONG AND CAUGHT MYSELF

**1. I nearly built the vertex-fetch arm first.** It was my plan for a good
while: the decoder is already instantiated and tied to `'0`, the guard socket
exists, `pub_vert_base_i` is already a port — it reads as the cheap, obviously
correct first increment, and three packets have named it as the missing piece. I
checked what the record actually holds only because I was about to write the
attribute-packing line and wanted the field widths. Had I written the arm first
and checked the widths later, I would have landed real RTL, moved a counter, and
reported progress on a path that cannot work. **The check that saved it was one
grep of a file I already had open** — which is CLAUDE.md's own line about where
the error lives.

**2. My first capacity arithmetic for the sidecar was wrong in the reassuring
direction.** I computed the free space in the PARAMBUF view and was drafting
"fits today, not at the giant" before checking that the console instantiates the
arena at **full R7 capacity** (`MAX_VERTS = 65536`, `MAX_TRIS = 16384`) rather
than at some smaller shipped value. At the real capacity it is 3,072 records
against 16,384 — not close. The softer sentence would also have been the wrong
one.

**3. I quoted 45 DSP for attrsetup before noticing the row had no device.**
`zhao_geom_attrsetup@gz-base` reads 45 DSP and is clean, and it went into the
first draft of the decision record as the price. It carries **no
`measuredDevice` field**, so it inherits a file-level default — quotable as an
order of magnitude, not as a measurement. I replaced it by mapping
`zhao_geom_attrpack` myself, which is the block that would actually be
duplicated, and which reads **36** DSP rather than 45.

**4. A heredoc ate my receipt script.** `PACKET-PROTOCOL.md` warns that heredocs
into `bash -c` fail on quote-heavy text; I used one anyway for a Python script
full of nested quotes and got `unexpected EOF`. No damage — it failed loudly —
but the trap was written down and I walked into it.

**5. `MAP_RC=0` with a 13-byte log.** My backgrounded map wrote an essentially
empty log and reported success. That is the broken-instrument shape exactly. I
did not take it at face value — I checked `zhao_block_fit.json` for the row and
the `.map.rpt` for the numbers, and the row was real — but the RC was not
evidence of anything, and a packet in a hurry would have quoted it.

---

## 9. BRANCH AND COMMITS

**Branch `gz/swapbuild`. Pushed. Never `--force`, never `--force-with-lease`.**

| commit | what |
|---|---|
| `920787a7` | the decision record: the swap is a TIME MULTIPLEX, not a second back end |
| `60e2ef27` | the measurement: a second attrpack back end is +36 DSP, and it had never been mapped |
| (this commit) | entry I55 amended — the recomputability claim refuted, three stale numbers corrected, the schema-v2 path costed; plus these FINDINGS |

### Gate state at the pushed commit

| gate | result |
|---|---|
| `completion_register.py` (bare) | **2** — no higher than when I started (RC 1 normal) |
| `check_console_inventory.py` | OK, self-test PASSED |
| `check_prod_manifest.py` | RC 0 |
| `gen_prod_top.py --check` | fresh |
| `gen_console_board.py --check` | fresh |
| `check_quartus17_syntax.py` | RC 0 |
| `gen_shell_paired_diff.py --check` | fresh |
| `check_case_labels.py` | RC 0 |
| `mutant_copy_drift.py` | run AFTER the commit, per ruling R121 |
| `npm run abi:check` | not run — `spec/commands.zidl` untouched |

**No RTL changed, so no lint, bench or directed test could regress.** The only
`fpga/rtl` edit in this packet is inside a `//` comment block. That is offered as
a structural fact, not as a test result: **I did not re-run the smoke suite and
do not claim its numbers as mine.** The `paramwalk` counters in section 3 and the
swap price in section 4 are **RASTERSWAP's, measured at this same tree**, and are
attributed rather than re-presented.

**`check_quartus17_syntax.py` at RC 0 settles one tool's opinion.** No block went
through `quartus_map` in this packet except `zhao_geom_attrpack`, and that row is
a map, not a fit — it carries no ALMs and no Fmax by construction.

---

## THE ONE-PARAGRAPH HANDOVER

The swap is blocked on a **record**, not a back end, and the record can be fixed
**for free** — the vertex slot already carries eight bytes of unused stride. The
next packet's first commit should be **ProjectedVertex v2**: carry r, g and b at
their full 32 bits, narrow x and y to the s21 that `pv_illegal_o` already
enforces, decide alpha's width explicitly, and refresh the three committed arena
mutants because they are copies. Only then is the vertex-fetch arm worth
building, and it belongs **inside `zhao_geom_paramwalk`** on the ENGINE1 socket
that block already owns — both memory shares are full, and going around them
would cost a round-robin re-proof. The back end it feeds is the **existing**
`u_geom_setup` / `u_geom_attrpack` pair, time-multiplexed into the drain window
where `zhao_geom_binner_v2.sv:818` proves they are idle. The throughput of that
multiplex — `attrpack` is 13 clocks per triangle and would run once per tile
REFERENCE, against the on-chip drain's 4.12 clocks per reference — is the number
that could still refuse the whole design, and it is the first thing to measure
once the schema is real.
