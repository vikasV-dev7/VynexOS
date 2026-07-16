#pragma once
#include "vynexos/desktop/widget_toolkit.hpp"

namespace vynexos::desktop {

class BasicWidgetToolkit final : public IWidgetToolkit {
public:
    void draw_rect(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) override;
    void draw_text(WindowBuffer& buffer, int32_t x, int32_t y, std::string_view text, Color color) override;
    
    void draw_button(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) override;
    void draw_panel(WindowBuffer& buffer, int32_t x, int32_t y, uint32_t width, uint32_t height) override;
};

} // namespace vynexos::desktop
