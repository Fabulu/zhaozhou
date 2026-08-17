// zref_frame.cpp — ZRef sealed frame packet + .zcap container implementation.
// Law: spec/capture_format.md (3 frame packet, 3.2 fail-safe order, 4 .zcap,
// 5 source IDs). Layout constants/CRC/error codes come from the generated
// zhao_abi.h. This is the C++ testee of the tri-language conformance suite.

#include "zref/zref_frame.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace zhao {

using namespace zhao_abi;

// ---------------------------------------------------------------- frame ----

namespace {

uint16_t rd16(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t rd32(const uint8_t* p) { return rd16(p) | (uint32_t(rd16(p + 2)) << 16); }

}  // namespace

ZhaoValidateResult zhao_frame_validate(const uint8_t* pkt, size_t len, uint32_t slot_bytes) {
  ZhaoValidateResult r;
  // Header-level aborts (checks 1-3) report exactly ZHAO_FRAME_HEADER_BYTES
  // consumed (capture_format.md 3.2) — W6 conformance fix: this used to
  // report 0, contradicting the spec, the SV stub (pinned at 36 by
  // test_stub_top) and the TS mirror (frame.ts defaults 36).
  const auto fail_hdr = [&](zhao_abi_error e) {
    r.error = e;
    r.commands_consumed = 0;
    r.bytes_consumed = ZHAO_FRAME_HEADER_BYTES;
    return r;
  };
  const auto fail = [&](zhao_abi_error e, uint32_t seen, uint32_t consumed) {
    r.error = e;
    r.commands_consumed = seen;
    r.bytes_consumed = consumed;
    return r;
  };

  // 1. magic (whenever 4 bytes exist), header completeness, abi version, flags
  if (len < 4) return fail_hdr(ZH_ABI_BAD_LENGTH);
  if (rd32(pkt + ZHAO_OFF_MAGIC) != ZHAO_FRAME_MAGIC) {
    return fail_hdr(ZH_ABI_BAD_MAGIC);
  }
  if (len < ZHAO_FRAME_HEADER_BYTES) return fail_hdr(ZH_ABI_BAD_LENGTH);
  if (rd16(pkt + ZHAO_OFF_ABI_VERSION) != ZHAO_ABI_VERSION) {
    return fail_hdr(ZH_ABI_BAD_ABI_VERSION);
  }
  const uint16_t frame_flags = rd16(pkt + ZHAO_OFF_FLAGS);
  if ((frame_flags & ~ZHAO_FRAME_FLAG_CONTAINS_DEBUG) != 0) {
    return fail_hdr(ZH_ABI_RESERVED_FLAG);
  }

  // 2. bounds
  const uint32_t command_bytes = rd32(pkt + ZHAO_OFF_COMMAND_BYTES);
  const uint32_t command_count = rd32(pkt + ZHAO_OFF_COMMAND_COUNT);
  const uint32_t full = ZHAO_FRAME_OVERHEAD + command_bytes;
  if (command_bytes % ZHAO_COMMAND_ALIGNMENT != 0) return fail_hdr(ZH_ABI_BAD_LENGTH);
  if (full > slot_bytes) return fail_hdr(ZH_ABI_BAD_LENGTH);
  if (uint64_t(command_count) * 16 > command_bytes) return fail_hdr(ZH_ABI_BAD_LENGTH);
  if (len != full) return fail_hdr(ZH_ABI_BAD_LENGTH);

  // 3. header CRC over [0,32)
  if (zhao_crc32c(0, pkt, 32) != rd32(pkt + ZHAO_OFF_HEADER_CRC)) {
    return fail_hdr(ZH_ABI_BAD_HEADER_CRC);
  }

  // 4. payload CRC over the command stream
  if (zhao_crc32c(0, pkt + ZHAO_FRAME_HEADER_BYTES, command_bytes) !=
      rd32(pkt + ZHAO_FRAME_HEADER_BYTES + command_bytes)) {
    return fail(ZH_ABI_BAD_PAYLOAD_CRC, 0, full);
  }

  // 5./6./9./10. record walk
  const uint8_t* stream = pkt + ZHAO_FRAME_HEADER_BYTES;
  uint32_t off = 0;
  uint32_t seen = 0;
  bool any_debug = false;
  while (off < command_bytes) {
    if (off + 16 > command_bytes) return fail(ZH_ABI_TRUNCATED, seen, full);
    const uint16_t opcode = rd16(stream + off);
    const uint32_t rec_bytes = rd16(stream + off + 2);
    if (rec_bytes % ZHAO_COMMAND_ALIGNMENT != 0 || rec_bytes < 16) {
      return fail(ZH_ABI_BAD_LENGTH, seen, full);
    }
    if (uint64_t(off) + rec_bytes > command_bytes) return fail(ZH_ABI_BAD_LENGTH, seen, full);
    const ZhCommandInfo* info = zhao_command_info(opcode);
    if (info == nullptr) return fail(ZH_ABI_UNKNOWN_OPCODE, seen, full);
    if (rec_bytes != info->record_bytes) return fail(ZH_ABI_BAD_LENGTH, seen, full);

    // 6. reserved fields: record-header flags (no defined bits in v1), reserved0
    if (rd32(stream + off + 8) != 0) return fail(ZH_ABI_RESERVED_FLAG, seen, full);
    if (rd32(stream + off + 12) != 0) return fail(ZH_ABI_RESERVED_FIELD, seen, full);
    // 6b. declared payload pad bytes must be zero
    for (uint16_t k = 0; k < info->pad_count; k++) {
      if (stream[off + 16 + info->pad_offsets[k]] != 0) {
        return fail(ZH_ABI_RESERVED_FIELD, seen, full);
      }
    }
    // 7. enum fields must carry a declared member value (ABI v2)
    if (!zhao_enum_value_ok(opcode, stream + off + 16)) {
      return fail(ZH_ABI_BAD_VALUE, seen, full);
    }

    if (opcode >= 0xF000 && opcode <= 0xF0FF) any_debug = true;
    off += rec_bytes;
    seen++;
  }
  if (off != command_bytes) return fail(ZH_ABI_TRUNCATED, seen, full);
  if (seen != command_count) return fail(ZH_ABI_COUNT_MISMATCH, seen, full);
  if (any_debug && (frame_flags & ZHAO_FRAME_FLAG_CONTAINS_DEBUG) == 0) {
    return fail(ZH_ABI_DEBUG_FLAG_REQUIRED, seen, full);
  }
  r.error = ZH_ABI_OK;
  r.commands_consumed = seen;
  r.bytes_consumed = full;
  return r;
}

ZhaoValidateResult zhao_frame_validate(const std::vector<uint8_t>& pkt, uint32_t slot_bytes) {
  return zhao_frame_validate(pkt.data(), pkt.size(), slot_bytes);
}

ZhaoFrameHeader zhao_frame_parse_header(const uint8_t* pkt) {
  ZhaoFrameHeader h;
  h.flags = rd16(pkt + ZHAO_OFF_FLAGS);
  h.frame_id = rd32(pkt + ZHAO_OFF_FRAME_ID);
  h.sequence = rd32(pkt + ZHAO_OFF_SEQUENCE);
  h.resource_epoch = rd32(pkt + ZHAO_OFF_RESOURCE_EPOCH);
  h.deadline_cycles = rd32(pkt + ZHAO_OFF_DEADLINE);
  h.command_count = rd32(pkt + ZHAO_OFF_COMMAND_COUNT);
  h.command_bytes = rd32(pkt + ZHAO_OFF_COMMAND_BYTES);
  return h;
}

// -------------------------------------------------------------- builder ----

ZhaoFrameBuilder& ZhaoFrameBuilder::begin_frame(uint32_t frame_id, uint32_t resource_epoch,
                                                uint32_t flags, uint32_t deadline_cycles,
                                                uint32_t source_id) {
  ZhRecordBeginFrame r = zhao_sample_begin_frame();
  r.hdr.source_id = source_id;
  r.payload.frame_id = frame_id;
  r.payload.resource_epoch = resource_epoch;
  r.payload.flags = flags;
  r.payload.deadline_cycles = deadline_cycles;
  std::vector<uint8_t> bytes;
  zhao_pack_begin_frame(r, bytes);
  return append_record(bytes);
}

ZhaoFrameBuilder& ZhaoFrameBuilder::nop(uint32_t source_id) {
  ZhRecordNop r = zhao_sample_nop();
  r.hdr.source_id = source_id;
  std::vector<uint8_t> bytes;
  zhao_pack_nop(r, bytes);
  return append_record(bytes);
}

ZhaoFrameBuilder& ZhaoFrameBuilder::end_frame(uint32_t completion_flags, uint32_t source_id) {
  ZhRecordEndFrame r = zhao_sample_end_frame();
  r.hdr.source_id = source_id;
  r.payload.completion_flags = completion_flags;
  std::vector<uint8_t> bytes;
  zhao_pack_end_frame(r, bytes);
  return append_record(bytes);
}

ZhaoFrameBuilder& ZhaoFrameBuilder::append_record(const uint8_t* bytes, size_t n) {
  stream_.insert(stream_.end(), bytes, bytes + n);
  count_++;
  return *this;
}

ZhaoFrameBuilder& ZhaoFrameBuilder::append_record(const std::vector<uint8_t>& bytes) {
  return append_record(bytes.data(), bytes.size());
}

std::vector<uint8_t> ZhaoFrameBuilder::seal(uint32_t frame_id, uint32_t sequence,
                                            uint32_t resource_epoch, uint32_t deadline_cycles,
                                            uint16_t flags) const {
  std::vector<uint8_t> out(ZHAO_FRAME_OVERHEAD + stream_.size(), 0);
  const auto put16 = [&](uint32_t off, uint16_t v) {
    out[off] = uint8_t(v & 0xFF);
    out[off + 1] = uint8_t(v >> 8);
  };
  const auto put32 = [&](uint32_t off, uint32_t v) {
    for (int i = 0; i < 4; i++) out[off + i] = uint8_t(v >> (8 * i));
  };
  put32(ZHAO_OFF_MAGIC, ZHAO_FRAME_MAGIC);
  put16(ZHAO_OFF_ABI_VERSION, ZHAO_ABI_VERSION);
  put16(ZHAO_OFF_FLAGS, flags);
  put32(ZHAO_OFF_FRAME_ID, frame_id);
  put32(ZHAO_OFF_SEQUENCE, sequence);
  put32(ZHAO_OFF_RESOURCE_EPOCH, resource_epoch);
  put32(ZHAO_OFF_DEADLINE, deadline_cycles);
  put32(ZHAO_OFF_COMMAND_COUNT, count_);
  put32(ZHAO_OFF_COMMAND_BYTES, static_cast<uint32_t>(stream_.size()));
  std::memcpy(out.data() + ZHAO_FRAME_HEADER_BYTES, stream_.data(), stream_.size());
  put32(ZHAO_OFF_HEADER_CRC, zhao_crc32c(0, out.data(), 32));
  put32(ZHAO_FRAME_HEADER_BYTES + static_cast<uint32_t>(stream_.size()),
        zhao_crc32c(0, out.data() + ZHAO_FRAME_HEADER_BYTES, stream_.size()));
  return out;
}

// ------------------------------------------------------------- executor ----

ZhaoExecutionResult zhao_frame_execute_empty(const uint8_t* pkt, size_t len, uint32_t slot_bytes) {
  ZhaoExecutionResult res;
  const ZhaoValidateResult v = zhao_frame_validate(pkt, len, slot_bytes);
  res.counters.bytes_consumed = v.bytes_consumed;
  if (v.error != ZH_ABI_OK) {
    res.status = static_cast<uint8_t>(v.error);
    res.completion_flags = ZHAO_COMPL_ERR;
    res.counters.frames_rejected = 1;
    return res;
  }

  // walk records: implemented commands execute as no-ops with counters
  const ZhaoFrameHeader hdr = zhao_frame_parse_header(pkt);
  const uint8_t* stream = pkt + ZHAO_FRAME_HEADER_BYTES;
  uint32_t off = 0;
  while (off < hdr.command_bytes) {
    const uint16_t opcode = rd16(stream + off);
    const uint32_t rec_bytes = rd16(stream + off + 2);
    const ZhCommandInfo* info = zhao_command_info(opcode);
    if (info == nullptr || !info->implemented) {
      // valid packet, but execution of a reserved command is not implemented
      res.status = static_cast<uint8_t>(ZH_ABI_UNIMPLEMENTED_COMMAND);
      res.completion_flags = ZHAO_COMPL_ERR;
      res.counters.frames_rejected = 1;
      return res;
    }
    switch (opcode) {
      case ZHAO_OP_BEGIN_FRAME:
        res.counters.begin_frames++;
        break;
      case ZHAO_OP_END_FRAME:
        res.counters.end_frames++;
        break;
      case ZHAO_OP_NOP:
        res.counters.nops++;
        break;
      default:
        break;  // implemented but semantically a no-op in Phase 1
    }
    off += rec_bytes;
    res.counters.commands_total++;
  }

  res.status = static_cast<uint8_t>(ZH_ABI_OK);
  res.completion_flags = ZHAO_COMPL_DONE;
  res.counters.frames_accepted = 1;
  return res;
}

// ----------------------------------------------------------------- .zcap ---

namespace {

void wr16(std::vector<uint8_t>& v, uint32_t off, uint16_t x) {
  v[off] = uint8_t(x & 0xFF);
  v[off + 1] = uint8_t(x >> 8);
}
void wr32(std::vector<uint8_t>& v, uint32_t off, uint32_t x) {
  for (int i = 0; i < 4; i++) v[off + i] = uint8_t(x >> (8 * i));
}
void wr64(std::vector<uint8_t>& v, uint32_t off, uint64_t x) {
  for (int i = 0; i < 8; i++) v[off + i] = uint8_t(x >> (8 * i));
}

}  // namespace

ZhaoZcapWriter::ZhaoZcapWriter(const std::string& path) : path_(path) {}

ZhaoZcapWriter::~ZhaoZcapWriter() = default;

void ZhaoZcapWriter::add_section(uint16_t type, uint16_t version, const uint8_t* body, size_t n,
                                 bool crc_present) {
  entries_.push_back(Entry{type, version,
                           crc_present ? uint16_t(ZHAO_ZCAP_SECTION_CRC_PRESENT) : uint16_t(0), 0,
                           n, crc_present ? zhao_crc32c(0, body, n) : 0});
  bodies_.emplace_back(body, body + n);
  count_++;
}

void ZhaoZcapWriter::add_section(uint16_t type, uint16_t version, const std::vector<uint8_t>& body,
                                 bool crc_present) {
  add_section(type, version, body.data(), body.size(), crc_present);
}

bool ZhaoZcapWriter::close() {
  if (closed_) {
    error_ = "close() called twice";
    return false;
  }
  closed_ = true;

  // Logical view (spec 4.4): placeholder header -> bodies -> section table ->
  // backpatch section_count / total_file_length / header CRC (last). Phase 1
  // serializes that layout in one pass over the buffered sections; the
  // observable file is exactly the spec's, regardless of syscall pattern.
  const uint32_t table_bytes = count_ * ZHAO_ZCAP_ENTRY_BYTES;
  uint64_t body_off = ZHAO_ZCAP_HEADER_BYTES + table_bytes;
  for (size_t i = 0; i < entries_.size(); i++) {
    entries_[i].body_offset = body_off;
    body_off += bodies_[i].size();
  }
  const uint64_t total = body_off;

  std::vector<uint8_t> out(static_cast<size_t>(total), 0);
  wr32(out, 0, ZHAO_ZCAP_MAGIC);
  wr16(out, 4, ZHAO_ZCAP_FORMAT_VERSION);
  wr16(out, 6, ZHAO_ZCAP_FLAG_LITTLE_ENDIAN);
  wr32(out, 12, count_);
  wr32(out, 16, ZHAO_ZCAP_HEADER_BYTES);
  wr32(out, 20, ZHAO_ZCAP_ENTRY_BYTES);
  wr64(out, 24, total);
  uint32_t eoff = ZHAO_ZCAP_HEADER_BYTES;
  for (const Entry& e : entries_) {
    wr16(out, eoff + 0, e.type);
    wr16(out, eoff + 2, e.version);
    wr16(out, eoff + 4, e.flags);
    wr16(out, eoff + 6, 0);
    wr64(out, eoff + 8, e.body_offset);
    wr64(out, eoff + 16, e.body_length);
    wr32(out, eoff + 24, e.crc);
    wr32(out, eoff + 28, 0);
    eoff += ZHAO_ZCAP_ENTRY_BYTES;
  }
  for (size_t i = 0; i < bodies_.size(); i++) {
    std::memcpy(out.data() + entries_[i].body_offset, bodies_[i].data(), bodies_[i].size());
  }
  wr32(out, 8, zhao_crc32c(0, out.data(), 8));  // backpatch: header CRC last

  std::FILE* f = std::fopen(path_.c_str(), "wb");
  if (f == nullptr) {
    error_ = "cannot open " + path_ + " for writing";
    return false;
  }
  const bool ok = std::fwrite(out.data(), 1, out.size(), f) == out.size();
  std::fclose(f);
  if (!ok) error_ = "short write on " + path_;
  return ok;
}

ZhaoZcapReader::ZhaoZcapReader(const std::string& path) : path_(path) {}

ZhaoZcapError ZhaoZcapReader::open() {
  std::FILE* f = std::fopen(path_.c_str(), "rb");
  if (f == nullptr) {
    error_ = "cannot open " + path_;
    return ZhaoZcapError::kIo;
  }

  std::vector<uint8_t> file;
  uint8_t chunk[65536];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
    file.insert(file.end(), chunk, chunk + n);
  }
  std::fclose(f);

