# FINDINGS — POSTGATHER (zhao_post_gather composed; R195 spent)

**Branch gz/postgather, 9 commits, head 769c2553. Register 22 -> 21 merged.**
**I17 did NOT close — bullet (c) did, the HUD half did not.**

> TRANSCRIBED BY THE COORDINATOR — the harness refused this lane a report .md.

---

## 6aa6d3ad

POSTGATHER 1/n: the R195 tag law and the plane store, standalone

Two new blocks, neither in any running fit's closure, both lint-clean under
`verilator --lint-only -Wall` -- which settles one tool's opinion and not
synthesizability; `check_quartus17_syntax.py` runs before this lands.

`zhao_post_gather_tag.sv` is owner ruling R195 as RTL. kGlowKnee 24,
kGlowSlope 0x1C, tint 255/236/224, kGlowMaster 255, not one coefficient
moved, every one of them a PARAMETER with the ratified value as its default
(CLAUDE.md art rule 6: a law buried in a composer is a law nobody can turn a
knob on). It differences bit-exactly against `zref::post::gather`, INCLUDING
the rounding order -- the model computes unit_mul(unit_mul(c, gain), tint),
two roundings in that order, and folding gain with tint first is
algebraically identical and arithmetically different because round-half-up
does not commute across a product. That fold would look right and drift one
LSB on a fraction of inputs.

R195 decision 3 is implemented as written: channels 0b10/0b11 are
UNALLOCATED in the frozen spec, so disp_x/disp_y/ink are ZERO and the
fragment is COUNTED on `reserved_channel_o`. That is the sentence POSTMEAS
read past through a grep window and then retracted (R186); it is a ruling,
not an omission, and the ports exist so the producer is already in place the
day a channel is allocated.

One register stage, and the whole beat moves in it together -- colour, tag,
in-tile position and both tile pulses. CLAUDE.md's metadata-swap chapter read
in the positive direction: there one ungated enable skewed a row from its
generation; here one ungated register is exactly what keeps a beat coherent.
Delaying the data and not the pulses would push each tile's last fragment
into the next tile's bank.

