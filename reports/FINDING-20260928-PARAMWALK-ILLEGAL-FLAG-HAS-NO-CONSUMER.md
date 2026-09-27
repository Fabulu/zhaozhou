# `t_illegal_o` travels with the record to say so — and nothing in the console listens

Coordinator, 2026-09-28. Found by fixing `cmake --preset`, which let the lint
suite run, which surfaced 27 real warnings in `zhao_console_core` that 209
generated-package `UNUSEDPARAM`s had been drowning. This is one of them.

## The finding

`zhao_geom_paramwalk` deliberately emits a malformed triangle rather than
dropping it, and flags it. From its own comment at `:1146`:

> **A MALFORMED DESCRIPTOR IS NOT FOLLOWED.** `td_illegal_c` means an id is past
> the frame's sealed vertex count, so the slot it names is outside what this
> frame wrote — reading it would put a guard request on an address the arena
> never published and decode whatever last occupied it. The triangle is still
> **EMITTED AND FLAGGED**, which is R7's "rejected and counted" … what is
> skipped is the fetch, and **the vertex registers keep the previous triangle's
> values** rather than being filled with a convenient zero. `t_illegal_o`
> travels with the record to say so.

The contract is therefore: **paramwalk flags, a downstream consumer rejects.**

**THE CONSUMER DOES NOT EXIST.** `zhao_console_core.sv:32784` connects
`.t_illegal_o (pw_t_illegal_w)`, and `pw_t_illegal_w` is read **nowhere** —
which is why Verilator reports it as `UNUSEDSIGNAL`. `t_valid_o` is
`(wstate_q == W_TD_EMIT)` and is **not** gated by the flag, so the record is
offered as valid like any other.

## What that means

A descriptor naming a vertex id past the sealed count would be **accepted by the
door and rasterized, carrying the PREVIOUS triangle's vertex values** — the
comment says so explicitly, and it says so because the author expected somebody
downstream to refuse it. Nothing does.

**The counter is not the missing consumer.** `tris_illegal_o` aggregates the
same event and IS observed — the smoke prints `paramwalk … illegal=0` — so this
is visible in the bench. But a counter is evidence after the fact; it does not
stop the triangle. R7's "rejected and counted" has its *counted* half wired and
its *rejected* half unwired.

**This matters more since I55 than before it.** The walk is now the SHIPPING
path — the console draws its pixels from SDRAM through exactly this record — so
an unenforced malformed-descriptor guard sits on the live rasterisation path
rather than on a retained oracle.

## Severity, stated honestly

**Not reachable on correct content.** `td_illegal_c` requires a descriptor whose
vertex id exceeds the frame's sealed count, which a well-formed frame never
produces; `illegal=0` in every smoke form measured. So this is a **defence that
is declared and not connected**, not an active corruption.

It is exactly the shape this repository catalogues, though: a guard whose
silence is read as safety while its enforcement path was never laid. The counter
reading zero is what makes it comfortable to leave alone.

## The repair, and why it is not made here

Gating the door's acceptance on `pw_t_illegal_w` — or refusing at
`zhao_geom_tilewalk`'s take — is a change to the console's live job path, and
the honest version needs to decide what "reject" means: drop the triangle
silently, drop and count, or fault the frame. R7 says "rejected and counted",
which suggests dropping with the existing counter, but the choice belongs with
the ruling rather than with a tail-end edit.

**It also needs a test that can FAIL**, which means stimulus containing a
descriptor past the sealed vertex count — something no current bench produces,
since every form reads `illegal=0`. That is a packet: build the malformed
stimulus, watch the counter move, then wire the refusal and watch the pixel not
appear.

## Related, from the same 27

* **2 × `SIMILARNAME` — CHECKED, and it is a real collision.** I wrote above
  that this was "a genuine hazard rather than a style note, and cheap to
  check". Checked. `zhao_console_core.sv` declares **both** of these, 78 lines
  apart, and **both are live**:

  ```systemverilog
  :33725  localparam logic [23:0] MAT_BASE_RGB_C = 24'hFF_FF_FF;  // white
  :33803  wire       [23:0] mat_base_rgb_c = zhao_ms_base_rgb(st_mat_token, st_domain);
  ```

  They differ only in case and they mean **opposite things**: the constant is
  the unmodulated white PLACEHOLDER (still read at `:33789-33790`), and the
  wire is the per-triangle value that **superseded** it. A case typo picks the
  wrong one and compiles silently — the most confusable possible pairing.

  The clash has a nameable cause: this tree uses `_c` for **combinational**, so
  a `localparam` carrying `_C` breaks that convention *and* lands on the wire's
  name. Renaming the **constant** (e.g. `MAT_BASE_RGB_WHITE`) fixes the
  collision and the convention together — three code sites, though several
  comments reference the old name and want the same pass.

  **Not renamed here.** It is production RTL on the material path, a rename
  wants the console smoke re-run behind it, and it is latent: no wrong value is
  produced today.
* **25 × `UNUSEDSIGNAL`** in total, including `pw_t_v0_w` and `pw_t_v1_w`
  alongside the flag above. Those two are plausibly benign — the walk arm takes
  its vertices through GEOM.FETCHARM rather than from these ports — but they
  have not been checked, and "plausibly benign" is how this one looked too.
