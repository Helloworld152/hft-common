#pragma once

#include <cstddef>
#include <cstdint>

namespace hft_common::market_data {

constexpr std::size_t kBookDepth = 10;
constexpr const char* kDefaultBookRingName = "/hft_order_book_demo";
constexpr const char* kDefaultTapeRingName = "/hft_order_tape_demo";

enum class TapeEventType : uint32_t {
    Add = 1,
    Cancel = 2,
    Trade = 3,
};

struct BookLevelIpc {
    int32_t price = 0;
    int32_t qty = 0;
};

struct BookSnapshotIpc {
    uint64_t step = 0;
    uint64_t producer_ts_ns = 0;
    uint32_t active_orders = 0;
    uint32_t bid_count = 0;
    uint32_t ask_count = 0;
    uint32_t trading_day = 0;
    int32_t last_price = 0;
    int32_t open_price = 0;
    int32_t high_price = 0;
    int32_t low_price = 0;
    int32_t prev_close_price = 0;
    int32_t total_bid_qty = 0;
    int32_t total_ask_qty = 0;
    BookLevelIpc bids[kBookDepth] = {};
    BookLevelIpc asks[kBookDepth] = {};
};

struct TapeEventIpc {
    uint64_t seq = 0;
    uint64_t producer_ts_ns = 0;
    uint32_t trading_day = 0;
    uint32_t hhmmssmmm = 0;
    TapeEventType event_type = TapeEventType::Add;
    char side = '\0';
    char reserved0 = '\0';
    uint16_t reserved1 = 0;
    int32_t price = 0;
    int32_t qty = 0;
    int32_t order_id = 0;
    int32_t bid_order_id = 0;
    int32_t ask_order_id = 0;
};

static_assert(sizeof(BookLevelIpc) == 8, "BookLevelIpc layout mismatch");
static_assert(sizeof(TapeEventType) == 4, "TapeEventType layout mismatch");

}  // namespace hft_common::market_data
