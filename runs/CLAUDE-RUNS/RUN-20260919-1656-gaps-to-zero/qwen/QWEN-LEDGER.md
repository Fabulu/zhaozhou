# Qwen ledger

One row per job. Verdict is the coordinator's check against source.

| Job | Kind | Title | Prompt tok (est) | Reasoning/answer tok | Seconds | Status | Verdict |
|---|---|---|---:|---:|---:|---|---|
| Q001 | task | memguard-r32-arm | 13642 | 16994/19255 | 225 | ok | verified: spot 3/3: 33-bit res_end33 guards wrap (guard.sv:289); check and latch are atomic, fwd_req latched at accept so later res_* changes cannot re-bound an accepted burst (guard.sv:383-390); pool constants 0x06A0_0000/0x0160_0000 match zhao_pkg.sv:156-157 (its open item, now closed). No findings, and none missed as far as the spot checks reach. |
| Q002 | task | geom2-vdecode-deadlock | 23680 | 39407/41775 | 666 | ok | partial: spot 4: F1 CONFIRMED as a conditional hazard (group_seq:366 charges a hole only in StAlloc/StFill, and the header's one-meshlet-in-flight assumption is stated at :104-107, not enforced here; forwarded to geom2 to prove or enforce). F2 is an INPUT limitation, not a defect: the mutant is the pre-existing seal-early mutant (drain_done_c -> 1'b1), unchanged in substance. F3/F4 are accurate but LOW, and F4 is by design (poison travels with the count). Its continuation correctly noted seal_early_o would not see a refusal hang. No false claims found. |
