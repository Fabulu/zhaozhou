# The @pktC receipt cannot be quoted, for a second reason

2026-09-08. The Decrufter brief requires of every packet: *"What disappeared from
the source → what disappeared from the synthesized circuit → what replaced it →
what the real measurements say."* Nothing produced that, so
`tools/quartus/packet_accounting.py` now does. Running it on packet C answers
the first and third questions and **refuses** the second and fourth.

## What it says

```
1. WHAT DISAPPEARED FROM THE SOURCE
   20 lines removed across fpga/rtl/texture/, fpga/rtl/raster/
     - counter bump   3 removed

3. WHAT REPLACED IT
   813 lines added
     + always_ff      6 added
     + instantiation  1 added
     + array decl     2 added
     + counter bump   16 added
```

That is the headline and it is worth sitting with. The Decrufter's thesis is
that the island needs **gutting**. Packet C removed twenty lines and added eight
hundred and thirteen, adding six `always_ff` blocks and thirteen net counter
bumps. Whatever packet C was, it was not a decruft — and the four-part account
makes that visible in a way "packet C complete" never could.

## Why sections 2 and 4 are refused

```
zhao_texture_island_v3_top@pktC: rtlCleanAtHead is FALSE -- the tree was
dirty when this was measured, so its sourceCommit 8ce30e84 names a commit
it did not fit
```

So the 15,911 ALM / 73.98 MHz milestone is unquotable on **two independent
grounds**: it measures a circuit carrying the live D0 metadata-swap defect
(`reports/D0-JOIN-SEAM-REPRODUCED-20260908.md`), and it was fitted from a dirty
tree, so its own digest describes something other than the commit it names.
I quoted it as packet C's cost. It is not evidence about that commit at all.

## The thing the tool found that I did not expect

The first version of this gate refused **both** rows, because the Stage C row's
status is `failed:structure`. That was my error and it is the instructive kind.
`failed:structure` does not mean the fit failed — it means the fit *completed*
and the budget rules rejected the result:

```json
"status": "failed:structure",
"rtlCleanAtHead": true,
"sourceDigest": "6094a429…",
"ruleViolations": [
  "registers 20561 > allowed 9000 …",
  "ALM 13133 > allowed 7500",
  "DSP 17 > allowed 14"
]
```

Now put the two rows side by side:

| | Stage C | @pktC |
|---|---|---|
| status | **`failed:structure`** | **`ok`** |
| tree clean | **true** | **false** |
| digest recorded | yes | yes |
| rule violations listed | 3 | 0 |

The row with a clean tree, a real digest and three honestly-declared budget
breaches is stamped **failed**. The row fitted from a dirty tree, whose digest
describes nothing, is stamped **ok**. A gate that reads `status` alone refuses
the trustworthy measurement and waves the untrustworthy one through.

And the zero is not compliance. **0 of 26 labelled rows carry any
`ruleViolations`; 12 of 92 unlabelled rows do.** Labelling a fit exempts it from
the rules silently. That is this repository's own law about instruments — the
defect makes the answer look better, and nobody audits good news — reproduced
inside the receipt schema itself. `ruleViolations: []` on a labelled row is
silence, and the tool now says so out loud rather than reading it as a pass.

**51 of 118 rows in the receipt have `rtlCleanAtHead: false`** — very nearly
half the fit ledger is unanchored to the commit it names.

## What follows

* Sections 1 and 3 need no fit and stand as recorded above.
* Sections 2 and 4 for packet C need a re-fit from a clean tree **after** the D0
  repair, not before — refitting the defective arrangement would buy an anchored
  measurement of a circuit we are about to change.
* The rule-application gap on labelled rows is a real defect in
  `check_fit_rules.ps1`'s coverage, not a reporting nicety. Filed here rather
  than fixed, because it is outside the texture island's scope and owner
  direction `49fc32e9` is explicit that unrelated measurement-tool expansion is
  not the current priority. It belongs in the ledger of known gaps.
