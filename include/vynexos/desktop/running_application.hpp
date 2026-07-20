#pragma once
#include "vynexos/apps/application.hpp"
#include "vynexos/desktop/desktop_types.hpp"
#include <memory>

namespace vynexos::desktop {

struct RunningApplication {
    apps::IApplication* app{nullptr};
    uint32_t window_id{0};
    std::shared_ptr<ISurface> surface;
    bool visible{true};
    bool focused{false};
};

} // namespace vynexos::desktop
