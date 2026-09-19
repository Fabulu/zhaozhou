# Q006 answer — wave-e-mana-map

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 071dc3e23ee64f2d89e92ea8b5d6eb06
- when: 2026-09-19T17:36:03  seconds: 64  finish: stop  status: ok
- usage: {"completion_tokens": 5547, "completion_tokens_details": {"reasoning_tokens": 2733}, "prompt_tokens": 7123, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 12670}
- inputs: [{"input": "tools/reel/zhao_reel.cpp:5530-5640", "sha256": "a7dfc6864ab4b327", "chars": 6455}, {"input": "tools/reel/zhao_reel.cpp:5800-5885", "sha256": "31bc06a00635160a", "chars": 5731}, {"input": "tools/reel/zhao_reel.cpp:8715-8730", "sha256": "2756dac4f6f1e207", "chars": 1064}, {"input": "tools/reel/zhao_reel.cpp:8875-8930", "sha256": "2f7b2ad801bad417", "chars": 3389}]
- kind: task

## Answer

# Q006: Wave E – Manafold v18 (bounded chunk)

## 1. Per-subject configuration as shown in the input

### manafold-crackle (zhao_reel.cpp:8910–8919)

| Field | Value | Source |
|---|---|---|
| Builder call | `subject_u02_clip(u02::kIdleFixedSlot, "manafold-crackle", u02::kIdleKeys, false, &kU02SunChannel)` | :8911–8912 |
| `u02_mana` | **4** (overridden after builder) | :8913 |
| `u02_smear` | **0** (explicit) | :8914 |
| `u02_mist` | inherited from builder → **true** (kIdleFixedSlot ≠ 7) | :5879 |
| Backdrop | `u02_backdrop(s, 58, 96, 132)` → **night** (violet, mag 25 px) | :8917 |
| Note | "the ADDLIGHTNING variant…" | :8918 |

### manafold-blown (zhao_reel.cpp:8721–8730)

| Field | Value | Source |
|---|---|---|
| Builder call | `subject_u02_clip(u02::kBlownSlot, "manafold-blown", u02::kBlownKeys, false, &kU02SunStartle)` | :8722–8723 |
| `u02_mana` | inherited → **9** (kBlownSlot ≠ 7) | :5867 |
| `u02_smear` | inherited → **0** | :5870 |
| `u02_mist` | inherited → **true** (kBlownSlot ≠ 7) | :5879 |
| Camera | `cam_k=168000, cam_ps=9000, cam_pc=64900` | :5843–5847 |
| Backdrop | **not shown** in the provided lines (no `u02_backdrop` call visible for blown in :8721–8730) | — |

### Generic live-subject builder `subject_u02_clip` (zhao_reel.cpp:5852–5879)

| Field | Value | Source |
|---|---|---|
| `u02_glow` | `kBellyGlowGainPm > 0` → **false** (gain held 0) | :5855 |
| `u02_mana` | `slot == 7 ? 0 : 9` → **9** for all live slots | :5867 |
| `u02_smear` | **0** (Direction 12) | :5870 |
| `u02_mist` | `slot != 7` → **true** for every non-diagnostic slot | :5879 |
| Camera | per-slot overrides (hasty bias :5813–5822, fall :5831–5835, blown :5843–5847, deaths :5848–5851) | :5813–5851 |
| Backdrop | **not set by the builder**; each showcase clip calls `u02_backdrop()` itself | :5624 comment |

---

## 2. What makes Crackle "night" and the minimal re-point

**What produces the night.** The single call at :8917, `u02_backdrop(s, 58, 96, 132)`, enters the branch at :5627–5638. Because `kU02BackdropBloom` is `false` (:5543) and `kU02NightBackdrop` is `true` (:5587), the function sets `s.planet = 1` (violet sky) and `s.planet_sun_mag = 25` (:5637). That is the entire "night": one flag pair plus a small sun splat. The "ordinary day presentation" is `s.planet = 0` (salmon sky), which is what you get when `u02_backdrop` is never called or `ZHAO_U02_NOPLANET=1` is set.

**Minimal change to put Crackle on candidate 9 + day:**

```cpp
if (wanted("manafold-crackle")) {
  SceneSubject s = subject_u02_clip(u02::kIdleFixedSlot, "manafold-crackle",
                                   u02::kIdleKeys, false, &kU02SunChannel);
  // s.u02_mana stays 9 from the builder; no override needed.
  // s.u02_smear is already 0 from the builder.
  // Do NOT call u02_backdrop() → planet stays 0 → salmon day sky.
  s.note = "ADDLIGHTNING on the ordinary day backdrop, candidate 9";
  rc |= render_scene(s);
}
```

Concretely: **delete line :8913** (`s.u02_mana = 4`) and **delete line :8917** (`u02_backdrop(s, 58, 96, 132)`). Everything else (keys, sun, slot) is unchanged.

**Legacy control that reproduces today's bytes exactly:**

```cpp
if (wanted("manafold-crackle-legacy")) {
  SceneSubject s = subject_u02_clip(u02::kIdleFixedSlot, "manafold-crackle-legacy",
                                   u02::kIdleKeys, false, &kU02SunChannel);
  s.u02_mana = 4;              // :8913 as-is
  s.u02_smear = 0;             // :8914 as-is
  u02_backdrop(s, 58, 96, 132); // :8917 as-is
  s.note = "legacy crackle: candidate 4 + night, byte-identical to pre-Wave-E";
  rc |= render_scene(s);
}
```

