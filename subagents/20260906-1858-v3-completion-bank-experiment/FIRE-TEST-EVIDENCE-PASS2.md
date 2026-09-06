# Fire-test evidence -- every check proved able to fail

Generated 2026-09-06 20:09 from fpga/rtl/texture/zhao_texture_v3own.sv
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

Anchor unique in file: True
Expected to break: D.1 consecutive duplicate: the second packet must be accepted

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 2

```
FAIL: D.1 payload write count is one: expected 0x1, got 0x2
FAIL: D.1 the FIRST payload survives, not the duplicate: expected 0xD78AA8B9F9, got 0xAF155173F2
[texture_v3own_adversarial] 2/460 checks FAILED
```

## M5 lost update: stale counter increments once for two faults

Anchor unique in file: True
Expected to break: simultaneous TMU and AUX faults must count ONE instead of TWO

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 1

```
FAIL: the lost-update defect: two faults, two counts: expected 0x2, got 0x1
[texture_v3own_adversarial] 1/460 checks FAILED
```

## M6 ordered retirement allowed to skip the incomplete head

Anchor unique in file: True
Expected to break: the output must emit an owner whose final result was never written

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M13 range check removed AT SOURCE (both defences at once)

Anchor unique in file: True
Expected to break: sample_index 3 must stop being a range fault AND must reach a source bit

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 2

```
FAIL: sample_index 3 rejected by range: expected 0x1, got 0x0
FAIL: range fault is NOT counted as unsolicited: expected 0x0, got 0x1
[texture_v3own_adversarial] 2/460 checks FAILED
```

## M12 final_claimed removed -- duplicate final may overwrite (18.4)

Anchor unique in file: True
Expected to break: a duplicate FINAL return must overwrite the committed result

Exit code:    (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## RESTORED (proof the tree is back to baseline)

```
SHA256 after restore: 8F74A74F5ABACA32E561AB11FE5FF84986B69D4EFD8B940433C61893AFD22983
--- case 20 (18.3): sustained emission rate through the retirement path
    emission span for 64 owners: 64 cycles
    longest back-to-back emission run: 64
[texture_v3own_adversarial] 460 checks passed
exit code: 0
```
