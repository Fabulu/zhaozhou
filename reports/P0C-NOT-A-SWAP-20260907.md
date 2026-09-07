# P0-C — `v3own` is not a drop-in for `fragrob`, and the port lists say so

Follow-on to `P0C-V3OWN-NOT-IN-THE-ISLAND-20260907.md`, which established that
`zhao_texture_v3own` is instantiated nowhere and the island still runs
`zhao_texture_fragrob`. The obvious next question is what the swap costs. The
answer is that **it is not a swap.**

## The interfaces barely overlap

| | ports | unique to it |
|---|---|---|
| `zhao_texture_fragrob` | 60 | 56 |
| `zhao_texture_v3own` | 48 | 44 |
| **shared names** | **4** | |

And the four shared names are handshake lines only:

    aux_rready_o   aux_rvalid_i   tmu_rready_o   tmu_rvalid_i

## They are not the same KIND of block

`fragrob` carries the **fragment datapath**: `f_u_i`/`f_v_i`, `f_binding_i`,
`f_lod_i`, `f_recipe_i`, `f_sample_count_i`, `tmu_u_o`/`tmu_v_o` (signed 32-bit
coordinates), `tmu_rgb_i` (24-bit colour in), `o_rgb_o`/`o_s_rgb_o`/`o_s_a_o`
out. Texture coordinates go in, colour comes out.

`v3own` carries **ownership and identity**: `adm_valid_i`/`adm_ctx_i`/
`adm_req_i` in, `adm_owner_o` out, opaque `result40` payloads
(`STATUS8 | alpha8 | RGB888`, Appendix B.1) into typed banks, and
`cmb_owner_o`/`cmb_s0_o`/`cmb_aux_o` to COMBINE. It never sees a texture
coordinate, a binding, a LOD or a recipe.

So `v3own` replaces fragrob's **lifetime, claim and publication machinery** —
which is precisely what §6.2's CAPTURE/SNAPSHOT/CLAIM/WRITE/PUBLISH/READY event
list describes — and replaces **none of its datapath**.

## What this does to the P0-C estimate

The earlier report noted v3own is 3,348 ALM against fragrob's 1,676 and said the
saving must come from what the swap makes deletable, recorded as an unmeasured
open claim. That framing was already cautious; this makes it sharper:

* **There is no instantiation-shaped change available.** Replacing the
  instantiation would leave fifty-six unconnected ports on one side and
  forty-four unfed ports on the other. The island's plan/dispatch/cache blocks
  currently hand fragrob coordinates and receive colour; under v3own they would
  address BANKS and exchange owner handles.
* **The deletion that pays for it is therefore not inside fragrob.** It is the
  ordering, result-storage and lifetime state distributed across `rsp_dispatch`
  (806 ALM / 1,432 registers) and the island top's own glue — which measured at
  **+2,390 ALM and +6,867 registers above the sum of its member blocks**, the
  largest single pool in the island.
* **That glue pool is the thing to measure next**, not the fragrob/v3own
  difference. The brief's P0-E says exactly this: *"Top-level payload stores and
  metadata joins — 23,181 fitted registers, duplicated state. One bank owner and
  credited joins."*

## Stated plainly

P0-C is an island restructuring, not a block substitution. Nothing here says it
is wrong — the brief argues for it on correctness grounds and the T2 identity
work is real — but anyone costing it from "3,348 versus 1,676" is costing the
wrong thing in both directions: the replacement is larger AND the work is bigger
than a swap.

**Not started.** This is a scoping finding, produced without a compiler while the
island reseed runs.
