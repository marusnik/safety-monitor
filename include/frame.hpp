#pragma once
#include <chrono>
#include <cstdint>
#include <opencv2/core.hpp>

struct Frame {
    cv::Mat                               image;
    std::chrono::steady_clock::time_point captured_at;
    std::uint64_t                         sequence = 0;     // 0, 1, 2, ... lets you detect gaps (dropped frames)
};