# The ABI v3 golden stamps: what was there before, and what regenerating proved

2026-09-04. `QFMT_VERSION` 2 -> 3 (amendment C2) left five marks in the tree.
Three were fixed when found. The remaining two were golden captures, and they
were **deliberately not regenerated** at the time, on this reasoning:

> Not regenerated deliberately -- that turns them green and destroys the record
> of what changed.

That was the right instinct and it is why this file exists. The record is taken
first, here, and only then were the captures regenerated.

## What the stale captures carried

Read out of each `.zcap`'s ABI-info section before anything was overwritten
(the section is found by searching for the generator name `zhaozhou-abi-gen`;
the stamps sit at +24 and +56):

    capture              generator SHA     zidl SHA
    duo_10frame.zcap     db6f6b2b... STALE  e54f05d4... STALE
    duo_markers.zcap     db6f6b2b... STALE  b9c5d806... STALE
    storm_10frame.zcap   db6f6b2b... STALE  e54f05d4... STALE
    z60_10frame.zcap     db6f6b2b... STALE  e54f05d4... STALE
    zcap_minimal.zcap    6b95fe82... CURRENT a2198a6e... CURRENT

Current values, from `runtime/include/zhao_abi.h`:

    ZHAO_GENERATOR_SHA256  6B95FE820B7F9F545F1E553820208FE02E28CA77066B1DF55F6AF5625023373F
    ZHAO_ZIDL_SHA256       A2198A6E55F7547754D5D9801A450F473609C97738E4E98684BED303988CA7B1

**Two facts fall straight out of that table.**

**The four wave2 goldens were never one vintage.** They share a generator SHA
but `duo_markers` carries a *different* zidl SHA from the other three. It was
captured against another revision of the interface description and nothing ever
said so -- the four sat in one directory looking like a set.

**`zcap_minimal.zcap` was already current**, which is the control. Without it,
"every golden has a stale stamp" is equally well explained by a generator that
stopped stamping correctly. One capture carrying the right pair rules that out.

## What regenerating proved

`shell_golden --write` rewrote the three it owns. Byte-diffing each against its
saved predecessor:

    z60_10frame     68 bytes differ in 2 runs:  56..59 (4),  792..855 (64)
    storm_10frame   68 bytes differ in 2 runs:  56..59 (4),  792..855 (64)
    duo_10frame     68 bytes differ in 2 runs:  56..59 (4),  792..855 (64)

Identical offsets and identical run lengths in all three. 4 bytes at 56 is the
file CRC. 64 bytes at 792 is the two contiguous 32-byte SHAs.

**Not one content byte moved.** Every frame and every CRC is byte-identical
across the ABI v3 migration.

So the worry that motivated holding these back resolves in the strongest
possible direction: **there was nothing in the content to destroy, because the
content never changed.** The earlier 32 + 32 + 4 diagnosis was reached by
inspecting one file; this reaches the same number independently, by
construction, on three.

## The method note worth keeping

A stale stamp and a real regression are indistinguishable from a test's PASS
list. `shell_golden_replay` failed in a way that looked exactly like a
regression someone had just caused, and was not. What separated them was
**diffing the bytes and finding the difference confined to fields that describe
the file rather than fields that are the file.** A capture format that keeps
provenance in a fixed, contiguous, identifiable region is what made that a
two-minute question instead of a bisect.

`shell_golden` 757/757 after regeneration, including "byte-identical to
committed". `golden_abi_info` went 8 failures -> 2, both `duo_markers`, which
has a different producer (`demos/wound_lab/duo_markers.cpp --write`).
