# SPEC v1: Verify Packet-B metadata cadence

**Run ID:** RUN-20260914-0016
**Created:** 2026-09-14 00:16 UTC+02:00
**Status:** Complete
**Previous Version:** N/A

---

## Objective

Determine whether the composed Packet-B metadata path limits a held all-hit cache response stream to one acceptance every three clocks, and identify the smallest correction that preserves metadata/token identity and backpressure.

Success is an exact cycle recurrence against the selected top/cache/metajoin RTL, a contract comparison, and a bounded repair recommendation. This verification run is read-only; implementation belongs to the owning hardware run.

---

## Scope

**In Scope:**

- `zhao_texture_island_v3_top` metadata launch/pending/hold/raw-accept equations.
- `zhao_texture_cache_pipe_v2` response FIFO hold and acceptance behavior.
- `zhao_texture_metajoin_v2` synchronous response latency.
- Packet-B cache/TMU cadence requirements and existing composed controls.

**Out of Scope:**

- RTL, test, contract, manifest, generated-artifact, or Git mutation.
- Quartus, resource/Fmax measurement, board activity, and production adoption.
- Cache, planner, or metajoin rearchitecture beyond the smallest top-local repair shape.

---

## Constraints

- Read actual selected bytes; do not infer cadence from leaf throughput labels.
- Track launch, pending, hold, ready, and accept cycle by cycle.
- Preserve token/metadata identity and stall stability in any recommendation.
- Make no ALM/Fmax claim without a later named subsystem fit.

---

## Don't Retry

- Do not treat eventual correct outputs as throughput evidence.
- Do not blame the cache before separating cache availability from the top-level mandatory metadata hold bubble.
- Do not replace the synchronous join or add a second metadata implementation when a fall-through skid answers the observed recurrence.

---

## Open Questions

- Resolved: the pre-repair top accepted raw responses on cycles 2/5/8 after launches 0/3/6, so steady cadence was one per three clocks.
- Resolved: a live-result fall-through with registered hold only on stall removes one mandatory cycle while retaining the synchronous query and held-response law.
- Deferred to the owning run: execute a warm three-sample exact-II=2 control and full Packet-B regressions after implementation.
