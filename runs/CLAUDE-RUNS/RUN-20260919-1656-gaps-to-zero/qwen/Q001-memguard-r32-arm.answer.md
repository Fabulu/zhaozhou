# Q001 answer — memguard-r32-arm

- model: qwen38-quasar-dflash2-k8v4-112k  dispatch: d6b81fa1dc1848229d65c809a098e1aa
- when: 2026-09-19T20:49:22  seconds: 225  finish: stop  status: ok
- usage: {"completion_tokens": 19255, "completion_tokens_details": {"reasoning_tokens": 16994}, "prompt_tokens": 14031, "prompt_tokens_details": {"cached_tokens": 0}, "total_tokens": 33286}
- inputs: [{"input": "fpga/rtl/memory/zhao_mem_guard.sv:1-408", "sha256": "ab844c1958aec778", "chars": 25199}, {"input": "git show 37bef328 -- fpga/rtl/memory/zhao_mem_guard.sv", "sha256": "2fb8cc98566b7753", "chars": 4399}, {"input": "git show 37bef328 -- reference/include/zref/zref_mem.hpp", "sha256": "3bf41de2a1c0fa37", "chars": 2497}, {"input": "git show 37bef328 -- tests/formal/formal_mem_guard.sv", "sha256": "aae3f51968d4ebdc", "chars": 8493}]
- kind: task

## Answer

## FINDINGS

none found

## Analysis

### Q1 — Can the new arm let TERRAIN_BUILD write outside RENDER.ASSET_POOL or outside [res_base, res_base+res_span)?

No. The containment chain is:

1. **`res_in_pool`** (zhao_mem_guard.sv:291-293) requires `res_base >= ZHAO_RENDER_ASSET_BASE` and `res_end33 <= {1'b0, ZHAO_RENDER_ASSET_BASE + ZHAO_RENDER_ASSET_SPAN}`, where `res_end33 = {1'b0,res_base} + {1'b0,res_span}` is computed in 33 bits (line 289). The 33-bit width closes the 32-bit wrap of `res_base + res_span` by construction. This proves `[res_base, res_base+res_span) ⊆ RENDER.ASSET_POOL`.

2. **`resource_wr_ok`** (zhao_mem_guard.sv:294-295) additionally requires `addr32 >= res_base` and `end33 <= res_end33`, where `end33 = {1'b0, end32}` (line 290) and `end32 = addr32 + req.len` in 32 bits (line 119). This proves the write span `[addr32, end32) ⊆ [res_base, res_base+res_span)`.

Edge cases checked:
- **Overflow/wrap**: `res_end33` is 33 bits, so `res_base=0xFFFFFFFF, res_span=1` gives `res_end33=0x1_0000_0000 > 0x0800_0000` → `res_in_pool=0`. No escape.
- **span=0**: `res_end33 = res_base`; then `end32 > addr32 >= res_base = res_end33` → `end33 <= res_end33` fails. No write passes.
- **Region at pool end**: `res_end33 = 0x0800_0000` is allowed (`<=`); a write ending at `0x0800_0000` has its last byte at `0x07FF_FFFF`, inside the pool.
- **Burst starts inside, ends outside**: `end33 > res_end33` → denied.
- **Burst starts outside, ends inside**: `addr32 < res_base` → denied.

TERRAIN_BUILD's other arms cannot reach the asset pool: `terrain_ok` (lines 263-266) is bounded to `[0x0400_0000, 0x054E_0000)`, disjoint from the asset pool; `terrain_rd_ok` (lines 269-272) requires `!req.write`.

### Q2 — Can any client other than TERRAIN_BUILD write into RENDER.ASSET_POOL?

No. The `unique case` (lines 302-311) is the only path to `pass_ok`:
- SCANOUT: `scan_ok` requires `!req.write` (line 130).
- BLIT_DMA / ENGINE0: `blit_ok` confines writes to the FB slot windows `[0x0000_0000, 0x0003_C000)` / `[0x0200_0000, 0x0203_C000)` (lines 146-153), disjoint from the asset pool.
- ENGINE1: `render_asset_ok` requires `!req.write` (line 170).
- TERRAIN_BUILD: `resource_wr_ok` is the only write arm into the asset pool (line 294).
- default: `pass_ok = 0` (line 309).

### Q3 — RTL vs zref containment test

Identical in logic and strictness:

| Term | RTL (zhao_mem_guard.sv:291-293) | zref (zref_mem.hpp diff) |
|---|---|---|
| valid | `res_valid` | `m.res_valid` |
| base | `res_base >= ZHAO_RENDER_ASSET_BASE` (32-bit) | `m.res_base >= kRenderAssetBase` (uint32_t) |
| end | `res_end33 <= {1'b0, BASE+SPAN}` (33-bit) | `uint64_t(res_base)+res_span <= uint64_t(BASE)+SPAN` (64-bit) |

