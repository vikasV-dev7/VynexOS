#include "sdl2_display_backend.hpp"
#include "sdl2_pixel_format.hpp"
#include <SDL.h>
#include <algorithm>

namespace vynexos::desktop {

struct SDL2DisplayBackend::SDLContext {
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    SDL_Texture* texture{nullptr};
    uint32_t current_width{0};
    uint32_t current_height{0};
    int last_out_w{0};
    int last_out_h{0};
    int pending_repaints{0};

    ~SDLContext() {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        if (SDL_WasInit(SDL_INIT_VIDEO)) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }
};

SDL2DisplayBackend::SDL2DisplayBackend(std::shared_ptr<core::ILogger> logger)
    : m_logger(std::move(logger)), m_ctx(std::make_unique<SDLContext>()) {}

SDL2DisplayBackend::~SDL2DisplayBackend() {
    shutdown();
}

std::expected<void, DisplayError> SDL2DisplayBackend::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) return {};

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        m_logger->error("SDL_InitSubSystem failed");
        return std::unexpected(DisplayError::BackendNotAvailable);
    }

    // Size the window to the logical desktop's aspect ratio, shrunk to fit the usable display area
    // (leaving room for window chrome) so the whole desktop, including the taskbar, is visible.
    int window_w = static_cast<int>(m_logical_size.width / 2);
    int window_h = static_cast<int>(m_logical_size.height / 2);
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(0, &usable) == 0 && usable.w > 0 && usable.h > 0) {
        const double fit = std::min({0.9 * usable.w / m_logical_size.width,
                                     0.9 * usable.h / m_logical_size.height, 1.0});
        window_w = std::max(320, static_cast<int>(m_logical_size.width * fit));
        window_h = std::max(180, static_cast<int>(m_logical_size.height * fit));
    }

    // Smooth scaling: the desktop is rarely presented at exactly 1:1.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    m_ctx->window = SDL_CreateWindow(
        "VynexOS Desktop",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_w, window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (m_ctx->window) {
        SDL_SetWindowMinimumSize(m_ctx->window, 320, 180);
    }
    
    if (!m_ctx->window) {
        m_logger->error("SDL_CreateWindow failed");
        return std::unexpected(DisplayError::BackendNotAvailable);
    }

    m_ctx->renderer = SDL_CreateRenderer(m_ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_ctx->renderer) {
        m_logger->error("SDL_CreateRenderer failed");
        return std::unexpected(DisplayError::BackendNotAvailable);
    }

    m_initialized = true;
    m_logger->info("SDL2 Display Backend initialized");
    return {};
}

std::expected<void, DisplayError> SDL2DisplayBackend::flush_buffer(const hal::FrameBuffer& buffer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return std::unexpected(DisplayError::BackendNotAvailable);

    if (buffer.pixels.empty() || buffer.width == 0 || buffer.height == 0) {
        return std::unexpected(DisplayError::InvalidBuffer);
    }

    if (!m_ctx->texture || m_ctx->current_width != buffer.width || m_ctx->current_height != buffer.height) {
        if (m_ctx->texture) SDL_DestroyTexture(m_ctx->texture);
        m_ctx->texture = SDL_CreateTexture(
            m_ctx->renderer,
            kFramebufferTextureFormat, // byte order R,G,B,A, matching the software surfaces on any endianness
            SDL_TEXTUREACCESS_STREAMING,
            static_cast<int>(buffer.width), static_cast<int>(buffer.height)
        );
        if (!m_ctx->texture) return std::unexpected(DisplayError::BackendNotAvailable);
        m_ctx->current_width = buffer.width;
        m_ctx->current_height = buffer.height;
    }

    SDL_UpdateTexture(m_ctx->texture, nullptr, buffer.pixels.data(), static_cast<int>(buffer.width * 4));
    present_locked();
    return {};
}

void SDL2DisplayBackend::present_locked() {
    int out_w = 0;
    int out_h = 0;
    SDL_GetRendererOutputSize(m_ctx->renderer, &out_w, &out_h);
    const hal::Viewport vp = hal::compute_viewport(static_cast<uint32_t>(std::max(out_w, 0)),
                                                   static_cast<uint32_t>(std::max(out_h, 0)),
                                                   m_ctx->current_width, m_ctx->current_height);
    const SDL_Rect dst{vp.x, vp.y, static_cast<int>(vp.width), static_cast<int>(vp.height)};

    SDL_SetRenderDrawColor(m_ctx->renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_ctx->renderer);
    SDL_RenderCopy(m_ctx->renderer, m_ctx->texture, nullptr, &dst);
    SDL_RenderPresent(m_ctx->renderer);
    m_ctx->last_out_w = out_w;
    m_ctx->last_out_h = out_h;
}

void SDL2DisplayBackend::refresh() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_ctx->texture) return;
    int out_w = 0;
    int out_h = 0;
    SDL_GetRendererOutputSize(m_ctx->renderer, &out_w, &out_h);
    if (out_w != m_ctx->last_out_w || out_h != m_ctx->last_out_h) {
        // The renderer's swap chain is resized lazily, so the first present after a resize can still
        // use the old buffer size. Re-present for a few consecutive frames to settle on the new size.
        m_ctx->pending_repaints = 3;
    }
    if (m_ctx->pending_repaints > 0) {
        --m_ctx->pending_repaints;
        present_locked();
    }
}

hal::LogicalPoint SDL2DisplayBackend::to_logical(int32_t window_x, int32_t window_y) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized || !m_ctx->window || !m_ctx->renderer) {
        return {window_x, window_y};
    }

    int win_w = 0;
    int win_h = 0;
    int out_w = 0;
    int out_h = 0;
    SDL_GetWindowSize(m_ctx->window, &win_w, &win_h);
    SDL_GetRendererOutputSize(m_ctx->renderer, &out_w, &out_h);
    if (win_w <= 0 || win_h <= 0 || out_w <= 0 || out_h <= 0) {
        return {window_x, window_y};
    }

    // Window units -> renderer pixels (differs from 1.0 on high-DPI displays).
    const int32_t px = hal::window_units_to_pixels(window_x, static_cast<uint32_t>(win_w), static_cast<uint32_t>(out_w));
    const int32_t py = hal::window_units_to_pixels(window_y, static_cast<uint32_t>(win_h), static_cast<uint32_t>(out_h));

    const hal::Viewport vp = hal::compute_viewport(static_cast<uint32_t>(out_w), static_cast<uint32_t>(out_h),
                                                   m_logical_size.width, m_logical_size.height);
    return hal::window_to_logical(vp, px, py);
}

void SDL2DisplayBackend::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized) return;
    
    m_ctx.reset(); 
    m_ctx = std::make_unique<SDLContext>(); 
    m_initialized = false;
    m_logger->info("SDL2 Display Backend shutdown");
}

} // namespace vynexos::desktop
