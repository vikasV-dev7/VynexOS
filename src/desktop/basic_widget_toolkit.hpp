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

    // Anti-aliased shapes and depth (see Painter). draw_rect above now shares the same blending and clipping.
    void draw_rounded_rect(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii, Color color) override;
    void draw_rounded_border(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii, float thickness,
                             Color color) override;
    void draw_gradient_rect(std::shared_ptr<ISurface> surface, const Rect& rect, const CornerRadii& radii,
                            const LinearGradient& gradient) override;
    void draw_shadow(std::shared_ptr<ISurface> surface, const Rect& box, const CornerRadii& radii, const ShadowSpec& shadow) override;

private:
    // Helper to draw a single character (using font8x8)
    void draw_char(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, char c, Color color);
};

} // namespace vynexos::desktop
