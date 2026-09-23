# Pass 25, the BACK-BALL packet: Hover's rear, diagnosed and damped

**Date:** 2026-09-23
**Direction:** `Upheaval/creature/Manafold/OWNER-DIRECTION-26-2026-09-22.md`
item 3, re-opened by the owner after the rear-calm ladder proved to be the
wrong lever.
**Worker:** Claude (sole Opus implementer; no sub-agents, no Qwen, no HomeAI)
**Source:** Zhaozhou `manafold-pass25`, on top of `79d69a58`
**Linked from:** `P25-IMPLEMENTATION.md` §3, which this supersedes on item 3.
**Not done here, by instruction:** the publication bank, the encode, the merge,
the deploy.

---

## THE ONE-PARAGRAPH ANSWER

Nobody had ever measured what moves Hover's back ball, and both failed levers
failed for the same reason: they were pointed at things that do not move it.
A committed probe — `tools/reel/manafold_backball.cpp`, `mback` — now decomposes
the rear's posed-surface motion by **muting one authority at a time on the posed
result**. It found that **the End swell's own position is the calmest back ball
in the bank** (0.99 mm per presentation sample, a tenth of a native pixel), that
**65 % of the little it does is the socket following the breathing body**, that
**90 % of its ORIENTATION is the rear rod aim**, and that the motion an eye can
actually see is **carrier C and the C→End rod** — which are **81 % "the
antenna's upstream life" with no single station owning more than 9 %, because
the chain is a travelling wave whose stations partially CANCEL**. Freezing
station B alone makes the last rod move **146 % more**. That is why four passes
of gain levers failed or backfired. The complaint is a FREQUENCY complaint
("finicky"), so the repair is a **filter**: `kBackBallDampPm`, a centred
zero-phase moving average over the authored keys of stations A, B and C, run
before the closure is solved. Shipped at **gain 700 per mille over a 21-key
window on bake slot 0**, chosen by eye. Carrier C's travel falls 18.5 → 13.0 mm
per sample and its angular rate 3.29 → 1.85 deg per sample; the back/front
angular ratio, **1.08 on Hover and under 1.00 on every other live clip**, comes
back to 0.99. **20 of the 22 live subjects are byte-identical frame by frame.**
Hover and Inspect both change, because they are one bake under two cameras;
Direction 26 authorises that in terms and it is declared rather than done
silently.

---

## 1. THE DIAGNOSIS

### 1a. The instrument, and why it works on the BAKED clip

`mback` builds the shipping bank and then, for each authority, makes a **named,
exact edit of the baked clip**, re-poses it, and measures the posed SKIN. It
never reads a rendered frame and never diffs two configurations' pixels — the
category error that produced pass 24's "52×".

The authorities are not separable at pose time; they are composed into three
baked channels, and the socket's orientation is itself a product:

```
q_socket = normalize(qd * authored)
qd       = q[JunctionF] * q[Neck] * q[A] * q[B] * q[C] * q[D]
```

