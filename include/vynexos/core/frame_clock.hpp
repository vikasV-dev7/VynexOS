#pragma once
#include <chrono>
#include <cstdint>

namespace vynexos::core {

class FrameClock {
public:
    explicit FrameClock(uint32_t target_fps = 60);

    // Call at the start of a frame.
    void begin_frame();

    // Call at the end of a frame. Calculates time elapsed and sleeps to maintain target FPS.
    void end_frame_and_wait();

private:
    uint32_t m_target_fps;
    std::chrono::microseconds m_target_frame_time;
    std::chrono::steady_clock::time_point m_frame_start;
};

} // namespace vynexos::core
