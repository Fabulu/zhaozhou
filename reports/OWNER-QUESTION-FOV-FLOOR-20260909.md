# One question, worth −18 DSP: is a ~53° vertical FOV floor acceptable?

2026-09-09. This is the highest-value owner decision on the board and **it has
never been asked.** `reports/PROJECT-CORE-OPERAND-WIDTH-20260909.md` identified
it; `grep` finds "53 degree" nowhere in `docs/`, nowhere in `spec/`, and in no
other report — so it has existed as an unasked question, behind a lever, for a
day. Everything needed to answer it is below.

## The question

> **May the view-projection matrix's vertical perspective coefficient be capped
> at 2.0, which sets a vertical field-of-view floor a shade above 53.13°?**
>
> Answering YES unlocks a narrowing of the projector's matrix operand to 18 bits.
> Answering NO closes that lever permanently and the DSP has to come from
> somewhere else.

## Why it is worth asking

Cyclone V variable-precision DSP does one 27×27 signed **or two 18×18**. So
narrowing the matrix operand to 18 bits is the difference between one product
per DSP and two. `zhao_project_core.sv:162` prices it:

> "**Width narrowing is where 22 of those 33 are.** At ≤ 27 bits the same eleven
> products cost 11. What that needs is a PROOF that 27 bits covers a world
> coordinate, which is a question about map size and the fixed-point format and
> belongs to the owner, not to this file."

**That framing has been superseded by measurement and it makes the question look
much bigger than it is.** Two things changed:

* **27 bits buys nothing.** `GEOM.WCACHE.md:81`, measured 2026-08-24: *"`32x27`
  costs 3 DSPs, exactly what `32x32` costs"*. And a later sweep found **32×22 is
  also 3**. Only **18** pays.
* **18 bits on the MATRIX operand is not a question about world size at all.**
  The vertices stay full-width s32 — ±32,768 world units, untouched. Only the
  nine *matrix coefficients* narrow. So the question stops being "how big is the
  playable world" and becomes "**how long is the longest lens**", which is a far
  smaller question and one you can answer from taste rather than from a proof.

The docket still carries the old framing, and still states a headline of
`66 → 33 → 11` whose third term assumed 27 bits. It is wrong there.

## The arithmetic, checked rather than inherited

An 18-bit signed Q16.16 operand holds ±131,071 / 65,536 = **±1.99998**.

A standard perspective matrix has `m00 = cot(fov_v/2) / aspect` and
`m11 = cot(fov_v/2)`. For any aspect wider than square, **`m11` is the larger of
the two, so `m11` binds and the aspect divide is irrelevant.**

| vertical FOV | `m11 = cot(fov/2)` | fits ≤ 1.99998? |
|---:|---:|---|
| 30° | 3.732 | no |
| 40° | 2.747 | no |
| 45° | 2.414 | no |
| **53.13°** | **2.000** | **no — exactly on the boundary** |
| 60° | 1.732 | **yes** |
| 70° | 1.428 | yes |
| 90° | 1.000 | yes |

`cot(fov/2) = 2.0` at **53.13°**, and since the representable maximum is
1.99998 rather than 2.0, the floor sits a hair **above** 53.13° — call it 53.2°.

### A correction to the source report, in the direction that matters

`PROJECT-CORE-OPERAND-WIDTH-20260909.md` says the constraint is
"`cot(fov/2)/aspect` passes 2.0 at about a 53 degree vertical field of view".
**The number is right; the expression named is not, and it understates the
constraint.** At 4:3, `cot(fov/2)/aspect` passes 2.0 at **41.11°** — which would
imply a more permissive floor than actually exists. The binding coefficient is
the vertical one *without* the aspect divide.

**One consequence is a genuine simplification: the floor is video-mode
independent.** `spec/video_rules.md` has three modes at three different aspects —
384×240, 320×240, and 512×240 as two 256×192 halves — and because aspect only
ever *reduces* `m00`, none of them changes the answer. One ruling covers all
three.

## What a "yes" costs you in practice

53.2° vertical is a **wide** field of view. Most third-person and RTS-style
cameras sit between 60° and 90° vertical, all of which fit comfortably. What a
yes forecloses is **long-lens work**: a 45° or 30° vertical zoom for a dramatic
push-in or a sniper-style view would need a coefficient the narrowed operand
cannot hold.

If any such shot is wanted, three outs exist and none needs this ruling
reversed: keep the matrix at full width for that camera and eat the DSP; express
the zoom by moving the camera rather than narrowing the lens; or scale the
coefficient and compensate in the viewport stage — which is arithmetic somebody
would have to prove exact, and is not on offer here.

## What a "no" costs

−18 DSP off a bill that is 192 against a device of 112, with a target of ≤94 and
a currently honest gap of 43. It is not the only lever, but it is the largest one
gated purely on a decision rather than on engineering.

## Not verified, stated as such

* **That −18 is achievable in one step.** Sharing, row multiplexing and width all
  act on the same eleven products, so the savings **multiply and never add**. The
  measured lattice is 66 → shared 33 → shared+rows **15** → +width **5**; the
  marginal value of width narrowing *after* the first two levers is about **−10**,
  not −18. −18 is its value if taken alone. Do not add these.
* **That the narrowed core still meets timing or fits.** No fit has been run at
  18 bits; only the DSP arithmetic is measured.
* **That the ±1.99998 bound is the only constraint the coefficients face.** The
  translation row and the w row carry different ranges; the existing random test
  sweeps matrix entries only to 19 bits, which the source report flags as a
  coverage gap on exactly the operand that matters.
