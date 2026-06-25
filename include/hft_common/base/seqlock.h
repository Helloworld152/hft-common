#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

template <typename T>
class SeqLock {
public:
    constexpr SeqLock() noexcept = default;
    constexpr explicit SeqLock(const T& initial) noexcept : value_(initial), sequence_(2) {}

    SeqLock(const SeqLock&) = delete;
    SeqLock& operator=(const SeqLock&) = delete;

    void write(const T& value) noexcept {
        const uint64_t seq = sequence_.load(std::memory_order_relaxed);
        sequence_.store(seq + 1, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_release);
        value_ = value;
        sequence_.store(seq + 2, std::memory_order_release);
    }

    bool read(T& out) const noexcept {
        const uint64_t begin = sequence_.load(std::memory_order_acquire);
        if (begin & 1U) {
            return false;
        }

        out = value_;
        std::atomic_thread_fence(std::memory_order_acquire);
        const uint64_t end = sequence_.load(std::memory_order_acquire);
        return begin == end;
    }

    uint64_t version() const noexcept {
        return sequence_.load(std::memory_order_acquire);
    }

    alignas(CACHE_LINE_SIZE) mutable std::atomic<uint64_t> sequence_{0};
    alignas(CACHE_LINE_SIZE) T value_{};
};
