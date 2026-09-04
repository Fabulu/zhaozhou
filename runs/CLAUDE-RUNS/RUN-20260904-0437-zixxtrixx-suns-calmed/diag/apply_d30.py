"""Direction 30 edit script: calmed sun table + additive-normal rewiring.

Run from the zhaozhou repo root. Every replacement asserts it matched exactly
once, so a drifted file fails loudly instead of half-editing.
"""
import re

p = "tools/reel/zhao_reel.cpp"
s = open(p, encoding="utf-8").read()
orig = s

# ---- Sun table: calmed Direction 30 values ----
table = {
 "Idle":       ("15556,  kZixxSunHeightMm, 15556",  (390, 230, 30),  (250, 120, 10),  "golden morning"),
 "Walk":       ("-12619,  kZixxSunHeightMm, 18022", (30, 130, 450),  (15, 55, 220),   "azure day"),
 "Run":        ("12619,  kZixxSunHeightMm, 18022",  (450, 160, 15),  (250, 70, 5),    "hot orange"),
 "Look":       ("12619,  kZixxSunHeightMm, 18022",  (260, 45, 420),  (140, 15, 230),  "violet"),
 "Balance":    ("14142,  kZixxSunHeightMm, 16853",  (25, 340, 320),  (10, 150, 185),  "teal"),
 "Taunt":      ("21666,  kZixxSunHeightMm, -3820",  (400, 30, 340),  (230, 10, 155),  "magenta"),
 "SlowTaunt":  ("11000,  kZixxSunHeightMm, 19053",  (370, 105, 170), (185, 45, 85),   "rose dusk"),
 "JumpOne":    ("13544,  kZixxSunHeightMm, 17336",  (170, 420, 30),  (80, 210, 15),   "spring lime"),
 "JumpMulti":  ("-13544,  kZixxSunHeightMm, 17336", (465, 105, 12),  (255, 55, 5),    "sunset red-orange"),
 "Attack":     ("14142,  kZixxSunHeightMm, 16853",  (480, 30, 20),   (270, 10, 10),   "crimson"),
 "Hit":        ("3820,  kZixxSunHeightMm, 21666",   (45, 165, 400),  (20, 75, 210),   "steel blue"),
 "Damage":     ("21666,  kZixxSunHeightMm, 3820",   (420, 195, 20),  (220, 90, 5),    "ember amber"),
 "Knockdown":  ("4574,  kZixxSunHeightMm, 21518",   (215, 35, 385),  (105, 15, 210),  "bruise violet"),
 "Fall":       ("-14142,  kZixxSunHeightMm, 16853", (90, 245, 435),  (45, 115, 225),  "ice blue"),
 "HitFloor":   ("21518,  kZixxSunHeightMm, 4574",   (400, 210, 35),  (210, 95, 10),   "dust orange"),
 "SaltoDummy": ("11000,  kZixxSunHeightMm, 19053",  (320, 355, 25),  (150, 165, 10),  "chartreuse gold"),
 "SaltoFly":   ("-11000,  kZixxSunHeightMm, 19053", (20, 105, 465),  (10, 45, 255),   "deep azure"),
 "SaltoSix":   ("16853,  kZixxSunHeightMm, 14142",  (420, 300, 30),  (225, 130, 10),  "gold"),
 "SaltoNine":  ("-16853,  kZixxSunHeightMm, 14142", (435, 45, 230),  (245, 18, 120),  "hot pink"),
 "Death":      ("21916,  kZixxSunHeightMm, 1917",   (450, 20, 12),   (250, 6, 6),     "deep red"),
 "Death2":     ("3062,  kZixxSunHeightMm, 21785",   (170, 185, 420), (80, 85, 215),   "moonlight"),
}
for name, (pos, m, a, mood) in table.items():
    pat = re.compile(r"constexpr ZixxSunSpec kZixxSun" + name + r"\s+\{[^}]*\};[ \t]*// [^\n]*")
    new = ("constexpr ZixxSunSpec kZixxSun%-10s {%s,  kZixxSunInnerMm, kZixxSunOuterMm, "
           "fxm(%d), fxm(%d), fxm(%d), fxm(%d), fxm(%d), fxm(%d)};  // %s"
           % (name, pos, m[0], m[1], m[2], a[0], a[1], a[2], mood))
    s, n = pat.subn(lambda _mo, new=new: new, s)
    assert n == 1, name

