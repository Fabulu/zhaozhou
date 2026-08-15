# Form Language Semantics — L1 v1

**Status:** Phase 1 stub expanded to **L1 v1** (wave 3, plan W3.1, decision D1;
charter §23 Phase 3; FORM_LANGUAGE_HARDWARE_CODESIGN.md §4–§13 illustrations,
§18 L1, §20, §22). This file is the grammar law the W3.2 frontend implements:
every rule here is testable, every refusal carries a `FORM-E-nnn` code, and
W3.2 exercises **every code** (plan W3.2 acceptance). `spec/form/field-ir.md`
is FROZEN and referenced, never redefined: everything inside a `field`
declaration lowers onto the existing FieldBuilder (D3 boundary).

**Phase-1 frozen content retained** (was §2 of the stub, still law): the
builder API was the Phase-1 frontend surface; `fx16` is Q16.16 per
`spec/qformats.md` §2; determinism, branchlessness and source identity are
unchanged. The parser arrives in W3.2; this document is its specification.

**Design law:** one language, several explicit execution domains (FORM §2);
truth and form separated by compiler checks (charter §29-8); no host floats in
deterministic paths — the qformats single-rounding law only (charter §29-7);
the language is deliberately small — OUT-list features are **refused with
error codes, not deferred silently** (plan D1/R3).

---

## 1. Lexical rules

A source file is UTF-8. One module per file; the file name SHOULD be
`<module_name>.form` (lowercase; not enforced — FORM-E-2xx name rules are).

1. **Whitespace:** space, tab, CR, LF, CRLF. The lexer is CRLF-tolerant and
   byte-offset-based: spans measure bytes from file start; `\r\n` counts as
   two bytes and neither is part of any token (plan W3.2).
2. **Comments:** `//` to end of line; `/* ... */` block comments nest and may
   span lines. Comments are not tokens.
3. **Identifiers:** `[A-Za-z_][A-Za-z0-9_]*`, byte length 1–64. Keywords
   (§1.1) are reserved. By convention types are `lower_snake_case` with a
   leading letter (e.g. `creature`), pools/globals `snake_case`, constants
   `SCREAMING_SNAKE_CASE`; convention violations are warnings, never errors
   (warnings never affect semantics — D2).
4. **Integer literals:** decimal `123`, `0`-prefixed-octal is NOT allowed
   (a leading `0` must be a lone zero), hex `0x5A17` (`0X` refused). Range
   checked against the literal's target type (§1.2).
5. **Fractional literals** are Q-format values with an optional unit suffix:
   `1.5`, `1.5m` (fx16 metres), `2.25w` (fx24), `0.65px` (fx16 pixels —
   sugar, there is no distinct pixel type in L1), `0.25turn` / `90deg`
   (angle16), `45%` (unit8, `raw = round(45 · 256 / 100)`). Fractional digits
   beyond the Q format's precision are a compile error unless the value is
   exactly representable (`0.5000001m` errors; `0.5m` is exact).
6. **Tick literals:** `120t` — u32 tick count (a tick type is not a distinct
   L1 type; the suffix types the literal `u32` and documents intent).
7. **Colour literals:** `#RRGGBB` / `#AARRGGBB` (hex, 6 or 8 digits) →
   `colour8` (ARGB8888 working set, qformats.md §2).
8. **Strings:** `"..."` — no escapes beyond `\\` and `\"`; byte length 1–256;
   appear ONLY in `import` paths, `sound.sample` references, `scenario.load`
   targets and `capture ... as` names. There is **no string type** (§8 OUT).
9. **Punctuation:** `{ } ( ) [ ] , ; : . :: -> .. = == != < <= > >= + - * /
   && || ! ~ & | ^ << >> % @ #` (the `%` of `45%` is part of the literal; the
   standalone `%` operator does not exist).

### 1.1 Keywords (reserved, L1)

```
module import const enum struct pool global system every tick ticks stagger
over reads writes fn let return for in if else true false random stream
field footprint max_ops none rect circle capsule presentation view from
budget shared emit draw_form draw_population draw_procedural surface_stamp
sound audio scenario seed load spawn player at assert capture as
input params sample material nav_cost height velocity age phase dt attr
build warp formation stamp (domain keywords — refused in L1, §8)
macro class interface extern f32 f64 float double string while break
continue (reserved-forever; use is FORM-E-701..712)
```

Domain keywords `build warp formation stamp` are lexed as keywords so that
`@warp`-style declarations fail as *refusals* (FORM-E-720s), not as unknown
syntax. `earth` and `flow` are the two admitted profile annotations (D1).

### 1.2 Literal typing (target-typed)

Literals are typed by context; the checker range-checks against the target:

| Literal | Type | Law |
|---|---|---|
| `123`, `0x5A17` | `i32` or `u32` by context | must fit target (qformats saturate law does NOT apply to literals) |
| `1.5`, `1.5m`, `0.65px` | `fx16` | Q16.16, must be exact |
| `2.25w` | `fx24` | Q1.39.24, must be exact |
| `0.25turn`, `90deg` | `angle16` | U 0.0.16 turns; wraps mod 1 |
| `45%` | `unit8` | U 0.0.8; `raw = (pct·256+50)/100`, saturate 255 |
| `120t` | `u32` | tick count |
| `#3060A0` | `colour8` | ARGB8888 (alpha FF when 6-digit) |
| `true`, `false` | `bool` | — |

---

## 2. Grammar (EBNF, L1)