  if (file.size() < ZHAO_ZCAP_HEADER_BYTES) return ZhaoZcapError::kBadMagic;
  if (rd32(file.data()) != ZHAO_ZCAP_MAGIC) return ZhaoZcapError::kBadMagic;
  if ((rd16(file.data() + 6) & ZHAO_ZCAP_FLAG_LITTLE_ENDIAN) == 0) {
    return ZhaoZcapError::kBadFlags;
  }
  if (zhao_crc32c(0, file.data(), 8) != rd32(file.data() + 8)) {
    return ZhaoZcapError::kBadHeaderCrc;
  }
  info_.format_version = rd16(file.data() + 4);
  if (info_.format_version != ZHAO_ZCAP_FORMAT_VERSION) return ZhaoZcapError::kBadMagic;
  const uint32_t count = rd32(file.data() + 12);
  const uint32_t table_off = rd32(file.data() + 16);
  const uint32_t entry_size = rd32(file.data() + 20);
  uint64_t total = 0;
  for (int i = 0; i < 8; i++) total |= uint64_t(file[24 + i]) << (8 * i);
  if (entry_size != ZHAO_ZCAP_ENTRY_BYTES || table_off != ZHAO_ZCAP_HEADER_BYTES) {
    return ZhaoZcapError::kBadTable;
  }
  if (uint64_t(table_off) + uint64_t(count) * entry_size > file.size() || total != file.size()) {
    return ZhaoZcapError::kBadTable;
  }

