# G8A Timing3 + ATTR3/BIL2 path report — 2026-09-15

## Verdict

The combined Timing3 control-path batch and primary DSP-rescue portfolio produced
a complete, clean, physically fitted G8A measurement from source commit
`3bf599d5b2a9f5af1c0d57bfb540122d9fdbb301`.

The DSP architecture worked exactly: the subsystem moved from **49 to 30 physical
variable-precision DSP blocks**, with the intended candidate hierarchy and no old
attribute or bilerp engines left in the mapped design.

Packet F nevertheless remains **timing-red**:

- restricted Fmax: **90.96 MHz**;
- setup WNS: **-0.994 ns**;
- setup TNS: **-131.275 ns**;
- hold WNS/TNS: **+0.242 ns / 0**.

Do not repeat this unchanged fit. The remaining timing debt is measured and now
small enough to attack as one more architectural batch.

## Scope correction: this is not the whole machine

The fitted **13,195 ALMs** belong only to the generated G8A raster/texture
subsystem wrapper. They are not a console total and are not “headroom under
30,000.” This receipt excludes the final shell/framework, terrain, shared
projection, remaining memory/video integration, unpriced required roots, and
G8C composition glue.

The owner's closure target remains unchanged: only a clean final G8C/production
composition may demonstrate comfortable whole-machine margin below **30,000 ALMs
and 85 DSPs at 100 MHz**. No standalone/subsystem arithmetic is substituted for
that receipt.

## Frozen specimen and evidence

| Item | Value |
|---|---|
| Source commit | `3bf599d5b2a9f5af1c0d57bfb540122d9fdbb301` |
| Source digest | `e322536216e7731564dc8028ddf4f86b25850478a9e7d291e1a2b93677d6718f` |
| Source count | 48, exact ordered manifest |
| Device/tool | `5CSEBA6U23I7`, Quartus Prime Lite 17.0.2 Build 602 |
| Boundary | 18 physical pins, 0 virtual pins |
| Seed | 1 |
| Top parameters | `ATTR_DSP3=1 BILERP_DSP2=1` |
| Vendor backend | `ZHAO_DUAL18_CYCLONEV=1` |
| Nested V3 | `MIGRATION_SHADOWS=0`, `BILERP_DSP2=1` |
| Runtime | 909.1 seconds |
| Receipt SHA-256 | `6d5330569172391ef423e371c692a5dba9da5037c31560b2a215abf84dd6337e` |
| Fit-manifest SHA-256 | `c8a3398596d9c83886a16c1dbaaa3b6d343347c1962da9ff701ca0e802cd4db4` |

Canonical receipt:
`reports/synthesis/zhao_g8a_raster_texture_timing3.json`

Attempt packet:
`reports/characterization/g8a_raster_texture_single_owner_characterization/3bf599d5-20260915T204015Z-timing3/`

Raw reports:
`reports/synthesis/blockpaths/zhao_raster_texture_v3_fit_top@g8a-timing3.*`

The receipt validator returns its expected red-gate status (`RC=2`) and reports
itself byte-fresh.

## Resource and timing movement

| Metric | Timing2 | Timing3 + DSP rescue | Delta |
|---|---:|---:|---:|
| ALMs (local subsystem) | 12,772 | 13,195 | +423 |
| Registers | 21,353 | 22,496 | +1,143 |
| Memory bits | 92,964 | 92,964 | 0 |
| RAM blocks | 71 | 71 | 0 |
| DSP blocks | 49 | 30 | **-19** |
| Fmax | 84.95 MHz | 90.96 MHz | **+6.01 MHz** |
| Setup WNS | -1.771 ns | -0.994 ns | **+0.777 ns** |
| Setup TNS | -2,204.611 ns | -131.275 ns | **+2,073.336 ns** |
| Hold WNS | +0.250 ns | +0.242 ns | -0.008 ns |

The 2,000-row Timing2 summary was entirely negative. The Timing3 summary contains
**497 negative and 1,503 nonnegative paths**. This is genuine broad recovery, not
one path trading places with another.

## Exact physical DSP ownership

The mapped entity hierarchy contains:

| Entity | Count | Physical DSP per entity | Subtotal |
|---|---:|---:|---:|
| `zhao_raster_attrgrad_dsp3` | 3 | 3 | 9 |
| `zhao_attr_mul72x13_dsp3` | 3 | inclusive in lane | — |
| `zhao_mul27_exact` | 9 | 1 | 9 |
| `zhao_texture_bilerp_lane_dsp2` | 4 | 2 | 8 |
| `zhao_dual18_mul` | 4 | 1 | 4 packed-horizontal subtotal |
| `zhao_raster_attrgrad_v2` | 0 | — | 0 |
| `zhao_texture_bilerp_lane_v2` | 0 | — | 0 |

