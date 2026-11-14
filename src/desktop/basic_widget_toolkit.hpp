#pragma once
#include "vynexos/desktop/widget_toolkit.hpp"

namespace vynexos::desktop {

class BasicWidgetToolkit final : public IWidgetToolkit {
public:
    /// Uses the shared anti-aliased text renderer (built once, at first use - so construct the toolkit during start-up).
    BasicWidgetToolkit();
    /// Uses `text_renderer`, which must outlive the toolkit. Passing nullptr selects the original 8x8 bitmap font
    /// for everything - this is also what happens automatically if the embedded fonts cannot be initialised.
    explicit BasicWidgetToolkit(const TextRenderer* text_renderer);
    /// As above, drawing with `theme` (which must outlive the toolkit) instead of the default dark theme. The theme is
    /// fixed for the toolkit's lifetime.
    BasicWidgetToolkit(const TextRenderer* text_renderer, const Theme& theme);

    [[nodiscard]] const Theme& theme() const override { return *m_theme; }
    [[nodiscard]] const TextRenderer* text_renderer() const override { return m_text; }

    // New Native Surface Interface
    void draw_rect(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, uint32_t width, uint32_t height, Color color) override;
    /// Legacy entry point, kept source- and layout-compatible: `y` is the TOP of an 8 px glyph cell. The text is now
    /// anti-aliased UTF-8 in the body style, with its capitals centred in that cell. Use draw_text_styled() for new code.
    void draw_text(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, std::string_view text, Color color) override;
    /// The original fixed-width 8x8 bitmap renderer (ASCII only, 1 bit, no blending). Kept as the fallback and for the
    /// legacy glyph tests; not used by draw_text() while a text renderer is available.
    void draw_text_bitmap(std::shared_ptr<ISurface> surface, int32_t x, int32_t y, std::string_view text, Color color);

    void draw_text_styled(std::shared_ptr<ISurface> surface, int32_t x, int32_t baseline_y, std::string_view utf8, const TextStyle& style,
                          Color color) override;
    [[nodiscard]] TextMetrics measure_text(std::string_view utf8, const TextStyle& style) const override;
    [[nodiscard]] FontMetrics font_metrics(const TextStyle& style) const override;
    [[nodiscard]] size_t fit_text_prefix(std::string_view utf8, const TextStyle& style, float max_width) const override;
    float draw_text_ellipsized(std::shared_ptr<ISurface> surface, int32_t x, int32_t baseline_y, std::string_view utf8, const TextStyle& style,
                               Color color, float max_width) override;
    [[nodiscard]] bool has_vector_text() const noexcept { return m_text != nullptr; }
    /// Legacy entry points, kept source-compatible and drawn by the shared components (components.hpp): a default
    /// button in its resting state, and a flat surface panel. Code that needs states or variants paints the components
    /// directly.
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

    const TextRenderer* m_text{nullptr};
    const Theme* m_theme{&default_theme()};
    int32_t m_legacy_baseline_offset{9};
};

} // namespace vynexos::desktop