  info_.total_file_length = total;
  info_.sections.clear();
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t* e = file.data() + table_off + i * entry_size;
    ZhaoZcapSectionInfo s;
    s.type = rd16(e + 0);
    s.version = rd16(e + 2);
    s.flags = rd16(e + 4);
    for (int k = 0; k < 8; k++) {
      s.body_offset |= uint64_t(e[8 + k]) << (8 * k);
      s.body_length |= uint64_t(e[16 + k]) << (8 * k);
    }
    s.crc = rd32(e + 24);
    info_.sections.push_back(s);
  }

  // keep the file for random-access body reads
  file_ = std::move(file);
  return ZhaoZcapError::kOk;
}

const ZhaoZcapSectionInfo* ZhaoZcapReader::find(uint16_t type) const {
  for (const auto& s : info_.sections) {
    if (s.type == type) return &s;
  }
  return nullptr;
}

bool ZhaoZcapReader::read_body(const ZhaoZcapSectionInfo& s, std::vector<uint8_t>& out) {
  if (s.body_offset + s.body_length > file_.size()) {
    error_ = "section body out of file bounds";
    return false;
  }
  out.assign(file_.begin() + static_cast<long>(s.body_offset),
             file_.begin() + static_cast<long>(s.body_offset + s.body_length));
  if ((s.flags & ZHAO_ZCAP_SECTION_CRC_PRESENT) != 0) {
    if (zhao_crc32c(0, out.data(), out.size()) != s.crc) {
      error_ = "section CRC mismatch";
      return false;
    }
  }
  return true;
}

