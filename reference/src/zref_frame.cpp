// zref_frame.cpp — ZRef sealed frame packet + .zcap container implementation.
// Law: spec/capture_format.md (3 frame packet, 3.2 fail-safe order, 4 .zcap,
// 5 source IDs). Layout constants/CRC/error codes come from the generated
// zhao_abi.h. This is the C++ testee of the tri-language conformance suite.

#include "zref/zref_frame.hpp"

#include <cstring>

namespace zhao {

using namespace zhao_abi;

// ---------------------------------------------------------------- frame ----

namespace {

uint16_t rd16(const uint8_t* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
uint32_t rd32(const uint8_t* p) {
  return rd16(p) | (uint32_t(rd16(p + 2)) << 16);
}

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
      case ZHAO_OP_BEGIN_FRAME: res.counters.begin_frames++; break;
      case ZHAO_OP_END_FRAME: res.counters.end_frames++; break;
      case ZHAO_OP_NOP: res.counters.nops++; break;
      default: break;  // implemented but semantically a no-op in Phase 1
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

void ZhaoZcapWriter::add_section(uint16_t type, uint16_t version, const uint8_t* body,
                                 size_t n, bool crc_present) {
  entries_.push_back(Entry{type, version,
                           crc_present ? uint16_t(ZHAO_ZCAP_SECTION_CRC_PRESENT) : uint16_t(0),
                           0, n, crc_present ? zhao_crc32c(0, body, n) : 0});
  bodies_.emplace_back(body, body + n);
  count_++;
}

void ZhaoZcapWriter::add_section(uint16_t type, uint16_t version,
                                 const std::vector<uint8_t>& body, bool crc_present) {
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

std::vector<uint8_t> zhao_zcap_build_source_map(const std::vector<ZhaoSourceMapEntry>& entries) {
  std::vector<uint8_t> blob;
  struct Off {
    uint16_t name, file;
  };
  std::vector<Off> offs;
  const auto put = [&](const std::string& s) -> uint16_t {
    const uint16_t off = static_cast<uint16_t>(blob.size());
    blob.insert(blob.end(), s.begin(), s.end());
    blob.push_back(0);
    return off;
  };
  for (const auto& e : entries) {
    Off o{put(e.name), put(e.file)};
    offs.push_back(o);
  }

  std::vector<uint8_t> body(4 + entries.size() * 16 + blob.size(), 0);
  wr32(body, 0, static_cast<uint32_t>(entries.size()));
  uint32_t off = 4;
  for (size_t i = 0; i < entries.size(); i++) {
    const auto& e = entries[i];
    wr32(body, off + 0, e.source_id);
    wr16(body, off + 4, e.module_id);
    body[off + 6] = e.kind;
    body[off + 7] = e.flags;
    wr32(body, off + 8, e.line);
    wr16(body, off + 12, offs[i].name);
    wr16(body, off + 14, offs[i].file);
    off += 16;
  }
  std::memcpy(body.data() + off, blob.data(), blob.size());
  return body;
}

std::vector<ZhaoSourceMapEntry> zhao_zcap_parse_source_map(const uint8_t* body, size_t n) {
  std::vector<ZhaoSourceMapEntry> out;
  if (n < 4) return out;
  const uint32_t count = rd32(body);
  const uint32_t blob_start = 4 + count * 16;
  if (blob_start > n) return out;
  const auto read_str = [&](uint16_t rel) {
    size_t p = blob_start + rel;
    std::string s;
    while (p < n && body[p] != 0) s.push_back(char(body[p++]));
    return s;
  };
  for (uint32_t i = 0; i < count; i++) {
    const uint8_t* e = body + 4 + i * 16;
    ZhaoSourceMapEntry entry;
    entry.source_id = rd32(e);
    entry.module_id = rd16(e + 4);
    entry.kind = e[6];
    entry.flags = e[7];
    entry.line = rd32(e + 8);
    entry.name = read_str(rd16(e + 12));
    entry.file = read_str(rd16(e + 14));
    out.push_back(std::move(entry));
  }
  return out;
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
