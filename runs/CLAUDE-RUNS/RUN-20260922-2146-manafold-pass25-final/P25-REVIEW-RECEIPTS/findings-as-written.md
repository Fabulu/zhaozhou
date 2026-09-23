# Reviewer findings, running

## Independent build
Built cel + mbolt + mback + mrear + msmooth + mrod + mnodule + mspan from a CLEAN
tree at b3c5760e into .tmp/p25rev. ALL_RC=0. Binary MD5s differ from the
implementer's (different output path), so the check is BEHAVIOURAL.

## R1. The shipping bank reproduces, 22/22
My renderer + my invocation, production ink, one call:
`diff crcs-backball.txt <my ship.crc>` is EMPTY. All 22 sequence CRCs match.
=> the committed receipt describes the tree at HEAD.

## R2. mbolt at 96 mm, my binary, my run
- 0 of 22 subjects intersect. B1 green on every row, every key+midpoint.
- B3: "all 22 live subjects carry 3D avoidance, 0 intersections remain anywhere"
- B4 MIRROR: 22 rows == u02::kBoltLiveSubjects, in order.
- B5 CLOSEST APPROACH: 32.3 mm on manafold-channel f247.  MATCHES the report.
- At 46 mm: 0 hits, nearest **4.4 mm on blown f130**. MATCHES.

## R3. NON-MONOTONICITY -- confirmed, and the gate fires
- 56 mm: rc=1. `FAIL B1 CLEAR manafold-blown: 3 of 20672 ... worst 27.7 mm at f122`
  and `FAIL B3 ROLLOUT: 22 of 22 avoid; 3 intersections remain across the bank`.
  MATCHES the report's "56 -> 3 hits".
  ALSO: B3 goes red WITH B1. That is pass-25's own repair firing on legal
  stimulus -- pass 24's B3 would have printed 0 here while B1 was red.

## R4. FULL clearance sweep, my binary, 12 rungs -- CONFIRMED
dirty: 56(3) 66(8) 70(11) 76(4) 106(1)   clean: 46 86 96 116 130 150 170
Every rc, every hit count and every worst-subject/frame matches the committed
receipt EXCEPT two INFO min-clearance numbers:
  116: receipt 56.7 (hover f99)  -> mine 81.5 (trick f261)
  130: receipt 36.1 (hover f99)  -> mine 59.8 (crackle f492)
Both are rows whose minimum was ON HOVER, and the back-ball damping moved
hover's rods. So `clearance-sweep.txt` PREDATES the back-ball packet. Benign:
the clean/dirty verdict is unchanged on all 12 rungs, both stale numbers moved
in the SAFE direction, and the SHIPPED rung (96) is unaffected because its
minimum is on channel. Noted, not a blocker.

## R5. mback, my binary
gate rc=0.  G1 4.929<=5.600  G2 1.380<=1.700  G3 13.005>=11.5 and 1.850>=1.2.
Controls, each failing for its OWN reason:
  --fail-undamped rc=1  G1 6.586 G2 2.339 breach; G3 OK (18.514/3.285)
  --fail-window   rc=1  same two breach (identity filter reproduces undamped)
  --fail-dead     rc=1  G3 breaches 10.587mm / 0.846deg; ceilings fine
=> the ART FLOORS are reachable with legal stimulus. No mutant needed.

## R6. THE DECOMPOSITION REPRODUCES NUMBER FOR NUMBER
socket-follows-body 65.0% / rear rod aim 90.3% of END ang / upstream life 81.1%
of C pos and 100.0% of C ang / station B alone -146.4% on the last rod.
Identical to the committed table. The diagnosis is verified, not just reported.

## R7. CENSUS -- the owner's premise, checked
slot 23 (crackle, undamped) ANGr = 1.08; slot 0 shipped = 0.99.
Among LIVE clips 1.08 is the ONLY one >= 1.00. (Slots 15/16 read 1.51/1.20 but
are diagnostics, not live.) Confirmed.

## R8. FRONT BALL across three passes (the owner-facing question 4a)
                 front ball travel   front ball own spin
  pass 23           8.698 mm/s          2.159 deg/s
  pass 24           8.904 (+2.4%)       2.166 (+0.3%)
  pass 25 ship      8.922 (+0.2%)       1.396 (-35.5%)
