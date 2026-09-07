# §10 L0 — the island top's array and port inventory

*2026-09-07. The first step of the master recovery handoff's controlled
top-storage series (§10.5: "L0 read archived/current array and port
inventory"). Analysis only; no RTL touched, no worktree needed.*

---

## Why the inventory has to be per ACCESS EVENT, not per site

§10.1 sets the bar and it is not a formality:

> They identify real conversion targets, **not a guaranteed 9,568-ALM saving**.
> Different access patterns need different port schedules. **Two independent
> reads PLUS a concurrent write are three accesses, not a free use of two-port
> RAM.** Pack fields only when their writer and read event genuinely coincide.

An earlier pass in this session counted textual sites per array, which cannot
tell a concurrent read from an alternative branch. This one records where each
access physically sits.

## The result

| array | width | writes | reads | write site | read site |
|---|---|---:|---:|---|---|
| `uvw_m` | 64 | 1 | 1 | `always_ff@708` | continuous |
| `fctx_m` | `CTXW`=64 | 1 | 1 | `always_ff@708` | continuous |
| `flod_m` | `LODW`=8 | 1 | 1 | `always_ff@708` | continuous |
| `fpgn_m` | `GENW`=8 | 1 | 1 | `always_ff@708` | continuous |
| `fcls_m` | 2 | 1 | 1 | `always_ff@708` | continuous |
| `fpsl_m` | `PSW` | 1 | 1 | `always_ff@708` | continuous |
| `faux_m` | 1 | 1 | 1 | `always_ff@708` | continuous |
| `class_m` | 2 | 1 | 1 | `always_ff@939` | continuous |

**Every one is a single-writer, single-reader structure.** §10.1's warning about
three concurrent accesses does **not** bite for these eight: one registered
write, one read, no second reader anywhere.

Seven of the eight share one write event — `always_ff@708`, guarded by
`frag_valid_i && frag_ready_o`, the ingress capture. They are eight parallel
fields of **one record written on one event**, which is what makes §10.4's
consolidation into `SAMPLE_DESC` / `OWNER_CONTEXT` rows structurally available
rather than merely desirable.

## What the reads actually are

```systemverilog
wire [63:0]      uvw_rd   = uvw_m[rcp_tok[FCTXW-1:0]];   // line 726
wire [CTXW-1:0]  fctx_rd  = fctx_m[fc_rp];               // line 750
```

**Continuous assigns — asynchronous array reads.** That is exactly why Quartus
reports all eight as *"uninferred due to asynchronous read logic"*, and why the
9,568 declared bits sit in fabric instead of an M10K.

So the port schedule is clean and the obstacle is purely the read's timing: each
needs a synchronous read, which costs a cycle at the read point. That cycle is
what §10.2 and §10.3's **credited joins** exist to absorb, and §10.2 is explicit
that registering the numerators alone is wrong — *"Registering only the
numerators pairs yesterday's U/V with today's mantissa/token"*.

## What L0 does NOT license

* **Not a 9,568-bit saving.** §10.1 says so directly, and the arrays are 32 to
  4,096 bits each — several are far below an M10K's 10,240, so converting them
  individually would spend a whole block on 32 bits. §10.4's consolidation into
  shared rows is the point, not eight separate memories.
* **Not that the conversion is free.** One cycle enters the read path per array,
  and §10.2's five-stage join (N0–N4) is called "a conservative design, not a
  measured latency minimum".
* **Not L1.** The numerator join is the next step and needs the separate
  worktree and top variant §10.5 requires, with one integration owner — this
  session holds the control lane, so the storage lane's edits are not started
  here.

## The instrument, and the trap it walked into

`tools/quartus/island_l0_inventory.py` is committed rather than left in a
scratch directory, under the same rule as the other probes.

Its first version reported every read as *"in `always_ff@708`"* — because it
tracked where blocks **begin** and never where they end, so a `wire` assign
following an always block was filed inside it. That reading would have said the
reads were already registered, which is the opposite of the finding.

Its second version still reported the same thing, and the reason is verbatim a
trap CLAUDE.md records:

> one self-check was written with a word-boundary escape that a shell heredoc
> turned into a **literal backspace character** — so it matched nothing and
> printed reassurance for its whole life.

The regex was `\s*(wire|assign)` followed by a `\b` that the heredoc had turned
into byte 0x08. It matched nothing, and the script kept printing the *reassuring*
answer. Found by `cat -A`. The rule stands: write scripts to a file, never
through a heredoc, when they contain escapes.
