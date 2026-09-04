# Direction 30 sun table: before (Direction 29) -> after (calmed v2)

Values are fxm thousandths (Q16.16 of 1.0 gain / of the 255 add scale).
Positions, radii (50 m up, 22 m lateral, 65 m inner) and moods unchanged.

| clip | mood | D29 mult R/G/B | D29 add R/G/B | D30 mult R/G/B | D30 add R/G/B |
| --- | --- | --- | --- | --- | --- |
| Idle | golden morning | 775/500/60 | 620/330/30 | 240/70/12 | 150/65/5 |
| Walk | azure day | 60/280/900 | 45/150/540 | 18/80/280 | 10/35/135 |
| Run | hot orange | 900/340/30 | 600/195/15 | 280/90/10 | 150/42/3 |
| Look | violet | 525/90/840 | 360/45/560 | 160/25/260 | 85/10/140 |
| Balance | teal | 50/680/650 | 30/390/450 | 18/250/240 | 8/115/140 |
| Taunt | magenta | 800/60/680 | 560/25/380 | 250/18/210 | 140/6/95 |
| SlowTaunt | rose dusk | 745/215/340 | 450/120/210 | 230/60/105 | 110/27/50 |
| JumpOne | spring lime | 340/840/60 | 210/500/45 | 105/260/18 | 48/125/9 |
| JumpMulti | sunset red-orange | 930/215/25 | 620/150/15 | 290/60/8 | 155/33/3 |
| Attack | crimson | 960/60/40 | 660/30/30 | 300/18/12 | 160/6/6 |
| Hit | steel blue | 90/340/800 | 60/195/510 | 28/100/250 | 12/45/125 |
| Damage | ember amber | 840/400/40 | 540/240/15 | 260/115/12 | 130/55/3 |
| Knockdown | bruise violet | 430/75/775 | 270/45/510 | 130/20/240 | 63/9/125 |
| Fall | ice blue | 185/495/870 | 120/300/560 | 55/150/270 | 27/70/135 |
| HitFloor | dust orange | 800/430/75 | 510/255/30 | 250/125/20 | 125/57/6 |
| SaltoDummy | chartreuse gold | 650/710/50 | 390/420/30 | 200/220/15 | 90/100/6 |
| SaltoFly | deep azure | 40/215/930 | 30/120/630 | 12/63/290 | 6/27/155 |
| SaltoSix | gold | 840/620/60 | 540/360/30 | 260/185/18 | 135/78/6 |
| SaltoNine | hot pink | 870/90/465 | 600/45/300 | 270/27/140 | 145/11/72 |
| Death | deep red | 900/40/25 | 600/15/15 | 280/12/8 | 150/4/4 |
| Death2 | moonlight | 340/370/840 | 210/225/540 | 105/115/260 | 48/51/130 |

Dominant adds sit at ~24% of Direction 29 (v1 at 40% still read too hot);
mults at ~30% with secondaries suppressed harder than proportionally, because
the green-rich pigment amplifies any mult G and dilutes warm hues toward
white (measured: idle authored add R:G 1.88 arrived on screen as 1.1 under
the old mults). Balance deliberately trimmed less -- the subtlest clip.
