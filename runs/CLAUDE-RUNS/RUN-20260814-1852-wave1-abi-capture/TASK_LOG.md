# Task Log: RUN-20260814-1852 - [Describe objective here]

**Created:** 2026-08-14 18:52 UTC+02:00
**Status:** In Progress
**Working Directory:** runs/CLAUDE-RUNS/RUN-20260814-1852-wave1-abi-capture/

---

## Objective

[Clear statement of what this task aims to accomplish]

---

## Progress Timeline

### 2026-08-14 18:52 UTC+02:00 - Task Started

- Generated Run ID: RUN-20260814-1852
- Created working directory
- Initial context: [brief description]

---

## Subagent Spawns

*Log subagent spawns and their findings here*

| Timestamp | Agent ID | Purpose | Status | Findings Link |
|-----------|----------|---------|--------|---------------|
| | | | | |

---

## Files Created

*Updated as files are created*

---

## Decisions Made

*Updated as decisions are made*

---

## Next Steps

*Updated as progress is made*

---

## 2026-08-14 — RECON entry (P5: ABI IDL generator + frame packet + .zcap)

- RECON agent surveyed IDL/codegen approaches (FlatBuffers/Cap'n Proto/protobuf/Kaitai rejected with reasons; SystemRDL/PeakRDL/ORDT noted for registers.zidl later; vk.xml/Mesa genxml cited as single-source precedents).
- Proposed .zidl grammar (EBNF): enums, bits containers, handle32{index:24,generation:8}, explicit pad discipline, natural alignment capped at 4, command records multiple of 16 B, opcode-range policy.
- Recommended CRC-32C (Castagnoli), reflected, init/xorout 0xFFFFFFFF, header CRC + trailing payload CRC; identical table-driven C++/TS/SV implementations; test vector "123456789" -> 0xE3069283.
- Frame packet: 32-B sealed header (magic, abi_version, frame_id, sequence, resource_epoch, deadline, command_count, command_bytes) + per-command 16-B prefix {u16 opcode, u16 length, u32 source_id, u32 flags}; per-command length prefix + validator => malformed commands fail safely with ZH_ABI_* error codes.
- .zcap: sectioned container, fixed header + section table, independently versioned + CRC-32C'd sections (ABI_INFO, FRAME_PACKET, RESOURCE_PAGES, CONTROLLER_SNAPSHOT, FRAMEBUFFER_CRC, TILE_CRC, DEPTH_STENCIL_CRC, COUNTERS, SOURCE_MAP, TRACE); unknown-section skip rule for evolution.
- Source IDs: 32-bit {kind:4, module:12, index:16} sequential registry + sourceids.zmap manifest; captures embed SOURCE_MAP so they stay valid across compiler changes.
- Generator architecture: parse -> semantic pass -> single LayoutIR -> emitters (C++/TS/SV/doc/fuzz), deterministic timestamp-free output, --check mode + golden vectors in CI.
- Full detail in FINDINGS.md (delivered as agent report text; harness blocks subagent file writes).

---

## 2026-08-14 — W4 IMPLEMENTER entry (ABI generator, frame packet, .zcap, empty-frame replay)

Worktree `C:\programmieren\zencrifice\.worktrees\w4`, branch `wp/w4-abi`, commits 11-14 pushed:

- `562787f` spec: commands.zidl + capture_format.md; abi-gen TS generator (5 emitters + goldens)
- `0383ed1` abi: generated C++/TS/SV + CRC-32C + golden binaries + fuzz corpus
- `9d8dc79` frame: sealed packet + .zcap container + ZRef/TS round-trips
- `4131647` replay: empty frame through ZRef + verilated stub; tri-language error parity

**Delivered:** spec/commands.zidl (12 commands: 5 implemented, 7 reserved; error enum; FRAME_SLOT_BYTES); spec/capture_format.md; tools/abi-gen (zero-dep parser -> semantic pass -> single LayoutIR -> emit_cpp/ts/sv/doc/fuzz + golden oracle + 17 tests); generated runtime/include/zhao_abi.h, compiler/src/generated/{abi,frame,zcap}.ts (frame/zcap from templates - single source, staleness-checked), fpga/rtl/generated/zhao_abi_pkg.sv (reverse-field-order packed structs + explicit localparam bit-range pack/unpack + CRC-32C table/step + full frame validator; lint-clean -Wall); reference/zref_frame.{hpp,cpp} + zref_sha256.hpp; tests/unit/{test_crc,test_abi_golden,test_zcap_roundtrip}.cpp; tests/differential/{zhao_abi_probe.sv,test_empty_frame_replay.cpp}; tests/fuzz/{abi_corpus_gen.ts,test_abi_fuzz_parity.cpp}; tools/capture CLI (zhao-capture info/verify/write); goldens (12 cmd records, frame_minimal.bin, zcap_minimal.zcap, abi_corpus.zcorpus 21 cases). Placeholder zhao_frame_pkg.sv REMOVED (swap complete); stub imports the generated package and validates header_crc32c for real.

**Verification:** ctest -L fast 9/9, -L nightly 9/9; npm abi-gen 17/17, compiler 7/7; abi:check clean (22 outputs); verilator lint clean (-Wall) on package + stub + probe.

**Deviations / corrections (documented in capture_format.md):**
1. CRC residue CORRECTED: P5's 0x1C2D19ED is not reproducible in this parameterization. Verified empirically: check constant crc32c(0, msg‖LE(crc)) = 0x48674BC7 for every message; init-seeded register residue = 0xB798B438 (the RevEng catalogue value). Empty-input 0x00000000 confirmed as derived.
2. Reserved-command record sizes differ from P5's estimates (TerrainField 112 B not 96, SurfaceStamp/DrawProcedural 64 B, SetPresentationContract pad[8]) - consequences of the ratified 4-byte fx16 and 24-byte transform2fx; the .zidl layout math is normative.
3. Grammar extensions ratified in-spec: mandatory command status keyword (implemented|reserved); anonymous pad[n].
4. v1 validator implements spec 3.2 steps 1-6+9+10; steps 7-8 (enum range, stale handle) are generated-but-inactive - no v1 field exercises them (documented deferral, not a gap).
5. .zcap writer serializes the spec's seek/backpatch layout in one buffered pass (observable bytes identical; syscall pattern documented).
6. Root CMakeLists gained `-static-libgcc -static-libstdc++`: the oss-cad-suite lib dir shadows winlibs libstdc++-6.dll and breaks C++ test entry points when it wins PATH resolution (0xc0000139). CI note: build lane keeps suite paths on PATH; static runtime makes this order-independent.
7. Lint test now invokes verilator_bin directly (VERILATOR_ROOT from the CMake cache; native-path normalization for mingw ctest).

**Machine gotchas logged:** devkitPro msys2 ctest still first on bare PATH (broken for native-path test commands - always source the env); npm must resolve to npm.cmd for CTest; core.autocrlf=true required a scoped .gitattributes pinning generated outputs to LF and binaries to -text.
