# DECISION — I34's constant pool is fixed in the LIBRARY, not in RTL

Taken 2026-09-27 by the coordinator under the standing delegation in
`reports/OWNER_VACATION_DIRECTIVE_2026-09-23.txt` §0. Format per §0: question;
chosen option; reason and alternatives; constraints/cost; consequences.

---

## QUESTION

MATFIELD found and proved the blocker behind the owner's clause 3: **the composed
console never delivers a field program's CONSTANT POOL to the execution register
file.** A program cannot hold the material token it means to write — the value
exists, the path exists, and **the number never arrives.**

MATFIELD raised this under protocol rule 4 rather than choosing, and recommended
a **library** fix: have `lower()` emit folded constants as **`LDC` uops**.

## DECISION

**Take the library fix. `lower()` emits folded constants as `LDC` uops. No RTL.**

## REASON

1. **It costs ZERO SILICON.** The console needs **293,352 ALUTs against 227,120
   present** and **375 DSP against 112**. Every RTL alternative spends area on a
   device already 350% over, to deliver a number the instruction set can already
   carry.
2. **The register file has room, measured**: high-water is **28 of 32**. This is
   not a capacity argument that needs a widening — the space is there.
3. **It cuts nothing the owner fixed.** His ruling forbids cutting field
   capacity, semantics, update behaviour or the destination. **An `LDC` lowering
   touches none of them**: the same program computes the same result and writes
   the same destination; only the route the constant takes into the register file
   changes.
4. **It is the smaller claim.** An RTL constant-pool delivery path would be a new
   production capability with its own ports, its own evidence surface and its own
   fit cost. A lowering change is testable in the compiler's own suite **and** in
   the console smoke, with no new silicon to verify.

## ALTERNATIVES CONSIDERED

* **An RTL constant-pool delivery path.** Rejected on area, and because it
  invents a capability to carry data the ISA already carries.
* **`LdUniform`.** **MATFIELD built this, disproved it by reading `ring_svc`, and
  reverted it before committing** rather than ship a non-repair under a repair's
  name. That is the right disposal and it stands — do not re-attempt it without
  addressing what `ring_svc` actually does.
* **Baking the constant into the program image as data.** Rejected: it makes the
  material token a property of the *capsule* rather than of the *program*, which
  breaks clause 3's whole point — the value must be something the field computes
  and writes, not a literal the fixture smuggled in.

## CONSTRAINTS AND COST

* **Zero silicon, and that is a claim to check**: the packet owes the evidence
  that no RTL changed — `gen_prod_top --check` and `gen_console_board --check`
  both fresh, as NOPROG demonstrated for its own zero-silicon fix.
* **The register high-water must be re-measured after the change**, not assumed
  from 28 of 32. Folding constants into `LDC` uops *adds* register pressure by
  construction.
* **This is the ONE compiler change the hardware lane needs, and it is not a
  licence to widen the compiler's scope.** Nanquan is provisional and the
  standing rule is to stop compiler overengineering; this is a lowering fix in
  service of hardware closure, nothing more.

## CONSEQUENCES

* **Code:** the lowering path and its program tests. **No RTL.**
* **Tests:** the compiler's own suite, plus `scorch_wash` executing in the
  console with clause 3 met — **and the two consumer assertions MATFIELD
  committed but could not exercise, because the run fatals above them.** Those
  become reachable once the constant arrives; **they must be seen to pass, and
  their ability to fail must be shown.**
* **What it does not settle:** `TERRAIN.COMPOSED_MATERIAL` remains commissioned
  and unbuilt. This decision unblocks clause 3; it does not substitute for the
  destination.
