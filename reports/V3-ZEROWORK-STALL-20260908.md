# A zero-work fragment does not retire in the composed island

**Found 2026-09-08 by the probe the owner brief's §5.1 asked for.**

```
zero-work wrap: submitted 16, retired 0
```

300 fragments were offered with `sample_count = 0` and `aux = 0`. Sixteen were
admitted; the pipeline then stopped accepting and **not one retired**.

## Why nothing caught this

`island_composed_directed` — 119 checks, the suite that gates the whole
restructure — drives exactly three sample counts into the composed island:

```
:861   frag_sample_count_i = 3
:903   frag_sample_count_i = 1 + (k & 1)     // 1 or 2
:2014  frag_sample_count_i = 3
```

**Never zero.** The zero-work case is covered only by the expander LEAF test,
which exercises 16 zero-sample fragments and confirms they are accepted and
issue nothing. The brief said precisely why that is not enough:

> *"A zero-sample expander unit test proves that no TMU requests are emitted. It
> does not establish the above implication for a separately completing owner."*

So this sat behind 119 passing composed checks, 541 owner checks, and a
392-record byte-identical paired run, because none of them ever presented the
input.

## What is established, and what is not

**Established:** with `sample_count = 0`, the composed island admits a bounded
number of fragments and then retires none. The same loop, same ports, same
phase structure retires normally at `sample_count = 1` — phases 1 and 3 of the
same test do exactly that and pass. So the difference is the zero-work input,
not the harness.

**Not established:** the mechanism. The plausible reading is the brief's two
completion domains — v3own makes an owner with `adm_req_i == 0` ready at
admission, while the material combiner downstream expects samples that never
arrive — but I have not traced it, and *the first explanation that fits is the
one to check hardest*. The number 16 smells like a queue depth rather than
anything semantic, which is consistent with "admitted until something filled,
then nothing drained".

**Also not established:** that this is the destructive slot-reuse trace §5.1
hypothesised. It is a simpler failure in the same unproven domain — the
fragments do not complete at all, so no slot is freed early. The brief's
reachability question is still open.

## Status

The probe is committed and is **failing**, deliberately, like the fault test
before it. It is not registered with `add_test`: a red suite hides regressions,
and this is a known, dated, written-down hole.

Two of its nine checks fail. The other seven — including all of repair A's
positive fault test — pass.
