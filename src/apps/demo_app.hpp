#pragma once
#include "vynexos/apps/application.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>

namespace vynexos::apps {

/// A static sample application: a panel with one button, painted once (it never reports changes).
class DemoApp final : public IApplication {
public:
    explicit DemoApp(std::shared_ptr<desktop::IWidgetToolkit> toolkit);

    bool initialize() override { return m_toolkit != nullptr; }
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event&) override { return false; }
    void render(desktop::ISurface& target_surface) override;
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "Demo App"; }

private:
    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
};

} // namespace vynexos::apps
