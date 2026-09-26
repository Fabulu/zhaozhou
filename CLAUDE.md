# Zencrifice — working rules

> ## ⚑ READ FIRST: `reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt`
>
> **The owner is on vacation and has DELEGATED the technical decisions. That file
> is standing authority and it OVERRIDES THE APPROVAL LOOP.** It does not expire
> at the end of a session, a packet, a day or a context window; only the owner
> revoking or replacing it ends it. Adopted 2026-09-25 (commits `460296f9`,
> `ead6f107`), reviewed against `13b22987`.
>
> *"Use your own brain. You are the implementation architect, not a relay that
> sends every unresolved engineering choice back to me. … Do not stop at another
> list headed OWNER DECISION. Make the decision, write down the rationale and
> consequences, implement it, and test it."*
>
> **Every open hold is decided there** — I34's four channels and their new bank-2
> destinations, I21's island-pitch authority, I20's fragment-state/stencil/
> provoking-vertex semantics, I53–I55's single geometry identity space, I56's
> quota seal, the terrain arm, SDRAM scheduling for frame-critical reads,
> DSF-01's wrapper, bounded map experiments, and worktree cleanup.
>
> **What the delegation is NOT:** authority to delete a feature, cut 16 fields to
> 4, remove Gouraud or detail normals, shrink the guaranteed giant, replace a live
> path with testbench stimulus, waive a correctness failure, or call reduced work
> equivalent to reach zero or fit a device. **The shipping target stays
> `5CSEBA6U23I7`;** a bigger diagnostic target does not change it. *"A measured
> engineering impossibility is a finding, not permission to invent a pass."*
>
> **Decision record format, and it is short:** question; chosen option; reason and
> alternatives; constraints/cost; code/tests/compatibility consequences. Then
> execute.
>
> **Before every integration push:** fetch the remote branch and inspect new owner
> commits FIRST. *"A push sends local commits OUT; it does not fetch new owner
> instructions."* A non-fast-forward rejection means fetch and integrate —
> **never `--force`, never `--force-with-lease`, over an owner commit.**

Three repos: **zhaozhou** (the console — silicon, reference oracle, tools),
**nanquan** (the language), **Upheaval** (the game, full title *Tribute
Upheaval*; the folder stays `Upheaval/`).

---

## WHEN MAKING MODELS, ANIMATIONS AND TEXTURES, WE ARE MAKING ART

**Measurement never trumps actually looking at things.**

Fabian, 2026-08-26, after a creature was rebuilt from measurements and came out
worse than the version authored by eye.

This is not a caution about being careless with numbers. It is the opposite
failure: a measured number *feels* like evidence, so it stops getting
questioned, and it quietly replaces the judgement that was supposed to be doing
the work. Three things went wrong in one pass, all the same error:

* **The body taper** was derived from a distance transform of the concept
  drawing. That is not the 3D body radius — it is the half-width of a drawn
  outline in the picture plane, conflating real thickness with foreshortening,
  with measuring across a *bend* instead of across the body, and with the places
  the shape overlaps itself. It was trusted precisely *because* it was "a
  measurement". The hand-authored version looked better.
* **The dorsal pink** was measured off the scan and shipped as "the sheet's own
  pigment". But a scanner flattens and lightens, and a pale rose that reads as
  pink on a white sheet under room light reads as grey-white on dark ground at
  240p under one key light. Matching the paper is not matching the READ, and the
  read is the thing. The earlier, "wrong", hand-saturated value was closer.
* **The crayon grain** was clipped to ±16% of luminance — narrower than the
  light rig's own range, so it was mathematically present and visually invisible.
  It measured fine and looked like flat plastic.

**The rules that follow:**

1. **Author by eye. Render. Look. Compare. Adjust.** That loop is the job.
2. **Measurement belongs on the COMPARISON side**, checking what was authored
   against the reference — never on the generation side. A tool that says "you
   are 20% off" is good; a tool that decides a radius is not.
3. **Measure things that ARE the thing, never a projection of them.** A pigment
   is a pigment. 3D form derived from a 2D drawing is not.
4. **Measurement can remove a BIAS; it cannot choose a VALUE.** Finding that a
   sampling method skewed pale is useful. The shipped colour is still chosen by
   looking at it in scene, at final resolution, against what it sits on.
5. **A drawing is an interpretation, not a scan.** The artist thickened forms
   where they needed visual weight and reached for the crayon that was in the
   box. Converting that mechanically keeps the artefacts and loses the intent.
6. **Never remove the owner's control in the name of fidelity.** Every shape,
   colour and timing value belongs in a named, editable constant. "This is
   generated from the reference, so it is not a knob" is how a wrong number
   becomes an unadjustable wrong number.
7. **Component checks passing is not likeness evidence.** A palette verified
   against the sheets, a light rig verified against a face table and a mesh
   verified by CRC can all pass while the creature is unrecognisable. Look at
   the whole thing, in motion, against the concept.

## Two more ways a measurement lies

Added 2026-08-28, from the first creature. Both are the art law's failure mode
wearing new clothes, and both cost days.

**A measurement across MISMATCHED POSES measures the pose.** A ratio said the
creature's tube was "a wire — 22% of loop height against the drawing's 40%", so it
was thickened substantially. That comparison put our grounded gameplay stance
against the sheet's aerial S: different pose, different foreshortening, different
loop. A later overlay that laid our radii along **the sheet's own traced pose**
showed we were **2x too thick** — we had moved the value confidently in the wrong
direction, and the owner's eye had said "too broad" before the tools did. Compare
like with like, or do not compare.

**A gate passing is not the thing looking right.** A snout measured horizontal
still hung from the bottom of a downward hook. Every automated gate passed while a
stray triangle sat in a creature's eye. Fixing a genuine rotation-wrap bug did not
turn a spin into an elegant wheel, because the fault was the shape changing during
the rotation, not the interpolation. Gates catch regressions; only looking catches
wrongness.

## A broken instrument lies in ONE direction

Added 2026-09-04, after nine separate measuring tools were found wrong in a
single session. Every one of them had the same property, and it is the reason
none had been caught:

**The defect always made the answer look BETTER, SMALLER or SIMPLER than the
truth.** A parser that silently drops what it cannot match reports fewer
problems. A self-check whose pattern matches nothing reports "no silent drops".
A rule written after the fit it governs reports a pass. A `status` field holding
the last *good* run reports `ok`. A count compared against a stale measurement
reports no change.

That asymmetry is the whole lesson. **Nobody audits good news.** A tool that
reported too many problems would have been fixed the first afternoon; a tool
that reports too few is trusted for weeks. So:

1. **A number that is exactly zero is a broken instrument until proven
   otherwise.** "0 counters match their port" was a regex with no provision for
   a width bracket. "0 arrays declared" was `^` without `re.MULTILINE`. Precision
   at zero is a tell, not a result.
2. **A detector that has not been shown to FIRE has not been tested.** Break it
   on purpose, watch the alarm go off, put it back. Three tools here now assert
   at import that their own pattern still matches a known-good example, because
   one self-check was written with a word-boundary escape that a shell heredoc
   turned into a literal backspace character — so it matched nothing and
   printed reassurance for its whole life. (This sentence lost its own escape
   the same way on first writing, which is either evidence or comedy.)
3. **Check the heuristic against a case you can verify by hand before believing
   the total.** A block-id-to-module rule said 25 blocks were unbuilt; three of
   them existed under a name the rule did not construct.
4. **Never compare a current file to an old measurement.** Declared-versus-
   measured is a real check; declared-today versus measured-a-week-ago is a
   different question wearing the same shape, and it produces confident
   nonsense.