// ------------------------------------------------------- section payloads --

std::vector<uint8_t> zhao_zcap_build_abi_info() {
  std::vector<uint8_t> body(88, 0);
  wr32(body, 0, ZHAO_ABI_VERSION);
  wr32(body, 4, ZHAO_ZCAP_SCHEMA_VERSION);
  for (size_t i = 0; i < 16 && ZHAO_GENERATOR_NAME[i] != '\0'; i++) {
    body[8 + i] = uint8_t(ZHAO_GENERATOR_NAME[i]);
  }
  std::memcpy(body.data() + 24, ZHAO_GENERATOR_SHA256, 32);
  std::memcpy(body.data() + 56, ZHAO_ZIDL_SHA256, 32);
  return body;
}

ZhaoZcapAbiInfo zhao_zcap_parse_abi_info(const uint8_t* body, size_t n) {
  ZhaoZcapAbiInfo info;
  if (n != 88) return info;
  info.abi_version = rd32(body);
  info.zcap_schema_version = rd32(body + 4);
  size_t name_end = 8;
  while (name_end < 24 && body[name_end] != 0) name_end++;
  info.generator_name.assign(reinterpret_cast<const char*>(body + 8), name_end - 8);
  std::memcpy(info.generator_sha256.data(), body + 24, 32);
  std::memcpy(info.zidl_sha256.data(), body + 56, 32);
  return info;
}

