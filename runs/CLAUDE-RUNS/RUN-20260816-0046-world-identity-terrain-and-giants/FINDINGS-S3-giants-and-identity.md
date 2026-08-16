# RECON S3 — Giants: Citizen Kabuto + design-identity synthesis

*Fable recon agent, 2026-08-16. Giants is web-research only (no local copy; verified `C:\programmieren` has no Giants/Kabuto/Planet Moon material). Persisted by orchestrator.*

## A. What Giants teaches

### A1. Kabuto himself
- **Scale is a gameplay variable, not an asset constant.** ~30–60 ft "and growing" — eating Smarties increases his dimensions and power; at max size he lays eggs that hatch mini-Kabutos. **The giant is a growth curve.**
- **Enormity was sold by presentation, not tech:**
  - **Movie-monster animation vocabulary** — attacks modelled on classic giant-monster films plus pro wrestling (elbow drops, foot stomps, cannonballs, the "butt flop"). Weight came from *choreography*.
  - **Special cameras** — first-person **from inside Kabuto's mouth** when targeting prey; a view **from beneath his foot** during stomps. Scale sold by putting the camera where only a giant creates a viewpoint.
  - **Audio** — the roar and stomps are what reviewers remember.
  - **Interaction verbs at giant scale** — pick up any lifeform, eat it, throw it, or **impale it on his horns "for later"** (a visible larder: small units become his decoration).
- **THE CAUTIONARY FINDING:** Kabuto had a deliberate weak point and an eat-to-heal economy, but reviewers found him **"boring to play" — sluggish, a big target. The giant read magnificently and played worst of the three.** Enormity sells screenshots; sluggishness kills in the hands.
- No published LOD/collision postmortem exists (no Gamasutra postmortem for Giants at all). Custom engine "Amityville" (Glide/OpenGL/D3D), hardware T&L; shipped crash-prone — the tech was over its skis.

### A2. The three asymmetric factions
Meccaryns (5-man jetpack squad shooter, base-building), Sea Reapers (fast solo spellcaster, swimming first-class), Kabuto (kaiju, no base, eats to grow). Multiplayer capped **5 vs 3 vs 1** — **asymmetry balanced by headcount, not stat mirroring**. Level design carried it: "cover to hide behind for Meccaryns, large water for Reapers, creatures for Kabuto to eat" — different *terrain affordances*, not different engines. One renderer, one world format, three **camera-and-verb packages**: asymmetry lives in software, cheap in hardware terms.

### A3. Terrain and world
24 islands "traveling through space". Signature brag was **draw distance**: "ocean, then hills, then higher hills, then clouds… not obstructed by fog, or an invisible wall, or loading screens", distant objects **slightly blurred as a depth cue** (not fog cutoff). CPU-computed terrain bump mapping, rippling reflective water with sun-ray reflections, dynamic shadows.

**Destructibility: weapons scarred terrain modestly, but the ambitious version was DESIGNED AND CUT for schedule** — Reapers **gorging water channels to isolate segments of land**, and Kabuto sculpting **"mud shepherd" defenders from the ground**. *Giants wanted to be a terrain-deformation game and couldn't afford it in 2000. That cut feature list is practically a requirements doc for Zhaozhou.*

### A4. Visual signatures worth stealing (cheap at 240p fixed-function)
1. **Sun-rays on water** — specular streak toward the sun = a CLUT ramp indexed by a fixed-point dot product. Marries with the ratified Noctis suns.
2. **Depth-cue blur/haze instead of fog walls** — a distance-banded CLUT shift toward sky palette evokes "unobstructed to the horizon" for free.
3. **Islands against open sky in every direction** — the vista *is* the identity shot. Floating islands give it harder: horizon *below* the terrain.
4. **Movie-monster staging** — costs nothing in silicon; giant attacks as short, readable, weighty set-pieces (wind-up, impact, aftermath particles).
5. **Terrain scarring as light-sourced decals** — our SURFACE.STAMP / FIELD.SEQ.STAMP lane.

### A5. Giant-vs-small in one frame
Planet Moon largely **dodged** the hard version: factions mostly played in separate missions; fixed 5/3/1 counts plus brute-force T&L covered multiplayer, and it shipped crash-prone anyway. Their real solutions were design-side. **Nobody in 2000 solved giant-and-squad-in-frame as a rendering problem — open territory for a machine with a screen-space-error LOD governor.** Our split-screen (one player as giant at 256×192, one as squad) is exactly the frame Giants never had to render.

