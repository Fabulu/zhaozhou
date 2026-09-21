#!/usr/bin/env bash
# The scope proof's other half: the SAME renderer, the SAME 22 subjects in the
# SAME order, with the press-depth ladder put back to the four values pass 22
# shipped. Everything else is identical, so any frame that differs is the
# press depth and nothing else.
cd /c/programmieren/zencrifice/manafold-p16/zhaozhou
R=./.tmp/p23rev/bin/zhao-reel-cel.exe
OUT=/c/programmieren/zencrifice/manafold-p16/p23-exactoff-reel-22
rm -rf "$OUT"
env -u ZHAO_U02_LIVE_MIST ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross \
  ZHAO_U02_KNEAD_DIP_CLIP_PM=1:730,14:635,17:635,18:590 \
  "$R" "$OUT" \
  manafold-hover manafold-inspect manafold-channel manafold-trick manafold-damage \
  manafold-hasty manafold-flight manafold-fall manafold-hit manafold-taunt \
  manafold-taunt2 manafold-death-drop manafold-death-gutter manafold-lasso \
  manafold-blown manafold-taunt3 manafold-drift manafold-curious manafold-startle \
  manafold-rest manafold-pirouette manafold-crackle \
  > .tmp/p23-exactoff-render.log 2>&1
echo "RENDER_RC=$?" >> .tmp/p23-exactoff-render.log
