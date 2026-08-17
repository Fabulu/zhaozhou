// test_field_fuzz_parity.cpp — nightly TS-vs-C++ Field IR differential over
// the committed bounded-random corpus (plan W5 deliverable 8; field-ir.md
// §10: TS is the subordinate interpreter). For every
// fuzz_seed_<seed>.zprog/.zvec pair: decode + re-validate, replay every
// record, compare outputs AND status words bit-exactly against the TS-own
// expected values in the .zvec.

#include <cstdio>
#include <string>
#include <vector>

#include "zfield/zfield.hpp"

#ifndef ZHAO_FIELD_CORPUS_DIR
#error "ZHAO_FIELD_CORPUS_DIR must be defined (tests/CMakeLists.txt)"
#endif

namespace {

std::vector<uint8_t> readFile(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return {};
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  std::vector<uint8_t> buf(n);
  if (n > 0 && fread(buf.data(), 1, n, f) != (size_t)n) buf.clear();
  fclose(f);
  return buf;
}

uint32_t rd32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int failures = 0;

void checkProgram(const std::string& name) {
  const std::vector<uint8_t> progBytes =
      readFile(std::string(ZHAO_FIELD_CORPUS_DIR) + "/" + name + ".zprog");
  if (progBytes.empty()) {
    printf("FAIL %s: .zprog missing (run `npm run -w compiler test`)\n", name.c_str());
    ++failures;
    return;
  }
  const zfield::DecodeResult dec = zfield::decode(progBytes.data(), progBytes.size());
  if (dec.error != zfield::DecodeError::kOk) {
    printf("FAIL %s: decode %s (%s)\n", name.c_str(), zfield::decodeErrorName(dec.error),
           dec.detail.c_str());
    ++failures;
    return;
  }

  const std::vector<uint8_t> vecBytes =
      readFile(std::string(ZHAO_FIELD_CORPUS_DIR) + "/" + name + ".zvec");
  if (vecBytes.size() < 32) {
    printf("FAIL %s: .zvec missing\n", name.c_str());
    ++failures;
    return;
  }
  const uint32_t hash = rd32(vecBytes.data() + 8);
  const uint32_t count = rd32(vecBytes.data() + 20);
  const uint32_t inLanes = vecBytes[24];
  const uint32_t outLanes = vecBytes[25];
  if (hash != dec.prog.program_hash) {
    printf("FAIL %s: hash mismatch zvec 0x%08x vs prog 0x%08x\n", name.c_str(), hash,
           dec.prog.program_hash);
    ++failures;
    return;
  }
  const size_t recBytes = (size_t)inLanes * 4 + outLanes * 4 + 4;
  if (vecBytes.size() != 32 + count * recBytes) {
    printf("FAIL %s: zvec length\n", name.c_str());
    ++failures;
    return;
  }
  const uint8_t* p = vecBytes.data() + 32;
  for (uint32_t i = 0; i < count; ++i, p += recBytes) {
    int32_t out[8] = {0};
    const zfield::Status st =
        zfield::interpret(dec.prog, reinterpret_cast<const int32_t*>(p), inLanes, out, outLanes);
    const uint32_t actStatus = (st.sat ? 1u : 0u) | (st.rcp0 ? 2u : 0u);
    const int32_t* exp = reinterpret_cast<const int32_t*>(p + inLanes * 4);
    const uint32_t expStatus = rd32(p + inLanes * 4 + outLanes * 4);
    for (uint32_t k = 0; k < outLanes; ++k) {
      if (out[k] != exp[k]) {
        printf("FAIL %s: record %u lane %u: expected 0x%08x got 0x%08x\n", name.c_str(), i, k,
               (uint32_t)exp[k], (uint32_t)out[k]);
        ++failures;
        return;
      }
    }
    if (actStatus != expStatus) {
      printf("FAIL %s: record %u status: expected 0x%x got 0x%x\n", name.c_str(), i, expStatus,
             actStatus);
      ++failures;
      return;
    }
  }
  printf("ok %s: %u records bit-identical (outputs + status)\n", name.c_str(), count);
}

}  // namespace

int main() {
  // decimal seed values 0x51CE 0x5A17 0xC0FF 0xEE 0x1234 0xF00D 0xBEEF 0x7A17
  // (the TS generator names files fuzz_seed_<seed-as-decimal>)
  const char* seeds[] = {"fuzz_seed_20942", "fuzz_seed_23063", "fuzz_seed_49407",
                         "fuzz_seed_238",   "fuzz_seed_4660",  "fuzz_seed_61453",
                         "fuzz_seed_48879", "fuzz_seed_31255"};
  for (const char* s : seeds) checkProgram(s);
  if (failures != 0) {
    printf("%d FAILURE(S)\n", failures);
    return 1;
  }
  printf("field fuzz parity: corpus green\n");
  return 0;
}