Canonical EBNF: `A = ... ;`, `{A}` = zero or more, `[A]` = optional, `|`
`alternatives`, terminals quoted. The W3.2 parser is hand-written
recursive-descent + Pratt for expressions (D2); this grammar is its contract.
Every AST node carries a `SourceSpan` — byte offsets `{file, start, end}` —
which lowers unchanged into the Field IR builder's `SourceSpan` (D2/D3;
field-ir.md §8 uses `{source_id, line, col}` derived from the same site).

```ebnf
module        = "module" ident "{" { import_decl | top_decl } "}" ;

import_decl   = "import" ident [ "{" ident { "," ident } "}" ] ";" ;

top_decl      = const_decl | enum_decl | struct_decl | pool_decl
              | global_decl | system_decl | fn_decl | field_decl
              | presentation_decl | scenario_decl | sound_decl ;

const_decl    = "const" IDENT ":" type "=" const_expr ";" ;
enum_decl     = "enum" ident "{" ident [ "=" int ] { "," ident [ "=" int ] } [","] "}" ;
struct_decl   = "struct" ident "{" { field_decl_typed } "}" ;
field_decl_typed = ident ":" type ";" ;
pool_decl     = "pool" ident ":" ident "[" capacity "]" ";" ;
capacity      = int | IDENT ;                       (* const u32 > 0, or literal *)
global_decl   = "global" ident ":" type "=" const_expr ";" ;

fn_decl       = "fn" ident "(" [ params ] ")" "->" type "{" { stmt } "}" ;
params        = ident ":" type { "," ident ":" type } ;

system_decl   = "system" ident "every" int ( "tick" | "ticks" )
                [ "stagger" "over" ident ]
                "reads" access { "," access }
                "writes" access { "," access }
                "{" { stmt } "}" ;

field_decl    = "@" ( "earth" | "flow" ) "field" ident
                "(" [ "params" ":" ident ] ")"
                "->" ( "terrain_delta" | "flow_update" )
                footprint "max_ops" int
                "{" { field_let } field_return "}" ;
footprint     = "footprint" ( "none"
                             | "rect" "(" expr "," expr "," expr "," expr ")"
                             | "circle" "(" expr "," expr "," expr ")"
                             | "capsule" "(" expr "," expr "," expr "," expr ")"
                             ) ";" ;
field_let     = "let" ident "=" field_expr ";" ;
field_return  = "return" ( "terrain_delta" | "flow_update" ) record_lit ";" ;

presentation_decl = "presentation" ident
                    "{" { view_item | emit_stmt } "}" ;
view_item     = "view" int "from" expr "budget" int "%" ";"
              | "shared" "budget" int "%" ";" ;
emit_stmt     = "emit" emit_kind emit_args ";" ;    (* §5 *)

scenario_decl = "scenario" ident "{" { scenario_item } "}" ;
scenario_item = "seed" int ";"
              | "load" ident ";"
              | "spawn" "player" int "at" ident ";"
              | "at" int ( "tick" | "ticks" ) scenario_action
              | "assert" scenario_assert ";"
              | "capture" "frame" int "as" string ";"
              | "assert_budget" ident ";" ;

sound_decl    = "sound" ident "{" "sample" string ";"
                 { tone_param } "}" ;
tone_param    = ( "gain" | "pitch" | "pan" ) const_expr ";" ;
```

### 2.1 Statements (system and function bodies)

```ebnf
stmt          = let_stmt | assign_stmt | if_stmt | for_stmt
              | call_stmt | spawn_stmt | kill_stmt | return_stmt ;
let_stmt      = "let" ident [ ":" type ] "=" expr ";" ;   (* single assignment *)
assign_stmt   = lvalue "=" expr ";" ;
lvalue        = ( ident | access ) { "." ident | "[" expr "]" } ;
if_stmt       = "if" expr block [ "else" ( if_stmt | block ) ] ;
for_stmt      = "for" ident "in" range block ;            (* §4.4 *)
range         = expr ".." expr
              | ident ;                                    (* pool sugar *)
call_stmt     = ident "(" [ args ] ")" ";" ;
spawn_stmt    = "spawn" "(" ident "," record_lit ")" ";" ;
kill_stmt     = "kill" "(" ident "," expr ")" ";" ;
return_stmt   = "return" [ expr ] ";" ;
block         = "{" { stmt } "}" ;
```

Single assignment (D1): a `let`-bound local may be assigned **exactly once**;
re-assignment or use-before-assignment is FORM-E-401/402. State writes
(`lvalue` targeting a pool component or global) are ordinary assignments and
must appear inside a system that declared the write (§effects,
domains-and-effects.md).

### 2.2 Expressions (Pratt precedence, loosest to tightest)

```ebnf
expr          = if_expr | binary ;
if_expr       = "if" expr block "else" ( if_expr | block ) ;  (* select-expression *)
binary        = logic_or ;
logic_or      = logic_and { "||" logic_and } ;
logic_and     = bitor { "&&" bitor } ;
bitor         = bitxor { "|" bitxor } ;
bitxor        = bitand { "^" bitand } ;
bitand        = shift { "&" shift } ;
shift         = equality { ( "<<" | ">>" ) equality } ;
equality      = comparison { ( "==" | "!=" ) comparison } ;
comparison    = range_term { ( "<" | "<=" | ">" | ">=" ) range_term } ;  (* non-assoc *)
range_term    = term [ ".." term ] ;                    (* only in `for` *)
term          = factor { ( "+" | "-" ) factor } ;
factor        = unary { ( "*" | "/" ) unary } ;
unary         = ( "-" | "!" | "~" ) unary | postfix ;
postfix       = primary { "." ident | "(" [ args ] ")" | "[" expr "]" } ;
primary       = literal | ident | "(" expr ")" | record_lit | intrinsic ;
record_lit    = ident "{" { ident "=" expr "," } [ ident "=" expr ] "}" ;
intrinsic     = "input" "." "player" "(" expr ")"
              | "random" "." ( "stream" | "u32" | "i32" | "fx16" | "unit8" | "angle16" ) ...
              | builtin_call ;                          (* §4.6 table *)
```

