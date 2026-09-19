# Q012 review-wave-d-spangate
max_tokens: 14000
effort: medium
root: C:\programmieren\zencrifice\manafold-p16\zhaozhou

## Brief
Independent bug review of committed commit 50803207 (Manafold v18 Wave D), file manafold_spangate.cpp. It gained: a terminal-cap check (only the last loop ring is a 2 mm buried ReturnTip cap; a --fail-terminal-cap control restores the full ring and must fire the burial detector), a swell-size category (selected 400 pm family vs legacy vs zero-swell, rebuilt from production geometry; --fail-swell-size control), and a Front-flex category (ten public clips must each move the Front root by >= 20 mm normal-vs-mute; --fail-front-flex control). Every control must fire only its own attribution bit.

## Questions
1. Terminal cap: does the check prove ONLY ring 63 differs and every other ring is unchanged? Is burial checked on every shipping key AND midpoint?
2. Swell size: is the comparison against independently rebuilt geometry (not the same code path compared with itself — a detector wired to two operands that move together cannot fire)?
3. Front flex: is the 20 mm floor measured on visible vertices, normal vs mute, same binary? Could a clip pass with no Front motion?
4. Attribution: can any control fire through a different category's bit?
5. P1/P2 only, else "none found".

## Inputs
show:50803207:tools/reel/manafold_spangate.cpp
