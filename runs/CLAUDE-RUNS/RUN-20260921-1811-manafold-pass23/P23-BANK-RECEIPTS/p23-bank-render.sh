#!/usr/bin/env bash
# The pass-23 exact bank: ONE renderer, ONE invocation, 22 live subjects,
# production ink. The renderer is the REVIEWER'S OWN build, the same binary
# whose gate output is byte-identical to the implementation's committed
# receipt -- so the shipped bank is tied to the reviewed configuration rather
# than to a claim about it.
cd /c/programmieren/zencrifice/manafold-p16/zhaozhou
R=./.tmp/p23rev/bin/zhao-reel-cel.exe
OUT=/c/programmieren/zencrifice/manafold-p16/p23-final-reel-22
rm -rf "$OUT"
env -u ZHAO_U02_LIVE_MIST ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross \
  "$R" "$OUT" \
  manafold-hover manafold-inspect manafold-channel manafold-trick manafold-damage \
  manafold-hasty manafold-flight manafold-fall manafold-hit manafold-taunt \
  manafold-taunt2 manafold-death-drop manafold-death-gutter manafold-lasso \
  manafold-blown manafold-taunt3 manafold-drift manafold-curious manafold-startle \
  manafold-rest manafold-pirouette manafold-crackle \
  > .tmp/p23-bank-render.log 2>&1
echo "RENDER_RC=$?" >> .tmp/p23-bank-render.log
