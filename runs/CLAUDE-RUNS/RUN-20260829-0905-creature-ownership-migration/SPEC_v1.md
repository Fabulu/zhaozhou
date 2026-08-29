# Creature asset ownership migration — SPEC v1

**Authoritative report:** `Upheaval/creature/CREATURE-ASSET-OWNERSHIP-ARCHITECTURE.md`
(commit `4b26951`, branch `zixxtrixx-v9-cel-main`). Read in full before acting.
The report's own hardware-agent prompt is the task definition; this SPEC records
only what is lane-specific.

## Base SHAs, recorded at lane creation

    zhaozhou   f6626a0305578070dd14f4c3025cda92aca8e167  (zixxtrixx-v8-closeout)
    Upheaval   fe6a5d827aae4cbe4f057dc963afc28768625c65  (zixxtrixx-v9-cel-main)

**The Upheaval tip is MOVING.** It was `4b26951` when I first read the report
and `fe6a5d8` twenty minutes later — the v9 art lane committed
"Require smooth toon lighting continuity" in between. The report commit is
still an ancestor, so nothing was rewritten, but this is the direct evidence
that the freeze is real and that this lane must not assume a stable Upheaval
tree. Rebase on the stated final tips at handoff, as the report requires.

## Isolation

    lane clones   C:\programmieren\zencrifice\_lanes\creature-migration\{zhaozhou,Upheaval}
    branch        creature-ownership-migration  (zhaozhou)

Outside both of the owner's shared checkouts, deliberately. The report exists
partly because two lanes shared one tree; putting the migration lane inside the
tree it is trying to disentangle would be the same mistake with a new name.

**This is not a theoretical hazard.** On 2026-08-29 in the hardware lane, two
mutation sweeps defaulted to the build directory an interactive session was
using. One was killed mid-run and left a MUTANT applied to shipped RTL. A guard
caught it and nothing reached a commit — but the guard existed only because the
same class of accident had put a mutant into a pushed commit the day before.

## The freeze, and what this lane may do under it

**May not touch** until an explicit handoff gives BOTH final main SHAs and
confirms the modelling agent *and its background jobs* have stopped:

    zhaozhou/tools/reel/zixxtrixx.h
    zhaozhou/tools/reel/zixxtrixx_page*.h
    Zixx-specific regions of zhaozhou/tools/reel/zhao_reel.cpp
    zhaozhou/tools/pack/mkcreaturepage.py
    zhaozhou/tools/reel/zixx_*.cpp
    Upheaval/creature/Zixxtrixx/**
    v9 website manifests / renders / archive
    generic cel/outline renderer regions changed by v9

**May do now** — the report's own step 2, additive and in new paths:

1. A generic creature-source/provider seam.
2. Generic reel machinery extracted into a linkable library.
3. Reusable validation primitives.
4. `tests/fixtures/creatures/synthetic_chain/` — a genuinely synthetic fixture,
   not a renamed Zixxtrixx.

The old path keeps working throughout. Nothing is moved or deleted before the
handoff.

## The law this migration is really about

`spec/creature_rules.md` and both CLAUDE.md files, but above all: **an ownership
move, not an art pass.** Preserve the final v9 model constants, texture bytes,
clip payloads, pose/sequence CRCs, every canonical render-frame CRC, and every
probe result. A changed output is a FAILURE to be explained, never a new golden
to be blessed.

And the art law applies to the verification: component checks passing is not
likeness evidence. Fixed-camera and contact-sheet evidence gets *looked at*.

## Definition of done

Not "a provider interface exists". Done is: the production source and probes
have moved, the reverse dependency is gone, zhaozhou builds and proves its
generic creature path in a clean clone with **no Upheaval checkout present**,
two clean generations are byte-identical, deleting generated output and
rebuilding changes no tracked file, exact cross-repository SHAs are recorded,
and every logical commit is pushed.

## Ordering against the hardware lane

The owner gave the hardware lane explicit direction immediately before this
report arrived: a reusable uniform/scalar path for prepared RING, and a bounded
multi-outstanding dispatcher. That work is live on `zixxtrixx-v8-closeout` and
is not dropped for this. The two lanes are separate clones, separate branches,
separate build trees.
