#include "sdl2_display_backend.hpp"
#include <SDL.h>

namespace vynexos::desktop {

struct SDL2DisplayBackend::SDLContext {
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};
    SDL_Texture* texture{nullptr};
    uint32_t current_width{0};
    uint32_t current_height{0};

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

    m_ctx->window = SDL_CreateWindow(
        "VynexOS Desktop",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
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
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            buffer.width, buffer.height
        );
        if (!m_ctx->texture) return std::unexpected(DisplayError::BackendNotAvailable);
        m_ctx->current_width = buffer.width;
        m_ctx->current_height = buffer.height;
    }

    SDL_UpdateTexture(m_ctx->texture, nullptr, buffer.pixels.data(), buffer.width * 4);
    
    SDL_RenderClear(m_ctx->renderer);
    SDL_RenderCopy(m_ctx->renderer, m_ctx->texture, nullptr, nullptr);
    SDL_RenderPresent(m_ctx->renderer);

    return {};
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