`if` is the **select-expression** (D1): both branches are always evaluated
(no short-circuit — branchless lowering, identical cost both sides);
`&&`/`||` likewise evaluate both operands. This is the language-level mirror
of the Field IR SELECT/CMP pair and removes all conditional-timing
nondeterminism.

---

## 3. Type system

Static, nominal, AOT (FORM §21-2). No inference beyond literal target-typing
and `let` with omitted type (inferred from initializer exactly).

### 3.1 Scalar types (D1; authority: spec/qformats.md §2)

| Type | Storage | Notes |
|---|---|---|
| `fx16` | s32, S 1.15.16 | core scalar; saturating add/sub; single-rounded mul |
| `fx24` | s64, S 1.39.24 | sim-truth accumulator; **never in field programs** (Q2) |
| `angle16` | u16, U 0.0.16 turns | wrapping; sin/cos via qformats §7.1 |
| `unit8` | u8, U 0.0.8 | weights/envelopes; `unit_mul` law |
| `i32` / `u32` | s32 / u32 | counts, ids, bitsets |
| `bool` | u8 stored, `0`/`1` only | comparisons produce bool |

### 3.2 Composite and space-typed types (D1; "space-typing where practical")

| Type | Shape | Law |
|---|---|---|
| `world2` | 2 × fx24 | world-space truth point/vector |
| `world3` | 3 × fx24 | world-space truth |
| `velocity3` | 3 × fx24 | **cannot mix with world3** in +,- without explicit conversion (FORM-E-330) |
| `colour8` | 4 × u8 | ARGB8888 working |
| `T[N]` | fixed array, `N` const | index bounds checked at compile time where provable, else runtime-abort (deterministic, FORM-E-820 family documented in §7) |
| enums | u32 backing | members only |
| structs | nominal, ordered fields | field set frozen at declaration |

Conversions are **explicit** (`x: fx24 as fx16` syntax? no — an intrinsic
`convert<T>(x)` is NOT provided; each conversion is a named intrinsic from
§4.6, e.g. `to_fx16(x)`, `to_unit8(x)`) so every rounding site is greppable
(qformats §4 `rescale()` is the one rounding primitive; each named conversion
cites its row of the qformats §2 table). Implicit conversions: **none**
except literal target-typing (§1.2). Mixed-precision arithmetic is refused
(FORM-E-331): both operands of a binary operator must have the same type.

Space-typing where practical (FORM §5): `world2/3` and `velocity3` are
distinct types; `+` refuses mixed operands; the field dialect uses fx16 lanes
only, and its `sample.x/z` are fx16 (Field IR lanes are Q16.16 everywhere —
field-ir.md §7.1/Q2).

### 3.3 Effect-free by construction outside systems

`fn` bodies are pure: no pool/global writes, no spawn/kill, no input reads
(FORM-E-450 when violated). Systems are the only writers. Presentation blocks
are pure *readers* that emit (domains-and-effects.md §2).

---

## 4. Declarations and scoping

### 4.1 Modules and imports

One `module` per file; names are globally unique across a compilation (the
cartridge is one program). `import m;` brings `m`'s public names in as
`m.name`; `import m { f, g };` brings selected names in unqualified.
Import cycles are refused (FORM-E-204). Duplicate top-level names in one
module: FORM-E-201. Unknown import: FORM-E-202; unknown imported name:
FORM-E-203.

### 4.2 Const, enum, struct

`const` initializers are constant expressions: literals, consts, enum
members, arithmetic over these (FORM-E-210 when non-const). Enums: u32
backing, explicit values ascending and unique (FORM-E-211/212). Structs:
fields may be scalars, vectors, enums, structs, `T[N]` — not pools, not
functions (FORM-E-213/714); no recursive struct types (FORM-E-214, bounded
memory law FORM §6).

### 4.3 Pools and globals

`pool creatures: creature[512]` — fixed capacity, compile-time visible
(FORM §21-9; capacity literal or `u32` const; `0` refused FORM-E-810).
Pool element must be a struct (FORM-E-811). Pools are SoA-laid-out in the
generated `FormState` (D4); iteration is ascending over `0..count`
(§5, deterministic-scheduling.md). Pool intrinsics: `.count` (u32),
`alive(pool, i)`, `spawn(pool, value) -> u32`, `kill(pool, i)` (§4.5).

