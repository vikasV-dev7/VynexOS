#include "vynexos/desktop/painter.hpp"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <type_traits>

namespace vynexos::desktop {

namespace {

// Coordinates are clamped to this range before any arithmetic so that Rect math can never overflow.
constexpr int64_t kCoordLimit = 1 << 28;

// Half-open pixel box [x0,x1) x [y0,y1).
struct Box {
    int32_t x0{0}, y0{0}, x1{0}, y1{0};
    [[nodiscard]] bool empty() const noexcept { return x0 >= x1 || y0 >= y1; }
};

[[nodiscard]] Box to_box(const Rect& r) noexcept {
    const int64_t x0 = std::clamp<int64_t>(r.x, -kCoordLimit, kCoordLimit);
    const int64_t y0 = std::clamp<int64_t>(r.y, -kCoordLimit, kCoordLimit);
    const int64_t x1 = std::clamp<int64_t>(static_cast<int64_t>(r.x) + static_cast<int64_t>(r.width), -kCoordLimit, kCoordLimit);
    const int64_t y1 = std::clamp<int64_t>(static_cast<int64_t>(r.y) + static_cast<int64_t>(r.height), -kCoordLimit, kCoordLimit);
    return Box{static_cast<int32_t>(x0), static_cast<int32_t>(y0), static_cast<int32_t>(x1), static_cast<int32_t>(y1)};
}

[[nodiscard]] Box intersect(const Box& a, const Box& b) noexcept {
    Box r{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
    if (r.empty()) return Box{};
    return r;
}

[[nodiscard]] Rect to_rect(const Box& b) noexcept {
    if (b.empty()) return Rect{0, 0, 0, 0};
    return Rect{b.x0, b.y0, static_cast<uint32_t>(b.x1 - b.x0), static_cast<uint32_t>(b.y1 - b.y0)};
}

// Everything a drawing routine needs: the pixel buffer and the effective clip. Plain data, no allocation.
struct Target {
    uint8_t* px;
    int32_t w;
    int32_t h;
    Box clip;

    [[nodiscard]] uint8_t* at(int32_t x, int32_t y) const noexcept {
        return px + (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
    }
};

// ------------------------------------------------------------------------------------------------
// Blending
// ------------------------------------------------------------------------------------------------

// Exact rounded v/255 for v in [0, 65025].
[[nodiscard]] inline uint32_t div255(uint32_t v) noexcept {
    v += 128;
    return (v + (v >> 8)) >> 8;
}

// Straight-alpha source-over of colour `c` scaled by coverage `cov` (0..255) onto one RGBA pixel.
inline void blend_px(uint8_t* d, Color c, uint32_t cov) noexcept {
    const uint32_t sa = cov >= 255 ? c.a : div255(static_cast<uint32_t>(c.a) * cov);
    if (sa == 0) return;
    if (sa == 255) {
        d[0] = c.r;
        d[1] = c.g;
        d[2] = c.b;
        d[3] = 255;
        return;
    }
    const uint32_t da = d[3];
    if (da == 0) {
        // Onto transparent: keep the colour as-is and carry the alpha. (Premultiplying here would darken edges.)
        d[0] = c.r;
        d[1] = c.g;
        d[2] = c.b;
        d[3] = static_cast<uint8_t>(sa);
        return;
    }
    if (da == 255) {
        const uint32_t ia = 255 - sa;
        d[0] = static_cast<uint8_t>(div255(c.r * sa + d[0] * ia));
        d[1] = static_cast<uint8_t>(div255(c.g * sa + d[1] * ia));
        d[2] = static_cast<uint8_t>(div255(c.b * sa + d[2] * ia));
        return;
    }
    // General case: translucent source over translucent destination.
    const uint32_t out_a = std::min<uint32_t>(255, sa + div255(da * (255 - sa)));
    const uint32_t ws = sa * 255;
    const uint32_t wd = da * (255 - sa);
    const uint32_t total = ws + wd;
    d[0] = static_cast<uint8_t>((c.r * ws + d[0] * wd + total / 2) / total);
    d[1] = static_cast<uint8_t>((c.g * ws + d[1] * wd + total / 2) / total);
    d[2] = static_cast<uint8_t>((c.b * ws + d[2] * wd + total / 2) / total);
    d[3] = static_cast<uint8_t>(out_a);
}

// Fills `count` pixels of one row. Opaque colours use a doubling memcpy; translucent ones blend per pixel.
inline void fill_span(uint8_t* row, int32_t count, Color c) noexcept {
    if (count <= 0) return;
    if (c.a == 255) {
        row[0] = c.r;
        row[1] = c.g;
        row[2] = c.b;
        row[3] = 255;
        size_t filled = 1;
        const size_t total = static_cast<size_t>(count);
        while (filled < total) {
            const size_t n = std::min(filled, total - filled);
            std::memcpy(row + filled * 4, row, n * 4);
            filled += n;
        }
        return;
    }
    if (c.a == 0) return;
    for (int32_t i = 0; i < count; ++i) blend_px(row + static_cast<size_t>(i) * 4, c, 255);
}

[[nodiscard]] inline uint32_t coverage_from_sd(float sd) noexcept {
    const float c = 0.5f - sd; // 1-pixel linear anti-aliasing band centred on the edge
    if (c <= 0.0f) return 0;
    if (c >= 1.0f) return 255;
    return static_cast<uint32_t>(c * 255.0f + 0.5f);
}

[[nodiscard]] inline float coverage_f(float sd) noexcept { return std::clamp(0.5f - sd, 0.0f, 1.0f); }

[[nodiscard]] inline bool finite(float v) noexcept { return std::isfinite(v); }

// ------------------------------------------------------------------------------------------------
// Signed distance to a rounded rectangle (negative inside), per-corner radii. Standard formulation:
// the corner radius is chosen by the pixel's quadrant.
// ------------------------------------------------------------------------------------------------

struct RR {
    float cx, cy, hx, hy;
    float tl, tr, br, bl;

    [[nodiscard]] float max_radius() const noexcept { return std::max(std::max(tl, tr), std::max(br, bl)); }

    [[nodiscard]] float sd(float px, float py) const noexcept {
        const float dx = px - cx;
        const float dy = py - cy;
        const float rad = dx < 0.0f ? (dy < 0.0f ? tl : bl) : (dy < 0.0f ? tr : br);
        const float qx = std::fabs(dx) - (hx - rad);
        const float qy = std::fabs(dy) - (hy - rad);
        const float ox = std::max(qx, 0.0f);
        const float oy = std::max(qy, 0.0f);
        const float outside = (ox > 0.0f && oy > 0.0f) ? std::sqrt(ox * ox + oy * oy) : std::max(ox, oy);
        const float inside = std::min(std::max(qx, qy), 0.0f);
        return outside + inside - rad;
    }
};

[[nodiscard]] RR make_rr(float x0, float y0, float x1, float y1, const CornerRadii& r) noexcept {
    const float hx = (x1 - x0) * 0.5f;
    const float hy = (y1 - y0) * 0.5f;
    const float limit = std::max(0.0f, std::min(hx, hy));
    auto clampr = [limit](float v) { return std::clamp(finite(v) ? v : 0.0f, 0.0f, limit); };
    return RR{x0 + hx, y0 + hy, hx, hy, clampr(r.top_left), clampr(r.top_right), clampr(r.bottom_right), clampr(r.bottom_left)};
}

// ------------------------------------------------------------------------------------------------
// Colour sources: a constant colour or a (dithered) linear gradient.
// ------------------------------------------------------------------------------------------------

// 4x4 Bayer matrix mapped to [-0.5, 0.5).
[[nodiscard]] inline float bayer(int32_t x, int32_t y) noexcept {
    static constexpr float m[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};
    return (m[y & 3][x & 3] + 0.5f) / 16.0f - 0.5f;
}

[[nodiscard]] inline uint8_t quantize(float v, float dither) noexcept {
    const float q = std::floor(v + 0.5f + dither);
    return static_cast<uint8_t>(std::clamp(q, 0.0f, 255.0f));
}

// Interpolates in premultiplied space so fading to a transparent colour never produces a grey fringe.
[[nodiscard]] inline Color lerp_color(Color a, Color b, float t, float dither) noexcept {
    const float aa = static_cast<float>(a.a) / 255.0f;
    const float ba = static_cast<float>(b.a) / 255.0f;
    const float alpha = aa + (ba - aa) * t;
    if (alpha <= 1e-6f) {
        return Color{t < 0.5f ? a.r : b.r, t < 0.5f ? a.g : b.g, t < 0.5f ? a.b : b.b, 0};
    }
    auto ch = [&](uint8_t x, uint8_t y) {
        const float pa = static_cast<float>(x) * aa;
        const float pb = static_cast<float>(y) * ba;
        return quantize((pa + (pb - pa) * t) / alpha, dither);
    };
    return Color{ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), quantize(alpha * 255.0f, dither)};
}

struct SolidFn {
    static constexpr bool kSolid = true;
    Color c;
    [[nodiscard]] Color operator()(int32_t, int32_t) const noexcept { return c; }
};

struct LinearFn {
    static constexpr bool kSolid = false;
    Color a;
    Color b;
    bool vertical;
    bool dither;
    float origin; // rect start along the gradient axis
    float inv_len;

    // A vertical gradient is constant along a row, and the 4x4 dither pattern repeats every 4 pixels, so one row needs
    // only four distinct colours (indexed by x & 3). They are computed once per row; per pixel it is a table lookup.
    mutable int32_t cached_row{INT32_MIN};
    mutable Color row_colors[4]{};

    [[nodiscard]] Color operator()(int32_t x, int32_t y) const noexcept {
        if (!vertical) {
            const float pos = (static_cast<float>(x) + 0.5f - origin) * inv_len;
            return lerp_color(a, b, std::clamp(pos, 0.0f, 1.0f), dither ? bayer(x, y) : 0.0f);
        }
        if (y != cached_row) {
            cached_row = y;
            const float t = std::clamp((static_cast<float>(y) + 0.5f - origin) * inv_len, 0.0f, 1.0f);
            for (int32_t phase = 0; phase < 4; ++phase) {
                row_colors[phase] = lerp_color(a, b, t, dither ? bayer(phase, y) : 0.0f);
            }
        }
        return row_colors[x & 3];
    }
};

[[nodiscard]] LinearFn make_linear(const LinearGradient& g, const Box& shape) noexcept {
    const float origin = static_cast<float>(g.vertical ? shape.y0 : shape.x0);
    const float len = static_cast<float>(g.vertical ? shape.y1 - shape.y0 : shape.x1 - shape.x0);
    return LinearFn{g.start, g.end, g.vertical, g.dither, origin, len > 0.0f ? 1.0f / len : 0.0f};
}

// ------------------------------------------------------------------------------------------------
// Rounded-rectangle fill. Rows that cannot touch a corner are solid spans; only the corner zones
// evaluate the distance field.
// ------------------------------------------------------------------------------------------------

template <class Fn>
void fill_rr(const Target& t, const Box& shape, const CornerRadii& radii, Fn&& fn) noexcept {
    using FnT = std::remove_cvref_t<Fn>;
    if (shape.empty()) return;
    const Box vis = intersect(shape, t.clip);
    if (vis.empty()) return;

    const RR rr = make_rr(static_cast<float>(shape.x0), static_cast<float>(shape.y0), static_cast<float>(shape.x1),
                          static_cast<float>(shape.y1), radii);
    const int32_t rmax = static_cast<int32_t>(std::ceil(rr.max_radius()));

    int32_t mid_x0 = shape.x0 + rmax;
    int32_t mid_x1 = shape.x1 - rmax;
    if (mid_x0 > mid_x1) mid_x0 = mid_x1 = shape.x0 + (shape.x1 - shape.x0) / 2;
    int32_t mid_y0 = shape.y0 + rmax;
    int32_t mid_y1 = shape.y1 - rmax;
    if (mid_y0 > mid_y1) mid_y0 = mid_y1 = shape.y0 + (shape.y1 - shape.y0) / 2;

    auto span = [&](int32_t y, int32_t xa, int32_t xb) {
        if (xa >= xb) return;
        if constexpr (FnT::kSolid) {
            fill_span(t.at(xa, y), xb - xa, fn(xa, y));
        } else {
            uint8_t* d = t.at(xa, y);
            for (int32_t x = xa; x < xb; ++x, d += 4) blend_px(d, fn(x, y), 255);
        }
    };
    auto zone = [&](int32_t y, int32_t xa, int32_t xb) {
        if (xa >= xb) return;
        uint8_t* d = t.at(xa, y);
        const float py = static_cast<float>(y) + 0.5f;
        for (int32_t x = xa; x < xb; ++x, d += 4) {
            const uint32_t cov = coverage_from_sd(rr.sd(static_cast<float>(x) + 0.5f, py));
            if (cov != 0) blend_px(d, fn(x, y), cov);
        }
    };

    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        if (y >= mid_y0 && y < mid_y1) {
            span(y, vis.x0, vis.x1);
            continue;
        }
        zone(y, vis.x0, std::min(vis.x1, mid_x0));
        span(y, std::max(vis.x0, mid_x0), std::min(vis.x1, mid_x1));
        zone(y, std::max(vis.x0, mid_x1), vis.x1);
    }
}

} // namespace

// ----------------------------------------------------------------------------------------------------
// Painter
// ----------------------------------------------------------------------------------------------------

Painter::Painter(ISurface& surface) : m_surface(surface) {
    m_pixels = m_surface.map_pixels();
    const uint32_t w = m_surface.width();
    const uint32_t h = m_surface.height();
    if (w > 0 && h > 0 && w <= (1u << 15) && h <= (1u << 15) && m_pixels.size() >= static_cast<size_t>(w) * h * 4) {
        m_width = w;
        m_height = h;
    }
    m_clips[0] = Rect{0, 0, m_width, m_height};
}

Painter::~Painter() {
    m_surface.unmap_pixels();
}

bool Painter::push_clip(const Rect& rect) noexcept {
    if (m_clip_depth + 1 >= kMaxClipDepth) return false;
    const Box merged = intersect(to_box(m_clips[m_clip_depth]), to_box(rect));
    m_clips[++m_clip_depth] = to_rect(merged);
    return true;
}

void Painter::pop_clip() noexcept {
    if (m_clip_depth > 0) --m_clip_depth;
}

Rect Painter::clip_bounds() const noexcept {
    return m_clips[m_clip_depth];
}

namespace {
[[nodiscard]] Target make_target(uint8_t* px, uint32_t w, uint32_t h, const Rect& clip) noexcept {
    return Target{px, static_cast<int32_t>(w), static_cast<int32_t>(h), intersect(to_box(clip), Box{0, 0, static_cast<int32_t>(w), static_cast<int32_t>(h)})};
}
} // namespace

void Painter::fill_rect(const Rect& rect, Color color) noexcept {
    if (m_width == 0 || color.a == 0) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const Box vis = intersect(to_box(rect), t.clip);
    for (int32_t y = vis.y0; y < vis.y1; ++y) fill_span(t.at(vis.x0, y), vis.x1 - vis.x0, color);
}

void Painter::fill_rounded_rect(const Rect& rect, const CornerRadii& radii, Color color) noexcept {
    if (m_width == 0 || color.a == 0) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    fill_rr(t, to_box(rect), radii, SolidFn{color});
}

void Painter::fill_rounded_rect(const Rect& rect, const CornerRadii& radii, const LinearGradient& gradient) noexcept {
    if (m_width == 0) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const Box shape = to_box(rect);
    fill_rr(t, shape, radii, make_linear(gradient, shape)); // temporary: its row cache lives for this call only
}

void Painter::fill_gradient(const Rect& rect, const LinearGradient& gradient) noexcept {
    fill_rounded_rect(rect, CornerRadii{}, gradient);
}

void Painter::stroke_rounded_rect(const Rect& rect, const CornerRadii& radii, float thickness, Color color) noexcept {
    if (m_width == 0 || color.a == 0 || !finite(thickness) || thickness <= 0.0f) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const Box shape = to_box(rect);
    if (shape.empty()) return;
    const Box vis = intersect(shape, t.clip);
    if (vis.empty()) return;

    const float x0 = static_cast<float>(shape.x0);
    const float y0 = static_cast<float>(shape.y0);
    const float x1 = static_cast<float>(shape.x1);
    const float y1 = static_cast<float>(shape.y1);
    const RR outer = make_rr(x0, y0, x1, y1, radii);
    if (thickness >= std::min(outer.hx, outer.hy)) {
        // The border consumes the whole shape: there is no interior left to cut out.
        fill_rr(t, shape, radii, SolidFn{color});
        return;
    }
    const float th = thickness;
    CornerRadii inner_r{std::max(0.0f, outer.tl - th), std::max(0.0f, outer.tr - th), std::max(0.0f, outer.br - th),
                        std::max(0.0f, outer.bl - th)};
    const RR inner = make_rr(x0 + th, y0 + th, x1 - th, y1 - th, inner_r);

    // Only the band near the perimeter can be touched; the interior is skipped entirely.
    const int32_t band = std::max(static_cast<int32_t>(std::ceil(outer.max_radius())), static_cast<int32_t>(std::ceil(th)) + 1);

    auto run = [&](int32_t y, int32_t xa, int32_t xb) {
        if (xa >= xb) return;
        uint8_t* d = t.at(xa, y);
        const float py = static_cast<float>(y) + 0.5f;
        for (int32_t x = xa; x < xb; ++x, d += 4) {
            const float px = static_cast<float>(x) + 0.5f;
            const float o = coverage_f(outer.sd(px, py));
            if (o <= 0.0f) continue;
            const float in = coverage_f(inner.sd(px, py));
            const float ring = o - in;
            if (ring <= 0.0f) continue;
            blend_px(d, color, static_cast<uint32_t>(ring * 255.0f + 0.5f));
        }
    };

    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        if (y < shape.y0 + band || y >= shape.y1 - band) {
            run(y, vis.x0, vis.x1);
        } else {
            const int32_t left_end = std::min(vis.x1, shape.x0 + band);
            run(y, vis.x0, left_end);
            run(y, std::max(left_end, std::max(vis.x0, shape.x1 - band)), vis.x1);
        }
    }
}