# Comment block above the table: record Direction 30 calming.
old_c = (
"""// Per-clip suns. Azimuth is authored per camera (yaw 0 / 45 / 67.5 deg
// subjects) so the sun always sits high on the camera side of the sky, offset
// to one flank -- never behind the animal relative to the shot. Colour is the
// clip's MOOD; dominant channel strong, complement suppressed, judged by eye
// at native 384x240 (art law: these numbers are knobs, not derivations).""")
new_c = (
"""// Per-clip suns. Azimuth is authored per camera (yaw 0 / 45 / 67.5 deg
// subjects) so the sun always sits high on the camera side of the sky, offset
// to one flank -- never behind the animal relative to the shot. Colour is the
// clip's MOOD; dominant channel strong, complement suppressed, judged by eye
// at native 384x240 (art law: these numbers are knobs, not derivations).
// Direction 30 calmed the whole table: the first authoring lifted the animal
// +60..+85 mean counts with near-equal R/G on warm suns (the mult gains rode
// the green-rich pigment and diluted every hue toward white -- the owner saw
// "one super bright normal sun, no color"). Adds are cut to ~40% and mults to
// ~50% of Direction 29, complements near zero, so the ADD carries a nameable
// hue and pigment/form dominate again (reference: Archive Generation 18).""")
assert old_c in s
s = s.replace(old_c, new_c)

# ---- Direction 30: additive is NORMAL ----
old = (
"""// Direction 29 revert switch (reel-side; the silicon-side revert is the
// g_creature_additive_light gate plus the CREATURE.LIGHT parameters).
bool g_zixx_suns_enabled = true;""")
new = (
"""// Direction 29/30 revert switch (reel-side; the silicon-side revert is the
// g_creature_additive_light gate plus the CREATURE.LIGHT parameters).
// ZIXX_SUNS=off restores the PRE-SUNS bank byte-identically: the clip suns go
// dark AND additive-normal mode drops, so the moving-light clip renders its
// original multiplicative-only form (proof: evidence-gateoff-identity in the
// suns-calmed run).
bool g_zixx_suns_enabled = true;
// Direction 30: the additive term is the NORMAL lighting mode for every
// animation -- not a per-subject experiment a future pass must remember to
// raise. The library gate zc::g_creature_additive_light itself stays
// default-OFF (the silicon-cost revert path, pending the ALM/M10K sweep);
// the reel raises it around every creature compose that carries point
// sources. Cleared together with the suns by ZIXX_SUNS=off.
bool g_zixx_additive_normal = true;""")
assert old in s
s = s.replace(old, new)

# SceneSubject field: remove the per-subject prototype flag
old = (
"""  // Direction 28 ADDITIVE-light prototype subject flag. Sets the compositor's
  // g_creature_additive_light gate (default OFF everywhere else) and gives the
  // moving sources their authored additive colours for this subject only.
  bool creature_additive_light = false;
""")
assert old in s
s = s.replace(old, "")

# ctx field
old = "  bool additive_light = false;  // Direction 28 prototype gate, subject-scoped\n"
assert old in s
s = s.replace(old, "")

# marker tint
old = (
"""    const uint8_t* tint = c.additive_light ? kZixxMovingSourceMarkerAdditive[si]
                                           : kZixxMovingSourceMarker[si];""")
new = (
"""    // Direction 30: additive is the normal mode, so the orbs wear the
    // additive palette (the red orb for the red-emitting orbit) unless the
    // whole look is reverted to the pre-suns bank.
    const uint8_t* tint = g_zixx_additive_normal ? kZixxMovingSourceMarkerAdditive[si]
                                                 : kZixxMovingSourceMarker[si];""")
assert old in s
s = s.replace(old, new)

# compose gate
old = (
"""      zc::g_creature_additive_light = c.additive_light;  // Direction 28 gate
    } else if (c.sun_light) {
      // Direction 29: the clip's sun joins the selected rig (Cool Cross stays
      // underneath); emission is ON through the same gate, scoped and restored.
      zc::g_creature_point_lights = c.moving_sources;
      zc::g_creature_point_light_count = 1;
      zc::g_creature_additive_light = true;
    }""")
