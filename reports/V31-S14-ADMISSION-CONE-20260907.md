# §14.1's admission rule, verified structurally rather than claimed

*2026-09-07. Analysis only. This closes a deliverable T1 asserted and the fit
corroborated, by checking the actual cone instead of inferring it from a timing
result.*

---

## The rule

> Ordinary admission depends on the **registered** run/admission-enable state and
> available owner capacity. **It must not combinationally depend on a sum of all
> queue pointer differences.**
>
> Detect approaching namespace reuse from **A**. At the last allocation before
> the full 14-bit namespace rolls to the next reused value, arm the barrier. **Do
> not wait for an unrelated downstream pin to propagate through quiet in the same
> cycle that the next request is accepted.**

## The cone, as it now stands

```systemverilog
assign adm_ready_o  = credit_ok_q && fence_open_q;
assign adm_fire_c   = adm_valid_i && adm_ready_o;
assign adm_accept_o = adm_fire_c;
```

**Two registers and nothing else.** `credit_ok_q` is registered from
`live_next_c`; `fence_open_q` is the fence's registered permission.

`quiet_c` — the sum of pointer differences the rule forbids — appears in exactly
two places:

* `FN_FINISH: if (quiet_c) fn_n_c = FN_REOPEN;` — the fence's **next-state**
  logic, feeding `fence_open_q`'s D input. One register away from admission.
* `assign ev_quiet_o = quiet_c;` — an observability output.

So quiet reaches admission only **through** a register. That is the rule
satisfied, not approximated.

## And the barrier is armed from A, as §14.1 specifies

After T2's Group A, the wrap tests read `sh_alloc_gen_q` — the allocation
ticket's own generation, which *is* **A** — rather than indexing the generation
table:

```systemverilog
assign wrap_block_c      = (sh_alloc_gen_q == 8'h00);
assign wrap_at_tail_p1_c = (tail_q == 63) ? (sh_alloc_gen_q == 8'hFF)
                                          : (sh_alloc_gen_q == 8'h00);
```

§14.1 says *"detect approaching namespace reuse from A"*, and that is now
literally what happens.

## Why this is worth writing down separately

The fit already corroborated it: the ten worst paths that used to end at
`adm_accept_o` / `adm_ready_o` via `wp_q → occ_o → rq_occ_c == 0 → quiet_c` are
gone, and reported Fmax rose from 75.79 to 87.37. **But a timing result is
evidence about a path, not about a cone.** A future edit could put a pointer sum
back into admission and still fit, because the fitter reports the worst path it
finds rather than the rule that was broken.

The structural check is the one that stays true, and it is cheap: `adm_ready_o`'s
right-hand side must contain only `_q` terms. Worth a lint rule eventually;
recorded as a property today.

## What this does not cover

§14.2's local drain condition lists nine requirements for the controller's
"done", and this report speaks to none of them. `quiet_c` covers live owners,
unfetched/fetched reservations, queue occupancy and output reservations — but
§14.2 also names context/packet writes, terminal records, ready candidates,
COMBINE reservations and captured returns from RCP/PERSPUV/sample/palette/AUX.
Several of those live outside this block entirely, so the audit belongs with
§10's integration work rather than here.
