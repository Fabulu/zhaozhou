# FINDINGS -- NOPROG, 2026-09-27

**Branch `gz/noprog`.** Base `a201825d`.
Written against the brief's nine deliverables and the owner ruling
`reports/OWNER-DECISION-20260927-I34-COMPOSED-MATERIAL.md`.

---

## 1. THE NOPROG MECHANISM, MEASURED -- AND IT IS NOT WHAT THE BRIEF SAID

The brief, and `FINDINGS-FIELDACTIVE.md` before it, attributed the composed
console's `noprog=1089` to `zhao_field_earth_adapter`'s documented
intake-versus-replay race, on the fingerprint (1,089 = 33x33) that the adapter's
own header records **verbatim**. That attribution is **wrong**, and the counter
it rests on **could not have said otherwise**.

### 1.1 The counter cannot attribute the refusal

`fld_earth_noprog_o` does **not** count the adapter's own binding flag. It
increments in the adapter's `E_WAIT`, on the **engine's** reply:

```systemverilog
// zhao_field_earth_adapter.sv:1425
if (resp_status_i == StNoProgram) begin
  if (noprog_o != 32'hFFFF_FFFF) noprog_o <= noprog_o + 32'd1;
```

and the engine's `0xF0` has **two** independent causes, ORed into one wire:

```systemverilog
// zhao_field_host_v2.sv:1306
wire gnoprog_c = req_noprog_i[pick_id] || !hdr_loaded[gslot_c];
```

The adapter's own header says so at `:846-849` -- *"a lane whose handle resolved
to no ready object and a lane whose object holds no header BOTH come back 0xF0
and are counted in one place."* So **neither `fldearth noprog` nor `fldhost
noprog` can say which cause fired**, and "the adapter's residency flag is the
only thing that disagrees" was an assumption the instrument structurally cannot
support. That is CLAUDE.md's own law, in the very block the brief sent me to.

### 1.2 What settled it cost no port at all

`terr_fl_replays_o`, `terr_fl_entries_replayed_o` and `terr_fl_open_at_patch_o`
have been **core ports and bench wires since the fieldlist was composed, and
nothing had ever read them**. The brief said *"the adapter exports no evidence
for the replay side"* -- true of the **adapter**, false of the **list beside
it**. Printing them:

```
SMOKE: fldlist records=1 sealed=1 unresolved=0 tail_rejected=0
               replays=1 entries_replayed=1 open_at_patch=0 idle=1
```

* **`open_at_patch=0`** -- **no patch job was ever taken while the list was
  open.** The brief's central premise, *"this console has a job pending FIRST"*,
  is **MEASURED FALSE**. The ordering the intake-versus-replay race requires
  never occurred in this console.
* **`entries_replayed=1`** -- the replay **did** reach the adapter, so its
  `b_res` was written from a resolved entry (`unresolved=0`).
* **`replays=1`** -- no second job replayed over the first one's binding.

**The adapter's binding was correct throughout.** It is not the defect, and the
"ONE NARROWED RESIDUAL" its header declares is not what this console hit.

---

## 2. THE REAL DEFECT: TWO PRODUCTION GUARDS THAT PULL OPPOSITE WAYS

Both are deliberate, both carry their reasons beside them, and **no single
ordering of one staging pass satisfies both.**

| guard | says |
|---|---|
| `zhao_field_doorbell.sv:429`  `head_refuse = head_is_commit && !head_hdr_ok` | a COMMIT for a slot whose HEADER was never written is REFUSED without touching the directory. **Header BEFORE commit.** |
| `zhao_field_host_v2.sv:1448`  `if (pc_cm_valid_i && pc_cm_ready_o && pc_cm_ok_i) hdr_loaded[pc_cm_slot_c] <= 1'b0;` | a successful insert invalidates the slot's program and init proof, because *"the directory has promised the slot to a new hash"*. **Header AFTER commit.** |

The underlying law is `zhao_field_progcache`'s own header: *"Phase B, COMMIT:
only after a miss ... insert into the first free slot, else evict the
least-recently-used entry"*, answering INSERTED **with a slot**. **The commit is
the allocator**, which is why the host invalidates on it.

### 2.1 Both directions MEASURED on the composed console

