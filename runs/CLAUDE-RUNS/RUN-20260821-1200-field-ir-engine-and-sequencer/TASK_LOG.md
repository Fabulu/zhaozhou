# RUN-20260821-1200 — the Field IR engine, the sequencer, and a gate that lied

Owner asks handled in this run: finish the Field IR engine and its sequencer,
fix the CI the owner reported broken, and file four content dumps into the
repo. One agent policy observed: **no subagents were used.**

Everything in this run is SIMULATION. Nothing has run on a board.

## State inherited

`main` at `0b8e43a`, with every Field IR *operation* built but no sequencer to
run them, and `DEBUG.FRAMEBLIT` step 4 parked on a branch pending an owner
decision (`reports/BLIT_INTEGRATION_PHASE_SHIFT.md`).

**CI was red on every push and had been for some time.** The local suite was
green at the same moment. Both readings were accurate, which is the part worth
recording.

## Work

### `f719dda` — `FIELD.SEQ.CORE`, the sequencer (block 92)

The register file and the instruction walk: zero the file, load the declared
input lanes, walk until `OP_END`, read the output lanes. 64x32 flops, three
combinational read ports walked over three states, six clocks per instruction.

Flops rather than M10K because a 64-entry file with three read ports does not
map onto a block RAM without duplicating it. One writer at a time: the walk
owns the file while busy, the host owns it otherwise, so there is no
arbitration to get wrong and no bypass network to need.

The block does not validate. `zfield::interpret` runs only on decoded programs
and its `default:` is `__builtin_unreachable()`; the decoder is the validator.
The one thing checked is not semantic — `instr_count_i` bounds the walk so a
mis-loaded instruction memory reports `ST_PC_OVERRUN` instead of hanging.

Evidence: 102 directed + ~1,850 random against `zfield::interpret` itself.
Mutation sweep **19 / 17 caught / 2 recorded equivalent / 0 discarded.**

The two equivalents are a redundant PAIR, not dead code: the ALU clears
`writes_o` for exactly `OP_END` and an unsupported op, so each guard is
redundant alone, while `both_write_guards_removed` is caught. That mutation
only became visible once a test named register 0 as an output — `OP_END`'s
`dst` is zero and no earlier program read register 0 back.

#### The defect it found in a block already signed off

`zhao_field_alu`'s `OP_ABS` returned `INT32_MIN` for `abs(INT32_MIN)`.
`spec/qformats.md` §3.7 is explicit: `abs(0x80000000) = 0x7FFFFFFF + SAT`.

The reason it survived weeks of testing is the point. The ALU's own test did
not ask the reference — it **restated** the rule, restated it the same wrong
way, and asserted the wrong value, under a comment warning that "the oracle and
the RTL could agree and both be wrong about what the reference does with this
one input". They did. The actual law was outvoted two to one.

Fixed, given the `sat_rescale_o` lane it was missing, and its test now asks the
shipped interpreter.

**A wrong restatement agrees with a wrong implementation forever.**

### `8264534` — the format gate that skipped locally and ran on CI

`format_check` needs clang-format, which oss-cad-suite does not ship. On a
machine without it the test reported SKIPPED and the suite went green; on the
runner it ran and went red. Thirty-five files had drifted behind that skip,
several from before this week.

**The fix is not to fail when the tool is missing — it is to make the tool
present.** `clang-format` is now a devDependency pinned to the same 1.8.0
(LLVM 15) the CI job installs, and `tests/CMakeLists.txt` searches that
`node_modules` path FIRST. `npm install` puts the authoritative binary on any
machine that builds this repo.

The version pin is why the pinned path is searched first. A system
clang-format from a different LLVM major reformats the same file differently,
so "clean locally" against an unpinned binary is not evidence about CI at all
— which is the real reason the skip existed.

Checked rather than assumed: with all whitespace removed, 33 of the 34 swept
files are byte-identical to their previous versions. The 34th
(`tests/measure/measure_tokens_binner.cpp`) differs only in the ORDER of four
`using` declarations, which is inert at namespace scope. `git diff -w` is NOT
sufficient evidence here — it reported 1,072 changed lines on a change that
altered no code, because reflowing moves tokens between lines.

CI job "format + static analysis (charter 27)": **green.**

### `0cc6c28` — the owner docket and three filed notes

`docs/OWNER_DOCKET.md` is new. It exists because the accumulated feature asks
lived in run QUEUE files and an agent's memory, so the only complete list was
outside the repo. Newest first; every entry is an ask, not a decision.

Today's entry: terrain rotated at arbitrary angles, and rotated in real time
("so a skyscraper suddenly falls over"). Static rotation is one format field —
islands carry translation only, and rotating the ISLAND rather than the patch
is what preserves the one-solid-interval column law. Real-time rotation leans
on `GEOM.LOOM`'s existing terrain-patch transform parenting, so the renderer is
not the new part. Four questions recorded for the owner rather than answered:
world-space column walking after a tilt, what the deep keel does when it stops
pointing down, the static-to-dynamic hand-off frame, and where the pivot comes
from.

