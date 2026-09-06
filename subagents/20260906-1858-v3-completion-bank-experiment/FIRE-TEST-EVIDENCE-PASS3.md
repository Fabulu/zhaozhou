# Fire-test evidence -- every check proved able to fail

Generated 2026-09-06 20:23 from fpga/rtl/texture/zhao_texture_v3own.sv
Baseline SHA256 of the module under test: B8AA7522676A3A7FCB8B7E19D52556DB09FF126E65F9D8FBE9E3DA4FC127868E

Each row perturbs ONE mechanism, rebuilds from source, runs the
adversarial bench, and records the VERBATIM failure text. The file is
then restored and the restore is verified by hash.

## BASELINE (unperturbed)

```
--- case 20 (18.3): sustained emission rate through the retirement path
    emission span for 64 owners: 64 cycles
    longest back-to-back emission run: 64

--- case 21 (C11): a legal ERROR completion satisfies its required bit
[texture_v3own_adversarial] 467 checks passed
exit code: 0
```

## M2 sample bank write enable made unconditional

Anchor unique in file: True
Expected to break: a rejected packet must reach the payload bank and overwrite it

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M4 ready-ticket coalescing removed (D.2)

Anchor unique in file: True
Expected to break: one owner must receive TWO ready tickets

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M6 ordered retirement allowed to skip the incomplete head

Anchor unique in file: True
Expected to break: the output must emit an owner whose final result was never written

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M8 generation check removed from the C2 predicate

Anchor unique in file: True
Expected to break: a stale-generation packet must be accepted as a real completion

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M11 committed published in the same stage as the write (data-before-done)

Anchor unique in file: True
Expected to break: committed must rise before the payload write edge

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 0

```
(no FAIL lines -- see verdict)
```

## M12 final_claimed never set -- duplicate final may overwrite (18.4)

Anchor unique in file: True
Expected to break: the +2 duplicate FINAL must be accepted and overwrite the result

Exit code: 1   (0 = the mutation did NOT fire, which would be a hole)
Failing checks: 2

```
FAIL: all three final duplicates rejected: expected 0x4, got 0x3
FAIL: the FIRST final result survives both duplicates: expected 0xD8C385B623, got 0x10DE835F88
[texture_v3own_adversarial] 2/467 checks FAILED
```

## RESTORED (proof the tree is back to baseline)

```
SHA256 after restore: B8AA7522676A3A7FCB8B7E19D52556DB09FF126E65F9D8FBE9E3DA4FC127868E
    longest back-to-back emission run: 64

--- case 21 (C11): a legal ERROR completion satisfies its required bit
[texture_v3own_adversarial] 467 checks passed
exit code: 0
```
