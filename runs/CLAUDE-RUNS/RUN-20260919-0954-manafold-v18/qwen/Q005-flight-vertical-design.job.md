# Q005 flight-vertical-design
max_tokens: 14000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Owner direction 19, item 7: "Flight should move up and down so you can actually see it's flight." Binding: re-author Flight so the creature visibly travels up and down as flight, preserving framing (it must stay in the camera frame), loop continuity (last key flows into the first), antenna/effect smoothness (C2), and no smear. The plan says: render independent AMPLITUDE and INTEGER-CYCLE ladders, then choose by eye; ONE phase continues to drive root Y, pitch, breath and lagged carriers. Recon found the current Flight runs four 300 mm bobs across 176 keys, which reads as a repeated bounce rather than flight.

You are preparing the ladder, not choosing the art value (values are chosen by looking at renders).

## Questions
1. Describe the current Flight motion key by key: which constants drive root X/Y/Z, pitch, breath, sway, carriers; cite lines. Is the loop seam exact (key K-1 -> key 0) for every channel? Show why.
2. Why might four 300 mm bobs read as a "bounce" rather than flight? Answer from the motion shape in the code (e.g. waveform, phase of pitch vs height, symmetry of rise/fall), not from taste.
3. Propose the named knobs for an amplitude ladder and an integer-cycle ladder (e.g. kFlightBobAmpMm, cycles per loop replacing kFlightBobPeriodKeys), plus any shape knob (asymmetric rise/fall, pitch lead) that would make vertical travel read as flight. Keep every value a named constant with an env-override hook only if the file already uses that pattern (say whether it does).
4. Propose 3-4 ladder rungs per knob and state the framing risk for each (camera constant kU02CamKFlight=250000 is the framing; you cannot compute the frustum, so state what must be checked on render).
5. What gates must stay green (seam, C2 of root and carriers), and which existing checks cover them (only if visible in the input)?

## Inputs
tools/reel/manafold_art.h:2755-2800
tools/reel/manafold_clips.h:4826-4910
tools/reel/manafold_clips.h:94-120