Also filed: `reports/RASTER_Polygon_Budget_Proposal.md` (deferred behind
FRAMEBLIT steps 4-8 and the composed fit, per the owner),
`docs/CREATURE_ANIMATION_APPROACH.md`, `docs/PROJECT_ODDS_AND_SCOPE.md`.

### `fcf7e79` — this run log

### `1250b59` — the three notes, refiled from the full text

The owner re-sent the three content dumps and asked whether they had been put
where they needed to go. They had — but the first filing was made from a
TRUNCATED view, and the result was three faithful-sounding paraphrases with the
information removed.

Restored: the six-projects list and the whole odds table (70-90% vertical
slice, 50-70% FPGA demo, 40-60% sellable short game, 10-25% full campaign,
single digits for all of it plus a physical console); every cost figure in the
raster proposal (92,160 pixels at 240p, 4.5 visible pixels per authored
triangle, 5+16+0-16 = 21-37 clocks per triangle x tile job, a 128-triangle /
1,024-reference binner at 2.83 clocks per reference, a TMU at one sample per
4 and 6 clocks), plus its authoring-tier table, its 53-creature battle budget
and its nine-step order; and in the animation note the owner's own two-modes
framing, the six hardware-lowering options, and the "armies share states"
insight.

**A paraphrase that sounds right is not the thing it paraphrases.** Dropping
the numbers dropped the content, and the same lesson as the `abs` defect
applies: a restatement can be wrong in ways that read as correct.

### `290814a` — D5's 338k figure marked superseded

`reports/status/phase2_wave2.md` declares 60 Hz full-canvas cadence infeasible
and builds it on a measured ~338k gpu cycles per Duo blit. The FRAMEBLIT
redesign streams fetch and commit instead of serialising them and measures ~58k
cheaper, so that headline is no longer the machine's number.

The CONCLUSION is unchanged and says so: the commit phase still dominates
against a 318,592-cycle frame, and the Z60 raw-demand bullet is a bandwidth
proof no blit redesign touches. The marker does not pre-empt the open decision
in `reports/BLIT_INTEGRATION_PHASE_SHIFT.md`; it exists so nobody reads 338k as
current while that decision waits.

### `d9a48ea` (zhaozhou-site) — update.ps1 passes --branch

`deploy.ps1` was fixed for this and `update.ps1` was not, leaving the more
dangerous path broken: update.ps1 is the script that regenerates every render,
so a full refresh through it landed as a Cloudflare PREVIEW while the public
URL kept serving the old page. Wrangler exits 0 and prints "Deployment
complete" either way.

The site directory was not a git repository when the bug first bit, which is
why wrangler had nothing to infer. It is one now (2026-08-20) and sits on
`main` — making the inference correct today and silently wrong the first time
anyone works on a branch. deploy.ps1's comment still asserts the old fact; the
new note says so rather than repeating it.

## The verification harness lied twice, in two new ways

Recorded because the sweep is the evidence every RTL claim in this run rests
on, and it was weaker than it looked.

**1. A failed apply was counted as a result.** Python on Windows printed the
mutation names with CRLF; command substitution strips only the trailing
newline, so 23 of 24 names reached `apply` with a `` attached and raised
KeyError. The script printed APPLY-FAILED, **continued**, and reported
`caught=0 survived=1 discarded=0` with exit code 0.

An apply that fails is not "survived" and not "caught". It is no evidence, and
counting it as an outcome makes the total a lie. The sweep now aborts on a
failed apply, and cross-checks `attempted == expected == accounted` before it
will report at all.

**2. THE DISCARD GUARD COULD NEVER FIRE.** This is the more serious one. The
guard exists to catch Verilator serving a cached model instead of re-elaborating
the mutated RTL, and it did that by checking the test BINARY's hash changed.

Two builds from byte-identical source produced `440d4a24...` and `8b57af01...`
— **the linked binary is not reproducible.** So "hash differs from pristine"
was unconditionally true and the discard branch was unreachable. Every
"0 discarded" reported in this run before now, including on the sequencer
commit `f719dda`, rested on a check that could not fail.

The generated Verilator model IS reproducible: the same two builds both
produced `5972223c...`. It is a pure function of the RTL, which is the property
the guard needed. The guard now hashes all 13 generated model sources, and was
tested in both directions before being trusted — mutate without rebuilding and
the hash is identical (discard fires); mutate and rebuild and it differs.

**3. A preflight, because the abort came too late.** The hardened sweep aborted
correctly on an ambiguous revert — after 13 mutations and forty minutes of
rebuilds. Every mutation is now proved to apply once, change something, and
revert byte-identically before any build runs. It is a two-second check.

## Limitations

- Everything is Verilator simulation. No synthesis, no fit, no board.
- Sequencer dispatch reaches the arithmetic core only at `f719dda`; the five
  `FIELD.SEQ.*` profile blocks stay SPECIFIED until the remaining op blocks are
  wired to the walk.
- `DEBUG.FRAMEBLIT` steps 5-8 remain blocked on the owner's decision in
  `reports/BLIT_INTEGRATION_PHASE_SHIFT.md`.
