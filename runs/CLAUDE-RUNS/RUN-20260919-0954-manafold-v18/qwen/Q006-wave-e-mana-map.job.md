# Q006 wave-e-mana-map
max_tokens: 12000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Prepare Wave E of Manafold version 18. Owner direction: "Drift I think is a relic with a weird mana, crackle too. Maybe keep them but change the mana to the normal one? Blown has similar issues." and "Hasty has the old smear effect again, we're getting rid of that everywhere."
Ratified plan (binding): disable the 48x30 persistent mist history plane (`u02_mist`) on EVERY live Manafold subject; keep contour shell; keep Drift/Blown on candidate 9 unless no-history pictures expose another fault; make Crackle candidate 9 + day presentation, retaining an exact candidate-4/night control for comparison. The "normal" shipping mana is `u02_mana = 9` (fold + lightning strand) on the ordinary day backdrop.

## Questions
1. From the input, list how each of these subjects is configured: manafold-crackle, manafold-blown, and the generic live-subject builder (subject_u02_clip): u02_mana value, u02_mist, u02_smear, backdrop/night flags, camera overrides, anything else special. Cite lines.
2. What exactly makes Crackle "night" (which flag/field/function), and what is the minimal change to put Crackle on candidate 9 + the ordinary day presentation while keeping a same-binary control subject (e.g. manafold-crackle-legacy) that reproduces today's bytes exactly? Show the code sketch.
3. The minimal change to turn off u02_mist for every live subject while leaving archived/diagnostic subjects (mist variants, fogprobe, etc.) exactly as they are. Which lines, and how do you tell live from diagnostic in this code?
4. Anything in the input suggesting Drift or Blown have special mana/smear settings beyond the mist? (Drift may not be in the input; say "not shown".)

## Inputs
tools/reel/zhao_reel.cpp:5530-5640
tools/reel/zhao_reel.cpp:5800-5885
tools/reel/zhao_reel.cpp:8715-8730
tools/reel/zhao_reel.cpp:8875-8930
