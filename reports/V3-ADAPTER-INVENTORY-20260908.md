# Adapter inventory: every site an identity is taken apart

Owner brief §4.3. Its instruction is specific and comes from a real cost:

> *"The inventory must include internal cache `c1_src`/`c2_src` registers, not
> only ports ... 'Six sites, not four' was the actual cache-width lesson."*

That is what happened. Threading `SRCW` through `zhao_texture_cache_pipe`, I
found four sites, moved on, and had to come back for two more. A port list is
not an inventory.

## `zhao_texture_cache_pipe` — the six

| # | site | kind |
|---|---|---|
| 1 | `acc_src_id_i` :122 | port |
| 2 | `smp_src_id_o` :128 | port |
| 3 | `rq_src [REQN]` :226 | internal array |
| 4 | `c1_src` :252 | **internal register** |
| 5 | `c2_src` :270 | **internal register** |
| 6 | `rs_src [REQN]` :325 | internal array |

Two visible from outside, four not. A width change that stops at the ports
elaborates cleanly and truncates in the middle of the pipe.

The file's own comment at :96-98 now records the tightness: with today's SRCW
the slack is `SRCW-2-$clog2(DEPTH)-2-GENW = 0`, so **a 6-bit slot has no room
left**. That is worth reading before anyone widens anything again.

## Raw slicing that the §4.1 package should replace

`zhao_texture_frag_expand` builds identities by hand in two places:

```
:197  req_src_id_o     = {cur_q.cls, cur_q.owner[13:8], sidx_q, cur_q.owner[7:0]}
:208  iss_tmu_handle_o = {cur_q.owner[13:8], sidx_q, cur_q.owner[7:0]}
```

Both are exactly `make_token(cls, make_sample_handle(slot, sidx, gen))` and
`make_sample_handle(...)` from `zhao_texture_ident_pkg`. They are correct today
and they are written in the notation that has been wrong five times.

**Not changed here.** The expander is inside the running Stage C fit's 18-file
closure, and the brief says not to change the source under that run. Adoption is
a follow-on, and the bit-identical test (`ident_encoding_directed`, 262,144
combinations) exists so that adoption can be shown to move no bit.

## The remaining boundaries, for completeness

The brief lists what the inventory must cover. Status of each:

| boundary | carries | state |
|---|---|---|
| cache `c1_src`/`c2_src` | route token | **six sites, threaded** |
| AUX sheet **request** | owner handle | fixed — the token is the handle, whole |
| AUX sheet **response** | owner handle | fixed — was a 4-bit slice of a 6-bit slot |
| RCP token storage | owner | via `uvw_m[fc_wp]`; keyed by queue slot |
| PERSPUV tag | fragment | pipelined with its fragment |
| expander queue + current record | owner, ctx | ctx added; **raw slices remain** |
| class queues | route token | class slice fixed to `[TOKW-1 -: 2]` |
| material alignment identity | owner | one alignment term, both handshake halves |
| COMBINE tag | owner | `comb_o_tag[13:0]` |
| FINAL | owner | `own_fin_owner_c` |
| external caller context | **caller tag** | fixed — was carrying the owner handle |

Five of these eleven were defects found during this integration. That ratio is
the argument for the inventory existing as a file rather than as care.

## What an inventory cannot do

The brief closes §4.3 with the caution that matters most here:

> *"A declaration-presence checker is useful, but it cannot establish record
> identity, stage alignment, or meaning. Keep the data-dependent tests."*

This repository proved that twice in one day: `undriven_outputs.py` passes the
V3 top while three of its fault ports are constant zero, and every one of the
five stale slices was a legal declaration. **The list tells you where to look.
The differential tests tell you whether it is right.**
