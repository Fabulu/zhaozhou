# T4 — a final must not be authorised by a reservation

*2026-09-07. `zhao_texture_v3own.sv`. The handoff says T4 "can land earlier once
isolated", and it turned out to be unblocked much earlier than I thought — see
the last section, which is the more useful finding.*

---

## The defect, in the design's own terms

§11.1 names three events that the RTL conflated:

1. local candidate reservation — the owner is claimed for eventual admission;
2. central bank-read reservation — storage reserved for a packet arriving later;
3. **actual COMBINE acceptance** — the packet transfers on `valid && ready`.

> Only the third sets `combine_issued` and authorizes a final terminal. **The old
> `cbi` bit was set at the second event.**

Traced in the source before changing anything:

| line | what it was |
|---|---|
| `cmb_pop_c = sel_v_c && ((cmb_res_q − cmb_fire_c) < CMBQD)` | the credited reservation, **event 2** |
| `if (cmb_pop_c && …) cbi_n_c[i] = 1'b1;` | `cbi` set at event 2 |
| `cmb_fire_c = cmb_valid_o && cmb_ready_i` | the real acceptance, **event 3** — *unused for `cbi`* |
| `c2f_acc_c = c1f_v_q && c2f_idok_c && c1f_cbi_q` | so a **final** was authorised by event 2 |

A final result could therefore be accepted, and its payload written, for an owner
whose COMBINE input **had never been taken**.

## The change

`cbi_q` now means event 3 and nothing else. A new `crs_q` (combine_reserved)
inherits event 2, because T4 says *"keep reservation separate"*:

```systemverilog
// ---- COMBINE RESERVATION (11.1 event 2) ----
if (cmb_pop_c && (sel_data_c[OWNERW-1 -: SLOTW] == SLOTW'(i)))
  crs_n_c[i] = 1'b1;

// ---- ACTUAL COMBINE ISSUE (11.1 event 3) ----
if (cmb_fire_c && (cmb_owner_o[OWNERW-1 -: SLOTW] == SLOTW'(i))
    && (gen_q[i] == cmb_owner_o[GENW-1:0]))
  cbi_n_c[i] = 1'b1;
```

Three things were checked rather than assumed:

* **`cq_rp_q` advances on `cmb_fire_c`**, so each fire corresponds to exactly one
  head entry and the marked owner is the one that transferred.
* **The generation is checked**, as every other event in that loop does, so a
  stale token naming a reused slot cannot mark the current owner issued.
* **`crs_q` has exactly the old `cbi_q` lifetime** — set at pop, cleared at
  admission — so the two eligibility guards that used to read `cbi_q`
  (`!rdy_q[s] && !crs_q[s]`, the READY-ticket claims) and the `a_combine_admit`
  assertion behave **identically to before**. That is what makes T4's
  "preserving all other finals and stalls" true by construction for them rather
  than by hope.

`cbi_q` keeps its one remaining reader, `c1f_cbi_q <= cbi_q[c0f_slot_q]`, which
feeds `c2f_acc_c` — the final authorisation. That is the whole point.

## Cost, stated rather than hidden

**+64 flip-flops.** Two per-owner facts now exist where one did, and both are
owner-lifetime, so both need 64 bits. That runs against T3's register-reduction
goal and it should be counted there, not quietly.

**It is removable later, and here is the arithmetic.** `cmb_pop_c` requires
`cmb_res_q < CMBQD` with `CMBQD = 4`, so **at most four owners are
reserved-but-not-issued at any instant**. Once issued, `cbi_q` covers the guard;
the only gap is the pop→fire window. So `crs_q` could be replaced by a ≤4-way
comparison against the in-flight owner tokens the design already holds
(`k0/k1/k2_owner_q` and the `cq_own_q` entries).

**Not done here.** That puts a comparator on the READY-ticket eligibility path,
and this session has already made three timing predictions from reading source
that the fitter falsified. It belongs to T3's bitplane work, measured.

## What is verified

* Verilator `-Wall` lint clean on the three-file closure.
* Owner adversarial bench: default lane and the `ZHAO_M6` lane.

## The finding that cost the most, and it was mine

I deferred T4 for hours believing `zhao_texture_v3own.sv` could not be edited
while its fit ran — the live-tree trap, `QUARTUS_GOTCHAS` §11: *"a running fit
reads the working tree."*

**That has not been true since 2026-09-03.** `run_block_fit.ps1` snapshots every
declared source into its workspace and points the QSF at the copy. The script
says so in its own comment — *"an ordinary edit to the live tree now cannot
affect this run at all"* — and every run prints it: *"snapshot: N source(s)
copied into the workspace; the live tree cannot reach this fit."* I found it in a
fit log, not in the rule.

§11 still opened with *"Nothing is copied into the workspace"*, so the rule
forbade something that had been safe for four days. It is now corrected with a
superseding box, including what is **not** superseded: §13's warning that
`design/fit_targets.yml` **is** still read live, once per block at preflight.
Config and sources now have different rules, which is exactly the split that
gets misremembered as one — and I edited that YAML twice today with a truncating
write while a fit was running, which was safe only because this is a
single-block run long past preflight.

**A stale prohibition is not free caution.** It silently removes work from every
session that obeys it, and it is invisible precisely because obeying it looks
like diligence.
