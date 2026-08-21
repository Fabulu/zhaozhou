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

## Limitations

- Everything is Verilator simulation. No synthesis, no fit, no board.
- Sequencer dispatch reaches the arithmetic core only at `f719dda`; the five
  `FIELD.SEQ.*` profile blocks stay SPECIFIED until the remaining op blocks are
  wired to the walk.
- `DEBUG.FRAMEBLIT` steps 5-8 remain blocked on the owner's decision in
  `reports/BLIT_INTEGRATION_PHASE_SHIFT.md`.
