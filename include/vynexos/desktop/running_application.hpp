#pragma once
#include "vynexos/apps/application.hpp"
#include "vynexos/desktop/surface.hpp"
#include <memory>

namespace vynexos::desktop {

struct RunningApplication {
    apps::IApplication* app{nullptr};
    uint32_t window_id{0};
    std::shared_ptr<ISurface> surface;
    bool visible{false};
};

} // namespace vynexos::desktop