| staging order | result |
|---|---|
| INSTALL -> LOADs (header last) -> COMMIT *(FIELDACTIVE's)* | commit OK, `hdr_loaded` cleared, every request 0xF0: **`fldearth runs=0 noprog=1089`**, `fldhost runs=0 noprog=1131 grants=1131` |
| INSTALL -> COMMIT -> LOADs (header last) *(my first attempt)* | doorbell law 2 **refuses the commit**, `fld_commit_ok_q` stays 0, the frame gate never opens: **`dma_done=0`**, *"GEOM.REPLAY released no meshlet"* |

The second row is mine and I record it as **a wrong call I made and caught** -- I
read the host's law, did not read the doorbell's, and reordered on half the
evidence. It cost one run, and the contradiction is now measured in both
directions rather than argued in one.

### 2.2 The resolution that removes no guard

Header LAST among the load words (satisfies the doorbell) -> COMMIT allocates
the slot -> **the header load word is RE-POSTED** (satisfies the host). The
doorbell clears its own `hdr_written` shadow on the same insert (`:684`), so the
two flags stay in step and the re-post lifts both. The re-posted word is the
generator's own word 42, not anything the bench composes.

**Nothing is narrowed and no law is relaxed.** That the recovery works is pinned
at the leaf rather than assumed -- see section 3.

### 2.3 THE CONTRADICTION IS NOT FIXED, AND I AM DECLARING IT

Reconciling it belongs in production RTL: either the doorbell's law 2 admits a
commit that **allocates** for a not-yet-loaded slot, or the host stops
invalidating an insert whose hash is the one already in that slot. **Both change
a guard, so both are decisions rather than repairs**, and a bench is the wrong
place to take either.

**Recommendation: the doorbell's law 2 is the one to move.** Its stated purpose
is to stop a commit referencing a slot with no program; an allocation for a slot
**about to be** loaded is exactly the legitimate case it currently forbids, while
the host's invalidation is the guard that protects a genuine eviction.

**Until then, every HPS staging a field program must write the header twice**,
and nothing in the tree said so before this packet. It is now written in the
bench and pinned by `FT029`.

---

## 3. THE INSTRUMENTS ADDED, AND THAT THEY DISCRIMINATE

* **`FT029` in `tests/field/field_host_v2_directed.cpp`** -- the leaf test no
  bench held. **The whole of `tests/field/` loads programs and never commits**,
  which is precisely why no component test could reproduce this -- and it is a
  different reason from the one the brief gave. It asserts the **correct**
  behaviour in both directions: (3) a successful insert un-loads the slot;
  (4) re-writing the HEADER ALONE restores it, with the microcode, output map
  and association shown to have survived. A test asserting only (3) would pass
  while the ordering remained unusable. It asserts the allocated slot is 0
  rather than assuming it.
* **The three unread fieldlist counters**, now printed (section 1.2).
* **`fld_commit_slot_q`** -- the slot the directory actually allocated, latched
  from the return and asserted against `SFF_SLOT`. **Its reset value is 7, not
  0**, so "never latched" cannot read as a pass.
* **`fld_commit_inserted_q`** -- asserts the commit INSERTED, not merely that it
  was answered `ok`. A law-2 refusal is also answered, so the old assertion
  could not tell an insert from a refusal.
* **The frame gate now waits on `fld_db_load_words_o >= SFF_N_LOAD + 1`** -- the
  HOST's count of load words CONSUMED, not posts the mailbox ACCEPTED. This is
  the same ACCEPTED-IS-NOT-COMPLETED defect FIELDACTIVE recorded one door along.

---

## 4. FALSE CLAIMS FOUND IN WHAT I WAS HANDED

1. **"`I34` classifies on a regex for the word BOUNDARY, so editing it moves the
   number."** -- **FALSE**, and it is in both my brief and the OWNER DECISION
   document. `completion_register.py` sets
   `t["mandatory_gap"] = t["kind"] != "resolved-in-composer"`. The `BOUNDARY`
   regex only selects the **printed label**; removing the word changes
   `boundary` to `unclassified` and **the entry still counts as a gap**. The
   phrase that actually settles an entry is `NOT a tie-off`, **in the head**,
   and the tool already hard-fails if it appears only in a body. The forbidden
   shortcut is real; it is a different string, and a packet told to avoid the
   wrong one could take the right one by accident.
2. **The `_BOUNDARY` regex is currently matching a STRUCK QUOTATION** of the
   entry's own superseded head. The classifier is reading prose *about* a word.
3. **"This console has a job pending FIRST."** -- **MEASURED FALSE**,
   `open_at_patch=0`. This was the brief's stated reason no leaf bench caught
   the defect; the real reason is in section 3.
4. **"The adapter exports no evidence for the replay side."** -- true of the
   adapter, **false of `zhao_terrain_fieldlist` beside it**, whose three replay
   counters were already core ports. The named next diagnostic (a new adapter
   port) was not needed.
5. **"`noprog` incremented exactly once per lattice vertex, which means the
   patch's field lane fired per vertex."** -- the conclusion happens to be true
   and the inference is invalid: `noprog` counts the ENGINE's reply, so its
   value says nothing about `add_fire_i`. `entries_replayed=1` is what says it.
6. **FIELDACTIVE's "1089 + 42 = 1131"** -- correct arithmetic that
   discriminates nothing: the sum holds identically under both causes of
   `gnoprog_c`, so it was quoted as evidence for a hypothesis it cannot
   distinguish.

---

## 5. WHAT I GOT WRONG AND CAUGHT MYSELF

* **I reordered on half the evidence** (section 2.1). I read the host's law and
  the allocator's, concluded "commit first", and did not read the doorbell
  before changing the order.
* **The frame gate had to move with the reorder**, and I nearly shipped it
  without: once the commit runs first, `fld_commit_ok_q` no longer implies the
  program is loaded.

---

## 6. THE REGISTER, MEASURED BARE

| | value |
|---|---|
| base `a201825d` | **2** -- I34, I55. python RC 1 |

**I did not touch `I34`'s prose.**