namespace {

bool zmap_utf8(const uint8_t* bytes, size_t size) {
  size_t i = 0;
  while (i < size) {
    const uint8_t first = bytes[i++];
    if (first <= 0x7Fu) continue;
    if (first >= 0xC2u && first <= 0xDFu) {
      if (i >= size || (bytes[i++] & 0xC0u) != 0x80u) return false;
      continue;
    }
    if (first >= 0xE0u && first <= 0xEFu) {
      if (i + 1u >= size) return false;
      const uint8_t second = bytes[i++];
      const uint8_t third = bytes[i++];
      if ((third & 0xC0u) != 0x80u) return false;
      if (first == 0xE0u) {
        if (second < 0xA0u || second > 0xBFu) return false;
      } else if (first == 0xEDu) {
        if (second < 0x80u || second > 0x9Fu) return false;
      } else if ((second & 0xC0u) != 0x80u) {
        return false;
      }
      continue;
    }
    if (first >= 0xF0u && first <= 0xF4u) {
      if (i + 2u >= size) return false;
      const uint8_t second = bytes[i++];
      const uint8_t third = bytes[i++];
      const uint8_t fourth = bytes[i++];
      if ((third & 0xC0u) != 0x80u || (fourth & 0xC0u) != 0x80u) return false;
      if (first == 0xF0u) {
        if (second < 0x90u || second > 0xBFu) return false;
      } else if (first == 0xF4u) {
        if (second < 0x80u || second > 0x8Fu) return false;
      } else if ((second & 0xC0u) != 0x80u) {
        return false;
      }
      continue;
    }
    return false;
  }
  return true;
}

bool zmap_text_ok(const std::string& text) {
  return text.find('\0') == std::string::npos &&
         zmap_utf8(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

ZhaoSourceMapBuildResult zmap_build_fail(ZhaoSourceMapError error, const char* diagnostic) {
  ZhaoSourceMapBuildResult result;
  result.error = error;
  result.diagnostic = diagnostic;
  return result;
}

ZhaoSourceMapParseResult zmap_parse_fail(ZhaoSourceMapError error, const char* diagnostic) {
  ZhaoSourceMapParseResult result;
  result.error = error;
  result.diagnostic = diagnostic;
  return result;
}

}  // namespace

ZhaoSourceMapSizeResult zhao_source_map_v1_byte_length(uint64_t entry_count, uint64_t file_count,
                                                       uint64_t string_blob_bytes) {
  ZhaoSourceMapSizeResult result;
  constexpr uint64_t kU32Max = std::numeric_limits<uint32_t>::max();
  if (entry_count > kU32Max || file_count > kU32Max || string_blob_bytes > kU32Max) {
    result.error = ZhaoSourceMapError::kSizeOverflow;
    return result;
  }
  const uint64_t body_bytes =
      entry_count * ZHAO_ZMAP_ENTRY_BYTES + file_count * ZHAO_ZMAP_FILE_BYTES + string_blob_bytes;
  if (body_bytes > kU32Max) {
    result.error = ZhaoSourceMapError::kSizeOverflow;
    return result;
  }
  result.bytes = ZHAO_ZMAP_HEADER_BYTES + body_bytes;
  return result;
}

ZhaoSourceMapError zhao_source_map_v1_admit_byte_length(uint64_t bytes) {
  return bytes > ZHAO_ZMAP_MAX_BYTES ? ZhaoSourceMapError::kTooLarge : ZhaoSourceMapError::kOk;
}

ZhaoSourceMapBuildResult zhao_zcap_build_source_map(const ZhaoSourceMap& map) {
  if (map.entries.size() > std::numeric_limits<uint32_t>::max() ||
      map.files.size() > std::numeric_limits<uint32_t>::max()) {
    return zmap_build_fail(ZhaoSourceMapError::kSizeOverflow,
                           "sourceids.zmap exceeds v1 u32 size limits");
  }
  if (map.files.size() > 0x1000u) {
    return zmap_build_fail(ZhaoSourceMapError::kInvalidInput,
                           "sourceids.zmap: file count exceeds module range");
  }

  std::vector<ZhaoSourceMapEntry> entries = map.entries;
  std::sort(entries.begin(), entries.end(),
            [](const ZhaoSourceMapEntry& a, const ZhaoSourceMapEntry& b) {
              return a.source_id < b.source_id;
            });
  bool any_hash = false;
  uint32_t previous = 0;
  bool have_previous = false;
  for (const auto& entry : entries) {
    if (have_previous && entry.source_id == previous) {
      return zmap_build_fail(ZhaoSourceMapError::kEntriesNotAscending,
                             "sourceids.zmap: duplicate source id");
    }
    have_previous = true;
    previous = entry.source_id;
    const uint16_t module = static_cast<uint16_t>((entry.source_id >> 16) & 0x0FFFu);
    if ((entry.source_id >> 28) != entry.kind) {
      return zmap_build_fail(ZhaoSourceMapError::kKindMismatch,
                             "sourceids.zmap: denormalized kind mismatch");
    }
    if (entry.module_id != module || entry.file_index != entry.module_id ||
        entry.file_index >= map.files.size()) {
      return zmap_build_fail(ZhaoSourceMapError::kModuleFileMismatch,
                             "sourceids.zmap: denormalized module/file");
    }
    if (entry.file != map.files[entry.file_index]) {
      return zmap_build_fail(ZhaoSourceMapError::kModuleFileMismatch,
                             "sourceids.zmap: entry file does not match file table");
    }
    const uint8_t expected_flags = entry.program_hash.has_value() ? 1u : 0u;
    if (entry.flags != expected_flags) {
      return zmap_build_fail(ZhaoSourceMapError::kReservedEntryBits,
                             "sourceids.zmap: denormalized program-hash flags");
    }
    if (entry.span_end < entry.span_begin) {
      return zmap_build_fail(ZhaoSourceMapError::kReversedSpan, "sourceids.zmap: reversed span");
    }
    if (!zmap_text_ok(entry.name) || !zmap_text_ok(entry.file)) {
      return zmap_build_fail(ZhaoSourceMapError::kInvalidUtf8,
                             "sourceids.zmap: strings must be NUL-free UTF-8");
    }
    any_hash = any_hash || entry.program_hash.has_value();
  }
  for (const auto& file : map.files) {
    if (!zmap_text_ok(file)) {
      return zmap_build_fail(ZhaoSourceMapError::kInvalidUtf8,
                             "sourceids.zmap: strings must be NUL-free UTF-8");
    }
  }

  std::unordered_map<std::string, uint32_t> interned;
  std::vector<std::string> strings;
  uint64_t blob_bytes = 0;
  const auto intern = [&](const std::string& text, uint32_t& offset) -> bool {
    const auto found = interned.find(text);
    if (found != interned.end()) {
      offset = found->second;
      return true;
    }
    if (text.size() > std::numeric_limits<uint32_t>::max() ||
        blob_bytes + static_cast<uint64_t>(text.size()) + 1u >
            std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    offset = static_cast<uint32_t>(blob_bytes);
    blob_bytes += static_cast<uint64_t>(text.size()) + 1u;
    interned.emplace(text, offset);
    strings.push_back(text);
    return true;
  };

  std::vector<uint32_t> file_offsets(map.files.size());
  std::vector<uint32_t> name_offsets(entries.size());
  for (size_t i = 0; i < map.files.size(); ++i) {
    if (!intern(map.files[i], file_offsets[i])) {
      return zmap_build_fail(ZhaoSourceMapError::kSizeOverflow,
                             "sourceids.zmap exceeds v1 u32 size limits");
    }
  }
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!intern(entries[i].name, name_offsets[i])) {
      return zmap_build_fail(ZhaoSourceMapError::kSizeOverflow,
                             "sourceids.zmap exceeds v1 u32 size limits");
    }
  }

  const auto layout = zhao_source_map_v1_byte_length(entries.size(), map.files.size(), blob_bytes);
  if (!layout.ok()) {
    return zmap_build_fail(layout.error, "sourceids.zmap exceeds v1 u32 size limits");
  }
  if (zhao_source_map_v1_admit_byte_length(layout.bytes) != ZhaoSourceMapError::kOk) {
    return zmap_build_fail(ZhaoSourceMapError::kTooLarge,
                           "sourceids.zmap exceeds v1 128 MiB global byte limit");
  }

  ZhaoSourceMapBuildResult result;
  result.bytes.assign(static_cast<size_t>(layout.bytes), 0u);
  auto& out = result.bytes;
  wr32(out, 0, ZHAO_ZMAP_MAGIC);
  wr16(out, 4, ZHAO_ZMAP_VERSION);
  wr16(out, 6, any_hash ? 1u : 0u);
  wr32(out, 8, static_cast<uint32_t>(entries.size()));
  wr32(out, 12, static_cast<uint32_t>(map.files.size()));
  wr32(out, 16, static_cast<uint32_t>(blob_bytes));
  wr64(out, 24, 0u);

  size_t offset = ZHAO_ZMAP_HEADER_BYTES;
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    wr32(out, static_cast<uint32_t>(offset + 0u), entry.source_id);
    wr16(out, static_cast<uint32_t>(offset + 4u), entry.file_index);
    out[offset + 6u] = entry.kind;
    out[offset + 7u] = entry.flags;
    wr32(out, static_cast<uint32_t>(offset + 8u), entry.span_begin);
    wr32(out, static_cast<uint32_t>(offset + 12u), entry.span_end);
    wr32(out, static_cast<uint32_t>(offset + 16u), name_offsets[i]);
    wr32(out, static_cast<uint32_t>(offset + 20u), entry.program_hash.value_or(0u));
    offset += ZHAO_ZMAP_ENTRY_BYTES;
  }
  for (size_t i = 0; i < map.files.size(); ++i) {
    wr32(out, static_cast<uint32_t>(offset), file_offsets[i]);
    wr32(out, static_cast<uint32_t>(offset + 4u), 0u);
    offset += ZHAO_ZMAP_FILE_BYTES;
  }
  for (const auto& text : strings) {
    std::memcpy(out.data() + offset, text.data(), text.size());
    offset += text.size() + 1u;
  }
  wr32(out, 20,
       zhao_crc32c(0, out.data() + ZHAO_ZMAP_HEADER_BYTES, out.size() - ZHAO_ZMAP_HEADER_BYTES));
  return result;
}

