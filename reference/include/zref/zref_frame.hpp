// zref_frame.hpp — ZRef sealed frame packet + .zcap container (charter 20,
// spec/capture_format.md 3-4). The C++ testee of the tri-language
// conformance suite: must agree with the TS mirror (compiler/src/generated/
// frame.ts, zcap.ts) and the SV package (fpga/rtl/generated/zhao_abi_pkg.sv)
// on every golden and every fuzz-corpus case.
//
// Law: spec/capture_format.md. Layout constants, error codes, CRC-32C and
// sample records come from the GENERATED runtime/include/zhao_abi.h — never
// re-derived here (charter 29-5/29-6).

#pragma once

#include "zhao_abi.h"  // generated (runtime/include)

#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace zhao {

// ---------------------------------------------------------------- frame ----

/** Fail-safe validation result (capture_format.md 3.2 order). */
struct ZhaoValidateResult {
  zhao_abi::zhao_abi_error error = zhao_abi::ZH_ABI_OK;
  uint32_t commands_consumed = 0;
  /** 36 on header-level abort, else 40 + command_bytes (spec 3.2) */
  uint32_t bytes_consumed = 36;
};

/** Validate a sealed packet against the fail-safe order (spec 3.2). */
ZhaoValidateResult zhao_frame_validate(const uint8_t* pkt, size_t len,
                                       uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES);
ZhaoValidateResult zhao_frame_validate(const std::vector<uint8_t>& pkt,
                                       uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES);

/** Decoded frame header (post-validation view). */
struct ZhaoFrameHeader {
  uint32_t frame_id = 0;
  uint32_t sequence = 0;
  uint32_t resource_epoch = 0;
  uint32_t deadline_cycles = 0;
  uint32_t command_count = 0;
  uint32_t command_bytes = 0;
  uint16_t flags = 0;
};

/** Parse the sealed header of an already-validated packet. */
ZhaoFrameHeader zhao_frame_parse_header(const uint8_t* pkt);

/**
 * Frame builder — the C++ mirror of the TS ZhaoFrameBuilder. Append
 * serialized records (see zhao_abi::zhao_pack_*), then seal() computes both
 * CRCs per capture_format.md 3.
 */
class ZhaoFrameBuilder {
 public:
  ZhaoFrameBuilder& begin_frame(uint32_t frame_id, uint32_t resource_epoch, uint32_t flags,
                                uint32_t deadline_cycles, uint32_t source_id = 0);
  ZhaoFrameBuilder& nop(uint32_t source_id = 0);
  ZhaoFrameBuilder& end_frame(uint32_t completion_flags, uint32_t source_id = 0);
  ZhaoFrameBuilder& append_record(const uint8_t* bytes, size_t n);
  ZhaoFrameBuilder& append_record(const std::vector<uint8_t>& bytes);

  std::vector<uint8_t> seal(uint32_t frame_id, uint32_t sequence, uint32_t resource_epoch,
                            uint32_t deadline_cycles = 0, uint16_t flags = 0) const;
  uint32_t command_count() const { return count_; }
  uint32_t command_bytes() const { return static_cast<uint32_t>(stream_.size()); }

 private:
  std::vector<uint8_t> stream_;
  uint32_t count_ = 0;
};

// -------------------------------------------------------------- executor ---

/** Execution counters — the Phase-1 "empty ZRef frame loop" outputs. */
struct ZhaoFrameCounters {
  uint32_t frames_accepted = 0;
  uint32_t frames_rejected = 0;
  uint32_t bytes_consumed = 0;
  uint32_t commands_total = 0;
  uint32_t begin_frames = 0;
  uint32_t end_frames = 0;
  uint32_t nops = 0;
};

struct ZhaoExecutionResult {
  uint8_t status = 0;            // zhao_abi_error code of the frame
  uint8_t completion_flags = 0;  // ZHAO_COMPL_DONE / ZHAO_COMPL_ERR
  ZhaoFrameCounters counters;
};

/**
 * The empty ZRef frame loop (Phase 1, plan W4/W6): validate the sealed
 * packet per spec 3.2, then execute implemented commands as no-ops with
 * counters. bytes_consumed mirrors the RTL stub exactly (36 on header-level
 * abort, else the whole packet) so the differential replay can compare all
 * counters. A reserved command validates but reports
 * ZH_ABI_UNIMPLEMENTED_COMMAND at execution time.
 */
ZhaoExecutionResult zhao_frame_execute_empty(const uint8_t* pkt, size_t len,
                                             uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES);

// ----------------------------------------------------------------- .zcap ---

