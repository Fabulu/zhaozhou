# Manafold version 18 — Wave B root-authority independent review

**Date:** 2026-09-19
**Scope:** uncommitted Wave-B source, gates, controls, builds and rendered structural evidence
**Verdict:** **PASS after repair — both source/checker truth gaps are closed and the complete normal/control matrix is green**

## Verified findings

### P1 — the gate omits one actually swollen rear ring

**Files:** `tools/reel/manafold_art.h`, `tools/reel/manafold_model.h`, `tools/reel/manafold_spangate.cpp`

The new constants claim `kRootSwellSupportStartMm/EndMm` are the exact discrete rings with nonzero Front/End swell, and both production ownership and `mspan` use that same interval. The Front interval is exact. The rear interval is not.

With the production constants (`total=2930`, 64 rings, End station 2660, half-width 280), the actual nonzero sampled rear rings are:

```text
52:2418 ... 62:2883, 63:2930
```

Ring 63 still has `d=270 < 280`, so `make_loop()` gives it nonzero End swell. The declared support ends at ring 62 / 2883. Production therefore transitions ring 63 to `ReturnTip`, while `check_root_authority()` never counts it. Because model and checker share the same hand-authored, incomplete interval, the positive control cannot expose this omission. The report's claim that every actually swollen sampled ring has RearSocket rotation is false as written.

Ring 63 is the deliberately buried terminal ring, so this does not yet prove a visible regression. It does block quoting the checker as evidence. Repair must be explicit and must not sacrifice ReturnTip semantics:

1. derive actual sampled swell support from the same profile predicate/constants used by `make_loop()`, rather than a second ring-number table;
2. name the terminal ring as a buried-only exception if it remains ReturnTip-owned;
3. walk every shipping key/midpoint and prove that exception stays fully buried, while every nonterminal nonzero-swell ring has RearSocket rotation authority;
4. make the legacy-split control still fire only `kCatRootAuthority`.

Do not widen the RearSocket core through the tip or weaken closure merely to make the sentence true.

**Resolution:** sampled support is now constexpr-derived from the same production predicate `abs(station-at) < half`. Ring 63 is explicitly classified as a ReturnTip-only buried exception. `mspan` checks all ten terminal vertices through 97,000 shipping key/midpoint samples; worst ellipsoid rho is `1090.13 pm` under the unchanged `1120 pm` burial ceiling. The legacy-split control still returns attributed root-only RC 1.

### P2 — `kLoopCarrierCoreAtMm` is dead authorship

**File:** `tools/reel/manafold_art.h`

The new comment says `kLoopCarrierCoreAtMm[5]` is shared by model, public metrics and gates. A repository-wide reference check finds only its definition and the architecture report. Production still reads the prior station constants/derived skeleton stations directly.

This is not a pixel defect, but it creates exactly the second-list drift risk the table was meant to prevent. Either thread the table through genuine semantic consumers where doing so preserves the accepted pivot/run laws, or remove it and correct the architecture/comment. Do not leave a reassuring unused source of truth.

**Resolution:** `kLoopCarrierCoreAtMm` now supplies station authorship to the production model, shared public-joint metric and `mspan`; the old direct End/visible-core station references are gone. It is a real shared operand rather than documentation.

## Checks that survived adversarial review

- Four helper IDs are append-only (`23..26`), all parents precede children, and the 27-bone skeleton remains under the 32-bone limit.
- Front helper translation is re-expressed from Neck local +Y into JunctionF parent space after Neck/nodule solving, so it retains the old F–A fraction without inheriting Neck rotation.
- Rear helper translations remain in HingeD parent space while staged helper quaternions re-express HingeD toward RearSocket rotation. Key and nonlinear midpoint finalizers write all four helpers; held-final midpoint copying remains exact.
- Integrated skin intervals are total and non-overlapping at the 64 sampled stations. The rear 1/3, 2/3 and full rotation stages land before the first declared visible swell ring and preserve exact signed fractions at their boundaries.
- `HingePlay::tilt_front/yaw_front` are append-only/default-zero. Composition is rest yaw → fold Z → Front X → Front Y. Deterministic stimuli alter JunctionF and no protected local carrier channel directly.
- Root/swell selectors are parsed before static type construction; root names are exact strings and swell uses strict full-string `0..1000` parsing.
- The legacy mutant is causal and attributed: it restores old palettes, produces the named root-authority failure only, and does not rely on another red category.
- Corrected normal receipt: 117/117 non-terminal Front/End support vertices, zero wrong palettes; ten ReturnTip-only terminal vertices, 97,000 burial samples, worst rho `1090.13 pm`; 339,500 posed steps, zero reversal/pinch, minimum projection/separation `3.466/6.423 mm`, unchanged legal runs and green closure.
- The new helper staging lowers the worst C–End projection from the legacy `6.882 mm` to `3.466 mm` at Hover f0158, but remains positive with `6.423 mm` separation. Exact 4× current/legacy review (`V18-ROOT-REVIEW-WORST-F0158-4X.png`) shows no visible pinch, crack or attachment regression. This is comparison evidence to retain through later root art, not a likeness acceptance.
- Full integrated/legacy sheets and `V18-ROOT-AUTHORITY-AB-4X.png` show no new gross root kink, socket/tip exposure, contact loss or outline closure. Material, size and Front-motion likeness remain correctly open.

## Commit boundary after repair

Production source manifest is exactly:

1. `tools/reel/manafold_art.h`
2. `tools/reel/manafold_rig.h`
3. `tools/reel/manafold_model.h`
4. `tools/reel/manafold_clips.h`
5. `tools/reel/manafold_spangate.cpp`
6. `tools/reel/manafold_public_joint_metric.h`
7. `tools/reel/zhao_reel.cpp`

Evidence packet after P1/P2 correction:

- `V18-ROOT-AUTHORITY-IMPLEMENTATION.md`
- `V18-ROOT-AUTHORITY-REVIEW.md`
- corrected `V18-ARCHITECTURE.md`
- `TASK_LOG.md`
- five `V18-ROOT-AUTHORITY-MANAFOLD_*-ALLFRAMES.png` sheets
- `V18-ROOT-AUTHORITY-AB-4X.png`
- `V18-ROOT-REVIEW-WORST-F0158-4X.png`

Both findings are corrected and independently reachable. Wave B is ready for its source commit/push followed by the curated evidence commit/push. Root material, swell-size and public Front-motion likeness remain intentionally open for Waves C/D; this review does not promote them early.
