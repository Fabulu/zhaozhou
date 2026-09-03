# Task Log: RUN-20260903-1958 - [Describe objective here]

**Created:** 2026-09-03 19:58 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260903-1958-island-recovery-and-known-defects/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-09-03 19:58 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260903-1958
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

## 2026-09-03 20:0x — the known-wrong list, worked in the owner's order

### A1 — a fit can no longer pass on Fmax while violating its storage law
`3bc25633`. The island brief's tripwires existed only as prose; nothing checked
them, which is why a cache with 2 M10K where its predecessor had 4 reported
`ok` at 98.66 MHz. `design/fit_targets.yml` gains `rules:`, the runner enforces
them, and `tools/quartus/check_fit_rules.ps1` audits every recorded row with no
Quartus at all. Against today's numbers it fails 4 of 4, including the one that
passed this morning.

Three self-inflicted faults on the way, all worth remembering: a regex that
silently extracted nothing and printed PASS for the block that violates all
three of its rules; a markdown backtick inside a double-quoted PowerShell
string (backtick is the escape character); and an em-dash in a script PowerShell
5.1 reads as ANSI -- where the python that stripped it opened with 'w', so the
encode error truncated the file to zero bytes and the retry cleanly processed
nothing.

### C1 — the cache storage, moved to a clock-only process
`97d3b637` (with the tile pipe). The file's own header claimed the registered
READ was "the entire reason this file was rewritten". It is half the reason.
The WRITES at `:412`/`:416` sat in the async-reset process, and an M10K has no
reset port. `data_r` and `tag_r` now live in the clock-only process that
already did the reads; `valid_r` stays where it is because it needs the reset a
memory cannot give.

Behaviour identical -- both assignments were already non-blocking, so a read and
a write to one address on one edge still returns the old contents, which is the
read-during-write mode an M10K provides. 10 checks pass, sustained throughput
unchanged at 1.02 clocks per access.

**FIT RUNNING** to answer the only question that matters here: does it infer?
Brief S25: if M10K does not infer, stop.

### B — Save the Renderer, commit one
`97d3b637`. TilePipe's 16-way priority encoder ran combinationally on
`pend_mask_r` and its output became `frag_addr` in the same period, entering
Early-Z's 256-bit presence lookup: ~2.6 ns of `column encode -> address ->
256:1 selection`. The cursor is now registered, using a staging opportunity
already in the protocol -- a coverage row is accepted only while the mask is
empty and a fragment only while it is not, so the accept is always one clock
ahead of the first fragment. No bubble, no delayed Early-Z decision.
74 + 12 + 16 checks pass including the pixel-CRC path.

`e814b772`. `run_shell_fit.ps1 -GpuPeriodNs` so the 105 MHz experiment can run
at 9.52381 ns instead of deriving an Fmax from a 10 ns placement. It throws
rather than fitting if the SDC substitution matched nothing.

### Next, in order
1. read the cache fit: **M10K >= 8 or stop**
2. the 105 MHz untouched-netlist experiment, five seeds
3. C2 TEXJOIN -- needs the C6-C8 banking design, not just a write move: its
   entry table is read COMBINATIONALLY through `head_q`, so no write change
   alone can make it memory
4. the production resource count, last

## IN PROGRESS at the moment the cache fit was due back

**`zhao_texture_fragrob` is written and lints clean; its TEST IS NOT WRITTEN.**

Per the brief S6.1 it is a NEW block beside v2, not an edit of it, and its port
list is deliberately identical so **v2 is the behavioural oracle**. Structure
per S6.3/S6.4: control state (valid, generation, required/arrived masks, aux
flags) in flops on purpose; payload in banks keyed by SAMPLE INDEX
(`desc_u_m[3]`, `desc_v_m[3]`, `desc_met_m[3]`, `res_rgb_m[3]`, `res_a_m[3]`,
plus context and aux), every one written AND read only inside a clock-only
process through registered addresses. Free-slot FIFO with a 16-cycle init
sweep, no scan. Issue and retire are two-stage state machines because a banked
descriptor cannot be picked combinationally -- that is the trade: latency for
structure.

Registered in `design/fit_targets.yml` with the S3.4 tripwires
(`min_m10k: 6`, `max_registers: 2500`, `max_dsp: 0`). NOT yet in
`design/blocks.yml` and it has no contract file.

### NEXT STEPS, in order
1. **the differential test** -- `tests/texture/fragrob_differential.cpp`:
   drive fragrob and `zhao_raster_texjoin_v2` from identical stimulus with a
   shared TMU/AUX responder, and compare the SEQUENCE of retired fragments.
   NOT cycle-by-cycle: fragrob has extra pipeline stages by design, so the
   contract being checked is same fragments, same order, same values.
   Directed cases to add: generation rejection of a stale return, ordered
   retire under out-of-order completion, allocation blocking when full.
2. `design/contracts/TEXTURE.FRAGROB.md` + a `blocks.yml` entry.
3. fit it against its tripwires; the acceptance question is **M10K, not Fmax**.
4. only then consider retiring v2 from the production manifest.