void Painter::draw_box(const Rect& rect, const CornerRadii& radii, Color fill, float border_thickness, Color border) noexcept {
    if (m_width == 0) return;
    const bool has_border = border.a != 0 && finite(border_thickness) && border_thickness > 0.0f;
    if (!has_border) {
        fill_rounded_rect(rect, radii, fill);
        return;
    }
    if (fill.a == 0) {
        stroke_rounded_rect(rect, radii, border_thickness, border);
        return;
    }
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const Box shape = to_box(rect);
    if (shape.empty()) return;
    const Box vis = intersect(shape, t.clip);
    if (vis.empty()) return;

    const float x0 = static_cast<float>(shape.x0);
    const float y0 = static_cast<float>(shape.y0);
    const float x1 = static_cast<float>(shape.x1);
    const float y1 = static_cast<float>(shape.y1);
    const RR outer = make_rr(x0, y0, x1, y1, radii);

    // The border composited over the fill: the colour of the ring where the border fully covers the shape.
    const auto border_over_fill = [&](float ring) noexcept {
        const float ba = static_cast<float>(border.a) / 255.0f * ring;
        const float fa = static_cast<float>(fill.a) / 255.0f;
        const float out_a = ba + fa * (1.0f - ba);
        if (out_a <= 0.0f) return Color{0, 0, 0, 0};
        auto ch = [&](uint8_t b, uint8_t f) {
            const float v = (static_cast<float>(b) * ba + static_cast<float>(f) * fa * (1.0f - ba)) / out_a;
            return static_cast<uint8_t>(std::clamp(v + 0.5f, 0.0f, 255.0f));
        };
        return Color{ch(border.r, fill.r), ch(border.g, fill.g), ch(border.b, fill.b),
                     static_cast<uint8_t>(std::clamp(out_a * 255.0f + 0.5f, 0.0f, 255.0f))};
    };

    const float th = border_thickness;
    if (th >= std::min(outer.hx, outer.hy)) {
        // The border consumes the whole shape: one fill in the composited colour.
        fill_rr(t, shape, radii, SolidFn{border_over_fill(1.0f)});
        return;
    }
    const CornerRadii inner_r{std::max(0.0f, outer.tl - th), std::max(0.0f, outer.tr - th), std::max(0.0f, outer.br - th),
                              std::max(0.0f, outer.bl - th)};
    const RR inner = make_rr(x0 + th, y0 + th, x1 - th, y1 - th, inner_r);

    // Pixels within `band` of the perimeter may be border, anti-aliased edge or corner; everything further in is plain
    // fill. The band is wide enough for the largest corner and for the border plus its inner anti-aliased edge.
    const int32_t band = std::max(static_cast<int32_t>(std::ceil(outer.max_radius())), static_cast<int32_t>(std::ceil(th)) + 1);

    auto edge = [&](int32_t y, int32_t xa, int32_t xb) {
        if (xa >= xb) return;
        uint8_t* d = t.at(xa, y);
        const float py = static_cast<float>(y) + 0.5f;
        for (int32_t x = xa; x < xb; ++x, d += 4) {
            const float px = static_cast<float>(x) + 0.5f;
            const float o = coverage_f(outer.sd(px, py)); // how much of the pixel the shape covers
            if (o <= 0.0f) continue;
            const float in = std::min(coverage_f(inner.sd(px, py)), o); // ... and how much of it is inside the border
            // Of the covered part, the fraction (o - in) / o is border-over-fill and the rest is fill: one source
            // colour for the pixel, blended once with the shape's coverage.
            blend_px(d, border_over_fill((o - in) / o), static_cast<uint32_t>(o * 255.0f + 0.5f));
        }
    };
    auto interior = [&](int32_t y, int32_t xa, int32_t xb) {
        if (xa < xb) fill_span(t.at(xa, y), xb - xa, fill);
    };

    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        if (y < shape.y0 + band || y >= shape.y1 - band) {
            edge(y, vis.x0, vis.x1);
            continue;
        }
        const int32_t left_end = std::min(vis.x1, shape.x0 + band);
        const int32_t right_begin = std::max(left_end, std::max(vis.x0, shape.x1 - band));
        edge(y, vis.x0, left_end);
        interior(y, std::max(vis.x0, left_end), std::min(vis.x1, right_begin));
        edge(y, right_begin, vis.x1);
    }
}

