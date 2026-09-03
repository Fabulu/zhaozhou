# Contract — GEOM.VDECODE (Vertex decode)

> Ledger: `design/blocks.yml` · owner ZH-053 · phase 8 · maturity SPECIFIED

## Purpose and exclusions
Fetch and decode vertex data (positions/normals/UV) into the skinning format.

**Format 0 — RAW/CANONICAL — is authorised now** and is buildable (ruling R11,
2026-09-02). Build against it first.

**Formats 1 and 2 (the packed encodings) are BAKE-OFF GATED.** They are not
deferred and not refused: they are waiting on a measured comparison, and no RTL
for them is authorised before that comparison exists.

**Descriptor fetch stays separate from decode** (R11, via GEOM.MESHFETCH):
MESHFETCH builds against the 64-byte-aligned versioned descriptor and the raw
format first, and culls only outside **every** active camera — a vertex outside
one Duo view may be inside the other.

### The ruling that unblocked this (owner, 2026-08-31 §6.2)

> Write the contract and land RAW/CANONICAL format 0 first. Packed rigid format
> 1 and two-weight skinned format 2 are additive formats chosen after an asset
> bake-off. **Do not block the geometry path on perfect compression.**

That last sentence is the design. This block previously could not be specified
because "the compressed vertex FORMAT is not pinned anywhere in `spec/`" — and
the ruling's answer is not to pin one, it is to **make format 0 uncompressed**
so the geometry path can be built and measured while compression is still an
open question.

**Format 0 is therefore not a placeholder.** It is the permanent fallback and
the differential reference: every later format must decode to bit-identical
output for the same source mesh, and format 0 is what "the same" is measured
against.

**Exclusions.** This block does not skin, does not project, does not fetch
descriptors (that is `GEOM.MESHFETCH`, which hands it offsets) and does not
choose LOD. It turns bytes into vertices.

## Clock and reset semantics
Single `gpu_clk`, synchronous active-low `rst_n`. No CDC.

Reset abandons the batch in flight and discards late memory responses **by
tag**, not by counting — the same rule as `GEOM.MESHFETCH`, and for the same
reason: a response that outlives its requester is precisely the case a counter
gets wrong.

A partially decoded batch is never emitted. Vertices leave in whole batches or
not at all, so a reset mid-batch cannot deliver a torn mesh downstream.

## Input and output packet layouts
### In

`{ vertex_offset, vertex_count, format_id, flags, mesh_id, src_id }` — the
offsets `GEOM.MESHFETCH` emitted, unchanged. This block does not re-read the
descriptor.

### Out — the format `GEOM.SKIN` already takes, and it is not negotiable

`zhao_geom_skin` is built, verified and fitted at 89.65 MHz, and its ports are
the target:

| field | width | note |
|---|---|---|
| `v_x/y/z` | s32 each | fx16 S15.16 |
| `v_w0` | 7 bits | **1/64 quanta; 64 == rigid** |
| `v_rigid` | 1 bit | `b1 == b0`, decided upstream |
| `v_src_id` | 16 bits | propagated for attribution |

**`GEOM.SKIN` outputs positions, NOT normals** (`reports/CREATURESANDLIGHTS`),
and nothing may be bolted onto its output. So normals decoded here travel a
parallel path to lighting; they do not enter the skin datapath.

### Format 0 — RAW / CANONICAL, frozen

32 bytes per vertex, naturally aligned, no bit-packing:

| off | size | field |
|---|---|---|
| 0 | 12 | position s32×3, fx16 |
| 12 | 3 | normal s8×3, the packed bind normal the cel path already uses |
| 15 | 1 | `w0` in 1/64 quanta (64 = rigid) |
| 16 | 4 | UV, 2× fx16 s16 |
| 20 | 2 | `bone0` |
| 22 | 2 | `bone1` |
| 24 | 8 | reserved, **must be zero** |

The reserved 8 bytes are what formats 1 and 2 will grow into without changing
the stride, and requiring them zero is what stops an older decoder silently
reading a newer file.

## Backpressure rules
Ready/valid both sides, house hygiene: `in_ready_o` never depends on
`in_valid_i`; `out_valid_o` never depends on `out_ready_i`.

Two backpressure sources again — memory grant and the consumer — and the same
anti-deadlock rule: an issued read is always retired into staging before another
is issued, so response progress never depends on the consumer.