KEY: pass 24 delivered the owner's "the front could move a little more" almost
ENTIRELY AS TRAVEL. In SPIN it delivered +0.3% -- essentially nothing. So the
35% spin damping cannot have undone an ask that was never delivered in spin,
and the travel the owner did get is preserved to +0.2%.

## R9. No bound relaxed
git diff p24main..HEAD over tools/reel: the ONLY removed constexpr in the whole
tree is `kBoltRodClearanceMm = 46` (raised to 96, the pass's subject).
No static_assert removed. No gate threshold lowered.

## R10. No gate default samples a subset
mbolt walks `for (f=0; f<r.frames; ++f)` -- every frame, no stride; B1 prints the
full presentation length per subject (hover 600). B4 binds its 22 rows to
u02::kBoltLiveSubjects. mback judges slot 0 and PRINTS every other slot as info.

## R11. The lightning cannot be restyled -- structural
`bolt_avoid_rods(int32_t pts[][3], int n, int lo, int hi)` (manafold_fx.h:2373)
takes ONLY the point array. Radius, colour, gain, stamp density, morph clock and
station identity are not in scope. msmooth (persistent identities + 60 Hz
continuity) rc=0 on my build. mrod, mrear, mspan, mnodule all rc=0.

## R12. Scope, at FRAME BYTE level, my own renders
  crackle  ship vs damp-off:  27 compared, 0 differing  (CONTAINMENT PROOF)
  hover    ship vs damp-off:  50 compared, 50 differing
  inspect  ship vs damp-off:  27 compared, 27 differing
Sequence CRC over the whole bank: exactly hover + inspect differ. 20/22 identical.

## R13. Exact-off, my own render
`ZHAO_U02_BACKBALL_DAMP_PM=0` -> 22/22 identical to committed P25 crcs-ship.txt.
Shipping -> 22/22 identical to committed P25-BB crcs-backball.txt.

## R14. Committed matrix audited
265 PASS / 0 FAIL / 265 rows, MATRIX_RC=0. e-item2 changes exactly the 19
non-authored-beat subjects; curious, startle and taunt III absent by name.

## VISUAL VERDICTS (my own renders)
V1 BACK BALL (L3, hover, tight crop on the rear column, 7 consecutive frames,
   pass-24 rig above / shipping below): CONFIRMED CALM. Undamped the column's
   silhouette boils -- its width and lean change frame to frame and the top mass
   jumps. Damped it holds ONE shape and drifts. And it still drifts: the rear is
   not frozen, the loop still breathes, the antenna keeps life.
V1b Same judgement on the FIXED camera (L2, crackle bake, 5x): clearer still --
   undamped the rear column's left edge jitters every frame; damped it is stable.
V2 FRONT BALL (L4 whole antenna 5 frames, L5 extreme zoom on the front joint,
   pass-24 vs pass-25): INDISTINGUISHABLE. Same position, same shape, same
   shading. The ball has no legible surface detail at this framing, so its own
   spin is not something the shot can show.
V3 BOLTS (L6, channel f247 and blown f130, pass-24 lightning vs pass-25):
   pass 24 -- the white bolt lies ON the rods and merges with them; pass 25 --
   a clear band of background shows between bolt and rod on both. Bead size,
   colour and density unchanged; only the path moved.
V4 HOVER TIGHTEST CLOSURE (L8b, f469/471/473, clearance 46 vs 96): the push does
   carry a small part of the figure outside the rods at 96, at the top-right,
   hugging the upper ball. It still reads as energy belonging to the antenna,
   not as a detached chain in air. ACCEPTABLE, as declared.
V5 EYES (L7, Rest, 600 above / 800 below, 6 frames): plainly stronger -- the
   stars travel further and change size visibly -- and still not loud. They come
   NEAR the inner lens edge at f008/f264 but do not ride or cross the rim.
V6 CRACKLE (L9, near-native 2x, shipping-undamped vs if-damped): the fault IS
   visible on crackle. Less glaring at 2x than at 5x, but present, and crackle is
   a LONG idle under a FIXED CLOSE camera -- the two conditions that make a
   fidgety rear noticeable.