void Painter::fill_circle(float cx, float cy, float radius, Color color) noexcept {
    if (m_width == 0 || color.a == 0 || !finite(cx) || !finite(cy) || !finite(radius) || radius <= 0.0f) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const float r = std::min(radius, static_cast<float>(kCoordLimit) / 4.0f);
    const Box bounds{static_cast<int32_t>(std::floor(cx - r - 1.0f)), static_cast<int32_t>(std::floor(cy - r - 1.0f)),
                     static_cast<int32_t>(std::ceil(cx + r + 1.0f)), static_cast<int32_t>(std::ceil(cy + r + 1.0f))};
    const Box vis = intersect(bounds, t.clip);
    const float solid2 = (r - 1.0f) > 0.0f ? (r - 1.0f) * (r - 1.0f) : -1.0f;
    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        const float dy = static_cast<float>(y) + 0.5f - cy;
        uint8_t* d = t.at(vis.x0, y);
        for (int32_t x = vis.x0; x < vis.x1; ++x, d += 4) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= solid2) {
                blend_px(d, color, 255);
            } else {
                const uint32_t cov = coverage_from_sd(std::sqrt(d2) - r);
                if (cov != 0) blend_px(d, color, cov);
            }
        }
    }
}

void Painter::stroke_circle(float cx, float cy, float radius, float thickness, Color color) noexcept {
    if (m_width == 0 || color.a == 0 || !finite(cx) || !finite(cy) || !finite(radius) || !finite(thickness) ||
        radius <= 0.0f || thickness <= 0.0f) {
        return;
    }
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const float r = std::min(radius, static_cast<float>(kCoordLimit) / 4.0f);
    const float th = std::min(thickness, r);
    const Box bounds{static_cast<int32_t>(std::floor(cx - r - 1.0f)), static_cast<int32_t>(std::floor(cy - r - 1.0f)),
                     static_cast<int32_t>(std::ceil(cx + r + 1.0f)), static_cast<int32_t>(std::ceil(cy + r + 1.0f))};
    const Box vis = intersect(bounds, t.clip);
    const float inner_solid = r - th - 1.0f;
    const float inner_solid2 = inner_solid > 0.0f ? inner_solid * inner_solid : -1.0f;
    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        const float dy = static_cast<float>(y) + 0.5f - cy;
        uint8_t* d = t.at(vis.x0, y);
        for (int32_t x = vis.x0; x < vis.x1; ++x, d += 4) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float d2 = dx * dx + dy * dy;
            if (d2 < inner_solid2) continue; // well inside the hole
            const float dist = std::sqrt(d2);
            const float ring = coverage_f(dist - r) - coverage_f(dist - (r - th));
            if (ring > 0.0f) blend_px(d, color, static_cast<uint32_t>(ring * 255.0f + 0.5f));
        }
    }
}

