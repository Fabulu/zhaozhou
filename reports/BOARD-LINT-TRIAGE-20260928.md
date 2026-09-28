# `lint_zhao_console_board`: all 27 real warnings, classified

Coordinator, 2026-09-28. The gate printed **236** diagnostics; 209 were
`UNUSEDPARAM` in one generated package and are now waived **at the generator**
(so a regeneration cannot revert the pragma). That left **27 real warnings in
production RTL**, which nobody could see while the gate looked broken.

This file is the classification. **Every one of the 27 is accounted for** — none
is left as "probably benign", because that phrase is what `pw_t_illegal_w` looked
like.

Count today: **23**. Four were fixed (see *Removed*).

---

## Defects — 2

| signal | file | what |
|---|---|---|
| `omap_src_c[15:6]` | `zhao_field_host_v2.sv:1064` | **Truncate-then-check.** ABI declares `source_index` as **u16**; RTL stores `[IDXW-1:0]` (6 bits) and the spec's out-of-window refusal then tests the *stored* value. `source_index = 68` → stored `4` → validates in-window. `FINDING-…-TRUNCATE-THEN-CHECK-IN-THE-FIELD-HOST.md` |
| `pw_t_illegal_w` | `zhao_console_core.sv` | **A declared refusal with no consumer.** GEOM.PARAMWALK flags a malformed descriptor by design; `t_valid_o` is not gated by it and nothing reads it, so the record is offered as valid carrying the PREVIOUS triangle's vertices. On the shipping path since I55. `FINDING-…-PARAMWALK-ILLEGAL-FLAG-HAS-NO-CONSUMER.md` |

Neither is reachable on correct content, and both need the same packet shape:
**build the malformed stimulus, watch the counter move, then wire the refusal.**

**The gate should stay RED while `pw_t_illegal_w` is unwired.** A gate saying "a
declared refusal has no consumer" is working, not failing.

---

## Removed — 4 (all proven zero-reader, smoke byte-identical)

| signal | why it was a hazard, not untidiness |
|---|---|
| `efa_ans_ready` | Verilator's *"not driven, **nor used**"* — the only one of its kind. Declared beside the LIVE `efa_ans_valid`, so it read as half a wired handshake; the real ready is `tvj_a_ready`. **It nearly attracted a `busy` term during the I34 pass.** |
| `gw_o_p_src_id` | A copy of the properly-waived sink `gw_o_p_src_id_unused`, sharing a declaration line with the live `gw_o_n_src_id`. The waiver covered the sink, not the copy. |
| `tps_v_cell_fire_c` | Outlived its last reader when I34 closed — **and its comment still recommended it** as "the fire the material plane's write port wants", which is exactly the defect the matjoin analysis found by being blocked by it. A loaded gun for the next person wiring a material write. |
| `st_mat_token_live_c` | A hoisted predicate both consumers compute internally (`zhao_ms_base_rgb`, `zhao_ms_weight`). Dead logic that looks like a select somebody forgot to use. |

**The shared shape, worth naming:** three of the four were **dead wires sharing a
declaration line with a live sibling**. The eye reads `wire a, b;` as one live
thing. Verilator is the only reader in the tree that does not.

---

## Benign, and each for a stated reason — 21

**GEOM.PARAMWALK's genuinely unread face (7).** `pw_t_v0_w`, `pw_t_v1_w`,
`pw_t_v2_w`, `pw_t_material_w`, `pw_t_raster_w`, `pw_t_source_w[31:16]`,
`pw_tri_id_wide_w`. The console takes its vertices through the **fetch arm**
(`pw_t_a_*`, all 27 of which ARE read), so the descriptor's vertex *ids* are not
needed; only `t_source_o[15:0]` is, as `pw_ident_w`. Note this is the block whose
comment claimed the whole face had "no consumer" — corrected; the arm is live and
the ids are the part that genuinely is not.

**Width headroom, guarded (3).** `tlf_w_slot[10]`, `tlf_inv_slot[10]`,
`tlf_chk_slot[10]`. `TERR_MEMSLOT = $clog2(TERR_POOL_SLOTS) + 1` — deliberately
one bit wider — consumed at `[TERR_SLOTW-1:0]`, **with a committed elaboration
`$fatal` at `zhao_console_core.sv` guarding exactly that narrowing** ("would
truncate a real handle"). *This is how a width overshoot should look*, and it is
the contrast that makes `omap_src_c` a defect rather than a twin.

**FIELD.EARTH_ADAPTER out-lanes (3).** `efa_nav_cost` — **owner-ruled elsewhere**
(`OWNER-DECISION-20260926-I34-NAV.md`), open by decision. `efa_ans_field` and
`efa_present[3,1:0]` — produced by the four-channel evaluation, with ordinal 2's
`efa_present[2]` the bit that is consumed.

**Redundant-by-construction (1).** `tw_frame_done_w` is a clean one-cycle pulse
(default-low, set only in `S_DONE`) that is high **exactly on the first cycle
`active_o` is low**. The console derives everything it needs from `tw_active_w`,
which it reads in 34 places. Exactly derivable, not dropped.

**Not yet chased (7).** `vid_tri_id`, `fld_resp_window_c`, `fld_resp_count_c`,
`sp_seal_refs_w`, `ms[127:58,25:0]`, and the **2 `SIMILARNAME`**. Each is located
and named; none has been shown benign. The `SIMILARNAME` pair is characterised in
full in the paramwalk finding — `MAT_BASE_RGB_C` (a white placeholder, still
read) against `mat_base_rgb_c` (the per-triangle value that superseded it), 78
lines apart, differing only in case. **Deliberately not renamed:** production RTL
on the material path, three code sites plus comments, and it wants the smoke
behind it.

---

## The argument this settles

The case for clearing 209 generated `UNUSEDPARAM`s was *readability*. The return
is **a defect in a shipped refusal** (`omap_src_c`) that sat in plain text —
*"sixteen bits decoded, six used"* — for as long as the gate printed 236
diagnostics and was read as broken.

A gate reporting 236 is read as broken; a gate reporting 27 is read as a list;
**a gate reporting a classified 23 is read as work.**
