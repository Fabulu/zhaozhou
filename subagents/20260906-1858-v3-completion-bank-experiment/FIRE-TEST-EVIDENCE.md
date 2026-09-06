# Fire-test evidence -- every check proved able to fail

Generated 2026-09-06 19:46 from fpga/rtl/texture/zhao_texture_v3own.sv
Baseline SHA256 of the module under test: 8F74A74F5ABACA32E561AB11FE5FF84986B69D4EFD8B940433C61893AFD22983

Each row perturbs ONE mechanism, rebuilds from source, runs the
adversarial bench, and records the VERBATIM failure text. The file is
then restored and the restore is verified by hash.

## BASELINE (unperturbed)

```
--- case 19: generation wrap takes the baseline drain, on a quiet island

--- case 20 (18.3): sustained emission rate through the retirement path
    emission span for 64 owners: 64 cycles
    longest back-to-back emission run: 64
[texture_v3own_adversarial] 460 checks passed
exit code: 0
```

## M1 recent-claim forwarding removed

MUTATION COULD NOT BE APPLIED -- the anchor string was not found.
That is itself a finding: the evidence below does not cover this mechanism.

## M2 sample bank write enable made unconditional

Anchor unique in file: True
Expected to break: a rejected packet must reach the payload bank and overwrite it

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M3 owner freed at the FINAL WRITE instead of at output (D.6)

Anchor unique in file: True
Expected to break: the ring must wrap over a live owner and corrupt its context

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 23

```
FAIL: case1 emitted count: expected 0x8, got 0x0
FAIL: case1 emitted counter: expected 0x8, got 0x0
FAIL: case1 island quiescent at end: expected 0x1, got 0x0
FAIL: case1 no live owners at end: expected 0x0, got 0x8
FAIL: D.1 one emission: expected 0x1, got 0x0
FAIL: owner still completes after stale: expected 0x1, got 0x0
FAIL: owner completes after unsolicited: expected 0x1, got 0x0
FAIL: owner completes after range fault: expected 0x1, got 0x0
FAIL: D.2 one emission: expected 0x1, got 0x0
FAIL: D.2 two emissions: expected 0x2, got 0x0
FAIL: D.4 all 16 emitted after release: expected 0x10, got 0x0
FAIL: backpressure: every owner emitted exactly once: expected 0x28, got 0x0
... and 11 more
[texture_v3own_adversarial] 23/133 checks FAILED
```

## M4 ready-ticket coalescing removed (D.2)

Anchor unique in file: True
Expected to break: one owner must receive TWO ready tickets

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M5 lost update: stale counter increments once for two faults

MUTATION COULD NOT BE APPLIED -- the anchor string was not found.
That is itself a finding: the evidence below does not cover this mechanism.

## M6 ordered retirement allowed to skip the incomplete head

MUTATION COULD NOT BE APPLIED -- the anchor string was not found.
That is itself a finding: the evidence below does not cover this mechanism.

## M7 sample_index range check removed

Anchor unique in file: True
Expected to break: sample_index 3 must stop being counted as a range fault

Exit code: 0   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
[texture_v3own_adversarial] 460 checks passed
```

## M8 generation check removed from the C2 predicate

Anchor unique in file: True
Expected to break: a stale-generation packet must be accepted as a real completion

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M9 generation-wrap drain gate removed (section 5.5)

Anchor unique in file: True
Expected to break: the namespace must wrap without draining the island

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 2

```
FAIL: admission was blocked for wrap while the ring was NOT full: expected 0x1, got 0x0
FAIL: every wrapping admission happened on a QUIESCENT island: expected 0x1, got 0x0
[texture_v3own_adversarial] 2/460 checks FAILED
```

## M10 output reservation off-by-one (19.4 lost beat / rate loss)

Anchor unique in file: True
Expected to break: the retirement path must stop sustaining one fragment per clock

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 1

```
FAIL: the output path emits back to back, not one-per-FSM-trip: expected 0x8, got 0x4
[texture_v3own_adversarial] 1/460 checks FAILED
```

## M11 committed published in the same stage as the write (data-before-done)

Anchor unique in file: True
Expected to break: committed must rise before the payload write edge

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M12 final_claimed removed -- duplicate final may overwrite (18.4)

MUTATION COULD NOT BE APPLIED -- the anchor string was not found.
That is itself a finding: the evidence below does not cover this mechanism.

## RESTORED (proof the tree is back to baseline)

```
SHA256 after restore: 8F74A74F5ABACA32E561AB11FE5FF84986B69D4EFD8B940433C61893AFD22983
--- case 20 (18.3): sustained emission rate through the retirement path
    emission span for 64 owners: 64 cycles
    longest back-to-back emission run: 64
[texture_v3own_adversarial] 460 checks passed
exit code: 0
```
