# Q015 review-wave-d-clips-low
max_tokens: auto
effort: xhigh
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Retry of Q013 (its token budget was too small at a lowered effort). Bug review of committed commit 50803207, file manafold_clips.h: public Front X/Y flex for ten clips must go ONLY through the appended HingePlay front fields (the duplicate FrontFlexPose/front_flex_at API was supposed to be removed); order on JunctionF is rest/fold Z -> Front X -> Front Y; every Front curve begins and ends at identity and is quintic C2; gain and mute scale only Front X/Y; Trick holds Front at identity through its planted keys.

## Questions
1. Is there any leftover second Front path (type, function or extra multiply onto JunctionF)? yes/no + lines.
2. Any Front table whose first and last keys differ, or any non-C2 (linear) interpolation? yes/no + lines.
3. Can gain/mute touch anything other than Front X/Y, and is Trick Front identity on every planted key? yes/no + lines.

## Inputs
show:50803207:tools/reel/manafold_clips.h