This is a same-binary, same-slot, same-sun subject whose only difference from the new crackle is the two lines we removed. No env-var trickery needed.

---

## 3. Turning off `u02_mist` for every live subject

**The single line to change:** zhao_reel.cpp:5879

```cpp
// BEFORE
s.u02_mist = slot != 7;
// AFTER
s.u02_mist = false;
```

**Why this is sufficient and safe:**

- Every live showcase subject (channel, crackle, blown, hasty, fall, deaths, rest, etc.) gets `u02_mist` only from this builder line. Setting it to `false` removes the persistent 48×30 history plane from all of them.
- **Diagnostic subjects that must keep mist override the builder after it returns:**
  - Mist variant sheet, :8905: `s.u02_mist = true;` (explicit, after `subject_u02_clip(5, …)` at :8904).
  - Fogprobe, :8882: `s.u02_mist = true;` (explicit, after its own builder call).
  - Slot 7 (form diagnostic) already had `mist = false` via the old `slot != 7` guard; it stays false.
- **How to tell live from diagnostic in this code:** diagnostic subjects are (a) created inside dedicated `if (wanted("manafold-mist-…"))` / `if (wanted("fogprobe-…"))` blocks that explicitly re-set `s.u02_mist = true` after the generic builder, or (b) slot 7, which is the named form diagnostic. Live subjects are the main showcase clips (channel, crackle, blown, hasty, fall, deaths, rest) that rely on the builder default and never re-set `u02_mist`. The comment at :5871–5878 confirms the design intent: mist rides "every clip that has mana," slot 7 opts out.

No other line needs changing. The contour shell (a different effect) is not touched.

---

## 4. Drift / Blown special mana or smear settings

- **Blown:** In the provided lines (:8721–8730, :5843–5847) the only specialisation is the camera (`cam_k`, `cam_ps`, `cam_pc`). No explicit `u02_mana`, `u02_smear`, or `u02_mist` override is shown. It inherits `mana=9, smear=0, mist=true` from the generic builder. **No special mana/smear settings beyond the mist are shown.** The owner's remark "Blown has similar issues" presumably refers to the mist (now addressed by Q3) and possibly the backdrop, but no mana or smear override for blown appears in the input.

- **Drift:** The comment block at :5804–5812 discusses drift's wrap behaviour and the camera-bias decision, but **the `manafold-drift` subject creation, its slot, keys, and any field overrides are not shown in the provided input.** → **not shown.**

---

## CONTINUATION
- **Done:**
  - Crackle configuration fully mapped: mana 4, smear 0, mist true (inherited), night backdrop via `u02_backdrop` (:8911–8918).
  - Blown configuration: mana 9, smear 0, mist true (inherited), camera overrides at :5843–5847; no explicit mana/smear/mist override shown.
  - Generic builder `subject_u02_clip` sets mana 9, smear 0, mist `slot!=7` (:5867, :5870, :5879); backdrop is per-subject opt-in.
  - Night = `s.planet=1` + `planet_sun_mag=25`, produced by `u02_backdrop` when `kU02NightBackdrop=true` and `kU02BackdropBloom=false` (:5587, :5543, :5627–5637).
  - Single-line mist kill at :5879 (`slot != 7` → `false`) is safe: all diagnostic subjects (mist variants :8905, fogprobe :8882) explicitly re-set `u02_mist=true` after the builder.
  - Crackle re-point: delete :8913 and :8917; legacy control is a verbatim copy under a new `wanted` name.

- **Open:**
  - Whether `manafold-blown` calls `u02_backdrop` (not visible in :8721–8730; the line range is cut at :8730). If it does, blown is also on night and needs the same treatment.
  - Drift's full subject definition (slot, keys, any mana/smear/mist/backdrop overrides) — not shown.
  - Whether `kU02SunChannel` vs `kU02SunStartle` vs `kU02SunCalm` have any visual difference that interacts with the day/night backdrop.
  - The `u02_mist` global `g_u02_mist` cfg (reset at :8908) — does disabling `s.u02_mist` in the builder also need a corresponding cfg change, or is the per-subject bool the sole gate?

- **Next chunk:** Verify whether `manafold-blown` (and any other live subjects between :8730 and :8875) call `u02_backdrop`, and locate the `manafold-drift` subject block. Inputs needed: zhao_reel.cpp lines ~8730–8720 (rest of blown) and the drift creation block (search for `"manafold-drift"` or `kDriftSlot`).

## Coordinator verdict

**partial** — spot 5: crackle mana4/night via u02_backdrop OK; night mechanism OK; blown inherits 9/mist OK; legacy-control sketch OK; OVERCLAIM - 'every diagnostic re-sets mist' is false: inspect/still/antenna-fixed/nodule-solo/antenna-quarter/trio/fogprobe-mana/off also use the builder default, so the one-line kill silently changes them too. Worker must enumerate the 35 builder subjects and decide per subject. Also missed: channel shares u02_backdrop night.
