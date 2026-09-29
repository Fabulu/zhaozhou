# `TERRAIN.GROUP_SEQ` and `GEOM.GROUP_SEQ` are composed hardware with no block id

Coordinator, 2026-09-29. Found chasing three `downstream` pattern errors in
`ledger:check`, and it is **not** the typo it looks like.

---

## What the ledger says, and why it looked like a typo

Three `downstream` lists name ids the schema rejects, because the pattern
`^([A-Z][A-Z0-9]*)(\.[A-Z][A-Z0-9]*)+$` has no room for an underscore:

| block | downstream entry |
|---|---|
| `TERRAIN.TESS` | `[TERRAIN.NORMALS, FORGE.CLIFF, TERRAIN.GROUP_SEQ]` |
| `TERRAIN.JOBISSUE` | `[TERRAIN.GROUP_SEQ]` |
| `GEOM.WARP` | `[GEOM.GROUP_SEQ, GEOM.LIGHT]` |

No block carries either id — `grep '^  - id: .*GROUP'` returns nothing — and the ledger
does have `TERRAIN.SEQ` and six `FIELD.SEQ.*`. So the comfortable reading is a typo for
`TERRAIN.SEQ`, and for GEOM a stale name. **Both readings are wrong.**

## What the RTL says

Tracing `u_terrain_tess`'s **49 output nets** through `zhao_console_core.sv`, its consumers
are:

```
u_terrain_group_seq     15 nets   (tt_job_ready, tt_job_reject, …)
u_terrain_heighttap      7 nets   (tt_cs_ci, tt_cs_cj, …)
u_terrain_compcache      3 nets   (tt_mat_ci, tt_mat_cj, …)
```

The largest consumer is an instance of **`zhao_terrain_group_seq`** —
`fpga/rtl/terrain/zhao_terrain_group_seq.sv`, **34,653 bytes**. `zhao_geom_group_seq` is
the same story: `fpga/rtl/geometry/zhao_geom_group_seq.sv`, **28,605 bytes**, instantiated
at `zhao_console_core.sv:19274`.

**Both are in the console fit's own 299-file source digest** (`zhao_geom_group_seq.sv` at
line 84, `zhao_terrain_group_seq.sv` at line 213 of
`reports/synthesis/blockpaths/zhao_console_core.sources.sha256`). They are shipping
hardware in the fit that is running right now.

And the id the ledger uses is *exactly* the one the project's own naming rule would
produce. `tools/design/check_counters.py:module_for_block` maps a block id to
`zhao_` + id.lower() with dots as underscores:

```
TERRAIN.GROUP_SEQ  ->  zhao_terrain_group_seq     the module that exists
GEOM.GROUP_SEQ     ->  zhao_geom_group_seq        the module that exists
```

So whoever wrote those `downstream` entries was naming real blocks by the correct
convention. **The rows were simply never created.**

## What this is, precisely

Two modules totalling ~63 KB of RTL are composed into `zhao_console_core`, are in the fit
closure, and have:

* **no `design/blocks.yml` row** — no contract path, no maturity, no tests, no counters
  declared;
* **no contract** — `design/contracts/` has `TERRAIN.SEQ.md` and six `FIELD.SEQ.*.md`, and
  nothing matching either GROUP_SEQ;
* for the terrain one, a **`prod_manifest.yml` row that is stale in the direction that
  matters**: `not-yet-adopted`, "used only inside the not-yet-adopted zhao_terrain_pipe",
  while `zhao_console_core` instantiates it directly.

## Why no gate caught it

`completion_register.py` reports **ZERO mandatory gaps**, and that is not wrong — it checks
every capability the ledger DECLARES against what the console instantiates. It is blind by
construction to the reverse: **a module in the machine that nothing declares.** Nothing was
left out of the composition; something was left out of the ledger, and a register keyed on
declarations cannot find a declaration that was never made.

The only instrument that noticed was the ledger's own `downstream` pattern, and it noticed
for an incidental reason — the underscore — not because it checks referential integrity.
**An id in `downstream` that matches no block is not currently an error at all**, which is
the checkable gap here: had someone written `TERRAIN.GROUPSEQ`, the pattern would have
passed and nothing would have complained.

## What I did NOT do, and why

I did not fix the three `downstream` entries. Two options exist and both are wrong to take
unilaterally:

* **create the two block rows** — needs a contract, a maturity with evidence, tests and
  counters for each, which is authoring two block specifications for hardware someone else
  designed;
* **repoint the edges** at `TERRAIN.SEQ` / something in GEOM — writes a **wrong edge into
  the dependency graph** that tools then read as fact, and the RTL says plainly that the
  existing edges are right.

**And I deleted a tool I had written to generalise this.** It asked "which composed modules
does no ledger block id resolve to" and answered 188 of 279 — a number that looks alarming
and means nothing, because the ledger claims **capabilities**, not modules: one row's
implementation is routinely a subtree of a dozen files, so `zhao_field_v3_exec`,
`zhao_field_host_v2` and `zhao_cmd_exec` all read as "unowned". Its first version was worse
still: it grepped for the module NAME in `blocks.yml`, which never appears there at all.

Both versions were plausible, both produced a big confident number, and neither was checked
against a case verifiable by hand before the total was believed — the exact rule in
CLAUDE.md. A tool whose headline invites a wrong conclusion is worse than no tool, so it is
gone rather than committed with a caveat nobody would read. The narrow finding above needed
none of it: it came from tracing 49 nets in the composed top and reading two `ls` outputs.

If anyone does want the general question answered, the machinery already exists —
`completion_register.py` computes block closures, and the honest check is "is every composed
module inside some declared capability's closure", which belongs in that file rather than in
a second implementation beside it.

## The three things that would close this

1. An owner or architect decision on whether these two get block rows, or an explicit
   exemption recorded where an exemption belongs.
2. `zhao_terrain_group_seq`'s `prod_manifest.yml` note corrected — it describes a module
   used only inside an unadopted pipe, and the console instantiates it.
3. A referential-integrity check on `downstream` / `upstream`: **every id must be a block
   id.** That is a rule the ledger does not have, and it is the one that would have caught
   this without depending on an underscore.
