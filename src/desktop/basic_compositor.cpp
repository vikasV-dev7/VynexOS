#include "basic_compositor.hpp"

namespace vynexos::desktop {

BasicCompositor::BasicCompositor(std::shared_ptr<IDisplayAbstraction> display, std::shared_ptr<core::ILogger> logger)
    : m_display(std::move(display)), m_logger(std::move(logger)) {}

void BasicCompositor::submit_buffer(const WindowBuffer& buffer) {
    std::lock_guard lock(m_mutex);
    m_window_buffers[buffer.id] = buffer; // Copy or update buffer
}

std::expected<void, DisplayError> BasicCompositor::render_frame() {
    std::lock_guard lock(m_mutex);
    
    // 1. Create a master framebuffer (e.g. 1920x1080)
    hal::FrameBuffer master_fb;
    master_fb.width = 1920;
    master_fb.height = 1080;
    master_fb.pixels.resize(master_fb.width * master_fb.height * 4, 0); // Clear to black
    
    // 2. In a real compositor, we sort by z_index and alpha-blend window_buffers into master_fb.
    m_logger->trace("BasicCompositor: Compositing frames...");
    
    // 3. Flush to display backend
    return m_display->flush_buffer(master_fb);
}

} // namespace vynexos::desktop
