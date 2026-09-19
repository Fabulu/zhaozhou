# Manafold Pass 17 final-bank review — batch 02

**Date:** 2026-09-19
**Renderer MD5:** `3390F2B8214473FEBFA32092A98E9056`
**Bank manifest SHA-256:** `560c12371251dae73e39c7be6e6caad8e0817cfbc6ae15fce75e586697dfe30d`
**Subjects:** Damage, Death Drop, Death Gutter, Drift
**Verdict:** **BLOCKED — 3/4 pass; Death Drop has one visible effect-off switch at settle**

## Review coverage

Every tile of all four exact final-bank contact sheets was reviewed: 464 Damage frames, 450 Death Drop frames, 590 Death Gutter frames and 300 Drift frames, for **1,804/1,804 presentation frames**. Candidate transitions were then reopened from the exact raw bank at native or nearest-neighbour 4× scale.

## Subject verdicts

### Damage — PASS

All four directional hits remain forceful and distinct. Their arrivals, squashes, antenna lag and recoveries are continuous; no body/deform wrap, one-frame carrier jump, socket slide, span buckle, detached tip, framing loss or effect reset appears. The former high-energy interval f0258–f0282 changes from round impact through the broad squash and recovery without a discontinuous silhouette or attachment change.

Witness: `PASS17-FINAL-BATCH02-DAMAGE-F0258-0282-NATIVE.png`.

### Death Drop — BLOCKER

The fall, five diminishing contacts, grounded final pose, eye/child registration, antenna skin, declared penetration and corpse hold are otherwise coherent. The fixed-camera face does not reset at the settle boundary, and the body/antenna pose remains continuous.

However, the attached folded-mana/lightning mass remains a large opaque navy/black shape throughout f0228–f0233 and then disappears completely in the single f0233→f0234 transition (authored settle key 117). It stays absent through f0449. At native and exact 4× this reads as the rejected **turn off** operation, not a continuous death envelope or particle/lightning handoff. The change is high-contrast against the peach opening and cannot be dismissed as thumbnail absence.

This violates Owner Direction 18's requirement that visible lightning/particle state transition continuously, including an authored death. The Q2 eye repair is intact; this is a separate composited-effect operand exposed only by looking at the exact shipping frames.

Primary evidence: `PASS17-FINAL-BATCH02-DEATH-DROP-F0228-0236-4X.png`. The native settle comparison is also included in `PASS17-FINAL-BATCH02-DEATH-SETTLES-NATIVE.png`.

### Death Gutter — PASS

The mana gutters out before the final body failure, the staged sags and recoveries remain readable, and the antenna progressively loses authority without a one-frame carrier/socket jump. The last collapse and f0380–f0386 settle are visually continuous; the empty outlined O, fixed-camera face, body pose and declared penetration hold through f0589 without resurrection or detached children. No effect reseed or off/reappear fault occurs at the settle boundary.

Witness plates: `PASS17-FINAL-BATCH02-DEATH-GUTTER-STAGES-NATIVE.png`, `PASS17-FINAL-BATCH02-DEATH-GUTTER-F0378-0386-4X.png`, and the native settle row in `PASS17-FINAL-BATCH02-DEATH-SETTLES-NATIVE.png`.

### Drift — PASS

The lazy S, bank and two corrections remain continuous with intact antenna attachment and solid-body/outline read. At f0296–f0298 the creature exits the left edge; f0299 re-enters on the right as the intentional screen-space traversal wrap. The f0299→f0000 site-loop handoff preserves the same pose, rig and location rather than resetting an attachment or body channel. The screen wrap is visible and deliberate, not a broken pose seam.

Witness: `PASS17-FINAL-BATCH02-DRIFT-SEAM-NATIVE.png`.

## Durable evidence manifest

- `PASS17-FINAL-BATCH02-DAMAGE-F0258-0282-NATIVE.png`
- `PASS17-FINAL-BATCH02-DEATH-SETTLES-NATIVE.png`
- `PASS17-FINAL-BATCH02-DEATH-DROP-F0228-0236-4X.png`
- `PASS17-FINAL-BATCH02-DEATH-GUTTER-STAGES-NATIVE.png`
- `PASS17-FINAL-BATCH02-DEATH-GUTTER-F0378-0386-4X.png`
- `PASS17-FINAL-BATCH02-DRIFT-SEAM-NATIVE.png`

No discarded probe is cited as evidence.

## Batch result

**PASS 3 / BLOCKER 1.** The exact bank must not proceed to encode or publication until Death Drop's f0233→f0234 visible effect-off switch is repaired, positively controlled, rerendered and reviewed. Because the repair changes production output, the eventual accepted 28-subject bank must be regenerated from one new exact binary.
