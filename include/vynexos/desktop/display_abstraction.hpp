#pragma once
#include <cstdint>
#include <expected>
#include <span>
#include <memory>
#include "vynexos/hal/display.hpp" // For FrameBuffer

namespace vynexos::desktop {

enum class DisplayError {
    BackendNotAvailable,
    RenderingFailed,
    InvalidBuffer
};

// Abstract interface for the display backend (e.g., mock, VM display, Wayland, DRM/KMS)
class IDisplayAbstraction {
public:
    virtual ~IDisplayAbstraction() = default;

    virtual std::expected<void, DisplayError> initialize() = 0;
    virtual std::expected<void, DisplayError> flush_buffer(const hal::FrameBuffer& buffer) = 0;
    virtual void shutdown() = 0;
};

} // namespace vynexos::desktop