Both use `>=` for the base and `<=` for the end (half-open interval containment). The 33-bit vs 64-bit width difference is immaterial: both prevent wrap for all 32-bit `res_base`/`res_span` values. No strictness or width discrepancy changes the verdict.

### Q4 — Formal harness vacuity

The harness computes `fwd_in_resource` **independently of the DUT** using the held environment registers `env_res_valid`, `env_res_base`, `env_res_span` (formal_mem_guard.sv diff, the `wire [32:0] res_end33 = …` block). The assertions:
- `a1_resource_bounded`: any write forwarded into the pool must satisfy the independently computed `fwd_in_resource`.
- `a1_render_asset_wr_owner`: any write into the pool must be from TERRAIN_BUILD.
- `a1_region` (new arm): allows the write only if `fwd_in_resource` holds.

The cover `c_forward_resource_wr` (formal_mem_guard.sv diff) requires a valid TERRAIN_BUILD write into the pool, ensuring the arm is reachable. The environment inputs are free (solver-chosen at cycle 0, held constant), so the solver can set `res_valid=1` with a valid in-pool region and exercise the arm. The proof does not pass vacuously: a DUT that allowed an out-of-region write would fail `a1_resource_bounded` and `a1_region`.

### Q5 — res_* changing mid-burst

The guard checks `pass_ok` combinationally in the acceptance cycle (line 384) and latches `fwd_req.{write,client,addr,len}` in the same `always_ff` (lines 387-390). The `res_*` inputs are **not** latched into `fwd_req`; they are consumed only in the combinational `res_in_pool`/`resource_wr_ok` at acceptance. Between acceptance and arbiter grant (`fwd_active && arb_rsp.grant`, line 380), `res_*` could in principle change. However:

- The documented intent is that `res_*` is **host configuration** (zhao_mem_guard.sv:81-87: "the host-configured destination region"), not a dynamic per-frame value.
- The formal harness models this by holding `env_res_*` constant for the entire trace (set at cycle 0, formal_mem_guard.sv diff).
- The guard's contract is to check at acceptance; it correctly does so. No request is forwarded without being checked (the check and latch are in the same clocked block, lines 383-390).

This is a design assumption (stability of `res_*` during the forwarding window), not a defect in the guard. No finding.

## CONTINUATION
- Done:
  - R32 arm (`resource_wr_ok`, zhao_mem_guard.sv:294-295) is correctly bounded: 33-bit `res_end33` (line 289) prevents `res_base+res_span` wrap; `res_in_pool` (lines 291-293) proves region ⊆ pool; write-bounds (line 295) prove write ⊆ region. All edge cases (span=0, pool-end, wrap, partial overlap) deny correctly.
  - Only TERRAIN_BUILD can write into RENDER.ASSET_POOL (case statement, lines 302-311); all other clients' arms are read-only or spatially disjoint from the pool.
  - RTL containment test matches zref exactly (same `>=`/`<=` strictness; 33-bit vs 64-bit width is immaterial).
  - Formal harness constrains the arm non-vacuously: `fwd_in_resource` is computed independently of the DUT; `a1_resource_bounded`, `a1_render_asset_wr_owner`, and `a1_region` collectively enforce the law; cover `c_forward_resource_wr` ensures reachability.
  - No TOCTOU defect: check and latch are atomic in the same `always_ff` (lines 383-390); `res_*` stability is a documented design assumption, correctly modeled by the harness.

- Open:
  - `ZHAO_RENDER_ASSET_BASE` / `ZHAO_RENDER_ASSET_SPAN` actual constant values in `zhao_pkg` are inferred from comments (0x06A0_0000, 0x0160_0000) but not shown in the provided material; a mismatch would shift the pool bounds. Not verifiable from the given inputs.
  - The `zhao_mem_guard_resbound_mutant` file (referenced at zhao_mem_guard.sv:286) is not shown; its exact mutation (removal of the pool-containment term) is described in comments but not independently verified.
  - System-level guarantee that `res_*` is stable during the `fwd_active` window is assumed but not shown in any interface contract.

- Next chunk:
  - Verify `ZHAO_RENDER_ASSET_BASE` and `ZHAO_RENDER_ASSET_SPAN` in `fpga/rtl/zhao_pkg.sv` (or wherever they are defined) match the comment-stated values 0x06A0_0000 / 0x0160_0000. Input needed: the package file defining these constants. This confirms the pool bounds used throughout the analysis.

## Coordinator verdict

**verified** — spot 3/3: 33-bit res_end33 guards wrap (guard.sv:289); check and latch are atomic, fwd_req latched at accept so later res_* changes cannot re-bound an accepted burst (guard.sv:383-390); pool constants 0x06A0_0000/0x0160_0000 match zhao_pkg.sv:156-157 (its open item, now closed). No findings, and none missed as far as the spot checks reach.
