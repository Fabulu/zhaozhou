# Q013 review-wave-d-clips
max_tokens: 14000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Independent bug review of committed commit 50803207 (Manafold v18 Wave D), file manafold_clips.h. Intent: public Front X/Y flex performances for ten clips, carried ONLY through the appended HingePlay front fields (a duplicate FrontFlexPose/front_flex_at API had to be removed); fixed order on JunctionF is rest/fold Z -> Front X -> Front Y; each clip's curve begins and ends at identity (loop seam) and is quintic C2; a global gain (1500 pm) and a mute control scale only the Front X/Y; Trick keeps Front at identity through its planted interval and pivots the body about the planted support centre.

## Questions
1. Is there exactly ONE Front X/Y representation and application path now? Any leftover duplicate type/function or second multiply onto JunctionF?
2. Order of composition on JunctionF: is it rest/fold Z, then X, then Y, everywhere?
3. Seams and C2: does every Front table start/end at identity; any clip where the last key != first key? Any linear (non-C2) interpolation?
4. Gain/mute: can they affect anything other than Front X/Y (e.g. Neck, signed length)? Any integer overflow in gain scaling?
5. Trick: is Front exactly identity on every planted key?
6. P1/P2 only, else "none found".

## Inputs
show:50803207:tools/reel/manafold_clips.h
