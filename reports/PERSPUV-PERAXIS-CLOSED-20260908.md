# The perspuv per-axis array split — CLOSED, its motivation is gone

A queued roadmap item, checked before being worked, and it should not be worked.

## What it was for

`G1D-COMPOSED-ISLAND-20260905.md`:757-762 records the motivation:

> `e_tag` — single read — inferred, while `e_num_u` and `e_q_u` did not, **even
> after the per-axis split** gave each one write and one read ADDRESS but left
> `e_q`'s asynchronous read feeding a port directly.

So the split itself was ALREADY DONE. What remained queued was the follow-on:
those arrays still refusing to infer as RAM, by the same asynchronous-read
mechanism `uvw_m` had.

## What the current fit says

Every uninferred RAM in `zhao_texture_island_top@p0c-stageA`, by reason:

**"asynchronous read logic" — three, and none is perspuv's:**

| array | disposition |
|---|---|
| `fragrob:u_fragrob|tok_m` | fragrob is DELETED by P0-C; resolves itself |
| `rcp24_svc|...rcp24_rom:u_rom|Ram0` | by design — a pure `always_comb` §6.2 lookup |
| `class_m` (top) | 2 b × 16 = 32 flops, negligible |

**"inappropriate RAM SIZE" — eleven, all correct refusals**, including
`perspuv_svc|e_dz`, which is NTOK 16 × 1 bit = **16 bits**. Refusing an M10K for
sixteen bits is the fitter being right.

**`e_num_u`, `e_num_v` and `e_q_u` appear in NEITHER list.** They are not refused
and they are not flagged. The condition that motivated the queued work is not
present in the current design.

## And `uvw_m` is gone from the list entirely

The same query confirms Stage A from the map side rather than from the register
count: `uvw_m` was `uninferred due to asynchronous read logic` before, and now
appears as `altsyncram:uvw_m_rtl_0`. Two independent confirmations of the same
change — the register delta (−4,092) and the inference itself.

## Conclusion

**Close the item.** The split happened; the follow-on condition it left behind is
no longer observable; and after Stage A the island has NO remaining
asynchronous-read array worth converting — one is deleted by P0-C, one is
correct by design, one is 32 flops.

That also closes P0-E as a line of work rather than as a task: its census
narrowed four candidates to one, that one was fixed, and this check confirms
nothing was left behind it.

**What would reopen it:** a future fit reporting `e_num_*` or `e_q_*` under Info
276007, or a worst-path family launching or capturing in them. Neither is true of
`@p0c-stageA`, whose worst 40 paths contain no `e_num` at all.
