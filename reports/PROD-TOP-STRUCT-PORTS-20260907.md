# `zhao_prod_top` does not map, and the cause is one line in its generator

*2026-09-07. Found while checking that today's texture changes had not broken the
production top's elaboration. They had not — but it was **already** broken, and
had been failing silently as far as any cheap check was concerned.*

---

## The state

The ledger row:

    zhao_prod_top    failed:quartus_map.exe    (no ALM figure)   0e8b1c9d

So the top that exists to answer the owner's question — its own header states it,
*"what does the planned console cost when counted ONCE?"* — **currently answers
nothing.** It does not survive `quartus_map`.

## The cause, confirmed in the generator rather than guessed

`tools/quartus/gen_prod_top.py:parse_ports` strips the leading keywords and then
takes the **first identifier** it finds as the port name:

```python
d = re.sub(r"^(var|wire|reg)\b", "", d).strip()
d = re.sub(r"^(logic|bit|byte|integer)\b", "", d).strip()
...
m2 = re.search(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", " ", d))
name = m2.group(0)
```

For an ordinary port (`output var logic [63:0] beat_data_o`) that is correct. For
a **struct-typed** port:

```systemverilog
output var zhao_guard_rsp_t guard_rsp_o
```

the first identifier is `zhao_guard_rsp_t` — **the type, not the port** — and
since no packed range precedes it, the width defaults to one bit. The generated
top therefore contains:

```systemverilog
logic [1-1:0] u23_zhao_guard_rsp_t;
```

and when one instance has two ports of the same struct type, the two wires
collide. Verilator names the consequences exactly:

    %Error: zhao_prod_top.sv:1492: Duplicate declaration of signal: 'u23_zhao_guard_rsp_t'
    %Error: zhao_prod_top.sv:1207: Instance attempts to connect to 'zhao_client_e',
            but it is a TYPEDEF 'zhao_client_e'

16 errors, all of this one shape. **None of them mention any block changed
today** — checked, because that was the question that started this.

## Why nobody noticed

**There is no lint target for `zhao_prod_top`.** Every other significant module
in `tests/CMakeLists.txt` has one; the generated top has none. Its only check is
a full Quartus run, which is hours, so a defect that Verilator reports in about
two seconds has been sitting behind the most expensive gate available.

That is the same shape as this repository's other expensive lessons: the check
existed in principle and nothing cheap ever ran it.

## What this costs, concretely

CLAUDE.md already records why this top matters:

> **Regenerate `zhao_prod_top.sv` after ANY port change.** … a new port that
> nobody connects is a `PINMISSING` that only the next fit discovers.

The rule assumes the fit *works*. It does not, so today there is no
whole-machine "counted once" number at all — which is also an input §12.4's
whole-island reconciliation will eventually want.

## Deliberately NOT fixed in this pass

The owner's direction is explicit: *"Terrain, projection, broad fit-sheet
evacuation, and unrelated measurement-tool expansion are not the current
implementation priority. An interesting new bottleneck elsewhere does not
override this."* The production resource top is measurement apparatus, and this
is exactly the "interesting bottleneck elsewhere" the direction names.

So it is recorded, not chased. The fix is small and the diagnosis is complete:

1. In `parse_ports`, after the keyword strips, if the remaining declaration is
   `IDENT IDENT` then the **first** is a type and the second is the port name.
2. The wire for such a port must be declared with the **type**, not
   `logic [1-1:0]` — `zhao_guard_rsp_t u23_guard_rsp_o;`.
3. Add a `lint_prod_top` ctest entry so this cannot regress behind a
   multi-hour gate again.

Step 3 is arguably worth doing on its own even under texture-first, because it
turns a class of silent breakage into a two-second failure — but it is a gate
change and belongs to a deliberate decision rather than a side effect.