so `qd` — the arm's solved arrival frame, i.e. the rear rod aim — can be held at
sample 0 while `authored` (the End ambient station plus the knead's B2 wag) runs
live, and vice versa. Freezing is to sample 0; every motion metric here is
offset-invariant, so that choice changes no reported number. Keys **and**
presentation midpoints are edited identically — a mute that skipped the
midpoints would leave half the samples live and report half the truth.

Two facts make the reading exact rather than approximate:

* the loop's skin carries **no deform authority** (`kLoopStretchStrength == 0`,
  so every loop ring's role is `kNone`), so posing without a deform frame is
  correct here;
* the balls and rods are read as **posed surface**, through
  `rods_ring(i).role/elem`, not as bone origins — mrear's pass-20 lesson, where
  a ball travelled half a metre with its nominal bone's origin never moving.

### 1b. The decomposition

Hover, bake slot 0, 600 presentation samples, root-local, posed surface.
Receipt: `P25-BB-RECEIPTS/decomposition-undamped.txt`.

**Baseline, undamped:**

| read | path | per sample | vmax | jerk max | jerk rms |
|---|---:|---:|---:|---:|---:|
| back ball (End swell) position | 592.7 mm | **0.988 mm** | 2.683 | 0.689 | 0.291 |
| back ball orientation | 1403.5 deg | **2.339 deg** | 5.310 | 1.454 | 0.490 |
| last rod (C→End) | 5619.0 mm | **9.365 mm** | 19.709 | 10.305 | 4.388 |
| carrier C ball | 11108.4 mm | **18.514 mm** | 37.840 | 17.226 | 6.586 |

**Share of the motion, muting one authority at a time** (share = 1 −
muted/baseline). Shares do **not** sum to 100 % and are not claimed to: the
authorities overlap by construction, and a negative share means freezing that
authority makes the part move MORE.

| authority muted | END pos | END ang | END jrms | C pos | C ang | rod pos |
|---|---:|---:|---:|---:|---:|---:|
| socket follows breathing body | **65.0 %** | 0.0 % | **54.0 %** | 0.0 % | 0.0 % | 0.0 % |
| rear rod aim (arm arrival `qd`) | 10.9 % | **90.3 %** | 9.0 % | 0.0 % | 0.0 % | 0.0 % |
| End authored rot (ambient + knead B2) | 0.9 % | 3.1 % | −4.1 % | 0.0 % | 0.0 % | 0.0 % |
| antenna upstream life (F/N/A/B/C/D) | 0.0 % | 0.0 % | 0.0 % | **81.1 %** | **100.0 %** | **40.4 %** |
| … station F alone (junction) | 0.0 % | 0.0 % | 0.0 % | 9.0 % | 1.4 % | 8.5 % |
| … station Neck alone | 0.0 % | 0.0 % | 0.0 % | 5.8 % | 8.2 % | 5.8 % |
| … station A alone | 0.0 % | 0.0 % | 0.0 % | 0.2 % | 20.6 % | **−66.4 %** |
| … station B alone (the peak) | 0.0 % | 0.0 % | 0.0 % | −6.6 % | 2.2 % | **−146.4 %** |
| … station C alone | 0.0 % | 0.0 % | 0.0 % | 0.7 % | 22.7 % | **−79.5 %** |
| … station D alone (closure solver) | 0.0 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % | **−114.1 %** |
| carrier nodule offsets (swallow + ambient) | 0.0 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % | −22.5 % |
| rear span/bow helpers | 0.0 % | 0.0 % | 0.0 % | 0.0 % | 0.0 % | −22.5 % |
| body bob + attitude (root) | 0.0 % | −0.0 % | 0.9 % | 0.0 % | −0.0 % | 0.0 % |

**Worst frame** (sampled by badness, not by index — sample 367, the highest
back-ball jerk in the clip): socket-follows-body **74.5 %**, rear rod aim
16.6 %, End authored rotation 3.0 %, everything else 0.0 %.

### 1c. What that says, plainly

1. **The End swell — the thing the codebase calls the back ball — barely moves.**
   0.988 mm per sample is under a tenth of a native pixel at this framing. Its
   position is 65 % the body's breath dragging the socket across the body
   surface, and at the worst frame 74.5 %. **Damping it would be invisible**, and
   that is worth saying out loud, because the lever Direction 25 named and pass
   24 built — the rear AMBIENT scale on the End station — was aimed exactly
   here. It was not too weak. It was pointed at the calmest part of the rear.
2. **What an eye sees in the rear is carrier C and the C→End rod** — 18.5 and
   9.4 mm per sample, an order of magnitude more.
3. **The back ball's ORIENTATION is 90.3 % the rear rod aim.** The End ball is a
   sphere rigid on the socket, so this shows as the whole rear join swinging
   with the arm's arrival frame rather than as the ball travelling.
4. **The authored swallow beat, the carrier nodule schedule, the knead's own
   offsets and the rear span/bow helpers move carrier C by exactly ZERO**, and
   freezing the helpers makes the rod move 22.5 % MORE. The swallow and the
   ambient carrier travel reach the skin through helper translations; the rear's
   motion does not come from them.
5. **No single station owns the rear.** F 9.0 %, Neck 5.8 %, A 0.2 %, B −6.6 %,
   C 0.7 % — against 81.1 % for the six together. The chain is a travelling wave
   with a per-station lag (by design: "the grip travels up the antenna"), and its
   stations **partially cancel**. Freeze one and you break a cancellation:
   station B alone costs the last rod **+146 %**.

Point 5 is the whole history of this complaint in one line. It is why lowering
the rear ambient moved 178 px, why the carrier-C calm ladder moved 1.3 %, and
why pass 25 found that *removing the knead dip makes C travel further*. **A gain
on any one authority cannot calm a wave whose parts already cancel.**

### 1d. The owner's premise, checked — and it is NOT "Hover moves the most"

"Every other clip is right" is a claim about a comparison, and nobody had made
it. `mback --census` reads the same quantities on every bake slot.
Receipt: `P25-BB-RECEIPTS/census.txt`.

**The first version of that census printed clip TOTALS and made Hover look like
the worst clip in the bank by 2×.** Hover is the longest clip in the bank — 300
keys, 600 presentation samples, against 90–200 for most of the rest — so a
summed path measures duration at least as much as busyness. That is CLAUDE.md's
mismatched-comparison law, committed by the very tool written to avoid it, and
caught one table later. Every rate below is **per sample**, and the tool now
prints totals marked as totals so the mistake stays visible.

Per sample, undamped, the live clips:

| | Hover (slot 0) | the busiest others | the calm others |
|---|---:|---|---|
| carrier C travel, mm | 18.514 | taunt2 24.0, hasty 23.4 | channel 11.3, rest 13.6 |
| carrier C jerk rms | 6.586 | taunt2 9.04, taunt 8.53, hasty 8.48 | flight 5.35, channel 5.32 |
| **carrier C angular, deg** | **3.285 — the highest in the bank** | hasty 2.933, drift 2.856 | rest 2.565 |
| **END angular / FRONT angular** | **1.08 — the only live choreography ≥ 1.00** | drift 0.99, hasty 0.98, flight 0.98 | death-drop 0.39 |

(Slot 23 — crackle's fixed-camera bake of this same choreography — reads the
same 1.08, because it is the same animation. It is deliberately left undamped:
see §3.)

**So Hover's rear was never the most violent in the bank.** Three clips shake
carrier C harder. What is singular about Hover is the **ratio**: it is the only
live clip on which the BACK of the antenna turns MORE than the FRONT. A chain
driven from the body should attenuate toward its far end; on Hover it amplifies.
And it does that on the long idle — the clip where the body is otherwise nearly
still and the one the owner watches most. That is the owner's sentence,
measured, and it is also the answer to "why is every other clip right": on
hasty and taunt2 the whole creature is working, so a busy rear reads as energy.

It also means a bank-wide ceiling would be false, and the gate says so: it
judges slot 0 and prints every other slot as `info`, where several read well
above Hover's number and are correct.

---

## 2. THE MECHANISM

### 2a. A filter, not a gain

The owner's words are a frequency statement — "too finicky and moves too much" —
and Direction 20's "a bit wiggly" says the low-frequency swing must survive. So:

**`backball_damp()` (`manafold_clips.h`): a CENTRED moving average over the
authored KEYS of exactly three local rotations — HingeA, HingeB and HingeC —
applied to one bake slot, before the closure is solved.**

* **Centred, and the clip is cyclic, so it introduces NO LAG.** A one-pole
  filter would drag the beat late, which is a different defect wearing the word
  "damping".
* **It runs BEFORE `finalize_rear_follow`**, so HingeD re-aims and the socket
  re-solves against the damped chain. Damping after the closure would leave the
  arm aimed where carrier C no longer is — the pass-19 rear rip with a new cause.
* **The window is in authored keys (30 Hz)**, before `compile_creature` bakes the
  midpoints, so the presentation interpolation follows the damped keys instead of
  fighting them. 21 keys is 700 ms.
* **`pm <= 0` takes an early return that touches no byte.**
* A looping clip wraps; a one-shot **clamps** — wrapping a death would average
  the corpse's last key with the living first key and flash the animal alive,
  the exact fault `Clip::hold_last` exists for.

**And it is called from INSIDE `finalize_rear_follow`, not beside it.** The
first version put a loop in `manafold.h`, which is where the bank is built —
and **six** places build a clip and finalize it: the bank plus `mnodule`,
`mprobe`, `mjointpub`, `mqa` and `mspan`. A damping applied at only one of them
gives the gates a creature the renderer does not draw, which is precisely the
defect pass 24's reviewer found in mbolt, where the gate held its own copy of
the per-subject lightning configuration and nothing bound it to the renderer's.
It happens that all five gate sites build slots 7, 11, 12, 13, 16 and 21, so
every one is a no-op at the shipped table today — **and "it is a no-op today" is
not a structure.** Moving the call to where the closure is solved makes the two
impossible to configure apart: a clip that is finalized is damped, and a clip
that is not finalized is not a production clip. `mnodule` independently rebuilds
slot 16 and asserts it equals the banked one quat for quat, so a future
divergence here is caught rather than argued about.

**The refactor is byte-neutral**, verified by re-rendering the whole 22-subject
bank across it: `diff` of the two CRC sets is empty on all 22 rows
(`P25-BB-RECEIPTS/byte-identity-frames.txt`). Every binary in the receipts was
rebuilt after it, and the matrix was re-run from scratch against those binaries
— the earlier run was stopped and discarded rather than quoted, because a
matrix that measured a different arrangement is a matrix about a different
creature.

### 2b. Why A, B and C, measured — not assumed

The front ball's pivot is `junctionF + R_F·arc0 + R_FN·arc1`: it depends on
**JunctionF and Neck and on nothing else in the damp set**. So "Hover's front
ball is already right; leave it" is honoured **by construction**. Measured, the
front ball's position moves 8.904 → 8.922 mm per sample, +0.2 %.

Four station sets were built and measured at full gain, window 21:

| damped stations | front ball A pos | B pos | **C pos** | **C ang** | END/FRONT ang |
|---|---:|---:|---:|---:|---:|
| none (shipping pass 25) | 8.904 | 14.697 | 18.514 | 3.285 | 1.08 |
| B + C | 8.904 | 14.746 | **19.104 — WORSE** | 2.335 | 0.95 |
| A + B | 8.931 | 11.559 | 11.960 | 2.378 | **1.22 — worse ratio** |
| **A + B + C (shipped)** | 8.931 | 11.559 | 12.004 | 1.403 | **0.95** |
| Neck + A + B + C | **6.289 — front ball moved** | 9.354 | 10.601 | 1.240 | 0.90 |

**B+C alone makes carrier C travel FURTHER** — the cancellation again, exactly as
the station-mute table predicted (freezing B alone: C pos −6.6 %). A+B calms the
travel but leaves the back turning 1.22× the front, worse than the fault.
Adding Neck calms everything and moves the front ball, which is forbidden.
**A+B+C is the only set that calms the rear and leaves the front ball's position
where the owner accepted it.**

**The honest cost, declared:** one quaternion serves both a station's own spin
and everything downstream of it, and there is no second channel. So the FRONT
ball's own spin is damped with the station — 2.166 → 1.396 deg per sample at the
shipped value. Its POSITION, its size, its ink and its place in the loop are
unchanged. Looked at at 3× and 4× across consecutive frames, the front ball
reads the same; it is a sphere, and its texture turning slightly slower is not
something the shot shows. If the owner disagrees, the fix is a second rotation
channel at A, which is a new mechanism and not this packet.

### 2c. The ladder, and the value chosen by eye

Window 21 keys; gain laddered at **0 / 300 / 500 / 700 / 1000**, each rung
rendered in full on both the orbiting camera (hover) and the fixed one
(crackle's bake, with slot 23 damped for the diagnostic only) and looked at at
native, 3×, 4×, 5× and 6×. Windows 5 / 7 / 11 / 15 / 21 / 31 were laddered
first. Receipt: `P25-BB-RECEIPTS/ladder.txt`.

**Two findings from the window ladder that a single metric would have hidden:**

* **Narrow windows make the WORST frame worse.** At window 5, carrier C's jerk
  rms falls (6.586 → 6.143) while its jerk **max rises to 20.598 against the
  undamped 17.226**. A gate watching only the rms would have called window 5 an
  improvement over a rear whose worst moment got 20 % more violent. Window 21
  improves both.
* **The filter saturates.** Window 31 buys almost nothing over 21 (C path 7087
  against 7202) and its jerk max starts climbing again.

At window 21 every metric improves monotonically and the jerk max never
regresses. The gain was then chosen by eye:

| gain | C travel mm/sample | C jerk rms | C angular | last rod mm/sample | END angular |
|---:|---:|---:|---:|---:|---:|
| 0 | 18.514 | 6.586 | 3.285 | 9.365 | 2.339 |
| 300 | 15.800 | 5.696 | 2.641 | 8.032 | 1.884 |
| 500 | 14.241 | 5.257 | 2.230 | 7.278 | 1.615 |
| **700 (shipped)** | **13.005** | **4.929** | **1.850** | **6.673** | **1.380** |
| 1000 | 12.004 | 4.873 | 1.403 | 6.194 | 1.143 |

**The visual reason for 700.** On the before/after plate the rear rod holds its
width and lean across six consecutive frames while the loop still visibly
breathes: the rear is one coherent shape drifting slowly, instead of a bar whose
width and angle change every other frame. At 500 the rod still ticks. At 1000 it
reads carried rather than alive, and the measurement agrees from the other side
— 1.403 deg per sample puts Hover's carrier C **below rest (2.565) and channel
(1.741)**, calmer than the bank's calmest clips, which is not what an idle with
a living antenna should be. 700 lands it among them rather than below them, and
brings the back/front angular ratio to 0.99 — inside the 0.39–0.99 band every
other live clip occupies.

---

## 3. SCOPE: 20 of 22 BYTE-IDENTICAL, AND WHY IT IS 20 AND NOT 21

`kBackBallDampClipPm` is keyed on the **bake slot** (`Clip::slot_id`), not on the
schedule slot, and that distinction is the difference between changing two
subjects and three. Slot 0 is the orbiting idle bake; slot 23 is the separate
fixed-camera bake `crackle` and the six mana tiles play, and every *scheduled*
layer on slot 23 is called with slot 0. A knob read inside `antenna_knead`
reaches all three; this one, read in `finalize_rear_follow`'s caller off
`c.slot_id`, reaches exactly the bake it is applied to.

**Slot 0 is TWO live subjects: `manafold-hover` and `manafold-inspect`.** They
are one baked clip under two cameras (`cam_k` 360000 and 460000) and **no
per-clip knob can separate them.** Separating them would need a third bake of
the idle — a new clip slot, and with it a new entry in every per-slot table in
`manafold_art.h`, which is the orphaned-index fault this creature has already
shipped twice. On a final pass that is the wrong trade.

Direction 26 authorises the alternative in terms: *"Other clips stay
byte-identical unless the owner's fault is visible there too — in which case say
so rather than changing them silently."* Inspect is the **same choreography under
a tighter camera**, so the fault is more visible there, not less. Leaving Inspect
undamped would ship two subjects of one animation with different rear behaviour.
**Said, not done silently.**

**Proved at frame level**, not only by sequence CRC — every `.rgb` compared byte
for byte against the pass-25 shipping render:

| subject | frames | differing |
|---|---:|---:|
| manafold-rest | 400 | **0** |
| manafold-drift | 300 | **0** |
| manafold-taunt2 | 240 | **0** |
| manafold-death-drop | 450 | **0** |
| **manafold-crackle** | 600 | **0** |
| manafold-hover | 600 | 600 |
| manafold-inspect | 600 | 600 |

**Crackle at 0 is the containment proof**: it is the same choreography, damping
switched off, on the separate bake. The whole 22-subject bank is covered by the
matrix's `e-ship`, `e-bb-off` and `e-bb-scope` legs.

**And crackle therefore still carries the fault**, which is said here rather
than left to be noticed: it plays this same choreography and reads the same 1.08
back/front ratio. It is left undamped for two reasons — the owner named Hover,
and a byte-identical control for a brand-new mechanism inside the shipping bank
is worth having on the pass that closes the creature. **If the owner wants
crackle calmed too it is one entry**, `kBackBallDampClipPm[23] = 700`, and
nothing else changes. It is open issue 1 below.

| | pass 25 | back-ball packet |
|---|---|---|
| manafold-hover | `0x8124751D` | `0x89EBA648` |
| manafold-inspect | `0xEF7FBD6D` | `0x3A179F08` |
| the other 20 | unchanged | unchanged |

**EXACT-OFF, verified before anything else was measured:**
`ZHAO_U02_BACKBALL_DAMP_PM=0` reproduces the pass-25 bank on **all 22 subjects,
bit for bit** (`diff` against `P25-RECEIPTS/crcs-ship.txt`, zero rows differ).
The new code is byte-neutral when off.

Nothing else moved: no bound, floor or window; the pass-21 rig, the knead,
B-lowest, Trick's plant, the pass-25 clearance and the eye ambience are all as
they were, and the matrix asserts each.

---

## 4. THE GATE, AND EVERY CONTROL FIRED

`manafold-backball.exe --gate`, measured on the posed surface, judging bake
slot 0 and printing every other slot as `info`.
Receipt: `P25-BB-RECEIPTS/gate-and-controls.txt`.

| leg | quantity | bound | shipping |
|---|---|---|---|
| G1 | carrier C positional jerk rms — the "finicky" operand | ≤ 5.600 | **4.929** |
| G2 | End swell angular rate — 90 % the rear rod aim | ≤ 1.700 deg/sample | **1.380** |
| G3a | carrier C travel — **the ART floor** | ≥ 11.500 mm/sample | **13.005** |
| G3b | carrier C angular — **the ART floor** | ≥ 1.200 deg/sample | **1.850** |

**G3 is the half of this gate that protects the art.** Every ceiling here is
satisfied better by a deader creature, and a bound that can only be met by
removing motion is a bound that asks for a corpse — this creature has already
shipped one knob whose entire job was to remove motion. So the floors assert
Direction 20's "a bit wiggly" as a requirement.

**All three controls fired, and the rc was read directly, never through a pipe:**

| control | what it does | result |
|---|---|---|
| `--fail-undamped` | the damping off everywhere — the owner's own fault state, and also the exact-off state | **rc 1**, G1 6.586 and G2 2.339 both breach |
| `--fail-window` | the window collapsed to 1 key: an identity filter with the gain still reading 700 | **rc 1**, same two breach |
| `--fail-dead` | window 151 keys at full gain — over-damped until the rear stops living | **rc 1**, G3 breaches at 10.587 mm and 0.846 deg |

`--fail-dead` is reachable with **legal stimulus**, so no committed mutant was
needed. That is why `ZHAO_U02_BACKBALL_DAMP_WIN`'s range runs to 201 rather than
to a tidy 31: at 31 the filter saturates at 11.8 mm/sample and **no legal input
could reach the floor**, which would have left "it can fire" as an argument
forever. The floors are also sized so the owner can take the gain to 1000 at the
shipped window (12.004 / 1.403) without tripping them — a floor that forbids the
next rung of its own ladder has stopped describing the art.

### A defect in my own control, found before it shipped

The first draft used `kBackBallDampPm = 0` as the bank-wide knob's compiled
default, on the same "the bank-wide value wins once it differs from its default"
rule every other knob on this creature uses. **That rule cannot express OFF when
the default IS off.** `--fail-undamped` sets the bank-wide knob to 0; 0 compared
equal to the default, the override declined, the per-clip table's 700 stayed in
place, **and the control would have reported the damping switched off while
rendering a fully damped creature.** A control that cannot fire, quoted as
evidence — CLAUDE.md's detector law, in the exact packet written to obey it.

Repaired with an explicit sentinel: `kBackBallDampPmUnset = -1` means "the table
decides", and 0 now genuinely reaches off. Found by asking what `0` would do,
before running anything.

### The matrix

`P25-BB-RECEIPTS/runmatrix_p25bb.sh` → `gate-matrix-backball.txt`, **one
invocation**, from a **frozen copy** of the script. The pass-25 matrix carried
forward in full, plus mback's normal leg and three controls, 19 new selector
legs (16 on the probe, 3 on the **reel** — a knob parsed by one `main()` is this
creature's signature defect and it has shipped twice), `DAMPOFF` on every leg
that compares against a pass-23/24 receipt, and the two new identity legs.

> **`e-ship` now compares against `P25-BB-RECEIPTS/crcs-backball.txt`, and
> `e-bb-off` against `P25-RECEIPTS/crcs-ship.txt`.** Three different files in
> that script are called `crcs-*ship*.txt` and `idfile` prints only a basename;
> the script says which is which at each line.

---

## 5. FIVE THINGS THIS PACKET LEARNED THE HARD WAY

1. **A summed path measures the clip's LENGTH.** The census's first draft made
   Hover look like the worst rear in the bank by 2× purely because it is the
   longest clip. Every rate here is per sample.
2. **`grep -h ... *.out` returns GLOB order, not the order you generated them
   in.** A five-rung ladder's CRCs were read in loop order off an alphabetical
   listing and appeared to say the shipped value rendered like a different rung.
   The renders and every plate were correctly labelled; only the reading was
   wrong, and it was wrong in a way that looked like a real defect for ten
   minutes. Nothing depended on it, and it is recorded because the next person
   will do it too.
3. **A temporal sweep image of the rear was saturated by the LIGHTNING.** The
   per-pixel max−min over a window of frames read 87.5 / 87.7 / 88.1 / 88.3
   across the whole ladder — a flat, confident, useless number, because the mana
   bolts move independently of anything the damping touches. Discarded, and the
   judgement was made from consecutive-frame plates instead.
4. **A gate that only watched the rms would have blessed window 5**, whose worst
   frame is 20 % more violent than the undamped creature's.
5. **A knob's "off" must be reachable.** See §4.

---

## 6. VERDICT ON THE OWNER'S COMPLAINT

**Answered, with two qualifications stated rather than buried.**

* The complaint is located for the first time: the rear motion an eye sees is
  carrier C and the C→End rod, not the End swell; Hover is the only live clip
  whose back turns more than its front; and the reason four levers failed is that
  the chain's stations cancel, so no gain can work.
* The repair is a real mechanism aimed at the real quantity, chosen by eye and
  checked by measurement: the rear holds its shape across consecutive frames
  while the loop still breathes, and Hover's rear now sits among the bank's calm
  clips instead of at the top of one of its columns.
* **Qualification 1: it changes Inspect too**, because Inspect and Hover are one
  bake. Authorised by Direction 26 and declared here; if the owner wants Inspect
  left alone it needs a third bake of the idle, which is a new clip slot.
* **Qualification 2: the front ball's own SPIN is damped 35 %** along with the
  station, because one quaternion carries both. Its position, size and ink are
  untouched. I judged this invisible on a textured sphere at this framing; it is
  the one thing in this packet I would most want the owner's eye on.

**Hover's authored performance was NOT changed.** The swallow beat, the knead,
the fold, the nodule schedule, the eye acting and every authored key are exactly
as they were; the filter acts on the baked result of three stations and the
closure re-solves around it. Nothing was softened, removed or re-timed.

---

## 6b. OPEN ISSUES

1. **Crackle carries the same fault and is left undamped.** Same choreography,
   same 1.08 back/front ratio, separate bake (slot 23). Kept as a byte-identical
   control on the pass that closes the creature, and because the owner named
   Hover. One table entry to change: `kBackBallDampClipPm[23] = 700`. The six
   `mana-*` tiles play that bake too.
2. **The front ball's own SPIN is damped 35 %** on hover and inspect, because
   one quaternion at station A carries both the ball's spin and everything
   downstream of it. Position, size and ink unchanged. Separating them needs a
   second rotation channel at A — a new mechanism, not this packet.
3. **Hover and Inspect cannot be separated** without a third bake of the idle
   (a new clip slot, and an entry in every per-slot table). Declared, authorised
   by Direction 26, not attempted on a final pass.
4. **mback judges bake slot 0 only.** Several other live clips read well above
   Hover's ceilings and are correct — the gate prints them as `info`. A
   bank-wide bound here would be false, and the info rows are the evidence for
   that rather than an omission.

---

## 7. EVIDENCE

| file | what it decides |
|---|---|
| `tools/reel/manafold_backball.cpp` | **the committed probe** — the decomposition, the census, the gate and the three controls |
| `P25-BB-RECEIPTS/decomposition-undamped.txt` | the decomposition table and the worst-frame shares |
| `P25-BB-RECEIPTS/decomposition-shipped.txt` | the same reading after the damping |
| `P25-BB-RECEIPTS/census.txt` | every bake slot, per sample, before and after — the owner's premise checked |
| `P25-BB-RECEIPTS/ladder.txt` | the five gain rungs at the shipped window |
| `P25-BB-RECEIPTS/gate-and-controls.txt` | the gate and its three fired controls, rc read directly |
| `P25-BB-RECEIPTS/gate-matrix-backball.txt` + `gatematrix_p25bb.sh` + `runmatrix_p25bb.sh` | the matrix, one invocation, frozen copy |
| `P25-BB-RECEIPTS/crcs-backball.txt` | the 22-subject bank this packet ships |
| `P25-BB-RECEIPTS/byte-identity-frames.txt` | the frame-by-frame byte comparison |
| `P25-BB-RECEIPTS/binaries-md5.txt` | every binary |
| `P25-BB-RECEIPTS/comment-only-edit.txt` | a doc comment corrected after the binaries were built, with the diff that proves it is comment-only |
| `P25-BB-LOOKS/*` | the ladder, the A/B pairs and the every-frame sheet this verdict rests on |

**Renderer:** `.tmp/p25bb/bin/zhao-reel-cel.exe`, MD5
`9f567357100fc37ec73d53d665f6bf10`, SHA-256
`5aa557df2aa2431cc64acab2ad58d6b724a978645de9c4814e1ba3abdad5d7f8`.
**Probe:** `manafold-backball.exe`, MD5 `f2cc355b23945ed6fb45fee9feb74a92`,
SHA-256 `782a6091d4541dfa3e3139c09bdd80363187d23cb65a68a93eb68e2d9246c8ab`.
Built directly with `tools/reel/build-direct.sh` and the `zhao-env.ps1`
toolchain (g++ 16.1.0 MinGW-W64 ucrt); RC read directly, never through a pipe.
No CMake was used and no CMake result is claimed.

**Presentation:** `ZIXX_EXP=celmain`, `ZIXX_LIGHT=diagonal-cool-cross`, set
explicitly on every render and every gate in this report.
