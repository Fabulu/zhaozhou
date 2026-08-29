# Hardware/toolchain lane — status for the art lane

**Stable path on purpose.** Not a run folder: every pass creates a new one, so
anything left in the current run is orphaned by the next. This file stays here
and gets updated in place.

**Last updated:** 2026-08-29, by the hardware lane.
**Branch:** `zixxtrixx-v8-closeout` (zhaozhou). Everything below is pushed.

---

## Creature-ownership migration — acknowledged, not started here

I have read `Upheaval/creature/CREATURE-ASSET-OWNERSHIP-ARCHITECTURE.md`
(commit `4b26951`, branch `origin/zixxtrixx-v9-cel-main`) including the
ready-to-use hardware-agent prompt at the end.

**It will get its own run and its own isolated clones.** I am not interleaving
it with live Field RTL work in the shared checkout. That is not caution for its
own sake — it happened today: two mutation sweeps defaulted to the build tree
an interactive session was using, and a sweep killed mid-run left a MUTANT
applied to shipped RTL. A guard caught it and nothing reached a commit, but two
lanes sharing one tree is exactly the failure the report is trying to end.

### The v9 freeze is being respected

I have touched **none** of the frozen paths. Nothing in:

    zhaozhou/tools/reel/zixxtrixx.h
    zhaozhou/tools/reel/zixxtrixx_page*.h
    Zixx-specific regions of zhaozhou/tools/reel/zhao_reel.cpp
    zhaozhou/tools/pack/mkcreaturepage.py
    zhaozhou/tools/reel/zixx_*.cpp
    Upheaval/creature/Zixxtrixx/**
    v9 website manifests / renders / archive

My changes today are confined to `fpga/rtl/field`, `fpga/rtl/synth`,
`tests/differential`, `tests/CMakeLists.txt` and `tools/sweep_*`.

I have also deliberately left `reference/src/zcreature/creature_sim.cpp` and
`reference/src/zrender/rast.cpp` alone, even though they are what currently
holds `format_check` red in the fast lane. They are the v9 lane's live edits and
reformatting a file another session is editing is a collision, not a fix.

### What I need at handoff

* **Both** final pushed main SHAs.
* Confirmation the modelling agent **and its background jobs** have stopped.
  That second half is not pedantry — "stopping an agent does not stop its
  background work" is a documented lesson in our CLAUDE.md and it cost a day.

### One thing I would flag back

The report's target layout gives `zhaozhou/tools/reel/` "a generic reel library
and thin generic CLI". `zhao_reel.cpp` currently also hosts **non-creature**
reel subjects. I intend to treat those as generic and keep them in zhaozhou
rather than moving them to Upheaval. Say so if that is wrong.

---

## Where my records live

* **`STATUS.md`** — top of file, newest first, written for Fabian in plain
  language. This is the channel.
* **`runs/CLAUDE-RUNS/RUN-20260828-2111-spline-hot-and-ring-prep/TASK_LOG.md`**
  — full engineering detail, including the mistakes.
* Sweep logs in that same run folder.

## What the hardware lane has done today

    854a3df  SPLINE's table lookup fixed; 6930 checks against the oracle
    214cf41  a second service on the path; SPLINE runs hot end to end
    ead2649  a SHIPPED DEADLOCK fixed -- see below
    2196544  every op the opcode table advertises is now actually served
    c532625  DISPATCH closes 30/30
    a472b0e  SVCPATH closes 37/37

**The deadlock is the one worth knowing about**, because it was live in
everything built on this engine: two contexts running *different* long
operations hung the machine forever, with no timeout and no flag. Normal
traffic, not a corner case. It was pre-existing and not caused by my changes,
which I proved by isolating it with two ops served by the *old* unit only.

**The second** is that the opcode table advertised four operations
(NORMALIZE2/3, ROT2/3) that the service path could not compute. They were being
answered by the noise unit and writing plausible wrong numbers into real
registers, with only a flag to say so.

## In flight

A 48-mutant checkpoint sweep on the four-service path. Then, on Fabian's
explicit direction: a reusable uniform/scalar path for prepared RING, and a
bounded multi-outstanding dispatcher.
