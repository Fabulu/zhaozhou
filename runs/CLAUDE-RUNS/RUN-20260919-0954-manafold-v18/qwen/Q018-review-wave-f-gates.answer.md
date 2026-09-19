# Q018 answer — review-wave-f-gates

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 6b11812d81fd4ae7a909b1069f958aa9
- when: 2026-09-19T19:35:57  seconds: 768  finish: stop  status: ok
- usage: {"completion_tokens": 58818, "completion_tokens_details": {"reasoning_tokens": 57006}, "prompt_tokens": 12072, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 70890}
- inputs: [{"input": "git show d7d51171 -- tools/reel/manafold_qa_p12.cpp", "sha256": "84d2e704852bd8a0", "chars": 26012}, {"input": "git show d7d51171 -- tools/reel/manafold_probe.cpp", "sha256": "41c037ef1546cde5", "chars": 6709}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P1 | tools/reel/manafold_qa_p12.cpp:913 | Fail-leg certification can exit 0 while a non-Wave-F category fails because attribution only receives Wave-F counters. | Line 914 checks only `others`; lines 923-929 pass only `q6a/q6b/q6c/q7` sums. |
| 2 | P2 | tools/reel/manafold_qa_p12.cpp:779 | Q6b root-XZ ease is skipped when segment peak ≤ `kJoinRootFloorMm`, so a small root-XZ acceleration jump passes. | `rr` stays 0 from line 777 unless line 779 `if (rpk > kJoinRootFloorMm)` assigns it. |
| 3 | P2 | tools/reel/manafold_qa_p12.cpp:899 | Q7 allows seam step/accel/jerk up to 5% + 0.5 mm above interior maxima, not “no larger”. | `seam_ok = e1 <= s1 * 1.05 + 0.5 && e2 <= s2 * 1.05 + 0.5 && e3 <= s3 * 1.05 + 0.5;` |
| 4 | P2 | tools/reel/manafold_qa_p12.cpp:451 | The 240 mm Trick root-step bound is an inline literal and whole-slot, not a named constant nor limited to the plant/lift window. | `{13, 240.0, "trick: the planted 360 swings the body round its planted antenna"},` in a slot-only table. |

## ANSWER

1. Operands and structural blindness  
- Q6a compares the shipped slot-13 root quaternion `spin->quats` with the no-spin builder root `none.quats` (tools/reel/manafold_qa_p12.cpp:678-679). The spin is extracted as `a * conj(b)`, normalized, and unwrapped key-by-key from the relative quaternion (tools/reel/manafold_qa_p12.cpp:681-696). The no-spin reference is built independently with `TrickSpinMode::kNone` (tools/reel/manafold_qa_p12.cpp:663-667), so common base motion moves together and cancels in the ratio.  
- Q6b compares the discrete acceleration at a join with the segment peak acceleration of the same signal, for progress and root XZ (tools/reel/manafold_qa_p12.cpp:775-780). A uniform amplitude scale moves both operands together, but the gated property is the ratio; a C1 ease changes that ratio. Root XZ is not judged below the floor (tools/reel/manafold_qa_p12.cpp:779).  
- Q6c compares carrier-B contact-patch centroids between the spin clip and no-spin clip (tools/reel/manafold_qa_p12.cpp:840). Common no-spin wander moves together and is subtracted; the gate measures spin-added drift.  
- Q7 compares seam stencils `e1/e2/e3` with interior maxima `s1/s2/s3` (tools/reel/manafold_qa_p12.cpp:892-899). Seam keys are excluded from interior maxima (tools/reel/manafold_qa_p12.cpp:888-890), but the test is relative, so a fault that raises both could be hidden.

2. Controls firing only their own category  
- Spin gain: line 922-923 uses `q6a_fails` as own and `q6b+q6c+q7` as others.  
- Spin ease: line 924-925 uses `q6b_fails` as own and `q6a+q6c+q7` as others.  
- Spin pivot: line 926-927 uses `q6c_fails` as own and `q6a+q6b+q7` as others.  
- Flight seam: line 928-929 uses `q7_fails` as own and `q6a+q6b+q6c` as others.  
However, this enforcement is only over Wave-F categories; earlier categories are ignored by the leg return (tools/reel/manafold_qa_p12.cpp:913-929), see finding 1.

3. Loosened bounds  
- 240 mm root-step ceiling: scoped to slot 13 only (tools/reel/manafold_qa_p12.cpp:451), but it is an inline literal and applies to the whole slot-13 clip, not only the plant/lift window.  
- Probe travel exclusion: scoped to `kTrickSlot` and the declared contact span `kTrickPlantKey - kTrickApronKeys` through `kTrickLiftKey + kTrickApronKeys` (tools/reel/manafold_probe.cpp:2032-2034). It uses named key constants and leaves only those keys out of `worst_eff` (tools/reel/manafold_probe.cpp:2036). Whether the contact contract above covers exactly those apron keys is not fully shown: `in_window` is visible at tools/reel/manafold_probe.cpp:241-242, but `in_apron` is only declared at tools/reel/manafold_probe.cpp:243.

## CONTINUATION

- Done: Q6a extracts spin from shipped slot-13 root vs no-spin build root and unwraps the relative quaternion (tools/reel/manafold_qa_p12.cpp:663-696). Q6b judges join acceleration ratios for progress and root XZ (tools/reel/manafold_qa_p12.cpp:775-780). Q6c measures spin vs no-spin carrier-B contact-patch drift (tools/reel/manafold_qa_p12.cpp:840). Q7 compares seam stencils to interior maxima (tools/reel/manafold_qa_p12.cpp:892-899). Fail-leg attribution uses only q6/q7 counters (tools/reel/manafold_qa_p12.cpp:913-929). The 240 mm Trick ceiling is a slot-13 table entry (tools/reel/manafold_qa_p12.cpp:451). Probe spin ownership/depth counts use `g_u02_trick_spin_start_key`/`settle_key` (tools/reel/manafold_probe.cpp:355-366, 421-424). Probe travel excludes declared Trick contact keys from `worst_eff` (tools/reel/manafold_probe.cpp:2032-2036).
- Open: whether the contact contract’s window/apron exactly matches the travel exclusion (tools/reel/manafold_probe.cpp:241-243 vs 2032-2034) is not shown. Whether `g_u02_trick_spin_start_key`/`settle_key` equal the Q6 plant/lift window (tools/reel/manafold_qa_p12.cpp:673-674) is not shown. Whether `kTrickSlot` equals literal 13 is not shown. Whether `--fail-flight-seam` preserves declared cycles/amplitude is not shown.
- Next chunk: verify the contact contract and spin key constants cover the probe travel exclusion and Q6 window, and decide whether leg attribution must include pre-Q6 `fails`; inputs: full `manafold_probe.cpp` around `support_ok`/`in_apron`, and headers for `kTrickSlot`, `kTrickApronKeys`, and the spin start/settle globals.

## Coordinator verdict

**partial** — xhigh, 57k reasoning, 768s. spot 4/4 at d7d51171: #1 REAL but P2 not P1 - Wave-F fail-leg attribution ignores non-Wave-F categories (qa_p12:913-929); normal run unaffected; fix sent to worker. #2 root-ease floor, #3 seam 5%+0.5mm tolerance, #4 Trick 240 table literal: accurate observations, P3 by design. Good catch on #1 - a detector self-certification gap nobody else saw.