void Painter::draw_line(float x0, float y0, float x1, float y1, float thickness, Color color) noexcept {
    if (m_width == 0 || color.a == 0 || !finite(x0) || !finite(y0) || !finite(x1) || !finite(y1) || !finite(thickness) ||
        thickness <= 0.0f) {
        return;
    }
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const float lim = static_cast<float>(kCoordLimit) / 4.0f;
    x0 = std::clamp(x0, -lim, lim);
    y0 = std::clamp(y0, -lim, lim);
    x1 = std::clamp(x1, -lim, lim);
    y1 = std::clamp(y1, -lim, lim);
    const float hw = std::min(thickness, lim) * 0.5f;
    const float pad = hw + 1.0f;
    const Box bounds{static_cast<int32_t>(std::floor(std::min(x0, x1) - pad)), static_cast<int32_t>(std::floor(std::min(y0, y1) - pad)),
                     static_cast<int32_t>(std::ceil(std::max(x0, x1) + pad)), static_cast<int32_t>(std::ceil(std::max(y0, y1) + pad))};
    const Box vis = intersect(bounds, t.clip);
    // Distances are computed in double: for very long segments the float projection error is a visible fraction of a pixel.
    const double ax = x0;
    const double ay = y0;
    const double vx = static_cast<double>(x1) - ax;
    const double vy = static_cast<double>(y1) - ay;
    const double len2 = vx * vx + vy * vy;
    const double half = hw;
    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        const double py = static_cast<double>(y) + 0.5;
        uint8_t* d = t.at(vis.x0, y);
        for (int32_t x = vis.x0; x < vis.x1; ++x, d += 4) {
            const double px = static_cast<double>(x) + 0.5;
            double u = 0.0;
            if (len2 > 1e-12) u = std::clamp(((px - ax) * vx + (py - ay) * vy) / len2, 0.0, 1.0);
            const double ex = px - (ax + vx * u);
            const double ey = py - (ay + vy * u);
            const double dist = std::sqrt(ex * ex + ey * ey);
            const uint32_t cov = coverage_from_sd(static_cast<float>(dist - half));
            if (cov != 0) blend_px(d, color, cov);
        }
    }
}

