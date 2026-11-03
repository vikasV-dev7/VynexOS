#include "demo_app.hpp"

namespace vynexos::apps {

DemoApp::DemoApp(std::shared_ptr<desktop::IWidgetToolkit> toolkit) : m_toolkit(std::move(toolkit)) {}

void DemoApp::render(desktop::ISurface& target_surface) {
    std::shared_ptr<desktop::ISurface> surface(&target_surface, [](desktop::ISurface*) {}); // aliased: the toolkit takes a shared_ptr
    m_toolkit->draw_panel(surface, 0, 0, target_surface.width(), target_surface.height());
    m_toolkit->draw_button(surface, 20, 20, 120, 40, "Click Me");
}

} // namespace vynexos::apps
