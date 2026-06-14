#pragma once

#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace hft_common::ipc {

constexpr uint64_t DAT_MAGIC = 0x4846544441543031ULL;
constexpr uint64_t META_MAGIC = 0x4846544D45544131ULL;
constexpr uint32_t VERSION = 1;
constexpr uint32_t HEADER_SIZE = 4096;
constexpr uint32_t STATE_OPEN = 1;
constexpr uint32_t STATE_CLOSED = 2;

constexpr uint32_t RECORD_KIND_TICK = 1;
constexpr uint32_t RECORD_KIND_KLINE = 2;

template <typename T>
struct MmapRecordTraits {
    static_assert(sizeof(T) == 0, "MmapRecordTraits must be specialized for this record type");
};

struct MmapDatHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t record_kind;
    uint32_t record_size;
    uint32_t record_align;
    uint32_t reserved0;
    uint64_t capacity;
    char reserved[HEADER_SIZE - 40];
};

struct MetaHeader {
    uint64_t magic;
    uint32_t version;
    uint32_t header_size;
    uint32_t record_kind;
    uint32_t record_size;
    uint32_t state;
    uint32_t reserved0;
    uint64_t capacity;
    std::atomic<uint64_t> write_cursor;
    char reserved[HEADER_SIZE - 48];
};

static_assert(sizeof(MmapDatHeader) == HEADER_SIZE, "MmapDatHeader must be 4KB");
static_assert(sizeof(MetaHeader) == HEADER_SIZE, "MetaHeader must be 4KB");

inline uint64_t file_size_or_throw(int fd, const std::string& path) {
    struct stat st {};
    if (fstat(fd, &st) != 0) {
        throw std::runtime_error("无法读取文件大小: " + path);
    }
    if (st.st_size < 0) {
        throw std::runtime_error("文件大小非法: " + path);
    }
    return static_cast<uint64_t>(st.st_size);
}

}  // namespace hft_common::ipc