// Section types (capture_format.md 4.2)
enum ZhaoZcapSection : uint16_t {
  ZHAO_ZCAP_ABI_INFO = 0x0001,
  ZHAO_ZCAP_FRAME_PACKET = 0x0002,
  ZHAO_ZCAP_RESOURCE_PAGES = 0x0003,
  ZHAO_ZCAP_CONTROLLER_SNAPSHOT = 0x0004,
  ZHAO_ZCAP_FRAMEBUFFER_EXPECTED = 0x0005,
  ZHAO_ZCAP_TILE_CRC = 0x0006,
  ZHAO_ZCAP_DEPTH_STENCIL_CRC = 0x0007,
  ZHAO_ZCAP_COUNTERS = 0x0008,
  ZHAO_ZCAP_SOURCE_MAP = 0x0009,
  ZHAO_ZCAP_TRACE = 0x000A,
};
constexpr uint16_t ZHAO_ZCAP_TOOL_NAMESPACE_LO = 0x8000;

constexpr uint32_t ZHAO_ZCAP_MAGIC = 0x5041435Au;  // 'Z','C','A','P' LE
constexpr uint16_t ZHAO_ZCAP_FORMAT_VERSION = 1;
constexpr uint32_t ZHAO_ZCAP_HEADER_BYTES = 32;
constexpr uint32_t ZHAO_ZCAP_ENTRY_BYTES = 32;
constexpr uint16_t ZHAO_ZCAP_FLAG_LITTLE_ENDIAN = 0x0001;
constexpr uint16_t ZHAO_ZCAP_SECTION_CRC_PRESENT = 0x0001;

/** Seekable-file .zcap writer: append bodies, backpatch header at close. */
class ZhaoZcapWriter {
 public:
  explicit ZhaoZcapWriter(const std::string& path);
  ~ZhaoZcapWriter();

  ZhaoZcapWriter(const ZhaoZcapWriter&) = delete;
  ZhaoZcapWriter& operator=(const ZhaoZcapWriter&) = delete;

  /** Append a section body; computes its CRC incrementally. crc_present=false writes 0. */
  void add_section(uint16_t type, uint16_t version, const uint8_t* body, size_t n,
                   bool crc_present = true);
  void add_section(uint16_t type, uint16_t version, const std::vector<uint8_t>& body,
                   bool crc_present = true);

  /** Write the section table + backpatch the header (idempotent-ish: call once). */
  bool close();

  uint32_t section_count() const { return count_; }
  const std::string& error() const { return error_; }

 private:
  struct Entry {
    uint16_t type;
    uint16_t version;
    uint16_t flags;
    uint64_t body_offset;
    uint64_t body_length;
    uint32_t crc;
  };
  std::vector<Entry> entries_;
  std::vector<std::vector<uint8_t>> bodies_;  // buffered; serialized at close()
  std::string path_;
  std::string error_;
  uint32_t count_ = 0;
  bool closed_ = false;
};

struct ZhaoZcapSectionInfo {
  uint16_t type = 0;
  uint16_t version = 0;
  uint16_t flags = 0;
  uint64_t body_offset = 0;
  uint64_t body_length = 0;
  uint32_t crc = 0;
};

enum class ZhaoZcapError { kOk, kBadMagic, kBadFlags, kBadHeaderCrc, kBadTable, kIo };

struct ZhaoZcapFile {
  ZhaoZcapError error = ZhaoZcapError::kOk;
  uint16_t format_version = 0;
  uint64_t total_file_length = 0;
  std::vector<ZhaoZcapSectionInfo> sections;
};

/** Random-access reader: header + section table only; bodies fetched on demand. */
class ZhaoZcapReader {
 public:
  explicit ZhaoZcapReader(const std::string& path);

  ZhaoZcapError open();  // loads + validates header and table

  const ZhaoZcapFile& info() const { return info_; }
  const std::vector<ZhaoZcapSectionInfo>& sections() const { return info_.sections; }

  /** First section of a type, or nullptr. */
  const ZhaoZcapSectionInfo* find(uint16_t type) const;

  /** Read + CRC-verify a section body. Returns false on bounds/CRC failure. */
  bool read_body(const ZhaoZcapSectionInfo& s, std::vector<uint8_t>& out);

  const std::string& error() const { return error_; }

 private:
  std::string path_;
  ZhaoZcapFile info_;
  std::vector<uint8_t> file_;  // loaded at open(); bodies sliced on demand
  std::string error_;
};

// ------------------------------------------------- .zcap section payloads --

/** ABI_INFO (spec 4.2): identity of the ABI a capture was produced against. */
struct ZhaoZcapAbiInfo {
  uint32_t abi_version = 0;
  uint32_t zcap_schema_version = 0;
  std::string generator_name;
  std::array<uint8_t, 32> generator_sha256{};
  std::array<uint8_t, 32> zidl_sha256{};
};

