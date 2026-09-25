# The inherited matrix's historic floors were already broken

Found by running the pass-25 matrix carried forward in full. Two legs went red
on subjects **pass 26 never touched**:

```
FAIL e-p24off 22-subject bank identical to crcs-ship.txt (rows=22)
FAIL e-p23off 22-subject bank identical to baseline-pass23-crcs.txt (rows=22)
```

and the diff is **hover and inspect only** -- not hasty.

## It is not pass 26's

Chased in four steps, each ruling out one explanation rather than arguing:

1. **Does the hurry knob leak into hover?** Rendered hover plain and with
   `ZHAO_U02_HASTY_HURRY=off` on the pass-26 binary: **0 of 600 frames differ.**
   It does not.
2. **Do the two binaries disagree under the floor?** Rendered hover under the
   full p24off floor on the **pass-25 reference binary** (git `f66d107c`,
   detached worktree) and on the pass-26 binary: **0 of 600 frames differ.**
   Pass 26 changes nothing here.
3. **Is the renderer non-deterministic for the orbiting subjects?** Hover twice
   from the same binary: `0x89EBA648` both times, **0 frames differ.** It is not.
4. **So the pass-25 binary itself misses the receipt.** Under the floor it
   renders hover `0xB7BE913A` against the receipt's `0x8EDC6DE3`, and inspect
   `0x9B6A2825` against `0xA7972F35` -- while **crackle matches exactly**
   (`0x7891EF60`).

## The cause, and the proof

Crackle matching is the tell. `kBackBallDampClipPm` is **700 on slot 0 and 0 on
slot 23**, and *"slot 0 is two subjects: hover AND inspect, one bake under two
names"*. The owner declined the slot-23 offer, so crackle carries no damping and
is unaffected; hover and inspect carry all of it.

**The floor ladders have no back-ball off-flag.** `P24OFF`, `EYEOFF` and
`CALMOFF` were written before the back-ball packet existed:

* the pass-25 matrix receipt was committed at `82218e2a`;
* the back-ball packet landed **after** it, at `af8c97c1` / `b3c5760e`,
  changing `manafold_art.h` and `manafold_clips.h` and adding 849 lines of
  `manafold_backball.cpp`.

`git diff --stat 82218e2a f66d107c -- tools/reel/` shows 1,134 insertions after
the matrix that certified the floors.

Proof, on the **pass-25 reference binary**, floor plus
`ZHAO_U02_BACKBALL_DAMP_CLIP_PM=0:0`:

```
manafold-hover:   600 frames, sequence_crc32c=0x8EDC6DE3   <- receipt: 0x8EDC6DE3
manafold-inspect: 600 frames, sequence_crc32c=0xA7972F35   <- receipt: 0xA7972F35
```

Exact, both.

## Why nobody saw it

**The full matrix was never re-run after the back-ball packet landed.** The
pass-25 report says so in as many words about a neighbouring case: *"`mbolt`
alone was rebuilt and all seven of its legs re-run on the new binary. The
renderer was not rebuilt."* The floors kept their PASS from a tree that no
longer existed, and that PASS was carried forward as evidence through the pass's
close, its review and its production verification.

This is the house pattern exactly: **a green leg is a claim about the tree it
ran on, not about the tree that ships.** The knob to switch the mechanism off
was written and documented in the same packet -- the pass-25 notes even say
`...=0:0` "restores the pass-25 pose on every clip, bit for bit" -- it was just
never added to the ladders that need it.

## The repair

`BBOFF="ZHAO_U02_BACKBALL_DAMP_CLIP_PM=0:0"` is spliced into every ladder that
reproduces a pre-back-ball bank, exactly as `$HASTYOFF` is spliced into every
ladder that reproduces a pre-pass-26 one. Both are the same act: a floor must
switch off **every** mechanism added since the bank it claims to reproduce, and
each new mechanism has to be added to the floors on the pass that introduces it.

---

## And one self-inflicted lesson while repairing it

The matrix run that found this died with

```
gatematrix_p26.sh: line 335: syntax error near unexpected token `)'
```

because **I edited the script while it was still executing.** `bash` does not
slurp a script; it reads and parses it incrementally from a byte offset, so
rewriting the file under a running shell makes it resume parsing at the wrong
place -- here, in the middle of a comment I had just inserted.

This is CLAUDE.md's live-tree trap wearing new clothes. The rule there is about
a Quartus fit reading the working tree; the same hazard applies to **any long
job whose input is a file in the tree**, and a shell script is its own input.
The 235 legs it had already written were real; the run was not complete, and the
`total` line it never printed is what says so.

Re-run clean, with the script untouched from start to finish.

---

## The same staleness reaches the SHIPPING row -- and the bank is still fine

`P25-RECEIPTS/crcs-ship.txt` was written by the pass-25 MATRIX (commit
`82218e2a`) and records hover `0x8124751D`, where the pass-25 tree actually
renders `0x89EBA648`. So `e-p25off` -- "hurry=off reproduces the pass-25 bank"
-- could not pass against it either, for the same reason and not because the
exact-off control is wrong (it is byte-exact against the pass-25 BINARY:
240/240 frames, matching sha256).

**Pass 25's shipped bank is not in doubt, and this is the part to get right.**
Two LATER receipts certify it, they agree with each other, and an independent
rebuild of `f66d107c` here agrees with both:

| source | hover | inspect | crackle | hasty |
| --- | --- | --- | --- | --- |
| `P25-BB-RECEIPTS/crcs-backball.txt` | 0x89EBA648 | 0x3A179F08 | 0x370F7F3E | 0xDC044A02 |
| `P25-REVIEW-RECEIPTS/crcs-reviewer-ship.txt` | 0x89EBA648 | 0x3A179F08 | 0x370F7F3E | 0xDC044A02 |
| this pass's rebuild of `f66d107c` | 0x89EBA648 | 0x3A179F08 | 0x370F7F3E | 0xDC044A02 |

Four for four on three independent paths. **One stale FILE, not a stale
creature.** `e-p25off` now compares against the reviewer's receipt, whose
provenance is an independent build.

It is also why this pass's byte-identity evidence was never at risk: it is
**binary against binary**, never against a committed receipt.

## A third self-inflicted lesson: a control that could not read its own baseline

`e-backball-live` -- the positive control for the repair above -- first ran
BEFORE `bank e-ship`, so it diffed against a file that did not exist yet.
`diff` wrote "No such file or directory" to stderr, the leg captured stdout, and
the empty result was reported as:

```
FAIL e-backball-live changed=[] want=[manafold-hover manafold-inspect ]
```

`changed=[]` **is exactly what a dead flag looks like.** Had this leg been
written to expect nothing, or had the harness treated an empty diff as a pass,
it would have reported the flag as inert with total confidence.

A positive control that cannot read its own baseline is worse than no control:
**it fails in the shape of the finding it exists to rule out.** Ordering fixed.
