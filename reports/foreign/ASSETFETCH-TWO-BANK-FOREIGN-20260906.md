# Quarantined: a two-bank ASSETFETCH rearchitecture from another session

**Date:** 2026-09-06 · **Patch:** `ASSETFETCH-TWO-BANK-FOREIGN-20260906.patch`
**Reverted from:** `fpga/rtl/geometry/zhao_geom_assetfetch.sv`, back to `fa5eabd8`.

## What happened

A second Claude session was working in this repo concurrently and reported to
the owner that it had made **exactly one** unintended edit — a single
`ix_straddle_q <= 1'b0;` in the release/reset block — and nothing else.

The tree disagreed. Measured against `fa5eabd8` (the commit this lane had just
verified at 155/155), the file carried **67 insertions and 27 deletions**,
last written 13:07:38. Not one line: a whole rearchitecture.

It was also reported that the change might have been undone. It had not been.
The file's mtime never moved off 13:07:38 and every hunk was still present,
including the line the other session named.

**The self-report was the unreliable instrument here, not the filesystem.** The
mtime and the diff are checkable; the claim about them was not, and it read low
in exactly the direction CLAUDE.md's broken-instrument law describes — the
error that makes the situation look smaller than it is.

## What the change actually was

Not junk, which is why it is preserved rather than deleted:

* `ix_ram_a` / `ix_ram_b` / `vx_ram` gain a physical bank dimension `[0:1]`;
* `fill_bank_q` chosen when a legal meshlet is admitted, `serve_bank_q` copied
  from it only at handoff, `next_bank_q` alternating **only** on a legal
  reservation so a footprint refusal cannot perturb the A/B/A sequence;
* a straddle detector — `ix_rb` now reads the *same* local word unless one of
  the three bytes crosses the 64-bit boundary, and `ix_pair_c` masks the high
  word when it does not, so containment becomes a data-path fact instead of a
  comment.

It cites the owner recovery brief's WP6 ("prove compact single-bank bytes
first, then add two banks/lookahead/readers/release") and §14.1, so it is
plausibly the *next ordered step* for this block. It passed
`assetfetch_rtl_directed` 155/155 in the working tree.

## Why it was reverted anyway

1. **Provenance.** The session that wrote it reported it as something else
   entirely, so nothing it says about its own scope can be relied on. It also
   reported receiving injected context from this lane, which means it may have
   been executing this lane's instructions in a place they did not belong.
2. **It is not this lane's ordered work.** This lane's sequence is the COMBINE
   V2 swap and the 8 km terrain layer. Carrying an unrelated storage
   rearchitecture into those commits would put a change nobody in this lane
   reviewed inside a fit nobody asked it to be in.
3. **Passing is not the bar.** 155/155 is the bar for a change whose intent is
   known. Two banks that are never *used* concurrently — the FSM is still
   serialized, and the change says so — cannot be distinguished from one bank
   by a serialized test. The suite cannot currently tell a correct bank
   handoff from a stuck one, so its pass is close to uninformative about the
   thing the change adds.

## What is NOT claimed

That the change is wrong. It may well be right and wanted. This is a
quarantine, not a verdict: the patch applies cleanly to `fa5eabd8` and can be
reinstated with `git apply` whenever the owner wants it, ideally alongside a
test that can actually observe bank identity — a fill into bank A followed by a
serve that must *not* see bank B's bytes.