std::vector<uint8_t> zhao_zcap_build_abi_info();  // from generated constants
ZhaoZcapAbiInfo zhao_zcap_parse_abi_info(const uint8_t* body, size_t n);

/** Canonical ZSMP v1 SOURCE_MAP (capture_format.md §7). */
constexpr uint32_t ZHAO_ZMAP_MAGIC = 0x504D535Au;
constexpr uint16_t ZHAO_ZMAP_VERSION = 1u;
constexpr size_t ZHAO_ZMAP_HEADER_BYTES = 32u;
constexpr size_t ZHAO_ZMAP_ENTRY_BYTES = 24u;
constexpr size_t ZHAO_ZMAP_FILE_BYTES = 8u;
constexpr size_t ZHAO_ZMAP_MAX_BYTES = 128u * 1024u * 1024u;

struct ZhaoSourceMapEntry {
  uint32_t source_id = 0;
  uint16_t module_id = 0;
  uint16_t file_index = 0;
  uint8_t kind = 0;
  uint8_t flags = 0;
  uint32_t span_begin = 0;
  uint32_t span_end = 0;
  std::string name;
  std::string file;
  std::optional<uint32_t> program_hash;
};

struct ZhaoSourceMap {
  uint16_t flags = 0;
  std::vector<ZhaoSourceMapEntry> entries;
  std::vector<std::string> files;
};

enum class ZhaoSourceMapError {
  kOk,
  kNullBody,
  kTooLarge,
  kTruncatedHeader,
  kBadMagic,
  kUnsupportedVersion,
  kReservedFlags,
  kReservedHeader,
  kInconsistentLengths,
  kBodyCrcMismatch,
  kEntriesNotAscending,
  kKindMismatch,
  kFileIndexOutsideTable,
  kModuleFileMismatch,
  kReservedEntryBits,
  kReversedSpan,
  kHashWithoutFlag,
  kProgramHashHeaderMismatch,
  kNonzeroFileReserved,
  kStringOffsetOutsideBlob,
  kUnterminatedString,
  kInvalidUtf8,
  kInvalidInput,
  kSizeOverflow,
};

struct ZhaoSourceMapSizeResult {
  ZhaoSourceMapError error = ZhaoSourceMapError::kOk;
  uint64_t bytes = 0;
  bool ok() const { return error == ZhaoSourceMapError::kOk; }
};

/** Non-allocating, wide v1 layout arithmetic and global byte admission. */
ZhaoSourceMapSizeResult zhao_source_map_v1_byte_length(
    uint64_t entry_count, uint64_t file_count, uint64_t string_blob_bytes);
ZhaoSourceMapError zhao_source_map_v1_admit_byte_length(uint64_t bytes);

struct ZhaoSourceMapParseResult {
  ZhaoSourceMapError error = ZhaoSourceMapError::kOk;
  std::string diagnostic;
  ZhaoSourceMap map;
  bool ok() const { return error == ZhaoSourceMapError::kOk; }
};

struct ZhaoSourceMapBuildResult {
  ZhaoSourceMapError error = ZhaoSourceMapError::kOk;
  std::string diagnostic;
  std::vector<uint8_t> bytes;
  bool ok() const { return error == ZhaoSourceMapError::kOk; }
};

ZhaoSourceMapBuildResult zhao_zcap_build_source_map(const ZhaoSourceMap& map);
ZhaoSourceMapParseResult zhao_zcap_parse_source_map(const uint8_t* body, size_t n);
inline ZhaoSourceMapParseResult zhao_zcap_parse_source_map(const std::vector<uint8_t>& body) {
  return zhao_zcap_parse_source_map(body.data(), body.size());
}

/** RESOURCE_PAGES entry (spec 4.2): page references + program hashes. */
struct ZhaoResourcePage {
  uint8_t kind = 0;
  uint32_t page_id = 0;
  uint64_t byte_length = 0;
  std::array<uint8_t, 32> sha256{};
  std::string ref;  // NUL-terminated reference string, <= 64 bytes
};

std::vector<uint8_t> zhao_zcap_build_resource_pages(const std::vector<ZhaoResourcePage>& pages);
std::vector<ZhaoResourcePage> zhao_zcap_parse_resource_pages(const uint8_t* body, size_t n);

// ------------------------------------------------------------- source ids --

/** Encode/decode the {kind:4, module:12, index:16} scheme (spec 5). */
constexpr uint32_t zhao_source_id(uint32_t kind, uint32_t module, uint32_t index) {
  return (kind << 28) | (module << 16) | index;
}
inline void zhao_source_id_decode(uint32_t id, uint32_t& kind, uint32_t& module, uint32_t& index) {
  kind = id >> 28;
  module = (id >> 16) & 0xFFF;
  index = id & 0xFFFF;
}

}  // namespace zhao
