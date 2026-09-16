# Manafold pass 16 — independent by-eye review

**Verdict: FAIL / do not publish yet.** Lightning depth, ordinary Lasso, and smear removal pass, and the protected overall likeness survives, but five-carrier motion, broad translucency, the always-surfaced rear socket, and pirouette inner ink fail. The full-bank sweep also exposes protected form/wrap/lightning-identity regressions, and revision `a818e70d` has an out-of-bounds shipping joint path.

## Scope

- Source: `a818e70dfed68010d13de0f047589eb5f879ff26`.
- Integrated binary MD5: `93D0BF2388BA34245F7A1B77153C48A2`.
- Reviewed the 28 final-bank every-frame sheets, the added 576-frame `manafold-nodule-solo.png`, targeted native evidence, and published pass-15 media.
- Native evidence provenance was checked: `hover-f300-2x.png` is pixel-identical to final-bank `manafold-hover/0300.rgb`; `lasso-f120-2x.png` is **not** identical to the final bank and is excluded.
- Calls below are visual calls, not counter/gate claims. Where thumbnails cannot prove a throughout-bank requirement, the call is conservative FAIL/unverified.

## Owner items

### 1. Lightning behind antenna is occluded; front lightning remains — PASS

- Final `manafold-hover` **0300**, inspected at native scale, correctly hides the rear/left white-core/blue segment behind the nearer pink span while retaining the front/top-right lightning. The same-binary through-control shows the rejected overpaint, so this is a real visual difference.
- Strongest sheet crossings also pass without a definite foreground leak: `hover` and `inspect` **0275–0449, 0500–0574**; `channel`, `mana-aqua`, `mana-cyan`, and `mana-stack` **0275–0399**; `crackle` **0000–0599**.
- Front lightning remains present and connected in those checks. The separate loss of the normal connected white/blue identity in three menu variants is recorded under regressions, not misclassified as a depth inversion.

### 2. No smear anywhere — PASS

No creature-history ghosts or grey silhouette smear are visible in `hit` **0000–0139**, `taunt` **0000–0279**, `taunt2` **0000–0239**, `death-drop` **0000–0449**, `death-gutter` **0000–0589**, `lasso` **0000–0334**, `blown` **0000–0291**, `taunt3` **0000–0368**, `drift` **0000–0299**, `curious` **0000–0179**, `startle` **0000–0159**, `rest` **0000–0399**, `pirouette` **0000–0239**, `crackle` **0000–0599**, or the reviewed hover/inspect sheets. The moving energy at `taunt2` **0142–0151** is travelling mana, not a fixed white bar or history smear.

### 3. Rear surface socket visible, stable, non-spasming and not stuck inside — FAIL

- No one-frame spasm or gross sliding pop is visible in the close clips.
- But the owner requires the socket to remain visibly surfaced **throughout**. It becomes unreadable/possibly buried in `rest` **0125–0149** and **0275–0349**, `pirouette` **0075–0095** and **0150–0205**, `trick` **0125–0319**, and effect-heavy `mana-boil` **0050–0190, 0250–0390, 0450–0560**.
- `manafold-nodule-solo` End segment **0384–0479** shows separate capability, but a solo probe cannot substitute for shipping clips; `hasty`, `flight`, and `fall` are also too small in-sheet to prove placement.

### 4. Ordinary Lasso throws mana — PASS

- Ordinary `manafold-taunt2`: setup **0000–0074**, release **0075–0099**, detached flight **0100–0199**, catch **0200–0211**, settle/home **0212–0239**. The mana leaves the antenna; pass 15 did not.
- Existing Mana Lasso remains continuous: setup **0000–0124**, loop **0125–0149**, flight/orbit **0150–0249**, edge wrap **0244–0251**, return/cinch **0250–0274**, catch **0275–0299**, settle **0300–0334**. No catch teleport.

### 5. Fog/translucency broader, stronger, more see-through without bleach — FAIL

- The no-bleach half passes: final hover **0300** remains richly magenta with dark terminator and ink.
- Some broader dark transmission is visible in hover **0300** and channel **0275–0389**, but `channel` **0000–0274, 0390–0418**, `startle` **0050–0098**, and `taunt3` **0225–0367** read chiefly as external particles/sparks around a substantially solid pink body.
- Compared with pass 15, stronger **mist** is readable; stronger/broader **background transmission through the outer shell** is not reliably readable at final presentation scale.

