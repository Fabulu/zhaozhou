# Phantom reference models — a full audit

**Date:** 2026-08-21
**Method:** every `reference_model` in `design/blocks.yml`, searched as a plain
symbol across every `.hpp`/`.cpp`/`.h` under `reference/`.

**Result: 25 of 73 declared reference models name a symbol that exists nowhere.**

This had been found nine times one at a time, each during the build of the block
that tripped over it. Auditing all of them at once turns a recurring surprise
into a known quantity.

## Why it matters

A `reference_model` is what "the RTL is correct" is measured against. Ledger rule
V17 requires it to resolve before a block can pass `REFERENCE_COMPLETE`, so none
of these has blocked anything yet — all 25 are still `SPECIFIED`. They will each
block their own block's first advance, one at a time, exactly as
`zref::Skin` (GEOM.SKIN) and `zref::DebugTrace` (DEBUG.TRACE) did.

So this is not a ledger defect. The names are aspirational placeholders written
when the block was specified, and the ledger is correctly refusing to treat them
as evidence. The finding is about **cost**: every remaining block needs a
reference written before its RTL can be verified against anything.

## The list

| Phase | Block | Declared | Subsystem |
| --- | --- | --- | --- |
| 0 | SYS.CDC | `zref::CdcFifo` | platform |
| 0 | SYS.PLL | `zref::SysPll` | platform |
| 0 | SYS.RESET | `zref::ResetSequencer` | platform |
| 2 | INPUT.SNAC | `zref::SnacAdapter` | input |
| 7 | FIELD.PROGCACHE | `zref::ProgCache` | field |
| 8 | GEOM.MESHFETCH | `zref::MeshFetch` | geometry |
| 8 | GEOM.PROJECT | `zref::GeomProject` | geometry |
| 8 | GEOM.VDECODE | `zref::VertexDecode` | geometry |
| 8 | GEOM.WCACHE | `zref::WorldCache` | geometry |
| 8 | MEASURE.HISTOGRAM | `zref::MeasureHistogram` | measure |
| 9 | GEOM.LOOM | `zref::TransformLoom` | geometry |
| 9 | GEOM.WARP | `zref::GeomWarp` | geometry |
| 10 | PART.COLLIDE | `zref::ParticleCollide` | particles |
| 10 | PART.EXPAND | `zref::ParticleExpand` | particles |
| 10 | PART.LADDER | `zref::RepresentationLadder` | particles |
| 10 | PART.SOFT | `zref::SoftParticles` | particles |
| 10 | PART.SPAWN | `zref::ParticleSpawn` | particles |
| 10 | PART.STATE | `zref::ParticleState` | particles |
| 10 | PART.UPDATE | `zref::ParticleUpdate` | particles |
| 11 | FORGE.PRIM | `zref::ForgePrim` | forge |
| 11 | POST.COMPOSITE | `zref::PostComposite` | compositor |
| 11 | POST.ECHO | `zref::PostEcho` | compositor |
| 11 | POST.GATHER | `zref::PostGather` | compositor |
| 11 | TWOD.PLANE | `zref::TwoPlanes` | compositor |
| 11 | TWOD.SPRITE | `zref::HudSprites` | compositor |

## They are not all the same problem

Resolving them one at a time has hidden that they fall into three kinds, and the
right fix differs.

**1. The law is already shipped under another name (cheapest).** This was
`CMD.DECODER`, whose `zref::CmdDecoder` did not exist while the packet validation
law did, and the forty `zref::fieldir::*` names, whose interpreter is
`zfield::interpret`. The fix is a thin view that forwards, plus a note saying so.
The trap is writing a *second* implementation and calling the agreement between
two things written by the same hand on the same day a verification.

Candidates here: `GEOM.PROJECT` (`zref::render::project_vertex` is real and does
this), and probably `GEOM.VDECODE` and `FIELD.PROGCACHE`.

**2. The law genuinely has no implementation.** This was `DEBUG.TRACE`. The wire
format was ratified in `capture_format.md` and the sources in the charter, but
nothing implemented the ring. A real reference has to be written, and the open
questions the spec leaves have to be answered in one place with the alternatives
recorded.

Candidates: all seven `PART.*` blocks, and the four compositor blocks.

**3. A scalar reference is the wrong idea entirely.** `SYS.PLL`, `SYS.RESET` and
`SYS.CDC` are the clear cases: a PLL has no scalar model, and a reset sequencer's
correctness is a timing and sequencing property, not a function from inputs to
outputs. Naming a C++ function for them was a category error at specification
time.

These three need a different evidence kind — formal properties and timing
closure — and the ledger has no way to say that today. It offers one
`reference_model` field and one ladder, so a platform block either carries a
fiction or cannot advance. That is worth fixing in `tools/ledger` before those
blocks come up, not while they are being built.

## What this changes about the plan

Nothing about the order, but it does change the cost of each remaining block.
The pattern that has worked four times today is:

1. check the reference resolves — **first**, before any RTL;
2. if it does not, decide which of the three kinds it is;
3. write or forward the reference, and say in the file which kind it was;
4. only then write the RTL and the differential.

Step 1 takes a minute. Discovering it at step 4 has cost noticeably more than
that every time it has happened.
