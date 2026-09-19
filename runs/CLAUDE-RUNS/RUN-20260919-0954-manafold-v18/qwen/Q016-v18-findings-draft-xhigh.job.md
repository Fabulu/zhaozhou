# Q016 v18-findings-draft-xhigh
max_tokens: auto
effort: xhigh
root: C:\programmieren\zencrifice\manafold-p16

## Brief
Draft the owner-facing findings document for Manafold VERSION 18 (the owner names it "version 18", never "pass 18"), covering Waves A–E only; Wave F (Flight + Trick 360) is still in progress, so leave a clearly marked placeholder section for it. Use PASS-17-FINDINGS.md as the format/tone template, and the Pass-17 site blurb (creatures.json line 37) as the style template for a short site blurb.

Rules for this draft:
- Every factual claim (numbers, hashes, commit SHAs, control tallies, chosen values) must come verbatim from the input reports, with the source report named in a trailing bracket, e.g. [V18-WAVE-E-CLEANUP]. Never invent or round a number. If reports disagree, say so rather than picking.
- Map each of the owner's eleven Direction-19 items to what was done, where, and its status (done / done-differently / declined-with-reason / pending-Wave-F). Items include the particle attempt (the pre-layer route was declined because it flattens depth; a mote surface fade shipped instead).
- Plain language for the owner; the art decisions are "chosen by eye", say so where the reports do.
- Keep hashes/SHAs in a compact provenance section, not sprinkled through prose.

## Questions
1. Produce `VERSION-18-FINDINGS.md` draft (markdown) in the answer.
2. Produce a draft site blurb for the live Manafold card (same style/length class as the Pass-17 blurb, "MANAFOLD, version 18 — ..."), with a PROVENANCE paragraph left as `[final bank provenance — fill after the final render]`.
3. In the FINDINGS table: list every place where the reports conflict, are ambiguous, or where you had to leave something unfilled — these are what the coordinator checks.

## Inputs
Upheaval/creature/Manafold/OWNER-DIRECTION-19-2026-09-19.md
Upheaval/creature/Manafold/PASS-17-FINDINGS.md
Upheaval/website/creatures.json:37-37
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ARCHIVE-CLOSURE.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-AUTHORITY-IMPLEMENTATION.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-ROOT-MATERIAL-IMPLEMENTATION.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-ART.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-SWELL-FRONT-REPAIR.md
zhaozhou/runs/CLAUDE-RUNS/RUN-20260919-0954-manafold-v18/V18-WAVE-E-CLEANUP.md