5. **When a tool explains itself, the explanation is a claim too.** A rule that
   fired saying "state that belongs in memories is in flip-flops" was right that
   the ceiling was breached and wrong about why — the payload was already in 13
   M10Ks. A wrong diagnosis attached to a right alarm sends the next person to
   reshape something that is already correct.

This is the same law as *measurement never trumps looking*, one level up: the
tool that does the measuring is itself a thing that has to be looked at.

## The first explanation that absolves the design is the one to check hardest

Added 2026-09-05, after the same mistake three times in one day. This is the
"broken instrument" law's twin: that one is about TOOLS reading low, this one is
about DIAGNOSES landing soft. Each of these was reached honestly, was plausible,
and was wrong in the direction that meant less work:

* **The combiner fit reported 8 DSP against a rule of 2**, and the obvious
  reading was that `multstyle = "logic"` was being ignored by Quartus. It was
  not. The block contained about fourteen multipliers, because
  `unit_mul_logic(...)` sat inside every arm of two seven-arm case statements.
  The tool was fine; the RTL was what the architecture's own sentence forbids.
* **The composed island came in 2.4% under the sum of standalone fits**, having
  been argued at length to be much smaller because 889 virtual pins inflate the
  standalone rows. The argument was right in kind and wrong in magnitude, and
  the write-up now says so rather than re-scoping the claim, because otherwise
  the next person inherits "standalone sums overstate" as a rule that is 2.4%
  true.
* **Every one of the twelve worst timing paths started at a virtual pin**,
  which reads exactly like a measurement artefact of a leaf fit. Splitting all
  2,000 summarised paths by origin showed 1,595 start INSIDE the design, worst
  slack −3.63 against the pins' −4.482. Deleting the boundary entirely would
  buy about 4 MHz of 36 needed. The artefact was real and almost irrelevant.

**The tell is the same every time: the comfortable explanation arrives first
and explains ALMOST all of the evidence.** The check that separates them is
cheap and specific — count the multiplier sites, difference the two totals,
split the paths by where they start — and in all three cases the data to do it
was already on disk.

So: when a diagnosis means the design is fine, spend the extra five minutes.
When it means the design is wrong, it will get checked anyway.

## Counters see what pictures cannot

Added 2026-09-05, twice in one day, both times a machine doing several times the
work while producing byte-identical output:

* the material combiner **issued every microjob about twice** — a job sets its
  `done` bit only when its result lands, so `issuable()` reissued everything in
  flight. Every colour still matched the oracle exactly, because recomputing a
  product is idempotent;
* the shell bench **re-submitted one meshlet fifteen times**, because
  `m_valid_i` was driven from a level held for the whole offer window. Every
  re-run produced the identical triangle.

Neither was visible in a framebuffer comparison, a golden CRC, or any
result-checking test — and the second was found only because the first had
taught the habit of asserting an exact per-recipe job count. A test that checks
WHAT came out cannot see HOW MANY TIMES the machine did it, and throughput
budgets are written against the second number.

## A detector wired to two operands that move together cannot fire

Added 2026-09-08, after a metadata bank shipped a record-swapping defect with a
live identity counter sitting beside it reading zero.

The bank registered its read UNCONDITIONALLY, so its output tracked whatever
address was being *offered* while the stage downstream held the previous
response. A stall therefore produced **response A's data, A's token, and B's
metadata** — with every accepted/emitted counter balancing perfectly, because no
counter looks at the field that moved.

The block already contained what looks like exactly the right guard:

```systemverilog
if (rd_v_q && (rd_q[OGEN_LO +: GENW] != rd_gen_q))
  rd_gen_mismatch_o <= rd_gen_mismatch_o + 1;
```

It never fired, and it never could. The captured generation was loaded by the
**same ungated assignment** as the row, so on the swap both moved to B together —
and B's stored generation agrees with B's offered one. **The two quantities the
detector differences were corrupted in lockstep.**

This is the cancelling-errors pattern with a new and worse consequence. Two
errors that cancel produce a right answer for a wrong reason; two errors that
cancel *inside a checker* produce a **reassuring** answer, and the checker is
then cited as evidence that the thing it cannot see is fine. "There is a
generation-mismatch counter and it reads zero" is what let the packet be called
complete.

So, for any checker:

1. **Ask what the two sides of the comparison are clocked by.** If one register
   enable drives both, the comparison is structurally blind to every fault that
   enable participates in. It can only catch faults in the *values*, never in
   the *timing* — and timing is what a join gets wrong.
2. **A detector reading zero is a claim, and it is the claim to check hardest**
   — the broken-instrument law applied to RTL. Fire it deliberately on a fault
   it *should* catch before quoting its silence.
3. **A gate that cannot reach the state is not evidence about the state.** 392
   byte-identical paired records did not catch this, because that workload never
   stalls the join, so the offered address never differs from the held one. The
   records were real and the conclusion drawn from them was not.
4. **Do not write a test that asserts the bug.** "The counter fires on the swap"
   passes only while the defect exists; after the repair there is no swap to
   miss. Assert the *correct* behaviour (the record holds) and keep the
   detector's positive control separate.

### A guard you cannot reach with legal stimulus needs a COMMITTED MUTANT

Added 2026-09-09, after sweeping a session's own new work for counters asserted
zero and never seen to move. Four were found. Three could be fired with stimulus
alone -- present a token whose generation is not the one the bank holds, offer an
illegal key, break a layout with a parameter. The fourth could not, and it is the
interesting one.

`wq_overflow_o` watches for a queue holding more entries than it owns. That state
is **unreachable while the full-guard is correct**, so no legal input can move the
counter, and "it can fire" stays an argument forever. The only demonstration is to
break the guard -- and the break must not be a temporary edit to production RTL,
because that is a live-tree hazard AND it leaves nothing behind: the next person
inherits the same argument and no evidence.

So the mutant is a **committed file** under `tests/mutants/`, renamed so a source
list cannot elaborate it by mistake, with a driver whose polarity is inverted --
it passes when the counter FIRES. It is evidence about the instrument, not about
the design. `tests/mutants/zhao_texture_frag_expand_mutant.sv` is the pattern:
one substantive line, `fq_full_c`'s `>=` becoming `>`, and a header saying what
was changed and why.

Two tool facts learned the same way, both of which would otherwise have been read
as reassurance:

* **`--lint-only` does not run `initial` blocks.** Linting a deliberately broken
  parameterisation returns RC=0 and says nothing whatever about the elaboration
  `$fatal` guarding it. A clean lint is not evidence about an elaboration check.
* **`// synthesis translate_off` does not make Verilator skip the block.** Proven
  by planting a syntax error inside one and watching lint reject it. So a guard in
  there is live in simulation -- which is the opposite of what the pragma's name
  suggests to a reader, and worth knowing before deciding a guard is dead weight.
