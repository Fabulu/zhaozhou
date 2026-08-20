// zref_cmd.hpp — CMD.DECODER's declared reference model.
//
// A THIN VIEW ONTO AN EXISTING RATIFIED LAW, NOT A SECOND IMPLEMENTATION.
//
// The ledger declared `zref::CmdDecoder`, and that symbol never existed —
// the eighth phantom reference_model found in this tree. But unlike a genuine
// gap, the LAW was already implemented and shipped: `zhao::zhao_frame_validate`
// in zref_frame.hpp walks the ten ordered checks of capture_format.md 3.2 and
// is what zhao_stub_top, the capture tooling and the ABI corpus already agree
// with.
//
// So the fix is not to write a decoder. Writing one would create exactly the
// drift this project forbids: two implementations of one law, differing
// eventually, with tests pinning whichever was consulted last. The fix is to
// expose the existing law under the name the ledger requires, and to say
// plainly that it is the same function.
//
// The ledger's schema requires `^zref::`, hence the namespace. Everything below
// forwards; there is no decode logic in this file and there must never be.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "zref/zref_frame.hpp"

namespace zref {
namespace cmd {

/** The verdict, unchanged: `zhao::ZhaoValidateResult`.
 *
 *  `error`             the generated zhao_abi_error code
 *  `commands_consumed` records walked
 *  `bytes_consumed`    36 on a header-level abort (checks 1-3), else 40 + N
 */
using Result = ::zhao::ZhaoValidateResult;

/** The decoded header view, unchanged: `zhao::ZhaoFrameHeader`. */
using Header = ::zhao::ZhaoFrameHeader;

/**
 * CMD.DECODER's reference: validate a sealed packet against the fail-safe
 * order of `spec/capture_format.md` 3.2.
 *
 * RTL "matches the oracle" therefore means "matches the function the stub
 * shell, the capture tools and tests/abi/golden/ have always agreed with",
 * which is a far stronger statement than matching something written alongside
 * the RTL by the same hand on the same day.
 */
inline Result validate(const uint8_t* pkt, std::size_t len,
                       uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES) {
  return ::zhao::zhao_frame_validate(pkt, len, slot_bytes);
}

inline Result validate(const std::vector<uint8_t>& pkt,
                       uint32_t slot_bytes = zhao_abi::FRAME_SLOT_BYTES) {
  return ::zhao::zhao_frame_validate(pkt, slot_bytes);
}

/** Header fields of an ALREADY-VALIDATED packet. Calling this on a packet that
 *  failed validation is a caller error: the fields are not trustworthy, which
 *  is the entire reason validation runs first. */
inline Header parse_header(const uint8_t* pkt) { return ::zhao::zhao_frame_parse_header(pkt); }

}  // namespace cmd
}  // namespace zref
