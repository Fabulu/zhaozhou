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

## Timing4 implementation questions

These are measured next actions, not completed fixes:

1. **Register the READ_LATE source payload before O selection.** Add one elastic
   source/payload stage aligned with context and phase so sample-bank outputs do
   not feed O operands combinationally. Preserve one accepted phase per clock,
   exact source-request ownership, count-zero/refusal behavior, reset, and quiet.
2. **Isolate reset-lifetime fault fanout.** Keep immediate top-level admission and
   publication blocking, but replace hundreds of direct owner-state enables with
   a local registered barrier request/transition. Prove that no same-edge illegal
   work is accepted and that only reset clears the fault.
3. **Split product finishing from completion-row assembly.** The existing WB
   register still receives rounding/status/row construction from `m_p0/m_p1` in
   one cone. Add a fully pipelined finish boundary while retaining II=1 and exact
   phase/completion counts.
4. **Retime BIL2 vertical interpolation.** If it remains negative after the
   higher-fanout cuts, add an elastic vertical-product/finish boundary or an
   equivalent exact one-DSP formulation. A latency change must be explicit; the
   current cycle-exact V2/BIL2 test may not be silently weakened.

Batch the verified changes before another connected fit. A fourth fit that changes
only one small cone while known -0.8-ns families remain would violate the project's
subsystem-fit policy.

## Packet-F disposition and roadmap

Packet F remains open because timing failed. The DSP reduction is retained as a
successful measured sub-result, not rolled back and not promoted to production.
Packet-H control-plane prerequisites may continue as excluded siblings, but
`shell_top_v2` and any production selection remain blocked until Packet F reaches
100 MHz with nonnegative setup slack/TNS and all existing structure gates.

R1 remains deferred until R0 reaches Packet K. The reusable quarter-square,
26+6 hybrid, and coefficient-table programme must not displace this timing closure
or be double-counted with ATTR3/BIL2.
