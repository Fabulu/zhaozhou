# Owner ratification — the six completion rulings, 2026-09-22

## Authority

**These six decisions are ratified by Fabian's adoption of them on 2026-09-22,
and by nothing older.**

* **Document:** `reports/Zhaozhou_proposed_completion_rulings_2026-09-22.txt`
  (126 lines), copied into the repo unaltered from the owner's own file.
* **Reviewed commit:** `9a8b329ae4b539a62f7c5ec7d86b6b0131091b2a` — the head of
  `claude/ceiling-architecture-20260912` at the time of review.
* **Adoption:** Fabian, 2026-09-22, sending the text to the working agent with
  the words **"We're fixing this"** and a decision table adopting all six.

The document's own first paragraph says it *"is a proposed ruling for Fabian to
adopt and send to the implementation agent. It is not an already-ratified
repository document."* **He adopted it and sent it. That act is the
ratification, and this file is the record of it** — the same discipline the
2026-09-20 WARP directive demanded in its own words: *"the implementation must
record that ratification in the repo, not attribute it to an older ruling that
never said it."*

**Read the source document, not this summary, before implementing any item.**

---

## THE GENERAL AUTHORIZATION — it changes how escalation works here

> *"Complete the mandatory console without silently cutting features or
> substituting testbench stimulus for production producers… The coordinator owns
> their implementation details, generated layouts, adapters, arbitration and
> validation. **Do not repeatedly escalate the same decision because its
> implementation needs another field, decoder arm or bounded helper.**"*

**Escalate ONLY:** a concrete contradiction with a still-binding owner
requirement; a new externally visible behaviour choice not covered by the six;
or a genuinely broader permission request. **Name the conflicting clauses and
recommend a resolution.**

> **"A missing implementation already commissioned here is work, not an
> unresolved policy decision."**

**This retires a habit that cost this campaign whole passes.** Eleven times in
three days a blocker survived re-reading and died on first contact (R237). Five
of those were *policy* holds that had already been decided. **A packet that now
reports "this needs an owner decision" for anything inside these six is wrong by
default and must say which clause it believes it contradicts.**

**Unchanged:** staffing (**one agent at a time**) and **fit at completion**.

**And the sentence that governs how I report:**

> **"Keep reporting measured costs separately from estimates. An estimate is not
> a reason to remove commissioned functionality, and a ZERO STRUCTURAL GAP COUNT
> IS NOT EVIDENCE THAT EVERY PATH HAS EXECUTED."**

---

## THE SIX, in one line each — the source text governs

1. **Particles — explicit `NO_MATERIAL` mode.** A lawful mode, **never** inferred
   from a failed lookup, a sentinel handle, the previous span's material, or the
   untextured bit. No resolution request, no fault counter. **R197 is kept for
   `MATERIAL_BACKED` primitives.** Material mode is **part of the span's
   identity** and mode changes honour the existing drain/ordering mechanism.
   **Covers the whole particle-to-raster connection including canonical depth
   conversion** — *"not permission to close the task after changing only the
   material gate."*
2. **Procedural material — a REAL `(set handle, record ID)` pair.**
   `material_set` is the **complete** `handle32[material_set]`; `material_id` is
   an independent **u16**. **Reserved payload bytes of `DrawProcedural` are
   authorized for the ID**, `frame_tick`'s allocation preserved, both made
   explicit in `commands.zidl` and regenerated. Zero-filled legacy ID bytes
   select **record 0, a valid index**. **Where an earlier implementation guessed
   differently, DISCLOSE the rendering difference — do not silently overwrite
   goldens.** An additive opcode is the authorized fallback **without another
   owner round-trip**.
3. **TWOD — BOTH `SetPlane` (0x0306) and `DrawSprite` (0x0307)**, checked against
   live reservations first; the coordinator may pick the next free opcode and
   record it **without a round-trip**. Descriptors are **frame-scoped**: staged,
   validated, **published as a sealed list**, and neither a later packet nor the
   next frame may mutate a list being consumed. **Includes the real asset/palette
   loading producer** — *"an opcode plus descriptors referring to data that only
   the testbench can inject is not completion."* **R235 preserved.**
4. **GEOM.PARAMBUF — a NARROW authorization**, not a blanket one.
   `[0x06000000,0x06400000)` view 0, `[0x06400000,0x06800000)` view 1,
   `[0x06800000,0x06A00000)` shared scratch — **half-open**. The asset pool at
   `[0x06A00000,0x08000000)` stays **read-only to ENGINE1**. Overflow-safe extent
   checks; **a request crossing a boundary is not allowed merely because both
   endpoints lie in the union.** Request identity travels with the request — *"do
   not validate a queued request against a later global view selector."*
   **Extend `mem_guard_no_escape`, do not bypass it, and include a deliberate
   fault that makes the proof FAIL.** Covers the arena producer, allocation,
   write route, readers and consumers — the existing decoder *"is not the whole
   subsystem."*
5. **`sparse_fill` — derived from the arena's validity mode.** On for
   `VALID_MODE == 0` (bitmap-valid), **off** for dense-seal. The unsafe
   combination is **refused**, the setting is **stable for a whole job**, and
   **no new command producer is required.**
6. **Neighbour edges — BUILD THE REAL PRODUCER.** `8'h00` is **retained as the
   conservative fallback until the real producer is validated**, and **must not**
   be changed to a cheaper constant. Bounded prepare/reconcile/emit sequencing
   over the frame's admitted terrain set is authorized; **a second terrain engine
   or duplicate world store is not.** *"This is an explicit authorization to
   build the missing scheduling functionality, not a claim that it is already
   cheap or complete."*

---

## THE COMPLETION REPORT FORMAT — mandatory, per item

> policy adopted · producer implemented · **production consumer connected** ·
> **real command-to-output behaviour exercised** · failure cases tested ·
> area/timing **estimated or measured, kept separate**.

> **"An otherwise green smoke whose upstream fixture never reaches the new path
> does not prove the path."**

**That sentence is aimed squarely at this tree and it is correct.** The smoke
**fails every terrain page's CRC**, so terrain's composed door never opens — a
lane had to move a counter mid-packet because one placed past that door *cannot
be fired*. **TERRABAKE said the same of its own work unprompted:** the composed
bake chain *"is QUIESCENT in the smoke and cannot be otherwise."*

---

## AND IT CORRECTS ME, on the item I recommended

I proposed making the material handle *"the (set, id) pair directly, so there's
one numbering rather than a translation table."* **That was wrong, and the owner
caught the mechanism:** the handle convention is **24 index bits + 8 generation
bits**, so reading its low 16 bits as the record ID means **changing a handle's
GENERATION silently changes which material is selected.** A residency event
would repaint geometry.

**My error ran in the flattering direction — it made the change look like two
lines and no new bytes**, which is R229/R237's axis exactly, and I had put that
very law in nine consecutive briefs. **The ruling's `(full 32-bit set handle,
independent u16 ID)` is the correct reference and supersedes my recommendation.**

---

## EXPECT THE REGISTER TO RISE

Five of these six commission **capability that does not exist yet** — a sprite
descriptor path with its asset producer, a parameter-buffer arena, neighbour-edge
reconciliation scheduling, the particle depth carriage. **R214 says a block with
a contract and silicon owes a ledger row, so building them makes the count go UP
until each is composed.** The count went **22 → 25** that way before it went
**25 → 10**.

**That is the instrument working. A number that falls because work was
commissioned away would be the broken one.**