The remaining 13 DSPs are the existing perspective/RCP/edgewalk and other
unmodified residual ownership. Quartus's complete physical mode table is:

- 11 `Independent 27x27` blocks;
- 14 `Two Independent 18x18` blocks;
- 3 `Sum of two 18x18` blocks;
- 2 `Independent 9x9` blocks;
- total **30**.

This proves the selected G8A structure. It does not yet adopt those candidates in
production; `BILERP_DSP2` remains zero in the selected production manifest until
the later atomic adoption packet.

## RAM and structure gates

All structure gates pass:

- one `zhao_texture_v3own` lifecycle owner;
- zero TEXJOIN instances;
- zero old attrgrad/bilerp engines;
- no mapped migration-shadow state;
- both TILESTORE banks inferred;
- ordered-retirement `oq_res_q` and `oq_ctx_q` remain inferred RAM;
- 71 RAM blocks and 92,964 memory bits, unchanged from Timing2;
- exact vendor-only dual18 macro closure;
- exact top and nested parameters.

The generic and Timing3-specific RAM validators both pass. Uninferred small or
asynchronous arrays remain explicitly listed in the canonical receipt; none
matches the receipt's critical asynchronous ownership/context categories.

## Reproducible path census

Tool:
`tools/quartus/g8a_timing_path_census.py`

Outputs:

- `reports/synthesis/zhao_g8a_raster_texture_timing2_paths.json`
- `reports/synthesis/zhao_g8a_raster_texture_timing3_paths.json`

The tool parses the committed TimeQuest summary bytes, binds their SHA-256,
retains the top 20 complete paths, and classifies all negative launch and endpoint
families. Its built-in positive/negative parser self-test passes.

Timing3 negative launch families:

| Launch family | Paths | Worst slack | Worst data delay |
|---|---:|---:|---:|
| sample-result-bank write enable | 7 | -0.994 ns | 9.083 ns |
| owner-mask lifetime fault | 196 | -0.848 ns | 10.188 ns |
| UV-join lifetime fault | 26 | -0.844 ns | 10.156 ns |
| combine product | 37 | -0.843 ns | 10.225 ns |
| fragment expand | 2 | -0.738 ns | 10.435 ns |
| BIL2 vertical stage | 25 | -0.674 ns | 10.386 ns |
| owner control | 50 | -0.582 ns | 9.911 ns |
| other core | 94 | -0.533 ns | 9.825 ns |
| early-descriptor RAM | 18 | -0.409 ns | 7.894 ns |
| tile control | 30 | -0.375 ns | 9.704 ns |
| ATTR3/divider | 12 | -0.366 ns | 10.005 ns |

Endpoint ownership is concentrated in owner control (255 paths), material combine
(44), AUX (37), BIL2 (25), UV join (18), ATTR3 (16), tile control (30), and 26
other island-control paths.

## Worst paths

1. **Sample-result bank → material O operand**
   - slack: **-0.994 ns**;
   - data delay: 9.083 ns;
   - clock skew: **-1.731 ns**;
   - launch: sample-result bank 0 inferred-RAM write-enable register;
   - endpoint: `zhao_texture_material_combine_v3:o_a0[3]`.

   The READ_LATE combiner aligns context/phase through Q→R→D, but source sample
   data remains combinational into the O-stage operand selection. The large skew
   makes a sub-10-ns data cone fail by nearly 1 ns.

2. **Reset-lifetime fault fanout → owner state**
   - `owner_mask_lifetime_fault_q` to owner claim/live/request/commit arrays;
   - worst **-0.848 ns**, 10.188 ns data;
   - 196 summarized negative launch paths.

3. **UV lifetime fault → owner/island state**
   - worst **-0.844 ns**, 10.156 ns data;
   - 26 negative launch paths.

4. **Combiner product → writeback row**
   - worst **-0.843 ns**, 10.225 ns data;
   - 37 negative launch paths spread across `m_p0/m_p1` to `wb_row`.

5. **BIL2 vertical interpolation**
   - worst **-0.674 ns**, 10.386 ns data;
   - 25 negative launch paths from B1 intermediates to B2 output.

The old Timing2 worst ATTR lane-1 column/fault path and broad OQ/returned-sequence
feedback family no longer dominate. Timing3's registered ATTR join, result head,
issue/combine notification cuts, lifetime classification, and output I/O
registration therefore did their intended work.

## Timing4 architecture batch