### 6. Body/head inner outline and antenna-top ink coexist — FAIL, localized regression

- **Pass examples:** final hover **0300**, `hit` **0000–0049**, `taunt` **0000–0074**, `curious` **0000–0074**, and `rest` **0000–0099** retain the dark body/head boundary while antenna-top ink remains.
- **Failure:** `pirouette` **0004–0049** and **0098–0149** fuse antenna/head into a cyan-white cap, losing the required internal boundary.

### 7. Front/A/B/C/End independently move in shipping clips — FAIL

- The diagnostic demonstrates capability: Front **0000–0095**, A **0096–0191**, B **0192–0287**, C **0288–0383**, End **0384–0479**, opposed pose **0480–0575**.
- Shipping clips show local bends and outer-chain changes around **0325–0399** and **0500–0549**, but `curious` **0000–0179**, `startle` **0000–0159**, `rest` **0000–0399**, and `pirouette` **0000–0239** still read predominantly as one carried chain. Front/A/B/C/End identities and the requested middle-down/outer-up opposition are not plainly attributable in public performances. `mana-boil` **0050–0190, 0250–0390, 0450–0560** obscures the joints inside one pale mass.
- **Source blocker:** `tools/reel/manafold_clips.h:1957` and `:2085` each allocate `int32_t swal[3]`, then pass it to helpers at `:1336–1381` that write/read five entries (Front/A/B/C/End). C and End therefore use out-of-bounds storage in shipping hover/channel choreography. The current binary's appearance cannot make undefined storage publishable.

## Protected wins and regressions

- Preserved from pass 15: camera-relative star eyes, straight return limb, mist identity, both distinct deaths, violet-night channel/crackle look, continuous non-beaded skin, and overall Manafold likeness.
- **Loop regressions:** one-frame pre-wrap resets/pops at `hasty` **0238→0239→0000**, `flight` **0350→0351→0000**, and `fall` **0338→0339→0000**. By contrast, `drift` **0299→0000**, `rest` **0399→0000**, `pirouette` **0239→0000**, and `crackle` **0599→0000** remain smooth.
- **Round-body regression:** `trick` **0100–0324** collapses into a broad faceted/slab-like mass, with the antenna buried or fused into it rather than preserving the round bouncy rotation.
- **Lightning-identity regression:** `mana-blue`, `mana-green`, and `mana-boil` **0000–0599** lack the protected connected white-core/blue-shimmer read, even though the depth checks on the normal lightning subjects pass.
- Other localized artifacts: a straight white/cyan cross-bar in `trick` **0361–0371**; sharp impact pinches/faceting in `damage` **0024–0049, 0140–0174, 0260–0299, 0400–0424**; minor taper stair-stepping in `mana-blue`/`mana-green` **0075–0199, 0400–0474**.
- No open seam, detached beaded skin, rear-tip pop, Lasso discontinuity, death regression, or loss of overall likeness was found.

## Ranked genuine publish blockers

1. **Five-joint shipping path is both visually under-articulated and memory-unsafe.** Fix both `swal[3]` call sites to provide five valid entries, then rebuild and re-author/review plainly distinct Front/A/B/C/End motion in real clips.
2. **Broad body translucency does not read.** Much of `channel`, plus `startle` **0050–0098** and `taunt3` **0225–0367**, still reads as opaque body plus external haze.
3. **Protected form and wraps regress.** Repair the slab/fused-body `trick` interval **0100–0324** and the one-frame pre-wrap resets in `hasty`, `flight`, and `fall`.
4. **Rear surface socket is not visible throughout.** Rework/recheck the `rest`, `pirouette`, `trick`, and `mana-boil` ranges above at native 384x240.
5. **Pirouette loses the internal body/head boundary.** Fix **0004–0049** and **0098–0149** without deleting cyan/front energy or antenna-top ink.
6. **Three menu variants lose the protected connected lightning identity.** Restore a readable white core plus blue shimmer in `mana-blue`, `mana-green`, and `mana-boil` without reintroducing smear.
7. **Localized rendering artifacts remain:** the `trick` cross-bar, repeated `damage` impact pinches/faceting, and exposed menu-variant taper steps at the ranges above.

There are genuine publish blockers. Pass 16 is not ready to ship in this form.