ZhaoSourceMapParseResult zhao_zcap_parse_source_map(const uint8_t* body, size_t n) {
  if (zhao_source_map_v1_admit_byte_length(n) != ZhaoSourceMapError::kOk) {
    return zmap_parse_fail(ZhaoSourceMapError::kTooLarge,
                           "sourceids.zmap exceeds v1 128 MiB global byte limit");
  }
  if (body == nullptr && n != 0u) {
    return zmap_parse_fail(ZhaoSourceMapError::kNullBody, "sourceids.zmap: null body");
  }
  if (n < ZHAO_ZMAP_HEADER_BYTES) {
    return zmap_parse_fail(ZhaoSourceMapError::kTruncatedHeader,
                           "sourceids.zmap: truncated header");
  }
  if (rd32(body) != ZHAO_ZMAP_MAGIC) {
    return zmap_parse_fail(ZhaoSourceMapError::kBadMagic, "sourceids.zmap: bad magic");
  }
  if (rd16(body + 4u) != ZHAO_ZMAP_VERSION) {
    return zmap_parse_fail(ZhaoSourceMapError::kUnsupportedVersion,
                           "sourceids.zmap: unsupported version");
  }
  const uint16_t header_flags = rd16(body + 6u);
  if ((header_flags & ~1u) != 0u) {
    return zmap_parse_fail(ZhaoSourceMapError::kReservedFlags, "sourceids.zmap: reserved flags");
  }
  uint64_t reserved = 0;
  for (size_t i = 0; i < 8u; ++i) reserved |= static_cast<uint64_t>(body[24u + i]) << (8u * i);
  if (reserved != 0u) {
    return zmap_parse_fail(ZhaoSourceMapError::kReservedHeader,
                           "sourceids.zmap: nonzero reserved header");
  }

  const uint32_t entry_count = rd32(body + 8u);
  const uint32_t file_count = rd32(body + 12u);
  const uint32_t blob_bytes = rd32(body + 16u);
  const auto layout = zhao_source_map_v1_byte_length(entry_count, file_count, blob_bytes);
  if (!layout.ok() || layout.bytes != n) {
    return zmap_parse_fail(ZhaoSourceMapError::kInconsistentLengths,
                           "sourceids.zmap: inconsistent lengths");
  }
  const uint64_t entry_bytes = static_cast<uint64_t>(entry_count) * ZHAO_ZMAP_ENTRY_BYTES;
  const uint64_t file_bytes = static_cast<uint64_t>(file_count) * ZHAO_ZMAP_FILE_BYTES;
  if (zhao_crc32c(0, body + ZHAO_ZMAP_HEADER_BYTES, n - ZHAO_ZMAP_HEADER_BYTES) !=
      rd32(body + 20u)) {
    return zmap_parse_fail(ZhaoSourceMapError::kBodyCrcMismatch,
                           "sourceids.zmap: body CRC mismatch");
  }

  ZhaoSourceMap parsed;
  parsed.flags = header_flags;
  parsed.entries.reserve(entry_count);
  std::vector<uint32_t> name_offsets;
  name_offsets.reserve(entry_count);
  size_t offset = ZHAO_ZMAP_HEADER_BYTES;
  uint32_t previous = 0;
  bool have_previous = false;
  bool any_hash = false;
  for (uint32_t i = 0; i < entry_count; ++i) {
    ZhaoSourceMapEntry entry;
    entry.source_id = rd32(body + offset);
    entry.file_index = rd16(body + offset + 4u);
    entry.kind = body[offset + 6u];
    entry.flags = body[offset + 7u];
    entry.span_begin = rd32(body + offset + 8u);
    entry.span_end = rd32(body + offset + 12u);
    const uint32_t name_offset = rd32(body + offset + 16u);
    const uint32_t hash = rd32(body + offset + 20u);
    entry.module_id = static_cast<uint16_t>((entry.source_id >> 16) & 0x0FFFu);
    if (have_previous && entry.source_id <= previous) {
      return zmap_parse_fail(ZhaoSourceMapError::kEntriesNotAscending,
                             "sourceids.zmap: entries not strictly ascending");
    }
    have_previous = true;
    previous = entry.source_id;
    if ((entry.source_id >> 28) != entry.kind) {
      return zmap_parse_fail(ZhaoSourceMapError::kKindMismatch,
                             "sourceids.zmap: denormalized kind mismatch");
    }
    if (entry.file_index >= file_count) {
      return zmap_parse_fail(ZhaoSourceMapError::kFileIndexOutsideTable,
                             "sourceids.zmap: file index outside table");
    }
    if (entry.module_id != entry.file_index) {
      return zmap_parse_fail(ZhaoSourceMapError::kModuleFileMismatch,
                             "sourceids.zmap: module/file index mismatch");
    }
    if ((entry.flags & ~1u) != 0u) {
      return zmap_parse_fail(ZhaoSourceMapError::kReservedEntryBits,
                             "sourceids.zmap: reserved entry bits");
    }
    if (entry.span_end < entry.span_begin) {
      return zmap_parse_fail(ZhaoSourceMapError::kReversedSpan, "sourceids.zmap: reversed span");
    }
    if ((entry.flags & 1u) == 0u && hash != 0u) {
      return zmap_parse_fail(ZhaoSourceMapError::kHashWithoutFlag,
                             "sourceids.zmap: hash without flag");
    }
    if ((entry.flags & 1u) != 0u) entry.program_hash = hash;
    any_hash = any_hash || entry.program_hash.has_value();
    parsed.entries.push_back(std::move(entry));
    name_offsets.push_back(name_offset);
    offset += ZHAO_ZMAP_ENTRY_BYTES;
  }
  if (((header_flags & 1u) != 0u) != any_hash) {
    return zmap_parse_fail(ZhaoSourceMapError::kProgramHashHeaderMismatch,
                           "sourceids.zmap: program-hash header flag mismatch");
  }

  const size_t blob_start = ZHAO_ZMAP_HEADER_BYTES + static_cast<size_t>(entry_bytes + file_bytes);
  const auto read_string = [&](uint32_t relative, std::string& text, ZhaoSourceMapError& error,
                               const char*& diagnostic) -> bool {
    if (relative >= blob_bytes) {
      error = ZhaoSourceMapError::kStringOffsetOutsideBlob;
      diagnostic = "sourceids.zmap: string offset outside blob";
      return false;
    }
    const size_t begin = blob_start + relative;
    size_t end = begin;
    while (end < n && body[end] != 0u) ++end;
    if (end == n) {
      error = ZhaoSourceMapError::kUnterminatedString;
      diagnostic = "sourceids.zmap: unterminated string";
      return false;
    }
    if (!zmap_utf8(body + begin, end - begin)) {
      error = ZhaoSourceMapError::kInvalidUtf8;
      diagnostic = "sourceids.zmap: invalid UTF-8 string";
      return false;
    }
    text.assign(reinterpret_cast<const char*>(body + begin), end - begin);
    return true;
  };

  std::vector<uint32_t> path_offsets;
  path_offsets.reserve(file_count);
  for (uint32_t i = 0; i < file_count; ++i) {
    path_offsets.push_back(rd32(body + offset));
    if (rd32(body + offset + 4u) != 0u) {
      return zmap_parse_fail(ZhaoSourceMapError::kNonzeroFileReserved,
                             "sourceids.zmap: nonzero file reserved word");
    }
    offset += ZHAO_ZMAP_FILE_BYTES;
  }
  parsed.files.reserve(file_count);
  for (const uint32_t path_offset : path_offsets) {
    std::string file;
    ZhaoSourceMapError error = ZhaoSourceMapError::kOk;
    const char* diagnostic = "";
    if (!read_string(path_offset, file, error, diagnostic)) {
      return zmap_parse_fail(error, diagnostic);
    }
    parsed.files.push_back(std::move(file));
  }
  for (size_t i = 0; i < parsed.entries.size(); ++i) {
    auto& entry = parsed.entries[i];
    ZhaoSourceMapError error = ZhaoSourceMapError::kOk;
    const char* diagnostic = "";
    if (!read_string(name_offsets[i], entry.name, error, diagnostic)) {
      return zmap_parse_fail(error, diagnostic);
    }
    entry.file = parsed.files[entry.file_index];
  }

  ZhaoSourceMapParseResult result;
  result.map = std::move(parsed);
  return result;
}

