# SPEC v1: Zixxtrixx pass 3 - head, eye, tail, pink, salto, fall

**Run ID:** RUN-20260827-0628
**Created:** 2026-08-27 06:28 UTC+02:00
**Status:** Active
**Previous Version:** N/A

---

## Objective

Land Fabian's seven third-pass items on Zixxtrixx WITHOUT regressing the
approved idle/walk motion, the S, or the body thickness: (1) head bigger and
looking up, mouth smaller, head-on view compared against Concept/Front.png;
(2) eye texture rotated so the pupil reads as the drawing's top-to-bottom
band; (3) fins raked almost parallel to the body, both faces big-pink +
weak-green; (4) pink less neon everywhere; (5) salto a lot higher, one long
straight ~30-deg skewer sticking in the ground, camera framing the burial,
STRONG screen shake proven on rendered frames; (6) fall: weaker rotation,
more slow bendy wobble. Probe bands verified; --check gate passes; both
repos committed and pushed.

---

## Scope

**In Scope:**

- tools/reel/zixxtrixx.h (stance, taper, blades, salto, fall, colours)
- tools/reel/zhao_reel.cpp (tracking camera aim, shake, diagnostic subject)
- tools/pack/mkcreaturepage.py + regenerated zixxtrixx_page.h
- Upheaval WORKLOG + 03-ANIMATION guide lessons

**Out of Scope:**

- idle/walk motion (approved; only shared stance/paint touches, re-verified)
- site video regeneration (deferred to sign-off; publishing is explicit)
- LOD ladder / mips

---

## Constraints

- <=32 bones, <=2 influences/vertex, integer-only, determinism
- ground contact authored only; salto burial is the authorised exception
- never cmake for the reel; direct g++ compile

---

## Don't Retry

*Record failed approaches here to avoid re-learning after context compaction*

---

## Open Questions

- [Question 1]
