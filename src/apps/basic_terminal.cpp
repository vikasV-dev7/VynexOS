#include "basic_terminal.hpp"
#include "vynexos/hal/input_driver.hpp"
#include <algorithm>
#include <utility>

namespace vynexos::apps {

BasicTerminal::BasicTerminal(std::shared_ptr<desktop::IWidgetToolkit> toolkit) : m_toolkit(std::move(toolkit)) {
    m_history.push_back("VynexOS Terminal v1.0");
}

bool BasicTerminal::initialize() {
    return m_toolkit != nullptr;
}

bool BasicTerminal::handle_event(const core::Event& event) {
    if (event.topic == hal::kTopicInputText && event.payload.type() == typeid(hal::TextInputEvent)) {
        const auto& text = std::any_cast<const hal::TextInputEvent&>(event.payload).text;
        if (text.empty()) return false;
        m_input += text;
        changed_prompt_line();
        return true;
    }
    if (event.topic == hal::kTopicInputKey && event.payload.type() == typeid(hal::KeyEvent)) {
        const auto key = std::any_cast<hal::KeyEvent>(event.payload);
        if (!key.pressed) return false;
        if (key.key_code == hal::kKeyReturn || key.key_code == hal::kKeyKeypadEnter) {
            submit();
            return true;
        }
        if (key.key_code == hal::kKeyBackspace && !m_input.empty()) {
            // Remove one UTF-8 character: continuation bytes, then the lead byte.
            while (!m_input.empty() && (static_cast<unsigned char>(m_input.back()) & 0xC0) == 0x80) m_input.pop_back();
            if (!m_input.empty()) m_input.pop_back();
            changed_prompt_line();
            return true;
        }
    }
    return false;
}

void BasicTerminal::render(desktop::ISurface& target_surface) {
    if (!m_toolkit) return;
    std::shared_ptr<desktop::ISurface> surface(&target_surface, [](desktop::ISurface*) {}); // aliased: the toolkit takes a shared_ptr
    const uint32_t width = target_surface.width();
    const uint32_t height = target_surface.height();

    m_toolkit->draw_panel(surface, 0, 0, width, height);

    // The most recent lines that fit, the prompt line last.
    const size_t rows = static_cast<size_t>(std::max<int32_t>(1, (static_cast<int32_t>(height) - kMargin) / kLineHeight));
    const size_t total = m_history.size() + 1;
    const size_t first = total > rows ? total - rows : 0;
    int32_t y = kMargin;
    for (size_t i = first; i < m_history.size(); ++i, y += kLineHeight) {
        m_toolkit->draw_text(surface, kMargin, y, m_history[i], desktop::Color{200, 200, 200, 255});
    }
    m_toolkit->draw_text(surface, kMargin, y, prompt_line(), desktop::Color{200, 200, 200, 255});
    m_prompt_y = y;
}

std::optional<desktop::Rect> BasicTerminal::take_invalidated_region() {
    const Dirty dirty = std::exchange(m_dirty, Dirty::None);
    if (dirty == Dirty::All) return kWholeContent;
    if (dirty == Dirty::Prompt) return line_band(m_prompt_y);
    return std::nullopt;
}

void BasicTerminal::submit() {
    const std::string line = m_input;
    append(kPrompt + line);
    m_input.clear();
    if (m_input_callback) m_input_callback(line);
    changed_everything(); // the lines move up once the screen is full
}

void BasicTerminal::append(std::string line) {
    m_history.push_back(std::move(line));
    if (m_history.size() > kMaxHistory) m_history.erase(m_history.begin(), m_history.begin() + static_cast<std::ptrdiff_t>(m_history.size() - kMaxHistory));
}

void BasicTerminal::write_output(std::string_view text) {
    append(std::string(text));
    changed_everything();
}

void BasicTerminal::on_input(std::function<void(std::string_view)> callback) {
    m_input_callback = std::move(callback);
}

void BasicTerminal::request_ai_explanation(std::string_view command_context) {
    (void)command_context;
    write_output("[AI is thinking...]");
}

void BasicTerminal::request_ai_suggestion(std::string_view intent) {
    (void)intent;
    write_output("[AI Suggestion pending...]");
}

} // namespace vynexos::apps