These are measured next actions, not completed fixes. A cone-level review corrected
one important first reading: the lifetime paths are not merely a local fault
fanout. They traverse lifetime fault → outer candidate-skid ready/pop → accepted
candidate decode → 64-entry owner-scoreboard initialization. Registering the
fault would create an unsafe one-cycle admission window. **Keep immediate fault
admission blocking; register accepted owner events instead.**

Primary cuts:

1. **Register the READ_LATE source payload before recipe/O selection.** Add an S
   stage after D that captures source planes plus aligned material controls,
   scratch, context, and phase. The worst endpoint is a DSP input register, so a
   register after O would miss the measured cone. Preserve one accepted phase per
   clock, count-zero/refusal behavior, reset, and quiet.
2. **Register owner events, never the lifetime fault.** Keep the direct lifetime
   level on fragment ready/admission blocking. Capture accepted admission
   `{valid,slot,request}` and use that local event for scoreboard initialization;
   similarly register the exact combine-pop owner before completion reservation.
   Align `join_validation_pending_q` to the registered accepted event. Prove
   fault-coincident offers cannot enter and preserve full generation identity,
   zero-work tickets, and same-edge release precedence.
3. **Split product finishing from completion-row assembly.** `wb_v/wb_row` already
   exist; adding another nominal WB would repeat Timing3. Insert an F boundary for
   `finish_lane` results and carried controls between M and WB, assemble the next
   scratch row from F, and retain II=1. Together with S, Q→continuation grows from
   roughly six to eight clocks: `NCTX=8` is sufficient but leaves no spare stage,
   so eight-context zero-bubble evidence is mandatory.
4. **Retime BIL2 without adding an elastic slot.** Make B2 register the vertical
   product and B1 base term, then derive the final add/round from those held B2
   registers to output. External three-stage occupancy/latency and hold behavior
   remain unchanged while the DSP output register splits subtract→DSP→adder. The
   MapOnly gate must remain exactly two DSPs.
5. **Stage AUX input-fault observation.** Capture a separate A0 degenerate fact;
   update `degenerate_o` and `frame_fault_o` from registered A0 inputs instead of
   the raw force-refuse/envelope cone. Fault visibility changes by one cycle, but
   acceptance, completion, and arithmetic must not.
6. **Remove fit-wrapper clear feedback.** Replace the wrapper's combinational
   auto-clear with a held recoverable-clear request, never generated by a
   lifetime structural fault, and suppress synthetic jobs while clear is pending.
   This is a real fit-boundary path, not production logic to ignore.

The original four named cuts cover only part of the 497 red rows. Do not spend
Timing4 until these remaining known families also receive a real boundary or an
exact logic reduction:

- RCP token → indexed UVW read: add one full-identity elastic owner/result stage;
- early descriptor generation gating: avoid duplicating the 287-bit bank; capture
  a small usability verdict and zero stale work at the existing joined boundary;
- tile abort → job-metadata enables: capture accepted metadata independently, and
  qualify only state/counter/start effects with the prior job's abort;
- ATTR negative-dividend preparation: split the 98-bit magnitude carry across two
  49-bit steps while preserving exact `~num + den` arithmetic;
- AUX restoring step: use one widened subtract whose borrow is the comparison;
- sequence mismatch broadcast: keep same-edge Packet-C suppression, but use the
  registered abort for later outer-tile draining.

Required pre-fit controls include every combiner recipe and READ_LATE 0/1 case,
source/context/phase alignment under stalls, exact issued/completed counts, eight
contexts with no bubbles, fault-coincident owner offers, back-to-back and
zero-work owners, stale-slot mutants, cycle-identical 4,000-tuple BIL2 differential,
held terminal output, consecutive AUX fault cases, clear/fault collision with
fault priority, and back-to-back RCP/descriptor identities under every relevant
stall combination.

Only after those focused suites pass and the source census accounts for every
currently known negative family should one connected Timing4 fit be spent. The
fit must retain 30 DSPs, nine ATTR leaves, four dual18 leaves, zero old engines,
and unchanged critical RAM inference.

## Packet-F disposition and roadmap

Packet F remains open because timing failed. The DSP reduction is retained as a
successful measured sub-result, not rolled back and not promoted to production.
Packet-H control-plane prerequisites may continue as excluded siblings, but
`shell_top_v2` and any production selection remain blocked until Packet F reaches
100 MHz with nonnegative setup slack/TNS and all existing structure gates.

R1 remains deferred until R0 reaches Packet K. The reusable quarter-square,
26+6 hybrid, and coefficient-table programme must not displace this timing closure
or be double-counted with ATTR3/BIL2.