Format 0 is a pure stride read, so the memory pattern is sequential and
prefetchable. **That is a reason to land it first that has nothing to do with
compression:** it makes the memory behaviour of the whole geometry path
measurable before a variable-rate decoder is introduced.

## Memory ownership
Reads only, through `MEM.GUARD`, as an ordinary client. Never writes.

It owns **no** descriptor state; `GEOM.MESHFETCH` already validated the
descriptor and handed over offsets. It owns only the staging for the batch in
flight.

Vertex-stream residency is the caller's problem. No cache in v1 — whether one
pays is a Class-B evidence question against a real 256-creature trace, and
assuming it into the first build is how an unmeasured structure becomes
permanent.

## Q formats and rounding
**Format 0 performs NO arithmetic.** Position is already fx16 S15.16, `w0` is
already 1/64 quanta, UV is already fx16. The block widens, it does not convert.

That is the point of landing it first: a decoder that computes nothing cannot
introduce a rounding law, so any later disagreement between format 0 and format
1 or 2 is unambiguously the *compressed* format's error.

Formats 1 and 2 will introduce rounding, and when they do the rule is the
project's existing one: the rounding is declared in this section, generated
where it is a table, and differentially proved against format 0 — never
inferred from a decoder implementation.

`w0 == 64` means rigid, and `v_rigid` is set from it upstream. A decoder that
emitted `w0 == 64` *and* `rigid == 0` would be internally inconsistent; the
refusal table below makes it an error rather than a surprise in `GEOM.SKIN`.

## Latency (fixed or variable)
**Variable**, dominated by the memory path rather than by decode.

Format 0 decode itself is **fixed, 1 clock** — a widen and a re-field, no
arithmetic. Everything variable about this block is `MEM.GUARD`'s latency and
the burst pattern, which is why the contract fixes the decode and refuses to
quote an end-to-end number until the memory path is measured.

Formats 1 and 2 may take more than one clock per vertex. If they do, the
architecture rule binds: *latency may grow; initiation rate and exact arithmetic
may not regress.*

## Target throughput
**One decoded vertex per clock** (ledger `target_throughput`), sustained, for
format 0.

Checked against the ruling's content tier rather than asserted: 256 creatures at
~1,930 vertices is ~494,000 vertices, or ~494,000 clocks — **37 % of a
1,333,333-clock frame** if every creature were at full mesh.

That number is uncomfortable and it is supposed to be. It is the strongest
argument in the tree for the LOD ladder actually being used, because the same
tier at reduced representation is a small fraction of it. The ruling is explicit
that 256 creatures does **not** promise 256 hero meshes — this is the arithmetic
that shows why.

At 32 B/vertex, format 0 also costs ~15.8 MB/frame of vertex traffic at full
mesh. **That is the number that will decide whether formats 1 and 2 are
required**, and it should be measured on a real trace before the bake-off, not
after.

## Overflow and malformed-input behaviour
Refuse, never guess. Each raises a refusal naming `mesh_id` and `src_id`:

| condition | why |
|---|---|
| `format_id` unknown | a newer format read as this one yields plausible garbage |
| any reserved byte nonzero | a newer field is in use by an older reader |
| `w0 > 64` | outside the 1/64 quanta domain `GEOM.SKIN` accepts |
| `w0 == 64` with `rigid == 0` | internally inconsistent; would surprise `GEOM.SKIN` |
| `bone0`/`bone1` ≥ the pose palette's bone count | would index outside the palette |
| `vertex_count == 0` | an empty batch is a caller bug, not a no-op |

A refusal drops the **batch**, not the frame — consistent with
`GEOM.MESHFETCH`. Capacity exhaustion is the separate hard-overflow law.

## Counters and traces
* `vertices_decoded` (ledger)
* `batches_decoded`, `batches_refused_by_reason[6]`
* `vertex_bytes_read` — the number that decides whether compression is needed
* `format_histogram[3]` — which formats a real frame actually used
* `mem_stall_cycles`, `consumer_stall_cycles` — counted separately, because
  "slow" is not actionable when there are two independent causes

Source ids propagate so a refused batch is attributable to the command that
introduced the instance.

## Scalar reference function
**The RECORD law:** `zref::geom::vdecode0`
(`reference/include/zref/zref_geom.hpp`), which decodes one 32-byte format-0
record, plus `zref::geom::vdecode0_w0_legal`.

