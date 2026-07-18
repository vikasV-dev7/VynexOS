#pragma once
#include "vynexos/desktop/widget_toolkit.hpp"

namespace vynexos::desktop {

class BasicWidgetToolkit final : public IWidgetToolkit {
public:
    // New Native Surface Interface
    void draw_rect(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) override;
    void draw_text(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, std::string_view text, Color color) override;
    void draw_button(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, std::string_view label) override;
    void draw_panel(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height) override;

private:
    static const uint8_t s_font8x8[128][8];
    void draw_char(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, char c, Color color);
};

} // namespace vynexos::desktop
