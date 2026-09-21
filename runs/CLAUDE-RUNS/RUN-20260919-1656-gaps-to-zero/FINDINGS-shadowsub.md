<!-- TRANSCRIBED FROM COMMIT MESSAGES, 2026-09-21.

The harness refuses a lane-written FINDINGS file, so this packet landed its
record as commit message(s). The text below is REPRODUCED VERBATIM from
89cccdf7, 9555a86d -- it is the lane's own words, not the
coordinator's summary of them. Cite this file; the commits remain the primary.
-->

# FINDINGS -- SHADOWSUB


## `89cccdf7` -- FINDINGS -- SHADOWSUB (the FORGE.SHADOW subsystem, commissioned by R234 D2)

```
FINDINGS -- SHADOWSUB (the FORGE.SHADOW subsystem, commissioned by R234 D2)

Branch gz/shadowsub, three commits. Register 22 -> 22.
No RTL changed. No tie-off created, removed or relocated.

THE HEADLINE: I did not compose the subsystem, and the reason is that the
blocker list it was scheduled against was wrong in FOUR places -- two blockers
that are not blockers, one that is not what it was called, and two nobody had
measured. Composing against the old list would have built the wrong things in
the wrong order.

=========================================================================
1. THE FIVE BLOCKERS: ALL FIVE STAND, RE-MEASURED BY INSTANTIATION
=========================================================================
At fe1d3ca0, searching every .sv under fpga/rtl for an instantiation at
statement position, excluding each module's own file and every comment:

  zhao_geom_ladderbank   0
  zhao_geom_lodstate     0
  zhao_view_projscale    0
  zhao_geom_projradius   1   zhao_geom_lodstate.sv only, itself uncomposed
  zhao_measure_governor  1   zhao_prod_top.sv only, the GENERATED pricing top
  zhao_forge_shadow      1   likewise

The brief's warning held: the raw count says three are instantiated and that
reading is false. Verified first-hand, not inherited.

TWO MORE uncomposed blocks belong to this chain and were not on anyone's list:
zhao_view_projq88 (0 instantiations; it is the governor's proj0/1_i producer
and zhao_view_projscale's only other consumer) and zhao_measure_starve (0; it
is starved0/1_i's producer). So the chain is SEVEN blocks, not five.

=========================================================================
2. THE RATIFIED LAW -- SURFACED, NOT DECIDED. And it is narrower than stated
=========================================================================
THE LAW IS R3 (owner, explicit), one of the seven only the owner can lift:

  "Third projector port for particles / FORGE.SHADOW (I24). Keep the
   time-multiplex. No third port in v1. Owed: a written schedule proof that
   geometry, particles and FORGE.SHADOW's instance-centre 1/w share client A's
   bandwidth within the frame at the guaranteed content tier."

WHAT WOULD WIDENING IT CHANGE? For the instance centre: NOTHING, because the
widening has already been performed and R3 sanctions it.

  * R3 does not forbid this subsystem's use of client A -- it NAMES
    FORGE.SHADOW's instance-centre 1/w as one of the three sharers. What it
    withholds is a third PORT on zhao_project_service, not a third CLIENT.
  * zhao_part_project went PAY_W 16 -> 17 and a one-bit TAG_BIT -> a two-bit
    OWNER field under R68 sub-build 4, expressly and in writing for
    zhao_geom_lodstate's instance centre. Verified in the RTL: OWNER_W = 2,
    OWNER_GEOM = 2'd0, OWNER_PART = 2'd1, 2'd2 and 2'd3 UNALLOCATED, with
    owner_unroutable_o counting a result carrying one and geom_tag_collision_o
    counting a geometry rider that arrives with an owner bit set. The console
    mirrors it: GEOM_PAY_A_W = 17, GEOM_OWNER_W_C = 2, and a live initial-block
    elaboration guard requiring arena 3 + index 12 <= 17 - 2.
  * The block's own header states the intent: "this port has a third coming:
    zhao_geom_lodstate projects the INSTANCE CENTRE through the same client A
    ... Owner ruling R3 keeps client A a time-multiplex, so the third owner is
    a third arm here and not a second projector. The encoding is sized for it
    NOW, because the failure mode of sizing it later is silent."

So R133's "a client-A widening that re-authors a ratified law" -- the sentence
that made this a subsystem rather than a build -- IS NOT TRUE OF THE PIECE THE
SUBSYSTEM NEEDS. No owner decision is required. What is required is a third
request ARM on zhao_part_project (it has exactly two today, g_* and p_*), and
the written schedule proof R3 OWES AND NOBODY HAS PRODUCED.

WHAT WOULD genuinely re-author a law is narrower and different: an ARENA-FILL
PATH ON CLIENT A'S RESULT PORT, if Route B's shadow-hull vertices are to get an
arena of their own. Client B has one (fill_landed_o / fill_arena_o into
zhao_terrain_wcache); client A does not, and zhao_geom_proj_lane cannot refuse
a result -- there is no a_ready_o on that side. That is a real design question.
It is not the one R133 named and it is not blocked by R3.

THE PROOF R3 OWES HAS TWO HALVES AND ONLY ONE IS MEASURABLE TODAY. The RATE
half (does the third client's per-frame demand fit client A's frame budget at
the guaranteed tier) is answerable now from per-client demand against the
service's measured throughput. The FAIRNESS half (does round-robin at three
starve anyone; does lodstate's single-in-flight FSM still close its 200-clock
evaluation under contention) CANNOT be measured without the arm, so it is owed
at the commit that adds it. Both are recorded in the contract.

RECOMMENDATION: no owner decision is needed to proceed. Direct the next lane to
discharge the rate half, then build the arm and the fairness half together.

=========================================================================
3. tap_* IS NOT CLEAR -- the one port group the table marked clear
=========================================================================
FORGE.SHADOW.md's blocker table: "NONE -- this one is genuinely clear.
zhao_terrain_heighttap mirrors these ports signal for signal and is already
composed." Both halves of that sentence are true and the conclusion does not
follow.

zhao_terrain_heighttap has EXACTLY ONE requester port group -- req_valid_i,
req_ready_o, req_x_i, req_z_i, req_surface_i -- and in zhao_console_core.sv
every one is connected to htp_req_*, driven by u_part_terrain_tap's
tap_req_*_o. There is no second port and no arbitration. Worse for a second
client: the response carries NO TAG AND NO RIDER (rsp_valid_o plus eighteen
data outputs, nothing identifying whose request they answer), so an arbiter in
front of req_* must hold the outstanding owner itself -- the u_terrain_rdshare
shape -- and no such arbiter exists.

A claim about PORT SHAPE was read as a claim about PORT AVAILABILITY. Same
class as R180's "upstream: is design intent, not a wiring claim".

AND THE CORE ALREADY HALF-KNEW. Above u_part_terrain_tap it declares
htp_height/nx/ny/nz under lint_off UNUSEDSIGNAL and says "FORGE.SHADOW, the
point answer's other customer, is not composed (its own blocker, the creature
rung, is in the FORGE.SHADOW header)" -- naming FORGE.SHADOW as the customer,
naming a DIFFERENT blocker, and never looking at the requester port. The point
answer is live silicon nothing reads: R228's shape, found by asking who READS
this rather than whether it EXISTS. The checkmark is why nobody measured it.

=========================================================================
4. A BLOCKER THAT HAS EXPIRED: R197's untextured door is BUILT AND COMPOSED
=========================================================================
Route A and Route B were both recorded as blocked by an attribute wall a shadow
hull cannot climb, having no u/v by law. R197 sanctioned the declared-untextured
profile and the mechanism has since been built, composed and given a fired
positive control:

  * zhao_geom_clip carries tri_untex_i through three stages to out_untex_o;
    zhao_geom_attrpack branches on tri_untex_i.
  * zhao_console_core.sv implements R197 law 3 at GEOM.CLIP's input door --
    cl_in_untex_c, cl_in_refuse_c, geom_untex_refused_o -- with the producer's
    declaration on the named seam GEOM_REPLAY_UNTEX_DECL.
  * tests/mutants/zhao_console_core_untex_decl_mutant.sv is the committed
    positive control, inverted polarity, driven by -UntexMutant, because no
    legal stimulus can move that counter.

And the core states the intent unprompted: "the arbiter that eventually admits
particles and shadow hulls will present its untex bit to THIS gate, not to a
second copy of it downstream."

So the u/v half of the vertex-consumer blocker is DISCHARGED and the door is
already the right shape. What remains for vtx_* is the ARBITER at that door and
the MATERIAL-WINDOW SPAN: cl_in_refuse_c refuses an untextured primitive
whenever mw_pub_sample_count != 0, so a shadow hull must arrive under a
published zero-sample material, and R187's three-way ordered join is what
decides that.

ROUTE A'S RECORDED CAUSE IS ALSO WRONG ON THE MERITS. The contract says it
deadlocks on a six-term AND "requiring a lit rgb and a u/v that a shadow hull
has neither of". done_o's binding term is (lit_ord_q == uv_ord_q) -- a COUNT
EQUALITY, which a hull supplying neither satisfies trivially. Route A's actual
obstacle is that zhao_geom_vattr is fed by the meshlet batch protocol (batch_i,
op_valid_i arenas, decoded uv_valid_i/lit_valid_i) that a shadow hull has no
producer for. Route B remains the route; the reason recorded for preferring it
was not the reason. (The citation was wrong too: ":490" is a comment banner,
done_o is :553. R133 recorded that correction and the table was never updated.)

I flag per R229 that this section is my flattering direction -- a discovery --
so it is the claim I checked hardest: every file and symbol above was read, not
grepped for a mention.

=========================================================================
5. TWO BLOCKERS NOBODY HAD MEASURED, on the COMPOSED DRAWJOB job seam
=========================================================================
zhao_geom_lodstate taps the DRAWJOB -> MESHFETCH handshake for
{j_instance_id_i, j_form_index_i[23:0], j_cx/cy/cz_i, j_view_i}. The handshake
is composed and carries NEITHER of the last two.

  (a) NO FORM INDEX. zhao_geom_drawjob emits j_desc_addr_o [26:0],
      j_format_o [7:0] (the VERTEX format, not the form), j_generation_o and
      j_stream_base_o. zhao_geom_ladderbank's key is the MESH_STREAM handle
      index -- the value arriving on upl_publish_index_o [23:0] that DRAWJOB
      writes into its own residency directory -- and the job does not carry it
      out. Closing this is a new output on a composed block: its whole
      instantiation chain plus every bench.

  (b) NO VIEW INDEX, AND THIS ONE IS AN OWNER DECISION. DRAWJOB emits
      j_active_mask_o [1:0], a two-view MASK. lodstate's j_view_i is a single
      bit selecting which camera's threshold the instance's ONE ladder is
      measured against. For active_mask == 2'b11 there is no honest answer in
      the tree. zhao_geom_lodstate's own header already docks it: "ONE LodState
      PER INSTANCE, NOT PER CAMERA -- AND THAT IS A DEVIATION ... the
      disagreement is REPORTED rather than resolved here. It is an owner
      decision, and the cost of changing it is one more index bit on the
      store." Composition is where it stops being reportable: something must
      choose, and choosing silently is inventing.

  OWNER DECISION DOCKED: for a creature drawn into both views, does its single
  ladder measure against view 0's threshold, or does the store gain a camera
  index bit (one more bit x INSTANCES)? The reference (zref::creature) holds ONE
  LodState and takes ONE threshold; PART.LADDER's 2026-08-31 2.5 ruling makes
  the sibling particle selection PER CAMERA. The two disagree and nothing
  ratifies either for creatures. Recommendation: pay the bit. The cost is
  small, the reference's silence is not a ruling, and the failure mode of
  picking view 0 is a creature that pops rungs in the second view for reasons
  nothing records.

=========================================================================
6. THE GOVERNOR: R223's TRADE HAS NOT MOVED, and one deferral HAS expired
=========================================================================
D2 unparks the governor, and its TERRAIN.LOD output group still has no
consumer. cam0/1_scale_o, targets_valid_o, cam*_en_o, hyst_o, min_hold_o,
morph_step_o and src_id_o go to zhao_terrain_lod, which is uncomposed.
zhao_terrain_lodfeed IS composed and is NOT it -- checked, because the name
invites the error: it is the deviation feed and consumes no governor output.
So composing the governor today still closes one gap by opening several, which
is R75's refusal and R223 item 4.

Its upstream is also still short: zhao_cmd_exec.sv says in terms "NOT
pixel_error -- MEASURE.GOVERNOR is not composed", so SetView.pixel_error and
SetPresentationContract.view_count have no executor arms. Both fields exist in
the ABI (zhao_abi_pkg.sv), so this is an executor arm, not an ABI change.

BUT ONE DEFERRAL'S STATED CAUSE HAS EXPIRED, and the CMD.EXEC lane should know.
zhao_console_core.sv's I14 entry defers those two arms on exactly this ground:
"MEASURE.GOVERNOR is PARKED, not merely uncomposed ... building the two CMD.EXEC
arms today would be an uncashed cheque written against a standing ruling."
R234 D2 unparks it. The cited cause is gone; the work is now ordinary.

=========================================================================
7. LADDERBANK IS A SIXTH REQUESTER, AND THE WRAPPER IS FIXED AT FIVE
=========================================================================
zhao_geom_mem_adapter takes NO parameters. It is a fixed A-E wrapper --
s_req[0..4] and an inner zhao_mem_share_n with .N(5), .CLIENT_ID(3) and
.FORCE_READ(1) -- with the five slots held by MESHFETCH, ASSETFETCH,
MATERIAL.RESOLVE, DRAWJOB and PART.TABLE.LOADER. A sixth is f_* ports, an
s_req[5] assign and .N(6); the round-robin bound is N-1 turns and must be
RE-PROVED at six, not assumed.

Easier than the entry implies: upl_publish_valid_o/_tag_o/_base_o/_extent_o are
already core outputs and already feed u_part_table_loader in exactly the
four-signal shape LADDERBANK declares. The kind-dispatch pattern to copy is the
port-level valid AND (tag == KIND) used by DRAWJOB's directory and
MATERIAL.RESOLVE. LADDERBANK's PAGE_KIND is 8'd8, CREATURE_FORM.

=========================================================================
8. WHY NOTHING WAS COMPOSED -- the refusal, with its measurement
=========================================================================
NOTHING IN THIS CHAIN COMPOSES ALONE. Every link's outputs terminate on the
next link, so composing any prefix dangles an edge: projscale's kx/vw dangle
without lodstate and projq88; projq88's proj0/1_o dangle without the governor;
the governor's thresh/deg dangle without lodstate and forge_shadow; lodstate's
c_* dangle without forge_shadow; forge_shadow's vtx_* dangle without the
GEOM.CLIP-door arbiter. It is ONE COMMIT OR NONE, and the terminal link is the
arbiter plus the material span, which is a build and not a wiring job.

I could have composed a prefix and reported a smaller-looking diff. That is
tie-off relocation -- the campaign's first prohibition and, as R133 noted when
it declined the same move, the flattering one.

THE BUILD ORDER for the next lane is recorded IN THE CONTRACT, not here, because
a run folder is the wrong home for anything durable (CLAUDE.md). Summary:
(1) R3's rate proof; (2) projscale; (3) projq88; (4) starve; (5) the two
CMD.EXEC arms, now un-deferred; (6) TERRAIN.LOD, or accept the governor's
dangling group as a DECISION; (7) the two DRAWJOB seam ports, one of them an
owner decision; (8) adapter at six, then ladderbank; (9) the third client-A arm
plus the fairness proof, then lodstate; (10) a heighttap arbiter, then
forge_shadow with cast_strength_i from a named constant (R133's
D-FORGESHADOW-A, accepted, still the right treatment); (11) the GEOM.CLIP-door
arbiter and the shadow material span.

Steps 6 and 7b are DECISIONS, not builds.

=========================================================================
9. R133 AND R223, ON THE MERITS
=========================================================================
R133 IS SPENT AND WAS RIGHT: a wiring job cannot close this. It was WRONG on
one load-bearing particular -- the client-A widening -- and that error made the
subsystem look larger and more legally fraught than it is. R226's point is the
reason I felt free to say so: R133 is a coordinator-authored prose section, not
an owner ruling, and argues on evidence.

R223's floor arithmetic needs one correction that goes the RIGHT way and one
that does not:
  * RIGHT WAY: R223 counts four blocks it cannot close. With R197's door built
    and the client-A widening already performed, FORGE.SHADOW is a BUILD, not a
    ruling. The floor it puts under the register is engineering, not law.
  * OTHER WAY: this chain is SEVEN blocks, not the five the brief carried, and
    two of its remaining blockers are owner decisions (the view selector) or
    dangling-output trades (the governor's TERRAIN.LOD group). True zero needs
    both settled, and neither is in this packet's gift.

=========================================================================
10. GATES, AND WHY THIS SET (R227)
=========================================================================
THE DIFF IS ONE MARKDOWN CONTRACT FILE. No RTL, no ports, no generated file, no
ledger. R227 says a gate earns its runtime by being able to change its answer,
so the smoke forms were NOT run: a .md diff cannot change raster pixels,
frames_admitted or any mutant's polarity, and running ten forms to re-prove
that comments do not simulate is the exact spend R227 named. I ran every static
gate anyway, including the three --check generators and the tie-off audit,
because they read files and are seconds.

  check_console_inventory            RC 0
  check_prod_manifest                RC 0
  check_quartus17_syntax             RC 0
  check_case_labels                  RC 0
  wrapper_port_parity                RC 0   1270 = 1270, missing 0, stale 0
  mutant_drivers                     RC 0
  uncashed_cheques                   RC 0
  check_counters                     RC 0
  refmodel_liveness                  RC 0
  duplicate_functions                RC 0
  packet_h_tieoff_audit (core)       RC 0   8 declared / 1 reasoned / 10 group / 0 SILENT
  gen_prod_top.py --check            RC 0   fresh
  gen_console_board.py --check       RC 0   fresh
  gen_shell_paired_diff --check      RC 0   fresh
  gen_shell_paired_diff --check --mutant  RC 0
  mutant_copy_drift (AFTER commit, R121)  OK, 57 copies / 361 modules
  completion_register                RC 1 = 22 gaps (normal; 9 tie-offs +
                                     13 disconnected + 0 unbuilt)

REGISTER 22 -> 22. No counter was added, so none is owed a positive control.

AN INSTRUMENT NOTE ON MYSELF, in the spirit of the rule. My first
completion_register run reported "RC=0" and I nearly quoted it. It was piped
through tail, so I had read the PIPELINE's exit code -- CLAUDE.md's "read the
build's exit code, not the pipeline's", committed by the person who had just
read that chapter. The real RC is 1 and the gap count was right both times, so
the error was harmless here and would not have been on a gate whose RC is the
answer.

=========================================================================
11. FILE-SET COLLISIONS WITH LIVE LANES
=========================================================================
None. My only changed file is design/contracts/FORGE.SHADOW.md. I did not
touch zhao_console_core.sv, tests/CMakeLists.txt, design/blocks.yml or
design/prod_manifest.yml, and git diff --cached --name-only was checked before
both commits. zhao_geom_ladderbank is named out of bounds for POSEREAD and is
mine; I did not edit it, so there is no overlap in either direction. DELTALAW
is in terrain/surface-stamp and my heighttap finding is a READ of
zhao_terrain_heighttap, not an edit.

=========================================================================
12. FOR THE COORDINATOR -- what the next lane needs
=========================================================================
  1. NO OWNER DECISION IS NEEDED FOR CLIENT A. R3 sanctions the third client;
     the widening is already in the tree. Do not schedule an escalation.
  2. ONE OWNER DECISION IS NEEDED: the per-instance ladder's view selector
     (section 5b). Recommendation attached: pay the index bit.
  3. ONE COORDINATOR DECISION IS NEEDED: whether to compose the governor with
     its TERRAIN.LOD output group dangling, or hold it until zhao_terrain_lod
     composes. R75 and R223 both say hold. I did not overturn that.
  4. THE CMD.EXEC LANE IS UN-BLOCKED on SetView.pixel_error and
     SetPresentationContract.view_count -- I14's deferral cites R133's parking
     and D2 has lifted it.
  5. A SIXTH FORGE PACKET IS NOT WASTE ANY MORE. R223 recorded that five passes
     produced zero movement and that a sixth should not be scheduled. That was
     correct against the list those passes shared. The list has changed: two
     blockers are gone, two are new, and one was never a law. The next one is a
     BUILD, and it should be briefed against the contract's build order rather
     than against R133's summary.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```

