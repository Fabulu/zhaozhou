# The intended-selection swap, APPLIED AND REVERTED — what it actually costs

Run end to end on 2026-09-18 as a dry run, then reverted deliberately. The
specification in `RESOURCE-RESCUE-ROADMAP-CURRENT-20260913.md` said what to
change; this file records what changing it COSTS, which nobody knew.

## It works, and the tree accepts it

Applied in full: six modules out of `top:`, three composed tops in, seven
displaced modules given `superseded` dispositions, nineteen more un-excluded
because the new tops actually reach them, the parameter-override section
removed, and twenty-four sources added to `zhao_prod_top`'s closure.

```
prod manifest: 275 modules, 60 tops, 98 inside, 117 excluded
manifest check OK -- every module counted once or declared absent
zhao_prod_top: 63 instances -> 60
verilator --lint-only --top-module zhao_prod_top: 170 sources, 0 errors
```

**So the intended console elaborates.** That was not known before today.

## What it costs, and this is the part worth having

**Seven packet registration gates go red, and each is right to.** They freeze
*which selection its packet was accepted against*:

| gate | objection |
|---|---|
| `packet_g_registration_static` | a module "is not exactly excluded:not-yet-adopted" — the swap made it `inside` |
| `packet_h_shell_prereqs_registration_static` | same class |
| `g8a_dsp_rescue_registration_static` | a pinned marker is no longer exact/unique |
| `packet_d_registration_static` | same class |
| `packet_f_g8a_registration_static` | same class |
| `raster_texture_stage_v3_registration_static` | same class |
| `raster_texture_v3_fit_top_generated_freshness` | the G8A wrapper's recorded sources moved |

These are not breakage. **Each one is a packet saying "I was accepted with the
projector private and the V1 shell selected", and that sentence stops being
true.** Re-asserting each packet's acceptance under the new selection is a
decision per packet, with a reason recorded per packet — the same shape as the
five `PROTECTED_HASHES` moves this campaign already made, and not something to
do in passing.

## Why it was reverted rather than pushed through

Three reasons, in order of weight:

1. **The baseline has not returned.** `@whole-console-sizing` was still in
   synthesis. Applying the swap first destroys the reference the entire
   comparison depends on, and re-running an hours-long fit to recover it is the
   expensive way to learn that.
2. **The intended selection is not final**, because the console is not complete.
   Particles is 0.26× of its envelope, the board wrapper is unpriced, fog is not
   connected, and the client-A producer does not exist. A selection frozen now
   would be re-frozen as soon as any of those lands.
3. **Seven red gates across five packets is worse than not starting**, if the
   session cannot finish them with a reason written for each.

## What to do with it

The swap is mechanical and now proven to work. When the baseline is in hand and
there is room to re-accept seven packets deliberately:

1. apply it (the steps above, in that order — the manifest checker walks you
   through them by refusing the wrong ones);
2. update each packet gate with its own recorded reason;
3. re-census on the same sizing device;
4. difference the two rows.

**And expect the checker to catch things.** It found a DOUBLE COUNT within
seconds — `zhao_texture_island_v3_top` left as a top while `zhao_shell_top_v2`
contains it, which would have inflated the intended selection by 15,446 census
ALUT and made the swap look worse than it is. It also refused nineteen
`not-yet-adopted` rows with the line worth quoting: *"it is in the machine
whatever the manifest says."*

One thing it does NOT check, found by linting instead: a missing PACKAGE.
`zhao_fb_tuple_pkg` was absent from the source list and surfaces only as
`Import package not found` at elaboration.
