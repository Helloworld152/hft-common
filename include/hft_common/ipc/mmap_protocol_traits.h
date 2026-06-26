#pragma once

#include "hft_common/ipc/mmap_types.h"
#include "hft_common/protocol/protocol.h"

namespace hft_common::ipc {

template <>
struct MmapRecordTraits<TickRecord> {
    static constexpr uint32_t kind = RECORD_KIND_TICK;
};

template <>
struct MmapRecordTraits<KlineRecord> {
    static constexpr uint32_t kind = RECORD_KIND_KLINE;
};

template <>
struct MmapRecordTraits<CryptoTickRecord> {
    static constexpr uint32_t kind = 3;
};

}  // namespace hft_common::ipc