new = (
"""      zc::g_creature_additive_light = g_zixx_additive_normal;  // Direction 30
    } else if (c.sun_light) {
      // Direction 29: the clip's sun joins the selected rig (Cool Cross stays
      // underneath); emission is ON through the same gate, scoped and restored.
      zc::g_creature_point_lights = c.moving_sources;
      zc::g_creature_point_light_count = 1;
      zc::g_creature_additive_light = g_zixx_additive_normal;
    }""")
assert old in s
s = s.replace(old, new)

# ctx wiring
old = (
"""    cr_ctx.moving_light = sub.creature_moving_light;
    cr_ctx.additive_light = sub.creature_additive_light;
    cr_ctx.sun_light =""")
new = (
"""    cr_ctx.moving_light = sub.creature_moving_light;
    cr_ctx.sun_light =""")
assert old in s
s = s.replace(old, new)

# colour-source sampling call
old = (
"""        sample_zixx_moving_colour_sources(f, sub.frames, dog_inst,
                                          cr_ctx.moving_sources,
                                          sub.creature_additive_light);""")
new = (
"""        sample_zixx_moving_colour_sources(f, sub.frames, dog_inst,
                                          cr_ctx.moving_sources,
                                          g_zixx_additive_normal);""")
assert old in s
s = s.replace(old, new)

# moving-light subject note + delete the additive subject
old = (
"""  s.note = "FINAL INSPECTION: held signature-S under dim sunlight; four visible "
           "world-space local sources -- the warm inspection lamp plus blue, "
           "orange and green -- move on their own authored paths, drive the real "
           "posed-vertex light, and mix where their pools intersect";
  return s;
}

// Direction 28 ADDITIVE-light prototype: byte-for-byte the published
// moving-light subject except that the additive gate is on, so each coloured
// source also EMITS its authored additive colour scaled by its own
// lambert*attenuation. The published clip is the side-by-side comparison; this
// subject exists so the owner can judge whether an additive term reads as
// light (and whether red can finally cross the green body as red).
SceneSubject subject_zixx_moving_light_additive() {
  SceneSubject s = subject_zixx_moving_light();
  s.name = "zixxtrixx-moving-light-additive";
  s.creature_additive_light = true;
  s.note = "EXPERIMENTAL (Direction 28): the published moving-light inspection "
           "with the prototype per-channel ADDITIVE point-light term enabled -- "
           "the orange orbit emits true red across the green body, the case "
           "multiplicative transport cannot show";
  return s;
}""")
new = (
"""  s.note = "FINAL INSPECTION: held signature-S under dim sunlight; four visible "
           "world-space local sources -- the warm inspection lamp plus blue, red "
           "and green -- move on their own authored paths, drive the real "
           "posed-vertex light with the normal per-channel additive emission "
           "(Direction 30), and mix where their pools intersect";
  return s;
}""")
assert old in s
s = s.replace(old, new)

# dispatch
old = (
"""  if (wanted("zixxtrixx-moving-light-additive"))
    rc |= render_scene(subject_zixx_moving_light_additive());
""")
assert old in s
s = s.replace(old, "")

# ZIXX_SUNS env parsing: clear additive normal too
old = (
"""  if (const char* suns = std::getenv("ZIXX_SUNS")) {
    if (std::string(suns) == "off") g_zixx_suns_enabled = false;
    std::fprintf(stderr, "ZIXX_SUNS=%s (Direction 29 clip suns %s)\\n", suns,
                 g_zixx_suns_enabled ? "on" : "OFF");
  }""")
new = (
"""  if (const char* suns = std::getenv("ZIXX_SUNS")) {
    if (std::string(suns) == "off") {
      g_zixx_suns_enabled = false;
      g_zixx_additive_normal = false;  // Direction 30: one switch, whole revert
    }
    std::fprintf(stderr,
                 "ZIXX_SUNS=%s (Direction 29/30 clip suns + additive-normal %s)\\n",
                 suns, g_zixx_suns_enabled ? "on" : "OFF");
  }""")
assert old in s
s = s.replace(old, new)

# kRed* comment: no longer "the additive subject"
old = "// Consumed only by the additive subject; the published clip keeps kOrange*."
new = ("// Direction 30: additive is normal, so the moving-light clip carries the\n"
       "// red source; ZIXX_SUNS=off reverts it to kOrange* (the pre-suns clip).")
assert old in s
s = s.replace(old, new)

open(p, "w", encoding="utf-8", newline="\n").write(s)
print("edits applied, delta bytes:", len(s) - len(orig))