**Sources:** [Wikipedia: Giants: Citizen Kabuto](https://en.wikipedia.org/wiki/Giants:_Citizen_Kabuto) · [Wikipedia: Planet Moon Studios](https://en.wikipedia.org/wiki/Planet_Moon_Studios) · [PCGamesN — Nick Bruty interview](https://www.pcgamesn.com/first-wonder/making-it-in-unreal-how-the-madness-of-mdk-and-giants-citizen-kabuto-feeds-into-first-wonder) · [GameRevolution review](https://www.gamerevolution.com/review/32723-giants-citizen-kabuto-review) · [The 500 retrospective](https://dollerz.com/the-500/349-325-a-running-start/giants-citizen-kabuto/) · [runoldpcgames tech notes](https://runoldpcgames.wordpress.com/2024/03/27/giants-citizen-kabuto/) · [GiantsWD modding](https://www.giantswd.org/?file=60) · [PCGamingWiki](https://www.pcgamingwiki.com/wiki/Giants:_Citizen_Kabuto)

## B. Synthesis for the Zhaozhou identity

### B1. The signature visual moment
**A giant kneels at the broken edge of a floating island and tears a piece off it — under a Noctis sun.** The shot must contain: (a) an island edge with sky *below*, cliff strata visible (FORGE.CLIFF); (b) a creature big enough that its hands work at terrain scale, mid-deformation, debris falling off the world's edge as polygon particles; (c) a low sun, lens-flared, sun-streak on water pooled in the scar; (d) a squad silhouetted on the giant or ridge for scale. Sacrifice gives the floating battlefield, Noctis the light, Giants the creature — **the moment (giant reshaping the island itself) is ours alone.**

### B2. Giant capabilities, costed
1. **The giant is a terrain body, not a big mesh** — back/shoulders use the same meshlet + LOD-governor path as terrain, limbs as rigid parts on the transform graph. *Cost: low-moderate* (reuses MEASURE.GOVERNOR + meshlet fetch; needs GEOM.LOOM to parent terrain-class meshlets under animated nodes). Payoff: **small units can stand on the giant with no special case**, and the governor handles giant-fills-screen vs giant-on-horizon identically.
2. **Heavy-gait profile + impact event chain** — LOOM gait/oscillator nodes with a slow phase clock and anticipation-impact-settle envelope; impact token triggers FIELD.SEQ.STAMP crater + PART.SPAWN debris + camera-shake field. *Cost: cheap* — parameters and event plumbing on planned blocks. **This is where "feels enormous" lives.**
3. **Terrain interaction verbs**: stomp-crater, gouge (drag a furrow), **edge-bite** (remove a chunk from an island rim — the Reaper channel-cut Giants had to cut, done one better because our islands have no water table to fake). *Cost: moderate, and **must be decided before Mantle's format freezes*** — edge-bite needs hole/rim topology in the terrain field.
4. **Eating/carrying small units** — reparent a unit's transform node under the giant's hand/mouth node. *Cost: near-free* on a hardware transform graph. The horn-larder gag is a one-line reparent and an enormous character win.
5. **Growth as scale on the root node + shifting screen-space-error budget.** *Cost: cheap mechanically*; the honest cost is **fill and geometry at max size in split-screen** — the giant player's own view of themselves must degrade gracefully (micro-mesh/splat tiers on far parts).
6. **Giant-view camera package** (mouth-cam, under-foot-cam) — *free*: camera transforms are nodes.

### B3. Donor conflicts and resolutions
| Conflict | Resolution |
|---|---|
| Sacrifice's saturated chaos vs Noctis's disciplined emptiness | **Zone it**: combat palettes Sacrifice-loud *on the islands*; sky/sun/void Noctis-disciplined. CLUT8 makes this literal — island CLUTs vs sky CLUTs. **The contrast is the look.** |
| Giants' playable giant vs Sacrifice's wizard-commands-armies | Don't make the giant a third symmetric side — Giants proves it played worst. Better: a summonable/awakened terrain-scale entity in the wizard loop, or the second split-screen player's asymmetric role (1 giant vs 1 commander), stealing the 5-vs-1 headcount balancing. |
| "Giant must feel heavy" vs "sluggish = boring" | Weight in the **world's response** (craters, debris, shake, units scattering), not in input latency. **Snappy controls, heavy consequences.** |
| Terrain-as-floor vs "more deformable than Sacrifice" | Floating islands resolve it: deformation has *stakes* (breach an island, drop things into the sky) that neither donor's sea-level worlds could offer. |
| Giants' unobstructed-horizon pride vs 240p fill budget | The governor's splat/glint tiers **are** the depth-cue blur Giants used — distance degradation as aesthetic, not apology. |

### B4. What is genuinely novel
None of the three donors could do **terrain, creatures, and particles as one continuum**. Sacrifice's terrain deformed but creatures were separate skeletal meshes; Giants' giant was a big mesh on mostly-static islands; Noctis had no creatures. Here: a creature can *be* terrain (walkable, deformable, LOD-governed), terrain can *become* creature (an island shoulder that stands up — Giants' cut "mud shepherds", buildable as a FIELD.SEQ.EARTH program driving LOOM nodes), and both shed polygon particles from the same pipeline. Plus the frame nobody has rendered: **two players at 60 Hz, one of them the monster the other is fighting, each with a correct LOD solve of the same deforming island.**

### B5. Ranked proposals
1. **Unit reparenting for grab/eat/carry/ride** — *near-zero cost*, pure transform-graph usage; **biggest character-per-gate win in the study**.
2. **Heavy-gait profile + impact event chain** — *low cost*; where "enormous" lives.
3. **Island rim topology in the terrain field format** (edge-bite, breaches, undercuts) — *moderate, MUST BE DECIDED NOW* before Mantle/FORGE.CLIFF freeze. **The "more deformable than Sacrifice" claim depends on it.**
4. **Terrain-class giant** (torso as governor-managed meshlet surface, walkable) — *moderate*; the novel flagship. Prototype in sim before committing silicon assumptions.
5. **Growth-as-scale with per-size LOD budgets** — *low mechanically*, real in split-screen fill.
6. **Palette zoning + sun-streak water ramp** — *trivial*; free identity.
7. **Mud-shepherd resurrection** (terrain stands up as creature via EARTH field driving LOOM) — *high cost*, new field↔transform-graph contract seam. Specify the seam, prototype later.
8. **Giant as symmetric playable faction** — *high cost, low priority*. Make the giant magnificent to *fight and summon* first.

**Most important negative finding:** there is no Giants postmortem because the tech barely shipped — they dreamed of deformable islands and cut them. **This machine was purpose-built to un-cut that feature.**
