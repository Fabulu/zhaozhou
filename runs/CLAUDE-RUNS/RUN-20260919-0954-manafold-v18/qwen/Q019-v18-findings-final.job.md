# Q019 v18-findings-final
root: C:\programmieren\zencrifice\manafold-p16
continue: Q016

## Brief
Finalize the owner-facing Manafold VERSION 18 findings and live-card site blurb. Q016 drafted Waves A–E; its full draft is included as an input (the answer file). Now fill the Wave F placeholder and the final-bank provenance from the Wave F report and the final bank integrity report, and apply these coordinator corrections:
- Q016 finding: V18-SWELL-FRONT-ART.md is superseded; cite V18-SWELL-FRONT-REPAIR.md for Wave D receipts.
- Scope "no smear" to live subjects (archived/legacy controls intentionally keep the old effect).
- The Trick planted-antenna slide (≈178 mm, legacy balance wobble) was fixed in the integration packet by pinning the support XZ through the whole contact window (12.99 mm vs 178.46 mm; toggle ZHAO_U02_TRICK_PLANT_PIN=legacy restores the old bytes).
- Flight, Trick, Drift chosen values come from the Wave F report; all art values chosen by eye.
- Final provenance = the final bank report's renderer MD5, manifest SHA-256, 22 subjects / frame count, source commit.
Same rules as before: every number verbatim from the inputs with a [source] tag; if reports disagree, say so. Owner-facing plain language; "version 18" never "pass 18".

## Questions
1. The final `VERSION-18-FINDINGS.md` (complete markdown).
2. The final live-card blurb ("MANAFOLD, version 18 — ..."), same style/length class as the Pass-17 blurb, with a real PROVENANCE paragraph.
3. FINDINGS table: every conflict, ambiguity or unfilled item the coordinator must check.

## Inputs
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/qwen/Q016-v18-findings-draft-xhigh.answer.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-F-PERFORMANCES.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-FINAL-BANK-INTEGRITY.md
Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md
Upheaval/website/creatures.json:37-37