* **Verilator's `-D` cannot override a FUNCTION-LIKE `` `define ``, and says
  nothing when it fails to.** Added 2026-09-16, after two combiner mutants passed
  while measuring unmutated production. The seam was the usual shape --

  ```systemverilog
  `ifndef ZHAO_THING
  `define ZHAO_THING(a, b) (a)      // production default
  `endif
  ```

  -- and the mutant build passed `-DZHAO_THING=...`. A command-line `-D` defines
  an OBJECT-like macro, so the `` `ifndef `` still saw the name as undefined,
  compiled the production default, and the build succeeded with no diagnostic.
  The mutant ran, the test passed, and it was testing nothing. This is the
  broken-instrument law wearing the toolchain's clothes: the failure is silent
  and in the flattering direction.

  The working shape is a plain `` `ifdef `` that selects between two definitions,
  which `-D` does reach. And the check that separates the two: **compile the
  mutant with the macro UNDEFINED and confirm the output differs.** If it does
  not, the selector never engaged. Every macro-selected mutant here owes that
  negative control.

And when a mutant trips a SIMULATION assertion before the synthesizable counter
can be read, disable the assertion **in the mutant only**, with the reason beside
it. The assertion firing is independent corroboration; the counter is the thing
that ships.

### A committed mutant is a COPY, and a copy goes stale in the flattering direction

Added 2026-09-16, after finding this in three separate lanes in one day. It is
the `.gitignore` lesson and the uncashed-cheque lesson wearing the same coat:
the knowledge was written down, correctly, and nothing ever read it back.

`zhao_texture_frag_expand_mutant.sv`'s own header says *"REGENERATE IT if
zhao_texture_frag_expand.sv changes shape: this is a copy, and a copy of an old
version is a positive control for a block that no longer exists."* Production
changed shape two commits later and gained **four ports**. The instruction was
never carried out, and the control went on passing.

The failure mode is specific and it is not "the test goes red":

* **Thirteen combiner copies** were cut at `005578fb`; production moved twice
  after. Each "one substantive line" had quietly become 70–140. All thirteen
  controls stayed GREEN throughout, because the *mutations* were intact — it was
  the body around them that was two weeks old.
* **Eight AUX-pipe copies** were cut at `10dd9cc4`; production moved at
  `005578fb`. **Seven went on passing**, because their drivers do not exercise
  what changed. The eighth failed with *"AUX credit reserves through terminal
  owner acceptance (exactly 16 live): expected 0x10, got 0x11"* — which reads
  exactly like a credit overflow in shipped RTL, was chased as one, and was the
  mutant measuring a machine that no longer exists. **A stale copy does not
  report that it is stale; it reports something alarming about the wrong
  component.**

So the green is worthless in one direction and the red points somewhere else.
Both halves are the broken-instrument law.

**`tools/budget/mutant_copy_drift.py` now detects it**, registered as the
`mutant_copy_drift` ctest. The signal is **provenance, not similarity**: if the
production file has been committed since the copy's file was, the copy cannot
contain what production gained. Diff size is corroboration only — removing a
pipeline stage is legitimately a large edit, so a size threshold alone produces
both false alarms and false silence. Two refinements it needs and has:

* a **wrapper** that instantiates the production module cannot drift, and must
  not be counted as a copy;
* a file holding a dozen renamed copies must be diffed **per module**, or the
  other eleven read as drift and drown the signal.

**To refresh a copy, three-way merge it** — base is the revision it was cut
from, "ours" is current production, "theirs" is the copy with its rename undone.
Each copy then carries its own mutation forward and nothing else, and a copy
whose mutation edited a line production has since replaced CONFLICTS, which is
the tool telling you the mutation needs re-authoring rather than transplanting.
When the copies re-aligned whitespace in regions production also edited, every
merge conflicts; isolate each mutation whitespace-insensitively and re-apply it
to the current body instead.

**NORMALISE THE BASE'S LINE ENDINGS BEFORE YOU BELIEVE THE CONFLICTS.** Added
2026-09-23, refreshing three `zhao_forge_cliff_ram_*` copies. `git merge-file`
reported **ten conflicts in a file whose real divergence is nine lines**, and
`diff` of copy against base claimed **1,415 changed lines out of 932**. The
cause is not the copies: `git show <rev>:<path>` hands back **INDEX** content,
which is **LF**, while the working copies are **CRLF**, so every line reads as
changed. Convert the base to the working copy's endings and the identical merge
reports **zero conflicts**.

This is `check_eol_worktree.py`'s chapter in new clothes — *"a line-ending
difference in a correct file reads as a content mismatch"* — and the obvious next
move, hand-editing around the conflicts, is exactly wrong. Note the direction:
for once the broken instrument is **alarming** rather than flattering, which
means the cost is wasted work and a re-authored mutation nobody needed, not a
false green.

And **a refreshed copy is stale again the moment production moves again** —
`tools/budget/mutant_copy_drift.py` caught exactly that, one change later, in
the same session that wrote it.

**"INHERITED" IS THE WORD THAT MAKES A RED NOBODY'S JOB.** Same day, three
copies, two readers: both called the drift inherited debt from an earlier
packet. It was not. Production and all three copies were committed **together**,
and production moved **that morning** in the reader's own merge. A same-day
regression described as inherited gets triaged instead of fixed. **Before
calling a red inherited, read the two commit dates** — it is one `git log -1`
per file, and it decides whose job it is.

One more from the same day, about receipts rather than RTL. A fit row stamped
`failed:structure` is **not** a failed measurement — the fit completed and the
*budget rules* rejected it. On the island the row with a clean tree, a real
digest and three honestly declared breaches was stamped `failed`, while a row
fitted from a **dirty tree**, whose digest describes nothing, was stamped `ok`.
A gate reading `status` alone refuses the trustworthy number and quotes the
worthless one. Read `rtlCleanAtHead` first, always. And `ruleViolations: []` on
a **labelled** row is silence, not compliance: labelled rows are never
rule-checked (0 of 26 carry violations, against 12 of 92 unlabelled).

## A rule that HIDES waste is not a rule that removes it

Added 2026-09-06, after the machine's C: drive reached **zero bytes free, 952 GB
of 952 GB**. Quartus died mid-placement 55 minutes into a fit, Verilator builds
died with "No space left on device", and the disk filled *during* a fire-test
mutation and left a **zero-byte backup file** — a moment later and the tree
would have held deliberately broken RTL with a truncated backup beside it.

About 33 GB of it was ours: ~129,000 `.rgb` raw frame buffers, the
uncompressed intermediates the render pipeline writes and never removes.

**The instructive part is that this was already half-fixed, eight days
earlier.** `.gitignore` has covered `*.rgb` since 2026-08-28, added after an
over-broad `git add` swept 867 raw frames into three commits and cost a history
rewrite. Its comment is exactly right: *the evidence is the PNG contact sheet,
not the frames it was made from.* So "never committed" was solved thoroughly —
and "never accumulates" was never solved at all. The frames simply stopped
being visible to git while continuing to fill the disk.

**Making waste invisible to your tooling is not the same as removing it**, and
the gap took a dead toolchain to surface because every tool that could have
noticed had been told to look away.

* `tools/maintenance/purge_render_intermediates.py` is the missing half. Dry
  run by default; spares anything touched in 48 h and the newest run folders
  wholesale; never touches `.webm`, `.png`, `.md` or git packs.
* Its root defaults to the **zencrifice root, not the repo** — 15 of the 33 GB
  sat in a sibling creature working directory outside `zhaozhou` entirely.
* When you add an ignore rule, ask what will now delete the thing. If the
  answer is nothing, you have moved the problem rather than fixed it.

## ONLY THE LATEST VERSION GETS COMPOSED OR FITTED

Fabian, 2026-09-19: *"YOU ONLY GET TO FIT THE LATEST VERSION. IF IT IS BROKEN
YOU FIX IT."*

Said after the console was found composing the **entire v1 FIELD datapath** --
alu, seq, ring, rot, noise, mul, len, normalize -- while all fourteen
`zhao_field_v3_*` modules sat outside the closure.

This is not tidiness. Fitting an old version **measures a machine nobody ships**:
it spends ALM and DSP on dead weight and makes the resource number describe the
wrong design. On a device already over on both, that is the most expensive kind
of wrong number, because it is wrong in a direction nobody questions -- a big
number looks like honest bad news.

**"It is broken so I composed the old one" is not an answer.** Repairing the
current version IS the task. Falling back is a decision to ship the old thing,
and it must not be made quietly inside a packet.

Two traps that made this easy to do by accident:

* **The naming is not consistent, so eyeballing fails.** This tree supersedes
  both ways -- `zhao_texture_cache_pipe_v2` by SUFFIX, `zhao_field_v3_len` by
  INFIX. A grep for `_v2$` finds half of it.
* **The completion register pointed at the old ones.** `FIELD.SEQ.CORE`
  resolved to `zhao_field_v2_core`, `FIELD.PROGCACHE` to the unversioned block,
  and nothing named v3 at all -- so "connected" was satisfiable by wiring the
  superseded module, and the instrument said fine.

`completion_register.py:superseded_in_closure()` now reports every INSTANTIATED
module for which a higher-versioned sibling exists on disk, in both naming
shapes. Instantiated, not merely listed: a module nothing elaborates costs the
fitter nothing, and a check that cries wolf about dead sources is one people
learn to skip.

It found two nobody was looking for, outside FIELD entirely:
`zhao_geom_binner` against `zhao_geom_binner_v2`, and `zhao_raster_tile_pipe`
against `zhao_raster_tile_pipe_v2`. **The ruling is general; check for it
whenever you compose anything.**
## Fit at SUBSYSTEM BOUNDARIES, not after every nodule

Fabian, 2026-09-08: *"fits are what's going to be the biggest blocker, they
cost so much time. We should only fit at big architectural subsystem, we can't
afford fitting after every tiny nodule."*

An island fit is 1.5–4 hours. Two of them consumed most of one session while the
actual engineering — reproducing a defect, repairing it, building two new
modules and their tests — took minutes each. The fit is the scarce resource and
must be spent like one.

**The working rule:**

1. **Ask what the question actually is before reaching for Quartus.** Area,
   Fmax, RAM inference and DSP count need a fit. *Everything else does not.*
   Correctness, throughput in clocks, handshake behaviour, field routing,
   atomicity under backpressure, parameter sensitivity — all of that is
   Verilator, and it answers in seconds. The RCP V3 swap sat behind a fit for
   days; the question that killed it ("does the tile meet its throughput
   criterion at the island's NCTX?") was one verilate flag and under a minute.
2. **Build new blocks standalone, wire several in at once.** A new file is in no
   running fit's closure, so it can be written and fully tested while a fit
   runs. Accumulate a subsystem's worth of change, then spend one fit on all of
   it.
3. **Name the fit gates in advance.** A plan should say where its few fits are
   and what question each answers. A fit nobody could state a question for is a
   fit that should not run.
4. **A fit that measures a circuit you already know is wrong is wasted.** The
   `@pktC` receipt measured an arrangement carrying a live metadata-swap defect,
   and was taken from a dirty tree besides. Repair first, then measure.

This does not license skipping fits. A subsystem that changes area or timing
and never gets fitted is an unmeasured claim, and the ALM/Fmax budget is real.
It licenses *batching* them.

## A thing BUILT is not a thing INSTALLED, and a thing FIXED is not a thing MEASURED

Added 2026-09-09, after an archaeology sweep found a ~33-DSP / ~6,000-ALM saving
that had been **fully planned two weeks earlier**, whose prerequisite was
deliberately BUILT, and whose final step was simply never performed. Nobody was
careless. The sequence was exemplary right up to the end:

1. notice GEOM and TERRAIN both contain a projector — done
2. extract the identical arithmetic into `zhao_project_core` — done
3. notice that deduplicated the SOURCE and not the SILICON — done, and written
   down: *"The duplication is gone from the source. It is NOT gone from the
   silicon."*
4. defer sharing, because combined throughput did not fit — correct at the time
5. **build the prerequisite** (the projected-vertex cache) — done
6. come back and instantiate ONE core — **never happened**

The plan was right, the analysis was right, the prerequisite got built, and the
cheque was never cashed. It was not forgotten because anyone forgot; it was
forgotten because **nothing in the tree was watching for it.**

That is the `.gitignore` lesson one level up. There, an ignore rule made 33 GB
invisible to git while it went on filling the disk. Here the knowledge was
written into module headers and commit messages, in detail, by people who knew
exactly what they were deferring — and no tool ever read them back.

**Both shapes are mechanically detectable, and `tools/budget/uncashed_cheques.py`
now detects them.** Run it; it is seconds, and it reuses `module_graph.build`
and `dsp_census.load_evidence` rather than reimplementing either.

* **BUILT, INSTALLED NOWHERE.** A module measured or fit-targeted that nothing
  instantiates. The refinement that made the check useful: **being NAMED in the
  manifest is not being SETTLED by it.** A note saying `superseded` or `probe`
  closes the question; a note saying `unused`, `not-yet-adopted` or `until it is
  composed` is *a deferral written down* and leaves it open.
* **FIXED, NEVER RE-MEASURED.** A fit row that is DIRTY (`rtlCleanAtHead: false`,
  so it never described any committed state exactly) or BEHIND (its
  `sourceCommit` is older than the last commit to the module's own file).
  `zhao_terrain_normals` went from six multipliers to one on 2026-08-24 and the
  database still describes it with a dirty 2026-08-20 row asserting 18 DSP —
  which reads **high**, the flattering direction for "look what we could save"
  and the wrong direction for a budget.

### And a fourth, added 2026-09-09 evening: read the SIBLING contract

The three above are about a thing that already exists. This one is about a thing
about to be built twice, and it is a different failure with a different detector.

`TERRAIN.SHADE` was built — 0 DSP, bit-exact, verified. Then `GEOM.LIGHT`'s
contract was opened, and line 118 said:

> "Writing the oracle as `normal -> ndot -> isqrt -> divide` would be a second
> implementation of the ratified arithmetic — **the exact failure this contract
> was written to prevent, and the one that shipped in September's terrain shade
> header.**"

`normal -> ndot -> root -> divide` is what had just been built. The contract of
the very next block warned against it and named a previous instance of the same
mistake. Both contracts were written the same day, from the same audit, and
**neither said which one owned the arithmetic** — which is exactly how the
projector's two cores came to exist.

**`tools/budget/uncashed_cheques.py` check 3 now catches this class**, and it
catches the projector: `zref::render::project_vertex` is declared as the
`reference_model` of BOTH `GEOM.PROJECT` and `TERRAIN.PROJECT` in
`design/blocks.yml`. **That signal was in the data the whole time and nothing read
it.** The check has two tiers because the strings differ even when the law does
not — `TERRAIN.SHADE` declares `shade_flat_tri_dir_unclamped` while `GEOM.LIGHT`
declares `shade_flat_tri_dir`, its D-1 wrapper, so exact matching reports zero and
looks like it worked.

Note what the tool cannot do, because the boundary matters: checks 1 and 2 find
duplication that is **inherited** — a module built and uninstantiated, a fit row
gone stale. Check 3 finds it **declared**. None of them finds duplication that
nobody wrote down. For that there is only the habit, and the habit is cheaper than
all three: **before building a block, read the contract of every block that
consumes or produces the same quantity.**

### A correction that removes work is the most valuable kind

Same evening, two hours later. The consolidation brief said `zhao_terrain_shade`
"needs a mode so the `face_normal` stage can be bypassed". **There is no
`face_normal` stage.** Its inputs are `n_x_i/n_y_i/n_z_i` — a world normal — with
the face normal computed upstream in `zhao_terrain_normals`. The RTL was already
the shared core, port for port.

So the RTL change was **zero**, and building the proposed bypass would have been a
mode for a stage that does not exist — an authored uncashed cheque inside the
commit meant to remove authored duplication.

Nine claims were checked and refused on 2026-09-09; **three were the author's
own**. The tell is identical every time and it is worth memorising: *"it is
drop-in", "the owner already ruled it", "the measurement says −6", "s1.15", "58
formal assertions"*. **The confident one-line summary is where the error lives**,
and the check is almost always one grep of a file already open.

### Three corollaries, each of which cost something the same day

**Before commissioning a new block, grep the tree for the thing it replaces.**
A projected-vertex arena was designed, built, linted, Quartus-gated, directed-
tested and committed with two fired positive controls — and `zhao_vertex_arena`
already existed, with 58 formal assertions, a committed SymbiYosys proof, and a
shell already composed in `zhao_prod_top`. **Not one of those gates can ask
whether the module needed to exist.** One `ls` would have found it, and its
header hands over the owner ruling, a three-option analysis and the cost number
in its first forty lines.

**When a correction lands, do not grep for the PRODUCER — somebody always fixes
the producer. Grep for every place the corrected value is STORED or FORWARDED.**
`GEOM.DEPTHQUANT` was corrected on 2026-09-03 to consume `w` rather than `1/w`;
`zhao_project_core` and `zhao_geom_project` both grew `out_w_o`. `zhao_geom_wcache`
was dated three days EARLIER and its payload had not been widened, so the replay
cache between the fixed producer and the consumer dropped the field.
Caches, replay buffers and packet layouts are **frozen copies of yesterday's
agreement** and carry no marker saying so. This is the harder variant to see,
because the cheque was PARTIALLY cashed: most links were fixed, so it reads as
done to anyone who checks the producer or the contract.

**THAT CHEQUE WAS CASHED AND THIS PARAGRAPH WENT ON SAYING IT WAS OUTSTANDING,
FOR FOURTEEN DAYS.** Corrected 2026-09-23. `zhao_geom_wcache.sv:43` reads
*"106 bits. WIDENED 75 -> 106 ON 2026-09-09, and the widening is a REPAIR"*, and
`PAYLOAD_W = 106` sits at `:95`. The widening happened the same day this
paragraph was written. It was found because I quoted this file to a lane as a
live defect and the lane went and looked.

**The correction is worth more than the example was.** A stale claim in the
RULES file is worse than a stale claim anywhere else, because this is the file
people quote INSTEAD of measuring — it is read as settled law, it is read by
every lane, and nothing in the tree checks it against the tree. The *rule* above
is sound and stands; its worked example had simply been repaired and nobody came
back to close it, which is this chapter's own subject arriving one level up.

So: **when you fix a thing this file names as broken, fix this file in the same
commit.** And when you are about to cite a defect from here, spend the one grep
first. The "X does not exist" claims this campaign has killed came overwhelmingly
out of DOCUMENTS rather than out of the tree, and a document cannot go stale
loudly.

**A deliberately-failing frontier point is not a saving.** `GEOM.SKIN`'s
`MUL_LANES=1` row shows 3 DSP against the shipping 9, and was quoted as a −6 DSP
lever. The disqualifying fact is **two columns to the right in the same table**:
it delivers 38,965 vertices/frame against the owner-ruled 120,000 — 32%, and the
contract says it "is kept because it fails", with a committed test asserting so.
Somebody built a configuration whose entire purpose is to fail; reading its DSP
column as available money inverts the author's intent. The tell was reading one
column, finding a number shaped like a lever, and stopping.

### And the same disease in prose

`reports/OWNER-DOCUMENT-INDEX.md` lists 33 owner "Agent please read" documents;
**20 have no recorded disposition.** Its own sentence is the law: *"Superseded is
a disposition and should be recorded as one — an unread instruction and a
satisfied instruction look identical from here."* Its origin story is this
chapter's own shape: a bump-mapping request sat unread because it landed at the
repo root where the sweep did not look, and widening the sweep immediately
surfaced three more owner briefs from the same day, 5,653 lines, none indexed.

## A REFUSAL IS AN INSTRUMENT, AND IT GOES BLIND IN THE FLATTERING DIRECTION

Added 2026-09-26, after the owner corrected a sequencing argument and the
correction kept paying out. Four entries in one stretch had been treated as
blocked on a DECISION. **All four were blocked on a BUILD**, and in every case
the decision was already written down, in a document that had been read.

The owner's words were about fits -- *"we still need to close all the things
first, too, otherwise fit isn't complete"* -- but the useful part was the lens.
Applying **"is this a DECISION or a BUILD?"** to every open refusal found:

* **I13's "two unsettled arithmetic laws" are settled and merely unbuilt.** The
  shade ladder is **frozen in the oracle with its own comment naming it** --
  `terrain.cpp`'s `shade_q = (shade + 8191) >> 14;  // the palette ladder
  (0..4)`. The S8.24 bound is **mandated in `spec/qformats.md`**, in the bounds
  column, which reads `saturate`. Three packets refused the WIRING, correctly --
  lay it without these and the pixel is wrong against a capture-exact law while
  every gate passes. But "unsettled" and "unbuilt" are different words and only
  the second was ever true.
* **I55's refusal was ruled on before it was written.** A packet measured that
  the swap is circular and 7.3x more expensive, and named what would close it.
  Directive section 4 already said *"a parallel legacy on-chip frame arena that
  still supplies the actual pixels is not closure"* -- the same shape, decided.
* **I56's packet declined an ABI change** on the sound ground that a field whose
  consumer does not exist is an uncashed cheque. Directive section 5 says the
  architect *"is not required to duplicate a large policy engine merely to avoid
  adding a command or mailbox field"* and to *"authorize the necessary generated
  command ... and connect its real producer and consumer."* **The owner had
  pre-authorised the exact thing being avoided.**
* **And an ESCALATION of mine over-asked on both its questions.** One was decided
  in the directive three days before I wrote it. The other had a default the
  escalation itself declared -- *"what I will start if you say nothing"* -- under
  a standing vacation directive where **saying nothing was always going to be the
  state.** A default nobody executes is not a default; it is a second escalation
  wearing a decision's clothes.

**This is the broken-instrument law applied to JUDGEMENT rather than to tools.**
That law says a defect survives when it makes the answer look better, smaller or
simpler, because **nobody audits good news**. This is the same asymmetry one
level up: **refusing, deferring and escalating all FEEL like the careful,
conservative act**, so they are never audited the way a build is. A refusal gets
written into an entry, quoted respectfully by the next packet, and inherited --
and a question that has already been answered can sit in a docket for weeks
looking exactly like diligence.

Note the direction, because it is the tell. A wrong BUILD gets caught by a gate,
a bench or a fit. A wrong REFUSAL is caught by nothing at all -- it produces no
output to be wrong, no counter to read zero, no red. **It is invisible to every
instrument in the tree by construction.**

**The rules that follow:**

1. **Ask DECISION or BUILD of every refusal you inherit, including your own.**
   "A law must be settled before a wire is laid" is right; "this law is
   unsettled" is a claim about the tree, and it is checkable.
2. **Before escalating, grep the standing directive for the entry's own name.**
   One command. For I34 it returns the decision, the destinations, the exact
   addresses and the numerical policy.
3. **A refusal is only as good as its scope.** Every one of the four above was
   CORRECT about what it refused and WRONG about what that implied. A packet
   rightly refusing to lay a wire does not thereby establish that the missing
   law is open, and that second, larger claim is the one that gets inherited.
4. **If you write a default, execute it.** "This is what I will do if nobody
   objects" in a document nobody is reading is not a plan.
5. **Re-read the refusals whenever the owner corrects your direction.** The
   correction rarely only means what it says; it usually means a habit is off.

And the general form, which is this file's own subject: **the campaign's rules
about false absences apply to documents written by this campaign.** Four of the
five findings above are corrections to briefs, entries and escalations written
here, three of them mine in the same week. **A document cannot go stale loudly.**

## Instructions are not delivered until they are read

Owner direction was posted four times because it kept not reaching the working
agent, then relayed into a run folder that had already closed, and five passes
solved the wrong problem in the meantime. **A run folder is the wrong home for
anything durable** — every pass creates a new one, so a file left in the current
run is orphaned by the next. Durable direction belongs beside the creature it
governs. And before starting any creature run, read every `OWNER-DIRECTION-*.md`
in the creature folder and check `reports/` for anything newer than the last run.

## Naming files in `git add` is not enough when two agents share one file

Added 2026-09-19. Two packets ran concurrently, each told to commit only its own
files. One ran `git add fpga/rtl/prod/zhao_console_core.sv` -- **its own file, by
name, exactly as instructed** -- and swept in the other packet's uncommitted
edits to that same file. The commit briefly could not elaborate, because it
referenced a module the other agent had not yet committed.

Naming the path is not the same as staging your change. `git add <file>` stages
**whatever is in the working tree**, including work you did not write and cannot
see.

* For a file only you touch, `git add <file>` is fine.
* For a SHARED file -- the composer top, `tests/CMakeLists.txt`,
  `design/fit_targets.yml` -- stage the hunk, not the file. `git add -p` is not
  available in this environment; two workers solved it independently with
  `git update-index --cacheinfo` against a blob built from `HEAD` plus their own
  edits, and with a marker-selected patch. Either is acceptable; silently
  committing a file you share is not.
* The tell is a commit whose diff contains lines you did not write. Look before
  pushing, not after somebody reports a broken revision.

This is the live-tree trap one level up: the fit snapshots its sources, so a
running fit is safe, and **the index does not snapshot anything**.

### And the same cause in the other direction: `git checkout --` DISCARDS

Added 2026-09-19, hours after the rule above, by the agent who wrote it.

The section above warns that `git add <file>` sweeps in work you did not write.
The inverse is worse. `git checkout -- <file>` **discards** everything in that
file that is not staged, including another agent's uncommitted work, and
**unstaged changes have no reflog** -- there is nothing to recover from.

I reverted my own abandoned edit to `zhao_console_core.sv` while another packet
had ~186 uncommitted lines in it. Its composition hunk was gone. It re-applied
from its own context and committed immediately, so nothing was lost permanently
-- but only because the author was still running and still remembered.

**A bad commit is recoverable. A discarded working tree is not.**

### And three more ways the shared index bites, all on the same day

Added 2026-09-19, after four concurrent packets. The two rules above are
necessary and were not sufficient; each of these cost someone real work.

**`git commit` with no pathspec commits the WHOLE INDEX, not what you just
added.** I ran `git add tools/budget/completion_register.py` and then a plain
`git commit`, and the commit carried another agent's already-staged CMD.EXEC
packet — five files, under my message, describing something else entirely.
Nothing was lost, but the authorship record is wrong forever and the agent's
next commit reported "nothing to commit", which reads exactly like a failure.
**Check `git diff --cached --name-only` before every commit**, not just
`git status`.

**`git commit --only <path>` is NOT the fix, and is wrong in the other
direction.** `-o` commits the **working-tree** contents of the named paths, so
on a shared file it sweeps in exactly the uncommitted edits you were trying to
avoid. It is correct only for files you exclusively own. A worker caught this
before using it; I had already recommended it to two others and had to correct
myself.

**What actually works** is a private index:

```powershell
$env:GIT_INDEX_FILE = "$env:TEMP\mine.idx"
git read-tree HEAD
git apply --cached my-hunks.patch
git commit -F msg.txt
Remove-Item env:GIT_INDEX_FILE
```

This writes **zero** entries to the shared index, so no other agent's staging is
disturbed in either direction. Its own gotcha, found the same day: `commit-tree`
and a private-index commit advance HEAD **without touching the shared index**,
so paths your commit changed then read as staged **reverts** in everyone else's
`git status`. Neutralise them (`git reset -q HEAD -- <paths>`, which touches the
index only) or the next agent will commit a revert of your work believing it is
cleanup.

#### `read-tree` and `commit` MUST BE ATOMIC — the recipe above is not enough

Added 2026-09-19, after the recipe **reverted a whole packet in full**, 529
deletions, having already been written down.

`git read-tree HEAD` snapshots HEAD *at that moment*. Run it in one tool call and
`git commit` in another, and any packet that commits in between makes your index
describe a **tree that is now the past** — so your commit silently reverts
everything they landed.

Every local signal said it was fine. `git apply --cached` returned 0. The staged
diff was exactly the author's own hunks. **The only tell was the commit summary
reporting FOUR files changed when one had been staged.** Read that line.

So either do the whole sequence in a single invocation, or re-check immediately
before committing:

```powershell
$before = git rev-parse HEAD
# ... read-tree, apply --cached, verify ...
if ((git rev-parse HEAD) -ne $before) { throw "HEAD moved; re-seed the index" }
git commit -F msg.txt
```

**A private index protects another agent's INDEX. It does nothing whatever for
their COMMITS.** That is the distinction the first version of this section
missed, and it cost a packet twice in one day — once to me, once to a worker
following the recipe exactly as written.

The general form, which is the thing to remember when the next variant of this
appears: **any read-modify-write against a repository other agents are
committing to is a race, and git gives you no lock.** Compare HEAD before and
after, or expect to write somebody's work out of history.

### Lint the BLOB you are about to commit, not the working tree

Same day, same cause. Two packets built a staged blob that was broken in ways
the working tree never showed: a `-U0` patch placed new ports **after** the
module's `);`, and an `endmodule` end-marker swallowed another packet's whole
instantiation. Both were caught only because the author checked out the staged
blob and linted *that*. `git show :<path>` or `git archive HEAD` gives you the
thing that will actually be compiled by whoever pulls next.

**The tree you test is not always the tree you ship.**

### The scratchpad is shared between concurrent agents

Two agents independently wrote `stage_mine.py` to the same scratchpad path and
one silently overwrote the other. The directory is per-session, not per-agent.
**Prefix every temp file with something unique to your packet.**

So before `git checkout -- <shared file>`:

* run `git diff <file>` and read it. If it contains lines you did not write,
  you are about to delete somebody's work.
* revert your own hunk instead — apply the reverse patch, or rebuild the file
  from `HEAD` plus the hunks that are yours.
* and tell the other agent immediately if you do destroy something. They can
  re-apply from context while they are still running; after they finish, the
  work is gone and nobody knows what it was.

### `[IO.File]` IGNORES `cd`, AND ITS WRITES LAND IN ANOTHER AGENT'S TREE

Added 2026-09-20 by the terrain8 packet, which did exactly this and caught it
only because `git status` in its own worktree came back **clean after a write
that reported success**.

`[Environment]::CurrentDirectory` — which every `System.IO` call resolves a
relative path against — is set once when the shell starts and **is never
updated by `cd` or `Set-Location`.** In an agent session it therefore stays at
the PRIMARY working directory for the whole run. Measured, in a worktree,
after an explicit `cd`:

```
PS $PWD                              = ...\gz-terrain8          <- mine
[Environment]::CurrentDirectory      = ...\zhaozhou-ceiling-lane-20260912
[IO.Path]::GetFullPath('tests\x.sv') = ...\zhaozhou-ceiling-lane-20260912\tests\x.sv
python -c "print(os.getcwd())"       = ...\gz-terrain8          <- mine
```

So **native children (`python`, `git`, `cmake`, `verilator`) get `$PWD` and are
fine; .NET file APIs do not.** That split is the whole trap: every gate you run
reads the right tree, and the one file you write goes somewhere else.

The consequences are the worst available. The write **succeeds**, silently, into
the coordinator's checkout — a tree this protocol forbids you to touch, that
other packets are gating against, and where it reads as an uncommitted edit
somebody else may `git add` or `git checkout --` away. Nothing in the gate set
can see it, because the gates all measure your own tree, correctly.

And note WHERE it comes from: PACKET-PROTOCOL.md says *"Read/write files with
`[IO.File]::ReadAllText/WriteAllText`, not `Get-Content | -join`."* That advice
is right — and followed literally, with a relative path, it aims every packet's
writes at the coordinator. A rule written down without the trap that comes with
it is this file's most repeated shape.

**So: pass `[IO.File]` an ABSOLUTE path, always.** Never a relative one, never
one built from `$PWD` implicitly. Belt and braces, at the top of any script
that writes: `[Environment]::CurrentDirectory = $PWD.Path`.

**And the tell is a clean `git status` after a successful write.** If a write
reported success and the file is not modified, it was written — look for where.
`git -C <the other checkout> status --porcelain` finds it. To undo it, build the
reverse patch (`git diff -- <file> | git apply --reverse`) rather than
`git checkout --`: the reverse patch REFUSES if the file moved underneath you,
and `checkout --` would take another agent's uncommitted work with it.

## Stopping an agent does not stop its background work

A stop instruction was sent and obeyed, and a build it had already launched ran to
completion anyway. **Kill the background tasks too**, then verify nothing is
running before assuming a lane is closed.

### And killing a lane's watcher WITHOUT TELLING IT makes the lane replace it

Added 2026-09-25, after **seven** `until … sleep` pollers accumulated on one
Quartus fit. The coordinator stopped six. The lane's own account of why there
were seven:

> *"I read every `Task … was stopped by main session` notification as **my watch
> has lapsed** and re-armed on each one. They were you clearing duplicates. So my
> correction loop was feeding the exact problem it was reacting to."*

**Four of the seven were created AFTER the cleanup started.** A stop looks
identical, from inside the lane, to a watcher that died on its own — so a
conscientious lane re-arms, and the tidier the coordinator is the faster they
breed.

**So stop the task and say so in the same action**, naming which one survives.
And two things the lane noticed that generalise beyond watchers:

* **A poll loop whose only terminal condition is "the process vanished" cannot
  report WEDGED.** A wedge and a long placement look identical to it — and
  identical to seven of it. *"The watchers were never carrying information I
  lacked"*: `tasklist | grep -i quartus` answered the same question directly,
  every time, and so does reading the process's CPU. **11,974 CPU seconds against
  74 minutes wall is ~2.7 cores sustained — that is placement working**, and it
  is a distinction no `until` loop can make.
* **Prefer one watcher plus a direct process read** to any number of watchers.

## Seeing the work properly

Judging an animation from a handful of evenly-spaced stills does not work —
uniform sampling finds the typical frame and misses the broken one. Prefer:
contact sheets of every frame; trajectory plots of tracked points over time
(a flat line IS "it never bobs"); sampling frames by *badness* rather than by
index; fixed orthographic diagnostic cameras; and before/after pairs so
regressions are visible.

## Ground contact

Clipping through the ground must be **authored, never accidental**. A belly
resting at exactly zero reads as hovering, and a spear that stops at the surface
reads weightless — so deliberate, declared penetration is correct and its
*absence* is also a bug. Each clip declares where, when and how deep; anything
outside that is the fault.

**Measure it with a COMMITTED 3D pose probe, not from the rendered frame.**
The obvious shortcut -- find the terrain in the image and count creature pixels
below it -- is unsound and will report a confident, wrong number: with a ground
plane receding in perspective, an animal STANDING on the ground is always below
the horizon, and one 2D frame cannot separate "in front of the dirt" from
"inside it". It reported 94.8% submerged on a clip that was barely touching.
Real penetration means walking every posed vertex of every clip frame against
terrain height. A probe that does this was written once and thrown away, so its
numbers are unreproducible -- commit the probe.

---

## START HERE IF YOU ARE PICKING UP THE CONSOLE COMPLETION CAMPAIGN

**`reports/HANDOVER-20260919.md`** — read it before touching anything. It carries
the current gap count, what the console does and does not do, every gate and what
its output means, the environment and git traps that cost hours, the fourteen
"X does not exist" claims that turned out false, the eleven decisions the console
is waiting on, and the exact `/goal` prompt to write.

It is meant to be UPDATED IN PLACE at the end of each session, never duplicated.
Two handovers is the same failure as two sources of truth.

## Process

* **Every session is a RUN**, created with `zhaozhou/runs/CLAUDE-RUNS/init-run.ps1`,
  logged as it happens, archived when done. **Every creature gets its own run.**
* **Commit and push as the work happens**, not batched at the end.
* **Barge ahead.** Decide on your own judgement rather than stopping to ask;
  state the assumption, make it cheap to reverse, keep moving. Blocking costs a
  working session; a wrong call costs one edit. This does not extend to
  destructive or outward-facing actions.
* **Publishing is always an explicit call** — with ONE standing exception.
  `Upheaval/website/deploy.ps1` requires `-Project` and `-Branch` and refuses a
  page without `noindex`.
  * **The bestiary is authorised to publish on every FINISHED CREATURE PASS**
    (Fabian, 2026-08-27: *"publish on every finished zixxtrixx, not every
    individual change"*). Do not stop to ask — but the trigger is **a pass that
    is done and worth looking at**, not a file save. A tweaked constant, a
    half-fixed head, a re-render mid-iteration: not a publish. The complete
    reworked creature with its clips encoded: publish, immediately, via
    `deploy.ps1 -Project upheaval -Branch main`.
  * Site-structure work (a card layout, an archive tab) ships with the next
    creature pass rather than on its own, unless Fabian asked for that change
    specifically — then it goes up when it is done.
  * The exception is that site and nothing else; it does not generalise to other
    outward-facing actions, and the page stays `noindex` — unlisted, for the
    owner, not public.
  * **`-Branch` is mandatory.** Wrangler accepts a missing branch, silently
    demotes the deploy to a PREVIEW, and production keeps serving the old build.

## Build note

`cmake --build` intermittently loses a race regenerating `build.ninja` against
Verilator, and the shell then runs the **stale binary and reports the old
numbers**. A measurement that did not move after a change that must have moved
it is the tell. Compile the reel directly instead, and after any struct-layout
change **recompile every `.cpp` that uses it** — a stale object with an old
layout looks exactly like a rendering bug.

**`git stash push` / `pop` does NOT reliably trigger a ninja rebuild, and the
tell is `ninja: no work to do`.** Added 2026-09-16, after it produced two false
readings in one investigation. Stashing an RTL file to measure the baseline,
building, measuring, then popping and building again printed "no work to do"
and ran the **stashed** binary — so the change under test and its baseline
reported byte-identical numbers, which is the one thing they could not honestly
do. It reads as "the change has no effect", the flattering direction.

Two habits kill it. **Never send a build's output to `Out-Null`** — a build
wrapped in a helper function that discards its output cannot be seen to have
failed or to have done nothing. And **assert the tree is what you think before
measuring**: one `Select-String` counting an identifier the change introduces
(`vs2_valid`, 11 hits with it, 0 without) is a second's work and turns the
stale-binary trap from a silent wrong number into a loud one. If ninja still
says "no work to do", force it with
`(Get-Item <file>).LastWriteTime = Get-Date`.

**`Copy-Item` is a second producer of the same trap, and it bites on the PUT
IT BACK half of a fire test.** Added 2026-09-20. `Copy-Item` carries the
SOURCE's `LastWriteTime` across, so restoring a backup taken before a
deliberate mutation leaves the restored file OLDER than the objects built from
the mutant. Ninja says "no work to do", the "restored" run reports the
MUTANT's failures, and the obvious reading is that the repair did not work —
which is the flattering direction for *"my fire test was real"* and the wrong
one for everything else. Measured here restoring `zhao_field_host.sv` after
proving R101's guard fires. The tell and the fix are the ones above; what is
new is that a *restore* looks nothing like an edit, so nobody thinks to check.
Verify the content, not the copy: `RESTORED_OK` on a `.Contains()` of the line
you put back, then force the timestamp.

**Configure from PowerShell with `tools/env/zhao-env.ps1` sourced, always.**
`CMakePresets.json` gates `windows-base` on `${hostSystemName} equals Windows`.
Run `cmake` from Git Bash and it resolves to the MSYS cmake, which reports a
non-Windows host, so the preset is **disabled** — *"Could not use disabled
preset windows-native"*. Configure without the preset instead and
`CMAKE_CXX_COMPILER` comes back as the string `C`. The preset's own `ZHAO_NOTE`
says this and warns about "the broken devkitPro msys2 cmake"; it still cost an
hour on 2026-09-04 because the symptom looks like a corrupt build tree rather
than a wrong shell.

**`ctest` needs `tools/env/zhao-env.ps1` SOURCED too, and without it the suite
hangs silently.** Added 2026-09-16. The rule above is written about `cmake`;
the same shell requirement applies to the test runner and nothing said so. A
`ctest` launched from a shell that had only `cd`'d to the repo sat at 28 of 915
tests for minutes. Its two children were `verilator_bin.exe` and they had
consumed **0.00 CPU seconds in three minutes** — alive, blocked, doing no work.
Re-running the identical command with the environment sourced, the same
children showed 8.8 CPU seconds each within a minute.

Two diagnostic traps came with it, and both pointed at the wrong answer first:

* **Low ctest CPU means nothing.** ctest forks the work, so its own CPU sits
  near zero however healthy the run is. The wedge signature above is "frozen
  CPU *and no child test process*" — a conjunction, and quoting it while
  measuring only the half that is always true reads as confirmation.
* **`Get-Process | Where ProcessName -like "test_*"` cannot see the children
  that matter.** They are `verilator_bin.exe` and `cmake.exe`. That filter
  returns nothing on a perfectly healthy suite, which manufactures the second
  half of the signature. Use
  `Get-CimInstance Win32_Process -Filter "ParentProcessId=<pid>"`, which names
  the children *and* lets you read their CPU.

**The tell worth memorising is ALIVE AT ZERO CPU, measured on the CHILD.** A
slow test burns CPU. A ctest wedged on `Testing/Temporary` debris burns none
and has no children. This burns none *while holding children that also burn
none* — a blocked child, not a stuck parent. It is consistent with the
launcher-popup trap (a GUI dialog waits forever at zero CPU and is invisible in
a non-interactive session), but only the fix was confirmed, not the mechanism.

On 2026-09-16 this was misdiagnosed twice before it was measured: first as the
documented concurrent-ctest wedge — a real rule that had genuinely been broken
in the same session, which is what made it so easy to believe — and then as
unresolved. **The explanation that blames something you already did wrong
arrives first and explains almost all of the evidence**, which is this file's
own law about diagnoses landing soft, wearing the clothes of an honest mea
culpa.

**A suite reads the LIVE TREE, so editing RTL while one runs makes its answer
worthless — and the failures look real.** Added 2026-09-18. A full `ctest -L
fast` was launched, then three RTL files were edited while it ran; it reported
`lint_shell_top_v2`, `lint_shell_v2_lease_path` and `lint_shell_paired_diff`
red. Linting the same top by hand, a minute later, gave **0 errors**. The reds
were the lint targets reading half-written files.

This is the exact converse of the rule above about not building into a tree a
suite is reading, and it is easy to miss because the two feel like one rule
about concurrency when they are two rules about *who is writing*. A fit is
safe to run alongside edits — `run_block_fit.ps1` snapshots its sources. A
SUITE is not, because nothing snapshots for it.

The tell is a red in a target you did not touch, on a file you did. Do not
debug it: stop the suite, clean `Testing/Temporary` (see above), finish the
edit, and re-run. A suite whose inputs moved underneath it is not evidence in
either direction — the greens are worth no more than the reds.

**When `build.ninja` is stale it can be unable to regenerate itself.** One
verilate rule declared `Vtb_perspuv_pair.cmake` among its outputs while running
`--make json`, which writes the `.json` that is actually there — so a
`copy_if_different` of a file nothing writes failed on every build, and because
that rule is part of `build.ninja`'s own regeneration, **ninja could not rebuild
the graph that would have fixed it**. 255 of 256 verilate directories had their
`.cmake`; the one that did not was the one holding everything. The fix is to
regenerate through `cmake --preset`, never through another `cmake --build`.

**Read the build's exit code, not the pipeline's.** `cmake --build ... | tail`
reports `tail`'s status. A build that failed on step 18 of 554 printed
`BUILD_RC=0` and was believed. This is the stale-binary trap wearing a new
costume: the shell told the truth about the wrong thing.

**`std::ofstream` faults at -O1 on this toolchain; use C stdio.** Measured
2026-09-05 writing the desktop host: an `ofstream` write crashed with an access
violation at `-O1` and `-O2`, and worked at `-O0`. Same for `ifstream` on read.
It was bisected by flushing stdout after every step -- the summary printed, the
byte count printed, and the process died inside the stream write. **Buffered
output lost in a crash makes a late fault look like an early one**: with no
flushes it presented as "no output at all", which points at start-up rather
than at the last thing the program does. `fopen`/`fread`/`fwrite` have no such
problem and the codebase already uses `std::printf` everywhere.

**Regenerate `zhao_prod_top.sv` after ANY port change.** It is generated by
`tools/quartus/gen_prod_top.py` and instantiates every production block by
name, so a new port that nobody connects is a `PINMISSING` that only the next
fit discovers. On 2026-09-04 it was found stale for two separate port changes
made days apart. A generated file that nobody regenerates is a stale file with
a reassuring provenance line at the top. `tools/quartus/check_prod_manifest.py`
now also checks that everything the top instantiates is in the production fit's
source list — registering a block in the ledger, the manifest and that list are
three different acts.

**A source list naming the file while Verilator says `MODMISSING` means the
GRAPH is stale, not the list.** Second instance of the trap above, 2026-09-07,
with a different tell: `zhao_raster_rcp24_v3.sv` instantiated
`zhao_raster_ticketq_rh`, the file was already in that test's `SOURCES`, and the
build still could not find the module — `build.ninja` predated the line and
could not regenerate itself because the failing rule is part of its own
regeneration. The instinct is to add the file again, which is a no-op followed
by confusion. Regenerate through `cmake --preset`.

**An `undefined reference to ..._nba_comb__TOP__...` is a STALE VERILATOR
PARTITION, and purging one target at a time is the expensive way to fix it.**
Added 2026-09-16, after it cost three consecutive full builds. Verilator splits
a large module into numbered partitions (`__pi2`, `__0`, `__3`) whose names
depend on how the design elaborated. Change a parent — here
`zhao_texture_island_v3_top`, which re-partitioned its `zhao_texture_v3own`
instance — and the regenerated sources call functions the leftover object files
never defined. The link fails; the compile never does.

The tell is that the missing symbol is a Verilator-internal name with a
partition index in it, not anything anyone wrote.

`cmake --preset` alone does NOT fix it. It regenerates the graph and leaves the
stale `.obj` files in place, so the next build fails on the NEXT target sharing
the same partition — one failure per build, forever. Delete every partition
directory for the changed module across the tree at once:

```powershell
Get-ChildItem build\tests -Recurse -Directory -Filter "V<module>.dir" |
  ForEach-Object { Remove-Item -Recurse -Force $_.FullName }
cmake --preset windows-native
```

Note each target holds TWO such directories (one nested under its own
`CMakeFiles/<target>.dir/`), and that other build trees — `build/packet-e` here
— have their own copies that are independent and need the same treatment only
if they are also being built.
