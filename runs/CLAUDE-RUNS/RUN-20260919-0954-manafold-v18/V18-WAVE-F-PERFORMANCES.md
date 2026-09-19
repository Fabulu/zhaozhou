# Manafold version 18 — Wave F: Flight and Trick performances

**Date:** 2026-09-19
**Worker:** Claude (sole Opus worker). The coordinator's `V18-WAVE-EF-PREP.md` was re-verified against source before use; two of its premises were wrong (§5).
**Scope:** Wave F only: Flight, Trick, and Drift framing (the prep's "fix only if cheap"). No other clip's motion changed.
**Verdict:** **CLOSED.** Flight now reads unmistakably as flight. Trick does the owner's 180 → pause → 360 → slight overshoot → correct → existing righting, pivoting about its planted antenna. Drift's framing no longer cuts the antenna. Gate matrix **115/115**, final bank 33/36 byte-identical to Wave E (the three that differ are exactly Flight/Trick/Drift), and every value was chosen by looking (`V18-WAVE-F-RECEIPTS/look-notes.md`: 16 looks, written down after each one).

## 1. Flight (Owner Direction 19 §7: "move up and down so you can actually see it's flight")

**Diagnosis, confirmed by picture (L1).** The version-17 bob is four symmetric 300 mm bounces under a tight camera. In frame it moves about 10 px at half scale: the owner's complaint is literal. The committed 3D probe gives the second limit: the v17 trough clears the dirt by only **210 mm**, so any bob deeper than about 470 mm breaks the 40 mm float floor. A deeper bob therefore needs the whole flight carried higher.

**Mechanism.** One clock still drives everything. There is an explicit integer cycle count (`kFlightBobCycles`, replacing `K / kFlightBobPeriodKeys`), and one periodic phase warp, `w = t + c·sin t + h·cos t`. Height, pitch, bank, breath and the nodule trail all read the warped phase; the antenna sway stays on the plain clock. A single harmonic of one cycle is periodic, so the seam is exact for every rung. It is monotone while |(c,h)| < 1 rad, and the reel refuses a warp above `kFlightWarpMaxA16`. `compress_at` was split into `compress_from_sin`, so Flight's breath shares the bank's arithmetic.

| Knob (named constant) | v17 | Shipped | Chosen by |
|---|---|---|---|
| `kFlightBobCycles` | 4 | **2** | L2: c4 still reads as a bounce, c2 as a slow climb-and-sink |
| `kFlightBobAmpMm` | 300 | **800** | L1/L2: 700–900 is the first range that reads as travel |
| `kFlightCruiseLiftMm` (new) | 0 | **500** | probe trough limit; lift = amplitude − 300 keeps v17's trough |
| `kFlightRiseFrac16` (new) | 0x8000 | **24000** (0.366) | L3/L4: quick climb, long glide |
| `kFlightTopHang16` (new) | 0 | **4000** | L3/L4: floats at the apex |
| `kFlightPitchLead16` (new) | 0 | **4096** | L4: nose leads the climb by about 5 keys |
| `kU02CamKFlight` / `kU02CamBiasFlight` (new) | 250000 / 0 | **220000 / +5000** | L2/L3: the apex antenna was cut at 900 mm |

Every neutral value reproduces the version-17 bytes exactly: Flight with the v17 environment is `0x5B272AB3`, the Wave-E bank value.

**Looked at.** L1 amplitude ladder; L2 amplitude+lift × cycles; L3 shape rungs; L4 the builder's own height/pitch trace per rung (comparison side only, after a screen-colour tracker proved unusable, §5). L5 every frame of the shipped clip: 352 populated frames, no crop, no pop, continuous wrap. L6 native apex/trough against v17: roughly 70 px of travel against v17's roughly 10. The apexes lift the whole body above the horizon and the troughs carry it low over the dirt. The purple body patches also appear in v17's frames; they are pre-existing shading, not Wave F.

## 2. Trick (Owner Direction 19 §9)

**Kept:** the v17 pure-X half-turn plant, the +90° face yaw, the fixed camera, the plant/lift keys, the righting, and `kTrickKeys = 200`.

**Decision: kTrickKeys does not grow.** Every `sinp(f,K,n)`, `antenna_knead` and `front_flex_play` in the clip is periodic in K. Growing K re-phases the whole clip, and pinning those oscillators to their old absolute period would break the loop seam (n·K′/200 is not an integer for the clip's cycle counts). The turn is therefore authored inside the existing 70-key plant hold, `[78,148)`, which is re-partitioned:

| Beat | Keys | Duration |
|---|---|---|
| touchdown → pause | 78–100 | 0.73 s |
| one full turn to 1000 + overshoot | 100–128 | 0.93 s |
| C2 correction back to exactly 1000 | 128–140 | 0.40 s |
| hold ("ta-da") | 140–148 | 0.27 s |
| existing righting | 148→ | unchanged |

**Mechanism.**
- `trick_spin_progress_u` authors unwrapped progress in micro-turns as two quintic segments `S(x)=10x³−15x⁴+6x⁵`, evaluated exactly in int64. It is converted to angle16 only at the quaternion. A whole number of turns is skipped outright, so the pause and the post-correction hold are byte-identical to the no-spin clip.
- The yaw is **world-vertical and pivots about the SUPPORT**. `trick_support_center_xyz` (the refactored walk; scale 1 reproduces the old millimetre Y walk bit for bit) is evaluated before and after the pre-multiply in micrometres, and the root X/Z absorbs exactly the horizontal displacement the yaw gives the support. Root X/Z are zero outside the turn.
- The walk runs after the planted branch's legacy JunctionF X balance flex (coordinator note), so both pivots include it. The Front table's ramp at keys 156–157 lies outside the spin window.
- The literal `158` is now `kTrickBalFadeEndKey = kTrickLiftKey + kTrickBalFadeKeys`.
- Selected values: start/turn/settle `100/128/140`, overshoot `kTrickSpinOvershootPm = 40` (14.4°), gain 1000.

**Framing.**
- The prep's premise was wrong: every Manafold clip inherits `cam_bias 0` from `u02_common`, not 14000.
- The bias ladder (L7) plus a numeric top-edge check validated on known negatives showed that at the house k no single aim holds both extremes. A bias of −12000 lifts the plant clear but puts antenna ink in the top 4 rows on 44 frames (the flip climb and the righting throw).
- A named `kU02CamKTrick` was added. **Shipped: k 330000, bias −10000** (L12). This gives 0 top-touch frames, and the planted crown sits on a band of dirt instead of the bottom edge (the Wave-D open item).

**Looked at.**
- L7 aim ladder.
- L8 timing ladder: every rung reads as one full turn; timing was decided on beats.
- L9 overshoot 20/40/70: 40 carries the eyes visibly past centre and settles back symmetric.
- L10/L11 every frame of the shipped clip in two halves: continuous, no pop at the spin joins or the lift, f399 matches f0.
- L12 zoom/aim.
- L13 the planted contact at 4× against the no-spin control: the crown stays on the dirt as the legs swap sides, with no float and no sink.

## 3. Drift framing (fixed camera, cheap and clean)

L14 showed the journey framed lopsided: it starts about ¾ of the way across and leaves through the left edge over f260–298. The renderer comment blamed the wrap; in fact it is plain framing. `kU02DriftCamBiasX = 14000` is a **constant** horizontal aim (not a tracker). A two-edge column check, validated on a known-cut frame, gives leftmost ink at column 1 (f297) and rightmost at 9 px from the edge (f0). The antenna and mana are never cut, where v17 cut them for about 40 frames. **Residual (owner call):** the traverse is about as wide as the frame, so no constant aim buys margin at both ends. That would need a shorter traverse or a pulled-back camera.

## 4. Gates — 115/115 (`V18-WAVE-F-RECEIPTS/gate-matrix.txt`)

- **Normals 11/11 RC 0**, including mmeshcheck `CLEAN 1416/4200`.
- **mspan 35/35**: normal output byte-identical to HEAD's.
- **msmooth 16/16**: all 16 attribution masks identical to Wave E.
- **Protected legs 27/27**: both Trick support controls now also fail the new PLANTED 360 line and nothing else.
- **New mqa Q6/Q7 and four new controls.** Each fires only its own category:
  - **Q6a revolution** (`--fail-trick-spin-gain`: two revolutions). Shipped: pause 78..100; peak 1039.98 pm at key 128; exactly 1000.00 pm by key 140; 1 reversal; worst step 69.4 pm/key (bound 90); off-axis 0.00004.
  - **Q6b C2 at joins** (`--fail-trick-spin-ease`: C1 smoothstep, ratios 0.52/0.64). Shipped: progress accel ratios 0.057/0.080/0.124, root-XZ 0.009/0.017/0.109 (bound 0.25).
  - **Q6c support drift** (`--fail-trick-spin-pivot`: turn about the root, 568 mm). Shipped: 15.75 mm, bound 24 mm, declared with reasons in source.
  - **Q7 Flight** (`--fail-flight-seam`). Shipped: 2 maxima, 800.0 mm; seam step/accel/jerk 77.5/4.8/1.07 within interior 86.4/9.4/1.45.
- **mprobe.** PLANTED 360 keys 100..140: carrier B owns 82/82 key+midpoint samples, depth −27..−18 mm. The whole window stays 140/140. Flight clearance is 211 mm.
- **Two gate adjustments, declared in source:**
  - mqa Q3 gains a Trick root-step ceiling of 240 mm. The body orbits its support at 178 mm/key at peak turn speed, which is C2 (Q6b), not a teleport.
  - mprobe's TRAVEL float gate now leaves Trick's declared contact keys to the contact contract. The Trick root now moves in XZ, and terrain rise is still taken on every key.
- **Selectors:** 14/14 malformed or out-of-range Wave-F values return RC 2.
- **Wave-E rows:** live-history gate normal 22/22 OK; `legacy` control fires on every live subject; list-drift control fires.
- **Byte identities (final binary):** Flight with v17-neutral knobs `0x5B272AB3`, Trick `SPIN=none` + v17 camera `0x22563A37`, Drift `CAM_BX=0` `0x3A11AB0C` -- each equals the Wave-E bank. Zixxtrixx Idle final == HEAD `0x1408F885`. The one-invocation bank of 22 live + Crackle-legacy + 13 non-live subjects is 33/36 identical to the Wave-E receipts; the 3 that differ are the intended Flight/Trick/Drift (`crc-final-live22-crackle-legacy-nonlive13.txt`).

## 5. Findings recorded on the way (instrument honesty)

1. **The prep's camera premise was wrong.** The house u02 bias is 0, not 14000. The neutral byte check against the Wave-E bank caught it before any art was chosen.
2. **Screen-space trackers lied twice.**
   - Flight's magenta mask loses the body at the trough, dropping from about 2000 px to 15 px as pitch turns the lit face away.
   - Drift's lighting defeats it entirely (a known-positive failure).
   - A bottom-gap metric read the dirt as ink.
   - None of them was used for a decision. Every numeric framing check was first run on a known-negative and a known-positive frame.
3. **Contact estimate.** A hard 25 mm contact patch holds 6–12 vertices on a ring mesh about 50 mm coarse, so it jitters. Q6c uses a height-weighted patch instead.
4. **Pre-existing, out of scope, now reported every run by mqa:** in the untouched no-spin plant, the balance wobble is height-pivoted only. The weighted antenna contact wanders **178 mm** from touchdown during the pause (v17/Wave-D behaviour). The spin adds at most 15.75 mm to it. Pinning the plant's XZ would make the pause truly planted, but it changes the pause the brief said to keep. **Owner call.**
5. Coordinator note verified: the legacy JunctionF X balance flex is included in the pivot math.

## 6. Binaries and bytes

Direct clean build (`build-direct.sh --output .tmp/v18-waveF-final --clean`), g++ 16.1.0; every binary postdates the last source edit. No CMake result is claimed.

| Binary | MD5 |
|---|---|
| `zhao-reel-cel.exe` (production) | `e17c5306313e3bdca9763646d06986b5` (SHA-256 `6aa39a2a…ed27`) |
| gates | `V18-WAVE-F-RECEIPTS/binaries.txt` |

Shipped sequence CRCs (production invocation): Flight `0x9FBC05CB` (352), Trick `0xBF695C69` (400), Drift `0x69158A83` (300). Flight and Trick equal the renders reviewed in L5/L6 and L10–L13.

## 7. Open

1. The Trick pause contact wander, 178 mm (§5.4), is pre-existing and needs an owner call.
2. Drift has no framing margin at either end (§3).
3. From Wave D: the End-swell stub, unchanged.
4. Flight's pitched underside reads darker at the troughs, as it does in v17. It is pre-existing shading.

## 8. Files

- **Source:** `tools/reel/manafold_art.h`, `manafold_clips.h`, `zhao_reel.cpp`, `manafold_qa_p12.cpp`, `manafold_probe.cpp`.
- **Evidence:**
  - this report;
  - `V18-WAVE-F-RECEIPTS/` (gate matrix, CRC lists, binaries, mqa/mprobe logs, look notes);
  - the ladder/witness sheets `V18-WAVE-F-*.png|jpg`;
  - the every-frame sheets `V18-WAVE-F-MANAFOLD_FLIGHT-ALLFRAMES.png`, `..._TRICK-ALLFRAMES-A/B.png` and `..._DRIFT-ALLFRAMES.png`, made from the final bank.
- **Not committed:** `.tmp/`, raw `.rgb` frames and the viewing JPEGs.
