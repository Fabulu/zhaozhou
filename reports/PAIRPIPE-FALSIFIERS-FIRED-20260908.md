# The paired PERSPUV tests, shown to fail

2026-09-08. Roadmap packet 4's remaining simulation obligation. Three mutations,
each one applied to `zhao_raster_perspuv_pairpipe.sv`, run, and reverted via
`git checkout` — no backup files, because the disk-full incident on 2026-09-06
left a **zero-byte backup** beside a mutated tree and a moment's worse timing
would have kept deliberately broken RTL with nothing to restore from.

A test that has never been seen to fail is not evidence. All eighteen checks pass
on the real module; the question is whether they *can* fail, and on the right
thing.

## 1. Credits freed at pipeline writeback instead of external acceptance

The mutation: `owned_q <= owned_q + accept - p3_v_q` rather than
`- out_fire_c`. This is the cache's documented lost-response bug — the producer
is told there is room while the item still occupies terminal storage.

```
output held shut: candidate accepted 300, its ready now 1, occupancy 4
FAIL  with its output held shut the candidate eventually REFUSES
FAIL  the burst ceiling is exactly 17            expected 0x11, got 0x12C
FAIL  the owned-count agrees with the wire       expected 0x11, got 0x4
FAIL  releasing ONE frees exactly ONE credit     expected 0x1,  got 0xA
```

**300 accepted against a ceiling of 17.** The mutant would have lost 283
responses, and four separate checks catch it. Note the occupancy reading of 4:
the block's own instrument would have *reassured* anyone reading it, because it
counts what the mutant believes it owns rather than what it actually holds. That
is the detector-blinding pattern again, and the reason the burst is measured on
the wire and not taken from the counter.

## 2. The lanes serialized

The mutation: `v_ready_o = (owned_q < CAP) && !p0_v_q`, so a pair can be accepted
only every other cycle — the throughput consequence of one shared multiplier
doing one of the two products per clock.

```
sustained: 128 pairs in 255 clocks (1.99 per pair)
FAIL  the candidate sustains ONE PAIR PER CLOCK
```

**1.99 against 1.00.** The rate check was written predicting "a serialized pair
would land near two"; it landed at 1.99. That is one of the few magnitude
predictions today that held, and it held because it followed from structure
rather than from reading a constant out of a comment.

## 3. The rescale intermediate narrowed to 56 bits

The mutation: the round-half-up constant computed in 56 bits and sign-extended,
rather than 64 throughout. At k=32, `sh_c` is 0 so `sh_c - 1` wraps to 63 and the
constant is `1 <<< 63` — the sign bit, which a 56-bit intermediate discards.

```
mismatches: u/v 84, tag 0, sat 0, dz 0
FAIL  the candidate's U and V are BIT-IDENTICAL to the frozen service's
```

**84 mismatched U/V pairs.** This is the check that matters most, because a
differential over pleasant midrange values would have passed with the narrower
width and the brief's §8.6 prohibition would have looked like fussiness. The
stimulus reaches the counterexample because the exponent sweep includes k = 0, 1,
2, 31, **32**, 33, 62, 63 deliberately, not because it is large.

## What this does not prove

It does not prove the candidate is correct — the differential does that, and only
against the service's behaviour. It proves the three named ways of getting this
class of block wrong are *visible* to the tests that guard them.

Nor does it say anything about area or timing. Those need FIT GATE 3, which is
two leaf fits at a matched profile, and the roadmap spends them only after the
island's current pair lands.
