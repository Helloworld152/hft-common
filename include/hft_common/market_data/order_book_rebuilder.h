#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include "hft_common/base/intrusive_pool.h"

struct kolo_order {
    char szCode[8] = {};
    int nActionDay = 0;
    int nTime = 0;
    int nOrder = 0;
    int nPrice = 0;
    int nVolume = 0;
    unsigned int nMainSeqNo = 0;
    unsigned int nSubSeqNo = 0;
    char chOrderSide = '\0';
    char chOrderKind = '\0';
};

struct kolo_trade {
    char szCode[8] = {};
    int nActionDay = 0;
    int nTime = 0;
    int nTrade = 0;
    int nPrice = 0;
    int nVolume = 0;
    int nAskOrder = 0;
    int nBidOrder = 0;
    unsigned int nMainSeqNo = 0;
    unsigned int nSubSeqNo = 0;
    char chTradeKind = '\0';
};

struct BookOrder {
    int order_id = 0;
    char side = '\0';
    int price = 0;
    int orig_qty = 0;
    int leaves_qty = 0;
    bool active = false;
};

struct PriceLevel {
    int price = 0;
    int qty = 0;
};

struct LevelSnapshot {
    int price = 0;
    int qty = 0;
};

struct DepthSnapshot {
    std::vector<LevelSnapshot> bids;
    std::vector<LevelSnapshot> asks;
};

struct RebuildStats {
    uint64_t duplicate_orders = 0;
    uint64_t missing_orders = 0;
    uint64_t missing_levels = 0;
    uint64_t overfills = 0;
    uint64_t overcancels = 0;
    uint64_t pool_exhausted = 0;
};

class OrderBookRebuilder {
public:
    using order_slot_t = IntrusivePool<BookOrder>::index_type;

    explicit OrderBookRebuilder(std::size_t max_orders)
        : order_pool_(max_orders) {
        order_index_.reserve(max_orders);
    }

    void reset(uint32_t trading_day) {
        for (const auto& entry : order_index_) {
            order_pool_.release(entry.second);
        }
        order_index_.clear();
        bids_.clear();
        asks_.clear();
        stats_ = RebuildStats{};
        trading_day_ = trading_day;
    }

    bool on_order(const kolo_order& order) {
        if (order.chOrderKind == 'A') return handle_add_order(order);
        if (order.chOrderKind == 'D') return handle_cancel_order(order);
        return false;
    }

    bool on_trade(const kolo_trade& trade) {
        if (trade.chTradeKind != 'T') return false;
        return handle_trade(trade);
    }

    DepthSnapshot snapshot(std::size_t depth) const {
        DepthSnapshot out;
        const std::size_t bid_count = std::min(depth, bids_.size());
        const std::size_t ask_count = std::min(depth, asks_.size());
        out.bids.reserve(bid_count);
        out.asks.reserve(ask_count);
        for (std::size_t i = 0; i < bid_count; ++i) {
            out.bids.push_back(LevelSnapshot{bids_[i].price, bids_[i].qty});
        }
        for (std::size_t i = 0; i < ask_count; ++i) {
            out.asks.push_back(LevelSnapshot{asks_[i].price, asks_[i].qty});
        }
        return out;
    }

    DepthSnapshot snapshot_full() const {
        return snapshot(std::numeric_limits<std::size_t>::max());
    }

    const RebuildStats& stats() const {
        return stats_;
    }

    std::size_t active_order_count() const {
        return order_index_.size();
    }

private:
    bool handle_add_order(const kolo_order& order) {
        if (order_index_.find(order.nOrder) != order_index_.end()) {
            ++stats_.duplicate_orders;
            return false;
        }

        const order_slot_t slot = order_pool_.allocate();
        if (slot == IntrusivePool<BookOrder>::npos) {
            ++stats_.pool_exhausted;
            return false;
        }

        BookOrder* book_order = order_pool_.get(slot);
        book_order->order_id = order.nOrder;
        book_order->side = order.chOrderSide;
        book_order->price = order.nPrice;
        book_order->orig_qty = order.nVolume;
        book_order->leaves_qty = order.nVolume;
        book_order->active = true;

        order_index_.emplace(order.nOrder, slot);
        add_level(book_order->side, book_order->price, book_order->orig_qty);
        return true;
    }

    bool handle_cancel_order(const kolo_order& order) {
        apply_cancel(order.nOrder, order.nVolume);
        return true;
    }

    bool handle_trade(const kolo_trade& trade) {
        apply_fill(trade.nBidOrder, trade.nVolume);
        apply_fill(trade.nAskOrder, trade.nVolume);
        return true;
    }

    BookOrder* find_order(int order_id, order_slot_t* slot = nullptr) {
        auto it = order_index_.find(order_id);
        if (it == order_index_.end()) return nullptr;
        if (slot != nullptr) *slot = it->second;
        return order_pool_.get(it->second);
    }

    void release_order(int order_id, order_slot_t slot) {
        order_pool_.release(slot);
        order_index_.erase(order_id);
    }

    void apply_fill(int order_id, int qty) {
        order_slot_t slot = IntrusivePool<BookOrder>::npos;
        BookOrder* order = find_order(order_id, &slot);
        if (order == nullptr) {
            ++stats_.missing_orders;
            return;
        }

        int fill_qty = qty;
        if (fill_qty > order->leaves_qty) {
            fill_qty = order->leaves_qty;
            ++stats_.overfills;
        }

        order->leaves_qty -= fill_qty;
        reduce_level(order->side, order->price, fill_qty);
        if (order->leaves_qty == 0) {
            order->active = false;
            release_order(order_id, slot);
        }
    }

    void apply_cancel(int order_id, int qty) {
        order_slot_t slot = IntrusivePool<BookOrder>::npos;
        BookOrder* order = find_order(order_id, &slot);
        if (order == nullptr) {
            ++stats_.missing_orders;
            return;
        }

        int cancel_qty = qty;
        if (cancel_qty > order->leaves_qty) {
            cancel_qty = order->leaves_qty;
            ++stats_.overcancels;
        }

        order->leaves_qty -= cancel_qty;
        reduce_level(order->side, order->price, cancel_qty);
        if (order->leaves_qty == 0) {
            order->active = false;
            release_order(order_id, slot);
        }
    }

    void add_level(char side, int price, int qty) {
        std::vector<PriceLevel>& levels = side == 'B' ? bids_ : asks_;
        for (auto& level : levels) {
            if (level.price == price) {
                level.qty += qty;
                return;
            }
        }

        PriceLevel level{price, qty};
        auto it = levels.begin();
        if (side == 'B') {
            while (it != levels.end() && it->price > price) ++it;
        } else {
            while (it != levels.end() && it->price < price) ++it;
        }
        levels.insert(it, level);
    }

    void reduce_level(char side, int price, int qty) {
        std::vector<PriceLevel>& levels = side == 'B' ? bids_ : asks_;
        for (auto it = levels.begin(); it != levels.end(); ++it) {
            if (it->price != price) continue;
            it->qty -= qty;
            if (it->qty <= 0) {
                levels.erase(it);
            }
            return;
        }
        ++stats_.missing_levels;
    }

    uint32_t trading_day_ = 0;
    IntrusivePool<BookOrder> order_pool_;
    std::unordered_map<int, order_slot_t> order_index_;
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
    RebuildStats stats_;
};