`zhao_post_gather_store.sv` answers the blocker POSTMEAS found and R195 does
NOT address: the flush stream carries no address. `c_index_o` is four bits
"within the tile"; composite reads absolute {view, cx, cy}. The mechanism is
named here, in the block that owns it.

  * THE ORIGIN IS NOT A NEW SIGNAL. `zhao_raster_tile_pipe` already emits
    fb_x_o/fb_y_o ("SURFACE pixel x of this beat") beside fb_addr_o =
    {row,col} in-tile, so the tile origin is (fb_x - addr[3:0], fb_y -
    addr[7:4]) on EVERY beat -- a four-bit subtract, no tile-start handshake,
    and therefore no second thing that can be one cycle out. Entry I17
    proposed latching at a pulse; this is the same number without the pulse.
  * THE ORIGIN PIPELINE IS TWO DEEP and that is the whole correctness
    argument. The gather ping-pongs: when cells are written, their tile is
    already closed and a new one may be streaming. Writing them at the
    CURRENT origin is exactly CLAUDE.md's metadata-swap defect -- right
    cells, wrong address, one tile late, every counter balancing. So
    org_cur_q (loaded by a fragment beat) and org_flush_q (loaded by the same
    pulse that swaps the gather's banks), and writes use the latter.
  * ITS DETECTOR IS WIRED TO TWO OPERANDS THAT DO NOT MOVE TOGETHER, which is
    the question CLAUDE.md says to ask of any checker. `flush_overrun_o`
    differences POST.GATHER's own flush_busy_o (a 16-clock walk) against the
    RASTER's tile cadence (256 pixels). Nothing clocks both.
  * `rdw_collide_o` is the instrument for "one plane is enough". The claim is
    that the raster phase writes and the post phase reads and post_lease
    makes those disjoint; the counter is what checks it instead of a comment
    asserting it. A second plane would be 27 M10K spent on a hazard that
    cannot occur -- but "cannot" is a claim.

ADDRESS MAP, and it is a multiply rather than a concatenation for a measured
reason. A power-of-two row stride needs 128, giving a 128 x 96 plane = 12,288
cells = FIFTY-FOUR M10K against POST.GATHER.md's ceiling of thirty. A 7x7
multiply by the row stride costs a few dozen ALMs and brings the plane to
8,192 cells: 8,192 x 16 = 13 M10K for displacement and 8,192 x 17 = 14 for
glow+ink. TWENTY-SEVEN, under the ceiling. The split into two memories is not
cosmetic -- gd_* and gg_* read DIFFERENT coordinates on the same beat
(composite: the glow plane is addressed by the DISPLACED coordinate), so one
memory would need three ports and infer nothing.

DUO STACKS, it does not tile side by side, and getting that backwards puts
player two's bloom on player one's screen. zhao_post_lease reads one tall
source per frame (rd_h_c = duo_i ? frame_h << 1 : frame_h) and
ZHAO_CANVAS_BYTES_DUO = 2*256*192*2 agrees. Writes are in canvas coordinates
and need no view at all; reads are view-local and add view_rows_i for view 1.

PART A, the separable blur, is NOT in this file and that is declared rather
than left to be found by looking at a frame. POST.COMPOSITE's own header
names the seam -- "a blur module sits between POST.GATHER's plane and that
port, or nothing does" -- and calls it optional. R195 ratified TWO passes as
the law; the module that performs them is owed. It goes in the core's
INCOMPLETE block in the composition commit.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## d3f90937

POSTGATHER 2/n: zhao_post_gather COMPOSED, and the smoke bench found my bug

Entry I17 bullet (c), owner ruling R195. POST.GATHER carries traffic in the
composed console for the first time:

  SMOKE: gather  frags=2560 [untagged=2560 below_knee=0 lit=0 reserved=0]
                 sat=0 clamps=0 cells_flushed=160
  SMOKE: gather  plane written=160 oob=0 commits=1
                 | reads[gd/gg]=[770308 770308] miss[gd/gg]=[0 0]
                 | overrun=0 rdw=0

2,560 fragments against the raster's own `pixels=2560` -- one for one, which
is the number to read first because POST.GATHER has no `ready` and a stalled
beat counted twice would light a cell as though one fragment were many. 160
cells is ten whole tiles (2,560 / 256), so no flush burst was cut short. Zero
out of bounds, so every tile origin landed on the plane. And 770,308 reads
answered from a complete plane with ZERO misses.

=== THE BUG THE BENCH FOUND, WHICH IS THE USEFUL HALF OF THIS COMMIT ===

The first version of that last number was `miss[gd/gg]=[582252 582253]` --
76% of reads answered NOT PRESENT on a plane that was complete the whole
time. I had taken the store's `plane_open_i` from the console frame tick,
reasoning that a new frame's raster begins there. IT DOES NOT. The post pass
runs for 823,547 gpu cycles -- most of a frame -- so the tick fires INSIDE
it, cleared the plane under the compositor's own read, and the machine looked
like a plane that was mostly absent.

The repair is to delete the port. The plane stops being the completed frame's
plane exactly when something OVERWRITES it, and that event is on the store's
own write port. CLAUDE.md rule 3: measure the thing, never a proxy for it --
and one event rather than two that can disagree. `plane_open_i` is gone;
`w_go_c` wins over `plane_commit_i` on the same clock, because a plane being
rebuilt has not got a complete frame in it.

Worth being precise about what caught it, because the flattering reading was
available: a 76% miss rate on a brand-new seam reads like "the bloom plane is
mostly empty, which is what you would expect from a bench with no emissive
geometry". That is wrong -- the plane is fully written every frame including
zeros, by R5's own no-reset-loop rule -- and only the counter said so. This
is CLAUDE.md's "counters see what pictures cannot": the framebuffer comparison
was byte-identical before and after the repair.

=== WHAT IS CONNECTED, END TO END ===

zhao_shell_top_v2 gains a SEVEN-PORT resolved-fragment tap (gth_valid_o,
gth_rgb565_o, gth_tag_o, gth_addr_o, gth_x_o, gth_y_o, gth_last_o) and
`rp_fb_tag_unused` / `rp_fb_addr_unused` are DELETED -- a name ending in
`_unused` that starts being used is worse than either state.

CORRECTION TO ENTRY I17, and it makes the repair bigger rather than smaller,
which is the direction worth checking twice. The entry says "the repair is ONE
8-BIT PORT ... `fb_valid_o`, `fb_rgb565_o`, `fb_x_o`, `fb_y_o` and `fb_last_o`
all leave on the lines around it". Those five leave `zhao_raster_tile_pipe`.
They land on the SHELL-INTERNAL wires `rpx_*` and go to RASTER.FBWRITE.
Nothing resolved left `zhao_shell_top_v2` before this commit. Seven ports, not
one. Still small, and now measured instead of inherited.

`gth_valid_o` is the ACCEPTED beat (`rpx_valid && rpx_ready`), not the raw
valid. R5 forbids POST.GATHER from backpressuring RASTER.RESOLVE, so the gate
has to be on the producer side; and `rpx_ready` already carries the post-phase
interlock, so the same expression keeps the gather silent while the compositor
owns the lease -- the property the single plane rests on, and which
`rdw_collide_o` measures rather than assumes (it reads 0).

In `zhao_console_core.sv`: the tap feeds `u_post_gather_tag` (R195's law),
which feeds `u_post_gather` (R5's accumulator, UNCHANGED -- not one line of it
moved to compose it), which feeds `u_post_gather_store`, which answers
`u_post_composite`'s `gd_*` and `gg_*` directly. FOURTEEN PORTS LEAVE THE
CORE'S EDGE (`post_gd_*`, `post_gg_*`).

THE TILE CADENCE, since it is the part that is easy to get one cycle wrong.
`gth_last_o` marks the 256th pixel; `tile_start` is one clock behind it and
`tile_flush` two, and both ride through the tag law's register stage with the
fragments. So the tile's last fragment is accumulated BEFORE the bank swaps,
and the flush of the closed bank happens after. `cells_flushed % 16 == 0` is
the bench's check on that.

THE TILE ORIGIN needs no new raster signal: the shell publishes the SURFACE
(x, y) of every beat beside its in-tile {row, col}, so the origin is a
four-bit subtract. Off-canvas tiles are declared by a FLAG (`org_ok_i`), not
by a reserved coordinate -- R197's binding constraint, because narrowing a
negative coordinate into seven bits aliases it back INTO the plane, and
forcing cx to 127 would work today only because no mode is 128 cells wide.

Four new smoke assertions, each of which can fail for a different reason: the
gather accumulated nothing; the four tag counters do not PARTITION the stream
(untagged + below_knee + lit + reserved must equal fragments -- one wrong
branch breaks the sum and no counter read alone could say so); a flush burst
was cut short; the plane accepted no cells. Plus the two tripwires quoted at
zero, which the next commit fires deliberately so that quoting their silence
is quoting an instrument that has been seen to work.

The bench's own `post_gd_*` / `post_gg_*` stimulus is deleted: six lines of
zero, which is exactly W10's "an absent output must not look like a zero
result" -- the bench could not tell the two apart either.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 6b1fe2a0

POSTGATHER 3/n: I17 rewritten to the HUD, three registrations, three regens

REGISTER 21 -> 20. `zhao_post_gather` leaves the BUILT-BUT-NOT-CONNECTED list.
I17 stays a tie-off, and its head line now says why in the words the register
prints: "POST.COMPOSITE's HUD (`post_hud_*`) -- BOUNDARY". It used to read
"gather planes and HUD"; the gather planes are gone from that edge.

=== WHAT I17 NOW DECLARES, INCLUDING THE THING NOBODY HAS TO FIND ===

R159's rule is that a tie-off you create must be declared in the same commit,
because the register parses the core's own INCOMPLETE block and cannot see
what is not written down. I created no tie-off -- but I did leave something
undone, and the entry says so at length rather than leaving it to be
discovered by looking at a frame:

  PART A, THE SEPARABLE BLUR, IS NOT BUILT. R195 ratified TWO passes as the
  law. POST.COMPOSITE's own header names the seam exactly -- "THE SEAM IS
  `gg_*`: a blur module sits between POST.GATHER's plane and that port, or
  nothing does" -- and today nothing does, so the glow reaches the compositor
  CELL-QUANTISED and the halo is blocky at quarter resolution.

No port is tied and nothing was narrowed: `gg_*` carries a real accumulated
value on every beat and the blur would refine it, not supply it. It is a
REFINEMENT THE RULING BOUGHT AND NOBODY HAS BUILT -- this repository's
uncashed-cheque shape -- so it is written where the cheque can be read back,
with its price already worked out: 23,040 cell-steps (1.4% of a frame) and one
glow-sized 14-M10K plane to ping-pong against, plus a pass engine between the
raster drain and `post_pass_start`, which is a change to the SHELL's post lease
and a different packet.

AND `completion_register.py` REFUSED MY FIRST WORDING OF THAT PARAGRAPH, which
is worth recording because the tool was right and I was not. I had written "IT
IS NOT A TIE-OFF AND IT IS NOT A NARROWING". The register rejects that phrase
in an entry's BODY:

  "That phrase settles an entry and must therefore be a DECLARATION, not
   prose: put it in the entry's head line, as I9 and I25 do, or reword. An
   entry that can be closed by a sentence about it is not a register."

Exactly the blind spot R159 describes, guarded from the other side. Reworded;
the head line still reads BOUNDARY, because the HUD half still is.

=== WHY I17 CANNOT CLOSE, MEASURED RATHER THAN ASSERTED ===

R195's own text says it "unblocks `zhao_post_gather` and tie-off I17, and
nothing else". On the evidence it unblocks the GATHER HALF of I17, not the
entry. The HUD half needs two things R195 does not touch and neither is a
composition:

  * an OWNER DECISION on 153 of 553 M10K (27.7%) for a frame-resident HUD
    store. Bullet 1 of this entry costs it and records that nobody has put
    that number in front of Fabian -- the entry eliminated on-chip memory by
    naming a TECHNOLOGY ("SDRAM, so I23") rather than producing a NUMBER.
  * the CMD descriptor seam of bullet 2 (`twod_pd_*` / `twod_sd_*`), which is
    I14/I30's gap and not a new one.

=== THREE REGISTRATIONS ARE THREE DIFFERENT ACTS, AND THE GATE PROVED IT ===

CLAUDE.md's build chapter says registering a block in the ledger, the manifest
and the production fit's source list are three separate acts. Adding the two
new modules took exactly three round trips, each caught by a different gate:

  1. `check_prod_manifest`: "UNACCOUNTED: zhao_post_gather_store ... is
     neither counted nor declared absent" -> counted as TOPS in
     design/prod_manifest.yml, for the same reason `zhao_twod_sampler` is:
     the console does not merely intend to have them.
  2. the same gate again: "zhao_prod_top.sv freshness is invalid ... STALE"
     -> regenerated.
  3. and again: "instantiated directly by the generated production top but is
     NOT in zhao_prod_top's source list in design/fit_targets.yml -- the fit
     would die at elaboration" -> added.

TWO NEW LEAF FIT ROWS, and they are separate rows rather than sources of
`zhao_post_gather`'s because they answer separate questions. The LAW is
combinational arithmetic, so its row answers "does R195 cost a DSP block and
does it meet gpu_clk in one stage?". The STORE is memory, so its row answers
"does the plane infer M10K at 27, under POST.GATHER.md's ceiling of 30, or
does it fall back to registers?". One row would average two answers nobody
asked in that form.

`zhao_post_gather`'s REFUSAL IS DELETED from the console_core closure comment
rather than annotated, because that list is a list of what is refused NOW.
What it said was "expanding a tag byte into glow RGB is a colour law invented
in the composer, and resolve's stream is internal besides". THE FIRST HALF WAS
RIGHT, and the answer was to get the law RULED rather than to invent it. The
second half named the wrong stream and I17 withdrew it on 2026-09-20.

=== GATES, ALL GREEN, RUN AFTER THE EDITS ===

check_console_inventory   OK -- 356 declared, 216 elaborated by the core,
                          222 fit sources, every reachable module a source
check_prod_manifest       OK -- 356 modules, 73 tops, 121 inside, 162 excluded
check_quartus17_syntax    OK -- 555 files, no Quartus-17.0-rejected forms
                          (the new blocks' elaboration guards are inside
                          `initial`, because a bare module-scope `if` lints
                          clean and fails quartus_map)
check_case_labels         OK
mutant_copy_drift         OK       mutant_drivers      OK
uncashed_cheques          OK       check_counters      OK
refmodel_liveness         OK       duplicate_functions OK
gen_prod_top --check      OK (73 instances, +2)
gen_console_board --check OK (1,274 core ports re-exported, was 1,278 --
                          fourteen gather ports out, seventeen counters in)
gen_shell_paired_diff     OK (59 shared inputs, 91 outputs compared, 4
                          divergent -- the seven new `gth_*` are V2-ONLY
                          OUTPUTS, which the generator does not compare
                          because only V1 outputs are comparable; the
                          superset property is intact and it checked)
packet_h_tieoff_audit     8 declared, 1 reasoned, 10 by group, 0 SILENT --
                          bit-identical to the baseline

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 6c74cf1e

POSTGATHER 4/n: two directed benches that BUILD AND RUN, and both found something

R60: a directed test must build and run, not merely lint. Both were compiled
and executed (statically linked, per CLAUDE.md -- a dynamically linked test
binary dies with STATUS_ENTRYPOINT_NOT_FOUND and it looks exactly like a test
crash). They also execute the two blocks' elaboration `$fatal` guards, which
`--lint-only` never runs.

  [post_gather_tag_directed]   16 checks passed
    swept 16777216 (tag, rgb565) pairs, 0 mismatch(es);
    the FOLDED law differs from the model on 1221632 of them
    white at full strength -> (68, 63, 60)

  [post_gather_store_directed] 22 checks passed
    POSITIVE CONTROLS FIRED: flush_overrun=1 rdw_collide=1 oob_writes=8

=== THE TAG BENCH CAUGHT ME TWICE, WHICH IS THE POINT OF WRITING IT ===

FIRST: it sampled 4,094 points on a stride, with a header sentence explaining
that the full 2^24 walk would be too slow. I TIMED IT. 2.3 seconds. The sample
was not the cheap option -- it was a guess about cost that nobody had measured,
which is this repository's own recurring shape one level down. It now walks all
16,777,216.

SECOND, and this is the better one. `mismatches == 0` over 16.7 million points
is exactly what CLAUDE.md calls a broken instrument until proven otherwise, so
the same walk scores the RTL against the WRONG law as a positive control -- the
fold this file warns about, `unit_mul(c, unit_mul(gain, tint))` instead of
`unit_mul(unit_mul(c, gain), tint)`, algebraically identical and arithmetically
different because round-half-up does not commute across a product.

THE CONTROL CAME BACK ZERO AND FAILED ITSELF. The reason is specific and would
have been invisible in prose: I scored it on RED, and `kGlowTint[0]` is 255,
and `unit_mul(x, 255) == x` for every x the ramp can produce (the ramp
saturates at 68; the identity only breaks near 255). So the fold IS harmless on
red, and the one channel I picked to demonstrate that rounding order matters
was the one channel on which it does not. Scored on all three: 1,221,632
channel-values differ. The claim in the header is now a measurement, and the
sweep is now known to be able to tell one law from another.

Had I shipped the red-only control, it would have read "positive control
present, passes" while proving nothing -- a checker whose own operands cannot
disagree, which is the failure this repository has a chapter about.

=== THE STORE BENCH, AND A FIRE TEST AGAINST A COPY ===

Nine cases: empty plane answers NOT PRESENT rather than zero (W10); sixteen
cells at origin + {index[3:2], index[1:0]}, each carrying a value derived from
its index so a transposed map fails rather than passing on a uniform fill; the
ORIGIN PIPELINE; Duo stacking; and the three counters.

CASE 3 IS THE ONE THAT MATTERS AND IT IS NOT AN ADDRESS CASE. It streams tile
A, closes it, STARTS STREAMING TILE B, and only then flushes A's cells --
which is what POST.GATHER's ping-pong produces on every tile but the first.
Writing them at the CURRENT origin is CLAUDE.md's metadata-swap defect exactly:
right cells, wrong address, one tile late, every counter balancing.

AND I PROVED THE CASE DISCRIMINATES RATHER THAN ASSERTING IT. A copy of the
store with `org_fl_*` replaced by `org_cur_*` -- one origin register instead of
two -- was built in the scratchpad and the bench run against it:

  FAIL: a tile's cells land at the origin of the tile that CLOSED ...
        expected 0xBEEF, got 0x0
  FAIL: and nothing was written at the streaming tile's origin:
        expected 0x0, got 0xBEEF
  FAIL: an off-canvas tile's whole flush is refused and counted ...
        expected 0x10, got 0x0
  FAIL: nothing landed at the streaming tile's origin either:
        expected 0x0, got 0x3333
  [post_gather_store_directed] 4/22 checks FAILED

Four of twenty-two. The other eighteen PASS on the broken store, which is the
header's claim measured: a one-register implementation passes every other case
in the file. The copy lives in the scratchpad and the production tree was never
edited -- `git diff` on the RTL is empty and the bench re-run against the real
file is 22/22.

NO COMMITTED MUTANT IS OWED FOR ANY OF THIS, and the reason is worth stating
because the rule is easy to over-apply. CLAUDE.md requires a committed mutant
for a guard UNREACHABLE WITH LEGAL STIMULUS -- `wq_overflow_o`'s shape, where
"it can fire" stays an argument forever. Every counter in these two blocks is
reachable at the block's own ports, and case 3's property is a BEHAVIOURAL
assertion of the correct outcome ("the cells hold their tile"), which is
precisely what CLAUDE.md says to write instead of a test that asserts the bug.
A committed copy here would be one more thing to go stale in the flattering
direction, guarded by `mutant_copy_drift` forever, for a control the ordinary
bench already provides.

=== THE TRIPWIRES, AND WHY THE SUMMARY LINE PRINTS CAPTURED VALUES ===

`tb_zhao_console_core_smoke.sv` quotes `flush_overrun_o` and `rdw_collide_o` at
ZERO in the composed console and $fatals otherwise. That is only worth
something once the instruments have been seen to work, so cases 5, 6 and 7 fire
all three deliberately:

  flush_overrun_o  a tile closes while `w_busy_i` is high. Its two operands are
                   POST.GATHER's sixteen-clock flush walk and the RASTER's
                   256-pixel tile cadence -- nothing clocks both, so it can see
                   a TIMING fault and not only a value fault.
  rdw_collide_o    a read and a write name one cell on one clock. This is the
                   instrument for "one plane is enough", i.e. that the raster
                   and post phases never overlap. A second plane would be 27
                   M10K spent on a hazard that cannot occur, and "cannot" is
                   the word that deserves an instrument.
  oob_writes_o     a tile at cell 78 on an 80-cell Storm row: the eight cells
                   past the edge are refused and counted, not wrapped onto the
                   next row.

The bench's closing line prints CAPTURED values rather than live ones, and that
is deliberate: cases 8 and 9 reset the block, so a live read prints zero on all
three and reads exactly like a bench that never fired them. Evidence that has
to be inferred from a bench's structure is evidence nobody reads.

Registered in tests/CMakeLists.txt as `post_gather_tag_directed` and
`post_gather_store_directed`, labels "fast;nightly".

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 48b66657

POSTGATHER 5/n: the phantom ledger counter retired, and a wrapper mutant that failed LOUDLY

=== THE PHANTOM COUNTER IS GONE, AND POSTMEAS'S READING IS CONFIRMED ===

`design/blocks.yml`'s POST.GATHER row declared exactly one counter,
`post_gather_vram_bytes_by_client`, and that string was in NO `.sv` file in the
repository. Packet POSTMEAS found it and read it correctly:

  "A VRAM-bytes-by-client counter cannot belong to a block whose own contract
   says it 'reads no external memory' -- IT DESCRIBES THE PLANE STORE, and the
   ledger has been carrying the unbuilt block's counter on the built block's
   row."

That reading is now confirmed by construction rather than by inference: the
plane store exists (`zhao_post_gather_store.sv`) and reads no external memory
EITHER -- it owns two M10K-inferred planes, 8,192 x 16 and 8,192 x 17. So the
counter that row was standing in for was never going to be written by anybody.
Retired, not implemented.

In its place, the four `zhao_post_gather.sv` actually exports, through
`counter_ports`. `check_counters.py` now resolves all four:

  POST.GATHER  post_gather_fragments        -> fragments_o
  POST.GATHER  post_gather_glow_saturations -> glow_saturations_o
  POST.GATHER  post_gather_disp_clamps      -> disp_clamps_o
  POST.GATHER  post_gather_cells_flushed    -> cells_flushed_o

=== AND A BOUNDARY THAT IS A DECISION, NOT AN OMISSION ===

The composition's other two thirds export THIRTEEN more counters, and they are
NOT declared on this row. `check_counters.py` resolves ONE MODULE PER BLOCK ID
(`module_for_block`), so listing a sibling's port here trades one phantom row
for thirteen -- I tried it and measured exactly that:

  gather_frag_untagged   mapped to frag_untagged_o, which is not a port
  ... x13

Giving the law and the store ledger rows of their own is the correct fix and it
is NOT taken here, because a blocks.yml row wants a CONTRACT and inventing a
contract to satisfy a reporting tool is backwards. So the thirteen are NAMED in
a comment on the row, with the four-counter partition explained, and the reason
they are not catalogued is written down. The next reader inherits a decision
instead of a gap -- which is this repository's own rule about deferrals
(`uncashed_cheques.py`: a note saying "not-yet-adopted" is a deferral written
down and leaves the question open; one saying "superseded" closes it).

Maturity REFERENCE_COMPLETE -> RTL_VERIFIED, with the smoke numbers as its
evidence and with what is NOT measured stated in the same breath: the ~27 M10K
for the plane and the DSP cost of the law's six 8x8 multiplies are ARITHMETIC,
not fits. Each block has its own leaf row in `design/fit_targets.yml` asking
exactly that question.

=== THE WRAPPER MUTANT FAILED IN THE LOUD DIRECTION, ON SCHEDULE ===

`run_console_core_smoke.ps1 -Mutant` went RED:

  %Error: tb_zhao_console_core_smoke.sv:1729:42: Can't find definition of
          variable: 'post_gg_req_v_o'   ... 31 errors

`tests/mutants/zhao_console_core_slot_overflow_mutant.sv` is a WRAPPER whose
PORT LIST is a verbatim copy of the core's, and the core just lost fourteen
ports and gained seventeen. Its own header, written eighteen days ago, says:

  "It cannot go stale SILENTLY: `.*` binds every production port to a wrapper
   port of the same name, so a port gained or lost in production fails to
   elaborate here and says so. That is the opposite of a stale copy, which
   fails by continuing to pass."

THE CLAIM HELD, ON THE FIRST RUN, POINTING AT THE RIGHT FILE. That is worth
recording rather than quietly fixing, because CLAUDE.md's mutant chapter is a
catalogue of the opposite outcome -- thirteen combiner copies and eight
AUX-pipe copies that kept PASSING while the machine underneath them moved two
weeks on. The wrapper shape is the reason, and this is the first time anything
in the tree has demonstrated it rather than asserted it. The header now says so.

Port block regenerated by the recipe the header specifies: the core's module
line through its closing `);`, module renamed, TERR_POOL_SLOTS = 512 the only
substantive edit. 2,634 -> 2,668 lines.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## e555c1e2

POSTGATHER 6/n: the contract said NOT RATIFIED, and that stopped being true yesterday

design/contracts/POST.GATHER.md carried "The tag-to-gather law -- PROPOSED,
owner ruling R37 (2026-09-19), NOT RATIFIED" and the sentence "nothing in
fpga/rtl implements it yet and this section is a proposal until the owner
rules". Owner ruling R195 ratified it on 2026-09-20 and this packet
implemented it on 2026-09-21, so the contract was describing a machine that
no longer exists in the direction that hides work already done.

RATIFIED, unchanged, with R195's reasoning carried into the contract rather
than left in a rulings file -- specifically the part a future reader will
need: "pick cheapest" is NOT "the lowest number in every column". Blur
passes are the real cost axis; knee runs the OTHER WAY (knee 16 is twelve
times the work AND the hazed image this contract's own text describes);
bloom_gain is not a cost at all.

WHAT IS STILL OWED IS NOW NAMED IN THE CONTRACT, not only in the core's
INCOMPLETE block: PART A, THE SEPARABLE BLUR, IS NOT BUILT. R195 ratified TWO
passes; nothing in fpga/rtl performs them; the glow reaches the compositor
CELL-QUANTISED. Priced at 23,040 cell-steps (1.4% of a frame) plus one
glow-sized plane (~14 M10K) and a pass engine in the SHELL's post lease.

THE COUNTERS SECTION IS RECONCILED. Three lists disagreed and no two matched
-- the contract asked for five, the RTL exported four, the ledger declared
one, and that one (post_gather_vram_bytes_by_client) was in no .sv file at
all. POSTMEAS diagnosed it correctly and the diagnosis is now confirmed by
construction: it described the PLANE STORE, which exists now and reads no
external memory either, so it was never going to be written. The three
modules' seventeen are tabulated, the four-counter PARTITION is explained,
and the two TRIPWIRES are labelled as such so nobody quotes their silence
without firing them first. The three counters that are NOT built are said to
be not built, with which is superseded (unknown_tags -> reserved_channel) and
which is simply absent (glow_cells_lit, ink_cells_set).

SYNTHESIS: "Unbuilt" -> "UNFITTED, not unbuilt", and both numbers under it
are declared ARITHMETIC rather than measurement -- which is the distinction
this contract's own M10K paragraph was written about. ~27 M10K for the plane,
and ZERO DSPs is now a CLAIM rather than a consequence: the accumulator is
still adds and clamps, but the law carries six 8x8 unit multiplies and the
store three 7x7 address multiplies. The two new leaf fit rows are what will
say.

AND THE CONTRACT'S OWN GAP IS RECORDED. Its entire statement of the output
seam was one sentence, "Writes out for POST.COMPOSITE", while c_index_o is
four bits within the tile and the compositor reads absolute {view, cx, cy}.
That is the blocker POSTMEAS found and R195 did not address, and a contract
that did not name it is part of why nobody had.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## cd880953

POSTGATHER 7/n: derive the cell-coordinate width instead of writing 7

GTH_CW was a literal 7 with a comment saying "cell-coordinate width, 0..127".
It is right TODAY and the casts around it are provably non-truncating at the
shipped parameters -- I measured rather than assumed:

  POST_LINE_W = 384 -> POST_XW = 9 -> gd_cx_o is [POST_XW-3:0] = 7 bits
  POST_MAX_H  = 240 -> POST_YW = 8 -> gd_cy_o is [POST_YW-3:0] = 6 bits

so `GTH_CW'(gd_cx_w)` is an identity and `GTH_CW'(gd_cy_w)` a zero-extend.

IT IS RIGHT FOR A REASON THAT IS NOT WRITTEN IN THE LITERAL, which is the
problem. POST_LINE_W is a PARAMETER. Raise it past 512 and cx widens to 8
bits, the explicit cast SILENTLY TRUNCATES -- an explicit cast is exactly the
construct that tells Verilator not to warn -- and an aliased cell-x puts the
right of the screen's bloom somewhere on the left. A constant that is correct
because of two other constants, with no expression tying them together, is a
wrong number waiting for someone to move one of them.

  localparam int unsigned GTH_CW =
      ((POST_XW - 2) > (POST_YW - 2)) ? (POST_XW - 2) : (POST_YW - 2);

Taking the wider of the two makes the cast from either port a zero-extend BY
CONSTRUCTION. CLAUDE.md's art rule 6 cuts the other way for a value the owner
should be able to turn -- but this is not one: it is a width IMPLIED by two
parameters, and deriving it is how it stays true when they move.

Re-verified after the change, not before: the plain smoke is green with the
same numbers (frags=2560, cells_flushed=160, written=160, oob=0,
miss[gd/gg]=[0 0]), and gen_prod_top, gen_console_board and
gen_shell_paired_diff all report FRESH.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## b664d7f2

POSTGATHER 8/n: a counter classified on the wrong side of the master gain

frag_below_knee_o read `is_glow_c && (gain_c == 0)`, where `gain_c` is the
ramp AFTER GLOW_MASTER. At the ratified GLOW_MASTER of 255 that is IDENTICAL
to classifying on the ramp itself -- which is exactly why it was worth pinning
down rather than leaving.

unit_mul(1, 1) is 0. So at a small GLOW_MASTER every glow fragment in the
frame would be reported BELOW THE KNEE, and the knee would look wrong to
whoever read the counter. R195 calls GLOW_MASTER "the one the owner is most
likely to want on a slider"; a counter that is right only because of another
knob's CURRENT VALUE is a wrong number waiting for that knob to move. It now
classifies on glow_gain(strength), so it means what its name says whatever the
master is doing.

AND THE BLOCK HEADER STATED ITS OWN COVERAGE WRONG: "over ALL 8,388,608 (tag,
rgb565) pairs". That is 2^23. An 8-bit tag and a 16-bit colour is 24 bits --
16,777,216, which is what the bench sweeps and prints. Corrected in place with
the correction named, because a header that states its own coverage and states
it wrong is a claim nobody re-derives.

RE-VERIFIED AFTER, NOT REASONED ABOUT BEFORE, since the smoke's fragments are
all untagged and therefore CANNOT distinguish the two classifications:

  post_gather_tag_directed        16 checks passed
    swept 16777216 pairs, 0 mismatch(es); the FOLDED law differs on 1221632
  check_quartus17_syntax          555 files, no Quartus-17.0-rejected forms
  run_console_core_smoke.ps1      PASS, frags=2560 cells_flushed=160
                                  written=160 oob=0 miss[gd/gg]=[0 0]
                                  overrun=0 rdw=0 -- identical, as expected

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>


---

## 769c2553

FINDINGS-postgather: zhao_post_gather COMPOSED, register 21 -> 20, I17 half-closed

The harness REFUSED a FINDINGS-*.md from a subagent, exactly as it did for
terrain6, terrain7, terrcomp, terrlaw and postmeas -- the message is
"Subagents should return findings as text, not write report files". Per those
precedents this commit message IS the report; transcribe it to
runs/CLAUDE-RUNS/RUN-20260919-1656-gaps-to-zero/FINDINGS-postgather.md.

Branch gz/postgather from 16a7023986fa on claude/ceiling-architecture-20260912
(head confirmed by git ls-remote, not the stale tracking ref). Own worktree
C:\programmieren\zencrifice\gz-postgather. Never rebased. Eight commits.

=== 0. BASELINE AND FINAL, MEASURED IN MY OWN TREE ===

                            BEFORE                      AFTER
completion_register         21 (9 tie-off + 12 disc)    20 (9 tie-off + 11 disc)
packet_h_tieoff_audit       8 declared, 1 reasoned,     IDENTICAL
                            10 by group, 0 SILENT
core closure modules        (not captured)              216 elaborated by the core
                                                        -- stated one-sided rather
                                                        than invented
fit sources                 219                         222
prod manifest tops          71                          73
core edge port lines        1,275                       1,278 (-14 gather, +17
                                                        counters; MEASURED at
                                                        both revisions, not
                                                        inferred -- my first
                                                        draft had it backwards)

zhao_post_gather LEAVES the BUILT-BUT-NOT-CONNECTED list. I17 REMAINS a
tie-off and its head line is rewritten to name what is actually left.

=== 1. WHAT WAS BUILT ===

zhao_post_gather_tag.sv    R195's tag-to-gather LAW. knee 24, slope 0x1C, tint
                           255/236/224, master 255 -- NOT ONE COEFFICIENT
                           MOVED, and every one is a PARAMETER (art rule 6).
zhao_post_gather.sv        R5's tile-local accumulator. UNCHANGED. Not one
                           line moved to compose it.
zhao_post_gather_store.sv  THE PLANE. Turns a sixteen-cell tile FLUSH into the
                           random access by {view, cx, cy} the compositor has
                           always asked for.

zhao_shell_top_v2 gains a SEVEN-PORT resolved-fragment tap; post_gd_* and
post_gg_* (fourteen ports) leave the core's edge; seventeen counters arrive.

EVIDENCE, tb_zhao_console_core_smoke.sv:

  SMOKE: gather  frags=2560 [untagged=2560 below_knee=0 lit=0 reserved=0]
                 sat=0 clamps=0 cells_flushed=160
  SMOKE: gather  plane written=160 oob=0 commits=1
                 | reads[gd/gg]=[770308 770308] miss[gd/gg]=[0 0]
                 | overrun=0 rdw=0

2,560 fragments against the raster's own pixels=2560 -- ONE FOR ONE, which is
the number to read first: POST.GATHER has no `ready` by law, so a stalled beat
counted twice would light a cell as though one fragment were many. 160 cells is
ten whole tiles. Zero out of bounds. ZERO MISSES on 770,308 reads.

=== 2. THE FLUSH-STREAM ADDRESS -- SOLVED, AND CHEAPER THAN I17 PROPOSED ===

POSTMEAS's fourth blocker was REAL and R195 does not address it: c_index_o is
four bits "within the tile" and composite reads absolute {view, cx, cy}.

IT NEEDS NO NEW RASTER SIGNAL. zhao_raster_tile_pipe already emits fb_x_o/
fb_y_o -- "SURFACE pixel x of this beat" -- beside fb_addr_o = {row,col}. So
the tile origin is (fb_x - addr[3:0], fb_y - addr[7:4]), a four-bit subtract,
valid on EVERY beat. Entry I17 proposed latching it at a tile-start pulse; this
is the same number WITHOUT A SECOND THING THAT CAN BE ONE CYCLE OUT.

THE ORIGIN PIPELINE IS TWO DEEP and that is the whole correctness argument. The
gather ping-pongs, so when cells arrive their tile is already CLOSED and the
next may be streaming. Writing them at the CURRENT origin is CLAUDE.md's
metadata-swap defect exactly -- right cells, wrong address, one tile late,
every counter balancing. Proved by fire test, section 6.

=== 3. WHY I17 CANNOT CLOSE -- MEASURED, NOT ASSERTED ===

R195 says it "unblocks zhao_post_gather and tie-off I17, and nothing else". ON
THE EVIDENCE IT UNBLOCKS THE GATHER HALF OF I17, NOT THE ENTRY. The HUD half
needs two things, neither a composition and neither touched by R195:

  * an OWNER DECISION on 153 of 553 M10K (27.7%) for a frame-resident HUD
    store. I17 bullet 1 costs it and records that nobody has put that number in
    front of Fabian -- the entry eliminated on-chip memory by naming a
    TECHNOLOGY ("SDRAM, so I23") rather than producing a NUMBER.
  * the CMD descriptor seam of bullet 2 (twod_pd_*/twod_sd_*), which is
    I14/I30's gap and not a new one.

The entry's head line now reads "POST.COMPOSITE's HUD (`post_hud_*`) --
BOUNDARY", which is the line the register prints.

=== 4. THE ONE THING OWED, DECLARED IN THREE PLACES ===

PART A, THE SEPARABLE BLUR, IS NOT BUILT. R195 ratified TWO passes.
POST.COMPOSITE's own header names the seam exactly -- "THE SEAM IS `gg_*`: a
blur module sits between POST.GATHER's plane and that port, or nothing does" --
and NOTHING DOES. The glow reaches the compositor CELL-QUANTISED and the halo
is blocky at quarter resolution.

No port is tied and nothing was narrowed: gg_* carries a real accumulated value
and the blur would REFINE it, not supply it. It is A REFINEMENT THE RULING
BOUGHT AND NOBODY HAS BUILT -- the uncashed-cheque shape -- so it is written
into the core's I17 entry, into design/contracts/POST.GATHER.md and into
design/blocks.yml, priced: 23,040 cell-steps (1.4% of a 1,666,666-clock frame)
plus one glow-sized plane (~14 M10K) to ping-pong against, plus a pass engine
between the raster's drain and post_pass_start -- which is a change to the
SHELL's post lease and therefore a different packet.

AND completion_register.py REFUSED MY FIRST WORDING OF THAT PARAGRAPH. I wrote
"IT IS NOT A TIE-OFF AND IT IS NOT A NARROWING"; the register rejects that
phrase in an entry's BODY -- "that phrase settles an entry and must therefore
be a DECLARATION, not prose ... an entry that can be closed by a sentence about
it is not a register". Exactly R159's blind spot guarded from the other side.
Reworded; the head line still reads BOUNDARY.

c_disp_* and c_ink_o read ZERO and that is R195 DECISION 3, not an omission.
The difference from a tie-off is checkable: a tie-off cannot become non-zero
without an edit to the core, and this path does the day stars_and_flares.md
allocates a channel, with a real producer already driving it. POSTMEAS filed
that as a missing owner call and RETRACTED it (R186) after reading the section
instead of a grep window.

=== 5. THREE THINGS MY OWN INSTRUMENTS CAUGHT, ALL UNFLATTERING ===

(a) THE SMOKE BENCH FOUND A LIVE DEFECT IN MY LIFETIME SIGNAL. First run read
miss[gd/gg]=[582252 582253] -- 76% NOT PRESENT on a plane that was complete the
whole time. I had taken plane_open_i from the console frame tick; THE POST PASS
RUNS 823,547 GPU CYCLES, so the tick fires INSIDE it and cleared the plane under
the compositor's own read. The flattering reading was available and wrong ("a
bench with no emissive geometry would have a mostly-empty bloom plane" -- but
the plane is fully written every frame INCLUDING ZEROS, by R5's own
no-reset-loop rule). REPAIR: DELETE THE PORT. The plane opens when something
OVERWRITES it, an event on the store's own write port. One event, not two that
can disagree. The framebuffer comparison was byte-identical before and after --
CLAUDE.md's "counters see what pictures cannot", exactly.

(b) MY POSITIVE CONTROL FAILED ITSELF, FOR A SPECIFIC REASON. The tag bench
sweeps all 2^24 (tag, rgb565) pairs and reports 0 mismatches -- the shape
CLAUDE.md calls a broken instrument until proven otherwise. So the same walk
scores the RTL against the FOLDED law (unit_mul(c, unit_mul(gain, tint))) and
REQUIRES a difference. IT CAME BACK ZERO. I had scored it on RED, and
kGlowTint[0] is 255, and unit_mul(x, 255) == x for every x the ramp can produce
(the ramp saturates at 68; the identity only breaks near 255). THE ONE CHANNEL
I PICKED TO SHOW THAT ROUNDING ORDER MATTERS WAS THE ONE CHANNEL ON WHICH IT
DOES NOT. Scored on all three: 1,221,632 channel-values differ. Shipped
red-only it would have read "positive control present, passes" while proving
nothing -- a checker whose own operands cannot disagree.

(c) THE SWEEP WAS A 4,094-POINT SAMPLE with a header sentence explaining that
the full walk would be too slow. I TIMED IT: 2.3 SECONDS. The sample was not
even the cheap option; it was a guess about cost nobody had measured.

=== 6. A FIRE TEST AGAINST A COPY, NEVER THE LIVE TREE ===

To prove the store bench's case 3 DISCRIMINATES rather than merely passes, a
copy of the store with org_fl_* replaced by org_cur_* -- one origin register
instead of two -- was built IN THE SCRATCHPAD and the bench run against it:

  FAIL: a tile's cells land at the origin of the tile that CLOSED ...
        expected 0xBEEF, got 0x0
  FAIL: and nothing was written at the streaming tile's origin:
        expected 0x0, got 0xBEEF
  FAIL: an off-canvas tile's whole flush is refused and counted:
        expected 0x10, got 0x0
  FAIL: nothing landed at the streaming tile's origin either:
        expected 0x0, got 0x3333
  [post_gather_store_directed] 4/22 checks FAILED

FOUR OF TWENTY-TWO. The other eighteen PASS on the broken store, which is the
bench header's claim measured rather than asserted. git diff on the production
RTL was empty throughout and the re-run against the real file is 22/22.

=== 7. COUNTERS -- EVERY NEW ONE PROVEN TO FIRE, AND NO MUTANT IS OWED ===

THE FOUR TAG COUNTERS ARE A PARTITION -- every accepted fragment lands in
exactly one of untagged / below-knee / lit / reserved-channel -- and the smoke
bench asserts the SUM rather than the parts. One wrong branch breaks it and no
counter read alone could say so.

  frag_untagged_o      tag 0x00
  frag_below_knee_o    GLOW at and below the knee
  frag_lit_o           GLOW above the knee
  reserved_channel_o   BOTH unallocated channels (R195 decision 3's instrument)
  oob_writes_o         a tile at cell 78 on an 80-cell Storm row: 8 refused
  flush_overrun_o      a tile closing while w_busy_i is high
  rdw_collide_o        a read and a write naming one cell on one clock

NO COMMITTED MUTANT IS OWED, and the reason is stated rather than assumed
because the rule is easy to over-apply. CLAUDE.md requires one for a guard
UNREACHABLE WITH LEGAL STIMULUS -- wq_overflow_o's shape, where "it can fire"
stays an argument forever. Every counter here is reachable at its block's own
ports, and the behavioural cases assert the CORRECT outcome rather than the
bug. A committed copy would be one more thing to go stale in the flattering
direction, guarded by mutant_copy_drift forever, for a control the ordinary
bench already provides.

flush_overrun_o's TWO OPERANDS DO NOT MOVE TOGETHER, which is the question
CLAUDE.md says to ask of any checker: POST.GATHER's sixteen-clock flush walk
against the RASTER's 256-pixel tile cadence. Nothing clocks both, so it can see
a TIMING fault and not only a value fault.

rdw_collide_o is the instrument for a claim I would otherwise have made in a
comment -- that ONE PLANE IS ENOUGH because the raster and post phases never
overlap. A second plane would be 27 M10K spent on a hazard that cannot occur,
and "cannot" is the word that deserves an instrument.

The store bench prints all three as CAPTURED values, deliberately: later cases
reset the block, so a live read prints zero on all three and reads exactly like
a bench that never fired them.

=== 8. THE WRAPPER MUTANT FAILED IN THE LOUD DIRECTION, ON SCHEDULE ===

run_console_core_smoke.ps1 -Mutant went RED with 31 "Can't find definition of
variable" errors naming the removed ports.
tests/mutants/zhao_console_core_slot_overflow_mutant.sv is a WRAPPER whose port
list is a verbatim copy of the core's, and its own header, written eighteen days
earlier, says: "It cannot go stale SILENTLY ... That is the opposite of a stale
copy, which fails by continuing to pass."

THE CLAIM HELD, ON THE FIRST RUN, POINTING AT THE RIGHT FILE. Worth recording
rather than quietly fixing, because CLAUDE.md's mutant chapter is a catalogue of
the opposite outcome -- thirteen combiner copies and eight AUX-pipe copies that
kept PASSING while the machine underneath them moved two weeks on. THIS IS THE
FIRST TIME ANYTHING IN THE TREE HAS DEMONSTRATED THE WRAPPER PROPERTY RATHER
THAN ASSERTED IT. Regenerated by the recipe its header specifies (2,634 ->
2,668 port-block lines, TERR_POOL_SLOTS = 512 the only substantive edit); the
header now records the test.

=== 9. CORRECTIONS TO INHERITED CITATIONS ===

I17's "THE REPAIR IS ONE 8-BIT PORT" IS WRONG IN THE SMALL DIRECTION, which is
the direction to check twice. The entry says fb_valid_o, fb_rgb565_o, fb_x_o,
fb_y_o and fb_last_o "all leave on the lines around it". THEY LEAVE
zhao_raster_tile_pipe. They land on the shell-INTERNAL wires rpx_* and go to
RASTER.FBWRITE; NOTHING RESOLVED LEFT zhao_shell_top_v2 AT ALL. Seven ports,
not one. Still small, and now measured instead of inherited.

THE PLANE COSTS 27 M10K, against I17's estimate of 30 for the same object --
8,192 cells as 8,192x16 (displacement) + 8,192x17 (glow+ink). The saving is an
address MULTIPLY instead of a concatenation: a power-of-two row stride needs
128, giving a 128x96 = 12,288-cell plane and FIFTY-FOUR M10K. ARITHMETIC, NOT A
FIT -- and each of the two new blocks has its own leaf fit row asking exactly
that question, separately, because the law's question (does R195 cost a DSP
block?) and the plane's (does it infer M10K?) are not the same question.

POSTMEAS'S LEDGER READING IS CONFIRMED BY CONSTRUCTION.
post_gather_vram_bytes_by_client described the PLANE STORE; the store exists now
and reads no external memory either, so that counter was never going to be
written by anybody. Retired, with the four zhao_post_gather.sv really exports
declared in its place through counter_ports. The sibling modules' thirteen are
NAMED in a comment rather than declared, because check_counters.py resolves ONE
MODULE PER BLOCK ID and declaring them trades one phantom row for thirteen -- I
tried it and measured exactly that. Giving the two new modules ledger rows of
their own is the correct fix and is NOT taken here: they would need contracts,
and inventing a contract to satisfy a reporting tool is backwards.

R65 IS NOT LOAD-BEARING HERE -- re-verified rather than inherited (R165): zero
citations in the contract or the RTL. POSTMEAS was right.

=== 10. ONE MORE THING THE CONTRACT ITSELF GOT WRONG ===

design/contracts/POST.GATHER.md still carried "The tag-to-gather law --
PROPOSED, owner ruling R37, NOT RATIFIED" and "nothing in fpga/rtl implements it
yet". R195 ratified it the day before and this packet implemented it. Updated,
with R195's reasoning carried INTO the contract rather than left in a rulings
file -- specifically the part a future reader needs: "pick cheapest" is NOT
"the lowest number in every column".

Its counters section is reconciled: three lists disagreed and no two matched.
Its synthesis section reads "UNFITTED, not unbuilt", and both numbers under it
are declared ARITHMETIC rather than measurement -- ZERO DSPs is now a CLAIM,
because the law carries six 8x8 unit multiplies and the store three 7x7 address
multiplies, and the leaf fits are what will say.

AND THE CONTRACT'S OWN GAP IS RECORDED. Its entire statement of the output seam
was one sentence, "Writes out for POST.COMPOSITE". That is the blocker POSTMEAS
found, and a contract that did not name it is part of why nobody had.

=== 11. GATES -- ALL GREEN, RUN AFTER THE COMMITS ===

check_console_inventory.py     OK  356 declared, 216 elaborated by the core
                                   (the tool's own figure, which already
                                   includes the two new blocks), 222 fit
                                   sources, every reachable module a source,
                                   and THE LATEST VERSION IS THE ONE WIRED --
                                   the superseded check, which matters here
                                   because the shell uses
                                   zhao_raster_tile_pipe_V2, not the v1 block
                                   whose ports I first traced (section 13)
check_prod_manifest.py         OK  356 modules, 73 tops, 121 inside, 162 excluded
check_quartus17_syntax.py      OK  555 files, no Quartus-17.0-rejected forms
                                   (both new blocks' elaboration guards are
                                   inside `initial`, because a bare
                                   module-scope `if` lints clean and fails
                                   quartus_map)
check_case_labels.py           OK
mutant_copy_drift.py           OK
mutant_drivers.py              OK
uncashed_cheques.py            OK
check_counters.py              OK  all four POST.GATHER rows resolve to real
                                   ports; the phantom is retired
refmodel_liveness.py           OK
duplicate_functions.py         OK
gen_prod_top.py --check        OK  fresh, 73 instances
gen_console_board.py --check   OK  fresh, 1,274 core ports re-exported
gen_shell_paired_diff --check  OK  fresh, 59 shared inputs, 91 compared
                                   outputs, 4 divergent. The seven new gth_*
                                   are V2-ONLY OUTPUTS, which the generator
                                   does not compare because only V1 outputs
                                   are comparable; the superset property is
                                   intact and it checked.
completion_register.py         21 -> 20
packet_h_tieoff_audit          8 declared, 1 reasoned, 10 by group, 0 SILENT
                               -- bit-identical to the baseline

SIX SMOKE FORMS, all via `powershell -File`:
  (no flag)      PASS   the gather line above
  -LintOnly      PASS   elaborates
  -Mutant        PASS   after regenerating the wrapper's port block. It went
                        RED first, which is section 8 and is the GOOD outcome,
                        and the inverted-polarity control still fires:
                        "terr_pl_slot_overflow_o=1 ... The detector works;
                        production's zero is a measurement."
  -BadVertex     PASS
  -NoEchoArm     PASS   gather carries (reads 576,368, miss 0)
  -BadTraceArm   PASS

TWO NEW DIRECTED BENCHES, BUILT AND RUN (R60), statically linked:
  post_gather_tag_directed     16 checks passed
  post_gather_store_directed   22 checks passed

=== 12. WHAT I DID NOT DO, SO NOBODY INHERITS IT AS DONE ===

  * NO FIT WAS RUN. The ~27 M10K and the DSP question are arithmetic. Two leaf
    rows exist in design/fit_targets.yml asking them; per the subsystem-boundary
    rule this belongs in the compositor's next batched fit, not in this packet.
  * PART A (the blur) is not built. Section 4.
  * The HUD half of I17 is not closed. Section 3.
  * zhao_post_gather_tag and zhao_post_gather_store have no blocks.yml rows of
    their own, and therefore no contracts. Named in POST.GATHER's row with the
    reason.
  * THE COMPOSED CONSOLE HAS NEVER CARRIED A LIT FRAGMENT, and section 13 is
    the whole of why. The law is verified exhaustively at block level and the
    seam is verified in the console; no test in this tree does both at once,
    and a green smoke on an untagged stream is not evidence about bloom.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>

=== 13. WHERE THE EFFECT TAG COMES FROM, CHASED BECAUSE THE NUMBER LOOKED ODD ===

The smoke reads `untagged=2560`, i.e. EVERY resolved fragment carries tag 0. I
nearly reported that as "the bench simply does not set a tag" and checked
instead, because the bench's `render_fill_word_i` is 0xA5A5_A5A5_A5A5_A5A5 and
byte [39:32] of that is 0xA5 -- channel 0b10, a RESERVED channel, which should
have made `reserved_channel_o` read 2,560 rather than zero. A counter that
disagrees with the stimulus is either a broken counter or a wrong model of the
path, and both are worth five minutes.

IT WAS A WRONG MODEL OF THE PATH, AND THE CORRECTION MATTERS.
`render_fill_word_i[39:32]` is the V1 `zhao_raster_tile_pipe`'s constant-tag
source. `zhao_shell_top_v2` does NOT use that block: it instantiates
`zhao_geom_bin_pipe_v2` -> `zhao_raster_tile_pipe_v2` ->
`zhao_raster_texture_stage_v3`, where the tag is

    frag_tag_o = retire_ctx.raster_continuation.post_earlyz.effect_tag

and that field is bits [15:8] of the 48-bit RASTER CONTINUATION TAIL, which
enters `zhao_console_core` as **`tri_continuation_tail_i` -- A BOUNDARY PORT
WITH NO PRODUCER IN THIS CONSOLE.** Entry I20 records it, the smoke bench's own
NOTE says it in as many words ("tri_continuation_tail_i and tri_fragment_state_i
are still boundary ports"), and the bench drives it `'0`.

SO `untagged=2560` IS CORRECT AND EXPECTED. It is not a defect in this packet's
wiring and not something this packet created: POST.GATHER is composed onto a
stream whose TAG FIELD is I20's gap, one port upstream of everything I17 was
about.

AND THE BENCH CANNOT BE MADE TO CARRY A LIT FRAGMENT WITHOUT BREAKING ITS OWN
PREMISE. I costed it rather than guessing: setting `tri_continuation_tail_i
[15:8]` to a GLOW tag would light the plane, and `bloom_gain` is 0x5A (not
zero), so the bloom would change pixels -- and the bench asserts the post pass
is an IDENTITY with a hard $fatal:

    if (fb_bad != 0)
      $fatal(1, "SMOKE: the framebuffer after the IDENTITY post pass differs
                 from the raster's frame in %0d word(s) ...");

That assertion is the post lane's own evidence (I15/I16), so re-authoring it is
a decision about THAT lane, not a tweak this packet may make on the way past.
NOT DONE, and named so the next packet inherits a costed option instead of a
blank: the honest shape is a new smoke FORM (`-GlowTag`, beside `-BadVertex`
and `-NoEchoArm`) that sets the tag, expects `fb_bad != 0`, and asserts
`gather_frag_lit_o > 0` with a non-zero glow in the plane. That would be the
first end-to-end evidence that R195's law runs in the composed machine rather
than only on a bench.

=== 14. TWO LAST PRECISION FIXES, BOTH IN A COUNTER'S MEANING ===

frag_below_knee_o WAS CLASSIFIED ON THE WRONG SIDE OF THE MASTER GAIN. It read
`is_glow_c && (gain_c == 0)`, where `gain_c` is the ramp AFTER GLOW_MASTER. At
the ratified GLOW_MASTER of 255 that is identical to classifying on the ramp
itself -- which is exactly why it was worth pinning down. unit_mul(1, 1) is 0,
so at a small GLOW_MASTER every glow fragment in the frame would be reported
BELOW THE KNEE and the knee would look wrong to whoever read the counter. It
now classifies on `glow_gain(strength)`, so the counter means what its name
says whatever the master is doing. R195 calls GLOW_MASTER "the one the owner is
most likely to want on a slider"; a counter that is right only because of
another knob's current value is a wrong number waiting for that knob to move.

AND THE TAG BLOCK'S HEADER STATED ITS OWN COVERAGE WRONG: "over ALL 8,388,608
(tag, rgb565) pairs". That is 2^23. An 8-bit tag and a 16-bit colour is 24
bits -- 16,777,216, which is what the bench prints and what it sweeps.
Corrected rather than quietly fixed, because a header that states its own
coverage and states it wrong is a claim nobody re-derives.


---

