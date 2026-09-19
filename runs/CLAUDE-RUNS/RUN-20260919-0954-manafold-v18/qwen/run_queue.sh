#!/bin/sh
# Runs queued Qwen jobs strictly one at a time.
for j in "$@"; do
  powershell.exe -NoProfile -File 'C:\programmieren\zencrifice\manafold-p16\zhaozhou\tools\qwen\qwen.ps1' run "$j"
done
