# Task Log: RUN-20260908-0530 - [Describe objective here]

**Created:** 2026-09-08 05:30 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260908-0530-p0c-v3own-island-restructure/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-08 05:30 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260908-0530
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

## P0-C: replacing fragrob with v3own inside the texture island

Owner direction 49fc32e9 governs: *"Finish the texture island. Terrain,
projection, broad fit-sheet evacuation, and unrelated measurement-tool
expansion are not the current implementation priority."* Nothing here leaves
texture.

### Three gates, and where they stand

1. **v3own's own 541-check suite on the UNMODIFIED file** -- PASSES. The
   restructure is confined to the island top and a new expander; `v3own.sv` and
   the old `island_top.sv` are both untouched, the second because it is the
   end-to-end ORACLE for gate 3.
2. **`island_v3_composed_directed`** (the same test source as the oracle's,
   built with `ISLAND_V3=1`, so the stimulus is identical by construction) --
   **119 checks, 10 failing.** Flow control is closed; what remains is colour.
3. **The paired run** -- not started. Gate 2 first.

### Stage B, closed

`zhao_texture_frag_expand`: 10/10 directed checks, including the request
sequence element for element against a model of fragrob's expansion, exact
per-fragment issue counts, and 16 zero-sample fragments accepted issuing
nothing. Leaf fit **ok, 323 ALM, 451 reg, 364 memory bits, 0 DSP**, worst path
**+1.623 ns** (`cur_q.count[0]` -> `iss_tmu_valid_o`).

Its fit target carries `max_dsp: 0` and **no `max_alms`**. The architecture's
own estimate is a range (200-600); a gate written from a range is docket M7 --
a rule written from belief, which has already failed two correct blocks.

### The defect class this restructure keeps producing

Five times now, and every one identical in shape: **a bit-slice that was
correct until the MEANING of the bits changed.** The v3own re-key moved the
owner handle from `{slot[3:0], gen}` to `{slot[5:0], gen}`, and each of these
was a place that had taken the old identity apart:

| site | what it sliced | why it survived elaboration |
|---|---|---|
| `uvw_m` index | old slot width | in-range, wrong row |
| `fc_wp` / `fc_rp` | queue pointers | in-range, wrong entry |
| `rsp_class_i` | `[15:14]` | in-range, wrong class |
| AUX return token | `[AUX_TOKW-1 -: 4]` of a 6-bit slot | in-range, wrong owner |

None failed lint. None failed elaboration. All produce plausible wrong data.
**A re-key's real cost is not in the port list -- it is in every place that
ever took the old identity apart**, and no tool in this tree finds those.

### The two fixes that closed flow control

* The AUX return token **is** the owner handle. It was being re-assembled from
  a stale slice; it needed taking whole.
* The COMBINE handshake had **two different alignment terms**: `f_valid_i` on a
  registered `mat_rdy_q`, `cmb_ready_i` on a combinational compare. A valid and
  a ready computed from different notions of "the material is here" cannot
  agree, and 32 retired became 0. The registered flag is **deleted**, not
  repaired -- the fault was having a second source of truth, and fixing one of
  two would have left the trap armed.

Colour mismatches per phase across that fix: 9/2/5/4/5/1/10 -> 2/2/1/1/1/2. The
residue is a DIFFERENT fault, not the same one smaller.

### Next step, written down before reading anything else

Aux sheet coordinates come out **17 where 22 are expected** -- five aux requests
lost in the ISLAND's wiring, not the expander's (its own suite passes 22/22
under randomised backpressure on both sinks). Start there: it is a counter, and
counters localise better than colours.

One failing check is the TEST's, not the design's: *"FRAGROB accepted
fragments: expected 1, got 0"*. fragrob is deleted in v3 by design; that check
needs an `ISLAND_V3` arm before it means anything.

### A tool of mine read flattering on its first real use

`worst_path_index.py`, written this session to preserve M6 evidence, recorded
**slack 0.000 with empty node names** for a block whose true worst path is
**+1.623 ns**. Its three-column regex also matched rows in the per-path DETAIL
tables further down the report. Its self-check only caught TOTAL failure
(fewer than three modules), never per-row garbage -- and a clean zero is
exactly the kind of number nobody audits.

The second attempt, an eight-group lazy regex, backtracked catastrophically and
hung on a real report. Two wrong parsers is the argument for not matching a
fixed-width table with a pattern at all: it now SPLITS on `;`, requires all
eight columns, and rejects endpoint-less rows. It carries a fire test built
from the real debris, and that test was verified to FIRE by mutating the column
count back to three and watching it refuse.
