# Manafold Pass 17 repaired final-bank review — batch 02

**Date:** 2026-09-19
**Renderer MD5:** `6FD6147B43056E823F0487F9DFBB40BA`
**Bank manifest SHA-256:** `7dbc84e2eb4bcc1263d11209709d862b822d77aeb60875bedbdcc321a34427e1`
**Exact raw root:** `C:\programmieren\zencrifice\manafold-p16\pass17-final2-reel-28`
**Exact sheet root:** `C:\programmieren\zencrifice\manafold-p16\pass17-final2-sheets`
**Verdict:** **PASS — 4/4 subjects**

## Review basis

`PASS17-FINAL-BANK-INTEGRITY-V2.md` proves the repaired bank is byte-identical to the rejected bank for Damage and Drift. Their already-complete visual verdicts therefore transfer exactly. Death Drop and Death Gutter are the only changed subjects; every tile of both current sheets was freshly reviewed (450 + 590 = **1,040/1,040 frames**) and their fade/contact/settle windows were reopened at native or nearest-neighbour 2x scale.

## `manafold-damage` — PASS by exact equality

All 464 frames are byte-identical to the reviewed batch-02 generation. The four impacts, squash/recovery, carrier lag, rear socket, signed spans and effect state therefore retain their accepted continuous read. Sequence CRC remains `0xF6FFFAF1`.

## `manafold-death-drop` — PASS from fresh current pictures

The full 450-frame sheet now shows one continuous loss of mana. The bright cyan/white/navy figure begins fading after the float-fail beat, progressively loses colour, soft-opaque backing and mote hearts across the diminishing contacts, and is visually negligible before the grounded settle. It does not become a broad black mass and then disappear.

The exact `f0228–f0238` 2x window confirms the former `f0233→f0234` blocker is gone: body, eyes, antenna, ground penetration and the remaining tiny blue/violet effect details advance continuously on both sides of the boundary. The corpse tail through f0449 remains grounded, dark and dead without lingering mana or resurrection.

Durable current-bank evidence:

- `PASS17-FINAL2-BATCH02-DEATH-DROP-FADE-2X.png`
- `PASS17-FINAL2-BATCH02-DEATH-DROP-STAGES-NATIVE.png`

## `manafold-death-gutter` — PASS from fresh current pictures

All 590 current frames preserve the distinct mana-first performance: cyan/white energy, navy backing and mote hearts fade progressively while the body still fights upright; the effect reaches zero around f0224 without a one-frame black or topology cutoff. The subsequent staged sags, one-at-a-time limp antenna read, final collapse and two small bounces remain coherent.

The exact `f0378–f0388` 2x settle window keeps body, empty outlined O, rear socket, eyes and declared penetration continuous. The long tail remains dark and dead, with no residual mass or restart.

Durable current-bank evidence:

- `PASS17-FINAL2-BATCH02-DEATH-GUTTER-FADE-NATIVE.png`
- `PASS17-FINAL2-BATCH02-DEATH-GUTTER-SETTLE-2X.png`

## `manafold-drift` — PASS by exact equality

All 300 frames are byte-identical to the reviewed batch-02 generation. The intentional screen traversal/wrap, rig pose and antenna attachment therefore retain their accepted read. Sequence CRC remains `0x830E581D`.

## Batch disposition

**PASS 4/4:** Damage and Drift transfer by exact bytes; both changed deaths pass fresh every-frame review. No effect cutoff, brightness/topology reset, particle pop, corpse resurrection, carrier/socket discontinuity, eye-child fault, contact regression or unintended lingering mass remains. The repaired exact bank may proceed to batch 03.

No exploratory plate is included in the durable manifest above.
