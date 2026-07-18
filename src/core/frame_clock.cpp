#include "vynexos/core/frame_clock.hpp"
#include <thread>

namespace vynexos::core {

FrameClock::FrameClock(uint32_t target_fps)
    : m_target_fps(target_fps > 0 ? target_fps : 60)
{
    m_target_frame_time = std::chrono::microseconds(1000000 / m_target_fps);
}

void FrameClock::begin_frame() {
    m_frame_start = std::chrono::steady_clock::now();
}

void FrameClock::end_frame_and_wait() {
    auto frame_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - m_frame_start);

    if (elapsed < m_target_frame_time) {
        std::this_thread::sleep_for(m_target_frame_time - elapsed);
    }
}

} // namespace vynexos::core
