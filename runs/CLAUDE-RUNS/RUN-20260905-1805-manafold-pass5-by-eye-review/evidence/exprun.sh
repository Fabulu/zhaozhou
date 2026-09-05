#!/usr/bin/env bash
set -e
cd /c/programmieren/zencrifice/manafold-pass5-review
export ZIXX_EXP=celmain ZIXX_LIGHT=diagonal-cool-cross
B=./build-exp/bin/zhao-reel-cel.exe
env -u U02_EXP_HALO -u U02_EXP_STENCIL -u U02_EXP_MOTES -u U02_EXP_CAMK $B exp-A-ship manafold-curious >/dev/null 2>&1
U02_EXP_HALO=4,6 $B exp-B-smallmote manafold-curious >/dev/null 2>&1
U02_EXP_STENCIL=470 $B exp-C-bigstencil manafold-curious >/dev/null 2>&1
U02_EXP_CAMK=360000 $B exp-D-closecam manafold-curious >/dev/null 2>&1
U02_EXP_HALO=4,6 U02_EXP_MOTES=40 $B exp-E-small40 manafold-curious >/dev/null 2>&1
U02_EXP_HALO=11,15 $B exp-F-bigmote manafold-curious >/dev/null 2>&1
U02_EXP_HALO=4,6 U02_EXP_STENCIL=430 U02_EXP_MOTES=34 $B exp-G-combo manafold-curious >/dev/null 2>&1
echo EXPDONE