void Painter::fill_radial_gradient(float cx, float cy, float radius, Color inner, Color outer, bool dither) noexcept {
    if (m_width == 0 || !finite(cx) || !finite(cy) || !finite(radius) || radius <= 0.0f) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const float r = std::min(radius, static_cast<float>(kCoordLimit) / 4.0f);
    const Box bounds{static_cast<int32_t>(std::floor(cx - r)), static_cast<int32_t>(std::floor(cy - r)),
                     static_cast<int32_t>(std::ceil(cx + r)), static_cast<int32_t>(std::ceil(cy + r))};
    const Box vis = intersect(bounds, t.clip);
    const float inv = 1.0f / r;
    // A glow usually fades one colour's alpha only (outer = inner with alpha 0); then the colour is constant and the
    // premultiplied interpolation reduces to a plain alpha ramp, which is much cheaper per pixel.
    const bool same_rgb = inner.r == outer.r && inner.g == outer.g && inner.b == outer.b;
    const float a0 = static_cast<float>(inner.a);
    const float a1 = static_cast<float>(outer.a);
    for (int32_t y = vis.y0; y < vis.y1; ++y) {
        const float dy = static_cast<float>(y) + 0.5f - cy;
        uint8_t* d = t.at(vis.x0, y);
        for (int32_t x = vis.x0; x < vis.x1; ++x, d += 4) {
            const float dx = static_cast<float>(x) + 0.5f - cx;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist >= r) continue;
            const float u = dist * inv;
            const float dith = dither ? bayer(x, y) : 0.0f;
            if (same_rgb) {
                blend_px(d, Color{inner.r, inner.g, inner.b, quantize(a0 + (a1 - a0) * u, dith)}, 255);
            } else {
                blend_px(d, lerp_color(inner, outer, u, dith), 255);
            }
        }
    }
}