## `9555a86d` -- FINDINGS -- SHADOWSUB, FOLLOW-UP (the build pass)

```
FINDINGS -- SHADOWSUB, FOLLOW-UP (the build pass)

Branch gz/shadowsub. Register 22 -> 22. One new RTL file, composed nowhere.

WHAT I BUILT, WHAT I DID NOT, AND THE ONE CLAIM I WITHDREW
==========================================================

BUILT AND LANDED: zhao_terrain_tapshare -- the arbiter the tap_* checkmark was
hiding. 97 directed checks, an inverted-polarity positive control, both
ledgers, and a REAL DEFECT FOUND IN MY OWN FIRST VERSION.

NOT BUILT: the third client-A arm and the seven-block composition. The reason
is structural and I give it in full below; it is not "ran out of time".

WITHDRAWN: my own claim that the arena-fill path on client A's result port is
needed. It is not, and composed silicon already disproved it. That claim is
why the coordinator ruled the path out of scope -- so the ruling protected
against nothing, and the subsystem has NO remaining law obstacle at all.

=========================================================================
1. zhao_terrain_tapshare -- BUILT, TESTED, LEDGERED, NOT COMPOSED
=========================================================================
The tap has EXACTLY ONE requester group and u_part_terrain_tap drives every
wire of it. The hard part is not the arbitration, it is that THE RESPONSE
CARRIES NO TAG AND NO RIDER: rsp_valid_o plus eighteen data outputs arrive with
nothing saying whose request they answer. Every other shared service here
routes by rider -- zhao_project_service's header says why: the rider "cannot
drift from the data it describes". This one cannot, so the block holds the
owner, which is precisely the structure CLAUDE.md's metadata-swap chapter is
about. Two properties make it safe and both are MEASURED:

  * ONE REGISTER, NOT A QUEUE, licensed by `assign req_ready_o = (st_q ==
    S_IDLE)` -- the tap is strictly single-in-flight. SINGLE_FLIGHT_ONLY is a
    parameter that records the premise and $fatal-guards it, so a future
    pipelined tap fails to elaborate instead of quietly needing a queue.
  * THE TWO SIDES OF stray_rsp_o HAVE DIFFERENT ENABLES -- own_v_q loads on the
    REQUEST handshake and clears on the RESPONSE. So it is not "a detector
    wired to two operands that move together"; it can fire, and it was seen to.

RESPONSE DATA IS BROADCAST, NOT MUXED N WAYS. Eighteen 32-bit-class buses times
N of multiplexer, for a value only one client can be waiting for, is real ALM
on a device where ALMs are the binding constraint. Clients qualify the shared
bus with their own r_rsp_valid_o bit. The cost of that choice is written into
the header rather than left implicit: a client must not latch the bus on a
cycle its own bit is low.

THE DIRECTED TEST FOUND A REAL DEFECT IN THE FIRST VERSION, and this is the
part worth keeping. The block could not grant on the cycle an owner retired, so
every height tap cost a dead cycle -- about 8% of the tap's thirteen-state
walk, paid on every tap PART.TERRAIN_TAP and FORGE.SHADOW will ever make. It
LINTED CLEAN with -Wall, it passed check_quartus17_syntax, it was functionally
correct, and no gate in this tree could have seen it. Only the "granted on the
retiring cycle" check did. That is R60's whole argument in one incident.

The repair assumes NOTHING about the service's internal timing: if the tap is
not ready on the retiring cycle, grant_c is low and the offer simply stands
next cycle -- the ordinary ready/valid outcome.

EVIDENCE, all built and run at the pushed commit:
  terrain_tapshare_directed        97 checks PASS
    - grants ALTERNATE exactly over 20 contended rounds: the N-1 fairness bound
      EXERCISED, not asserted. contended_o = 20.
    - the discriminating case: an owner held across a SIX-CYCLE walk WHILE THE
      OTHER CLIENT ASSERTS VALID. An arbiter routing by "who is asking now"
      answers the wrong client and every other check still passes.
    - stray_rsp_o = 0 throughout: the NEGATIVE control.
  terrain_tapshare_mutant_control   3 checks PASS, POLARITY INVERTED
    - stray_rsp_o = 1. BOTH halves asserted: the counter FIRED, and the fault
      it watches for is REAL (the answer reached no client at all). R95: a
      counter must DISCRIMINATE, not merely move.
  lint_zhao_terrain_tapshare        PASS
  All three through ctest: 3/3, 100%.

The mutant is a COPY and I REGENERATED it after the dead-cycle repair. Its diff
against production is the rename plus one substantive line. A copy of an old
version is a positive control for a block that no longer exists, and it goes
stale in the flattering direction -- it keeps passing.

LEDGERS: both gates correctly REFUSED the file until it had a disposition --
"G4 UNCLASSIFIED" and "UNACCOUNTED". That is R210 working, and it is the second
instrument in this packet that earned its keep. console_inventory
pending_compose, prod_manifest not-yet-adopted, each naming what it is, what it
costs, and what deletes the entry.

COST, recorded and not estimated into the budget (R236): 0 DSP, 0 M10K; a
combinational rotation, one owner register, one index register and three
counters per client. Tens of ALM expected at N=2. NOT FITTED, and I did not run
Quartus.

NOT COMPOSED, deliberately: its second client zhao_forge_shadow is not
composed, so wiring it in today puts an arbiter in front of a port with one
user -- cost without capability, and a gap opened to close none.

=========================================================================
2. THE CLAIM I WITHDREW, AND WHY IT MATTERS MORE THAN THE BLOCK
=========================================================================
In my first pass I wrote that what would genuinely re-author a ratified law is
an ARENA-FILL PATH ON CLIENT A'S RESULT PORT, and that whether shadow hulls get
an arena is a real design question. The coordinator ruled that path out of
scope on the strength of it.

IT IS NOT NEEDED. zhao_part_project takes its particle results STRAIGHT OUT on
q_* -- q_x_o/q_y_o as signed [20:0] canvas coordinates, q_d_o, size, colour --
with NO ARENA ANYWHERE on that path. The only occurrences of `arena` in that
file are the geometry rider's bit layout and one comment. A client-A client is
not obliged to land in an arena; it is obliged to carry an owner in the rider
and take its results back on a demux arm. Particles do exactly that, today, in
composed silicon.

So a shadow hull's route is the PARTICLE's, not terrain's: world vertices into
client A under an owner, screen-space vertices back on a demux arm, a small fan
assembler, GEOM.CLIP's door. THE WIDTHS ALREADY AGREE -- q_x_o/q_y_o are
signed [20:0] and zhao_geom_clip's tri_ax_i is signed [20:0], which is R188's
"the clamp is the law, the extra bit is headroom" arriving where it is needed.

CONSEQUENCE, stated plainly: NOTHING IN THIS SUBSYSTEM RE-AUTHORS A RATIFIED
LAW. R133's sentence now has no surviving referent -- the owner-field widening
was performed under R68 sub-build 4 and R3 sanctions it, and the arena path it
might otherwise have meant is not required. What remains is entirely
engineering, and the out-of-scope ruling guards nothing.

A note on the shape of both my errors this packet, because it is the useful
part. BOTH ran in the direction that made the work look BIGGER. That is the
inverse of the failure CLAUDE.md warns about, and it has the same cause and
none of the immune response: nobody audits a blocker. A diagnosis that absolves
the design gets challenged; a blocker that defers the work gets believed and
re-quoted, which is exactly how this cluster accumulated five passes of zero
movement.

=========================================================================
3. WHY THE ARM AND THE COMPOSITION DID NOT LAND -- the structural reason
=========================================================================
THE THIRD ARM CANNOT LAND ON ITS OWN. Adding ports to zhao_part_project obliges
zhao_console_core to connect them (an unconnected port is a PINMISSING the next
fit discovers), and with no LODSTATE to connect them to, the only legal
connection is a tie-off. That closes NO gap and opens one -- strictly worse
than not building it, and the campaign's first prohibition. SystemVerilog gives
no port default that Quartus would honour, so there is no third option.

And LODSTATE cannot land either, for two reasons I measured in the first pass
and re-checked here:
  (a) its c_* caster output has no consumer until zhao_forge_shadow composes;
  (b) its j_form_index_i and j_view_i have no producer on the composed DRAWJOB
      job seam. The form index is EXPOSABLE, not inventable -- drawjob already
      holds form_idx_q [23:0] = d_form_i[31:8] and compares it against
      dir_index_q, which IS ladderbank's key, exactly as ladderbank's header
      says ("the key zhao_geom_drawjob already uses for its residency
      directory"). The VIEW is the open owner decision.

So the chain is: forge_shadow needs the fan assembler and the GEOM.CLIP-door
arbiter; lodstate needs forge_shadow; the arm needs lodstate. ONE COMMIT OR
NONE, and its terminal link is a build I could not responsibly land in this
packet at the quality the rest of it got.

R3'S SCHEDULE PROOF LANDS WITH THE ARM, BOTH HALVES, and the reason is a fact
about the benches rather than a preference. The RATE half needs
zhao_part_project against the REAL zhao_proj_subsystem with every arm
saturated. tb_part_project drives the block STANDALONE -- its verilate()
sources are zhao_part_project.sv and zhao_part_record.sv, no service and no
core -- so composed multi-client throughput has never been measured and cannot
be from any bench in this tree until the arm exists. Quoting
zhao_project_service's header figure of 398,784 of 1,666,666 clocks as the
answer would be comparing a current design to an old claim, which is the one
thing CLAUDE.md says never to do with a measurement. I declined to do it.

=========================================================================
4. GATES, AND WHY THIS SET (R227)
=========================================================================
MY DIFF TOUCHES THE CONSOLE CLOSURE NOWHERE, and that is checked rather than
asserted: `git diff --name-only <merged head>..HEAD -- fpga/` returns exactly
ONE file, fpga/rtl/terrain/zhao_terrain_tapshare.sv, and
`grep -c zhao_terrain_tapshare` returns 0 in BOTH zhao_console_core.sv and
zhao_prod_top.sv. The console is byte-identical. So no smoke form can change
its answer, and the ten forms were not run. Everything that CAN change its
answer was run:

  check_console_inventory        RC 0   (RED first -- G4 UNCLASSIFIED -- until
                                         the disposition landed)
  check_prod_manifest            RC 0   (RED first -- UNACCOUNTED)
  check_quartus17_syntax         RC 0   self-test 13 fire / 22 no-fire PASSED
  check_case_labels              RC 0
  wrapper_port_parity            RC 0   1281 = 1281 on the FIXED parser
  mutant_copy_drift              RC 0   58 copies (was 57) -- run AFTER commit
  mutant_drivers                 RC 0
  uncashed_cheques               RC 0
  check_counters                 RC 0
  refmodel_liveness              RC 0
  duplicate_functions            RC 0
  check_findings_citations       RC 0   (new gate)
  packet_h_tieoff_audit (core)   RC 0   unchanged, 0 SILENT
  gen_prod_top --check           RC 0   fresh
  gen_console_board --check      RC 0   fresh
  gen_shell_paired_diff --check  RC 0   fresh, and --check --mutant RC 0
  completion_register            RC 1 = 22 gaps (normal)
  ctest -R tapshare              3/3 PASS

I MERGED THE SHARED HEAD e8c027c2 rather than rebasing, to pick up the fixed
wrapper_port_parity. That mattered: the old parser read 1270 = 1270 AFTER
DELTALAW ADDED A PORT, because both sides were parsed by the same
line-anchored pattern and the symmetry held. Gating a port change against it
would have been gating against an instrument blind to port changes.

ON MY OWN TRANSCRIPTION: my first merge failed with "not something we can
merge" because I dropped a character from the hash. git said the short form
resolved and the long form did not, which is the tell. Worth one line because
the campaign keeps paying for mis-transcribed identifiers.

=========================================================================
5. FILE-SET COLLISIONS WITH LIVE LANES
=========================================================================
GOURAUDBUILD is in geometry and so am I, but my only fpga/ file is under
terrain/ and is new. I did not touch zhao_geom_clip.sv, zhao_geom_attrpack.sv,
zhao_geom_vattr.sv or zhao_console_core.sv -- I READ all four.
FORGEPRIM: untouched; I took no opcode and went nowhere near 0x0304/0x0305.
DELTALAW: my heighttap work is a READ of zhao_terrain_heighttap and a NEW file
beside it; I did not edit the tap, deliberately, because a port on a leaf costs
its whole instantiation chain.
SHARED FILES I did edit: tests/CMakeLists.txt (appended), console_inventory.yml
and prod_manifest.yml (one entry each). git diff --cached --name-only checked
before every commit; all four commits carried exactly my files.

=========================================================================
6. FOR THE NEXT LANE -- the whole remaining subsystem, in order
=========================================================================
The build order lives in design/contracts/FORGE.SHADOW.md, not here, because a
run folder is the wrong home for anything durable. What changed in it:

  1. NO LAW BLOCKS THIS ANY MORE. Not R3, not R133, not R197. Do not schedule
     an escalation and do not brief against R133's summary.
  2. THE TERMINAL LINK is the shadow fan assembler + the GEOM.CLIP-door
     arbiter + the shadow material span. The door EXISTS and carries the untex
     bit; the core's own comment says the arbiter that admits shadow hulls
     presents its bit to THAT gate. The projection route is the particle's q_*
     shape. This is the largest remaining build and it is ordinary engineering.
  3. THE ARBITER IS DONE. zhao_terrain_tapshare, tested, ledgered, waiting.
  4. ONE OWNER DECISION REMAINS: the per-instance ladder's view selector for
     active_mask == 2'b11. Recommendation unchanged: pay the index bit.
  5. ONE COORDINATOR DECISION REMAINS: the governor's TERRAIN.LOD output group.
     R75 and R223 say hold; I did not overturn it.
  6. THE DRAWJOB FORM INDEX IS EXPOSABLE, NOT INVENTABLE. drawjob already holds
     it. That is a new output on a composed block and costs its chain, but it
     is not an absence and not a decision.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>
```
