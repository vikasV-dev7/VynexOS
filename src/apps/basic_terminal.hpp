#pragma once
#include "vynexos/apps/application.hpp"
#include "vynexos/apps/terminal.hpp"
#include "vynexos/desktop/widget_toolkit.hpp"
#include <memory>
#include <string>
#include <vector>

namespace vynexos::apps {

/// A line-editing terminal: typed text (TextInputEvent) edits the prompt line, Backspace deletes, Return submits the
/// line to the input callback and starts a new prompt. There is no command interpreter. It renders into whatever
/// surface the shell gives it (the client area) and shows the most recent lines that fit.
class BasicTerminal final : public IApplication, public ITerminal {
public:
    explicit BasicTerminal(std::shared_ptr<desktop::IWidgetToolkit> toolkit);

    // IApplication
    bool initialize() override;
    void start() override {}
    void update(double) override {}
    bool handle_event(const core::Event& event) override;
    void render(desktop::ISurface& target_surface) override;
    void shutdown() override {}
    [[nodiscard]] std::string get_name() const noexcept override { return "Terminal"; }
    [[nodiscard]] std::optional<desktop::Rect> take_invalidated_region() override;

    // ITerminal
    void write_output(std::string_view text) override;
    void on_input(std::function<void(std::string_view)> callback) override;
    void request_ai_explanation(std::string_view command_context) override;
    void request_ai_suggestion(std::string_view intent) override;

    // Layout and state, for hosts and tests.
    static constexpr int32_t kMargin = 10;
    static constexpr int32_t kLineHeight = 20;
    static constexpr const char* kPrompt = "root@vynex:~# ";
    static constexpr size_t kMaxHistory = 1000;
    [[nodiscard]] const std::vector<std::string>& history() const { return m_history; }
    [[nodiscard]] const std::string& input() const { return m_input; }
    /// The line currently being edited, as shown (prompt, input, cursor).
    [[nodiscard]] std::string prompt_line() const { return kPrompt + m_input + "_"; }
    /// The content band a line drawn at `y` may paint: its whole slot, half a line above and below, since glyphs
    /// reach past the line's nominal 20 px box (the text baseline sits inside it, ascenders above it).
    [[nodiscard]] static desktop::Rect line_band(int32_t y) {
        return {0, y - kLineHeight / 2, kWholeContent.width, static_cast<uint32_t>(2 * kLineHeight)};
    }

private:
    void submit();
    void append(std::string line);
    void changed_everything() { m_dirty = Dirty::All; }
    void changed_prompt_line() {
        if (m_dirty == Dirty::None) m_dirty = Dirty::Prompt;
    }

    std::shared_ptr<desktop::IWidgetToolkit> m_toolkit;
    std::vector<std::string> m_history;
    std::string m_input;
    std::function<void(std::string_view)> m_input_callback;

    enum class Dirty { None, Prompt, All };
    Dirty m_dirty{Dirty::All};
    int32_t m_prompt_y{kMargin}; // where the prompt line was last drawn (content coordinates)
};

} // namespace vynexos::apps
