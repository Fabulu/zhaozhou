# SPEC v1: Commit4 -- the owner/COMBINE resident-read seam

**Run ID:** RUN-20260910-0837
**Created:** 2026-09-10 08:37 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

The texture island's ready tickets carry only the owner handle; the material
combiner reads sample planes 0/1/2 and AUX from the owner's own M10K banks per
phase, through one reader per plane. Every recipe and every output preserved.

## Scope

**In Scope:**

- `zhao_texture_v3own` READ_LATE parameter: handle-only job queue, plane port,
  publication/release assertions, `ev_src_unpub_o` tripwire.
- `zhao_texture_material_combine_v2` READ_LATE parameter: slot-addressed phase
  reads, 47-bit payload, canonicalisation at D.
- `zhao_texture_island_v3_top`: wire the seam, delete the aux/s2 mux.
- A differential SEEN TO FAIL on a committed mutant; a directed test that fires
  the counter; the report with the one fit gate named.

**Out of Scope (named as the next increment):**

- The scoreboard -> epoch-plane move and the ready-claimed table deletion
  (architecture report 4.2/4.4: needs the cross-pipe commit-forwarding
  structure, the cycle table, an offset-sweep kernel and a mutant).
- Any Quartus fit. Any commit.

## Acceptance

- island_v3_composed_directed, island_v3_prod_composed_directed,
  island_v3_fault_directed, island_v3_paired (gate 3),
  material_combine_v2_diff, texture_v3own_adversarial: PASS, UNCHANGED.
- New: material_combine_readlate_diff PASS; its mutant control PASS (i.e. the
  checkers fire); texture_v3own_readlate_directed PASS.
- Lint -Wall clean for both READ_LATE values of both blocks;
  check_quartus17_syntax, check_prod_manifest clean; check_v3_banks no new
  findings.