void Painter::draw_coverage_mask(int32_t x, int32_t y, uint32_t width, uint32_t height, const uint8_t* coverage, size_t stride,
                                 Color color) noexcept {
    if (m_width == 0 || color.a == 0 || coverage == nullptr || width == 0 || height == 0 || stride < width) return;
    // Beyond this the clamped box arithmetic in to_box() would no longer line up with the mask rows.
    if (x < -kCoordLimit || x > kCoordLimit || y < -kCoordLimit || y > kCoordLimit) return;

    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());
    const Box mask{x, y, static_cast<int32_t>(std::min<int64_t>(int64_t{x} + width, kCoordLimit)),
                   static_cast<int32_t>(std::min<int64_t>(int64_t{y} + height, kCoordLimit))};
    const Box vis = intersect(mask, t.clip);
    for (int32_t row = vis.y0; row < vis.y1; ++row) {
        const uint8_t* src = coverage + static_cast<size_t>(row - y) * stride + static_cast<size_t>(vis.x0 - x);
        uint8_t* d = t.at(vis.x0, row);
        for (int32_t col = vis.x0; col < vis.x1; ++col, ++src, d += 4) {
            if (*src != 0) blend_px(d, color, *src);
        }
    }
}

void Painter::draw_shadow(const Rect& box, const CornerRadii& radii, const ShadowSpec& s, bool cut_out) noexcept {
    if (m_width == 0 || s.color.a == 0 || !finite(s.blur) || !finite(s.offset_x) || !finite(s.offset_y) || !finite(s.spread)) return;
    const Box b = to_box(box);
    if (b.empty()) return;
    const Target t = make_target(m_pixels.data(), m_width, m_height, clip_bounds());

    const float lim = static_cast<float>(kCoordLimit) / 4.0f;
    const float blur = std::clamp(s.blur, 0.0f, 512.0f);
    const float spread = std::clamp(s.spread, -lim, lim);
    const float ox = std::clamp(s.offset_x, -lim, lim);
    const float oy = std::clamp(s.offset_y, -lim, lim);

    // The shadow shape: the box grown by `spread`, shifted by the offset, with correspondingly grown radii.
    const RR box_rr = make_rr(static_cast<float>(b.x0), static_cast<float>(b.y0), static_cast<float>(b.x1), static_cast<float>(b.y1), radii);
    const float sx0 = static_cast<float>(b.x0) - spread + ox;
    const float sy0 = static_cast<float>(b.y0) - spread + oy;
    const float sx1 = static_cast<float>(b.x1) + spread + ox;
    const float sy1 = static_cast<float>(b.y1) + spread + oy;
    if (sx1 <= sx0 || sy1 <= sy0) return;
    CornerRadii grown{box_rr.tl + spread, box_rr.tr + spread, box_rr.br + spread, box_rr.bl + spread};
    const RR shape = make_rr(sx0, sy0, sx1, sy1, grown);

    const Box region = intersect(Box{static_cast<int32_t>(std::floor(sx0 - blur)) - 1, static_cast<int32_t>(std::floor(sy0 - blur)) - 1,
                                     static_cast<int32_t>(std::ceil(sx1 + blur)) + 1, static_cast<int32_t>(std::ceil(sy1 + blur)) + 1},
                                 t.clip);
    if (region.empty()) return;

    // Rows/columns of the box that are certainly fully inside it can be skipped when cutting out.
    const int32_t rmax = static_cast<int32_t>(std::ceil(box_rr.max_radius()));
    const int32_t core_y0 = b.y0 + rmax;
    const int32_t core_y1 = b.y1 - rmax;
    const int32_t core_x0 = b.x0 + rmax;
    const int32_t core_x1 = b.x1 - rmax;
    const float peak = static_cast<float>(s.color.a);

    for (int32_t y = region.y0; y < region.y1; ++y) {
        const float py = static_cast<float>(y) + 0.5f;
        int32_t skip_a = 0;
        int32_t skip_b = 0; // empty
        if (cut_out) {
            if (y >= core_y0 && y < core_y1) {
                skip_a = b.x0;
                skip_b = b.x1;
            } else if (y >= b.y0 && y < b.y1 && core_x0 < core_x1) {
                skip_a = core_x0;
                skip_b = core_x1;
            }
        }
        uint8_t* row = t.at(0, y);
        for (int32_t x = region.x0; x < region.x1; ++x) {
            if (x >= skip_a && x < skip_b) {
                x = skip_b - 1;
                continue;
            }
            const float px = static_cast<float>(x) + 0.5f;
            const float sd = shape.sd(px, py);
            float a;
            if (blur <= 0.0f) {
                a = coverage_f(sd);
            } else {
                if (sd >= blur) continue;
                const float u = std::clamp((blur - sd) / (2.0f * blur), 0.0f, 1.0f);
                a = u * u * (3.0f - 2.0f * u); // smoothstep falloff
            }
            if (a <= 0.0f) continue;
            if (cut_out) a *= 1.0f - coverage_f(box_rr.sd(px, py));
            const uint32_t sa = static_cast<uint32_t>(a * peak + 0.5f);
            if (sa == 0) continue;
            blend_px(row + static_cast<size_t>(x) * 4, Color{s.color.r, s.color.g, s.color.b, static_cast<uint8_t>(sa)}, 255);
        }
    }
}

} // namespace vynexos::desktop