**The BATCH engine's oracle is not yet written.** This section used to cite
`zref::VertexDecode` with an entry point `decode(bytes, format_id, count)`, and
no such symbol has ever existed -- a phantom citation the ledger's V17 caught
once this block gained real evidence to check it against.

It owns the byte-level layout and the refusal taxonomy, and nothing else —
format 0 computes nothing, so there is no arithmetic law here to own yet.

When formats 1 and 2 arrive, this same oracle gains them, and the differential
is **format N against format 0 on the same source mesh**, required to be
bit-identical in position and within a declared bound in normal and UV. That
comparison is only meaningful because format 0 is exact.

## Directed tests
`tests/geometry/geom_vdecode_directed.cpp` covers the RECORD law:
field-by-field against `zref::geom::vdecode0` over 1,500 random records, field
isolation byte by byte, `rigid == (bone1 == bone0)`, and each refusal reason
emitting NO vertex.

The batch-level cases below are **planned and not yet written** -- they need
the batch engine, which does not exist:

* every field at its extremes — position at ±32767.99998, `w0` at 0, 1, 63, 64,
  bone indices at 0 and max;
* `w0 == 64` with `rigid == 1` accepted, with `rigid == 0` refused;
* each of the 8 reserved bytes nonzero in turn: 8 refusals;
* unknown `format_id`: refused, and **no vertex emitted** — the safety case;
* `vertex_count == 0`: refused;
* a batch straddling a burst boundary, which is where a stride decoder's
  addressing is most likely wrong;
* **round trip**: a known mesh encoded to format 0 and decoded back must equal
  the source exactly. With no arithmetic in the path this is an equality
  assertion, not a tolerance — and if it ever needs a tolerance, something has
  started computing that should not.

## Randomized differential tests
**PLANNED, not written.** `tests/geometry/geom_vdecode_random.cpp`, RTL against
`zref::geom::vdecode0`.

Random meshes with random valid batches, plus a deliberate corruption fraction,
with the generator **reporting its refusal mix** so a change that makes
corruption unreachable shows as a shift rather than as silent lost coverage —
the same requirement placed on `GEOM.MESHFETCH`.

Mutation sweep expected to the standard the geometry blocks already meet
(`zhao_geom_lod` 26/25/1 proved, `zhao_geom_cull` 32/30/2 proved).

## Formal properties
**PLANNED, not written.** `tests/formal/geom_vdecode_batch.sby`:

* **no partial batch** — a batch either emits `vertex_count` vertices or none.
  This is the safety property: a torn batch would reach `GEOM.SKIN` as a mesh
  with missing vertices, which renders as geometry rather than as an error;
* **no emission after refusal**, for every refusal reason;
* handshake hygiene on both channels;
* every accepted batch produces exactly one completion or one refusal, never
  both and never neither, under arbitrary backpressure on memory and consumer;
* reset drops in flight, and no pre-reset vertex is ever emitted after.

## Synthesis / resource ceiling
Unbuilt. **Ceiling for format 0: 600 ALMs, 0 DSPs, 0 M10K.**

Zero DSPs is a real constraint, not an estimate: format 0 performs no
arithmetic, so a DSP appearing in its fit means something is computing that this
contract says does not compute. Zero M10K likewise — staging is a small register
file, and an inferred RAM would mean the batch buffer grew past its bound.

Formats 1 and 2 get their own ceilings when the bake-off measures them; the
ruling names decoder ALM/DSP/Fmax as one of its five comparison axes.

## Integration capture cases
* **256 creatures at mixed LOD** — the ruling's tier. Measures
  `vertex_bytes_read` against real bandwidth, which is the evidence that decides
  whether compression is needed at all. Capture it before the bake-off, not
  after.
* **one creature at full mesh, held** — the worst per-instance case, and the
  trace that tells us whether one vertex per clock is really sustained.
* **a corrupt batch mid-frame** — the batch drops, the frame completes, the
  counter attributes it. Explicitly not a frame fault.
* **the bake-off corpus** — Zixxtrixx plus ten structurally different creatures,
  compared on silhouette error, cel-band flips, normal angular error, bytes per
  vertex, and decoder ALM/DSP/Fmax. Those five axes are the ruling's, and
  cel-band flips is the one a generic mesh-compression study would omit: a
  normal error too small to see on a smooth surface can still move a toon band
  edge, which is a visible line on the creature.

## Notes

Compression format owned by SW.TOOLS.ASSET (pack side) — one spec, two ends.