std::vector<uint8_t> zhao_zcap_build_resource_pages(const std::vector<ZhaoResourcePage>& pages) {
  std::vector<uint8_t> body(4 + pages.size() * 112, 0);
  wr32(body, 0, static_cast<uint32_t>(pages.size()));
  uint32_t off = 4;
  for (const auto& p : pages) {
    body[off] = p.kind;
    wr32(body, off + 4, p.page_id);
    wr64(body, off + 8, p.byte_length);
    std::memcpy(body.data() + off + 16, p.sha256.data(), 32);
    for (size_t i = 0; i < 64; i++) {
      body[off + 48 + i] = i < p.ref.size() ? uint8_t(p.ref[i]) : 0;
    }
    off += 112;
  }
  return body;
}

std::vector<ZhaoResourcePage> zhao_zcap_parse_resource_pages(const uint8_t* body, size_t n) {
  std::vector<ZhaoResourcePage> out;
  if (n < 4) return out;
  const uint32_t count = rd32(body);
  if (4 + uint64_t(count) * 112 > n) return out;
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t* e = body + 4 + i * 112;
    ZhaoResourcePage p;
    p.kind = e[0];
    p.page_id = rd32(e + 4);
    for (int k = 0; k < 8; k++) p.byte_length |= uint64_t(e[8 + k]) << (8 * k);
    std::memcpy(p.sha256.data(), e + 16, 32);
    size_t end = 48;
    while (end < 112 && e[end] != 0) end++;
    p.ref.assign(reinterpret_cast<const char*>(e + 48), end - 48);
    out.push_back(std::move(p));
  }
  return out;
}

}  // namespace zhao
