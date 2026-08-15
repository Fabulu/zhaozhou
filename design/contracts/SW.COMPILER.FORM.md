# Contract — SW.COMPILER.FORM (Form compiler)

> Ledger: `design/blocks.yml` · owner ZH-019 · phase 1 · maturity SPECIFIED

## Purpose and exclusions

Form language frontend → HIR → ZIR → backends (C++/ZDL); emits Field IR
programs via SW.FIELDIR.

Wave-3 scope (plan W3.2/W3.3, this contract's active surface): the L1
frontend (lexer, recursive-descent+Pratt parser, type checker with domain
effects and the FORM-E-nnn diagnostics) and the HIR/ZIR partitioner,
compile-time scheduler and C++17 emitter. The language law is
`spec/form/language-semantics.md` (L1 v1), the effect law is
`spec/form/domains-and-effects.md`, the schedule law is
`spec/form/deterministic-scheduling.md`, the cost/source-map artifacts are
`spec/form/cost-model.md` §2 and `spec/capture_format.md` §7. Excluded: L2+
profiles (`@warp`/`@formation`/`@stamp`), forms/ladders/populations (L3),
terrain materials (L4), production tooling (L5) — all refused with codes,
never parsed (language-semantics §8).

## Input and output packet layouts

Inputs: Form source set (`*.form`, one module per file, UTF-8; lexical law
language-semantics §1) plus the cartridge page table for page-id const
resolution (spec/cartridge.md §3). No binary inputs.

Outputs (all byte-stable, deterministic, LF, version-banner — plan D4):

1. Generated C++17: one `.hpp`/`.cpp` pair per module under
   `<out>/generated/form/<module>.{hpp,cpp}`, `namespace form::<module>`;
   cartridge-wide `form_game.hpp` exposing `struct FormState` (SoA pools +
   globals + RNG states), `void sim_tick(FormState&, const PadFrame[4],
   u32 tick)`, `void present_frame(const FormState&, zref::FrameBuilder&)`,
   `u32 sim_hash(const FormState&, u32 prev)` (D5 chain law), and one entry
   per scenario. Links ZRef runtime only (fixp/trig, zfield interpreter,
   zhao_abi.h builder, .zcap writer). No `<cmath>`, no float/double token
   (grep-audited).
2. Field programs via the frozen builder: `.zprog` + C++ wrapper + `.zvec`
   goldens per `@earth`/`@flow` declaration (field-ir.md §5/§11 — unchanged
   pipeline, W3.4 lowers the dialect onto it).
3. `sourceids.zmap` (binary, magic ZSMP — capture_format §7) and
   `costs.zcost` (canonical JSON — cost-model §2).
4. Structured diagnostics `{file, span, code "FORM-E-nnn", message}`
   (collected, never thrown; non-zero exit on any; no partial emission).

## Backpressure rules

Backpressure: `none`. Batch tool; consumers read completed files only (the
no-partial-emission law guarantees a consumer never sees a half-written
output tree).

## Memory ownership

Tool process owns everything; zero shared state with targets. Generated code
owns its `FormState` (fixed-capacity arrays sized from pool capacity
literals — no allocation in the tick path, FORM §21-5); frame arenas are
the ZRef builder's. The compiler never emits dynamic allocation (refused in
L1, FORM-E-719).

## Q formats and rounding

Every generated numeric operation cites `spec/qformats.md`: fx16 (Q16.16)
core, fx24 (s64) sim-truth accumulators — never in field programs (Q2);
angle16 wraps mod 2^16; unit8 `unit_mul`; single rounding via `rescale()`
only (charter §29-7). Conversions are named intrinsics (language-semantics
§4.6) so every rounding site is greppable; saturation events count in the
SatLedger (qformats §5). Field dialect: Q16.16 lanes only, op semantics
frozen in field-ir.md §3.

## Latency (fixed or variable)

Latency: `variable` (batch compile; wall time is not part of any contract).

## Target throughput

Target throughput: batch. `form:gen`/`form:check` run per commit; the
checker (byte-identity regen + diff) is the gate.

## Overflow and malformed-input behaviour

Malformed source: structured FORM-E-nnn diagnostics (language-semantics §7),
all collected before exit; zero outputs on any error (no partial emission).
Overflow: fixed-point ops saturate and record (qformats §2); pool spawn past
capacity is a deterministic runtime abort (FORM-E-821); the one-writer and
purity rules are compile-time refusals (FORM-E-500/405). A program whose
lowered field body exceeds `max_ops`/ceiling never emits (FORM-E-654/655).

## Directed tests

`compiler/test/` (W3.2/W3.3, labels fast): lexer span/CRLF cases; parser
goldens (AST byte-stable, committed); **one test per FORM-E code**
(language-semantics §7 — the W3.2 acceptance gate); D1 OUT-features rejected
with the exact codes (E-700..719); one-writer conflict cites both spans;
present-purity negative (write in presentation rejected); schedule goldens
(pure function of declaration order + deps; multi-rate + stagger); emitter
byte-stability (run twice, diff zero); grep-audit no-float/no-cmath/
no-host-clock over `<out>/generated/form/**`.

## Randomized differential tests

Field dialect: every `.zvec` differential is TS↔C++ byte-identity over
random vectors (wave-1 discipline, field-ir §6; W3.4 keeps it). Frontend:
fuzz lexer/parser over generated token streams — crash-safety + diagnostic
completeness only (no crash may bypass the diagnostic collector).

## Integration capture cases

W3.7 e2e: the Wound Lab Form program compiles → 600-tick sim-hash chain
golden (D5), per-frame CRCs, `sourceids.zmap` + `costs.zcost` committed and
schema-validated; the negative capture — a presentation block attempting a
truth write is rejected at compile time (Phase-3 gate: truth/presentation
enforced by compiler checks).

## Notes

Phase-1-active software block (wave-1 stub → wave-3 L1 surface). Field IR
emission goes through SW.FIELDIR's frozen builder; `spec/form/field-ir.md`
is never edited. Maturity target UNIT_VERIFIED at wave-3 gate (plan D12)
with commit-pinned evidence.
