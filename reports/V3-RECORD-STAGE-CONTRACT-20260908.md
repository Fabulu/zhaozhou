# Every stored field: one event, one stage

Owner brief §4.2 asks for this table for every stored field, because *"a
record-level contract prevents the next field, such as fog amount, repeating the
same error."* The errors in question are the two stage misalignments this
integration actually shipped and then repaired.

Derived from source today, not from memory.

## The two keyings, and why both exist

The island stores fragment data under **two different keys**, and the difference
is the whole contract:

| keying | index | written | lives until |
|---|---|---|---|
| **front-end queue** | `fc_wp` → read `fc_rp` | ingress admission beat | the planner consumes it |
| **per-owner** | owner slot `[13:8]` | ingress admission beat | COMBINE retires the owner |

**Both copies of the base colour are justified**, and the naive reading is
wrong. `fbase_m[fc_rp]` feeds the planner's `req_mat_a_i`/`req_mat_b_i` at the
front-end stage; `mat_m[owner]` feeds COMBINE. COMBINE runs after arbitrary
out-of-order completion, by which time the front-end queue entry has been
recycled for a later fragment. Deleting either copy as "duplicate storage"
breaks the machine — which is exactly the kind of saving §8 warns against
claiming without the read-site inventory.

## The table

| field | width | writer event | writer stage | key | reader stage | rule |
|---|---|---|---|---|---|---|
| `uvw_m` | 64 | fragment accepted | ingress | `fc_wp` | RCP return | belongs to the owner RCP returns, **not its generation bits** |
| `fctx_m` | 64 | fragment accepted | ingress | `fc_wp` | planner + expander | caller's opaque context |
| `fbase_m` | 32 | fragment accepted | ingress | `fc_wp` | planner (`req_mat_*`) | front-end copy |
| `fbind_m`,`flod_m`,`fcls_m`,`faux_m`,`fsc_m`,`frec_m`,`fwt_m` | — | fragment accepted | ingress | `fc_wp` | planner / expander | travel with the fragment |
| `fpsl_m`,`fpgn_m` | — | fragment accepted | ingress | `fc_wp` | planner | palette identity, front-end copy |
| `mat_m` | 46 | `own_adm_accept` | **ingress** | owner slot | COMBINE | survives reordering |
| `class_m`,`palslot_m`,`palgen_m` | — | `own_adm_accept` | **ingress** | owner slot | dispatch / palette | **fixed today** |
| `sampmeta_m` | 21 | planned sample accepted | planner | `[slot][sidx]` | CLUT, nearest, bilinear | §7's join candidate |

## The rule that was broken, twice, in one file

**A record written at stage X must be composed of stage-X values.**

`mat_m` is written at `own_adm_accept` — an *ingress* event, since
`own_adm_valid_c = frag_valid_i && rcp_v_ready` — from the input ports. Correct.

`palslot_m`/`palgen_m` sat in the **same `always_ff`**, on the same event, and
took `f_pal_slot_c`/`f_pal_gen_c` — which are `fpsl_m[fc_rp]`/`fpgn_m[fc_rp]`,
*planner*-stage values belonging to a fragment several cycles older. Two
per-owner tables, one event, disagreeing about which fragment they described.

The same shape appeared at the AUX request, which took its world coordinates
from `own_out_ctx` — the context of whatever the **output** stage was emitting.

Both are repaired. **Neither was caught by 119 functional checks until the
symptom happened to become visible**, and the palette one showed as just three
stale lookups because a phase holds one palette slot for most of its fragments:
the misalignment is invisible wherever the old value and the new one are equal.

## What this means for the next field

The brief names fog amount specifically. Its row must be filled in **before** it
is wired, not after:

> fog factor | 17 | ? | ? | ? | ? | ?

The interpolant rides ATTRSTEP per vertex, so its writer stage is geometry and
its reader is the rasteriser's post-toon mix — a different pair from anything in
the table above. That is precisely when this kind of error gets made.

**And a stage-valid bit is not a substitute.** The brief says so directly, and
this file demonstrates it: every one of these writes was guarded by a correct
valid. The valid was never the problem — the *value* was from the wrong stage.