`global` is explicit persistent truth state (FORM §6 "explicit persistent
state"; required by D4 `FormState` and the D5 sim-hash serialization).
Initializer must be a constant expression (FORM-E-210). Globals participate
in reads/writes access lists like pool components.

### 4.4 For loops (bounded, ascending only)

`for i in a..b` — `a`, `b` are u32-typed expressions; the loop runs
`max(0, b − a)` times visiting `a, a+1, ..` — **ascending always** (D6).
A compile-time-provable `a > b` is refused (FORM-E-501). A descending range
form does not exist and `while`/`break`/`continue` are reserved-forever
keywords (FORM-E-712/713). Trip counts must be statically bounded: `b − a`
must reduce to a constant or `pool.count`-of-known-capacity expression
(FORM-E-502) — this is the "no unbounded loops in sim/present" law (FORM §6).

`for c in creatures` is sugar for an ascending index loop over the pool's
dense range with `c` bound to `creatures[i]` (an explicit index sugar binding
`c_index` is NOT automatic; use the `a..b` form when you need the index).
Mutating pool membership (spawn/kill) inside a pool-sugar loop is refused
(FORM-E-503); explicit-index loops may spawn (appends are beyond the
snapshot bound) but not kill (§4.5).

### 4.5 Pool membership laws (deterministic)

- `spawn` appends at `count`; `count == capacity` at spawn is a deterministic
  runtime abort (FORM-E-821, emulator/tool diagnose; the generated code calls
  `form_abort(FORM_E_821_POOL_OVERFLOW)` — never silently drops).
- `kill(pool, i)` marks dead; the compiler inserts one **stable compaction**
  pass at system end (ascending sweep, relative order preserved). Dense
  index order after compaction is therefore a pure function of the operation
  sequence — the D5 hash serialization law holds.
- Access `pool.field[i]` with `i >= count` at runtime: deterministic abort
  (FORM-E-822). Compile-time-provable OOB (constant index, `i` from a
  smaller pool's range): compile error FORM-E-820.

### 4.6 Built-in intrinsics

| Intrinsic | Domain | Type | Law |
|---|---|---|---|
| `input.player(n) -> PadFrame` | sim | n: u32 0..3 | the ABI `PadFrame` (commands.zidl); absent pads per input_rules §2.2 |
| `input.held(p, b) -> bool` | sim | b: u32 bit index | `(p.buttons >> b) & 1`; button names via `input_rules` §4 bits (`BTN_UP`..`BTN_START` built-in consts) |
| `random.stream(seed, id...) -> stream` | sim, present, test | ids: u32 | derived RNG (FORM §5); PCG per qformats §7.5; no global generator |
| `random.u32(s) -> u32` (also `i32`, `fx16(lo,hi)`, `unit8`, `angle16`) | sim, present, test | s: stream | each draw advances `s`; streams are values |
| `abs/min/max/clamp(x…) -> T` | all | numeric | saturating per qformats §2 |
| `sin(a)/cos(a) -> fx16` | all | a: angle16 | qformats §7.1 table |
| `atan2_approx(y,x) -> angle16` | sim | fx16 operands | qformats §7 |
| `sqrt_approx(x) -> fx16` | sim | — | `isqrt_u32` basis, qformats §7.2 |
| `to_fx16(x) / to_fx24(x) / to_unit8(x) / to_angle16(x)` | all | one rounding each | qformats §2 conversion rows; saturation counted (SatLedger §5) |
| `dot2/dot3(a, b) -> fx24` | sim | world/velocity vectors | exact sum, one rescale (field dialect has its own fx16 forms §6) |
| `length(a) / normalize(a) -> …` | sim | vectors | approx per qformats §7.4 |
| `terrain.height(x: world2) -> fx16` | sim | reads terrain truth | sample at column |
| `mix(a, b, t: unit8) -> T` | sim, present | lerp with unit weight | round-half-up |

Terrain truth access (`terrain.height`) is a **read** of the `terrain`
component and must be declared in the system's reads list (domains-and-effects
§3). There is no L1 terrain-mutation statement — terrain truth changes only
through applying an `@earth` field program (§6.4, FORM-E-460 otherwise).

---

## 5. Presentation blocks and the emit vocabulary

`presentation` declarations are **pure** (domains-and-effects.md §2): they
read truth state and emit semantic commands; they never mutate truth
(FORM §13). A presentation block has two parts: optional **view layout**
(the Duo contract, FORM §12) and a sequence of **emit statements**, each of
which becomes exactly one ABI v3 command template in PresentZIR (D3) with the
emit site's source ID (kind 6, capture_format.md §5):

```ebnf
emit_kind      = "draw_form" | "draw_population" | "draw_procedural"
               | "surface_stamp" | "audio" ;
emit_args      = "(" [ emit_arg { "," emit_arg } ] ")" ;
emit_arg       = ident ":" expr ;
```

| Emit statement | ABI command | L1 arguments (lowered record fields) |
|---|---|---|
| `draw_form(form:, transform:, view_mask:, weight:)` | DrawForm 0x0300 | form = cartridge page-id const (u32); transform = world3 + fx16 size (marker/billboard law, commands.zidl flags b0); view_mask u8; weight u8 |
| `draw_population(pool:, view_mask:, weight:)` | DrawPopulation 0x0301 | pool = a declared pool; the renderer reads its SoA state through the generated-code manifest |
| `draw_procedural(patch:, transform:, screen_error:)` | DrawProcedural 0x0302 | patch = cartridge page-id const (u32, kind `forge_kind.heightfield_patch`); screen_error fx16 |
| `surface_stamp(brush:, at:, radius:, ring_width:, tag:, strength:)` | SurfaceStamp 0x0210 | brush = page-id const; circle/ring into the 64×64 sheet (D7) |
| `audio(sound:, at:)` | EmitAudioEvent 0x0400 | sound = a declared `sound` name; `at` selects the tone's pan/gain from the declaration |

Emit arguments must be pure expressions of truth state and constants. Emit
statements execute in source order **within a block**, but the compiler and
runtime are free to reorder opaque work across statements (FORM §13) — source
order is not a synchronization primitive; L1 defines no `layer`/`after`
constructs (L3+, FORM-E-717). Missing/unknown argument names: FORM-E-601/602.
Unknown emit kind: FORM-E-603.

`view` items compile to the frame's SetPresentationContract + SetView pair:
`view 0 from cameras[0] budget 45%` fixes viewport 0's camera binding and its
geometry/fragment token share; `shared budget 10%` the shared reserve (FORM
§12). At most two views in L1 (the Duo law; a third is FORM-E-604). The
budget split must sum to ≤ 100% (FORM-E-605).

Draw-arguments referencing cartridge resources use **page-id constants**
(`const ISLAND_PATCH: u32 = 3;`) resolved against the .zpak resource-page
manifest at pack time (spec/cartridge.md §5; unknown id: FORM-E-830 at pack).

### 5.1 Sound declarations (tone events only, D1)

```form
sound earth_break {
    sample "tone/earth_break.ztone";
    gain 80%;
    pitch 1.0;
    pan 0;
}
```

A `sound` declares a wave-2 mixer tone event: `sample` references a tone-bank
page in the cartridge; `gain` unit8; `pitch` fx16 multiplier; `pan` i16.
Sounds are **declaration-only** in L1 — no audio graph, envelopes, or
spatialisation grammar (L4; FORM-E-718). Audio failure never changes truth
(FORM §15); the EmitAudioEvent command is fire-and-forget into the mixer.

---

## 6. THE FIELD DIALECT (D3 boundary — the frozen FieldBuilder)

Everything inside an `@earth`/`@flow` `field` declaration is type-checked in
the **field dialect** and lowers **onto the existing frozen FieldBuilder**
(`compiler/src/field_ir/builder.ts`) → serialized `.zprog` + C++ wrapper +
`.zvec` goldens. `spec/form/field-ir.md` is not edited by this spec; where
this section cites records, lanes and ceilings, field-ir.md §7 is the
authority and wins any wording conflict.

### 6.1 Dialect laws (admission rules, each an error code)

1. **Branchless:** no `if` *statements* (the `if` select-expression is
   required — it lowers to CMP+SELECT); no loops; no calls (not even pure
   `fn`s); no early returns. (FORM-E-651/652/653.)
2. **Loopless and bounded:** the body is a straight-line sequence of
   `let`s and one `return`; the lowered instruction count must be
   `≤ max_ops ≤ profile ceiling` (field-ir.md §7.3: earth 32, flow 48,
   global 64). Violations: FORM-E-654 (declared max_ops above ceiling),
   FORM-E-655 (lowered count above declared max_ops).
3. **Record→record:** input is the profile's frozen input record; output is
   the profile's frozen output record (field-ir.md §7.1). No state access,
   no input, no random streams inside the dialect (FORM-E-656/657).
4. **Q16.16 only:** every lane and local is fx16 (s32 Q16.16) — `fx24`
   never appears (Q2; FORM-E-658). Literals obey §1.2 fx16 exactness.
5. **Register budget:** ≤ 64 live values (field-ir.md §11.2 — no spilling;
   FORM-E-659).
6. **Tables** (`curve`/`spline`/`dcurve`) are declared per program by the
   lowering with `{x,y}` pairs; table bytes count in the cost report
   (field-ir.md §9; costs travel in costs.zcost §cost-model.md).

### 6.2 Profile bindings (L1 admits earth and flow — D1)

In-scope names inside the body are exactly the profile's input lanes
(field-ir.md §7.1), bound as struct accesses, plus `params.<field>` for each
field of the params struct packed into the `p` lanes **in declaration
order**:

**earth** — `sample: { x: fx16, z: fx16, age: u32, phase: fx16 }`;
`params: <author struct, ≤ 8 fx16 fields>` (→ p0..p7);
returns `terrain_delta { height: fx16, velocity: fx16, material: u32,
nav_cost: fx16 }`.

**flow** — `p: { x, y, z, vx, vy, vz: fx16, age: u32, seed: u32,
dt: fx16 }`; `params: <author struct, ≤ 4 fx16 fields>` (→ p0..p3);
returns `flow_update { x, y, z, vx, vy, vz: fx16, attr0: fx16 }`.

Params-struct violations (non-fx16 field, too many fields, missing params
binding): FORM-E-660/661/662. A field-decl returning the wrong record type
for its profile: FORM-E-663.

The output record of `flow_update` maps back onto the emitting pool's SoA
columns by name convention: the pool's struct must contain fields named
`position: world3`, `velocity: velocity3`, `age: u32` (→ input lanes) and may
contain `representation: fx16` (← `attr0`); the pool is attached by a system
calling the field program per element (§6.4). Structs not matching the lane
mapping: FORM-E-664.

### 6.3 Dialect expression surface (exactly the builder, nothing more)

Available expression forms, each lowering to one builder call (op classes and
semantics frozen in field-ir.md §2/§3 — this table adds no semantics):

| Dialect form | Builder op(s) | Notes |
|---|---|---|
| `a + b`, `a - b`, `a * b` | ADD / SUB / MUL | saturating / single-rounded |
| `a * b + c` | MAD | fused when syntactically a·b+c |
| `min/max/abs/clamp(a,b,c)` | MIN/MAX/ABS/CLAMP | |
| `if c { a } else { b }` | CMP + SELECT | the only conditional |
| comparisons | CMP (imm mode) | produce fx16-as-bool 0/1, SELECT consumes |
| `dot2(a, b)` / `dot3(a, b)` | DOT2/DOT3 | fx16 lanes |
| `length2/length3(v)` | LEN2/LEN3 | approx (§3) |
| `dist(ax, az, bx, bz)` | DIST2 | approx |
| `normalize2/normalize3(v)` | NORMALIZE2/3 | 0-vector pinned |
| `rcp(x)` | RCP | field_rcp, rcp(0) pinned |
| `sin(a)` / `cos(a)` | SIN/COS | a: angle16 |
| `curve(t, x)` / `spline(t, x)` / `dcurve(t, x)` | CURVE/SPLINE/DCURVE | t names a table declared beside the program |
| `noise2(x, z, seed)` | NOISE2 | two unit lanes |
| `ridge(x, z, seed)` | RIDGE | |
| `ring(d, r0, r1)` | RING | annular band-pass (§3.13) |
| `rot2(v, a)` / `rot3(v, a, axis)` | ROT2/ROT3 | |
| `smoothstep(e0, e1, x)` | macro (§3.14) | **not** an opcode (slot 0x20 reserved) |
| literals, `params.f`, `sample.x` | LDC/MOV | |

Anything else inside a field body — division `/` (use `rcp`), `%`, bitwise
ops, vector fx24 types, calls, state reads — is FORM-E-665 (with the specific
offending form cited in the diagnostic message).

### 6.4 Footprints and application

`@earth` requires a footprint; `@flow` requires `footprint none` (mismatch:
FORM-E-666). Footprint forms and their conservative reduction to the
`rectfx` AABB carried by the TerrainField command:

- `rect(x0, z0, x1, z1)` — used verbatim;
- `circle(cx, cz, r)` — envelope `[cx−r, cz−r, cx+r, cz+r]`;
- `capsule(ax, az, bx, bz, r)` — axis-aligned envelope of the swept disc.

Application: a system applies an earth field with
`apply terrain_field rising_ridge(origin: w2, params: ridge_params)
duration 45t` — a **sim statement** (terrain is truth) which (a) enqueues the
truth evaluation over the footprint (software: zfield interpreter over the
heightfield columns; hardware lane: TerrainField command) and (b) records the
scar/surface request the presentation later emits (§5). Applying a field
program from a non-sim domain is FORM-E-461; applying a flow program to
anything but its attached pool is FORM-E-667. The `age`/`phase` input lanes
derive from `start_tick`/`duration_ticks` exactly as the TerrainField command
record defines them (commands.zidl).

---

## 7. FORM-E-nnn error catalog

Diagnostics (D2): structured `{file, span, code: "FORM-E-nnn", message}`,
collected never thrown; non-zero exit on any error; no partial emission
(no `.zprog`, C++, zmap or zcost is written if any diagnostic exists);
warnings (code `FORM-W-nnn`) never affect semantics. **One code per rule.**
W3.2 must exercise every code below at least once (positive and negative
goldens); codes are frozen once W3.2 ships — new rules get new codes.

### FORM-E-001..009 — lexical

| Code | Rule |
|---|---|
| FORM-E-001 | source file is not valid UTF-8 (byte outside comments/strings) |
| FORM-E-002 | unterminated block comment (`/*` without `*/`) |
| FORM-E-003 | unterminated string literal |
| FORM-E-004 | illegal escape in string (only `\\` and `\"`) |
| FORM-E-005 | character does not begin any token |
| FORM-E-006 | malformed integer literal (`0x` without digits, leading-zero octal) |
| FORM-E-007 | integer literal exceeds its target type's range |
| FORM-E-008 | fractional literal not exactly representable in its Q format |
| FORM-E-009 | literal exceeds byte-length limit (ident 64, string 256) |

### FORM-E-100..129 — parse

| Code | Rule |
|---|---|
| FORM-E-100 | expected token T, found other (message names both) |
| FORM-E-101 | unexpected token at top level (declaration keyword required) |
| FORM-E-102 | duplicate declaration name in module |
| FORM-E-103 | `module` not first token of file / file has trailing content |
| FORM-E-104 | record literal names a field the type does not have |
| FORM-E-105 | record literal omits a field (all fields required, in any order) |
| FORM-E-106 | duplicate field in record literal |
| FORM-E-107 | `..` range used outside a `for` header |
| FORM-E-108 | statement form not admitted in this context (e.g. `spawn` in `fn`) |
| FORM-E-109 | keyword used as identifier |
| FORM-E-110 | expression grammar violation not otherwise classified |

### FORM-E-200..229 — modules, names, imports

| Code | Rule |
|---|---|
| FORM-E-201 | duplicate top-level declaration in a module |
| FORM-E-202 | import of unknown module |
| FORM-E-203 | use of an unknown name (unresolved identifier) |
| FORM-E-204 | import cycle |
| FORM-E-205 | ambiguous unqualified name (two imports bring the same name) |
| FORM-E-206 | use of a private name from another module without qualification |

### FORM-E-300..339 — types

| Code | Rule |
|---|---|
| FORM-E-300 | type mismatch (operator/argument/assignment; message shows both types) |
| FORM-E-301 | unknown type name |
| FORM-E-302 | `let` re-assignment (single-assignment law) |
| FORM-E-303 | use of a local before its `let` |
| FORM-E-304 | wrong argument count in call |
| FORM-E-305 | array index not `u32`/`i32` |
| FORM-E-306 | struct field access on non-struct |
| FORM-E-307 | enum member does not exist |
| FORM-E-308 | non-constant initializer where a constant expression is required |
| FORM-E-309 | `fn` return type mismatch with returned expression |
| FORM-E-310 | missing `return` in a value-returning `fn` |
| FORM-E-311 | `if` select-expression branches have different types |
| FORM-E-312 | bool expected (condition position) |
| FORM-E-313 | wrong literal suffix for target type (§1.2 table) |
| FORM-E-320 | `u32` used where signedness matters in a negative context (negative literal to `u32`) |
| FORM-E-330 | `world3`/`velocity3` mixed in one operator (space-typing) |
| FORM-E-331 | mixed-precision binary operands (fx16 vs fx24) |
| FORM-E-332 | `fx24`/`world2/3`/`velocity3` used inside a field declaration (Q2) |
| FORM-E-333 | assignment target is not an lvalue / not declared writable |
| FORM-E-334 | conversion intrinsic argument type wrong |

### FORM-E-400..459 — domains and effects

| Code | Rule |
|---|---|
| FORM-E-400 | write to truth state outside a `system` (in `fn`/`presentation`) |
| FORM-E-401 | pool/global written by a system that did not declare it in `writes` |
| FORM-E-402 | pool/global read by a block that did not declare it in `reads` (systems/presentations declare; `fn`s inherit the caller's admission statically) |
| FORM-E-403 | `input` read outside `sim`/`scenario` |
| FORM-E-404 | `random` stream in `field` dialect |
| FORM-E-405 | presentation block calls a mutating intrinsic (spawn/kill/apply/assign) |
| FORM-E-406 | `spawn`/`kill` outside `sim` |
| FORM-E-407 | system reads and writes the same component in one phase where the scheduler requires separation (delegates to FORM-E-505 family; raised when the read is *only* satisfiable pre-write) |
| FORM-E-408 | `sound` referenced before declaration |
| FORM-E-409 | scenario statement outside `scenario` block |

### FORM-E-460..479 — terrain/present object model

| Code | Rule |
|---|---|
| FORM-E-460 | direct terrain truth mutation outside an `@earth` application |
| FORM-E-461 | `apply terrain_field` outside a `sim` system |
| FORM-E-462 | applied field program is not `@earth` |
| FORM-E-463 | `duration` of zero or missing footprint arguments |
| FORM-E-464 | camera binding expression not a world3 transform source |

### FORM-E-500..519 — scheduling (spec/form/deterministic-scheduling.md)

| Code | Rule |
|---|---|
| FORM-E-500 | two systems write one state component in one phase (message cites BOTH spans — D6) |
| FORM-E-501 | descending or provably-empty `for` range |
| FORM-E-502 | `for` trip count not statically bounded |
| FORM-E-503 | pool mutation (spawn/kill) inside pool-sugar iteration |
| FORM-E-504 | stagger without exactly one iteration pool (`stagger over`) |
| FORM-E-505 | cyclic read-write dependency between systems (no topological order) |
| FORM-E-506 | `every N` with N = 0 |
| FORM-E-507 | stagger rate N not equal to the system's `every N` |

### FORM-E-600..619 — presentation emit

| Code | Rule |
|---|---|
| FORM-E-600 | emit statement outside a `presentation` block |
| FORM-E-601 | required emit argument missing |
| FORM-E-602 | unknown emit argument name |
| FORM-E-603 | unknown emit kind |
| FORM-E-604 | more than two views declared |
| FORM-E-605 | view budgets + shared sum exceeds 100% |
| FORM-E-606 | view id repeated or not 0/1 |
| FORM-E-607 | view has no camera binding |
| FORM-E-608 | `draw_population` names a non-pool |
| FORM-E-609 | `audio` names a non-sound |
| FORM-E-610 | resource page-id argument is not a `u32` const |

### FORM-E-650..679 — field dialect (§6)

| Code | Rule |
|---|---|
| FORM-E-650 | `@<profile>` other than earth/flow on a field declaration |
| FORM-E-651 | `if` statement, loop, or early return in field body (branchless law) |
| FORM-E-652 | call (even pure `fn`) inside field body |
| FORM-E-653 | statement other than `let`/`return` in field body |
| FORM-E-654 | declared `max_ops` above the profile ceiling (field-ir §7.3) |
| FORM-E-655 | lowered instruction count above the declared `max_ops` |
| FORM-E-656 | state access (pool/global/terrain) inside field body |
| FORM-E-657 | `input`/`random` inside field body |
| FORM-E-658 | non-fx16 lane/local type inside field body |
| FORM-E-659 | more than 64 live values (register budget, field-ir §11.2) |
| FORM-E-660 | params struct field not `fx16` |
| FORM-E-661 | params struct exceeds the profile's p-lane count (8 earth / 4 flow) |
| FORM-E-662 | params binding missing or not a struct |
| FORM-E-663 | return record does not match the profile output record |
| FORM-E-664 | attached pool struct does not match the flow lane mapping |
| FORM-E-665 | expression form not admitted in the field dialect (message names it; e.g. `/`, bitwise) |
| FORM-E-666 | footprint/none mismatch for the profile |
| FORM-E-667 | flow program applied to a pool other than its mapping |
| FORM-E-668 | table declaration malformed or tables exceed table-byte budget |

### FORM-E-700..729 — L1 OUT list (refused, D1; FORM §20; charter §26)

| Code | Refused feature | Returns |
|---|---|---|
| FORM-E-700 | `form` declarations / representation ladders | L3 |
| FORM-E-701 | `macro` declarations | never-before-core (§20) |
| FORM-E-702 | generics beyond pool capacity literals (`<T>`) | L2+ |
| FORM-E-703 | `class`/`interface`/inheritance (OOP) | refused (§20) |
| FORM-E-704 | closures / lambda syntax | refused (§20) |
| FORM-E-705 | string type / string operations (strings only in the four §1 positions) | refused (§20) |
| FORM-E-706 | first-class functions / function values | refused (§20) |
| FORM-E-707 | pointers / references | refused (§20) |
| FORM-E-708 | `while` / unbounded loops | refused (§20, FORM §6) |
| FORM-E-709 | recursion (self or mutual; detected on the call graph) | refused (FORM §6) |
| FORM-E-710 | host FFI (`extern`/`importc`/escape hatches) | refused in L1 (§20 — C++ escape hatch is a cartridge concern, not a language one) |
| FORM-E-711 | floating-point types/literals (`f32`/`f64`/`float`/`double`/scientific notation) | refused (numeric policy, FORM §5) |
| FORM-E-712 | `break`/`continue` | refused (bounded-loop law) |
| FORM-E-713 | `terrain_material` declarations | L4 |
| FORM-E-714 | `population` declarations (pool + `@flow` + present emit is the L1 surface) | L3 |
| FORM-E-715 | `@build`/`@warp`/`@formation`/`@stamp` domains | L2 profiles |
| FORM-E-716 | `surface`/`spell` declarations | L2+ |
| FORM-E-717 | `layer`/`after`/`transparent_group`/`barrier` ordering constructs | L3 |
| FORM-E-718 | audio graph constructs beyond the tone-event declaration | L4 |
| FORM-E-719 | dynamic allocation / unbounded collection growth | refused (FORM §6) |
| FORM-E-720 | `assert_budget` naming an undeclared budget set | L3 registry |

### FORM-E-800..839 — capacities, bounds, packing

| Code | Rule |
|---|---|
| FORM-E-800 | pool capacity not a positive integer or `u32` const |
| FORM-E-801 | pool element type is not a struct / is recursive |
| FORM-E-820 | compile-time-provable array/pool index out of bounds |
| FORM-E-821 | pool overflow at spawn (deterministic runtime abort) |
| FORM-E-822 | pool/array index out of bounds at runtime (deterministic runtime abort) |
| FORM-E-830 | cartridge page-id const unresolved at pack time (spec/cartridge.md §5) |
| FORM-E-831 | cartridge kind mismatch for a page id (e.g. non-patch used by `draw_procedural`) |
| FORM-E-832 | two emit sites share one source-ID slot (registry overflow: > 65536 declarations per module) |

### FORM-E-900..919 — scenarios

| Code | Rule |
|---|---|
| FORM-E-900 | `seed` missing or repeated in a scenario |
| FORM-E-901 | `load` target not a module/map known to the cartridge |
| FORM-E-902 | `spawn player` index not 0..3 |
| FORM-E-903 | `at N ticks` not ascending across the scenario script |
| FORM-E-904 | scenario action names an unknown system/spell entry |
| FORM-E-905 | `capture frame N` before any earlier `at` tick |
| FORM-E-906 | assertion references undeclared state |
| FORM-E-907 | tolerance literal not representable (uses §1.2 fx16 law) |

Warnings `FORM-W-nnn` (non-exhaustive, non-semantic): naming-convention
deviations (§1.3), unused declarations, budget shares below the FORM §12
minimum when views exist, `%`-budget rounding to a non-exact unit8.

---

## 8. D1 IN/OUT cross-check (law; reviewed against charter §26 + FORM §20)

**IN (L1, this document):** modules+import; const; enum; struct; `pool
T: Struct[cap]`; `global` persistent state (FORM §6, D4/D5 consequence);
`system name every N ticks [stagger over p] reads..writes..`; pure `fn`;
single-assignment `let`; expressions over fx16/fx24/angle16/unit8/i32/u32/bool
+ world2/world3/velocity3/colour8 with space-typing where practical; bounded
ascending `for`; `if` as select-expression (both arms evaluated);
`random.stream(seed, id...)`; `@earth`/`@flow` field declarations (field
dialect §6, frozen FieldBuilder); pure `presentation` blocks with the L1 emit
vocabulary (§5); `scenario` blocks; `sound` as tone-event declaration only;
`input.player(n)` pad snapshots; cartridge page-id resource constants.

**OUT until L2+ — refused with FORM-E-7xx codes, never silently parsed:**
forms/ladders (L3, E-700/717); terrain_material grammar (L4, E-713);
build/warp/formation/stamp domains (L2, E-715); macros (E-701); generics
beyond capacity literals (E-702); OOP/closures/strings/first-class
functions/pointers/while/recursion/host FFI (E-703..712); float types
(E-711); `population` declarations (E-714); `surface`/`spell` (E-716);
dynamic allocation (E-719). Charter §26 additionally refuses machine-level
features (general fragment shaders, unrestricted render-to-texture, …) —
those are console refusals, not language ones; the language cannot express
them because no L1 construct lowers to them.

Cross-check vs charter §26: no L1 construct depends on a refused machine
feature; the L1 draw surface emits only commands that exist in ABI v3
(implemented status, spec/commands.zidl). Cross-check vs FORM §20: every §20
non-goal has a refusal code above (E-701/703/704/705/706/707/708/710/711) —
"language whose behaviour depends on source-order draw calls" is refused
structurally by the FORM §13 reorderability law (domains-and-effects.md §2)
rather than by a single code.
