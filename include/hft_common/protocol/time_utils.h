#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>

inline uint64_t local_timestamp_yyyymmddhhmmssmmm() {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t now_time = system_clock::to_time_t(now);

    std::tm local_tm{};
    localtime_r(&now_time, &local_tm);

    uint64_t ts = static_cast<uint64_t>(local_tm.tm_year + 1900);
    ts = ts * 100 + static_cast<uint64_t>(local_tm.tm_mon + 1);
    ts = ts * 100 + static_cast<uint64_t>(local_tm.tm_mday);
    ts = ts * 100 + static_cast<uint64_t>(local_tm.tm_hour);
    ts = ts * 100 + static_cast<uint64_t>(local_tm.tm_min);
    ts = ts * 100 + static_cast<uint64_t>(local_tm.tm_sec);
    ts = ts * 1000 + static_cast<uint64_t>(ms.count());
    return ts;
}
