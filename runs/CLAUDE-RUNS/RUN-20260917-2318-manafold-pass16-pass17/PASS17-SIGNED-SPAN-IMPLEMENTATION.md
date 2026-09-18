# Manafold Pass 17 signed-span implementation

**Date:** 2026-09-18
**Direction:** Owner Direction 16
**Architecture:** `PASS17-STRETCH-ARCHITECTURE.md`

## Structural result

Manafold now carries signed visible length with seven appended translation-only skin helpers:

- `SpanDeltaA/B/C`: constant-slope partial deltas at the ends of the three free runs;
- `SpanDeltaE`: an unskinned full C/closure-to-End delta receipt;
- `SpanDeltaEStart`, `SpanDeltaEMid` and `SpanDeltaEPreSocket`: staged fractions of that same End delta.

Existing bone IDs 0..15 remain stable. The helpers are IDs 16..22, identity-rotation, zero-rest-offset children of their respective upstream articulations. At zero delta each helper palette is therefore exactly its parent palette. With a signed local-Y delta it differs only by translated endpoint position; no non-rigid bone matrix, generic clip sidecar or RTL change is introduced.

A/B/C now distribute translation at one constant slope across each visible free run **and its existing 90 mm incoming bend**. `Rig::set_span_delta` writes the full delta to the real child and the exact free-run/complete-run fraction to its partial helper; the helper-to-child bend supplies the remainder while introducing the child's independent articulation. This preserves every rigid carrier core and gives the shortest B-C run 117 mm at its legal compaction floor instead of a fitted 27 mm remainder.

C-End preserves the accepted 90 mm HingeC-to-HingeD rotation ramp bit-for-bit at zero delta and stages translation across the complete 840 mm C-core-to-End-core run: HingeC to `EStart`, then `EMid`, then `EPreSocket`, then the body-attached `RearSocket`. The unskinned full receipt remains available for an independent helper/socket endpoint comparison. All fractions are derived from named run lengths and written from one helper at authored keys and nonlinear midpoints.

The extra C-End stages are required by the real 6-bit skin weights. A single partial stage still let legal maximum compaction reverse a local ring step. The four translation zones keep the analytic `-707 mm` extreme at a positive `4.97 mm` synthetic minimum step without clamping the End or changing zero-delta rotation.

Front/A/B/C/End visible cores remain rigid. Front remains body attached. Rear finalization still aims Hinge D at the lane-0-deformed body target, writes the exact Root-local `RearSocket`, and keeps `ReturnTip` as a buried sibling. Deform lanes 1..3 are explicitly cleared at the one key-write site and carry zero axial authority, so the rejected positive-only deform cannot double-transform length.

Named safety bounds remain editable content constraints, in F-A/A-B/B-C/C-End order:

- extension ceilings: `+320/+480/+400/+440 pm`;
- compaction floors: `-320/-330/-430/-700 pm`;
- minimum complete signed-run remainder: `80 mm`.

One common percentage remains inappropriate because centre distances differ. The complete translation runs are now 470/240/280/840 mm and retain 253/128/117/133 mm respectively at their authored compaction floors. The 80 mm rejection floor therefore has at least 37 mm structural headroom instead of being fitted 1--3 mm below the current B-C result. The solver never silently clamps: a bad pose fails the bank gate rather than detaching its carrier.

## Direct validation

A clean direct compile of the changed 23-bone content path succeeds.

- `manafold-meshcheck`: **PASS**, 34 meshlets, 2,800 triangles, CLEAN topology.
- `manafold-nodule`: **PASS**, five-carrier independence and rear socket green. The real skin reaches approximately `-201..+199 mm` on A, `-204..+197 mm` on B and `-205..+196 mm` on C in the solo diagnostic.
- `manafold-probe`: **PASS**; terrain/contact, closure, rear socket, mist and corrected scale-aware eye leash are all green.
- `manafold-spangate`: **PASS**. It reads 668 compiled loop vertices with zero pair/weight mismatch, 0/7 zero-delta palette differences, 13,158 retired-lane samples all zero, exact A/B/C and C-End staged fractions at every key/midpoint, zero shipping bound/run-margin breaches and 8,772 rear closure samples.
- The new production-bank posed walk decodes and skins every compiled loop ring at every shipping key and midpoint: 307,020 adjacent steps, zero reversal/pinch, worst projection `6.891 mm` and separation `7.442 mm` at slot 0 key 79 C-End. `--fail-posed-order` leaves scalar tracks legal but rotates one shipping partial helper; only the posed walk then reports six reversed steps.
- All 20 rigid/clamp/drift/overcompaction/E-stage/posed-order mutations return nonzero.
- Shipping traces retain both signs on all four spans. C-End remains `-698..+417 mm`, but its 840 mm staged run stays ordered instead of buckling the former 660 mm region. Worst full-receipt/socket coincidence remains `8.965 mm` inside the quantized closure tolerance and still requires final-resolution likeness review.
- Fall Q5 is independently green through `mqa --fall-only`; `--fail-fall-wrap` returns success only after Q5 itself catches the restored non-root reset. The broad omnibus `mqa` remains RC 1 on three pre-existing Q2/Q3 debts outside this repair (two death-eye snaps and Startle's root-step ceiling), while Q5 itself is clean. This row records the final23 span binary at that moment; `PASS17-Q2-Q3-REPAIR.md` and its independent review later close only those omnibus debts and do not alter this span evidence.

## Independent review and picture closure

The review's two findings are structurally resolved: actual posed shipping rings are walked independently of scalar receipts, and signed translation is spread through the incoming bends so the minimum run has real headroom.

A clean final 23-bone renderer (MD5 `F7C2DAA085661C1E00CADF09C65B5F8A`) re-rendered the exact former failures and complete 25-frame windows. Native, exact 4x and direct before/after review passes Hover f0157, Channel f0160, Rest f0164, Flight f0202 and fixed-idle/Crackle f0157: the broad diagonal/buckled shoulder is gone, the return stick and End carrier stay distinct, and there is no crack, reversal, pinch, sliding core or detached socket/tip.

Final evidence:

- `PASS17-SPAN-CEND-FINAL23-NATIVE.png`
- `PASS17-SPAN-CEND-FINAL23-4X.png`
- `PASS17-SPAN-CEND-FINAL23-BEFORE-AFTER-4X.png`
- five `PASS17-SPAN-CEND-FINAL23-*-WINDOW-5COL.png` every-frame sheets

The earlier `PASS17-SPAN-CEND-REPAIR-PROVISIONAL-*` files remain labelled stale checkpoint evidence and are not acceptance inputs.

## Fit disposition

No Quartus fit. This is content using the existing 32-bone/two-weight/local-translation format and changes no generic decoder/RTL semantics. Native compilation and production-path geometry gates answer the structural question; final-resolution renders answer likeness and amplitude.

## Art boundary

This packet does **not** select larger A/B/C travel. The signed mechanism and former C-End collapse are structurally and visually closed on the final 23-bone binary. Direction 16's remaining amplitude work must show each A/B/C carrier both above and below the other two, repeated pairwise crossings, and both signs on all four connecting spans without seams, collapsed bands or sliding cores.
