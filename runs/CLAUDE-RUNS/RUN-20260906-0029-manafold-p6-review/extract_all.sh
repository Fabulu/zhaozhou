#!/bin/sh
# Extract EVERY frame of every shipped webm to PNG at native 384x240.
# No scaling, no filters -- the encoded pixels, as the site serves them.
set -e
M="$1"; O="$2"
mkdir -p "$O"
for f in "$M"/*.webm; do
  n=$(basename "$f" .webm)
  mkdir -p "$O/$n"
  ffmpeg -v error -y -i "$f" -vsync 0 -pix_fmt rgb24 "$O/$n/%04d.png"
  echo "$n $(ls "$O/$n" | wc -l)"
done
