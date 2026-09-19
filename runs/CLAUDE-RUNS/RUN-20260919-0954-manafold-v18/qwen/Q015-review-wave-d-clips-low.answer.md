# Q015 answer — review-wave-d-clips-low

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: 7fff622989034d1d80310b904ef45b7f
- when: 2026-09-19T18:46:25  seconds: 564  finish: stop  status: ok
- usage: {"completion_tokens": 47076, "completion_tokens_details": {"reasoning_tokens": 45754}, "prompt_tokens": 12011, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 59087}
- inputs: [{"input": "git show 50803207 -- tools/reel/manafold_clips.h", "sha256": "3abe7450d1992b5d", "chars": 29338}]
- kind: task

## Answer

## FINDINGS

| # | P1/P2/P3 | file:line | claim (one line) | evidence (one line) |
|---|---|---|---|---|
| 1 | P1 | tools/reel/manafold_clips.h:3276 | Trick applies a non-HingePlay JunctionF X multiply during planted frames, bypassing the Front fields and not enforcing identity. | `g.q[kBJunctionF] = quat_mul(g.q[kBJunctionF], quat_x(...));` inside `if (planted)`, where `planted = bal > 0` at :3249. |
| 2 | P1 | tools/reel/manafold_clips.h:1613,3210,3248 | kTrick Front table is non-identity for late planted frames in the shown 0..199 trick timeline. | :1613 ends identity at 780 pm; :3248 keeps planted through `f < 158`; :3210 shows key timeline to 199. |
| 3 | P2 | tools/reel/manafold_clips.h:1554,1559,1582 | A second Front data/query path remains via `FrontFlexPose` / `front_flex_curve` / `front_flex_authored`. | `struct FrontFlexPose` at :1554; `front_flex_curve` returns it at :1559; `front_flex_authored` returns it at :1582. |

## ANSWERS

1. **Leftover second Front path?** Yes. The shown diff still introduces/uses a separate Front query path: `FrontFlexPose` at `tools/reel/manafold_clips.h:1554`, `front_flex_curve` at `:1559`, and `front_flex_authored` at `:1582`. It also has an extra multiply onto `kJunctionF` in trick at `:3276`. `front_flex_at` is not shown. The intended HingePlay path is used through `front_flex_play` at `:1641` and passed into builders at `:2478`, `:2552`, `:2803`, `:2884`, `:3065`, `:3275`/`:3281`, `:4486`, `:4734`, and `:4885`.

2. **Front table first/last mismatch or non-C2/linear interpolation?** No shown first/last mismatch: all shown tables begin `{0, 0, 0}` and end `{1000, 0, 0}` in `:1583`-`:1618`. Non-C2 cannot be confirmed from the shown material: `front_flex_curve` calls `motion_c2_ease` at `:1569`, but `motion_c2_ease` is not shown. No linear interpolation is shown inside `front_flex_curve`.

3. **Can gain/mute touch anything other than Front X/Y? Is Trick Front identity on every planted key?** In the shown diff, no: `g_u02_front_flex_mute` and `g_u02_front_flex_gain_pm` are only used in `front_flex_play` at `:1643`, `:1645`, and `:1647`, affecting only `hp.tilt_front` / `hp.yaw_front`. Trick Front identity: no. The planted branch directly multiplies `g.q[kBJunctionF]` at `:3276`, and the kTrick table’s identity segment ends at 780 pm at `:1613` while the planted branch is defined through `f < 158` at `:3248`.

## CONTINUATION

- Done: Commit adds HingePlay front fields (`manafold_clips.h:359`). Shown public Front entry points pass HingePlay: hover `:2478`, drift `:2552`, rest `:2803`, hasty `:2884`, taunt `:3065`, trick `:3275`/`:3281`, blown `:4486`, taunt3 `:4734`, flight `:4885`. `front_flex_play` applies mute/gain only to `tilt_front`/`yaw_front` (`:1643`-`:1647`). All shown Front tables start/end identity (`:1583`-`:1618`). Interpolation calls `motion_c2_ease` (`:1569`), implementation not shown. Direct non-HingePlay JunctionF X multiply in trick planted branch `:3276`; kTrick identity segment ends 780 pm (`:1613`) while planted bound is `f < 158` (`:3248`) on the shown 0..199 timeline (`:3210`).
- Open: `loop_pose` body and its JunctionF application order are not shown; `motion_c2_ease` is not shown; other uses of `g_u02_front_flex_gain_pm` / `g_u02_front_flex_mute` are not shown; `kTrickPlantKey` / `kTrickLiftKey` values are not shown; whether drift-trail subtract at `:2551` should be muted/scaled is unspecified.
- Next chunk: Read current `loop_pose` and `motion_c2_ease` in `manafold_clips.h` to verify the required fold Z → Front X → Front Y order and quintic C2, plus confirm `kTrickPlantKey` / `kTrickLiftKey`. Inputs: full current file or those function/constant definitions.

## Coordinator verdict

**partial** — xhigh, 45.8k reasoning, 564s. spot 3/3: #1 accurate observation but pre-existing (Wave C :3274 legacy balance flex 1100*bal on JunctionF X, not a Front API) -> P2 note, not P1; #2 REFUTED as P1: kTrick identity 360..780pm = keys 72..155 covers contact window 78..148; only balance-fade keys 156-157 ramp (P3); #3 REFUTED: FrontFlexPose is the data type feeding the single HingePlay path. Sharper reading than medium runs, but severity still inflated.
